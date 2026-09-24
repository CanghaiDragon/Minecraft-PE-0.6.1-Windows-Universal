#include "FixedBiomeSource.h"
#include "Biome.h"
#include "../ChunkPos.h"

FixedBiomeSource::FixedBiomeSource(Biome* biome, float temperature, float downfall)
: BiomeSource(), biome(biome), temperature(temperature), downfall(downfall), capacity(0)
{
}

FixedBiomeSource::~FixedBiomeSource() {
}

void FixedBiomeSource::ensureCapacity(int size) {
    if (capacity >= size) return;
    capacity = size;
    fixedBiomes.resize(size);
    delete[] temperatures;
    temperatures = new float[size];
    delete[] downfalls;
    downfalls = new float[size];
}

Biome* FixedBiomeSource::getBiome(const ChunkPos& chunk) {
    return biome;
}

Biome* FixedBiomeSource::getBiome(int x, int z) {
    return biome;
}

float FixedBiomeSource::getTemperature(int, int) {
    return temperature;
}

float* FixedBiomeSource::getTemperatureBlock(int x, int z, int w, int h) {
    int size = w * h;
    ensureCapacity(size);
    for (int i = 0; i < size; ++i) temperatures[i] = temperature;
    return temperatures;
}

Biome** FixedBiomeSource::getBiomeBlock(int x, int z, int w, int h) {
    int size = w * h;
    ensureCapacity(size);
    for (int i = 0; i < size; ++i) {
        fixedBiomes[i] = biome;
        downfalls[i] = downfall;
        temperatures[i] = temperature;
    }
    return &fixedBiomes[0];
}
