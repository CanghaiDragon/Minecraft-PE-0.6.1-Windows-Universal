#include "OptionsScreen.h"

#include "StartMenuScreen.h"
#include "DialogDefinitions.h"
#include "../../Minecraft.h"
#include "../../../AppPlatform.h"
#include "../../../locale/I18n.h"

#include "../components/OptionsPane.h"
#include "../components/ImageButton.h"
#include "../components/OptionsGroup.h"
#include "../components/TextBox.h"
#include "../Gui.h"
#include "../../renderer/gles.h"
#include "../../../platform/input/Mouse.h"
#include "../../../util/Mth.h"
OptionsScreen::OptionsScreen()
: btnClose(NULL),
  bHeader(NULL),
  currentOptionPane(NULL),
  usernameBox(NULL),
  selectedCategory(0),
  _catScrollY(0),
  _catScrollVelocity(0),
  _catContentHeight(0),
  _catVisibleHeight(0),
  _catDragging(false),
  _catDragStartY(0),
  _catDragStartScroll(0),
  _catLastDragY(0) {
}

OptionsScreen::~OptionsScreen() {
	if(btnClose != NULL) {
		delete btnClose;
		btnClose = NULL;
	}
	if(bHeader != NULL) {
		delete bHeader,
		bHeader = NULL;
	}
	for(std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		if(*it != NULL) {
			delete *it;
			*it = NULL;
		}
	}
	for(std::vector<OptionsPane*>::iterator it = optionPanes.begin(); it != optionPanes.end(); ++it) {
		if(*it != NULL) {
			delete *it;
			*it = NULL;
		}
	}
	categoryButtons.clear();
	optionPanes.clear();
	currentOptionPane = NULL;
}

void OptionsScreen::init() {
	bHeader = new Touch::THeader(0, "Options");
	btnClose = new ImageButton(1, "");
	ImageDef def;
	def.name = "gui/touchgui.png";
	def.width = 34;
	def.height = 26;

	def.setSrc(IntRectangle(150, 0, (int)def.width, (int)def.height));
	btnClose->setImageDef(def, true);

	// Category order follows Options.md.  Audio sits directly above Additional.
	Touch::TButton* b;
	b = new Touch::TButton(2, I18n::get("options.category.game"));       b->width = 80; b->height = 28; categoryButtons.push_back(b);
	b = new Touch::TButton(3, I18n::get("options.category.controls"));   b->width = 80; b->height = 28; categoryButtons.push_back(b);
	b = new Touch::TButton(4, I18n::get("options.category.graphics"));   b->width = 80; b->height = 28; categoryButtons.push_back(b);
	b = new Touch::TButton(5, I18n::get("options.category.audio"));      b->width = 80; b->height = 28; categoryButtons.push_back(b);
	b = new Touch::TButton(6, I18n::get("options.category.additional")); b->width = 80; b->height = 28; categoryButtons.push_back(b);
	if (minecraft->options.skinMenu) {
		b = new Touch::TButton(7, I18n::get("options.category.skin"));       b->width = 80; b->height = 28; categoryButtons.push_back(b);
	}
	buttons.push_back(bHeader);
	buttons.push_back(btnClose);
	for(std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		buttons.push_back(*it);
		tabButtons.push_back(*it);
	}
	generateOptionScreens();

}
void OptionsScreen::setupPositions() {
	int headerH = btnClose->height;
	int catW = (categoryButtons.size() > 0) ? categoryButtons[0]->width : 80;

	btnClose->x = width - btnClose->width;
	btnClose->y = 0;

	bHeader->x = 0;
	bHeader->y = 0;
	bHeader->width = width - btnClose->width;
	bHeader->height = headerH;

	int curY = headerH + 2;
	for(std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		(*it)->x = 0;
		(*it)->y = curY;
		(*it)->selected = false;
		curY += (*it)->height + 1;
	}
	_catContentHeight = curY - (headerH + 2);

	for(std::vector<OptionsPane*>::iterator it = optionPanes.begin(); it != optionPanes.end(); ++it) {
		(*it)->x = catW + 2;
		(*it)->y = headerH + 2;
		(*it)->width = width - catW - 2;
		(*it)->setupPositions();
	}
	selectCategory(0);
}

