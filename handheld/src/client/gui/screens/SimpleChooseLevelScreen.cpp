#include "SimpleChooseLevelScreen.h"
#include "ProgressScreen.h"
#include "ScreenChooser.h"
#include "../../Minecraft.h"
#include "../Gui.h"
#include "../../renderer/gles.h"
#include "../../../platform/input/Mouse.h"
#include "../../../world/level/LevelSettings.h"
#include "../../../platform/time.h"
#include "../../../util/StringUtils.h"
#include <cmath>
#include <cstdio>

SimpleChooseLevelScreen::SimpleChooseLevelScreen(const std::string& levelName)
:	bTitle(0),
	bBack(0),
	bCreative(0),
	bSurvival(0),
	bOldWorld(0),
	bInfiniteWorld(0),
	bSkyWorld(0),
	bAlphaWorld(0),
	hasChosen(false),
	chosenGameType(-1),
	worldNameBox(0, "World name"),
	seedBox(1, "World seed"),
	contentTop(0),
	visibleBottom(0),
	visibleHeight(0),
	contentHeight(0),
	baseWorldNameY(0),
	baseSeedY(0),
	baseCreativeY(0),
	baseSurvivalY(0),
	baseOldWorldY(0),
	baseInfiniteWorldY(0),
	baseSkyWorldY(0),
	baseAlphaWorldY(0),
	scrollY(0),
	scrollVelocity(0),
	scrollDragging(false),
	scrollMoved(false),
	dragStartY(0),
	lastDragY(0),
	dragStartScroll(0),
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
	delete bAlphaWorld;
}

void SimpleChooseLevelScreen::init()
{
	ChooseLevelScreen::init();
	bTitle        = new Touch::THeader(0, "Game Mode");
	if (!minecraft->options.javaUi) {
		bBack         = new Touch::TButton(3, "Back");
		bCreative     = new Touch::TButton(1, "Creative");
		bSurvival     = new Touch::TButton(2, "Survival");
		bOldWorld     = new Touch::TButton(4, "Old World");
		bInfiniteWorld= new Touch::TButton(5, "Infinite");
		bSkyWorld     = new Touch::TButton(8, "Sky");
		bAlphaWorld   = new Touch::TButton(9, "Alpha");
	} else {
		// The desktop flow keeps the PE page layout, but uses the same classic
		// Java button skin as the keyboard-and-mouse title and pause menus.
		bBack         = new Button(3, "Back");
		bCreative     = new Button(1, "Creative");
		bSurvival     = new Button(2, "Survival");
		bOldWorld     = new Button(4, "Old World");
		bInfiniteWorld= new Button(5, "Infinite");
		bSkyWorld     = new Button(8, "Sky");
		bAlphaWorld   = new Button(9, "Alpha");
	}

	bOldWorld->visible      = false;  bOldWorld->active      = false;
	bInfiniteWorld->visible = false;  bInfiniteWorld->active = false;
	bSkyWorld->visible = false; bSkyWorld->active = false;
	bAlphaWorld->visible = false; bAlphaWorld->active = false;

	buttons.push_back(bTitle);
	buttons.push_back(bBack);
	buttons.push_back(bCreative);
	buttons.push_back(bSurvival);
	buttons.push_back(bOldWorld);
	buttons.push_back(bInfiniteWorld);
	buttons.push_back(bSkyWorld);
	buttons.push_back(bAlphaWorld);

	tabButtons.push_back(bBack);
	tabButtons.push_back(bCreative);
	tabButtons.push_back(bSurvival);
	tabButtons.push_back(bOldWorld);
	tabButtons.push_back(bInfiniteWorld);
	tabButtons.push_back(bSkyWorld);
	tabButtons.push_back(bAlphaWorld);
	textBoxes.push_back(&worldNameBox);
	textBoxes.push_back(&seedBox);
	worldNameBox.text = levelName;
}

