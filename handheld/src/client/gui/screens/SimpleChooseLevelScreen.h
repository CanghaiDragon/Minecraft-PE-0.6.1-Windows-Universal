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

	bool hasChosen;
	int chosenGameType; // -1 until chosen
	void startLevel(int worldType);
	void setSeedControlsVisible(bool visible);
	TextBox worldNameBox;
	TextBox seedBox;

	std::string levelName;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__*/
