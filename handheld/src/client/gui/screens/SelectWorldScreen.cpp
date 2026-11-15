#include "SelectWorldScreen.h"
#include "StartMenuScreen.h"
#include "ProgressScreen.h"
#include "DialogDefinitions.h"
#include "../Font.h"
#include "../../renderer/Tesselator.h"
#include "../../../AppPlatform.h"
#include "../../../util/StringUtils.h"
#include "../../../util/Mth.h"
#include "../../../platform/input/Mouse.h"
#include "../../../Performance.h"
#include "../../../world/level/LevelSettings.h"

#include <algorithm>
#include <set>
#include "../../renderer/Textures.h"
#include "SimpleChooseLevelScreen.h"
#include "../../../platform/input/Keyboard.h"

static float Max(float a, float b) {
	return a>b? a : b;
}

namespace {
const char* kUwpRepositoryUrl =
	"https://github.com/CanghaiDragon/Minecraft-PE-0.6.1-UWP";
const char* kUwpRepositoryLabel = "UWP version: Visit GitHub Repository";
const char* kPe09ConversionNotice =
	"Conversion to original PE 0.9+ is not available.";
const char* kUseUwpConversionNotice =
	"Please use the UWP version for conversion.";
}

//
// World Selection List
//
WorldSelectionList::WorldSelectionList( Minecraft* minecraft, int width, int height )
:	_height(height),
	hasPickedLevel(false),
	currentTick(0),
	stoppedTick(-1),
	mode(0),
	RolledSelectionListH(minecraft, width, height, 0, width, 26, height-32, 120)
{
}

int WorldSelectionList::getNumberOfItems() {
	return (int)levels.size();
}

void WorldSelectionList::selectItem( int item, bool doubleClick ) {
	//LOGI("sel: %d, item %d\n", selectedItem, item);
	if (selectedItem < 0 || (selectedItem != item))
		return;
	
	if (!hasPickedLevel) {
		hasPickedLevel = true;
		pickedLevel = levels[item];
	}
}

bool WorldSelectionList::isSelectedItem( int item ) {
	return item == selectedItem;
}

void WorldSelectionList::renderItem( int i, int x, int y, int h, Tesselator& t ) {

	int centerx = x + itemWidth/2;

	float a0 = Max(1.1f - std::abs( width / 2 - centerx ) * 0.0055f, 0.2f);
	if (a0 > 1) a0 = 1;
	int textColor =  (int)(255.0f * a0) * 0x010101;
	int textColor2 = (int)(140.0f * a0) * 0x010101;

	const int TY = y + 42;
	const int TX = centerx - itemWidth / 2 + 5;

	StringVector v = _descriptions[i];
	drawString(minecraft->font, v[0].c_str(), TX, TY +  0, textColor);
	drawString(minecraft->font, v[1].c_str(), TX, TY + 10, textColor2);
	drawString(minecraft->font, v[2].c_str(), TX, TY + 20, textColor2);
	drawString(minecraft->font, v[3].c_str(), TX, TY + 30, textColor2);

	minecraft->textures->loadAndBindTexture(_imageNames[i]);
	t.color(0.3f, 1.0f, 0.2f);

	//float x0 = (float)x;
	//float x1 = (float)x + (float)itemWidth;

	const float IY = (float)y - 8;
	t.begin();
		t.color(textColor);
		t.vertexUV((float)(centerx-32), IY,      blitOffset, 0, 0);
		t.vertexUV((float)(centerx-32), IY + 48, blitOffset, 0, 1);
		t.vertexUV((float)(centerx+32), IY + 48, blitOffset, 1, 1);
		t.vertexUV((float)(centerx+32), IY,      blitOffset, 1, 0);
	t.draw();
}

void WorldSelectionList::stepLeft() {
	if (selectedItem > 0) {
		td.start = xo;
		td.stop = xo - itemWidth;
		td.cur = 0;
		td.dur = 8;
		mode = 1;
		tweenInited();
	}
}

void WorldSelectionList::stepRight() {
	if (selectedItem >= 0 && selectedItem < getNumberOfItems()-1) {
		td.start = xo;
		td.stop = xo + itemWidth;
		td.cur = 0;
		td.dur = 8;
		mode = 1;
		tweenInited();
	}
}

