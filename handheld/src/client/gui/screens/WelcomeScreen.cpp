#include "WelcomeScreen.h"

#include "../../Minecraft.h"
#include "../Gui.h"
#include "../../renderer/gles.h"
#include "ScreenChooser.h"
#include "../../../platform/input/Mouse.h"
#include "../../../platform/input/Multitouch.h"
#include "../../../locale/I18n.h"

WelcomeScreen::WelcomeScreen()
:	touchButton(1, "Touch Controls"),
	keyboardButton(2, "Keyboard & Mouse"),
	guiScaleButton(3, "GUI Scale"),
	continueButton(5, "Continue"),
	touchControls(true),
	guiScale(0),
	scrollY(0),
	contentHeight(0),
	dragging(false),
	dragMoved(false),
	dragLastY(0)
{
}

void WelcomeScreen::init()
{
	touchControls = minecraft->options.useTouchScreen;
	guiScale = minecraft->options.guiScale;

	buttons.push_back(&touchButton);
	buttons.push_back(&keyboardButton);
	buttons.push_back(&guiScaleButton);
	buttons.push_back(&continueButton);
	updateSelection();
}

void WelcomeScreen::setupPositions()
{
	// touchgui's button strip is only 66px wide before scaling.  Keeping the
	// controls compact avoids stretched/cut-off ends on large GUI scales.
	int buttonWidth = width - 24;
	if (buttonWidth > 220) buttonWidth = 220;
	const int buttonHeight = 26;
	const int x = (width - buttonWidth) / 2;

	touchButton.x = keyboardButton.x = guiScaleButton.x = continueButton.x = x;
	touchButton.width = keyboardButton.width = guiScaleButton.width = continueButton.width = buttonWidth;
	touchButton.height = keyboardButton.height = guiScaleButton.height = continueButton.height = buttonHeight;

	touchButton.y = 66;
	keyboardButton.y = 98;
	guiScaleButton.y = 140;
	continueButton.y = 172;
	contentHeight = continueButton.y + continueButton.height + 48;
	clampScroll();
}

void WelcomeScreen::render(int xm, int ym, float a)
{
	// Match Screen::mouseEvent's coordinate conversion for desktop pointers.
	// GameRenderer's touch coordinate may intentionally be -9999 when no
	// touch pointer is active, so keep that path unchanged.
	// On a touch-capable Windows device useTouchscreen() can remain true even
	// after choosing keyboard/mouse mode.  In that case GameRenderer supplies
	// -9999 while no finger is down, which prevents mouse hover highlighting.
	// Use the real mouse position whenever it is available, and override it
	// only while an actual touch pointer is active.
	int hoverX = Mouse::getX() * width / minecraft->width;
	int hoverY = Mouse::getY() * height / minecraft->height - 1;
	if (Multitouch::getFirstActivePointerIdExThisUpdate() >= 0) {
		hoverX = xm;
		hoverY = ym;
	}
	renderDirtBackground(0);
	const int headerHeight = 30;
	fill(0, 0, width, headerHeight, 0x99000000);
	drawCenteredString(font, "Welcome", width / 2, 10, 0xffffff);

	const int visibleHeight = height - headerHeight;
	GLint scissorY = minecraft->height - (GLint)(Gui::GuiScale * height);
	glEnable2(GL_SCISSOR_TEST);
	glScissor(0, scissorY, (GLsizei)(Gui::GuiScale * width), (GLsizei)(Gui::GuiScale * visibleHeight));
	glPushMatrix2();
	glTranslatef2(0, -scrollY, 0);

	drawCenteredString(font, "Choose your controls", width / 2, 38, 0xffffff);
	drawCenteredString(font, "Touch or keyboard/mouse controls are exclusive in a world.", width / 2, 52, 0xc0c0c0);
	drawCenteredString(font, "Adjust interface size", width / 2, 128, 0xffffff);
	drawCenteredString(font, "You are free to change these settings at any time in options.", width / 2, 206, 0xc0c0c0);
	drawCenteredString(font, "Don't forget to take a look at \"Additional\" in options!", width / 2, 218, 0xc0c0c0);
	for (unsigned int i = 0; i < buttons.size(); ++i)
		buttons[i]->render(minecraft, hoverX, hoverY + (int)scrollY);

	glPopMatrix2();
	const int maxScroll = contentHeight - visibleHeight;
	if (maxScroll > 0) {
		float barHeight = (float)visibleHeight * visibleHeight / contentHeight;
		if (barHeight < 8) barHeight = 8;
		float barY = headerHeight + (scrollY / maxScroll) * (visibleHeight - barHeight);
		fill(width - 4, (int)barY, width - 2, (int)(barY + barHeight), 0x88ffffff);
	}
	glDisable2(GL_SCISSOR_TEST);
}