void OptionsScreen::render( int xm, int ym, float a ) {
	renderBackground();

	int catW = (categoryButtons.size() > 0) ? categoryButtons[0]->width : 80;
	int headerH = (bHeader != NULL) ? bHeader->height : 26;

	// Sidebar background & border
	fill(0, headerH, catW, height, 0x66000000);
	fill(catW, headerH, catW + 1, height, 0x88888888);

	// Render header and close button (unscissored)
	if (bHeader != NULL) ((Button*)bHeader)->render(minecraft, xm, ym);
	if (btnClose != NULL) ((Button*)btnClose)->render(minecraft, xm, ym);

	// --- Scrollable sidebar category buttons ---
	int sidebarTop = headerH + 2;
	_catVisibleHeight = height - sidebarTop;
	if (_catVisibleHeight < 1) _catVisibleHeight = 1;
	int maxScroll = _catContentHeight - _catVisibleHeight;
	if (maxScroll < 0) maxScroll = 0;
	if (_catScrollY < 0) _catScrollY = 0;
	if (_catScrollY > maxScroll) _catScrollY = (float)maxScroll;

	float scale = Gui::GuiScale;
	int physH = minecraft->height;
	GLint sx = 0;
	GLint sy = physH - (GLint)(scale * (sidebarTop + _catVisibleHeight));
	GLsizei sw = (GLsizei)(scale * catW);
	GLsizei sh = (GLsizei)(scale * _catVisibleHeight);
	glEnable2(GL_SCISSOR_TEST);
	glScissor(sx, sy, sw, sh);

	glPushMatrix2();
	glTranslatef2(0, -(int)_catScrollY, 0);

	// Render only category buttons inside scissor
	for (size_t i = 0; i < categoryButtons.size(); ++i) {
		categoryButtons[i]->render(minecraft, xm, ym);
	}

	glPopMatrix2();

	// Sidebar scrollbar
	if (maxScroll > 0) {
		float barH = (float)_catVisibleHeight * _catVisibleHeight / _catContentHeight;
		if (barH < 8) barH = 8;
		float barY = sidebarTop + (_catScrollY / maxScroll) * (_catVisibleHeight - barH);
		int barX = catW - 3;
		fill(barX, (int)barY, barX + 2, (int)(barY + barH), 0x88ffffff);
	}

	glDisable2(GL_SCISSOR_TEST);

	// GameRenderer already gives Screen coordinates in GUI units.  Applying a
	// second conversion here made option-row hover tests miss at non-1x GUI
	// scales, even though the click path used the right coordinates.
	if(currentOptionPane != NULL)
		currentOptionPane->render(minecraft, xm, ym);
}

void OptionsScreen::removed()
{
	saveUsername();
}

void OptionsScreen::saveUsername()
{
	if (usernameBox == NULL || usernameBox->text == minecraft->options.username)
		return;
	minecraft->options.username = usernameBox->text;
	minecraft->options.save();
}
void OptionsScreen::buttonClicked( Button* button ) {
	if(button == btnClose) {
		saveUsername();
		minecraft->reloadOptions();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	} else if(button->id > 1 && button->id < 8) {
		// This is a category button
		int categoryButton = button->id - categoryButtons[0]->id;
		selectCategory(categoryButton);
	}
}

void OptionsScreen::selectCategory( int index ) {
	int currentIndex = 0;
	for(std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		if(index == currentIndex) {
			(*it)->selected = true;
		} else {
			(*it)->selected = false;
		}
		currentIndex++;
	}
	if(index < (int)optionPanes.size())
		currentOptionPane = optionPanes[index];
}

