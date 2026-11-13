#ifndef NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__
#define NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__

//package net.minecraft.world.level.chunk;

#include "ChunkSource.h"
#include "storage/ChunkStorage.h"
#include "EmptyLevelChunk.h"
#include "../Level.h"
#include "../LevelConstants.h"
#include "../ChunkPos.h"
#include <unordered_map>
#include <cstdint>
#include <vector>

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

    // Storage maintenance must inspect resident chunks without calling
    // getChunk(), because getChunk() creates or generates a missing chunk.
    void getLoadedChunks(std::vector<LevelChunk*>& result) {
        result.clear();
        result.reserve(chunks.size());
        for (auto& entry : chunks) {
            if (entry.second != NULL && entry.second != emptyChunk)
                result.push_back(entry.second);
        }
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
            LevelChunk* newChunk = load(x, z);
			bool updateLights = false;
			bool generatedChunk = false;
            if (newChunk == NULL) {
                if (source == NULL) newChunk = emptyChunk;
				else {
					newChunk = source->getChunk(x, z);
					generatedChunk = newChunk != NULL && newChunk != emptyChunk;
				}
            } else {
				updateLights = true;
            }
            chunks[key] = newChunk;
            newChunk->lightLava();

			// A generated chunk runs recalcHeightmap() in its source before
			// ChunkCache has inserted it into the resident-chunk map.  Repeat the
			// pass after insertion so light-gap updates can see the chunk itself.
			if (generatedChunk)
				newChunk->recalcHeightmap();

			if (updateLights) {
				for (int cx = 0; cx < 16; cx++) {
					for (int cz = 0; cz < 16; cz++) {
						int height = level->getHeightmap(cx + x * 16, cz + z * 16);
						for (int cy = height; cy >= 0; cy--) {
							level->updateLight(LightLayer::Sky, cx + x * 16, cy, cz + z * 16, cx + x * 16, cy, cz + z * 16);
							level->updateLight(LightLayer::Block, cx + x * 16 - 1, cy, cz + z * 16 - 1, cx + x * 16 + 1, cy, cz + z * 16 + 1);
						}
					}
				}
			}

            LevelChunk* stored = chunks[key];
            if (stored != NULL) stored->load();

            // Tell the renderer (via LevelListener) that this chunk's blocks exist now,
            // so it rebuilds the faces at the boundary with already-rendered neighbours.
            level->setTilesDirty(x * 16, 0, z * 16, x * 16 + 15, 127, z * 16 + 15);

            if (!chunks[key]->terrainPopulated && hasChunk(x + 1, z + 1) && hasChunk(x, z + 1) && hasChunk(x + 1, z)) postProcess(this, x, z);
            if (hasChunk(x - 1, z) && !getChunk(x - 1, z)->terrainPopulated && hasChunk(x - 1, z + 1) && hasChunk(x, z + 1) && hasChunk(x - 1, z)) postProcess(this, x - 1, z);
            if (hasChunk(x, z - 1) && !getChunk(x, z - 1)->terrainPopulated && hasChunk(x + 1, z - 1) && hasChunk(x, z - 1) && hasChunk(x + 1, z)) postProcess(this, x, z - 1);
            if (hasChunk(x - 1, z - 1) && !getChunk(x - 1, z - 1)->terrainPopulated && hasChunk(x - 1, z - 1) && hasChunk(x, z - 1) && hasChunk(x - 1, z)) postProcess(this, x - 1, z - 1);
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
			// Mark the chunk populated only after all terrain features (trees,
			// snow, ores, etc.) have been generated.  Post-processing modifies
			// blocks outside the initial terrain pass, so force the completed
			// result to be written on the next save.
			chunk->terrainPopulated = true;
			chunk->markUnsaved();
        }
    }

	// Finish population passes that can be completed before saving. A chunk at
	// the loading edge may otherwise remain unpopulated because the original
	// +X/+Z neighbours have not been loaded yet. Keep the support area bounded
	// and leave genuinely incomplete chunks unpopulated so the next load can
	// continue the pass instead of losing it permanently.
	void finishPostProcess() {
		std::vector<ChunkPos> pendingPopulation;
		for (auto& entry : chunks) {
			LevelChunk* chunk = entry.second;
			if (chunk != NULL && chunk != emptyChunk && !chunk->terrainPopulated)
				pendingPopulation.push_back(ChunkPos(chunk->x, chunk->z));
		}

		if (level->isInfinite()) {
			for (size_t i = 0; i < pendingPopulation.size(); ++i) {
				const ChunkPos& pos = pendingPopulation[i];
				for (int dx = 0; dx <= 2; ++dx) {
					for (int dz = 0; dz <= 2; ++dz) {
						if (dx == 0 && dz == 0)
							continue;
						getChunk(pos.x + dx, pos.z + dz);
					}
				}
			}
		}

		while (true) {
			// Collect keys first because postProcess() can generate neighbours and
			// mutate the cache while the pass is running.
			std::vector<int64_t> ready;
			for (auto& entry : chunks) {
				LevelChunk* chunk = entry.second;
				if (chunk == NULL || chunk == emptyChunk || chunk->terrainPopulated)
					continue;
				if (hasChunk(chunk->x + 1, chunk->z + 1) &&
					hasChunk(chunk->x, chunk->z + 1) &&
					hasChunk(chunk->x + 1, chunk->z))
					ready.push_back(entry.first);
			}
			if (ready.empty())
				break;

			for (size_t i = 0; i < ready.size(); ++i) {
				auto entry = chunks.find(ready[i]);
				if (entry == chunks.end() || entry->second == NULL ||
					entry->second == emptyChunk || entry->second->terrainPopulated)
					continue;
				LevelChunk* chunk = entry->second;
				postProcess(this, chunk->x, chunk->z);
			}
		}
	}

	// Requeue a bounded 3x3 area around the player. Each vertical slice is
	// deliberately small so the normal light-update budget can consume the
	// repair over several frames instead of blocking on one full-chunk pass.
	void queueLightUpdatesAround(int chunkX, int chunkZ) override {
		for (int dx = -1; dx <= 1; ++dx)
			for (int dz = -1; dz <= 1; ++dz)
				queueLightUpdatesForChunk(chunkX + dx, chunkZ + dz);
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
		finishPostProcess();

		// A queued light repair must be completed before the chunk snapshot is
		// serialized. Otherwise the refreshed light arrays can remain only in
		// memory and disappear after reopening the world.
		if (level != NULL) {
			while (level->updateLights())
				;
		}

		if (storage != NULL) {
			std::vector<LevelChunk*> chunkList;
			for (auto& kv : chunks) {
				LevelChunk* chunk = kv.second;
				// Population may modify neighbouring chunks before they complete
				// their own pass. Infinite-world chunks must remain eligible for
				// serialization so their terrainPopulated state can be resumed after
				// reload. Newly generated chunks are also included once even if the
				// normal unsaved flag has not reached the storage layer yet.
				if (chunk && chunk != emptyChunk &&
					(chunk->terrainPopulated || level->isInfinite()))
					if (!onlyUnsaved || chunk->shouldSave(false) || !chunk->createdFromSave)
						chunkList.push_back(chunk);
			}
			storage->saveAll(level, chunkList);
		}
	}