bool WelcomeScreen::handleBackEvent(bool isDown)
{
	return true;
}

void WelcomeScreen::mouseEvent()
{
	const MouseAction& event = Mouse::getEvent();
	if (event.action == MouseAction::ACTION_MOVE && Mouse::isButtonDown(MouseAction::ACTION_LEFT) && event.dy != 0) {
		scrollY -= event.dy * height / minecraft->height;
		clampScroll();
		dragging = true;
		dragMoved = true;
		return;
	}
	Screen::mouseEvent();
}

void WelcomeScreen::mouseClicked(int x, int y, int buttonNum)
{
	dragging = true;
	dragMoved = false;
	dragLastY = y;
	Screen::mouseClicked(x, y + (int)scrollY, buttonNum);
}

void WelcomeScreen::mouseReleased(int x, int y, int buttonNum)
{
	if (dragMoved) {
		if (clickedButton != NULL) {
			clickedButton->released(x, y + (int)scrollY);
			clickedButton = NULL;
		}
		dragging = false;
		return;
	}
	dragging = false;
	Screen::mouseReleased(x, y + (int)scrollY, buttonNum);
}

void WelcomeScreen::mouseScrolled(int x, int y, int delta)
{
	scrollY -= delta * 18;
	clampScroll();
}

void WelcomeScreen::tick()
{
	// Touch input is not guaranteed to be mirrored into the legacy Mouse
	// queue on Windows.  Read the active pointer directly so the welcome page
	// can be dragged on a touchscreen just like the options page.
	const int* pointerIds = NULL;
	const int pointerCount = Multitouch::getActivePointerIdsThisUpdate(&pointerIds);
	for (int i = 0; i < pointerCount; ++i) {
		const int pointerId = pointerIds[i];
		if (Multitouch::isPointerDown(pointerId)) {
			const int dy = Multitouch::getDY(pointerId);
			if (dy != 0) {
				scrollY -= (float)dy * Gui::InvGuiScale;
				clampScroll();
				dragging = true;
				dragMoved = true;
			}
		}
	}
	// Some Windows touch drivers expose the contact through the legacy Mouse
	// state only.  Keep a drag baseline so those contacts scroll as well.
	if (dragging && Mouse::isButtonDown(MouseAction::ACTION_LEFT)) {
		int my = Mouse::getY() * height / minecraft->height - 1;
		const int dy = my - dragLastY;
		if (dy != 0) {
			scrollY -= (float)dy;
			clampScroll();
			dragMoved = true;
			dragLastY = my;
		}
	}
	super::tick();
}

void WelcomeScreen::keyPressed(int eventKey)
{
	// The welcome screen must not acquire the desktop menu's initial focus or
	// be dismissed with Escape before its two choices have been made.
}

void WelcomeScreen::buttonClicked(Button* button)
{
	if (button == &touchButton) {
		touchControls = true;
		updateSelection();
	} else if (button == &keyboardButton) {
		touchControls = false;
		updateSelection();
	} else if (button == &guiScaleButton) {
		guiScale = (guiScale + 1) % 5;
		updateSelection();
		// Apply immediately so setSize() recalculates Gui::GuiScale and
		// re-lays out this welcome page for the newly selected size.
		minecraft->options.set(&Options::Option::GUI_SCALE, guiScale);
	} else if (button == &continueButton) {
		const bool inputModeChanged = minecraft->options.useTouchScreen != touchControls;
		minecraft->options.useTouchScreen = touchControls;
		minecraft->options.set(&Options::Option::GUI_SCALE, guiScale);
		minecraft->options.save();
		// reloadOptions() sees the already-updated value and therefore cannot
		// tell that the existing TouchInputHolder must be replaced.  Use the
		// same transition path as the Controls setting so D-Pad/touch look is
		// fully torn down before the desktop start menu is opened.
		if (inputModeChanged)
			minecraft->optionUpdated(&Options::Option::INPUT_MODE, touchControls ? 1 : 0);
		minecraft->reloadOptions();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
}

void WelcomeScreen::updateSelection()
{
	touchButton.selected = touchControls;
	keyboardButton.selected = !touchControls;
	guiScaleButton.selected = false;
	guiScaleButton.msg = std::string("GUI Scale: ") + I18n::get(Options::GUI_SCALE[guiScale]);
}

void WelcomeScreen::clampScroll()
{
	const int maxScroll = contentHeight - (height - 30);
	if (scrollY < 0) scrollY = 0;
	if (maxScroll <= 0) scrollY = 0;
	else if (scrollY > maxScroll) scrollY = (float)maxScroll;
}
