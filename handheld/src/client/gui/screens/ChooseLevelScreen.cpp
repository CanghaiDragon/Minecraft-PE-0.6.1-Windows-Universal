#include "ChooseLevelScreen.h"
#include <algorithm>
#include <set>
#include "../../Minecraft.h"

void ChooseLevelScreen::init() {
	LOGI("ARM diag: ChooseLevelScreen::init begin\n");
	loadLevelSource();
	LOGI("ARM diag: ChooseLevelScreen::init end levels=%u\n", (unsigned)levels.size());
}

void ChooseLevelScreen::loadLevelSource()
{
	LOGI("ARM diag: loadLevelSource begin\n");
	LevelStorageSource* levelSource = minecraft->getLevelSource();
	LOGI("ARM diag: levelSource=%p\n", (void*)levelSource);
	if (levelSource == NULL) {
		LOGI("ARM diag: levelSource is null\n");
		return;
	}
	levelSource->getLevelList(levels);
	LOGI("ARM diag: getLevelList returned levels=%u\n", (unsigned)levels.size());
	std::sort(levels.begin(), levels.end());
	LOGI("ARM diag: loadLevelSource end\n");
}

std::string ChooseLevelScreen::getUniqueLevelName( const std::string& level ) {
	std::set<std::string> Set;
	for (unsigned int i = 0; i < levels.size(); ++i)
		Set.insert(levels[i].id);

	std::string s = level;
	while ( Set.find(s) != Set.end() )
		s += "-";
	return s;
}
