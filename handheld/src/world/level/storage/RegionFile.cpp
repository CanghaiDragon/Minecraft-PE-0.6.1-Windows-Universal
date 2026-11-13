#include "RegionFile.h"
#include "../../../platform/log.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <zlib.h>

const int SECTOR_BYTES = 4096;
const int SECTOR_INTS = SECTOR_BYTES / 4;
const int SECTOR_COLS = 32;

static const unsigned char COMPRESSED_CHUNK_MAGIC[4] = { 'M', 'C', 'Z', '1' };
static const std::size_t COMPRESSED_CHUNK_HEADER_BYTES = 8;
static const char* const REGION_DAT_NAME = "chunks.dat";

static void logAssert(int actual, int expected) {
    if (actual != expected) {
        LOGI("ERROR: I/O operation failed (%d vs %d)\n", actual, expected);
    }
}

RegionFile::RegionFile(const std::string& basePath)
:   file(NULL),
    _compressChunks(false)
{
    filename = basePath;
    filename += "/";
    filename += REGION_DAT_NAME;

    offsets = new int[SECTOR_INTS];

    emptyChunk = new int[SECTOR_INTS];
    memset(emptyChunk, 0, SECTOR_INTS * sizeof(int));
}

RegionFile::RegionFile(const std::string& fullFilePath, bool /*isFullPath*/)
:   file(NULL),
    _compressChunks(false)
{
    filename = fullFilePath;

    offsets = new int[SECTOR_INTS];

    emptyChunk = new int[SECTOR_INTS];
    memset(emptyChunk, 0, SECTOR_INTS * sizeof(int));
}

RegionFile::RegionFile(const std::string& fullFilePath, bool /*isFullPath*/, bool compressChunks)
:   file(NULL),
    _compressChunks(compressChunks)
{
    filename = fullFilePath;

    offsets = new int[SECTOR_INTS];

    emptyChunk = new int[SECTOR_INTS];
    memset(emptyChunk, 0, SECTOR_INTS * sizeof(int));
}

RegionFile::~RegionFile()
{
    close();
    delete [] offsets;
    delete [] emptyChunk;
}

bool RegionFile::open()
{
    close();

    memset(offsets, 0, SECTOR_INTS * sizeof(int));

    // attempt to open file
    file = fopen(filename.c_str(), "r+b");
    if (file)
    {
        // read offset table
        logAssert(fread(offsets, sizeof(int), SECTOR_INTS, file), SECTOR_INTS);

        // mark initial sector as blocked
        sectorFree[0] = false;

        // setup blocked sectors
        for (int sector = 0; sector < SECTOR_INTS; sector++)
        {
            int offset = offsets[sector];
            if (offset)
            {
                int base = offset >> 8;
                int count = offset & 0xff;

                for (int i = 0; i < count; i++)
                {
                    sectorFree[base + i] = false;
                }
            }
        }
    }
    else
    {
        // new world
        file = fopen(filename.c_str(), "w+b");
        if (!file)
        {
            LOGI("Failed to create chunk file %s\n", filename.c_str());
            return false;
        }

        // write sector header (all zeroes)
        logAssert(fwrite(offsets, sizeof(int), SECTOR_INTS, file), SECTOR_INTS);

        // mark initial sector as blocked
        sectorFree[0] = false;
    }

    return file != NULL;
}

void RegionFile::close()
{
    if (file)
    {
        fclose(file);
        file = NULL;
    }
}

void RegionFile::flush()
{
    if (file)
        fflush(file);
}

