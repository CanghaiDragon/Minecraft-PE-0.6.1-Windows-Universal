#if !defined(DEMO_MODE) && !defined(APPLE_DEMO_PROMOTION)

#include "LevelData.h"
#include "RegionFile.h"
#include "ExternalFileLevelStorage.h"
#include "FolderMethods.h"
#include "../chunk/LevelChunk.h"
#include "../chunk/ChunkCache.h"
#include "../Level.h"
#include "../LevelConstants.h"
#include "../../entity/player/Player.h"
#include "../tile/TreeTile.h"
#include "../../entity/EntityFactory.h"
#include "../../../nbt/NbtIo.h"
#include "../../../util/RakDataIO.h"
#include "../../../util/Mth.h"
#include "../../../raknet/GetTime.h"
#include "../../../platform/log.h"
#include "../tile/entity/TileEntity.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

static const int ChunkVersion_Light = 1;
static const int ChunkVersion_Entity = 2;

// The save queue must distinguish every loaded chunk in an infinite world.
// x + z * CHUNK_CACHE_WIDTH aliases distant coordinates and is only valid for
// the old finite-world coordinate range.
static inline int64_t chunkSaveKey(int x, int z)
{
	return ((int64_t)(uint32_t)x << 32) | (uint32_t)z;
}

const char* const fnLevelDatOld = "level.dat_old";
const char* const fnLevelDatNew = "level.dat_new";
const char* const fnLevelDat    = "level.dat";
const char* const fnPlayerDat   = "player.dat";

// Keep this marker contract identical to the UWP implementation.  Infinite
// and Sky worlds use the compressed per-chunk format and must not be opened as
// if they were old raw worlds.  Finite/Old worlds deliberately do not use it.
const char* const fnUwpStorageMarker = "uwp_storage.version";
const char UwpStorageMarkerContents[] =
    "MCPEUWP_STORAGE_V2\n"
    "CHUNK_CODEC=zlib\n"
    "ENTITY_STORAGE=per_chunk\n";
const char UwpStorageMarkerV1Contents[] =
    "MCPEUWP_STORAGE_V1\n"
    "CHUNK_CODEC=raw\n"
    "ENTITY_STORAGE=per_chunk\n";

bool hasRecognizedUwpStorageMarker(const std::string& directory)
{
    const std::string filename = directory + "/" + fnUwpStorageMarker;
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file)
        return false;

    char contents[sizeof(UwpStorageMarkerContents)] = {};
    const size_t bytesRead = fread(contents, 1, sizeof(contents), file);
    fclose(file);

    const size_t currentSize = sizeof(UwpStorageMarkerContents) - 1;
    const size_t v1Size = sizeof(UwpStorageMarkerV1Contents) - 1;
    return (bytesRead == currentSize &&
        memcmp(contents, UwpStorageMarkerContents, currentSize) == 0) ||
        (bytesRead == v1Size &&
        memcmp(contents, UwpStorageMarkerV1Contents, v1Size) == 0);
}

bool hasCurrentUwpStorageMarker(const std::string& directory)
{
    const std::string filename = directory + "/" + fnUwpStorageMarker;
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file)
        return false;

    char contents[sizeof(UwpStorageMarkerContents)] = {};
    const size_t bytesRead = fread(contents, 1, sizeof(contents), file);
    fclose(file);
    const size_t size = sizeof(UwpStorageMarkerContents) - 1;
    return bytesRead == size && memcmp(contents, UwpStorageMarkerContents, size) == 0;
}

bool writeCurrentUwpStorageMarker(const std::string& directory)
{
    const std::string filename = directory + "/" + fnUwpStorageMarker;
    if (hasCurrentUwpStorageMarker(directory))
        return true;

    const std::string temporaryFilename = filename + ".tmp";
    FILE* file = fopen(temporaryFilename.c_str(), "wb");
    if (!file)
        return false;

    const size_t size = sizeof(UwpStorageMarkerContents) - 1;
    const bool written = fwrite(UwpStorageMarkerContents, 1, size, file) == size;
    const bool closed = fclose(file) == 0;
    if (!written || !closed)
    {
        remove(temporaryFilename.c_str());
        return false;
    }

    // Write through a temporary file so a partially written marker is never
    // accepted as a valid storage marker.
    remove(filename.c_str());
    if (rename(temporaryFilename.c_str(), filename.c_str()) != 0)
    {
        remove(temporaryFilename.c_str());
        return false;
    }
    return true;
}

//
// Helpers for converting old levels to newer
//
class LevelConverters
{
public:
	// Replacing old Cloth (id based) with new Cloth (data based)
	static bool v1_ClothIdToClothData(LevelChunk* c) {
		bool changed = false;
		unsigned char* blocks = c->getBlockData();
		unsigned char newTile = Tile::cloth->id;

		for (int i = 0; i < 16*16*128; ++i) {
			unsigned char oldTile = blocks[i];
			//Tile::cloth_00 to Tile::cloth_61
			if (oldTile >= 101 && oldTile <= 115) {
				int color = 0xf - (oldTile - 101);
				blocks[i] = newTile;
				c->data.set(i, color);
				changed = true;
			}
		}
		return changed;
	}

