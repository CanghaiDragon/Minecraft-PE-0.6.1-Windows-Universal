#include "AlphaLevelSource.h"

#include "feature/FeatureInclude.h"
#include "../Level.h"
#include "../MobSpawner.h"
#include "../biome/Biome.h"
#include "../biome/BiomeSource.h"
#include "../chunk/LevelChunk.h"
#include "../material/Material.h"
#include "../tile/HeavyTile.h"
#include "../tile/Tile.h"
#include "../../../util/Random.h"

namespace {

static const int WATER_HEIGHT = 64;
static const float NOISE_SCALE = 684.412f;

static bool isSnowCoveredBiome(Level* level, int x, int z) {
	BiomeSource* source = level->getBiomeSource();
	Biome* biome = source != NULL ? source->getBiome(x, z) : NULL;
	return biome != NULL && biome->isSnowCovered();
}

static void placeWinterSurface(Level* level, int xo, int zo) {
	for (int x = 0; x < 16; ++x) {
		for (int z = 0; z < 16; ++z) {
			const int wx = xo + x;
			const int wz = zo + z;
			const int y = level->getTopSolidBlock(wx, wz);
			if (y <= 0 || y >= Level::DEPTH) continue;
			if (!level->isEmptyTile(wx, y, wz)) continue;
			const Material* below = level->getMaterial(wx, y - 1, wz);
			if (below == Material::ice || !below->blocksMotion()) continue;
			if (Tile::topSnow->mayPlace(level, wx, y, wz))
				level->setTileNoUpdate(wx, y, wz, Tile::topSnow->id);
		}
	}
}

static void freezeExposedWater(Level* level, int xo, int zo) {
	for (int x = 0; x < 16; ++x) {
		for (int z = 0; z < 16; ++z) {
			const int wx = xo + x;
			const int wz = zo + z;
			for (int y = Level::DEPTH - 2; y >= 0; --y) {
				const int tile = level->getTile(wx, y, wz);
				if (tile == Tile::water->id || tile == Tile::calmWater->id) {
					// Liquid data 0 is a full source block. Nonzero values encode
					// flowing/falling water; leave those exposed flows liquid.
					if (level->getData(wx, y, wz) == 0
						&& level->isEmptyTile(wx, y + 1, wz)
						&& level->canSeeSky(wx, y + 1, wz))
						level->setTileNoUpdate(wx, y, wz, Tile::ice->id);
					break;
				}
				if (tile != 0 && Tile::tiles[tile]->material->blocksMotion()) break;
			}
		}
	}
}

static void placeOre(Random& random, Level* level, int x, int z, int y, int tile, int size) {
	if (y < 0 || y >= Level::DEPTH) return;
	OreFeature feature(tile, size);
	feature.place(level, &random, x, y, z);
}

struct AlphaGenerationState {
	Level* level;
	bool generating;
	bool falling;
	bool ticking;

	AlphaGenerationState(Level* level)
	:	level(level),
		generating(level->isGeneratingTerrain),
		falling(HeavyTile::instaFall),
		ticking(level->instaTick)
	{
		level->isGeneratingTerrain = true;
		HeavyTile::instaFall = true;
	}

	~AlphaGenerationState() {
		level->isGeneratingTerrain = generating;
		HeavyTile::instaFall = falling;
		level->instaTick = ticking;
	}
};

}

AlphaLevelSource::AlphaLevelSource(Level* level, long seed, bool spawnMobs)
:	level(level),
	spawnMobs(spawnMobs),
	random(seed),
	lperlinNoise1(&random, 16),
	lperlinNoise2(&random, 16),
	perlinNoise1(&random, 8),
	perlinNoise2(&random, 4),
	perlinNoise3(&random, 4),
	scaleNoise(&random, 10),
	depthNoise(&random, 16),
	forestNoise(&random, 8)
{
}

AlphaLevelSource::~AlphaLevelSource() {
}

