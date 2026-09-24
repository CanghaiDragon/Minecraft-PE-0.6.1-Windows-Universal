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
	bCreative(0),
	bSurvival(0),
	bOldWorld(0),
	bInfiniteWorld(0),
	bSkyWorld(0),
	hasChosen(false),
	chosenGameType(-1),
	worldNameBox(0, "World name"),
	seedBox(1, "World seed"),
	levelName(levelName)
{
}

SimpleChooseLevelScreen::~SimpleChooseLevelScreen()
{
	delete bTitle;
	delete bBack;
	delete bCreative;
	delete bSurvival;
	delete bOldWorld;
	delete bInfiniteWorld;
	delete bSkyWorld;
}

void SimpleChooseLevelScreen::init()
{
	ChooseLevelScreen::init();
	bTitle        = new Touch::THeader(0, "Game Mode");
	if (minecraft->useTouchscreen()) {
		bBack         = new Touch::TButton(3, "Back");
		bCreative     = new Touch::TButton(1, "Creative");
		bSurvival     = new Touch::TButton(2, "Survival");
		bOldWorld     = new Touch::TButton(4, "Old World");
		bInfiniteWorld= new Touch::TButton(5, "Infinite");
		bSkyWorld     = new Touch::TButton(8, "Sky");
	} else {
		// The desktop flow keeps the PE page layout, but uses the same classic
		// Java button skin as the keyboard-and-mouse title and pause menus.
		bBack         = new Button(3, "Back");
		bCreative     = new Button(1, "Creative");
		bSurvival     = new Button(2, "Survival");
		bOldWorld     = new Button(4, "Old World");
		bInfiniteWorld= new Button(5, "Infinite");
		bSkyWorld     = new Button(8, "Sky");
	}

	bOldWorld->visible      = false;  bOldWorld->active      = false;
	bInfiniteWorld->visible = false;  bInfiniteWorld->active = false;
	bSkyWorld->visible = false; bSkyWorld->active = false;

	buttons.push_back(bTitle);
	buttons.push_back(bBack);
	buttons.push_back(bCreative);
	buttons.push_back(bSurvival);
	buttons.push_back(bOldWorld);
	buttons.push_back(bInfiniteWorld);
	buttons.push_back(bSkyWorld);

	tabButtons.push_back(bBack);
	tabButtons.push_back(bCreative);
	tabButtons.push_back(bSurvival);
	tabButtons.push_back(bOldWorld);
	tabButtons.push_back(bInfiniteWorld);
	tabButtons.push_back(bSkyWorld);
	textBoxes.push_back(&worldNameBox);
	textBoxes.push_back(&seedBox);
	worldNameBox.text = levelName;
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
	const int btnH  = minecraft->useTouchscreen() ? 36 : 22;
	const int btnY  = headerH + ((height - headerH) * 3) / 4 - btnH / 2;
	const int textBoxWidth = width < 236 ? width - 24 : 212;
	const int textBoxHeight = 22;
	worldNameBox.x = (width - textBoxWidth) / 2;
	worldNameBox.y = headerH + 30;
	worldNameBox.width = textBoxWidth;
	worldNameBox.height = textBoxHeight;
	seedBox.x = worldNameBox.x;
	seedBox.y = headerH + 82;
	seedBox.width = textBoxWidth;
	seedBox.height = textBoxHeight;


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
	// Keep the touch strips compact: the source texture does not stretch
	// cleanly across a full large-GUI width.
	int worldTypeWidth = width - 24;
	const int maxWorldTypeWidth = minecraft->useTouchscreen() ? 220 : 200;
	if (worldTypeWidth > maxWorldTypeWidth) worldTypeWidth = maxWorldTypeWidth;
	bOldWorld->width = bInfiniteWorld->width = bSkyWorld->width = worldTypeWidth;
	bSkyWorld->height = btnH;
	bOldWorld->x = bInfiniteWorld->x = bSkyWorld->x = (width - worldTypeWidth) / 2;
	bOldWorld->y = headerH + 12;
	bInfiniteWorld->y = bOldWorld->y + btnH + 8;
	bSkyWorld->y = bInfiniteWorld->y + btnH + 8;
}

