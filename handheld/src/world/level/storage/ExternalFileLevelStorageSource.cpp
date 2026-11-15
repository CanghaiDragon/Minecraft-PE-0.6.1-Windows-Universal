#if !defined(DEMO_MODE) && !defined(APPLE_DEMO_PROMOTION)

#include "LevelData.h"
#include "ExternalFileLevelStorageSource.h"
#include "ExternalFileLevelStorage.h"
#include "RegionFile.h"
#include "FolderMethods.h"
#include "../../../platform/file.h"
#include "../../../util/StringUtils.h"
#include "../../../SharedConstants.h"
#include "../LevelConstants.h"
#include "../tile/Tile.h"
#include "../../../nbt/NbtIo.h"
#include "../../../util/RakDataIO.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sys/types.h>

#ifndef WIN32
#include <dirent.h>
#endif

#ifdef __APPLE__
#include "MoveFolder.h"
#endif

static const char ILLEGAL_FILE_CHARACTERS[] = {
	'/', '\n', '\r', '\t', '\0', '\f', '`', '?', '*', '\\', '<', '>', '|', '\"', ':'
};

namespace {

struct RegionCoordinate {
	int x;
	int z;
};

typedef std::unordered_map<int64_t, std::vector<CompoundTag*> >
	LegacyTileEntityMap;

static const int LegacyOutputChunkEntityVersion = 2;

static inline int64_t legacyChunkKey(int x, int z)
{
	return ((int64_t)(uint32_t)x << 32) | (uint32_t)z;
}

static inline int floorDivBy16(int value)
{
	return value < 0 ? (value - 15) / 16 : value / 16;
}

static void deleteLegacyTileEntities(LegacyTileEntityMap& tileEntities)
{
	for (auto& entry : tileEntities) {
		for (std::size_t i = 0; i < entry.second.size(); ++i) {
			if (entry.second[i] != NULL) {
				entry.second[i]->deleteChildren();
				delete entry.second[i];
			}
		}
	}
	tileEntities.clear();
}

static bool loadLegacyTileEntities(const std::string& directory,
	LegacyTileEntityMap& tileEntities, int& tileEntityCount)
{
	tileEntities.clear();
	tileEntityCount = 0;
	const std::string filename = directory + "/entities.dat";
	FILE* file = std::fopen(filename.c_str(), "rb");
	if (file == NULL)
		return true; // Old worlds without entities.dat are valid.

	bool success = false;
	do {
		char header[4];
		int version = 0;
		int numBytes = 0;
		if (std::fread(header, 1, sizeof(header), file) != sizeof(header) ||
			std::memcmp(header, "ENT\0", sizeof(header)) != 0 ||
			std::fread(&version, sizeof(version), 1, file) != 1 ||
			std::fread(&numBytes, sizeof(numBytes), 1, file) != 1)
			break;

		const int remainingBytes = getRemainingFileSize(file);
		if (version != 1 || numBytes <= 0 || numBytes > remainingBytes)
			break;

		std::vector<unsigned char> buffer((std::size_t)numBytes);
		if (std::fread(buffer.data(), 1, (std::size_t)numBytes, file) !=
			(std::size_t)numBytes)
			break;

		RakNet::BitStream stream(buffer.data(), numBytes, false);
		RakDataInput input(stream);
		CompoundTag* root = NbtIo::read(&input);
		if (root == NULL)
			break;

		if (root->contains("TileEntities", Tag::TAG_List)) {
			ListTag* list = root->getList("TileEntities");
			for (int i = 0; i < list->size(); ++i) {
				Tag* raw = list->get(i);
				if (raw == NULL || raw->getId() != Tag::TAG_Compound)
					continue;

				CompoundTag* tag = static_cast<CompoundTag*>(raw);
				if (!tag->contains("x", Tag::TAG_Int) ||
					!tag->contains("y", Tag::TAG_Int) ||
					!tag->contains("z", Tag::TAG_Int))
					continue;

				const int x = tag->getInt("x");
				const int z = tag->getInt("z");
				const int chunkX = floorDivBy16(x);
				const int chunkZ = floorDivBy16(z);
				tileEntities[legacyChunkKey(chunkX, chunkZ)].push_back(
					static_cast<CompoundTag*>(tag->copy()));
				++tileEntityCount;
			}
		}

		root->deleteChildren();
		delete root;
		success = true;
	} while (false);

	std::fclose(file);
	if (!success)
		deleteLegacyTileEntities(tileEntities);
	return success;
}

static bool parseRegionFileName(const char* name, RegionCoordinate& result)
{
	if (name == NULL)
		return false;

	const std::string filename(name);
	const std::string prefix = "chunks.r.";
	const std::string suffix = ".dat";
	if (filename.compare(0, prefix.size(), prefix) != 0 ||
		filename.size() <= prefix.size() + suffix.size() ||
		filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0)
		return false;

	const std::string coordinates = filename.substr(
		prefix.size(), filename.size() - prefix.size() - suffix.size());
	const std::size_t separator = coordinates.find('.');
	if (separator == std::string::npos ||
		coordinates.find('.', separator + 1) != std::string::npos)
		return false;

	const std::string xText = coordinates.substr(0, separator);
	const std::string zText = coordinates.substr(separator + 1);
	if (xText.empty() || zText.empty())
		return false;

	char* xEnd = NULL;
	char* zEnd = NULL;
	const long long x = std::strtoll(xText.c_str(), &xEnd, 10);
	const long long z = std::strtoll(zText.c_str(), &zEnd, 10);
	if (xEnd == xText.c_str() || *xEnd != '\0' ||
		zEnd == zText.c_str() || *zEnd != '\0' ||
		x < static_cast<long long>((std::numeric_limits<int>::min)()) ||
		x > static_cast<long long>((std::numeric_limits<int>::max)()) ||
		z < static_cast<long long>((std::numeric_limits<int>::min)()) ||
		z > static_cast<long long>((std::numeric_limits<int>::max)()))
		return false;

	result.x = static_cast<int>(x);
	result.z = static_cast<int>(z);
	return true;
}

static bool collectRegionFiles(const std::string& directory,
	std::vector<RegionCoordinate>& regions)
{
	regions.clear();

#ifdef WIN32
	const std::string pattern = directory + "/chunks.r.*.*.dat";
	WIN32_FIND_DATAA fileData;
	HANDLE handle = FindFirstFileA(pattern.c_str(), &fileData);
	if (handle == INVALID_HANDLE_VALUE)
		return true;

	do {
		if ((fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
			RegionCoordinate region;
			if (parseRegionFileName(fileData.cFileName, region))
				regions.push_back(region);
		}
	} while (FindNextFileA(handle, &fileData));
	FindClose(handle);
#else
	DIR* dir = opendir(directory.c_str());
	if (dir == NULL)
		return false;

	struct dirent* entry = NULL;
	while ((entry = readdir(dir)) != NULL) {
		RegionCoordinate region;
		if (parseRegionFileName(entry->d_name, region))
			regions.push_back(region);
	}
	closedir(dir);
#endif

	std::sort(regions.begin(), regions.end(),
		[](const RegionCoordinate& left, const RegionCoordinate& right) {
			if (left.x != right.x)
				return left.x < right.x;
			return left.z < right.z;
		});
	regions.erase(std::unique(regions.begin(), regions.end(),
		[](const RegionCoordinate& left, const RegionCoordinate& right) {
			return left.x == right.x && left.z == right.z;
		}), regions.end());
	return true;
}

static bool copyBinaryFile(const std::string& source, const std::string& target)
{
	FILE* input = std::fopen(source.c_str(), "rb");
	if (input == NULL)
		return false;

	FILE* output = std::fopen(target.c_str(), "wb");
	if (output == NULL) {
		std::fclose(input);
		return false;
	}

	unsigned char buffer[64 * 1024];
	bool success = true;
	while (!std::feof(input)) {
		const std::size_t bytesRead = std::fread(buffer, 1, sizeof(buffer), input);
		if (bytesRead > 0 && std::fwrite(buffer, 1, bytesRead, output) != bytesRead) {
			success = false;
			break;
		}
		if (std::ferror(input)) {
			success = false;
			break;
		}
	}

	if (std::fclose(output) != 0)
		success = false;
	std::fclose(input);
	if (!success)
		remove(target.c_str());
	return success;
}

static inline void setNibble(std::vector<unsigned char>& data, int index, int value)
{
	const int slot = index >> 1;
	if ((index & 1) == 0)
		data[slot] = (unsigned char)((data[slot] & 0xf0) | (value & 0x0f));
	else
		data[slot] = (unsigned char)((data[slot] & 0x0f) | ((value & 0x0f) << 4));
}

static bool convertLegacyBlockIds(std::vector<unsigned char>& blocks,
	std::vector<unsigned char>& data, int storageVersion)
{
	bool changed = false;

	// Keep the same conversion used by normal chunk loading for old storage
	// version 1 worlds.
	if (storageVersion == 1) {
		const unsigned char newClothId = Tile::cloth->id;
		for (int i = 0; i < CHUNK_BLOCK_COUNT; ++i) {
			const unsigned char oldTile = blocks[i];
			if (oldTile >= 101 && oldTile <= 115) {
				const int color = 0xf - (oldTile - 101);
				blocks[i] = newClothId;
				setNibble(data, i, color);
				changed = true;
			}
		}
	}

	// Match the current loader's unavailable-block handling so the converted
	// chunk does not remain dirty merely because it was opened once.
	for (int i = 0; i < CHUNK_BLOCK_COUNT; ++i) {
		const unsigned char oldTile = blocks[i];
		const unsigned char newTile = Tile::transformToValidBlockId(oldTile);
		if (oldTile != newTile) {
			blocks[i] = newTile;
			changed = true;
		}
	}
	return changed;
}

static void rebuildLegacySkyLight(const std::vector<unsigned char>& blocks,
	std::vector<unsigned char>& skyLight)
{
	// Storage versions before ChunkVersion_Light did not serialize light
	// arrays. Reproduce the old LevelChunk heightmap sky-light pass without
	// constructing a live Level, which is not available during offline copy.
	for (int x = 0; x < CHUNK_WIDTH; ++x) {
		for (int z = 0; z < CHUNK_DEPTH; ++z) {
			int brightness = 15;
			const int column = (x << 11) | (z << 7);
			for (int y = LEVEL_HEIGHT - 1; y >= 0 && brightness > 0; --y) {
				brightness -= Tile::lightBlock[blocks[column + y] & 0xff];
				if (brightness > 0)
					setNibble(skyLight, column + y, brightness);
			}
		}
	}
}

static bool convertLegacyChunk(RegionFile& source, RegionFile& target,
	int regionX, int regionZ, int localX, int localZ, int storageVersion,
	const LegacyTileEntityMap& tileEntities,
	std::unordered_set<int64_t>& writtenChunks, int& convertedChunks,
	volatile int* progress, int workIndex, int totalWork)
{
	RakNet::BitStream* sourceData = NULL;
	if (!source.readChunk(localX, localZ, &sourceData)) {
		if (progress != NULL && totalWork > 0)
			*progress = 5 + (workIndex * 85) / totalWork;
		return true; // Empty slot in the region file.
	}

	bool success = false;
	do {
		sourceData->ResetReadPointer();
		std::vector<unsigned char> blocks(CHUNK_BLOCK_COUNT);
		std::vector<unsigned char> data(CHUNK_BLOCK_COUNT / 2);
		std::vector<unsigned char> skyLight(CHUNK_BLOCK_COUNT / 2);
		std::vector<unsigned char> blockLight(CHUNK_BLOCK_COUNT / 2);
		std::vector<unsigned char> updateMap(CHUNK_COLUMNS);

		const int blockBytes = CHUNK_BLOCK_COUNT;
		const int nibbleBytes = CHUNK_BLOCK_COUNT / 2;
		const int updateBytes = CHUNK_COLUMNS;
		const int minimumBytes = blockBytes + nibbleBytes + updateBytes;
		if (sourceData->GetNumberOfUnreadBits() < minimumBytes * 8)
			break;

		if (!sourceData->Read((char*)blocks.data(), blockBytes) ||
			!sourceData->Read((char*)data.data(), nibbleBytes))
			break;

		if (storageVersion >= 1) {
			if (sourceData->GetNumberOfUnreadBits() <
				(nibbleBytes * 2 + updateBytes) * 8)
				break;
			if (!sourceData->Read((char*)skyLight.data(), nibbleBytes) ||
				!sourceData->Read((char*)blockLight.data(), nibbleBytes))
				break;
		} else {
			// The old loader rebuilt sky light for these records. Keep the same
			// behavior in the converted record and leave block light at zero.
			rebuildLegacySkyLight(blocks, skyLight);
		}

		if (!sourceData->Read((char*)updateMap.data(), updateBytes))
			break;

		convertLegacyBlockIds(blocks, data, storageVersion);

		RakNet::BitStream convertedData;
		convertedData.Write((const char*)blocks.data(), blockBytes);
		convertedData.Write((const char*)data.data(), nibbleBytes);
		convertedData.Write((const char*)skyLight.data(), nibbleBytes);
		convertedData.Write((const char*)blockLight.data(), nibbleBytes);
		convertedData.Write((const char*)updateMap.data(), updateBytes);

		const int globalChunkX = regionX * 32 + localX;
		const int globalChunkZ = regionZ * 32 + localZ;
		const int64_t chunkKey = legacyChunkKey(globalChunkX, globalChunkZ);
		ListTag* entityTags = new ListTag();
		ListTag* tileEntityTags = new ListTag();
		LegacyTileEntityMap::const_iterator tileEntitiesAtChunk =
			tileEntities.find(chunkKey);
		if (tileEntitiesAtChunk != tileEntities.end()) {
			for (std::size_t i = 0;
				i < tileEntitiesAtChunk->second.size(); ++i) {
				if (tileEntitiesAtChunk->second[i] != NULL)
					tileEntityTags->add(static_cast<CompoundTag*>(
						tileEntitiesAtChunk->second[i]->copy()));
			}
		}

		CompoundTag chunkMetadata;
		chunkMetadata.put("Entities", entityTags);
		chunkMetadata.put("TileEntities", tileEntityTags);
		chunkMetadata.putByte("TerrainPopulated", 1);
		RakNet::BitStream entityData;
		RakDataOutput entityOutput(entityData);
		NbtIo::write(&chunkMetadata, &entityOutput);
		const int entityBytes = entityData.GetNumberOfBytesUsed();
		convertedData.Write(LegacyOutputChunkEntityVersion);
		convertedData.Write(entityBytes);
		if (entityBytes > 0)
			convertedData.Write((const char*)entityData.GetData(), entityBytes);
		chunkMetadata.deleteChildren();
		writtenChunks.insert(chunkKey);

		if (!target.writeChunk(localX, localZ, convertedData))
			break;

		++convertedChunks;
		if (progress != NULL && totalWork > 0)
			*progress = 5 + (workIndex * 85) / totalWork;
		LOGI("Converted legacy chunk (%d, %d)\n", globalChunkX, globalChunkZ);
		success = true;
	} while (false);

	delete[] sourceData->GetData();
	delete sourceData;
	return success;
}

static bool convertLegacyRegion(const std::string& sourceDirectory,
	const std::string& targetDirectory, const RegionCoordinate& region,
	int storageVersion, const LegacyTileEntityMap& tileEntities,
	std::unordered_set<int64_t>& writtenChunks, int& convertedChunks,
	volatile int* progress, int regionIndex, int totalRegions)
{
	const std::string filename = "chunks.r." + std::to_string(region.x) +
		"." + std::to_string(region.z) + ".dat";
	const std::string sourcePath = sourceDirectory + "/" + filename;
	const std::string targetPath = targetDirectory + "/" + filename;

	RegionFile source(sourcePath, true, false);
	RegionFile target(targetPath, true, true);
	if (!source.open() || !target.open())
		return false;

	for (int localZ = 0; localZ < 32; ++localZ) {
		for (int localX = 0; localX < 32; ++localX) {
			const int workIndex = regionIndex * 1024 + localZ * 32 + localX;
			const int totalWork = totalRegions * 1024;
			if (!convertLegacyChunk(source, target, region.x, region.z,
				localX, localZ, storageVersion, tileEntities, writtenChunks,
				convertedChunks, progress, workIndex, totalWork))
				return false;
		}
	}

	target.flush();
	return true;
}

static std::string getUniqueTargetPath(const std::string& basePath,
	const std::string& targetIdBase)
{
	std::string targetId = targetIdBase;
	std::string targetPath = basePath + "/" + targetId;
	while (exists(targetPath.c_str())) {
		targetId += "_";
		targetPath = basePath + "/" + targetId;
	}
	return targetPath;
}

static std::string getConversionTargetPath(const std::string& basePath,
	const std::string& levelId)
{
	return getUniqueTargetPath(basePath, levelId + "_converted");
}

} // namespace

ExternalFileLevelStorageSource::ExternalFileLevelStorageSource(const std::string& externalPath, const std::string& temporaryFilesPath)
:	_hasTempDirectory(temporaryFilesPath != externalPath)
{
#ifndef STANDALONE_SERVER
	const char* p0 = "/games";
	const char* p1 = "/com.mojang";
	const char* p2 = "/minecraftWorlds";
	const char* tree[] = {
		p0, p1, p2
	};

	int treeLength = sizeof(tree) / sizeof(tree[0]);

	createTree(externalPath.c_str(), tree, treeLength);
	
	if (hasTempDirectory())
		createTree(temporaryFilesPath.c_str(), tree, treeLength);

	basePath = externalPath + p0 + p1 + p2;
	tmpBasePath = temporaryFilesPath + p0 + p1 + p2;
#else
	basePath = externalPath;
	tmpBasePath = temporaryFilesPath;
#endif
}

void ExternalFileLevelStorageSource::addLevelSummaryIfExists(LevelSummaryList& dest, const char* dirName)
{
	std::string directory = basePath;
	directory += "/";
	directory += dirName;

	LevelData levelData;

	if (ExternalFileLevelStorage::readLevelData(directory, levelData))
	{
		LevelSummary summary;
		summary.id = dirName;
		summary.name = levelData.levelName;
		summary.lastPlayed = levelData.getLastPlayed();
		summary.sizeOnDisk = (unsigned int)levelData.getSizeOnDisk();
		summary.gameType = levelData.getGameType();

		dest.push_back(summary);
	}
}

void ExternalFileLevelStorageSource::getLevelList(LevelSummaryList& dest)
{
#ifdef WIN32
	LOGI("ARM diag: getLevelList basePath=%s\n", basePath.c_str());

	WIN32_FIND_DATAA fileData;
	HANDLE hFind;

	std::string searchString = basePath;
	searchString += "/*";

	hFind = FindFirstFileA(searchString.c_str(), &fileData);
	LOGI("ARM diag: FindFirstFile handle=%p\n", (void*)hFind);
	if (hFind != INVALID_HANDLE_VALUE)  {
		do {
			if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				addLevelSummaryIfExists(dest, fileData.cFileName);

		} while (FindNextFileA(hFind, &fileData));
		FindClose(hFind);
	} 
	LOGI("ARM diag: getLevelList scan end\n");


#else
	DIR *dp;
	struct dirent *dirp;
	if((dp  = opendir(basePath.c_str())) == NULL) {
		LOGI("Error listing base folder %s: %d", basePath.c_str(), _errno());
		return;
	}

	while ((dirp = readdir(dp)) != NULL) {
		if (dirp->d_type == DT_DIR)
		{
			addLevelSummaryIfExists(dest, dirp->d_name);
		}
	}
	closedir(dp);
#endif

}

LevelStorage* ExternalFileLevelStorageSource::selectLevel(const std::string& levelId, bool createPlayerDir) {
	return new ExternalFileLevelStorage(levelId, getFullPath(levelId));
}

void ExternalFileLevelStorageSource::deleteLevel( const std::string& levelId )
{
	std::string path = getFullPath(levelId);
	if (!DeleteDirectory(path)) { // If we couldn't delete whole folder, try to remove chunks..
		remove((path + "/chunks.dat").c_str());
		remove((path + "/player.dat").c_str());
		remove((path + "/level.dat").c_str());
	}
}

static std::string getUniqueLevelName(const LevelSummaryList& levels, const std::string& level )
{
	std::set<std::string> Set;
	for (unsigned int i = 0; i < levels.size(); ++i)
		Set.insert(levels[i].id);

	std::string s = level;
	while ( Set.find(s) != Set.end() )
		s += "-";
	return s;
}

void ExternalFileLevelStorageSource::renameLevel( const std::string& oldLevelId_, const std::string& newLevelName_ )
{
#define _renFULLPATH(s) ((basePath + "/" + s).c_str())
    bool isTempFile = (TempLevelId == oldLevelId_);
    std::string oldFolder = getFullPath(oldLevelId_);

	if (_access(oldFolder.c_str(), 0) != 0) {
		LOGI("Couldn't access %s\n", oldFolder.c_str());
		return;
	}

	std::string levelName = Util::stringTrim(newLevelName_);
	std::string levelId = levelName;
	for (int i = 0; i < sizeof(ILLEGAL_FILE_CHARACTERS) / sizeof(char); ++i)
		levelId = Util::stringReplace(levelId, std::string(1, ILLEGAL_FILE_CHARACTERS[i]), "");

	LevelSummaryList levels;
	getLevelList(levels);
	levelId = getUniqueLevelName(levels, levelId);

	bool couldRename = false;
	if (hasTempDirectory() && isTempFile) {
#ifdef __APPLE__
		std::string newFolder = basePath + "/" + levelId;
		moveFolder(oldFolder, newFolder);
        couldRename = (_access(newFolder.c_str(), 0) == 0);
#endif
	}

    couldRename = couldRename || rename(_renFULLPATH(oldLevelId_), _renFULLPATH(levelId)) == 0;
	if (!couldRename) // != 0: fail
		levelId = oldLevelId_; // Try to rewrite the level name anyway

	// Rename the level name and write back to file
	LevelData levelData;
	ExternalFileLevelStorage::readLevelData(_renFULLPATH(levelId), levelData);
	levelData.setLevelName(newLevelName_);
	// Renaming must not turn an old Infinite/Sky world into a current-format
	// world. Preserve its original level.dat version and marker state; the
	// explicit Convert action is responsible for changing storage format.
	ExternalFileLevelStorage::saveLevelData(_renFULLPATH(levelId), levelData,
		NULL, false, false);
}

std::string ExternalFileLevelStorageSource::getName()
{
	return "External File Level Storage";
}

LevelData* ExternalFileLevelStorageSource::getDataTagFor( const std::string& levelId )
{
	return NULL;
}

bool ExternalFileLevelStorageSource::isNewLevelIdAcceptable( const std::string& levelId )
{
	return true;
}

std::string ExternalFileLevelStorageSource::getFullPath(const std::string& levelId) {
    return ((TempLevelId == levelId)? tmpBasePath : basePath) + "/" + levelId;
}

bool ExternalFileLevelStorageSource::requiresConversion(const std::string& levelId)
{
	LevelData levelData;
	const std::string sourcePath = getFullPath(levelId);
	if (!ExternalFileLevelStorage::readLevelData(sourcePath, levelData))
		return false;

	// The normal loader deliberately rejects unmarked Infinite/Sky/Alpha
	// worlds. They are the only worlds handled by this first conversion pass;
	// finite worlds remain on their original storage path.
	return levelData.isInfinite() &&
		!hasRecognizedUwpStorageMarker(sourcePath);
}

bool ExternalFileLevelStorageSource::isConvertible(const std::string& levelId)
{
	return requiresConversion(levelId);
}

bool ExternalFileLevelStorageSource::canConvertFiniteLevelToInfinite(
	const std::string& levelId)
{
	LevelData levelData;
	const std::string sourcePath = getFullPath(levelId);
	return ExternalFileLevelStorage::readLevelData(sourcePath, levelData) &&
		!levelData.isInfinite() &&
		levelData.getStorageVersion() == SharedConstants::StorageVersion &&
		exists((sourcePath + "/chunks.dat").c_str());
}

bool ExternalFileLevelStorageSource::convertLevel(const std::string& levelId,
	ProgressListener* progress)
{
	(void)progress;
	return convertLevelWithProgress(levelId, NULL);
}

bool ExternalFileLevelStorageSource::convertLevelWithProgress(
	const std::string& levelId, volatile int* progress)
{
	if (progress != NULL)
		*progress = 0;

	const std::string sourcePath = getFullPath(levelId);
	LevelData levelData;
	if (!ExternalFileLevelStorage::readLevelData(sourcePath, levelData) ||
		!levelData.isInfinite() ||
		hasRecognizedUwpStorageMarker(sourcePath)) {
		LOGW("Legacy conversion is not applicable to level %s\n", levelId.c_str());
		return false;
	}

	// Keep the source world untouched. The target receives a unique folder
	// rather than reusing the old level id, so a failed conversion cannot make
	// the original world unreadable.
	const std::string targetPath = getConversionTargetPath(basePath, levelId);
	if (!createFolderIfNotExists(targetPath.c_str())) {
		LOGE("Could not create conversion target %s\n", targetPath.c_str());
		return false;
	}

	bool success = false;
	LegacyTileEntityMap tileEntities;
	do {
		std::vector<RegionCoordinate> regions;
		if (!collectRegionFiles(sourcePath, regions)) {
			LOGE("Could not enumerate legacy region files in %s\n", sourcePath.c_str());
			break;
		}
		if (progress != NULL)
			*progress = 2;

		int tileEntityCount = 0;
		if (!loadLegacyTileEntities(sourcePath, tileEntities,
			tileEntityCount)) {
			LOGE("Could not read legacy entities.dat\n");
			deleteLegacyTileEntities(tileEntities);
			break;
		}
		if (progress != NULL)
			*progress = 5;

		int convertedChunks = 0;
		std::unordered_set<int64_t> writtenChunks;
		bool allRegionsConverted = true;
		for (std::size_t i = 0; i < regions.size(); ++i) {
			if (!convertLegacyRegion(sourcePath, targetPath, regions[i],
				levelData.getStorageVersion(), tileEntities, writtenChunks,
				convertedChunks, progress, (int)i, (int)regions.size())) {
				LOGE("Failed to convert legacy region %d,%d\n",
					regions[i].x, regions[i].z);
				allRegionsConverted = false;
				break;
			}
		}
		if (!allRegionsConverted)
			break;
		if (regions.size() > 0 && convertedChunks == 0) {
			LOGE("Legacy conversion produced no readable chunks\n");
			break;
		}
		for (LegacyTileEntityMap::const_iterator it = tileEntities.begin();
			it != tileEntities.end(); ++it) {
			if (writtenChunks.find(it->first) == writtenChunks.end()) {
				LOGE("Legacy tile entities refer to a missing chunk\n");
				allRegionsConverted = false;
				break;
			}
		}
		if (!allRegionsConverted)
			break;
		LOGI("Loaded %d legacy tile entities\n", tileEntityCount);

		// Read the old player.dat before writing the new level.dat. This keeps
		// the old player position and inventory file separate from the level
		// metadata conversion.
		ExternalFileLevelStorage::readPlayerData(
			sourcePath + "/player.dat", levelData);
		levelData.setStorageVersion(SharedConstants::StorageVersion);
		if (!ExternalFileLevelStorage::writeLevelData(
			targetPath + "/level.dat", levelData, NULL)) {
			LOGE("Could not write converted level.dat\n");
			break;
		}
		if (progress != NULL)
			*progress = 94;

		const std::string sourcePlayer = sourcePath + "/player.dat";
		const std::string targetPlayer = targetPath + "/player.dat";
		if (exists(sourcePlayer.c_str()) &&
			!copyBinaryFile(sourcePlayer, targetPlayer)) {
			LOGE("Could not copy converted player.dat\n");
			break;
		}

		// The marker is the commit point: only a complete new-format directory
		// may become visible to the normal Infinite/Sky loader.
		if (!writeCurrentUwpStorageMarker(targetPath)) {
			LOGE("Could not commit converted storage marker\n");
			break;
		}
		if (progress != NULL)
			*progress = 100;

		LOGI("Converted legacy level %s to %s (%d chunks)\n",
			levelId.c_str(), targetPath.c_str(), convertedChunks);
		success = true;
	} while (false);
	deleteLegacyTileEntities(tileEntities);

	if (!success) {
		// The source remains untouched. The target was created by this call, so
		// remove the incomplete copy rather than leaving a misleading world
		// directory behind.
		DeleteDirectory(targetPath);
	}
	return success;
}

bool ExternalFileLevelStorageSource::convertFiniteLevelToInfiniteWithProgress(
	const std::string& levelId, volatile int* progress)
{
	if (progress != NULL)
		*progress = 0;

	const std::string sourcePath = getFullPath(levelId);
	LevelData levelData;
	if (!ExternalFileLevelStorage::readLevelData(sourcePath, levelData) ||
		levelData.isInfinite() ||
		levelData.getStorageVersion() != SharedConstants::StorageVersion) {
		LOGW("Finite-to-Infinite conversion is not applicable to level %s\n",
			levelId.c_str());
		return false;
	}

	const std::string sourceChunks = sourcePath + "/chunks.dat";
	if (!exists(sourceChunks.c_str())) {
		LOGW("Finite level %s has no chunks.dat\n", levelId.c_str());
		return false;
	}

	// Keep the finite source untouched. The converted world receives a unique
	// folder so both the original finite world and the Infinite copy remain
	// available in the world list.
	const std::string targetPath = getUniqueTargetPath(basePath,
		levelId + "_infinite");
	if (!createFolderIfNotExists(targetPath.c_str())) {
		LOGE("Could not create finite conversion target %s\n",
			targetPath.c_str());
		return false;
	}

	bool success = false;
	LegacyTileEntityMap tileEntities;
	do {
		int tileEntityCount = 0;
		if (!loadLegacyTileEntities(sourcePath, tileEntities, tileEntityCount)) {
			LOGE("Could not read finite entities.dat\n");
			break;
		}
		if (progress != NULL)
			*progress = 5;

		// A finite world stores all 16x16 chunks in one chunks.dat file. Copy
		// those records into the first 32x32 Infinite region, preserving the
		// original block coordinates.
		RegionFile source(sourcePath);
		RegionFile target(targetPath + "/chunks.r.0.0.dat", true, true);
		if (!source.open() || !target.open()) {
			LOGE("Could not open finite conversion region files\n");
			break;
		}

		int convertedChunks = 0;
		std::unordered_set<int64_t> writtenChunks;
		bool allChunksConverted = true;
		for (int localZ = 0; localZ < 16 && allChunksConverted; ++localZ) {
			for (int localX = 0; localX < 16; ++localX) {
				const int workIndex = localZ * 16 + localX;
				if (!convertLegacyChunk(source, target, 0, 0,
					localX, localZ, levelData.getStorageVersion(), tileEntities,
					writtenChunks, convertedChunks, progress, workIndex, 256)) {
					LOGE("Failed to convert finite chunk %d,%d\n",
						localX, localZ);
					allChunksConverted = false;
					break;
				}
			}
		}
		target.flush();
		if (!allChunksConverted || convertedChunks == 0)
			break;

		for (LegacyTileEntityMap::const_iterator it = tileEntities.begin();
			it != tileEntities.end(); ++it) {
			if (writtenChunks.find(it->first) == writtenChunks.end()) {
				LOGE("Finite tile entities refer to a missing chunk\n");
				allChunksConverted = false;
				break;
			}
		}
		if (!allChunksConverted)
			break;

		// Finite worlds normally carry Player in level.dat, but preserve the
		// old player.dat fallback as well. Set Infinite before reading the
		// fallback so finite-world coordinate clamping is not applied.
		levelData.setWorldType(WorldType::Infinite);
		ExternalFileLevelStorage::readPlayerData(
			sourcePath + "/player.dat", levelData);
		levelData.setStorageVersion(SharedConstants::StorageVersion);
		if (!ExternalFileLevelStorage::writeLevelData(
			targetPath + "/level.dat", levelData, NULL)) {
			LOGE("Could not write finite Infinite level.dat\n");
			break;
		}
		if (progress != NULL)
			*progress = 94;

		const std::string sourcePlayer = sourcePath + "/player.dat";
		const std::string targetPlayer = targetPath + "/player.dat";
		if (exists(sourcePlayer.c_str()) &&
			!copyBinaryFile(sourcePlayer, targetPlayer)) {
			LOGE("Could not copy finite player.dat\n");
			break;
		}

		// Commit only after all chunks and metadata have been copied.
		if (!writeCurrentUwpStorageMarker(targetPath)) {
			LOGE("Could not commit finite Infinite storage marker\n");
			break;
		}
		if (progress != NULL)
			*progress = 100;

		LOGI("Converted finite level %s to Infinite level %s (%d chunks, %d tile entities)\n",
			levelId.c_str(), targetPath.c_str(), convertedChunks, tileEntityCount);
		success = true;
	} while (false);
	deleteLegacyTileEntities(tileEntities);

	if (!success)
		DeleteDirectory(targetPath);
	return success;
}


#endif /*DEMO_MODE*/