	// Replacing unavailable blocks with "Update!" blocks
	static bool ReplaceUnavailableBlocks(LevelChunk* c) {
		//int st = getTimeMs();
		
		bool changed = false;
		unsigned char* blocks = c->getBlockData();

		for (int i = 0; i < 16*16*128; ++i) {
			unsigned char oldTile = blocks[i];
			unsigned char newTile = Tile::transformToValidBlockId(oldTile);
			if (oldTile != newTile) {
				blocks[i] = newTile;
				changed = true;
			}
		}

		//int et = getTimeMs();
		//LOGI("time: %d\n", et - st);
		return changed;
	}

	//static bool ConvertPlayerDatToLevelDat() {
	//	return false;
	//}
};


ExternalFileLevelStorage::ExternalFileLevelStorage(const std::string& levelId, const std::string& fullPath)
:	levelId(levelId),
	levelPath(fullPath),
	loadedLevelData(NULL),
	regionFile(NULL),
	entitiesFile(NULL),
	tickCount(0),
	lastSavedEntitiesTick(-999999),
	level(NULL),
	loadedStorageVersion(SharedConstants::StorageVersion)
{
	createFolderIfNotExists(levelPath.c_str());

	std::string datFileName   = levelPath + "/" + fnLevelDat;
	std::string levelFileName = levelPath + "/" + fnPlayerDat;
	loadedLevelData = new LevelData();
	if (readLevelData(levelPath, *loadedLevelData))
	{
		// Infinite/Sky worlds are only valid when their storage contract is
		// explicitly identified.  Finite/Old worlds retain the original format.
		if (loadedLevelData->isInfinite() &&
			!hasRecognizedUwpStorageMarker(levelPath))
		{
			LOGW("UWP refusing legacy Infinite/Sky world without storage marker: %s\n",
				levelPath.c_str());
			delete loadedLevelData;
			loadedLevelData = NULL;
			throw std::runtime_error("MCPEUWP_LEGACY_INFINITE_STORAGE");
		}

		loadedStorageVersion = loadedLevelData->getStorageVersion();
		readPlayerData(levelFileName, *loadedLevelData);
	} else {
		delete loadedLevelData;
		loadedLevelData = NULL;
	}
}

ExternalFileLevelStorage::~ExternalFileLevelStorage()
{
	clearPendingChunkEntityTags();
	delete regionFile;
	delete loadedLevelData;
	for (auto& kv : regionFiles)
		delete kv.second;
}

void ExternalFileLevelStorage::clearPendingChunkEntityTags()
{
	for (auto& entry : pendingChunkEntityTags) {
		for (std::size_t i = 0; i < entry.second.size(); ++i) {
			if (entry.second[i] != NULL) {
				entry.second[i]->deleteChildren();
				delete entry.second[i];
			}
		}
	}
	pendingChunkEntityTags.clear();
}

void ExternalFileLevelStorage::saveLevelData(LevelData& levelData, std::vector<Player*>* players) {
	ExternalFileLevelStorage::saveLevelData(levelPath, levelData, players);
}

void ExternalFileLevelStorage::saveLevelData( const std::string& levelPath, LevelData& levelData, std::vector<Player*>* players )
{
	std::string directory = levelPath + "/";
    std::string tmpFile = directory + fnLevelDatNew;
    std::string datFile = directory + fnLevelDat;
    std::string oldFile = directory + fnLevelDatOld;

	levelData.setStorageVersion(SharedConstants::StorageVersion);
	if (!writeLevelData(tmpFile, levelData, players))
        return;

    // Remove old backup
    remove(oldFile.c_str());

    // If it exists, move the previous save to backup (and possibly delete it)
    if (exists(datFile.c_str())) {
        if (rename(datFile.c_str(), oldFile.c_str())) {
            LOGE("Error@saveLevelData: Couldn't move savefile to level.dat_old\n");
            return;
        }
        remove(datFile.c_str());
    }
    // Move the new save to level.dat
    if (rename(tmpFile.c_str(), datFile.c_str())) {
        LOGE("Error@saveLevelData: Couldn't move new file to level.dat\n");
        return;
    }

	// New markers are written only for newly saved Infinite/Sky storage.  Old
	// finite worlds remain byte-for-byte free of this UWP-only gate.
	if (levelData.isInfinite() && !writeCurrentUwpStorageMarker(levelPath))
		LOGE("UWP could not write storage marker at %s\n", levelPath.c_str());

    // Remove the temporary save, if the rename didn't do it
    remove(tmpFile.c_str());
}

LevelData* ExternalFileLevelStorage::prepareLevel(Level* _level)
{
	level = _level;
	return loadedLevelData;
}

