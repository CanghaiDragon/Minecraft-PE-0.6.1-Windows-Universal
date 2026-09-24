#include "SkyLevelSource.h"
#include "../Level.h"
#include "../MobSpawner.h"
#include "../biome/BiomeSource.h"
#include "../chunk/LevelChunk.h"
#include "../tile/Tile.h"
#include "../tile/HeavyTile.h"
#include "../material/Material.h"
#include "feature/FlowerFeature.h"
#include "feature/ReedsFeature.h"
#include "feature/CactusFeature.h"
#include "feature/SpringFeature.h"
#include "feature/ClayFeature.h"
#include "feature/OreFeature.h"
#include <cstdint>

namespace {

// OreFeature offsets its input height upward by up to four blocks. Keep its
// starting coordinate below the top of the 128-block PE world.
static int getOreStartY(Random& random, int lowestRockY, int vanillaRange) {
    const int maximumStartY = Level::DEPTH - 5;
    int baseY = lowestRockY >= vanillaRange ? lowestRockY : 0;
    if (baseY > maximumStartY) return -1;

    int range = vanillaRange;
    if (baseY + range - 1 > maximumStartY) range = maximumStartY - baseY + 1;
    return baseY + random.nextInt(range);
}

static int getTriangularOreStartY(Random& random, int lowestRockY) {
    const int maximumStartY = Level::DEPTH - 5;
    int baseY = lowestRockY >= 32 ? lowestRockY : 0;
    if (baseY > maximumStartY) return -1;
    // Java's lapis band is nextInt(16) + nextInt(16), not a uniform 0..31
    // band. Preserve that distribution and translate the whole band only when
    // the island starts above it.
    int offset = random.nextInt(16) + random.nextInt(16);
    if (baseY + offset > maximumStartY) offset = maximumStartY - baseY;
    return baseY + offset;
}

// OreFeature works over the population chunk plus its positive neighbours.
// Find the lowest stone in that same 32 x 32 area without creating any extra
// chunks. An empty area retains Java's original low-Y sampling, which simply
// produces no vein because OreFeature replaces stone only.
static int getLowestRockY(Level* level, int xt, int zt) {
    int lowest = Level::DEPTH;
    for (int chunkX = 0; chunkX < 2; ++chunkX)
        for (int chunkZ = 0; chunkZ < 2; ++chunkZ) {
            LevelChunk* chunk = level->getChunk(xt + chunkX, zt + chunkZ);
            unsigned char* blocks = chunk->getBlockData();
            for (int x = 0; x < 16; ++x)
                for (int z = 0; z < 16; ++z)
                    for (int y = 0; y < lowest; ++y)
                        if (blocks[(x << 11) | (z << 7) | y] == Tile::rock->id) {
                            lowest = y;
                            break;
                        }
        }
    return lowest;
}

static void placeOre(Random& random, Level* level, int x, int z, int y, int tile, int size) {
    if (y < 0) return;
    OreFeature feature(tile, size);
    feature.place(level, &random, x, y, z);
}

}

SkyLevelSource::SkyLevelSource(Level* level, long seed, bool spawnMobs)
:   level(level), spawnMobs(spawnMobs), random(seed),
    lperlinNoise1(&random, 16), lperlinNoise2(&random, 16),
    perlinNoise1(&random, 8), surfaceNoise(&random, 4), forestNoise(&random, 8)
{
}

SkyLevelSource::~SkyLevelSource() {
    // ChunkCache owns block data and chunks; all noise buffers are inline.
}

void SkyLevelSource::getHeights(int x, int z) {
    const float s = 684.412f * 2;
    const float hs = 684.412f;
    perlinNoise1.getRegion(pnr, (float)x, 0, (float)z, X_SIZE, Y_SIZE, X_SIZE,
        s / 80, hs / 160, s / 80);
    lperlinNoise1.getRegion(ar, (float)x, 0, (float)z, X_SIZE, Y_SIZE, X_SIZE, s, hs, s);
    lperlinNoise2.getRegion(br, (float)x, 0, (float)z, X_SIZE, Y_SIZE, X_SIZE, s, hs, s);

    int p = 0;
    for (int xx = 0; xx < X_SIZE; ++xx)
        for (int zz = 0; zz < X_SIZE; ++zz)
            for (int yy = 0; yy < Y_SIZE; ++yy, ++p) {
                float a = ar[p] / 512;
                float b = br[p] / 512;
                float mix = (pnr[p] / 10 + 1) / 2;
                float val;
                if (mix < 0) val = a;
                else if (mix > 1) val = b;
                else val = a + (b - a) * mix;

                // Sky has no Overworld height gradient or climate modulation.
                val -= 8;
                if (yy > Y_SIZE - 32) {
                    float slide = (yy - (Y_SIZE - 32)) / 31.0f;
                    val = val * (1 - slide) - 30 * slide;
                }
                if (yy < 8) {
                    // Intentionally 8/7 at yy=0, as in Beta Sky; do not clamp.
                    float slide = (8 - yy) / 7.0f;
                    val = val * (1 - slide) - 30 * slide;
                }
                buffer[p] = val;
            }
}

