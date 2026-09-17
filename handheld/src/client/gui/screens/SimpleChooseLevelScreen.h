#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__

#include "ChooseLevelScreen.h"
#include "../components/Button.h"

class SimpleChooseLevelScreen: public ChooseLevelScreen
{
public:
	SimpleChooseLevelScreen(const std::string& levelName);

	virtual ~SimpleChooseLevelScreen();

	void init();
	void setupPositions();
	void render(int xm, int ym, float a);
	void buttonClicked(Button* button);
	void mouseClicked(int x, int y, int buttonNum);
	bool handleBackEvent(bool isDown);
	void keyPressed(int eventKey);
	void keyboardNewChar(char inputChar);

private:
	// Header / nav
	Touch::THeader* bTitle;
	Touch::TButton* bBack;

	// Step 1: game mode
	// The seed field is a normal touch button so that it receives focus and
	// opens the system keyboard when the player taps it.
	Touch::TButton* bWorldName;
	Touch::TButton* bSeed;
	Touch::TButton* bCreative;
	Touch::TButton* bSurvival;

	// Step 2: world type
	Touch::TButton* bOldWorld;
	Touch::TButton* bInfiniteWorld;

	bool hasChosen;
	int chosenGameType; // -1 until chosen
	bool seedFocused;
	bool worldNameFocused;
	void startLevel(int worldType);
	void setSeedControlsVisible(bool visible);
	void appendSeedCharacter(char character);

	std::string levelName;
	std::string worldName;
	std::string seedText;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__*/