bool ExternalFileLevelStorage::readLevelData(const std::string& directory, LevelData& levelData)
{
    // Try to load level.dat
    std::string datFilename = directory + "/" + fnLevelDat;
	FILE* file = fopen(datFilename.c_str(), "rb");

    // If that fails, try to load level.dat_old
    if (!file) {
        datFilename = directory + "/" + fnLevelDatOld;
        file = fopen(datFilename.c_str(), "rb");
    }

    if (!file)
        return false;

	int version = 0;
	int size = 0;
	unsigned char* data = NULL;

	do {
		if (fread(&version, sizeof(version), 1, file) != 1)
		{
			break;
		}
		if (fread(&size, sizeof(size), 1, file) != 1)
		{
			break;
		}

		int left = getRemainingFileSize(file);
		if (size > left || size <= 0)
			break;

		data = new unsigned char[size];
		if (fread(data, 1, size, file) != size)
		{
			break;
		}

		if (version == 1) {
			RakNet::BitStream bitStream(data, size, false);
			levelData.v1_read(bitStream, version);
		} else if (version >= 2) {
			//LOGI("---> Trying to load level with version %d\n", version);
			RakNet::BitStream bitStream(data, size, false);
			RakDataInput stream(bitStream);
			//LOGI("dat: %s\n", datFileName.c_str());
			CompoundTag* tag = NbtIo::read(&stream);
			if (tag) {
				levelData.getTagData(tag);
				tag->deleteChildren();
				delete tag;
			}
			//LOGI("<--- Finished reading level tag: %p\n", tag);
		}
	} while (false);

	fclose(file);
	delete [] data;

    return true;
}

bool ExternalFileLevelStorage::writeLevelData(const std::string& datFileName, LevelData& levelData, const std::vector<Player*>* players)
{
	LOGI("Writing down level seed as: %ld\n", levelData.getSeed());
	//return true;

	// Write level info
	FILE* file = fopen(datFileName.c_str(), "wb");
	if (!file)
        return false;

    //if (levelData.getStorageVersion() == 1) {
	RakNet::BitStream data;
	if (levelData.getStorageVersion() == 1)
		levelData.v1_write(data);
	else {
		RakDataOutput buf(data);
		//LOGI("---> Trying to write level with version %d\n", version);
		CompoundTag* tag = NULL;
		if (players && !players->empty())
			tag = levelData.createTag(*players);
		else
			tag = levelData.createTag();

		NbtIo::write(tag, &buf);
		tag->deleteChildren();
		delete tag;
		//LOGI("<--- Finished writing level data. Size: %d\n", fdout.bytesWritten);
	}

	int version = levelData.getStorageVersion(); // 1
	fwrite(&version, sizeof(version), 1, file);
	int size = data.GetNumberOfBytesUsed();
	fwrite(&size, sizeof(size), 1, file);

	fwrite(data.GetData(), 1, size, file);
	fclose(file);

	return true;
}

bool ExternalFileLevelStorage::readPlayerData(const std::string& filename, LevelData& dest)
{
	FILE* fp = fopen(filename.c_str(), "rb");
	if (!fp)
		return false;

	do {
		int version;
		if (fread(&version, 4, 1, fp) != 1)
			break;

		int size;
		if (fread(&size, 4, 1, fp) != 1)
			break;

		if (version == 1) {
			if (fread(&dest.playerData, 1, sizeof(dest.playerData), fp) != size)
				break;

			// Fix coordinates — only clamp x/z for finite (Old) worlds.
			// Infinite worlds have no boundaries, so don't snap the player.
			Vec3& pos = dest.playerData.pos;
			if (!dest.isInfinite()) {
				if (pos.x < 0.5f) pos.x = 0.5f;
				if (pos.z < 0.5f) pos.z = 0.5f;
				if (pos.x > (LEVEL_WIDTH - 0.5f)) pos.x = LEVEL_WIDTH - 0.5f;
				if (pos.z > (LEVEL_DEPTH - 0.5f)) pos.z = LEVEL_DEPTH - 0.5f;
			}
			if (pos.y < 0) pos.y = 64;

			dest.playerDataVersion = version;
		}
	} while (false);

	fclose(fp);
	return true;
}