bool RegionFile::readChunk(int x, int z, RakNet::BitStream** destChunkData)
{
    if (destChunkData == NULL)
        return false;
    *destChunkData = NULL;

    int offset = offsets[x + z * SECTOR_COLS];

    if (offset == 0)
    {
        // not written to file yet
        return false;
    }

    int sectorNum = offset >> 8;
    int length = 0;
    std::vector<unsigned char> storedData;

    if (!_fileCache.empty())
    {
        // Fast path: read directly from the in-memory cache.
        size_t byteOffset = (size_t)sectorNum * SECTOR_BYTES;
        if (byteOffset + sizeof(int) > _fileCache.size())
        {
            LOGI("ERROR: RegionFile cache underrun at sector %d\n", sectorNum);
            return false;
        }
        memcpy(&length, &_fileCache[byteOffset], sizeof(int));
        if (length < (int)sizeof(int) ||
            length > (offset & 0xff) * SECTOR_BYTES)
            return false;
        length -= sizeof(int);
        storedData.resize(length > 0 ? (std::size_t)length : 0);
        if (length > 0)
        {
            size_t dataOffset = byteOffset + sizeof(int);
            if (dataOffset + (size_t)length > _fileCache.size())
            {
                LOGI("ERROR: RegionFile cache data underrun for chunk (%d,%d)\n", x, z);
                return false;
            }
            memcpy(storedData.data(), &_fileCache[dataOffset], length);
        }
    }
    else
    {
        // Slow path: seek + read from file.
        fseek(file, sectorNum * SECTOR_BYTES, SEEK_SET);
        if (fread(&length, sizeof(int), 1, file) != 1)
            return false;
        if (length < (int)sizeof(int) ||
            length > (offset & 0xff) * SECTOR_BYTES)
            return false;
        length -= sizeof(int);
        storedData.resize(length > 0 ? (std::size_t)length : 0);
        if (length > 0 &&
            (int)fread(storedData.data(), 1, length, file) != length)
            return false;
    }

    std::vector<unsigned char> chunkData;
    if (!decodeChunk(storedData.empty() ? NULL : storedData.data(),
        storedData.size(), chunkData))
        return false;

    unsigned char* data = new unsigned char[chunkData.empty() ? 1 : chunkData.size()];
    if (!chunkData.empty())
        memcpy(data, chunkData.data(), chunkData.size());
    *destChunkData = new RakNet::BitStream(data, (int)chunkData.size(), false);
    return true;
}

bool RegionFile::preloadToMemory()
{
    if (!file)
        return false;

    // Determine file size
    if (fseek(file, 0, SEEK_END) != 0)
        return false;
    long fileSize = ftell(file);
    if (fileSize <= 0)
        return false;
    rewind(file);

    _fileCache.resize((size_t)fileSize);
    size_t bytesRead = fread(_fileCache.data(), 1, (size_t)fileSize, file);
    if ((long)bytesRead != fileSize)
    {
        LOGI("ERROR: preloadToMemory only read %zu of %ld bytes\n", bytesRead, fileSize);
        _fileCache.clear();
        return false;
    }

    // Re-read the offset table from the cache (it was already loaded in open(),
    // but now the file pointer is at EOF; restore it so write() still works).
    rewind(file);

    LOGI("RegionFile: preloaded %ld bytes into memory cache\n", fileSize);
    return true;
}

void RegionFile::freeMemoryCache()
{
    std::vector<unsigned char>().swap(_fileCache); // release memory
}

bool RegionFile::writeChunk(int x, int z, RakNet::BitStream& chunkData)
{
    std::vector<unsigned char> storedData;
    if (!encodeChunk(chunkData, storedData))
        return false;

    int size = (int)storedData.size() + sizeof(int);

    int offset = offsets[x + z * SECTOR_COLS];
    int sectorNum = offset >> 8;
    int sectorCount = offset & 0xff;
    int sectorsNeeded = (size / SECTOR_BYTES) + 1;

    if (sectorsNeeded > 256)
    {
        LOGI("ERROR: Chunk is too big to be saved to file\n");
        return false;
    }

    if (sectorNum != 0 && sectorCount == sectorsNeeded) {
        // the sector can be written on top of its old data
        write(sectorNum, storedData);
    }
    else
    {
        // we need a new location

        // mark old sectors as free
        for (int i = 0; i < sectorCount; i++)
        {
            sectorFree[sectorNum + i] = true;
        }

        // find an available slot with enough run length
        int slot = 0;
        int runLength = 0;
        bool extendFile = false;
        while (runLength < sectorsNeeded)
        {
            if (sectorFree.find(slot + runLength) == sectorFree.end())
            {
                extendFile = true;
                break;
            }
            if (sectorFree[slot + runLength] == true)
            {
                runLength++;
            }
            else
            {
                slot = slot + runLength + 1;
                runLength = 0;
            }
        }

        if (extendFile)
        {
            fseek(file, 0, SEEK_END);
            for (int i = 0; i < (sectorsNeeded - runLength); i++)
            {
                fwrite(emptyChunk, sizeof(int), SECTOR_INTS, file);
                sectorFree[slot + i] = true;
            }
        }
        offsets[x + z * SECTOR_COLS] = (slot << 8) | sectorsNeeded;
        // mark slots as taken
        for (int i = 0; i < sectorsNeeded; i++)
        {
            sectorFree[slot + i] = false;
        }

        // write!
        write(slot, storedData);

        // write sector data
        fseek(file, (x + z * SECTOR_COLS) * sizeof(int), SEEK_SET);
        fwrite(&offsets[x + z * SECTOR_COLS], sizeof(int), 1, file);
    }

    return true;
}