void AlphaLevelSource::prepareHeights(int xOffs, int zOffs, unsigned char* blocks) {
	const bool winter = isSnowCoveredBiome(level, xOffs * 16 + 8, zOffs * 16 + 8);
	const int cells = 16 / CHUNK_WIDTH;
	getHeights(buffer, xOffs * cells, 0, zOffs * cells,
		X_SIZE, Y_SIZE, X_SIZE);

	for (int xc = 0; xc < cells; ++xc)
		for (int zc = 0; zc < cells; ++zc)
			for (int yc = 0; yc < 128 / CHUNK_HEIGHT; ++yc) {
				const int p = (xc * X_SIZE + zc) * Y_SIZE + yc;
				float s0 = buffer[p];
				float s1 = buffer[p + Y_SIZE];
				float s2 = buffer[p + X_SIZE * Y_SIZE];
				float s3 = buffer[p + (X_SIZE + 1) * Y_SIZE];
				const float s0a = (buffer[p + 1] - s0) / CHUNK_HEIGHT;
				const float s1a = (buffer[p + Y_SIZE + 1] - s1) / CHUNK_HEIGHT;
				const float s2a = (buffer[p + X_SIZE * Y_SIZE + 1] - s2) / CHUNK_HEIGHT;
				const float s3a = (buffer[p + (X_SIZE + 1) * Y_SIZE + 1] - s3) / CHUNK_HEIGHT;

				for (int y = 0; y < CHUNK_HEIGHT; ++y) {
					float left = s0;
					float right = s1;
					const float leftStep = (s2 - s0) / CHUNK_WIDTH;
					const float rightStep = (s3 - s1) / CHUNK_WIDTH;

					for (int x = 0; x < CHUNK_WIDTH; ++x) {
						int offs = ((xc * CHUNK_WIDTH + x) << 11)
							| ((zc * CHUNK_WIDTH) << 7)
							| (yc * CHUNK_HEIGHT + y);
						const float step = (right - left) / CHUNK_WIDTH;
						float value = left;
						for (int z = 0; z < CHUNK_WIDTH; ++z) {
							const int worldY = yc * CHUNK_HEIGHT + y;
							// Alpha uses moving water in its raw generator. The
							// PE tile is kept here so the normal fluid system
							// remains responsible for subsequent updates.
							blocks[offs] = value > 0
								? (unsigned char)Tile::rock->id
								: (worldY < WATER_HEIGHT
									? (unsigned char)((winter && worldY == WATER_HEIGHT - 1)
										? Tile::ice->id : Tile::water->id) : 0);
							offs += 128;
							value += step;
						}
						left += leftStep;
						right += rightStep;
					}

					s0 += s0a;
					s1 += s1a;
					s2 += s2a;
					s3 += s3a;
				}
			}
}

void AlphaLevelSource::buildSurfaces(int xOffs, int zOffs, unsigned char* blocks) {
	const bool winter = isSnowCoveredBiome(level, xOffs * 16 + 8, zOffs * 16 + 8);
	const float s = 1.0f / 32.0f;
	perlinNoise2.getRegion(sandBuffer, (float)(xOffs * 16), (float)(zOffs * 16), 0,
		16, 16, 1, s, s, 1);
	perlinNoise2.getRegion(gravelBuffer, (float)(xOffs * 16), 109.0134f,
		(float)(zOffs * 16), 16, 1, 16, s, 1, s);
	perlinNoise3.getRegion(surfaceDepthBuffer, (float)(xOffs * 16),
		(float)(zOffs * 16), 0, 16, 16, 1, s * 2, s * 2, s * 2);

	for (int x = 0; x < 16; ++x) {
		for (int z = 0; z < 16; ++z) {
			const int column = x + z * 16;
			const bool sand = sandBuffer[column] + random.nextFloat() * 0.2f > 0;
			const bool gravel = gravelBuffer[column] + random.nextFloat() * 0.2f > 3;
			const int runDepth = (int)(surfaceDepthBuffer[column] / 3
				+ 3 + random.nextFloat() * 0.25f);

			int run = -1;
			int top = Tile::grass->id;
			int material = Tile::dirt->id;

			for (int y = 127; y >= 0; --y) {
				const int offs = (z * 16 + x) * 128 + y;

				// Alpha's bedrock test is nextInt(6)-1, producing a
				// zero-to-five-block lower layer.
				if (y <= random.nextInt(6) - 1) {
					blocks[offs] = (unsigned char)Tile::unbreakable->id;
					continue;
				}

				const int old = blocks[offs];
				if (old == 0) {
					run = -1;
				} else if (old == Tile::rock->id) {
					if (run == -1) {
						if (runDepth <= 0) {
							top = 0;
							material = Tile::rock->id;
						} else if (y >= WATER_HEIGHT - 4 && y <= WATER_HEIGHT + 1) {
							top = Tile::grass->id;
							material = Tile::dirt->id;
							if (gravel) {
								top = 0;
								material = Tile::gravel->id;
							}
							if (sand) {
								top = Tile::sand->id;
								material = Tile::sand->id;
							}
						}

						if (y < WATER_HEIGHT && top == 0)
							top = winter && y == WATER_HEIGHT - 1
							? Tile::ice->id : Tile::water->id;

						run = runDepth;
						blocks[offs] = (unsigned char)(y >= WATER_HEIGHT - 1
							? top : material);
					} else if (run > 0) {
						--run;
						blocks[offs] = (unsigned char)material;
					}
				}
			}
		}
	}
}