void ExternalFileLevelStorage::tick()
{
	tickCount++;
	if ((tickCount % 50) == 0 && level)
	{
		// Enumerate only resident chunks.  Calling Level::getChunk() for every
		// coordinate would generate missing chunks and is not valid for infinite
		// worlds.
		ChunkSource* chunkSource = level->getChunkSource();
		if (chunkSource != NULL && chunkSource->isChunkCache)
		{
			ChunkCache* chunkCache = static_cast<ChunkCache*>(chunkSource);
			std::vector<LevelChunk*> loadedChunks;
			chunkCache->getLoadedChunks(loadedChunks);
			for (LevelChunk* chunk : loadedChunks)
			{
				if (chunk == NULL || !chunk->unsaved ||
					(!level->isInfinite() && !chunk->terrainPopulated))
					continue;

				const int64_t pos = chunkSaveKey(chunk->x, chunk->z);
				UnsavedChunkList::iterator prev = unsavedChunkList.begin();
				for (; prev != unsavedChunkList.end(); ++prev)
				{
					if ((*prev).pos == pos)
					{
						// The chunk has been modified again, so update its time.
						(*prev).addedToList = RakNet::GetTimeMS();
						break;
					}
				}
				if (prev == unsavedChunkList.end())
				{
					UnsavedLevelChunk unsaved;
					unsaved.pos = pos;
					unsaved.addedToList = RakNet::GetTimeMS();
					unsaved.chunk = chunk;
					unsavedChunkList.push_back(unsaved);
				}
				// Do not clear unsaved here.  The queue is only a scheduling list;
				// save() clears the flag after a successful disk write.
			}
		}
		else if (!level->isInfinite())
		{
			// Preserve the legacy finite-world fallback for non-ChunkCache
			// sources.  It is not used by the normal world path.
			for (int z = 0; z < CHUNK_CACHE_WIDTH; z++)
			{
				for (int x = 0; x < CHUNK_CACHE_WIDTH; x++)
				{
					LevelChunk* chunk = level->getChunk(x, z);
					if (chunk && chunk->unsaved)
					{
						const int64_t pos = chunkSaveKey(x, z);
						UnsavedChunkList::iterator prev = unsavedChunkList.begin();
						for (; prev != unsavedChunkList.end(); ++prev)
						{
							if ((*prev).pos == pos)
							{
								(*prev).addedToList = RakNet::GetTimeMS();
								break;
							}
						}
						if (prev == unsavedChunkList.end())
						{
							UnsavedLevelChunk unsaved;
							unsaved.pos = pos;
							unsaved.addedToList = RakNet::GetTimeMS();
							unsaved.chunk = chunk;
							unsavedChunkList.push_back(unsaved);
						}
					}
				}
			}
		}

        savePendingUnsavedChunks(2);
	}
	if (tickCount - lastSavedEntitiesTick > (60 * SharedConstants::TicksPerSecond)) {
		saveEntities(level, NULL);
	}
}

// Floor-divide cx by 32, rounding toward -infinity (for negative chunk coords).
static inline int floorDivBy32(int v)
{
	return v < 0 ? (v - 31) / 32 : v / 32;
}

static inline int64_t regionKey(int rx, int rz)
{
	return ((int64_t)(uint32_t)rx << 32) | (uint32_t)rz;
}

RegionFile* ExternalFileLevelStorage::getOrOpenRegion(int cx, int cz)
{
	int rx = floorDivBy32(cx);
	int rz = floorDivBy32(cz);
	int64_t key = regionKey(rx, rz);

	auto it = regionFiles.find(key);
	if (it != regionFiles.end())
		return it->second; // may be nullptr if open() failed previously

	std::string path = levelPath + "/chunks.r." +
	                   std::to_string(rx) + "." + std::to_string(rz) + ".dat";
	RegionFile* rf = new RegionFile(path, true, true);
	if (!rf->open())
	{
		delete rf;
		rf = NULL;
	}
	regionFiles[key] = rf;
	return rf;
}

void ExternalFileLevelStorage::save(Level* level, LevelChunk* levelChunk)
{
	int cx = levelChunk->x;
	int cz = levelChunk->z;
	int lx = cx, lz = cz; // local coords within the region file

	RegionFile* rf;
	if (level->isInfinite())
	{
		rf = getOrOpenRegion(cx, cz);
		if (!rf) return;
		int rx = floorDivBy32(cx);
		int rz = floorDivBy32(cz);
		lx = cx - rx * 32;
		lz = cz - rz * 32;
	}
	else
	{
		if (!regionFile)
		{
			regionFile = new RegionFile(levelPath);
			if (!regionFile->open())
			{
				delete regionFile;
				regionFile = NULL;
				return;
			}
		}
		rf = regionFile;
	}

	// Write chunk
	RakNet::BitStream chunkData;
	chunkData.Write((const char*)levelChunk->getBlockData(), CHUNK_BLOCK_COUNT);
	chunkData.Write((const char*)levelChunk->data.data, CHUNK_BLOCK_COUNT / 2);

	chunkData.Write((const char*)levelChunk->skyLight.data, CHUNK_BLOCK_COUNT / 2);
	chunkData.Write((const char*)levelChunk->blockLight.data, CHUNK_BLOCK_COUNT / 2);

	chunkData.Write((const char*)levelChunk->updateMap, CHUNK_COLUMNS);

	// Infinite-world chunks store their entities in the chunk record.  Players
	// remain in player.dat/level.dat and are deliberately excluded here.
	if (level->isInfinite()) {
		ListTag* entityTags = new ListTag();
		int entityCount = 0;
		for (unsigned int i = 0; i < level->entities.size(); ++i) {
			Entity* entity = level->entities[i];
			if (entity == NULL || entity->isPlayer() ||
				Mth::floor(entity->x / 16.0f) != cx ||
				Mth::floor(entity->z / 16.0f) != cz)
				continue;

			CompoundTag* tag = new CompoundTag();
			if (entity->save(tag)) {
				entityTags->add(tag);
				++entityCount;
			} else {
				delete tag;
			}
		}

		ListTag* tileEntityTags = new ListTag();
		int tileEntityCount = 0;
		const LevelChunk::TEMap& chunkTileEntities = levelChunk->getTileEntityMap();
		for (LevelChunk::TEMapCIterator it = chunkTileEntities.begin();
			it != chunkTileEntities.end(); ++it) {
			TileEntity* tileEntity = it->second;
			if (tileEntity == NULL || tileEntity->isRemoved() ||
				!tileEntity->shouldSave())
				continue;

			CompoundTag* tag = new CompoundTag();
			if (tileEntity->save(tag)) {
				tileEntityTags->add(tag);
				++tileEntityCount;
			} else {
				delete tag;
			}
		}

		// Preserve entities that were read from this chunk but have not yet been
		// instantiated by the deferred loader.
		const int64_t entityKey =
			((int64_t)(uint32_t)cx << 32) | (uint32_t)cz;
		auto pending = pendingChunkEntityTags.find(entityKey);
		if (pending != pendingChunkEntityTags.end()) {
			for (std::size_t i = 0; i < pending->second.size(); ++i) {
				if (pending->second[i] != NULL) {
					entityTags->add(static_cast<CompoundTag*>(
						pending->second[i]->copy()));
					++entityCount;
				}
			}
		}

		CompoundTag chunkMetadata;
		chunkMetadata.put("Entities", entityTags);
		chunkMetadata.put("TileEntities", tileEntityTags);
		chunkMetadata.putByte("TerrainPopulated",
			levelChunk->terrainPopulated ? 1 : 0);

		RakNet::BitStream entityData;
		RakDataOutput entityOutput(entityData);
		NbtIo::write(&chunkMetadata, &entityOutput);
		const int entityBytes = entityData.GetNumberOfBytesUsed();

		chunkData.Write(ChunkVersion_Entity);
		chunkData.Write(entityBytes);
		if (entityBytes > 0)
			chunkData.Write((const char*)entityData.GetData(), entityBytes);

		chunkMetadata.deleteChildren();
		LOGI("Saving chunk entities: %d,%d count=%d bytes=%d\n",
			cx, cz, entityCount + tileEntityCount, entityBytes);
	}

	const bool writeSucceeded = rf->writeChunk(lx, lz, chunkData);
	if (writeSucceeded)
	{
		// The chunk is clean only after the complete record, including its
		// optional entity and tile-entity data, has reached the region file.
		levelChunk->createdFromSave = true;
		levelChunk->unsaved = false;
	}

	//LOGI("Saved chunk (%d, %d)\n", levelChunk->x, levelChunk->z);
}