bool RegionFile::encodeChunk(const RakNet::BitStream& chunkData,
    std::vector<unsigned char>& storedData) const
{
    const unsigned char* rawData = (const unsigned char*)chunkData.GetData();
    const std::size_t rawSize = (std::size_t)chunkData.GetNumberOfBytesUsed();

    if (_compressChunks && rawSize > 0 && rawSize <= (std::size_t)0xffffffffu)
    {
        uLongf compressedSize = compressBound((uLong)rawSize);
        std::vector<unsigned char> compressed(COMPRESSED_CHUNK_HEADER_BYTES +
            (std::size_t)compressedSize);
        const int result = compress2(
            compressed.data() + COMPRESSED_CHUNK_HEADER_BYTES,
            &compressedSize,
            rawData,
            (uLong)rawSize,
            Z_DEFAULT_COMPRESSION);
        if (result == Z_OK &&
            (std::size_t)compressedSize + COMPRESSED_CHUNK_HEADER_BYTES < rawSize)
        {
            memcpy(compressed.data(), COMPRESSED_CHUNK_MAGIC,
                sizeof(COMPRESSED_CHUNK_MAGIC));
            const uint32_t uncompressedSize = (uint32_t)rawSize;
            memcpy(compressed.data() + sizeof(COMPRESSED_CHUNK_MAGIC),
                &uncompressedSize, sizeof(uncompressedSize));
            compressed.resize(COMPRESSED_CHUNK_HEADER_BYTES +
                (std::size_t)compressedSize);
            storedData.swap(compressed);
            return true;
        }
    }

    storedData.resize(rawSize);
    if (rawSize > 0)
        memcpy(storedData.data(), rawData, rawSize);
    return true;
}

bool RegionFile::decodeChunk(const unsigned char* storedData, std::size_t storedSize,
    std::vector<unsigned char>& chunkData) const
{
    if (storedSize >= COMPRESSED_CHUNK_HEADER_BYTES &&
        memcmp(storedData, COMPRESSED_CHUNK_MAGIC,
            sizeof(COMPRESSED_CHUNK_MAGIC)) == 0)
    {
        uint32_t uncompressedSize = 0;
        memcpy(&uncompressedSize,
            storedData + sizeof(COMPRESSED_CHUNK_MAGIC),
            sizeof(uncompressedSize));
        if (uncompressedSize == 0 ||
            uncompressedSize > (uint32_t)0x7fffffff ||
            storedSize - COMPRESSED_CHUNK_HEADER_BYTES > (std::size_t)0xffffffffu)
            return false;

        chunkData.resize(uncompressedSize);
        uLongf outputSize = (uLongf)uncompressedSize;
        const int result = uncompress(
            chunkData.data(),
            &outputSize,
            storedData + COMPRESSED_CHUNK_HEADER_BYTES,
            (uLong)(storedSize - COMPRESSED_CHUNK_HEADER_BYTES));
        if (result != Z_OK || outputSize != (uLongf)uncompressedSize)
        {
            chunkData.clear();
            return false;
        }
        return true;
    }

    // Raw records are also part of the current format: compression is used
    // only when it makes the record smaller.
    chunkData.resize(storedSize);
    if (storedSize > 0)
        memcpy(chunkData.data(), storedData, storedSize);
    return true;
}

bool RegionFile::write(int sector, const std::vector<unsigned char>& storedData)
{
    fseek(file, sector * SECTOR_BYTES, SEEK_SET);
    int size = (int)storedData.size() + sizeof(int);
    logAssert(fwrite(&size, sizeof(int), 1, file), 1);
    if (!storedData.empty())
        logAssert(fwrite(storedData.data(), 1, storedData.size(), file),
            (int)storedData.size());

    return true;
}