void SkyLevelSource::prepareHeights(int xOffs, int zOffs, unsigned char* blocks) {
    const int cells = 16 / CELL_WIDTH;
    getHeights(xOffs * cells, zOffs * cells);
    for (int xc = 0; xc < cells; ++xc)
        for (int zc = 0; zc < cells; ++zc)
            for (int yc = 0; yc < 128 / CELL_HEIGHT; ++yc) {
                int p = (xc * X_SIZE + zc) * Y_SIZE + yc;
                float s0 = buffer[p];
                float s1 = buffer[p + Y_SIZE];
                float s2 = buffer[p + X_SIZE * Y_SIZE];
                float s3 = buffer[p + (X_SIZE + 1) * Y_SIZE];
                float s0a = (buffer[p + 1] - s0) / CELL_HEIGHT;
                float s1a = (buffer[p + Y_SIZE + 1] - s1) / CELL_HEIGHT;
                float s2a = (buffer[p + X_SIZE * Y_SIZE + 1] - s2) / CELL_HEIGHT;
                float s3a = (buffer[p + (X_SIZE + 1) * Y_SIZE + 1] - s3) / CELL_HEIGHT;
                for (int y = 0; y < CELL_HEIGHT; ++y) {
                    float left = s0;
                    float right = s1;
                    float leftStep = (s2 - s0) / CELL_WIDTH;
                    float rightStep = (s3 - s1) / CELL_WIDTH;
                    for (int x = 0; x < CELL_WIDTH; ++x) {
                        int offs = ((xc * CELL_WIDTH + x) << 11)
                            | ((zc * CELL_WIDTH) << 7) | (yc * CELL_HEIGHT + y);
                        float val = left;
                        float step = (right - left) / CELL_WIDTH;
                        for (int z = 0; z < CELL_WIDTH; ++z) {
                            blocks[offs] = val > 0 ? (unsigned char)Tile::rock->id : 0;
                            offs += 128;
                            val += step;
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

void SkyLevelSource::buildSurfaces(int xOffs, int zOffs, unsigned char* blocks, Biome** biomes) {
    const float s = 1 / 16.0f;
    surfaceNoise.getRegion(depthBuffer, (float)(xOffs * 16), (float)(zOffs * 16), 0,
        16, 16, 1, s, s, s);
    // Keep PE's surface traversal and biome/noise indexing conventions.
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) {
            Biome* biome = biomes[x + z * 16];
            int runDepth = (int)(depthBuffer[x + z * 16] / 3 + 3 + random.nextFloat() * 0.25f);
            int run = -1;
            int top = biome->topMaterial;
            int material = biome->material;
            for (int y = 127; y >= 0; --y) {
                int offs = (z * 16 + x) * 128 + y;
                int old = blocks[offs];
                if (old == 0) run = -1;
                else if (old == Tile::rock->id) {
                    if (run == -1) {
                        if (runDepth <= 0) {
                            top = 0;
                            material = Tile::rock->id;
                        }
                        run = runDepth;
                        blocks[offs] = (unsigned char)top;
                    } else if (run > 0) {
                        --run;
                        blocks[offs] = (unsigned char)material;
                        if (run == 0 && material == Tile::sand->id) {
                            run = random.nextInt(4);
                            material = Tile::sandStone->id;
                        }
                    }
                }
            }
        }
}

LevelChunk* SkyLevelSource::getChunk(int x, int z) {
    std::pair<int, int> key(x, z);
    std::map<std::pair<int, int>, LevelChunk*>::iterator it = chunkMap.find(key);
    if (it != chunkMap.end()) return it->second;

    // PE's chunk seed constants, with explicit 32-bit wraparound.
    uint32_t seed = (uint32_t)x * 341872712u + (uint32_t)z * 132899541u;
    random.setSeed((long)(int32_t)seed);
    unsigned char* blocks = new unsigned char[LevelChunk::ChunkBlockCount];
    LevelChunk* chunk = new LevelChunk(level, blocks, x, z);
    chunkMap.insert(std::make_pair(key, chunk));
    Biome** biomes = level->getBiomeSource()->getBiomeBlock(x * 16, z * 16, 16, 16);
    prepareHeights(x, z, blocks);
    buildSurfaces(x, z, blocks, biomes);
    chunk->recalcHeightmap();
    return chunk;
}

LevelChunk* SkyLevelSource::create(int x, int z) { return getChunk(x, z); }
bool SkyLevelSource::hasChunk(int x, int z) { return true; }
void SkyLevelSource::postProcess(ChunkSource* parent, int xt, int zt) {
    // Features can request neighbouring chunks. Keep the population RNG local
    // so getChunk() cannot reseed it during those requests.
    Random random(level->getSeed());
    uint32_t xScale = (uint32_t)(random.nextInt() / 2 * 2 + 1);
    uint32_t zScale = (uint32_t)(random.nextInt() / 2 * 2 + 1);
    uint32_t seed = ((uint32_t)xt * xScale + (uint32_t)zt * zScale) ^ (uint32_t)level->getSeed();
    random.setSeed((long)(int32_t)seed);

    // Preserve the caller's flags, including nested population during feature placement.
    struct GenerationState {
        Level* level;
        bool generating, falling, ticking;
        GenerationState(Level* level): level(level), generating(level->isGeneratingTerrain),
            falling(HeavyTile::instaFall), ticking(level->instaTick) {
            level->isGeneratingTerrain = true;
            HeavyTile::instaFall = true;
        }
        ~GenerationState() {
            level->isGeneratingTerrain = generating;
            HeavyTile::instaFall = falling;
            level->instaTick = ticking;
        }
    } state(level);

    int xo = xt * 16;
    int zo = zt * 16;
    Biome* biome = level->getBiomeSource()->getBiome(xo + 16, zo + 16);
    // Sample the unmodified terrain before dirt and gravel veins replace any
    // of its lowest stone blocks.
    int lowestRockY = getLowestRockY(level, xt, zt);

    // Java Beta Sky uses the same vein counts and sizes as its Overworld.
    // Lakes remain deliberately absent, so the Java clay pass is retained but
    // only succeeds next to water supplied by already populated terrain.
    for (int i = 0; i < 10; ++i) {
        ClayFeature feature(32);
        feature.place(level, &random, xo + random.nextInt(16), random.nextInt(128), zo + random.nextInt(16));
    }
    for (int i = 0; i < 20; ++i)
        placeOre(random, level, xo + random.nextInt(16), zo + random.nextInt(16), random.nextInt(128), Tile::dirt->id, 32);
    for (int i = 0; i < 10; ++i)
        placeOre(random, level, xo + random.nextInt(16), zo + random.nextInt(16), random.nextInt(128), Tile::gravel->id, 32);

    for (int i = 0; i < 20; ++i)
        placeOre(random, level, xo + random.nextInt(16), zo + random.nextInt(16), random.nextInt(128), Tile::coalOre->id, 16);
    for (int i = 0; i < 20; ++i)
        placeOre(random, level, xo + random.nextInt(16), zo + random.nextInt(16), getOreStartY(random, lowestRockY, 64), Tile::ironOre->id, 8);
    for (int i = 0; i < 2; ++i)
        placeOre(random, level, xo + random.nextInt(16), zo + random.nextInt(16), getOreStartY(random, lowestRockY, 32), Tile::goldOre->id, 8);
    for (int i = 0; i < 8; ++i)
        placeOre(random, level, xo + random.nextInt(16), zo + random.nextInt(16), getOreStartY(random, lowestRockY, 16), Tile::redStoneOre->id, 7);
    // PE 0.6.1 stores the diamond ore block as emeraldOre (description id:
    // oreDiamond); this is the established PE equivalent of Java diamond.
    placeOre(random, level, xo + random.nextInt(16), zo + random.nextInt(16), getOreStartY(random, lowestRockY, 16), Tile::emeraldOre->id, 7);
    int lapisY = getTriangularOreStartY(random, lowestRockY);
    placeOre(random, level, xo + random.nextInt(16), zo + random.nextInt(16), lapisY, Tile::lapisOre->id, 6);

    int forest = (int)((forestNoise.getValue(xo * 0.5f, zo * 0.5f) / 8
        + random.nextFloat() * 4 + 4) / 3);
    int trees = random.nextInt(10) == 0 ? 1 : 0;
    if (biome == Biome::forest || biome == Biome::rainForest || biome == Biome::taiga)
        trees += forest + 5;
    if (biome == Biome::seasonalForest) trees += forest + 2;
    if (biome == Biome::desert || biome == Biome::tundra || biome == Biome::plains)
        trees -= 20;
    for (int i = 0; i < trees; ++i) {
        int x = xo + random.nextInt(16) + 8;
        int z = zo + random.nextInt(16) + 8;
        int y = level->getHeightmap(x, z);
        Feature* tree = biome->getTreeFeature(&random);
        if (tree) {
            if (y > 0 && y < Level::DEPTH) {
                tree->init(1, 1, 1);
                tree->place(level, &random, x, y, z);
            }
            delete tree;
        }
    }

    // Beta Sky uses random-height flower patches, not a surface-only scatter.
    // PE's features check the support and light before placing any plant.
    int plants[] = { Tile::flower->id, Tile::rose->id, Tile::mushroom1->id, Tile::mushroom2->id };
    for (int kind = 0; kind < 4; ++kind) {
        int count = kind == 0 ? 2 : (random.nextInt(1 << kind) == 0 ? 1 : 0);
        for (int i = 0; i < count; ++i) {
            int x = xo + random.nextInt(16) + 8;
            int y = random.nextInt(128);
            int z = zo + random.nextInt(16) + 8;
            FlowerFeature feature(plants[kind]);
            feature.place(level, &random, x, y, z);
        }
    }
    for (int i = 0; i < 10; ++i) {
        int x = xo + random.nextInt(16) + 8;
        int y = random.nextInt(128);
        int z = zo + random.nextInt(16) + 8;
        ReedsFeature feature;
        feature.place(level, &random, x, y, z);
    }
    if (biome == Biome::desert)
        for (int i = 0; i < 10; ++i) {
            int x = xo + random.nextInt(16) + 8;
            int y = random.nextInt(128);
            int z = zo + random.nextInt(16) + 8;
            CactusFeature feature;
            feature.place(level, &random, x, y, z);
        }

    // The same stone-enclosed, one-open-side springs used by PE Overworld.
    // SpringFeature starts a liquid tick, allowing waterfalls down island sides.
    for (int i = 0; i < 50; ++i) {
        int x = xo + random.nextInt(16) + 8;
        int y = random.nextInt(random.nextInt(120) + 8);
        int z = zo + random.nextInt(16) + 8;
        SpringFeature feature(Tile::water->id);
        feature.place(level, &random, x, y, z);
    }
    for (int i = 0; i < 20; ++i) {
        int x = xo + random.nextInt(16) + 8;
        int y = random.nextInt(random.nextInt(random.nextInt(112) + 8) + 8);
        int z = zo + random.nextInt(16) + 8;
        SpringFeature feature(Tile::lava->id);
        feature.place(level, &random, x, y, z);
    }

    // Match Infinite's population-time friendly spawning. The fixed Sky biome
    // provides the Sky-specific animal weighting.
    if (spawnMobs && !level->isClientSide)
        MobSpawner::postProcessSpawnMobs(level, biome, xo + 8, zo + 8, 16, 16, &random);

    // BiomeSource owns a scratch buffer: copy it before any further world access.
    float temperatures[16 * 16];
    float* source = level->getBiomeSource()->getTemperatureBlock(xo + 8, zo + 8, 16, 16);
    for (int i = 0; i < 16 * 16; ++i) temperatures[i] = source[i];
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) {
            int wx = xo + x + 8;
            int wz = zo + z + 8;
            int y = level->getTopSolidBlock(wx, wz);
            if (y <= 0 || y >= Level::DEPTH) continue;
            float temperature = temperatures[x * 16 + z] - (y - 64) / 64.0f * 0.3f;
            const Material* below = level->getMaterial(wx, y - 1, wz);
            if (temperature < 0.5f && level->isEmptyTile(wx, y, wz)
                && below->blocksMotion() && below != Material::ice)
                level->setTile(wx, y, wz, Tile::topSnow->id);
        }
}
bool SkyLevelSource::tick() { return false; }
bool SkyLevelSource::shouldSave() { return true; }
std::string SkyLevelSource::gatherStats() { return "SkyLevelSource"; }

Biome::MobList SkyLevelSource::getMobsAt(const MobCategory& category, int x, int y, int z) {
    BiomeSource* source = level->getBiomeSource();
    Biome* biome = source ? source->getBiome(x, z) : NULL;
    return biome ? biome->getMobs(category) : Biome::MobList();
}