LevelChunk* ExternalFileLevelStorage::load(Level* level, int x, int z)
{
	int lx = x, lz = z; // local coords within the region file

	RegionFile* rf;
	if (level->isInfinite())
	{
		rf = getOrOpenRegion(x, z);
		if (!rf) return NULL;
		int rx = floorDivBy32(x);
		int rz = floorDivBy32(z);
		lx = x - rx * 32;
		lz = z - rz * 32;
	}
	else
	{
		if (!regionFile)
		{
			regionFile = new RegionFile(levelPath);
			if (!regionFile->open())
			{
				delete regionFile;
				regionFile = NULL;
				return NULL;
			}
			// Pre-read the entire region file into memory so all subsequent
			// readChunk() calls are pure memory accesses with no fseek/fread.
			// Call finishPreload() once the chunk load loop is complete.
			regionFile->preloadToMemory();
		}
		rf = regionFile;
	}

	RakNet::BitStream* chunkData = NULL;
	if (!rf->readChunk(lx, lz, &chunkData))
	{
		//LOGI("Failed to read data for %d, %d\n", x, z);
		return NULL;
	}

	chunkData->ResetReadPointer();

	unsigned char* blockIds = new unsigned char[CHUNK_BLOCK_COUNT];
	chunkData->Read((char*)blockIds, CHUNK_BLOCK_COUNT);

	LevelChunk* levelChunk = new LevelChunk(level, blockIds, x, z);
	chunkData->Read((char*)levelChunk->data.data, CHUNK_BLOCK_COUNT / 2);
	if (loadedStorageVersion >= ChunkVersion_Light) {
		chunkData->Read((char*)levelChunk->skyLight.data, CHUNK_BLOCK_COUNT / 2);
		chunkData->Read((char*)levelChunk->blockLight.data, CHUNK_BLOCK_COUNT / 2);
	}
	chunkData->Read((char*)levelChunk->updateMap, CHUNK_COLUMNS);

	// The population state and entity lists are stored in the optional chunk
	// metadata suffix.  Ordinary entities are instantiated lazily below; tile
	// entities are restored immediately because the chunk owns their map.
	bool savedTerrainPopulated = true;
	if (level->isInfinite()) {
		const unsigned int entityHeaderBytes = sizeof(int) * 2;
		if (chunkData->GetNumberOfUnreadBits() >= entityHeaderBytes * 8) {
			int entityVersion = 0;
			int entityBytes = 0;
			if (chunkData->Read(entityVersion) && chunkData->Read(entityBytes) &&
				entityVersion == ChunkVersion_Entity && entityBytes >= 0 &&
				static_cast<unsigned int>(entityBytes) <=
					chunkData->GetNumberOfUnreadBits() / 8) {
				if (entityBytes > 0) {
					unsigned char* entityBuffer = new unsigned char[entityBytes];
					if (chunkData->Read((char*)entityBuffer, entityBytes)) {
						RakNet::BitStream entityStream(entityBuffer, entityBytes, false);
						RakDataInput entityInput(entityStream);
						CompoundTag* chunkMetadata = NbtIo::read(&entityInput);
						if (chunkMetadata != NULL &&
							chunkMetadata->contains("TerrainPopulated", Tag::TAG_Byte)) {
							savedTerrainPopulated =
								chunkMetadata->getBoolean("TerrainPopulated");
						}

						if (chunkMetadata != NULL &&
							chunkMetadata->contains("Entities", Tag::TAG_List)) {
							ListTag* entityTags = chunkMetadata->getList("Entities");
			if (entityTags->size() > 0) {
				const int64_t entityKey =
					((int64_t)(uint32_t)x << 32) | (uint32_t)z;
				std::vector<CompoundTag*>& pending =
					pendingChunkEntityTags[entityKey];
				for (int i = 0; i < entityTags->size(); ++i) {
					Tag* rawTag = entityTags->get(i);
					if (rawTag != NULL &&
						rawTag->getId() == Tag::TAG_Compound)
						pending.push_back(static_cast<CompoundTag*>(
							rawTag->copy()));
				}
							}
						}

						if (chunkMetadata != NULL &&
							chunkMetadata->contains("TileEntities", Tag::TAG_List)) {
							ListTag* tileEntityTags =
								chunkMetadata->getList("TileEntities");
							for (int i = 0; i < tileEntityTags->size(); ++i) {
								Tag* rawTag = tileEntityTags->get(i);
								if (rawTag == NULL ||
									rawTag->getId() != Tag::TAG_Compound)
									continue;

								TileEntity* tileEntity = TileEntity::loadStatic(
									static_cast<CompoundTag*>(rawTag));
								if (tileEntity == NULL)
									continue;

								if (!levelChunk->hasTileEntityAt(tileEntity)) {
									levelChunk->addTileEntity(tileEntity);
									if (levelChunk->hasTileEntityAt(tileEntity))
										level->tileEntities.push_back(tileEntity);
									else
										delete tileEntity;
								} else {
									delete tileEntity;
								}
							}
						}

						if (chunkMetadata != NULL) {
							chunkMetadata->deleteChildren();
							delete chunkMetadata;
						}
					}
					delete[] entityBuffer;
				}
			}
		}
	}

	delete [] chunkData->GetData();
	delete chunkData;

	//bool dbg = (x == 7 && z == 9);

	//int t = 0;
	//for (int i = 0; i < CHUNK_COLUMNS; ++i) {
	//	char bits = levelChunk->updateMap[i];
	//	t += (bits != 0);
	//	int xx = x * 16 + i%16;
	//	int zz = z * 16 + i/16;
	//	if (dbg && xx == 125 && zz == 152) {
	//		LOGI("xz: %d, %d: %d\n", xx, zz, bits);
	//		for (int j = 0; j < 8; ++j) {
	//			if (bits & (1 << j)) {
	//				LOGI("%d - %d\n", j << 4, ((j+1) << 4) - 1);
	//			}
	//		}
	//	}
	//}

	//
	// Convert LevelChunks here if necessary
	//
	//LOGI("level version: %d: upd: %d - (%d, %d)\n", loadedStorageVersion, t, x, z);
	bool changed = false;

	// Loaded level has old Cloth types (one Tile* per color)
	if (loadedStorageVersion == 1)
		changed |= LevelConverters::v1_ClothIdToClothData(levelChunk);

	// Loaded level is newer than our level - replace all unavailable block types
	//if (loadedStorageVersion > SharedConstants::StorageVersion)
		changed |= LevelConverters::ReplaceUnavailableBlocks(levelChunk);

	// The chunk record already contains complete sky/block light arrays.  Only
	// rebuild the heightmap here so loading does not overwrite serialized light
	// data before the normal lighting system resumes.
	if (loadedStorageVersion >= ChunkVersion_Light) {
		levelChunk->recalcHeightmapOnly();
	} else {
		levelChunk->recalcHeightmap();
		changed = true;
	}
	levelChunk->unsaved = changed;
	levelChunk->terrainPopulated = savedTerrainPopulated;
	levelChunk->createdFromSave = true;

	return levelChunk;
}