void WorldSelectionList::commit() {
	for (unsigned int i = 0; i < levels.size(); ++i) {
		LevelSummary& level = levels[i];

		std::stringstream ss;
		ss << level.name << "/preview.png";
		TextureId id = Textures::InvalidId;//minecraft->textures->loadTexture(ss.str(), false);

		if (id != Textures::InvalidId) {
			_imageNames.push_back( ss.str() );
		} else {
			_imageNames.push_back("gui/default_world.png");
		}

		StringVector lines;
		lines.push_back(level.name);
		lines.push_back(minecraft->platform()->getDateString(level.lastPlayed));
		lines.push_back(level.id);
		lines.push_back(LevelSettings::gameTypeToString(level.gameType));
		_descriptions.push_back(lines);

		selectedItem = 0;
	}
}

static float quadraticInOut(float t, float dur, float start, float stop) {
	const float delta = stop - start;
	const float T = (t / dur) * 2.0f;
	if (T < 1) return 0.5f*delta*T*T + start;
	return -0.5f*delta * ((T-1)*(T-3) - 1) + start;
}

void WorldSelectionList::tick()
{
	RolledSelectionListH::tick();

	++currentTick;

	if (Mouse::isButtonDown(MouseAction::ACTION_LEFT) || dragState == 0)
		return;

	// Handle the tween (when in "mode 1")
	selectedItem = -1; 
	if (mode == 1) {
		if (++td.cur == td.dur) {
			mode = 0;
			xInertia = 0;
			xoo = xo = td.stop;
			selectedItem = getItemAtPosition(width/2, height/2);
		} else {
			tweenInited();
		}
		return;
	}

	// It's still going fast, let it run
	float speed = Mth::abs(xInertia);
	bool slowEnoughToBeBothered = speed < 5.0f;
	if (!slowEnoughToBeBothered) {
		xInertia = xInertia * .9f;
		return;
	}

	xInertia *= 0.8f;

	if (speed < 1 && dragState < 0) {
		const int offsetx = (width-itemWidth) / 2;
		const float pxo = xo + offsetx;
		int index = getItemAtXPositionRaw((int)(pxo - 10*xInertia));
		int indexPos = index*itemWidth;

		// Pick closest
		float diff = (float)indexPos - pxo;
		if (diff < -itemWidth/2) {
			diff += itemWidth;
			index++;
			//indexPos += itemWidth;
		}
		if (Mth::abs(diff) < 1 && speed < 0.1f) {
			selectedItem = getItemAtPosition(width/2, height/2);
			return;
		}

		td.start = xo;
		td.stop = xo + diff;
		td.cur = 0;
		td.dur = (float) Mth::Min(7, 1 + (int)(Mth::abs(diff) * 0.25f));
		mode = 1;
		//LOGI("inited-t %d\n", dragState);
		tweenInited();
	}
}

float WorldSelectionList::getPos( float alpha )
{
	if (mode != 1) return RolledSelectionListH::getPos(alpha);

	float x0 = quadraticInOut(td.cur, td.dur, td.start, td.stop);
	float x1 = quadraticInOut(td.cur+1, td.dur, td.start, td.stop);
	return x0 + (x1-x0)*alpha;
}

bool WorldSelectionList::capXPosition() {
	bool capped = RolledSelectionListH::capXPosition();
	if (capped) mode = 0;
	return capped;
}

void WorldSelectionList::tweenInited() {
	float x0 = quadraticInOut(td.cur,   td.dur, td.start, td.stop);
	float x1 = quadraticInOut(td.cur+1, td.dur, td.start, td.stop);
	xInertia = x0-x1; // yes, it's all backwards and messed up..
}

//
// Select World Screen
//
SelectWorldScreen::SelectWorldScreen()
:	bEdit   (1, "Edit"),
	bCreate (2, "Create new"),
	bBack   (3, "Back"),
	bWorldView(4, ""),
	worldsList(NULL),
	_state(_STATE_DEFAULT),
	_hasStartedLevel(false)
{
	bEdit.active = false;
}

