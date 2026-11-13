#ifndef NET_MINECRAFT_WORLD_LEVEL_TILE__StoneTile_H__
#define NET_MINECRAFT_WORLD_LEVEL_TILE__StoneTile_H__

//package net.minecraft.world.level.tile;

#include "../../../util/Random.h"
#include "../material/Material.h"

#include "Tile.h"

class StoneTile: public Tile
{
public:
    StoneTile(int id, int tex)
	:	Tile(id, tex, Material::stone)
	{
    }

    int getResource(int data, Random* random) {
        return Tile::stoneBrick->id;
    }
};

class CobblestoneTile: public Tile
{
public:
	CobblestoneTile(int id, int tex)
	: Tile(id, tex, Material::stone)
	{
	}

	int getTexture(int face) {
		if (Tile::useAlphaTextures())
			return Tile::ALPHA_COBBLESTONE_TEXTURE;
		return Tile::getTexture(face);
	}

	int getTexture(int face, int data) {
		return getTexture(face);
	}

	int getTexture(LevelSource* level, int x, int y, int z, int face) {
		if (Tile::useAlphaTextures())
			return Tile::ALPHA_COBBLESTONE_TEXTURE;
		return Tile::getTexture(level, x, y, z, face);
	}
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_TILE__StoneTile_H__*/
