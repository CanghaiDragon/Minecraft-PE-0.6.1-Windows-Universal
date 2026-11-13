#ifndef NET_MINECRAFT_WORLD_LEVEL_TILE__CarriedTile_H__
#define NET_MINECRAFT_WORLD_LEVEL_TILE__CarriedTile_H__

//package net.minecraft.world.level.tile;

#include "../../../util/Random.h"
#include "../material/Material.h"

#include "Tile.h"

class CarriedTile: public Tile
{
    typedef Tile super;
public:
    CarriedTile(int id, int texDefault, int texTop = -1)
	:	super(id, tex, Material::dirt),
		texDefault(texDefault),
		texTop(texTop >= 0? texTop : texDefault)
	{
    }

    int getTexture(int face, int data) {
        if (Tile::useAlphaTextures() && this == Tile::grass_carried) {
            if (face == 1) return Tile::ALPHA_GRASS_TOP_TEXTURE;
            if (face == 0) return Tile::ALPHA_GRASS_BOTTOM_TEXTURE;
            return Tile::ALPHA_GRASS_SIDE_TEXTURE;
        }
        if (face == 1) return texTop;
        return texDefault;
    }

    int getResource(int data, Random* random) {
        return 0;
    }

    int getResourceCount(Random* random) {
        return 0;
    }

private:
    int texDefault;
    int texTop;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_TILE__CarriedTile_H__*/
