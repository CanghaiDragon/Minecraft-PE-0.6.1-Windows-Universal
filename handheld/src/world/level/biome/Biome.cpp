#include "BiomeInclude.h"

#include "../levelgen/feature/TreeFeature.h"
#include "../levelgen/feature/TallgrassFeature.h"

#include "../../entity/EntityTypes.h"
#include "../../entity/MobCategory.h"
#include "../../level/tile/TallGrass.h"

Biome* Biome::rainForest	 = NULL;
Biome* Biome::swampland		 = NULL;
Biome* Biome::seasonalForest = NULL;
Biome* Biome::forest		 = NULL;
Biome* Biome::savanna		 = NULL;
Biome* Biome::shrubland		 = NULL;
Biome* Biome::taiga			 = NULL;
Biome* Biome::desert		 = NULL;
Biome* Biome::plains		 = NULL;
Biome* Biome::iceDesert		 = NULL;
Biome* Biome::tundra		 = NULL;
Biome* Biome::sky			 = NULL;

/*static*/
Biome::MobList Biome::_emptyMobList;
int Biome::defaultTotalEnemyWeight = 0;
int Biome::defaultTotalFriendlyWeight = 0;

/*static*/
Biome* Biome::map[64*64];

Biome::Biome()
:	topMaterial(((Tile*)Tile::grass)->id),
	material(((Tile*)Tile::dirt)->id),
	leafColor(0x4EE031)
{
    _friendlies.insert(_friendlies.end(), MobSpawnerData(MobTypes::Sheep, 12, 2, 3));
    _friendlies.insert(_friendlies.end(), MobSpawnerData(MobTypes::Pig, 10, 1, 3));
    _friendlies.insert(_friendlies.end(), MobSpawnerData(MobTypes::Chicken, 10, 2, 4));
    _friendlies.insert(_friendlies.end(), MobSpawnerData(MobTypes::Cow, 8, 2, 3));

    _enemies.insert(_enemies.end(), MobSpawnerData(MobTypes::Spider, 8, 2, 3));
    _enemies.insert(_enemies.end(), MobSpawnerData(MobTypes::Zombie, 12, 2, 4));
    _enemies.insert(_enemies.end(), MobSpawnerData(MobTypes::Skeleton, 6, 1, 3));
    _enemies.insert(_enemies.end(), MobSpawnerData(MobTypes::Creeper, 4, 1, 1));
    //_enemies.insert(_enemies.end(), MobSpawnerData(Slime.class, 10, 4, 4));
    //_enemies.insert(_enemies.end(), MobSpawnerData(EnderMan.class, 1, 1, 4));

    // wolves are added to forests and taigas
    // _friendlies.insert(_friendlies.end(), new MobSpawnerData(Wolf.class, 2));

    //_waterFriendlies.insert(_waterFriendlies.end(), (new MobSpawnerData(Squid.class, 10, 4, 4));

	//
	// Sum up the weights
	//
	defaultTotalEnemyWeight = 0;
	for (MobList::const_iterator cit = _enemies.begin(); cit != _enemies.end(); ++cit)
		defaultTotalEnemyWeight += cit->randomWeight;

	defaultTotalFriendlyWeight = 0;
	for (MobList::const_iterator cit = _friendlies.begin(); cit != _friendlies.end(); ++cit)
		defaultTotalFriendlyWeight += cit->randomWeight;
}

Biome* Biome::setName( const std::string& name )
{
	this->name = name;
	return this;
}

Biome* Biome::setLeafColor( int leafColor )
{
	this->leafColor = leafColor;
	return this;
}

Biome* Biome::setColor( int color )
{
	this->color = color;
	return this;
}

Biome* Biome::setSnowCovered()
{
	return this;
}

Biome* Biome::clearMobs( bool friendlies /*= true*/, bool waterFriendlies /*= true*/, bool enemies /*= true*/ )
{
	if (friendlies) _friendlies.clear();
	if (waterFriendlies) _waterFriendlies.clear();
	if (enemies) _enemies.clear();
	return this;
}


/*static*/
void Biome::recalc()
{
	for (int a = 0; a < 64; a++) {
		for (int b = 0; b < 64; b++) {
			map[a + b * 64] = _getBiome(a / 63.0f, b / 63.0f);
		}
	}

	Biome::desert->topMaterial = Biome::desert->material = (char) Tile::sand->id;
	Biome::iceDesert->topMaterial = Biome::iceDesert->material = (char) Tile::sand->id;
}

