#ifndef NET_MINECRAFT_WORLD_LEVEL_LEVELGEN__AlphaLevelSource_H__
#define NET_MINECRAFT_WORLD_LEVEL_LEVELGEN__AlphaLevelSource_H__

#include "../chunk/ChunkSource.h"
#include "LargeCaveFeature.h"
#include "synth/PerlinNoise.h"

#include <map>
#include <utility>

class Level;
class LevelChunk;

// Java Alpha 1.1.2_01-style terrain source. The block/chunk format remains
// the PE 0.6.1 format; only terrain density, surfaces and population differ.
class AlphaLevelSource: public ChunkSource
{
public:
	static const int CHUNK_HEIGHT = 8;
	static const int CHUNK_WIDTH = 4;
	static const int X_SIZE = 5;
	static const int Y_SIZE = 17;
	static const int DENSITY_COUNT = X_SIZE * X_SIZE * Y_SIZE;

	AlphaLevelSource(Level* level, long seed, bool spawnMobs);
	virtual ~AlphaLevelSource();

	virtual bool hasChunk(int x, int z);
	virtual LevelChunk* create(int x, int z);
	virtual LevelChunk* getChunk(int x, int z);
	virtual void postProcess(ChunkSource* parent, int x, int z);
	virtual bool tick();
	virtual bool shouldSave();
	virtual std::string gatherStats();
	virtual Biome::MobList getMobsAt(const MobCategory& category, int x, int y, int z);

private:
	void prepareHeights(int x, int z, unsigned char* blocks);
	void buildSurfaces(int x, int z, unsigned char* blocks);
	float* getHeights(float* buffer, int x, int y, int z,
		int xSize, int ySize, int zSize);

	Level* level;
	bool spawnMobs;
	Random random;

	PerlinNoise lperlinNoise1;
	PerlinNoise lperlinNoise2;
	PerlinNoise perlinNoise1;
	PerlinNoise perlinNoise2;
	PerlinNoise perlinNoise3;
	PerlinNoise scaleNoise;
	PerlinNoise depthNoise;
	PerlinNoise forestNoise;

	float buffer[DENSITY_COUNT];
	float scaleBuffer[X_SIZE * X_SIZE];
	float depthBuffer[X_SIZE * X_SIZE];
	float pnr[DENSITY_COUNT];
	float ar[DENSITY_COUNT];
	float br[DENSITY_COUNT];
	float sandBuffer[16 * 16];
	float gravelBuffer[16 * 16];
	float surfaceDepthBuffer[16 * 16];

	LargeCaveFeature caveFeature;
	std::map<std::pair<int, int>, LevelChunk*> chunkMap;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_LEVELGEN__AlphaLevelSource_H__*/
