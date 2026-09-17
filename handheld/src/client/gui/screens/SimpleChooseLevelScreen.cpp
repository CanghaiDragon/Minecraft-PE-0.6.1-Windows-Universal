#include "SimpleChooseLevelScreen.h"
#include "ProgressScreen.h"
#include "ScreenChooser.h"
#include "../../Minecraft.h"
#include "../../../world/level/LevelSettings.h"
#include "../../../platform/time.h"
#include "../../../util/StringUtils.h"
#include <cstdio>

SimpleChooseLevelScreen::SimpleChooseLevelScreen(const std::string& levelName)
:	bTitle(0),
	bBack(0),
	bWorldName(0),
	bSeed(0),
	bCreative(0),
	bSurvival(0),
	bOldWorld(0),
	bInfiniteWorld(0),
	levelName(levelName),
	hasChosen(false),
	chosenGameType(-1),
	seedFocused(false),
	worldNameFocused(false),
	worldName(levelName)
{
}

SimpleChooseLevelScreen::~SimpleChooseLevelScreen()
{
	delete bTitle;
	delete bBack;
	delete bWorldName;
	delete bSeed;
	delete bCreative;
	delete bSurvival;
	delete bOldWorld;
	delete bInfiniteWorld;
}

void SimpleChooseLevelScreen::init()
{
	bTitle        = new Touch::THeader(0, "Game Mode");
	bBack         = new Touch::TButton(3, "Back");
	bWorldName    = new Touch::TButton(7, "World Name");
	bSeed         = new Touch::TButton(6, "Seed: Random");
	// The field is drawn below in the Java-edition style.  The invisible button
	// keeps the standard touch hit-testing and release behavior.
	bWorldName->visible = false;
	bSeed->visible = false;
	bCreative     = new Touch::TButton(1, "Creative");
	bSurvival     = new Touch::TButton(2, "Survival");
	bOldWorld     = new Touch::TButton(4, "Old World");
	bInfiniteWorld= new Touch::TButton(5, "Infinite");

	bOldWorld->visible      = false;  bOldWorld->active      = false;
	bInfiniteWorld->visible = false;  bInfiniteWorld->active = false;

	buttons.push_back(bTitle);
	buttons.push_back(bBack);
	buttons.push_back(bWorldName);
	buttons.push_back(bSeed);
	buttons.push_back(bCreative);
	buttons.push_back(bSurvival);
	buttons.push_back(bOldWorld);
	buttons.push_back(bInfiniteWorld);

	tabButtons.push_back(bBack);
	tabButtons.push_back(bCreative);
	tabButtons.push_back(bSurvival);
	tabButtons.push_back(bOldWorld);
	tabButtons.push_back(bInfiniteWorld);
	tabButtons.push_back(bWorldName);
	tabButtons.push_back(bSeed);
}

void SimpleChooseLevelScreen::setupPositions()
{
	const int headerH = bTitle->height; // 26px
	const int bw = 100;
	const int bh = bBack->height; // 26px

	// Header bar: title spans between Back button and right edge
	bBack->x  = 0;
	bBack->y  = 0;
	bBack->width = bw;

	bTitle->x    = bBack->width;
	bTitle->y    = 0;
	bTitle->width = width - bBack->width;

	// Content buttons — two big buttons side by side in the center
	const int btnW  = (width / 2) - 16;
	const int btnH  = 36;
	const int btnY  = headerH + ((height - headerH) * 3) / 4 - btnH / 2;
	bWorldName->x = 8;
	bWorldName->y = headerH + 30;
	bWorldName->width = width - 16;
	bWorldName->height = 32;
	bSeed->x = 8;
	bSeed->y = headerH + 106;
	bSeed->width = width - 16;
	bSeed->height = 32;


	bCreative->width  = btnW;  bCreative->height  = btnH;
	bSurvival->width  = btnW;  bSurvival->height  = btnH;
	bOldWorld->width  = btnW;  bOldWorld->height  = btnH;
	bInfiniteWorld->width  = btnW;  bInfiniteWorld->height  = btnH;

	bCreative->x  = 8;
	bCreative->y  = btnY;
	bSurvival->x  = width / 2 + 8;
	bSurvival->y  = btnY;

	bOldWorld->x      = 8;
	bOldWorld->y      = btnY;
	bInfiniteWorld->x = width / 2 + 8;
	bInfiniteWorld->y = btnY;
}