/*static*/
void Biome::initBiomes() {
	rainForest		= (new RainforestBiome())->setColor(0x08FA36)->setName("Rainforest")->setLeafColor(0x1FF458);
	swampland		= (new SwampBiome())->setColor(0x07F9B2)->setName("Swampland")->setLeafColor(0x8BAF48);
	seasonalForest	= (new Biome())->setColor(0x9BE023)->setName("Seasonal Forest");
	forest			= (new ForestBiome())->setColor(0x056621)->setName("Forest")->setLeafColor(0x4EBA31);
	savanna			= (new FlatBiome())->setColor(0xD9E023)->setName("Savanna");
	shrubland		= (new Biome())->setColor(0xA1AD20)->setName("Shrubland");
	taiga			= (new TaigaBiome())->setColor(0x2EB153)->setName("Taiga")->setSnowCovered()->setLeafColor(0x7BB731);
	desert			= (new FlatBiome())->setColor(0xFA9418)->clearMobs(true, true, false)->setName("Desert");
	plains			= (new FlatBiome())->setColor(0xFFD910)->setName("Plains");
	iceDesert		= (new FlatBiome())->setColor(0xFFED93)->clearMobs(true, false, false)->setName("Ice Desert")->setSnowCovered()->setLeafColor(0xC4D339);
	tundra			= (new Biome())->setColor(0x57EBF9)->setName("Tundra")->setSnowCovered()->setLeafColor(0xC4D339);
	// Keep PE's ordinary animals and hostile mobs, but bias Sky strongly
	// toward chickens. The inherited friendly weights total 40, including
	// 10 chicken and 30 non-chicken; adding 80 yields 90 / 120 = 75%.
	// FixedBiomeSource supplies the fixed Sky climate.
	sky				= (new Biome())->setColor(0x8080FF)->setName("Sky");
	sky->_friendlies.insert(sky->_friendlies.end(), MobSpawnerData(MobTypes::Chicken, 80, 2, 4));
	
	recalc();
}
/*static*/
void Biome::teardownBiomes() {
	delete rainForest;		rainForest= NULL;
	delete swampland;		swampland = NULL;
	delete seasonalForest;	seasonalForest = NULL;
	delete forest;			forest    = NULL;
	delete savanna;			savanna   = NULL;
	delete shrubland;		shrubland = NULL;
	delete taiga;			taiga     = NULL;
	delete desert;			desert    = NULL;
	delete plains;			plains    = NULL;
	delete iceDesert;		iceDesert = NULL;
	delete tundra;			tundra	  = NULL;
	delete sky;				sky		 = NULL;
}

Feature* Biome::getTreeFeature( Random* random )
{
	if (random->nextInt(10) == 0) {
		//return /*new*/ BasicTree();
	}
	return new TreeFeature(false);
}
Feature* Biome::getGrassFeature( Random* random ) {
	return new TallgrassFeature(Tile::tallgrass->id, TallGrass::TALL_GRASS);
}

Biome* Biome::getBiome( float temperature, float downfall )
{
	int a = (int) (temperature * 63);
	int b = (int) (downfall * 63);

	//printf("Getting biome: %s\n", map[a + b * 64]->name.c_str());

	return map[a + b * 64];
}

Biome* Biome::_getBiome( float temperature, float downfall )
{
	downfall *= (temperature);
	if (temperature < 0.10f) {
		return Biome::tundra;
	} else if (downfall < 0.20f) {
		if (temperature < 0.50f) {
			return Biome::tundra;
		} else if (temperature < 0.95f) {
			return Biome::savanna;
		} else {
			return Biome::desert;
		}
	} else if (downfall > 0.5f && temperature < 0.7f) {
		return Biome::swampland;
	} else if (temperature < 0.50f) {
		return Biome::taiga;
	} else if (temperature < 0.97f) {
		if (downfall < 0.35f) {
			return Biome::shrubland;
		} else {
			return Biome::forest;
		}
	} else {
		if (downfall < 0.45f) {
			return Biome::plains;
		} else if (downfall < 0.90f) {
			return Biome::seasonalForest;
		} else {
			return Biome::rainForest;
		}
	}
}

float Biome::adjustScale( float scale )
{
	return scale;
}

float Biome::adjustDepth( float depth )
{
	return depth;
}

int Biome::getSkyColor( float temp )
{
	// Java Beta's BiomeGenBase.getSkyColorByTemp.  The ordinary PE path does
	// not call this method, so its original sky palette remains untouched.
	temp /= 3.0f;
	if (temp < -1.0f) temp = -1.0f;
	if (temp > 1.0f) temp = 1.0f;
	float hue = 0.6222222f - temp * 0.05f;
	float saturation = 0.50f + temp * 0.10f;
	float h = hue * 6.0f;
	int sector = (int)h;
	float f = h - sector, p = 1.0f - saturation;
	float q = 1.0f - saturation * f, t = 1.0f - saturation * (1.0f - f);
	float r = 1.0f, g = 1.0f, b = 1.0f;
	switch (sector % 6) {
	case 0: r = 1; g = t; b = p; break; case 1: r = q; g = 1; b = p; break;
	case 2: r = p; g = 1; b = t; break; case 3: r = p; g = q; b = 1; break;
	case 4: r = t; g = p; b = 1; break; case 5: r = 1; g = p; b = q; break;
	}
	return ((int)(r * 255.0f) << 16) | ((int)(g * 255.0f) << 8) | (int)(b * 255.0f);
}

Biome::MobList& Biome::getMobs(const MobCategory& category)
{
    if (&category == &MobCategory::monster)
		return _enemies;
    if (&category == &MobCategory::creature)
		return _friendlies;
    if (&category == &MobCategory::waterCreature)
		return _waterFriendlies;

	LOGE("Unknown MobCategory!");
	return _emptyMobList;
}

float Biome::getCreatureProbability() {
    return 0.08f;
}
