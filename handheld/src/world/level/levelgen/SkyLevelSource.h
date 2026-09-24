#ifndef NET_MINECRAFT_WORLD_LEVEL_LEVELGEN__SkyLevelSource_H__
#define NET_MINECRAFT_WORLD_LEVEL_LEVELGEN__SkyLevelSource_H__

#include "../chunk/ChunkSource.h"
#include "synth/PerlinNoise.h"
#include <map>
#include <utility>

class Level;
class LevelChunk;

class SkyLevelSource: public ChunkSource
{
public:
    SkyLevelSource(Level* level, long seed, bool spawnMobs);
    virtual ~SkyLevelSource();

    virtual bool hasChunk(int x, int z);
    virtual LevelChunk* create(int x, int z);
    virtual LevelChunk* getChunk(int x, int z);
    virtual void postProcess(ChunkSource* parent, int x, int z);
    virtual bool tick();
    virtual bool shouldSave();
    virtual std::string gatherStats();
    virtual Biome::MobList getMobsAt(const MobCategory& category, int x, int y, int z);

private:
    // The chunk is still 16 x 128 x 16; these are density sampling steps.
    static const int CELL_WIDTH = 8;
    static const int CELL_HEIGHT = 4;
    static const int X_SIZE = 3;
    static const int Y_SIZE = 33;
    static const int DENSITY_COUNT = X_SIZE * X_SIZE * Y_SIZE;

    void getHeights(int x, int z);
    void prepareHeights(int x, int z, unsigned char* blocks);
    void buildSurfaces(int x, int z, unsigned char* blocks, Biome** biomes);

    Level* level;
    bool spawnMobs;
    Random random;
    PerlinNoise lperlinNoise1;
    PerlinNoise lperlinNoise2;
    PerlinNoise perlinNoise1;
    PerlinNoise surfaceNoise;
    PerlinNoise forestNoise;
    float buffer[DENSITY_COUNT];
    float ar[DENSITY_COUNT];
    float br[DENSITY_COUNT];
    float pnr[DENSITY_COUNT];
    float depthBuffer[16 * 16];

    // Non-owning, like RandomLevelSource: ChunkCache deletes the chunks.
    // Store coordinates themselves so distant infinite chunks cannot alias.
    std::map<std::pair<int, int>, LevelChunk*> chunkMap;
};

#endif