void OptionsScreen::generateOptionScreens() {
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	// Game
	OptionsGroup& gameGroup = optionPanes[0]->createOptionsGroup("options.group.game");
	usernameBox = gameGroup.addTextInput("Username", "Username", minecraft->options.username);
	gameGroup.addOptionItem(&Options::Option::DIFFICULTY, minecraft)
		.addOptionItem(&Options::Option::THIRD_PERSON, minecraft)
		.addOptionItem(&Options::Option::SERVER_VISIBLE, minecraft);

	// Controls
	OptionsGroup& controlsGroup = optionPanes[1]->createOptionsGroup("options.group.controls");
	controlsGroup
		.addOptionItem(&Options::Option::INPUT_MODE, minecraft)
		.addOptionItem(&Options::Option::SENSITIVITY, minecraft)
		.addOptionItem(&Options::Option::DPAD_SIZE, minecraft)
		.addOptionItem(&Options::Option::LEFT_HANDED, minecraft)
		.addOptionItem(&Options::Option::USE_TOUCH_JOYPAD, minecraft)
		.addOptionItem(&Options::Option::INVERT_MOUSE, minecraft)
		.addDisabledItem("Vibrate on Destroy", "Unavailable");

	// Graphics
	optionPanes[2]->createOptionsGroup("options.group.graphics")
		.addOptionItem(&Options::Option::GRAPHICS, minecraft)
		.addOptionItem(&Options::Option::AMBIENT_OCCLUSION, minecraft)
		.addOptionItem(&Options::Option::RENDER_DISTANCE, minecraft)
		.addOptionItem(&Options::Option::GUI_SCALE, minecraft)
		.addOptionItem(&Options::Option::VIEW_BOBBING, minecraft)
		.addOptionItem(&Options::Option::FOV, minecraft)
		.addOptionItem(&Options::Option::HIDE_GUI, minecraft);

	// Audio
	optionPanes[3]->createOptionsGroup("options.group.audio")
		.addOptionItem(&Options::Option::MUSIC, minecraft)
		.addOptionItem(&Options::Option::SOUND, minecraft);

	// Additional: opt-in features and clearly marked future placeholders.
	optionPanes[4]->createOptionsGroup("options.category.additional")
		.addOptionItem(&Options::Option::INFINITE_WORLDS, minecraft)
		.addOptionItem(&Options::Option::SKIN_MENU, minecraft)
		.addDescription("Re-enter Options to open Skin Settings")
		.addOptionItem(&Options::Option::TOUCH_SNEAK, minecraft)
		.addOptionItem(&Options::Option::BETA_VISUALS, minecraft)
		.addOptionItem(&Options::Option::DEBUG_SCREEN, minecraft)
		.addDisabledItem("Chinese", "Coming soon");

	// Skin: choose the model explicitly; imported skins are always 64x64.
	OptionsGroup& skinGroup = optionPanes[5]->createOptionsGroup("options.category.skin");
	skinGroup
		.addOptionItem(&Options::Option::SKIN_ARM_TYPE, minecraft)
		.addDescription("Choose Classic or Slim model")
		.addImportSkinItem(minecraft)
		.addDescription("Use a 64x64 PNG skin")
		.addClearSkinCacheItem(minecraft)
		.addDescription("Restart the game to apply skin changes");
// 	int mojangGroup = optionPanes[0]->createOptionsGroup("Mojang");
// 	static const int arr[] = {5,4,3,15};
// 	std::vector<int> vec (arr, arr + sizeof(arr) / sizeof(arr[0]) );
// 	optionPanes[0]->createStepSlider(minecraft, mojangGroup, "This works?", &Options::Option::DIFFICULTY, vec);
// 
// 	// Game Pane
// 	int gameGroup = optionPanes[1]->createOptionsGroup("Game");
// 	optionPanes[1]->createToggle(gameGroup, "Third person camera", &Options::Option::THIRD_PERSON);
// 	optionPanes[1]->createToggle(gameGroup, "Server visible", &Options::Option::SERVER_VISIBLE);
// 	
// 	// Input Pane
// 	int controlsGroup = optionPanes[2]->createOptionsGroup("Controls");
// 	optionPanes[2]->createToggle(controlsGroup, "Invert X-axis", &Options::Option::INVERT_MOUSE);
// 	optionPanes[2]->createToggle(controlsGroup, "Lefty", &Options::Option::LEFT_HANDED);
// 	optionPanes[2]->createToggle(controlsGroup, "Use touch screen", &Options::Option::USE_TOUCHSCREEN);
// 	optionPanes[2]->createToggle(controlsGroup, "Split touch controls", &Options::Option::USE_TOUCH_JOYPAD);
// 	int feedBackGroup = optionPanes[2]->createOptionsGroup("Feedback");
// 	optionPanes[2]->createToggle(feedBackGroup, "Vibrate on destroy", &Options::Option::DESTROY_VIBRATION);
// 
// 	int graphicsGroup = optionPanes[3]->createOptionsGroup("Graphics");
// 	optionPanes[3]->createProgressSlider(minecraft, graphicsGroup, "Gui Scale", &Options::Option::PIXELS_PER_MILLIMETER, 3, 4);
// 	optionPanes[3]->createToggle(graphicsGroup, "Fancy Graphics", &Options::Option::INVERT_MOUSE);
// 	optionPanes[3]->createToggle(graphicsGroup, "Fancy Skies", &Options::Option::INVERT_MOUSE);
// 	optionPanes[3]->createToggle(graphicsGroup, "Animated water", &Options::Option::INVERT_MOUSE);
// 	int experimentalGraphicsGroup = optionPanes[3]->createOptionsGroup("Experimental graphics");
// 	optionPanes[3]->createToggle(experimentalGraphicsGroup, "Soft shadows", &Options::Option::INVERT_MOUSE);
}

