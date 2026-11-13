#ifndef NET_MINECRAFT_WORLD_LEVEL_TILE__IceTile_H__
#define NET_MINECRAFT_WORLD_LEVEL_TILE__IceTile_H__

//package net.minecraft.world.level.tile;

#include "Tile.h"
#include "../LevelSource.h"
#include "../material/Material.h"
#include "../LightLayer.h"
#include "../../../util/Random.h"

class IceTile: public Tile
{
	typedef Tile super;
public:
    IceTile(int id, int tex)
	:	super(id, tex, Material::ice)
	{
        friction = 0.98f;
        setTicking(true);
    }

    // Java Beta's translucent ice path is tied to Fancy graphics. Keep the
    // original opaque PE path for PE visuals and Beta+Fast.
    int getRenderLayer() {
        return Tile::useBetaFancyGraphics()
            ? Tile::RENDERLAYER_BLEND
            : Tile::RENDERLAYER_OPAQUE;
    }

    bool isSolidRender() {
        return !Tile::useBetaFancyGraphics();
    }

    bool shouldRenderFace(LevelSource* level, int x, int y, int z, int face) {
        // Match BlockBreakable(false): adjacent ice cubes share one surface.
        if (Tile::useBetaFancyGraphics() && level->getTile(x, y, z) == this->id)
            return false;
        return Tile::shouldRenderFace(level, x, y, z, face);
    }

    void playerDestroy(Level* level, Player* player, int x, int y, int z, int data) {
        const Material* below = level->getMaterial(x, y - 1, z);
        if (below->blocksMotion() || below->isLiquid()) {
            level->setTile(x, y, z, Tile::water->id);
        }
    }

    int getResourceCount(Random* random) {
        return 0;
    }

    void tick(Level* level, int x, int y, int z, Random* random) {
        if (level->getBrightness(LightLayer::Block, x, y, z) > 11 - Tile::lightBlock[id]) {
            this->spawnResources(level, x, y, z, level->getData(x, y, z));
            level->setTile(x, y, z, Tile::calmWater->id);
        }
    }
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_TILE__IceTile_H__*/