void SimpleChooseLevelScreen::setupPositions()
{
	const bool useJavaUi = minecraft->options.javaUi;
	const int headerH = bTitle->height; // 26px
	const int bw = 100;
	contentTop = headerH + 8;
	visibleBottom = height - 4;
	if (visibleBottom < contentTop)
		visibleBottom = contentTop;

	// Header bar: title spans between Back button and right edge
	bBack->x  = 0;
	bBack->y  = 0;
	bBack->width = bw;

	bTitle->x    = bBack->width;
	bTitle->y    = 0;
	bTitle->width = width - bBack->width;

	// Content buttons — two big buttons side by side in the center
	const int btnW  = (width / 2) - 16;
	const int btnH  = useJavaUi ? 22 : 36;
	const int textBoxWidth = width < 236 ? width - 24 : 212;
	const int textBoxHeight = 22;
	worldNameBox.x = (width - textBoxWidth) / 2;
	baseWorldNameY = contentTop + 24;
	worldNameBox.y = baseWorldNameY;
	worldNameBox.width = textBoxWidth;
	worldNameBox.height = textBoxHeight;
	seedBox.x = worldNameBox.x;
	baseSeedY = contentTop + 76;
	seedBox.y = baseSeedY;
	seedBox.width = textBoxWidth;
	seedBox.height = textBoxHeight;


	bCreative->width  = btnW;  bCreative->height  = btnH;
	bSurvival->width  = btnW;  bSurvival->height  = btnH;
	bOldWorld->width  = btnW;  bOldWorld->height  = btnH;
	bInfiniteWorld->width  = btnW;  bInfiniteWorld->height  = btnH;

	bCreative->x  = 8;
	baseCreativeY = baseSeedY + textBoxHeight + 18;
	bCreative->y  = baseCreativeY;
	bSurvival->x  = width / 2 + 8;
	baseSurvivalY = baseCreativeY;
	bSurvival->y  = baseSurvivalY;

	bOldWorld->x      = 8;
	baseOldWorldY = contentTop + 12;
	bOldWorld->y      = baseOldWorldY;
	bInfiniteWorld->x = width / 2 + 8;
	baseInfiniteWorldY = baseOldWorldY + btnH + 8;
	bInfiniteWorld->y = baseInfiniteWorldY;
	// Keep the touch strips compact: the source texture does not stretch
	// cleanly across a full large-GUI width.
	int worldTypeWidth = width - 24;
	const int maxWorldTypeWidth = useJavaUi ? 200 : 220;
	if (worldTypeWidth > maxWorldTypeWidth) worldTypeWidth = maxWorldTypeWidth;
	bOldWorld->width = bInfiniteWorld->width = bSkyWorld->width = bAlphaWorld->width = worldTypeWidth;
	bSkyWorld->height = btnH;
	bAlphaWorld->height = btnH;
	bOldWorld->x = bInfiniteWorld->x = bSkyWorld->x = bAlphaWorld->x = (width - worldTypeWidth) / 2;
	baseSkyWorldY = baseInfiniteWorldY + btnH + 8;
	baseAlphaWorldY = baseSkyWorldY + btnH + 8;
	bSkyWorld->y = baseSkyWorldY;
	bAlphaWorld->y = baseAlphaWorldY;

	updateScrollBounds();
	applyScrollOffset();
}

void SimpleChooseLevelScreen::render( int xm, int ym, float a )
{
	renderBackground();
    glEnable2(GL_BLEND);

	updateScrollBounds();
	applyScrollOffset();

	// Keep the header fixed while the form below it scrolls.
	if (visibleHeight > 0) {
		const float scale = Gui::GuiScale;
		const int physicalHeight = minecraft->height;
		const GLint sx = 0;
		const GLint sy = physicalHeight - (GLint)(scale * visibleBottom);
		const GLsizei sw = (GLsizei)(scale * width);
		const GLsizei sh = (GLsizei)(scale * visibleHeight);
		glEnable2(GL_SCISSOR_TEST);
		glScissor(sx, sy, sw, sh);

		if (chosenGameType == -1) {
			drawString(minecraft->font, "World name:", worldNameBox.x,
				worldNameBox.y - Font::DefaultLineHeight - 2, 0xffcccccc);
			drawString(minecraft->font, "World seed:", seedBox.x,
				seedBox.y - Font::DefaultLineHeight - 2, 0xffcccccc);
			const int descY = bCreative->y + bCreative->height + 6;
			drawCenteredString(minecraft->font, "Unlimited resources, no damage",
				bCreative->x + bCreative->width / 2, descY, 0xffaaaaaa);
			drawCenteredString(minecraft->font, "Survive, gather, build",
				bSurvival->x + bSurvival->width / 2, descY, 0xffaaaaaa);
		} else {
			drawCenteredString(minecraft->font, "Sky: floating islands", width / 2,
				bSkyWorld->y + bSkyWorld->height + 8, 0xffaaaaaa);
			drawCenteredString(minecraft->font, "Alpha: classic terrain", width / 2,
				bAlphaWorld->y + bAlphaWorld->height + 8, 0xffaaaaaa);
		}

		for (unsigned int i = 0; i < buttons.size(); ++i) {
			Button* button = buttons[i];
			if (button != bBack && button != bTitle)
				button->render(minecraft, xm, ym);
		}
		for (unsigned int i = 0; i < textBoxes.size(); ++i)
			textBoxes[i]->render(minecraft, xm, ym);

		glDisable2(GL_SCISSOR_TEST);
	}

	// Render the header after the clipped form so it remains fixed and usable.
	bBack->render(minecraft, xm, ym);
	((Button*)bTitle)->render(minecraft, xm, ym);

	const int maxScroll = contentHeight - visibleHeight;
	if (maxScroll > 0) {
		int barHeight = visibleHeight * visibleHeight / contentHeight;
		if (barHeight < 8) barHeight = 8;
		const int barY = contentTop + (int)((scrollY / maxScroll) * (visibleHeight - barHeight));
		fill(width - 3, barY, width - 1, barY + barHeight, 0x88ffffff);
	}

    glDisable2(GL_BLEND);
}