void ExternalFileLevelStorage::saveEntities( Level* level, LevelChunk* levelChunk )
{
	// Infinite-world entities are serialized in their owning chunk records.
	// Keep the legacy global file for finite/Old worlds only.
	if (level != NULL && level->isInfinite()) {
		(void)levelChunk;
		lastSavedEntitiesTick = tickCount;
		return;
	}

	lastSavedEntitiesTick = tickCount;
	int count = 0;
	float st = getTimeS();

	// Version 1: Save ALL Entities for all chunks in one structure
	EntityList& entities = level->entities;

	ListTag* entityTags = new ListTag();
	for (unsigned int i = 0; i < entities.size(); ++i) {
		Entity* e = entities[i];

		CompoundTag* tag = new CompoundTag();
		if (e->save(tag)) {
			count++;
			entityTags->add(tag);
		} else
			delete tag;
	}

	// Version 1: Save ALL TileEntities for all chunks in one structure
	TileEntityList& tileEntities = level->tileEntities;
	//TileEntityList keep, dontKeep;
	//partitionTileEntities

	ListTag* tileEntityTags = new ListTag();
	for (unsigned int i = 0; i < tileEntities.size(); ++i) {
		TileEntity* e = tileEntities[i];
		if (!e->shouldSave()) continue;

		CompoundTag* tag = new CompoundTag();
		if (e->save(tag)) {
			count++;
			tileEntityTags->add(tag);
		} else
			delete tag;
	}

	CompoundTag base;
	base.put("Entities", entityTags);
	base.put("TileEntities", tileEntityTags);

	RakNet::BitStream stream;
	RakDataOutput dos(stream);
	NbtIo::write(&base, &dos);
	int numBytes = stream.GetNumberOfBytesUsed();

	FILE* fp = fopen((levelPath + "/entities.dat").c_str(), "wb");
	if (fp) {
		int version = 1;
		fwrite("ENT\0", 1, 4, fp);
		fwrite(&version, sizeof(int), 1, fp);
		fwrite(&numBytes, sizeof(int), 1, fp);
		fwrite(stream.GetData(), 1, numBytes, fp);
		fclose(fp);
	}

	base.deleteChildren();

	float tt = getTimeS() - st;
	LOGI("Time to save %d entities: %f s. Size: %d bytes\n", count, tt, numBytes);
}