void OptionsScreen::mouseClicked( int x, int y, int buttonNum ) {
	int catW = (categoryButtons.size() > 0) ? categoryButtons[0]->width : 80;
	int headerH = (bHeader != NULL) ? bHeader->height : 26;
	int sidebarTop = headerH + 2;

	if (x >= 0 && x < catW && y >= sidebarTop) {
		// Click in sidebar — start drag
		_catDragging = true;
		_catDragStartY = y;
		_catDragStartScroll = _catScrollY;
		_catLastDragY = y;
		_catScrollVelocity = 0;
		// Pass click with scroll offset to buttons
		super::mouseClicked(x, y + (int)_catScrollY, buttonNum);
	} else {
		if(currentOptionPane != NULL)
			currentOptionPane->mouseClicked(minecraft, x, y, buttonNum);
		super::mouseClicked(x, y, buttonNum);
	}
}

void OptionsScreen::mouseReleased( int x, int y, int buttonNum ) {
	int catW = (categoryButtons.size() > 0) ? categoryButtons[0]->width : 80;
	int headerH = (bHeader != NULL) ? bHeader->height : 26;
	int sidebarTop = headerH + 2;

	_catDragging = false;

	if (x >= 0 && x < catW && y >= sidebarTop) {
		// Release in sidebar — offset y for button hit test
		super::mouseReleased(x, y + (int)_catScrollY, buttonNum);
	} else {
		if(currentOptionPane != NULL)
			currentOptionPane->mouseReleased(minecraft, x, y, buttonNum);
		super::mouseReleased(x, y, buttonNum);
	}
}

void OptionsScreen::mouseScrolled(int x, int y, int delta) {
	int catW = (categoryButtons.size() > 0) ? categoryButtons[0]->width : 80;
	if (x < catW) {
		int maxScroll = _catContentHeight - _catVisibleHeight;
		if (maxScroll < 0) maxScroll = 0;
		_catScrollY -= delta * 18;
		if (_catScrollY < 0) _catScrollY = 0;
		if (_catScrollY > maxScroll) _catScrollY = (float)maxScroll;
		_catScrollVelocity = 0;
	} else if (currentOptionPane != NULL) {
		currentOptionPane->mouseScrolled(delta);
	}
}

void OptionsScreen::keyPressed(int eventKey)
{
	if (usernameBox != NULL)
		usernameBox->keyPressed(minecraft, eventKey);
	super::keyPressed(eventKey);
}

void OptionsScreen::keyboardNewChar(char inputChar)
{
	if (usernameBox != NULL)
		usernameBox->charPressed(minecraft, inputChar);
}

void OptionsScreen::tick() {
	// Sidebar scroll handling
	if (_catDragging) {
		int mx = Mouse::getX();
		int my = Mouse::getY();
		toGUICoordinate(mx, my);
		float delta = (float)(_catDragStartY - my);
		_catScrollVelocity = (float)(_catLastDragY - my);
		_catLastDragY = my;
		_catScrollY = _catDragStartScroll + delta;
	} else if (_catScrollVelocity > 0.5f || _catScrollVelocity < -0.5f) {
		_catScrollY += _catScrollVelocity;
		_catScrollVelocity *= 0.85f;
	}

	if(currentOptionPane != NULL)
		currentOptionPane->tick(minecraft);
	super::tick();
}