SelectWorldScreen::~SelectWorldScreen()
{
	delete worldsList;
}

void SelectWorldScreen::buttonClicked(Button* button)
{
	if (button->id == bCreate.id) {
		//minecraft->setScreen( new CreateWorldScreen() );
		//minecraft->locateMultiplayer();
		//minecraft->setScreen(new JoinGameScreen());

		//minecraft->hostMultiplayer();
		//minecraft->setScreen(new ProgressScreen());

		if (_state == _STATE_DEFAULT && !_hasStartedLevel) {
			minecraft->platform()->createUserInput(DialogDefinitions::DIALOG_CREATE_NEW_WORLD);
			_state = _STATE_CREATEWORLD;
		}
	}
	if (button->id == bEdit.id) {
		if (isIndexValid(worldsList->selectedItem)) {
			LevelSummary level = worldsList->levels[worldsList->selectedItem];
			LOGI("level: %s, %s\n", level.id.c_str(), level.name.c_str());
			minecraft->setScreen( new EditWorldScreen(level) );
		}
	}
	if (button->id == bBack.id) {
		minecraft->cancelLocateMultiplayer();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
	if (button->id == bWorldView.id) {
		// Try to "click" the item in the middle
		worldsList->selectItem( worldsList->getItemAtPosition(width/2, height/2), false );
	}
}

bool SelectWorldScreen::handleBackEvent(bool isDown)
{
	if (!isDown)
	{
		minecraft->cancelLocateMultiplayer();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
	return true;
}

bool SelectWorldScreen::isIndexValid( int index )
{
	return worldsList && index >= 0 && index < worldsList->getNumberOfItems();
}

static char ILLEGAL_FILE_CHARACTERS[] = {
	'/', '\n', '\r', '\t', '\0', '\f', '`', '?', '*', '\\', '<', '>', '|', '\"', ':'
};

void SelectWorldScreen::tick()
{
	if (_state == _STATE_CREATEWORLD) {
		#if defined(RPI)
			std::string levelId = getUniqueLevelName("world");
			LevelSettings settings(getEpochTimeS(), GameType::Creative);
			minecraft->selectLevel(levelId, levelId, settings);
			if (minecraft->legacyStorageRejected())
				return;
			minecraft->hostMultiplayer();
			minecraft->setScreen(new ProgressScreen());
			_hasStartedLevel = true;
		#elif defined(WIN32) || defined(MACOS) || defined(LINUX)
			std::string name = getUniqueLevelName("world");
			minecraft->setScreen(new SimpleChooseLevelScreen(name));
		#else
			int status = minecraft->platform()->getUserInputStatus();
			if (status > -1) {
				if (status == 1) {
					StringVector sv = minecraft->platform()->getUserInput();

					// Read the level name.
					// 1) Trim name 2) Remove all bad chars 3) Append '-' chars 'til the name is unique
					std::string levelName = Util::stringTrim(sv[0]);
					std::string levelId = levelName;

					for (int i = 0; i < sizeof(ILLEGAL_FILE_CHARACTERS) / sizeof(char); ++i)
						levelId = Util::stringReplace(levelId, std::string(1, ILLEGAL_FILE_CHARACTERS[i]), "");
                    if ((int)levelId.length() == 0) {
                        levelId = "no_name";
                    }
					levelId = getUniqueLevelName(levelId);

					// Read the seed
					int seed = getEpochTimeS();
					if (sv.size() >= 2) {
						std::string seedString = Util::stringTrim(sv[1]);
						if (seedString.length() > 0) {
							int tmpSeed;
							// Try to read it as an integer
							if (sscanf(seedString.c_str(), "%d", &tmpSeed) > 0) {
								seed = tmpSeed;
							} // Hash the "seed"
							else {
								seed = Util::hashCode(seedString);
							}
						}
					}
					// Read the game mode
					bool isCreative = true;
					if (sv.size() >= 3 && sv[2] == "survival")
						isCreative = false;

					// Read the world type
					int worldType = WorldType::Old;
					if (sv.size() >= 4 && sv[3] == "infinite")
						worldType = WorldType::Infinite;
		else if (sv.size() >= 4 && sv[3] == "alpha")
			worldType = WorldType::Alpha;

					// Start a new level with the given name and seed
					LevelSettings settings(seed, isCreative? GameType::Creative : GameType::Survival, worldType);
					LOGI("Creating a level with id '%s', name '%s' and seed '%d'\n", levelId.c_str(), levelName.c_str(), seed);
					minecraft->selectLevel(levelId, levelName, settings);
					if (minecraft->legacyStorageRejected())
						return;
					minecraft->hostMultiplayer();
					minecraft->setScreen(new ProgressScreen());
					_hasStartedLevel = true;
				}
				_state = _STATE_DEFAULT;
			}
		#endif

		return;
	}

	worldsList->tick();

	if (worldsList->hasPickedLevel) {
		minecraft->selectLevel(worldsList->pickedLevel.id, worldsList->pickedLevel.name, LevelSettings::None());
		if (minecraft->legacyStorageRejected())
			return;
		minecraft->hostMultiplayer();
		minecraft->setScreen(new ProgressScreen());
		_hasStartedLevel = true;
		return;
	}

	// copy the currently selected item
	LevelSummary selectedWorld;
	//bool hasSelection = false;
	if (isIndexValid(worldsList->selectedItem))
	{
		selectedWorld = worldsList->levels[worldsList->selectedItem];
		//hasSelection = true;
	}

	bEdit.active = isIndexValid(worldsList->selectedItem);
}

void SelectWorldScreen::init()
{
	worldsList = new WorldSelectionList(minecraft, width, height);
	loadLevelSource();
	worldsList->commit();

	buttons.push_back(&bEdit);
	buttons.push_back(&bCreate);
	buttons.push_back(&bBack);

	_mouseHasBeenUp = !Mouse::getButtonState(MouseAction::ACTION_LEFT);

	tabButtons.push_back(&bWorldView);
	tabButtons.push_back(&bEdit);
	tabButtons.push_back(&bCreate);
	tabButtons.push_back(&bBack);
}

void WorldSelectionList::mouseScrolled(int delta) {
	if (mode != 0 || delta == 0) return;
	// Use the existing snap animation instead of leaving the carousel between
	// worlds after a free-form horizontal offset.
	if (delta > 0)
		stepLeft();
	else
		stepRight();
}

void SelectWorldScreen::mouseScrolled(int x, int y, int delta)
{
	if (worldsList != NULL)
		worldsList->mouseScrolled(delta);
}

void SelectWorldScreen::setupPositions() {
	int yBase = height - 28;

	//#ifdef ANDROID
	bCreate.y =	yBase;
	bBack.y   = yBase;
	bEdit.y = yBase;

	bBack.width = bEdit.width = bCreate.width = 84;
	//bEdit.h = bCreate.h = bBack.h = 24;
	//#endif

	// Center buttons
	bEdit.x   = width / 2 - 4 - bEdit.width - bEdit.width / 2;
	bCreate.x   = width / 2					- bCreate.width / 2;
	bBack.x     = width / 2 + 4 + bCreate.width - bBack.width / 2;
}

void SelectWorldScreen::render( int xm, int ym, float a )
{
	//Performance::watches.get("sws-full").start();
	//Performance::watches.get("sws-renderbg").start();
	renderBackground();
	//Performance::watches.get("sws-renderbg").stop();
	//Performance::watches.get("sws-worlds").start();

	worldsList->setComponentSelected(bWorldView.selected);

	if (_mouseHasBeenUp)
		worldsList->render(xm, ym, a);
	else {
		worldsList->render(0, 0, a);
		_mouseHasBeenUp = !Mouse::getButtonState(MouseAction::ACTION_LEFT);
	}

	//Performance::watches.get("sws-worlds").stop();
	//Performance::watches.get("sws-screen").start();
	Screen::render(xm, ym, a);
	//Performance::watches.get("sws-screen").stop();

	//Performance::watches.get("sws-string").start();
	drawCenteredString(minecraft->font, "Select world", width / 2, 8, 0xffffffff);
	//Performance::watches.get("sws-string").stop();

	//Performance::watches.get("sws-full").stop();
	//Performance::watches.printEvery(128);
}

void SelectWorldScreen::loadLevelSource()
{
	LevelStorageSource* levelSource = minecraft->getLevelSource();
	levelSource->getLevelList(levels);
	std::sort(levels.begin(), levels.end());

	for (unsigned int i = 0; i < levels.size(); ++i) {
		if (levels[i].id != LevelStorageSource::TempLevelId)
			worldsList->levels.push_back( levels[i] );
	}
}


std::string SelectWorldScreen::getUniqueLevelName( const std::string& level )
{
	std::set<std::string> Set;
	for (unsigned int i = 0; i < levels.size(); ++i)
		Set.insert(levels[i].id);

	std::string s = level;
	while ( Set.find(s) != Set.end() )
		s += "-";
	return s;
}

bool SelectWorldScreen::isInGameScreen() { return true;  }

void SelectWorldScreen::keyPressed( int eventKey )
{
	if (bWorldView.selected) {
		if (eventKey == minecraft->options.keyLeft.key)
			worldsList->stepLeft();
		if (eventKey == minecraft->options.keyRight.key)
			worldsList->stepRight();
	}

	Screen::keyPressed(eventKey);
}

//
// Delete World Screen
//
DeleteWorldScreen::DeleteWorldScreen(const LevelSummary& level)
:	ConfirmScreen(NULL, "Are you sure you want to delete this world?",
						"'" + level.name + "' will be lost forever!",
						"Delete", "Cancel", 0),
	_level(level)
{
	tabButtonIndex = 1;
}

void DeleteWorldScreen::postResult( bool isOk )
{
	if (isOk) {
		LevelStorageSource* storageSource = minecraft->getLevelSource();
		storageSource->deleteLevel(_level.id);
	}
	minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
}

EditWorldScreen::EditWorldScreen(const LevelSummary& level)
:   _level(level),
	levelNameBox(NULL),
	bApply(NULL),
	bDelete(NULL),
	bConvert(NULL),
	bUpgrade(NULL),
	bBack(NULL),
	_convertible(false),
	_finiteUpgradeable(false)
{
}

EditWorldScreen::~EditWorldScreen()
{
	delete levelNameBox;
	delete bApply;
	delete bDelete;
	delete bConvert;
	delete bUpgrade;
	delete bBack;
}

void EditWorldScreen::init()
{
	LevelStorageSource* storageSource = minecraft->getLevelSource();
	_convertible = storageSource != NULL &&
		storageSource->isConvertible(_level.id);
	_finiteUpgradeable = storageSource != NULL &&
		storageSource->canConvertFiniteLevelToInfinite(_level.id);

	levelNameBox = new TextBox(4, "World name");
	levelNameBox->text = _level.name;
	bApply = new Button(5, "Apply");
	bDelete = new Button(1, "Delete");
	bBack = new Button(3, "Back");
	buttons.push_back(bDelete);
	if (_convertible) {
		bConvert = new Button(2, "Convert");
		buttons.push_back(bConvert);
	}
	if (_finiteUpgradeable) {
		bUpgrade = new Button(6, "Upgrade");
		buttons.push_back(bUpgrade);
	}
	buttons.push_back(bApply);
	buttons.push_back(bBack);

	tabButtons.push_back(bDelete);
	if (bConvert != NULL)
		tabButtons.push_back(bConvert);
	if (bUpgrade != NULL)
		tabButtons.push_back(bUpgrade);
	tabButtons.push_back(bApply);
	tabButtons.push_back(bBack);
	textBoxes.push_back(levelNameBox);
}

void EditWorldScreen::setupPositions()
{
	// Keep the editor action buttons the same width as the New World buttons.
	const int buttonWidth = (width / 2) - 16;
	const int buttonHeight = 22;
	const int applyWidth = 100;
	const int gap = 4;
	const int nameRowGap = 8;
	const int nameRowWidth = buttonWidth + applyWidth + nameRowGap;
	const int nameRowX = (width - nameRowWidth) / 2;
	const int nameRowY = 34;

	levelNameBox->x = nameRowX;
	levelNameBox->y = nameRowY;
	levelNameBox->width = buttonWidth;
	levelNameBox->height = buttonHeight;
	bApply->width = applyWidth;
	bApply->height = buttonHeight;
	bApply->x = nameRowX + buttonWidth + nameRowGap;
	bApply->y = nameRowY;

	const int actionCount = 1 + (_convertible ? 1 : 0) +
		(_finiteUpgradeable ? 1 : 0);
	const int totalHeight = actionCount * buttonHeight +
		(actionCount - 1) * gap;
	const int actionX = (width - buttonWidth) / 2;
	int actionTop = height / 2 - totalHeight / 2;
	const int minimumActionTop = nameRowY + buttonHeight + 12;
	if (actionTop < minimumActionTop)
		actionTop = minimumActionTop;

	bDelete->width = buttonWidth;
	bDelete->height = buttonHeight;
	bDelete->x = actionX;
	bDelete->y = actionTop;

	int nextActionY = actionTop + buttonHeight + gap;
	if (bConvert != NULL) {
		bConvert->width = buttonWidth;
		bConvert->height = buttonHeight;
		bConvert->x = actionX;
		bConvert->y = nextActionY;
		nextActionY += buttonHeight + gap;
	}

	if (bUpgrade != NULL) {
		bUpgrade->width = buttonWidth;
		bUpgrade->height = buttonHeight;
		bUpgrade->x = actionX;
		bUpgrade->y = nextActionY;
	}

	bBack->width = buttonWidth;
	bBack->height = buttonHeight;
	bBack->x = actionX;
	bBack->y = height - buttonHeight - 4;
}

void EditWorldScreen::tick()
{
	levelNameBox->tick(minecraft);
}

void EditWorldScreen::render(int xm, int ym, float a)
{
	renderBackground();
	drawCenteredString(font, "Edit world", width / 2, 8, 0xffffffff);
	drawString(font, "World name:", levelNameBox->x,
		levelNameBox->y - Font::DefaultLineHeight - 2, 0xffcccccc);
	const int linkY = bBack->y - Font::DefaultLineHeight - 4;
	const int useUwpY = linkY - Font::DefaultLineHeight - 2;
	drawCenteredString(font, kPe09ConversionNotice, width / 2,
		useUwpY - Font::DefaultLineHeight - 2, 0xffaaaaaa);
	drawCenteredString(font, kUseUwpConversionNotice, width / 2, useUwpY,
		0xffaaaaaa);
	drawCenteredString(font, kUwpRepositoryLabel, width / 2, linkY,
		0xffcccccc);
	super::render(xm, ym, a);
}

void EditWorldScreen::buttonClicked(Button* button)
{
	if (button == bDelete)
		minecraft->setScreen(new DeleteWorldScreen(_level));
	else if (button == bConvert)
		minecraft->openLegacyWorldConversionScreen(_level.id);
	else if (button == bUpgrade)
		minecraft->openFiniteWorldUpgradeScreen(_level.id);
	else if (button == bApply) {
		std::string newName = Util::stringTrim(levelNameBox->text);
		if (!newName.empty()) {
			minecraft->getLevelSource()->renameLevel(_level.id, newName);
			// renameLevel may also change the folder id. Rebuild the list so that
			// subsequent Delete/Convert actions cannot use the old id.
			minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
		}
	}
	else if (button == bBack)
		minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
}

void EditWorldScreen::mouseClicked(int x, int y, int buttonNum)
{
	const int linkY = bBack->y - Font::DefaultLineHeight - 4;
	const int linkWidth = font->width(kUwpRepositoryLabel);
	if (buttonNum == MouseAction::ACTION_LEFT &&
		x >= (width - linkWidth) / 2 && x < (width + linkWidth) / 2 &&
		y >= linkY && y < linkY + Font::DefaultLineHeight) {
		minecraft->platform()->openUrl(kUwpRepositoryUrl);
		return;
	}
	super::mouseClicked(x, y, buttonNum);
}

bool EditWorldScreen::handleBackEvent(bool isDown)
{
	if (!isDown)
		minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
	return true;
}

void EditWorldScreen::keyPressed(int eventKey)
{
	if (eventKey == Keyboard::KEY_ESCAPE) {
		minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
		return;
	}
	super::keyPressed(eventKey);
}
