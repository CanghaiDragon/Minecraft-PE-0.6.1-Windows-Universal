#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__

#include "ChooseLevelScreen.h"
#include "../components/Button.h"
#include "../components/TextBox.h"

class SimpleChooseLevelScreen: public ChooseLevelScreen
{
public:
	SimpleChooseLevelScreen(const std::string& levelName);

	virtual ~SimpleChooseLevelScreen();

	void init();
	void setupPositions();
	void tick();
	void render(int xm, int ym, float a);
	void buttonClicked(Button* button);
	void mouseClicked(int x, int y, int buttonNum);
	void mouseReleased(int x, int y, int buttonNum);
	void mouseScrolled(int x, int y, int delta);
	bool handleBackEvent(bool isDown);
	void keyPressed(int eventKey);
	void keyboardNewChar(char inputChar);

private:
	// Header / nav
	Touch::THeader* bTitle;
	Button* bBack;

	// Step 1: game mode
	Button* bCreative;
	Button* bSurvival;

	// Step 2: world type
	Button* bOldWorld;
	Button* bInfiniteWorld;
	Button* bSkyWorld;
	Button* bAlphaWorld;

	bool hasChosen;
	int chosenGameType; // -1 until chosen
	void startLevel(int worldType);
	void setSeedControlsVisible(bool visible);
	void updateScrollBounds();
	void applyScrollOffset();
	void resetScroll();
	TextBox worldNameBox;
	TextBox seedBox;

	int contentTop;
	int visibleBottom;
	int visibleHeight;
	int contentHeight;
	int baseWorldNameY;
	int baseSeedY;
	int baseCreativeY;
	int baseSurvivalY;
	int baseOldWorldY;
	int baseInfiniteWorldY;
	int baseSkyWorldY;
	int baseAlphaWorldY;
	float scrollY;
	float scrollVelocity;
	bool scrollDragging;
	bool scrollMoved;
	int dragStartY;
	int lastDragY;
	float dragStartScroll;

	std::string levelName;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__*/
