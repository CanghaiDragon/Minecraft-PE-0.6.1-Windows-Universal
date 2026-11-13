#ifndef NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__
#define NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__

#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "../../../raknet/BitStream.h"

typedef std::map<int, bool> FreeSectorMap;

class RegionFile
{
public:
    RegionFile(const std::string& basePath);
    // Construct with a full file path (no "/chunks.dat" appended)
    RegionFile(const std::string& fullFilePath, bool isFullPath);
    // Infinite-world region files can opt into the MCZ1/zlib record codec.
    // The reader still accepts raw records because the writer intentionally
    // falls back to raw storage when compression does not reduce the size.
    RegionFile(const std::string& fullFilePath, bool isFullPath, bool compressChunks);
    virtual ~RegionFile();

    bool open();
    bool readChunk(int x, int z, RakNet::BitStream** destChunkData);
    bool writeChunk(int x, int z, RakNet::BitStream& chunkData);
    void flush();

    // Pre-read the entire region file into memory so subsequent readChunk()
    // calls use memory lookups instead of fseek/fread. Safe to call from any
    // thread; call freeMemoryCache() once all chunks are loaded to reclaim RAM.
    bool preloadToMemory();
    void freeMemoryCache();

private:
    bool write(int sector, const std::vector<unsigned char>& storedData);
    bool encodeChunk(const RakNet::BitStream& chunkData,
        std::vector<unsigned char>& storedData) const;
    bool decodeChunk(const unsigned char* storedData, std::size_t storedSize,
        std::vector<unsigned char>& chunkData) const;
    void close();

    FILE* file;
    std::string filename;
    int* offsets;
    int* emptyChunk;
    FreeSectorMap sectorFree;
    bool _compressChunks;

    // In-memory cache of the whole file (populated by preloadToMemory())
    std::vector<unsigned char> _fileCache;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__*/