void SimpleChooseLevelScreen::updateScrollBounds()
{
	visibleBottom = height - 4;
	if (visibleBottom < contentTop)
		visibleBottom = contentTop;
	visibleHeight = visibleBottom - contentTop;

	int baseBottom;
	if (chosenGameType == -1) {
		baseBottom = baseCreativeY + bCreative->height;
		if (baseSeedY + seedBox.height > baseBottom)
			baseBottom = baseSeedY + seedBox.height;
	} else {
		baseBottom = baseAlphaWorldY + bAlphaWorld->height;
	}
	contentHeight = baseBottom + 12 - contentTop;
	if (contentHeight < visibleHeight)
		contentHeight = visibleHeight;

	const int maxScroll = contentHeight - visibleHeight;
	if (scrollY < 0)
		scrollY = 0;
	if (scrollY > maxScroll)
		scrollY = (float)maxScroll;
}

void SimpleChooseLevelScreen::applyScrollOffset()
{
	const int offset = (int)scrollY;
	worldNameBox.y = baseWorldNameY - offset;
	seedBox.y = baseSeedY - offset;
	bCreative->y = baseCreativeY - offset;
	bSurvival->y = baseSurvivalY - offset;
	bOldWorld->y = baseOldWorldY - offset;
	bInfiniteWorld->y = baseInfiniteWorldY - offset;
	bSkyWorld->y = baseSkyWorldY - offset;
	bAlphaWorld->y = baseAlphaWorldY - offset;
}

void SimpleChooseLevelScreen::resetScroll()
{
	scrollY = 0;
	scrollVelocity = 0;
	scrollDragging = false;
	scrollMoved = false;
	updateScrollBounds();
	applyScrollOffset();
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
			bAlphaWorld->visible = false; bAlphaWorld->active = false;
			setSeedControlsVisible(true);
			resetScroll();
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
		bAlphaWorld->visible = true; bAlphaWorld->active = true;
		setSeedControlsVisible(false);
		resetScroll();
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
	else if (button == bAlphaWorld)
		worldType = WorldType::Alpha;
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
	if (minecraft->legacyStorageRejected())
		return;
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
	if (buttonNum == MouseAction::ACTION_LEFT
		&& y >= contentTop && y <= visibleBottom) {
		scrollDragging = true;
		scrollMoved = false;
		dragStartY = y;
		lastDragY = y;
		dragStartScroll = scrollY;
		scrollVelocity = 0;
	}
	Screen::mouseClicked(x, y, buttonNum);
}

void SimpleChooseLevelScreen::mouseReleased(int x, int y, int buttonNum) {
	if (scrollDragging)
		scrollDragging = false;
	if (scrollMoved)
		clickedButton = NULL;
	Screen::mouseReleased(x, y, buttonNum);
	scrollMoved = false;
}

void SimpleChooseLevelScreen::mouseScrolled(int x, int y, int delta) {
	if (y < contentTop || y > visibleBottom)
		return;

	updateScrollBounds();
	const int maxScroll = contentHeight - visibleHeight;
	scrollY -= delta * 18;
	if (scrollY < 0)
		scrollY = 0;
	if (scrollY > maxScroll)
		scrollY = (float)maxScroll;
	scrollVelocity = 0;
	applyScrollOffset();
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
	updateScrollBounds();
	if (scrollDragging) {
		int mx = Mouse::getX();
		int my = Mouse::getY();
		toGUICoordinate(mx, my);
		const int movement = dragStartY - my;
		if (std::abs(movement) > 3) {
			scrollMoved = true;
			clickedButton = NULL;
		}
		scrollVelocity = (float)(lastDragY - my);
		lastDragY = my;
		scrollY = dragStartScroll + movement;
	} else if (scrollVelocity > 0.5f || scrollVelocity < -0.5f) {
		scrollY += scrollVelocity;
		scrollVelocity *= 0.85f;
	}
	updateScrollBounds();
	applyScrollOffset();
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
			bAlphaWorld->visible = false; bAlphaWorld->active = false;
			setSeedControlsVisible(true);
			resetScroll();
		} else {
			minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
		}
	}
	return true;
}