float* AlphaLevelSource::getHeights(float* output, int x, int y, int z,
	int xSize, int ySize, int zSize)
{
	scaleNoise.getRegion(scaleBuffer, x, z, xSize, zSize, 1.0f, 1.0f, 1.0f);
	depthNoise.getRegion(depthBuffer, x, z, xSize, zSize, 100.0f, 100.0f, 1.0f);
	perlinNoise1.getRegion(pnr, (float)x, (float)y, (float)z,
		xSize, ySize, zSize, NOISE_SCALE / 80.0f,
		NOISE_SCALE / 160.0f, NOISE_SCALE / 80.0f);
	lperlinNoise1.getRegion(ar, (float)x, (float)y, (float)z,
		xSize, ySize, zSize, NOISE_SCALE, NOISE_SCALE, NOISE_SCALE);
	lperlinNoise2.getRegion(br, (float)x, (float)y, (float)z,
		xSize, ySize, zSize, NOISE_SCALE, NOISE_SCALE, NOISE_SCALE);

	int noiseIndex = 0;
	int surfaceIndex = 0;
	for (int xx = 0; xx < xSize; ++xx) {
		for (int zz = 0; zz < zSize; ++zz) {
			float scale = (scaleBuffer[surfaceIndex] + 256.0f) / 512.0f;
			if (scale > 1.0f) scale = 1.0f;

			float depth = depthBuffer[surfaceIndex] / 8000.0f;
			if (depth < 0.0f) depth = -depth;
			depth = depth * 3.0f - 3.0f;

			if (depth < 0.0f) {
				depth /= 2.0f;
				if (depth < -1.0f) depth = -1.0f;
				depth /= 1.4f;
				depth /= 2.0f;
				scale = 0.0f;
			} else {
				if (depth > 1.0f) depth = 1.0f;
				depth /= 6.0f;
			}

			scale += 0.5f;
			depth = depth * ySize / 16.0f;
			const float yCenter = ySize / 2.0f + depth * 4.0f;
			++surfaceIndex;

			for (int yy = 0; yy < ySize; ++yy, ++noiseIndex) {
				float value;
				const float yOffset = ((float)yy - yCenter) * 12.0f / scale;
				float adjustedOffset = yOffset;
				if (adjustedOffset < 0.0f) adjustedOffset *= 4.0f;

				const float a = ar[noiseIndex] / 512.0f;
				const float b = br[noiseIndex] / 512.0f;
				const float blend = (pnr[noiseIndex] / 10.0f + 1.0f) / 2.0f;
				if (blend < 0.0f) value = a;
				else if (blend > 1.0f) value = b;
				else value = a + (b - a) * blend;
				value -= adjustedOffset;

				if (yy > ySize - 4) {
					const float slide = (float)(yy - (ySize - 4)) / 3.0f;
					value = value * (1.0f - slide) - 10.0f * slide;
				}

				output[noiseIndex] = value;
			}
		}
	}

	return output;
}