void ExternalFileLevelStorage::loadEntities(Level* level, LevelChunk* chunk) {
	// Infinite-world chunks load their entity records from the chunk suffix;
	// reading entities.dat here would duplicate them or reintroduce stale data.
	if (level != NULL && level->isInfinite()) {
		(void)chunk;
		return;
	}

	lastSavedEntitiesTick = tickCount;
	FILE* fp = fopen((levelPath + "/entities.dat").c_str(), "rb");
	if (fp) {
		char header[5];
		int version, numBytes;
		fread(header, 1, 4, fp);
		fread(&version, sizeof(int), 1, fp);
		fread(&numBytes, sizeof(int), 1, fp);

		int left = getRemainingFileSize(fp);
		if (numBytes <= left && numBytes > 0) {
			unsigned char* buf = new unsigned char[numBytes];

			fread(buf, 1, numBytes, fp);

			RakNet::BitStream stream(buf, numBytes, false);
			RakDataInput dis(stream);

			CompoundTag* tag = NbtIo::read(&dis);
			//
			// Read Entity:es
			//
			if (tag->contains("Entities", Tag::TAG_List)) {
				ListTag* entityTags = tag->getList("Entities");
				for (int i = 0; i < entityTags->size(); ++i) {
					Tag* _et = entityTags->get(i);
					if (!_et || _et->getId() != Tag::TAG_Compound) {
						LOGE("Entity tag is either NULL or not a compoundTag: %p : %d!\n", _et, _et?_et->getId() : -1);
						continue;
					}
					CompoundTag* et = (CompoundTag*)_et;
					if (Entity* e = EntityFactory::loadEntity(et, level)) {
						level->addEntity(e);
					}
				}
			}
			//
			// Read TileEntity:s
			//
			if (tag->contains("TileEntities", Tag::TAG_List)) {
				ListTag* tileEntityTags = tag->getList("TileEntities");
				for (int i = 0; i < tileEntityTags->size(); ++i) {
					Tag* _et = tileEntityTags->get(i);
					if (!_et || _et->getId() != Tag::TAG_Compound) {
						LOGE("TileEntity tag is either NULL or not a compoundTag: %p : %d!\n", _et, _et?_et->getId() : -1);
						continue;
					}
					CompoundTag* et = (CompoundTag*)_et;
					if (TileEntity* e = TileEntity::loadStatic(et)) {
						
						LevelChunk* chunk = level->getChunkAt(e->x, e->z);
						if (chunk && !chunk->hasTileEntityAt(e)) {
							LOGI("Adding TileEntity %d to %d, %d, %d\n", e->type, e->x, e->y, e->z);
							chunk->addTileEntity(e);
						} else {
							if (!chunk)
								LOGE("Couldn't find chunk at %d, %d to add %d\n", e->x, e->z, e->type);
							else
								LOGE("Already have TileEntity at %d, %d to add %d\n", e->x, e->z, e->type);
							delete e;
						}
					}
				}
			}

			tag->deleteChildren();
			delete tag;

			delete[] buf;
		}
		LOGI("header: %s, version: %d, bytes: %d (remaining: %d)\n", header, version, numBytes, left);

		//fread(stream.GetData(), 1, numBytes, fp);
		fclose(fp);
	}
}

