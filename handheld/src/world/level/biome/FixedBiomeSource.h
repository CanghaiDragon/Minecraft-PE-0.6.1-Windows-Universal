#ifndef NET_MINECRAFT_WORLD_LEVEL_BIOME__FixedBiomeSource_H__
#define NET_MINECRAFT_WORLD_LEVEL_BIOME__FixedBiomeSource_H__

#include "BiomeSource.h"
#include <vector>

// The PE equivalent of Java Beta's WorldChunkManagerHell: one biome and one
// climate value everywhere, with no climate-noise allocation or sampling.
class FixedBiomeSource: public BiomeSource
{
public:
    FixedBiomeSource(Biome* biome, float temperature, float downfall);
    virtual ~FixedBiomeSource();

    virtual Biome* getBiome(const ChunkPos& chunk);
    virtual Biome* getBiome(int x, int z);
    virtual float getTemperature(int x, int z);
    virtual float* getTemperatureBlock(int x, int z, int w, int h);
    virtual Biome** getBiomeBlock(int x, int z, int w, int h);

private:
    void ensureCapacity(int size);

    Biome* biome;
    float temperature;
    float downfall;
    int capacity;
    std::vector<Biome*> fixedBiomes;
};

#endif