void SimpleChooseLevelScreen::render( int xm, int ym, float a )
{
	renderBackground();
    glEnable2(GL_BLEND);

	int descY = bCreative->y + bCreative->height + 6;
	if (chosenGameType == -1) {
		std::string displayedName = worldName.empty() ? "World" : worldName;
		while (minecraft->font->width(displayedName) > bWorldName->width - 8)
			displayedName.erase(0, 1);
		std::string displayedSeed = seedText.empty() ? "Random" : seedText;
		while (minecraft->font->width(displayedSeed) > bSeed->width - 8)
			displayedSeed.erase(0, 1);
		// Match the Java Edition fields: dark input areas with thin outlines.
		fill(bWorldName->x - 2, bWorldName->y - 2, bWorldName->x + bWorldName->width + 2, bWorldName->y + bWorldName->height + 2,
			 worldNameFocused ? 0xffffffff : 0xffa0a0a0);
		fill(bWorldName->x, bWorldName->y, bWorldName->x + bWorldName->width, bWorldName->y + bWorldName->height, 0xff000000);
		drawString(minecraft->font, "World Name", bWorldName->x, bWorldName->y - 18, 0xffaaaaaa);
		drawString(minecraft->font, displayedName + (worldNameFocused ? "_" : ""), bWorldName->x + 4,
			bWorldName->y + (bWorldName->height - minecraft->font->height(displayedName)) / 2, 0xffffffff);
		drawString(minecraft->font, "Name your new world", bWorldName->x, bWorldName->y + bWorldName->height + 10, 0xffaaaaaa);

		fill(bSeed->x - 2, bSeed->y - 2, bSeed->x + bSeed->width + 2, bSeed->y + bSeed->height + 2,
			 seedFocused ? 0xffffffff : 0xffa0a0a0);
		fill(bSeed->x, bSeed->y, bSeed->x + bSeed->width, bSeed->y + bSeed->height, 0xff000000);
		drawString(minecraft->font, "Seed for the World Generator", bSeed->x, bSeed->y - 18, 0xffaaaaaa);
		drawString(minecraft->font, displayedSeed + (seedFocused ? "_" : ""), bSeed->x + 4,
			bSeed->y + (bSeed->height - minecraft->font->height(displayedSeed)) / 2, 0xffffffff);
		drawString(minecraft->font, "Leave blank for a random seed", bSeed->x, bSeed->y + bSeed->height + 10, 0xffaaaaaa);
		drawCenteredString(minecraft->font, "Unlimited resources, no damage", bCreative->x + bCreative->width / 2, descY, 0xffaaaaaa);
		drawCenteredString(minecraft->font, "Survive, gather, build",          bSurvival->x + bSurvival->width / 2, descY, 0xffaaaaaa);
	} else {
		drawCenteredString(minecraft->font, "Finite 256x256",    bOldWorld->x      + bOldWorld->width      / 2, descY, 0xffaaaaaa);
		drawCenteredString(minecraft->font, "Endless world",     bInfiniteWorld->x + bInfiniteWorld->width / 2, descY, 0xffaaaaaa);
	}

	Screen::render(xm, ym, a);
    glDisable2(GL_BLEND);
}

void SimpleChooseLevelScreen::buttonClicked( Button* button )
{
	if (chosenGameType == -1 && button == bWorldName) {
		worldNameFocused = true;
		seedFocused = false;
		minecraft->platform()->showKeyboard();
		return;
	}
	if (chosenGameType == -1 && button == bSeed) {
		seedFocused = true;
		worldNameFocused = false;
		minecraft->platform()->showKeyboard();
		return;
	}

	if (button == bBack) {
		if (chosenGameType != -1) {
			// Go back to step 1
			chosenGameType = -1;
			bTitle->msg     = "Game Mode";
			bCreative->visible = true;   bCreative->active = true;
			bSurvival->visible = true;   bSurvival->active = true;
			bOldWorld->visible = false;  bOldWorld->active = false;
			bInfiniteWorld->visible = false; bInfiniteWorld->active = false;
			setSeedControlsVisible(true);
		} else {
			minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
		}
		return;
	}

	// Step 1: pick game mode
	if (chosenGameType == -1) {
		if (button == bCreative)
			chosenGameType = GameType::Creative;
		else if (button == bSurvival)
			chosenGameType = GameType::Survival;
		else
			return;

		if (!minecraft->options.infiniteWorlds) {
			startLevel(WorldType::Old);
			return;
		}

		bTitle->msg     = "World Type";
		bCreative->visible = false;  bCreative->active = false;
		bSurvival->visible = false;  bSurvival->active = false;
		bOldWorld->visible = true;   bOldWorld->active = true;
		bInfiniteWorld->visible = true;  bInfiniteWorld->active = true;
		setSeedControlsVisible(false);
		return;
	}

	// Step 2: pick world type
	if (hasChosen) return;

	int worldType = -1;
	if (button == bOldWorld)
		worldType = WorldType::Old;
	else if (button == bInfiniteWorld)
		worldType = WorldType::Infinite;
	else
		return;

	startLevel(worldType);
}

