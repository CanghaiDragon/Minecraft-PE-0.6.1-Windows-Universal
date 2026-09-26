#ifndef NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__
#define NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__

//package net.minecraft.world.level.chunk;

#include "ChunkSource.h"
#include "storage/ChunkStorage.h"
#include "EmptyLevelChunk.h"
#include "../Level.h"
#include "../LevelConstants.h"
#include "../../../util/PerfTimer.h"
#include <unordered_map>
#include <set>
#include <deque>
#include <cstdint>

class ChunkCache: public ChunkSource {
    //static const int CHUNK_CACHE_WIDTH = CHUNK_CACHE_WIDTH; // WAS 32;
    static const int MAX_SAVES = 2;
public:
    ChunkCache(Level* level_, ChunkStorage* storage_, ChunkSource* source_)
	:	xLast(-999999999),
		zLast(-999999999),
		last(NULL),
		level(level_),
		storage(storage_),
		source(source_)
	{
		isChunkCache = true;
		emptyChunk = new EmptyLevelChunk(level_, NULL, 0, 0);
    }

	~ChunkCache() {
		delete source;
		delete emptyChunk;
		for (auto& kv : chunks) {
			if (kv.second && kv.second != emptyChunk) {
				kv.second->deleteBlockData();
				delete kv.second;
			}
		}
	}

	static int64_t chunkKey(int x, int z) {
		return ((int64_t)(uint32_t)x << 32) | (uint32_t)z;
	}

    bool fits(int x, int z) {
        if (level->isInfinite()) return true;
        return (x >= 0 && z >= 0 && x < CHUNK_CACHE_WIDTH && z < CHUNK_CACHE_WIDTH);
    }

    bool hasChunk(int x, int z) {
        if (!fits(x, z)) return true;
        if (x == xLast && z == zLast && last != NULL) return true;
        int64_t key = chunkKey(x, z);
        auto it = chunks.find(key);
        return it != chunks.end() && (it->second == emptyChunk || it->second->isAt(x, z));
    }

    bool hasLoadedChunk(int x, int z) override {
        if (x == xLast && z == zLast && last != NULL) return true;
        int64_t key = chunkKey(x, z);
        auto it = chunks.find(key);
        return it != chunks.end() && it->second != NULL && it->second != emptyChunk
            && it->second->isAt(x, z);
    }

    LevelChunk* create(int x, int z) {
        return getChunk(x, z);
    }

    LevelChunk* getChunk(int x, int z) {
		if (x == xLast && z == zLast && last != NULL) return last;
		if (!fits(x, z)) return emptyChunk;

        int64_t key = chunkKey(x, z);
        if (!hasChunk(x, z)) {
            // No eviction needed — hash map holds each (x,z) independently
			TIMER_PUSH("chunkLoadStorage");
			LevelChunk* newChunk = load(x, z);
			TIMER_POP();
            if (newChunk == NULL) {
                if (source == NULL) newChunk = emptyChunk;
				else {
					TIMER_PUSH("chunkTerrainGeneration");
					newChunk = source->getChunk(x, z);
					TIMER_POP();
				}
            } else {
            }
            chunks[key] = newChunk;
			TIMER_PUSH("chunkLighting");
			newChunk->lightLava();

			// Loaded chunks already carry their saved light arrays. Recomputing
			// every column here duplicates the saved data and can enqueue millions
			// of light updates while exploring. Local block changes still use the
			// normal Level::updateLight path.

            LevelChunk* stored = chunks[key];
            if (stored != NULL) stored->load();

            // Tell the renderer (via LevelListener) that this chunk's blocks exist now,
            // so it rebuilds the faces at the boundary with already-rendered neighbours.
            level->setTilesDirty(x * 16, 0, z * 16, x * 16 + 15, 127, z * 16 + 15);

			TIMER_POP();
			queuePostProcess(x, z);
        }
        xLast = x;
        zLast = z;
        last = chunks[key];
        return chunks[key];
    }

	Biome::MobList getMobsAt(const MobCategory& mobCategory, int x, int y, int z) {
		return source->getMobsAt(mobCategory, x, y, z);
	}

	void postProcess(ChunkSource* parent, int x, int z) {
		if (!fits(x, z)) return;
        LevelChunk* chunk = getChunk(x, z);
        if (!chunk->terrainPopulated) {
            if (source != NULL) {
                source->postProcess(parent, x, z);
				chunk->clearUpdateMap();
            }
			// Mark only after the complete feature pass returns. Features such
			// as snow and trees can write into neighbouring chunks.
			chunk->terrainPopulated = true;
        }
	}

