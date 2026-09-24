#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__WelcomeScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__WelcomeScreen_H__

#include "../Screen.h"
#include "../components/Button.h"

// Shown only when the game has no saved options yet.  It deliberately uses
// the touch-style controls regardless of the choice currently being made.
class WelcomeScreen: public Screen
{
	typedef Screen super;
public:
	WelcomeScreen();

	void init();
	void setupPositions();
	void render(int xm, int ym, float a);
	bool handleBackEvent(bool isDown);
	void mouseEvent();

protected:
	void buttonClicked(Button* button);
	void mouseClicked(int x, int y, int buttonNum);
	void mouseReleased(int x, int y, int buttonNum);
	void mouseScrolled(int x, int y, int delta);
	void tick();
	// This is deliberately a touch-style setup page, even before the user has
	// selected an input mode.  Do not give it the desktop menu's key focus.
	void keyPressed(int eventKey);

private:
	void updateSelection();
	void clampScroll();

	Touch::TButton touchButton;
	Touch::TButton keyboardButton;
	Touch::TButton guiScaleButton;
	Touch::TButton continueButton;
	bool touchControls;
	int guiScale;
	float scrollY;
	int contentHeight;
	bool dragging;
	bool dragMoved;
	int dragLastY;
};

#endif /* NET_MINECRAFT_CLIENT_GUI_SCREENS__WelcomeScreen_H__ */