void SimpleChooseLevelScreen::render( int xm, int ym, float a )
{
	renderBackground();
    glEnable2(GL_BLEND);

	int descY = bCreative->y + bCreative->height + 6;
	if (chosenGameType == -1) {
		drawString(minecraft->font, "World name:", worldNameBox.x, worldNameBox.y - Font::DefaultLineHeight - 2, 0xffcccccc);
		drawString(minecraft->font, "World seed:", seedBox.x, seedBox.y - Font::DefaultLineHeight - 2, 0xffcccccc);
		drawCenteredString(minecraft->font, "Unlimited resources, no damage", bCreative->x + bCreative->width / 2, descY, 0xffaaaaaa);
		drawCenteredString(minecraft->font, "Survive, gather, build",          bSurvival->x + bSurvival->width / 2, descY, 0xffaaaaaa);
	} else {
		drawCenteredString(minecraft->font, "Sky: floating islands", width / 2,
			bSkyWorld->y + bSkyWorld->height + 8, 0xffaaaaaa);
	}

	Screen::render(xm, ym, a);
    glDisable2(GL_BLEND);
}

void SimpleChooseLevelScreen::buttonClicked( Button* button )
{
	if (button == bBack) {
		if (chosenGameType != -1) {
			// Go back to step 1
			chosenGameType = -1;
			bTitle->msg     = "Game Mode";
			bCreative->visible = true;   bCreative->active = true;
			bSurvival->visible = true;   bSurvival->active = true;
			bOldWorld->visible = false;  bOldWorld->active = false;
			bInfiniteWorld->visible = false; bInfiniteWorld->active = false;
			bSkyWorld->visible = false; bSkyWorld->active = false;
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

		// PE 0.6.1 only creates Old worlds.  The optional switch exposes all
		// additional generators as a separate world-type step.
		if (!minecraft->options.infiniteWorlds) {
			startLevel(WorldType::Old);
			return;
		}

		bTitle->msg     = "World Type";
		bCreative->visible = false;  bCreative->active = false;
		bSurvival->visible = false;  bSurvival->active = false;
		bOldWorld->visible = true;   bOldWorld->active = true;
		bInfiniteWorld->visible = true;  bInfiniteWorld->active = true;
		bSkyWorld->visible = true; bSkyWorld->active = true;
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
	else if (button == bSkyWorld)
		worldType = WorldType::Sky;
	else
		return;

	startLevel(worldType);
}

void SimpleChooseLevelScreen::startLevel(int worldType) {
	long seed = getEpochTimeS();
	if (!seedBox.text.empty()) {
		long parsed;
		if (sscanf(seedBox.text.c_str(), "%ld", &parsed) == 1)
			seed = parsed;
		else
			seed = Util::hashCode(seedBox.text);
	}
	std::string requestedName = Util::stringTrim(worldNameBox.text);
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
	Screen::keyPressed(eventKey);
}

void SimpleChooseLevelScreen::keyboardNewChar(char inputChar) {
	Screen::keyboardNewChar(inputChar);
}

void SimpleChooseLevelScreen::mouseClicked(int x, int y, int buttonNum) {
	Screen::mouseClicked(x, y, buttonNum);
}

void SimpleChooseLevelScreen::setSeedControlsVisible(bool visible) {
	worldNameBox.visible = visible;
	worldNameBox.active = visible;
	seedBox.visible = visible;
	seedBox.active = visible;
	if (!visible) {
		worldNameBox.loseFocus(minecraft);
		seedBox.loseFocus(minecraft);
	}
}

void SimpleChooseLevelScreen::tick() {
	worldNameBox.tick(minecraft);
	seedBox.tick(minecraft);
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
			bSkyWorld->visible = false; bSkyWorld->active = false;
			setSeedControlsVisible(true);
		} else {
			minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
		}
	}
	return true;
}