private:
	void queueLightUpdatesForChunk(int x, int z) {
		if (!fits(x, z)) return;

		auto it = chunks.find(chunkKey(x, z));
		if (it == chunks.end() || it->second == NULL || it->second == emptyChunk)
			return;

		LevelChunk* chunk = it->second;

		const int x0 = x * CHUNK_WIDTH;
		const int z0 = z * CHUNK_WIDTH;
		const int x1 = x0 + CHUNK_WIDTH - 1;
		const int z1 = z0 + CHUNK_WIDTH - 1;
		const int lightSliceHeight = 8;
		for (int y0 = 0; y0 < Level::DEPTH; y0 += lightSliceHeight) {
			int y1 = y0 + lightSliceHeight - 1;
			if (y1 >= Level::DEPTH)
				y1 = Level::DEPTH - 1;

			level->updateLight(LightLayer::Sky, x0, y0, z0, x1, y1, z1);
			level->updateLight(LightLayer::Block, x0, y0, z0, x1, y1, z1);
		}

		// A loaded chunk may otherwise have no block changes and therefore no
		// unsaved flag, even though the refreshed light arrays are serialized.
		// Mark it dirty so the repair survives the next save/reload.
		if (chunk->createdFromSave)
			chunk->markUnsaved();
	}

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
    ChunkStorage* storage;
    std::unordered_map<int64_t, LevelChunk*> chunks;
    Level* level;

    LevelChunk* last;

};

#endif /*NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__*/