void SimpleChooseLevelScreen::startLevel(int worldType) {
	long seed = getEpochTimeS();
	if (!seedText.empty()) {
		long parsed;
		if (sscanf(seedText.c_str(), "%ld", &parsed) == 1)
			seed = parsed;
		else
			seed = Util::hashCode(seedText);
	}
	std::string requestedName = Util::stringTrim(worldName);
	std::string safeName;
	for (unsigned int i = 0; i < requestedName.length(); ++i) {
		char c = requestedName[i];
		if (c != '/' && c != '\\' && c != ':' && c != '*' && c != '?' &&
			c != '"' && c != '<' && c != '>' && c != '|')
			safeName += c;
	}
	if (safeName.empty()) safeName = "world";
	std::string levelId = getUniqueLevelName(safeName);
	LevelSettings settings(seed, chosenGameType, worldType);
	minecraft->selectLevel(levelId, levelId, settings);
	minecraft->hostMultiplayer();
	minecraft->setScreen(new ProgressScreen());
	hasChosen = true;
}

void SimpleChooseLevelScreen::keyPressed(int eventKey) {
	if (chosenGameType == -1 && eventKey == Keyboard::KEY_BACKSPACE && worldNameFocused && !worldName.empty())
		worldName.erase(worldName.size() - 1);
	else if (chosenGameType == -1 && eventKey == Keyboard::KEY_BACKSPACE && seedFocused && !seedText.empty())
		seedText.erase(seedText.size() - 1);
	else if (chosenGameType == -1 && (seedFocused || worldNameFocused) && eventKey == Keyboard::KEY_RETURN) {
		seedFocused = false;
		worldNameFocused = false;
	}
	else
		Screen::keyPressed(eventKey);
}

void SimpleChooseLevelScreen::keyboardNewChar(char inputChar) {
	if (chosenGameType == -1 && worldNameFocused && inputChar >= 32 && inputChar < 127 && worldName.length() < 48)
		worldName += inputChar;
	else if (chosenGameType == -1 && seedFocused)
		appendSeedCharacter(inputChar);
}

void SimpleChooseLevelScreen::mouseClicked(int x, int y, int buttonNum) {
	Screen::mouseClicked(x, y, buttonNum);
}

void SimpleChooseLevelScreen::appendSeedCharacter(char character) {
	if (character >= 32 && character < 127 && seedText.length() < 48) {
		seedText += character;
		seedFocused = true;
	}
}

void SimpleChooseLevelScreen::setSeedControlsVisible(bool visible) {
	bWorldName->visible = false;
	bWorldName->active = visible;
	bSeed->visible = false;
	bSeed->active = visible;
	if (!visible) {
		seedFocused = false;
		worldNameFocused = false;
	}
}

bool SimpleChooseLevelScreen::handleBackEvent(bool isDown) {
	if (!isDown) {
		if (chosenGameType != -1) {
			chosenGameType = -1;
			bTitle->msg     = "Game Mode";
			bCreative->visible = true;   bCreative->active = true;
			bSurvival->visible = true;   bSurvival->active = true;
			bOldWorld->visible = false;  bOldWorld->active = false;
			bInfiniteWorld->visible = false; bInfiniteWorld->active = false;
			setSeedControlsVisible(true);
		} else {
			minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
		}
	}
	return true;
}