bool ExternalFileLevelStorage::loadDeferredEntities(Level* level, int maxEntities)
{
	if (level == NULL || !level->isInfinite() || maxEntities <= 0)
		return pendingChunkEntityTags.empty();

	// Only instantiate entities near a player.  Chunk preloading can cover a
	// much larger area than the active simulation range, so restoring all NBT
	// records in one frame would cause a noticeable startup spike.
	static const int EntityActivationRadius = 4;
	struct PendingChunkCandidate {
		int64_t key;
		int chunkX;
		int chunkZ;
		int distanceSquared;
	};

	std::vector<PendingChunkCandidate> candidates;
	candidates.reserve(pendingChunkEntityTags.size());

	for (auto it = pendingChunkEntityTags.begin();
		it != pendingChunkEntityTags.end(); ++it) {
		const int chunkX = static_cast<int>(
			static_cast<uint64_t>(it->first) >> 32);
		const int chunkZ = static_cast<int>(static_cast<uint32_t>(it->first));
		int nearestDistanceSquared = 0x7fffffff;

		for (unsigned int i = 0; i < level->players.size(); ++i) {
			Player* player = level->players[i];
			if (player == NULL || player->removed)
				continue;

			const int playerChunkX = Mth::floor(player->x / 16.0f);
			const int playerChunkZ = Mth::floor(player->z / 16.0f);
			const int dx = Mth::abs(chunkX - playerChunkX);
			const int dz = Mth::abs(chunkZ - playerChunkZ);
			if (dx <= EntityActivationRadius && dz <= EntityActivationRadius) {
				const int distanceSquared = dx * dx + dz * dz;
				if (distanceSquared < nearestDistanceSquared)
					nearestDistanceSquared = distanceSquared;
			}
		}

		if (nearestDistanceSquared != 0x7fffffff)
			candidates.push_back(PendingChunkCandidate{
				it->first, chunkX, chunkZ, nearestDistanceSquared});
	}

	std::sort(candidates.begin(), candidates.end(),
		[](const PendingChunkCandidate& left,
			const PendingChunkCandidate& right) {
			if (left.distanceSquared != right.distanceSquared)
				return left.distanceSquared < right.distanceSquared;
			if (left.chunkX != right.chunkX)
				return left.chunkX < right.chunkX;
			return left.chunkZ < right.chunkZ;
		});

	int loaded = 0;
	for (std::size_t candidateIndex = 0;
		candidateIndex < candidates.size() && loaded < maxEntities;
		++candidateIndex) {
		auto it = pendingChunkEntityTags.find(candidates[candidateIndex].key);
		if (it == pendingChunkEntityTags.end())
			continue;

		std::vector<CompoundTag*>& tags = it->second;
		while (!tags.empty() && loaded < maxEntities) {
			CompoundTag* entityTag = tags.back();
			tags.pop_back();
			if (entityTag != NULL) {
				if (Entity* entity = EntityFactory::loadEntity(entityTag, level))
					level->addEntity(entity, false);
				entityTag->deleteChildren();
				delete entityTag;
			}
			++loaded;
		}

		if (tags.empty())
			pendingChunkEntityTags.erase(it);
	}

	if (loaded > 0)
		LOGI("Loaded %d nearby chunk entities; pending chunks=%u\n",
			loaded, static_cast<unsigned int>(pendingChunkEntityTags.size()));
	return pendingChunkEntityTags.empty();
}

void ExternalFileLevelStorage::saveGame(Level* level) {
	saveEntities(level, NULL);
}

int ExternalFileLevelStorage::savePendingUnsavedChunks( int maxCount ) {
    if (maxCount < 0)
        maxCount = unsavedChunkList.size();

    int count = 0;
    while (count < maxCount && !unsavedChunkList.empty()) {

        UnsavedChunkList::iterator it = unsavedChunkList.begin();
        UnsavedChunkList::iterator remove = unsavedChunkList.begin();
        UnsavedLevelChunk* oldest = &(*it);

        for ( ; it != unsavedChunkList.end(); ++it) {
            if ((*it).addedToList < oldest->addedToList) {
                oldest = &(*it);
                remove = it;
            }
        }
        // Copy the queue record before erasing its list node.
        const UnsavedLevelChunk pending = *oldest;
        LevelChunk* chunk = pending.chunk;
        unsavedChunkList.erase(remove);

        // saveAll() may have written a queued chunk through the direct list
        // before we reach this deferred queue.  Avoid writing that now-clean
        // chunk a second time.
        if (chunk != NULL && chunk->unsaved)
            save(level, chunk);
        ++count;
    }
    return count;
}

void ExternalFileLevelStorage::saveAll( Level* level, std::vector<LevelChunk*>& levelChunks ) {
    ChunkStorage::saveAll(level, levelChunks);
	int numChunks = savePendingUnsavedChunks(-1);
	flush();
    LOGI("Saving %d additional chunks.\n", numChunks);
}

void ExternalFileLevelStorage::flush()
{
	if (regionFile)
		regionFile->flush();
	for (auto& kv : regionFiles)
		if (kv.second)
			kv.second->flush();
	if (entitiesFile)
		entitiesFile->flush();
}

void ExternalFileLevelStorage::finishPreload()
{
	if (regionFile)
		regionFile->freeMemoryCache();
	for (auto& kv : regionFiles)
		if (kv.second) kv.second->freeMemoryCache();
}

#endif /*DEMO_MODE*/
