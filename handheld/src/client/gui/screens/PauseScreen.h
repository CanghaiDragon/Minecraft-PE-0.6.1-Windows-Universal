#ifndef NET_MINECRAFT_CLIENT_GUI__PauseScreen_H__
#define NET_MINECRAFT_CLIENT_GUI__PauseScreen_H__

//package net.minecraft.client.gui;

#include "../Screen.h"
#include "../components/ImageButton.h"

class Button;

// The F3 button uses two separate ImageDefs (dark/light) rather than the
// normal horizontal hover frame used by ImageButton.  Hover feedback is
// still provided by ImageButton::scaleWhenPressed.
class F3ImageButton : public ImageButton {
public:
	F3ImageButton(int id, const std::string& msg) : ImageButton(id, msg) {}

protected:
	virtual bool isSecondImage(bool hovered) { return false; }
};

class PauseScreen: public Screen
{
	typedef Screen super;
public:
	PauseScreen(bool wasBackPaused);
	~PauseScreen();

	void init();
	void setupPositions();

	void tick();
	void render(int xm, int ym, float a);
protected:
    void buttonClicked(Button* button);
private:
	void updateServerVisibilityText();

	int saveStep;
	int visibleTime;
	bool wasBackPaused;

	Button* bContinue;
	Button* bQuit;
	Button* bQuitAndSaveLocally;
	Button* bServerVisibility;
//	Button* bThirdPerson;

	OptionButton bSound;
	OptionButton bThirdPerson;
    OptionButton bHideGui;
	F3ImageButton bEntityButton;
	ImageDef entityButtonOff;
	ImageDef entityButtonOn;
	bool entityButtonState;
};

#endif /*NET_MINECRAFT_CLIENT_GUI__PauseScreen_H__*/