LevelChunk* AlphaLevelSource::getChunk(int x, int z) {
	const std::pair<int, int> key(x, z);
	std::map<std::pair<int, int>, LevelChunk*>::iterator found = chunkMap.find(key);
	if (found != chunkMap.end()) return found->second;

	// Keep the Alpha chunk-seed constants separate from the PE infinite
	// generator. The current Random class still supplies PE's RNG semantics.
	const long long chunkSeed = (long long)x * 0x4f9939f508LL
		+ (long long)z * 0x1ef1565bd5LL;
	random.setSeed((long)chunkSeed);

	unsigned char* blocks = new unsigned char[LevelChunk::ChunkBlockCount];
	LevelChunk* chunk = new LevelChunk(level, blocks, x, z);
	chunkMap.insert(std::make_pair(key, chunk));

	prepareHeights(x, z, blocks);
	buildSurfaces(x, z, blocks);
	caveFeature.apply(this, level, x, z, blocks, LevelChunk::ChunkBlockCount);
	chunk->recalcHeightmap();
	return chunk;
}

LevelChunk* AlphaLevelSource::create(int x, int z) {
	return getChunk(x, z);
}

bool AlphaLevelSource::hasChunk(int, int) {
	return true;
}

void AlphaLevelSource::postProcess(ChunkSource* parent, int xt, int zt) {
	(void)parent;
	AlphaGenerationState state(level);

	Random populationRandom(level->getSeed());
	const long xScale = populationRandom.nextInt() / 2 * 2 + 1;
	const long zScale = populationRandom.nextInt() / 2 * 2 + 1;
	populationRandom.setSeed((long)(((long long)xt * xScale
		+ (long long)zt * zScale) ^ (long long)level->getSeed()));

	const int xo = xt * 16;
	const int zo = zt * 16;
	Biome* biome = level->getBiomeSource()->getBiome(xo + 16, zo + 16);

	// Alpha's first population pass is dungeon generation. The current PE
	// tree/feature layer has no compatible dungeon + mob-spawner implementation,
	// so it is intentionally left out until that storage path is ported.

	for (int i = 0; i < 10; ++i)
		ClayFeature(32).place(level, &populationRandom,
			xo + populationRandom.nextInt(16), populationRandom.nextInt(128),
			zo + populationRandom.nextInt(16));

	for (int i = 0; i < 20; ++i)
		placeOre(populationRandom, level, xo + populationRandom.nextInt(16),
			zo + populationRandom.nextInt(16), populationRandom.nextInt(128),
			Tile::dirt->id, 32);
	for (int i = 0; i < 10; ++i)
		placeOre(populationRandom, level, xo + populationRandom.nextInt(16),
			zo + populationRandom.nextInt(16), populationRandom.nextInt(128),
			Tile::gravel->id, 32);
	for (int i = 0; i < 20; ++i)
		placeOre(populationRandom, level, xo + populationRandom.nextInt(16),
			zo + populationRandom.nextInt(16), populationRandom.nextInt(128),
			Tile::coalOre->id, 16);
	for (int i = 0; i < 20; ++i)
		placeOre(populationRandom, level, xo + populationRandom.nextInt(16),
			zo + populationRandom.nextInt(16), populationRandom.nextInt(64),
			Tile::ironOre->id, 8);
	for (int i = 0; i < 2; ++i)
		placeOre(populationRandom, level, xo + populationRandom.nextInt(16),
			zo + populationRandom.nextInt(16), populationRandom.nextInt(32),
			Tile::goldOre->id, 8);
	for (int i = 0; i < 8; ++i)
		placeOre(populationRandom, level, xo + populationRandom.nextInt(16),
			zo + populationRandom.nextInt(16), populationRandom.nextInt(16),
			Tile::redStoneOre->id, 7);
	for (int i = 0; i < 1; ++i)
		// PE 0.6.1's emeraldOre slot is the existing diamond equivalent.
		placeOre(populationRandom, level, xo + populationRandom.nextInt(16),
			zo + populationRandom.nextInt(16), populationRandom.nextInt(16),
			Tile::emeraldOre->id, 7);

	const int treeNoise = (int)((forestNoise.getValue(xo * 0.5f, zo * 0.5f) / 8.0f
		+ populationRandom.nextFloat() * 4.0f + 4.0f) / 3.0f);
	int trees = treeNoise;
	if (populationRandom.nextInt(10) == 0) ++trees;

	// Alpha is a single green land type, so the Java forest-noise count is
	// used directly rather than applying Beta biome-specific modifiers.
	for (int i = 0; i < trees; ++i) {
		const int x = xo + populationRandom.nextInt(16) + 8;
		const int z = zo + populationRandom.nextInt(16) + 8;
		const int y = level->getHeightmap(x, z);
		TreeFeature tree(false);
		tree.init(1, 1, 1);
		if (y > 0 && y < Level::DEPTH)
			tree.place(level, &populationRandom, x, y, z);
	}

	for (int i = 0; i < 2; ++i)
		FlowerFeature(Tile::flower->id).place(level, &populationRandom,
			xo + populationRandom.nextInt(16) + 8, populationRandom.nextInt(128),
			zo + populationRandom.nextInt(16) + 8);
	if (populationRandom.nextInt(2) == 0)
		FlowerFeature(Tile::rose->id).place(level, &populationRandom,
			xo + populationRandom.nextInt(16) + 8, populationRandom.nextInt(128),
			zo + populationRandom.nextInt(16) + 8);
	if (populationRandom.nextInt(4) == 0)
		FlowerFeature(Tile::mushroom1->id).place(level, &populationRandom,
			xo + populationRandom.nextInt(16) + 8, populationRandom.nextInt(128),
			zo + populationRandom.nextInt(16) + 8);
	if (populationRandom.nextInt(8) == 0)
		FlowerFeature(Tile::mushroom2->id).place(level, &populationRandom,
			xo + populationRandom.nextInt(16) + 8, populationRandom.nextInt(128),
			zo + populationRandom.nextInt(16) + 8);

	for (int i = 0; i < 10; ++i)
		ReedsFeature().place(level, &populationRandom,
			xo + populationRandom.nextInt(16) + 8, populationRandom.nextInt(128),
			zo + populationRandom.nextInt(16) + 8);

	// Alpha tried one cactus per chunk even though it had no biome system.
	CactusFeature().place(level, &populationRandom,
		xo + populationRandom.nextInt(16) + 8, populationRandom.nextInt(128),
		zo + populationRandom.nextInt(16) + 8);

	for (int i = 0; i < 50; ++i)
		SpringFeature(Tile::water->id).place(level, &populationRandom,
			xo + populationRandom.nextInt(16) + 8,
			populationRandom.nextInt(populationRandom.nextInt(120) + 8),
			zo + populationRandom.nextInt(16) + 8);
	for (int i = 0; i < 20; ++i)
		SpringFeature(Tile::lava->id).place(level, &populationRandom,
			xo + populationRandom.nextInt(16) + 8,
			populationRandom.nextInt(populationRandom.nextInt(populationRandom.nextInt(112) + 8) + 8),
			zo + populationRandom.nextInt(16) + 8);

	if (biome != NULL && biome->isSnowCovered()) {
		placeWinterSurface(level, xo, zo);
		freezeExposedWater(level, xo, zo);
	}

	if (spawnMobs && !level->isClientSide && biome != NULL)
		MobSpawner::postProcessSpawnMobs(level, biome, xo + 8, zo + 8,
			16, 16, &populationRandom);
}

bool AlphaLevelSource::tick() {
	return false;
}

bool AlphaLevelSource::shouldSave() {
	return true;
}

std::string AlphaLevelSource::gatherStats() {
	return "AlphaLevelSource";
}

Biome::MobList AlphaLevelSource::getMobsAt(const MobCategory& category,
	int x, int y, int z)
{
	(void)y;
	Biome* biome = level->getBiomeSource()->getBiome(x, z);
	return biome ? biome->getMobs(category) : Biome::MobList();
}