	void queuePostProcess(int x, int z) {
		if (!fits(x, z) || !hasLoadedChunk(x, z)) return;
		LevelChunk* chunk = chunks[chunkKey(x, z)];
		int64_t key = chunkKey(x, z);
		if (chunk != NULL && !chunk->terrainPopulated && postProcessQueued.insert(key).second)
			postProcessQueue.push_back(std::make_pair(x, z));
	}

    //bool save(bool force, ProgressListener progressListener) {
    //    int saves = 0;
    //    int count = 0;
    //    if (progressListener != NULL) {
    //        for (int i = 0; i < chunks.length; i++) {
    //            if (chunks[i] != NULL && chunks[i].shouldSave(force)) {
    //                count++;
    //            }
    //        }
    //    }
    //    int cc = 0;
    //    for (int i = 0; i < chunks.length; i++) {
    //        if (chunks[i] != NULL) {
    //            if (force && !chunks[i].dontSave) saveEntities(chunks[i]);
    //            if (chunks[i].shouldSave(force)) {
    //                save(chunks[i]);
    //                chunks[i].unsaved = false;
    //                if (++saves == MAX_SAVES && !force) return false;
    //                if (progressListener != NULL) {
    //                    if (++cc % 10 == 0) {
    //                        progressListener.progressStagePercentage(cc * 100 / count);
    //                    }
    //                }
    //            }
    //        }
    //    }

    //    if (force) {
    //        if (storage == NULL) return true;
    //        storage.flush();
    //    }
    //    return true;
    //}

    bool tick() {
		// Population is deliberately incremental. Entries are retained until
		// their neighbouring chunks are resident, so throttling cannot create
		// permanently unpopulated/empty terrain.
		int budget = 1;
		while (budget-- > 0 && !postProcessQueue.empty()) {
			std::pair<int, int> pos = postProcessQueue.front();
			postProcessQueue.pop_front();
			postProcessQueued.erase(chunkKey(pos.first, pos.second));
			if (!hasLoadedChunk(pos.first, pos.second)) continue;
			LevelChunk* chunk = chunks[chunkKey(pos.first, pos.second)];
			if (chunk == NULL || chunk->terrainPopulated) continue;
			if (hasLoadedChunk(pos.first + 1, pos.second + 1) &&
				hasLoadedChunk(pos.first, pos.second + 1) &&
				hasLoadedChunk(pos.first + 1, pos.second)) {
				TIMER_PUSH("chunkPostProcess");
				postProcess(this, pos.first, pos.second);
				TIMER_POP();
			} else {
				if (postProcessQueued.insert(chunkKey(pos.first, pos.second)).second)
					postProcessQueue.push_back(pos);
			}
		}
		if (storage != NULL) storage->tick();
        return source->tick();
    }

    bool shouldSave() {
        return true;
    }

    std::string gatherStats() {
        return "ChunkCache: 1024";
    }
	
	void saveAll(bool onlyUnsaved) {
		if (storage != NULL) {
			std::vector<LevelChunk*> chunkList;
			for (auto& kv : chunks) {
				LevelChunk* chunk = kv.second;
				if (chunk && chunk != emptyChunk)
					if (!onlyUnsaved || chunk->shouldSave(false))
						chunkList.push_back(chunk);
			}
			storage->saveAll(level, chunkList);
		}
	}
private:
    LevelChunk* load(int x, int z) {
        if (storage == NULL) return NULL;
        LevelChunk* levelChunk = storage->load(level, x, z);
        if (levelChunk != NULL) {
            levelChunk->lastSaveTime = level->getTime();
        }
        return levelChunk;
    }

    void saveEntities(LevelChunk* levelChunk) {
        if (storage == NULL) return;
        //try {
            storage->saveEntities(level, levelChunk);
        //} catch (Error e) {
        //    e.printStackTrace();
        //}
    }

    void save(LevelChunk* levelChunk) {
        if (storage == NULL) return;
        //try {
            levelChunk->lastSaveTime = level->getTime();
            storage->save(level, levelChunk);
        //} catch (IOException e) {
        //    e.printStackTrace();
        //}
    }

public:
	int xLast;
    int zLast;
private:
    LevelChunk* emptyChunk;
	ChunkSource* source;
	std::deque<std::pair<int, int>> postProcessQueue;
	std::set<int64_t> postProcessQueued;
    ChunkStorage* storage;
    std::unordered_map<int64_t, LevelChunk*> chunks;
    Level* level;

    LevelChunk* last;

};

#endif /*NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__*/
