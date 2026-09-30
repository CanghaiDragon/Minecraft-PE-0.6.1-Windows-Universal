#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchStartMenuScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchStartMenuScreen_H__

#include "../../Screen.h"
#include "../../components/LargeImageButton.h"
#include "../../components/ImageButton.h"

class BuyButton: public ImageButton {
	typedef ImageButton super;
public:
	BuyButton(int id);
	void render(Minecraft* minecraft, int xm, int ym);
};

namespace Touch {

class StartMenuScreen: public Screen
{
public:
	StartMenuScreen();
	virtual ~StartMenuScreen();
	
	void init();
	void setupPositions();
	
	void tick();
	void render(int xm, int ym, float a);

	void buttonClicked(Button* button);
	void mouseClicked(int x, int y, int buttonNum);
	bool handleBackEvent(bool isDown);
	bool isInGameScreen();
private:
	void _updateLicense();
	
	LargeImageButton bHost;
	LargeImageButton bJoin;
	LargeImageButton bOptions;
	ImageButton bQuit;
	TButton bTest;
	BuyButton bBuy;

	std::string copyright;
	int copyrightPosX;

	std::string version;
	int versionPosX;

	std::string githubUrl;
	std::string githubLabel;
	std::string buildInfo;
};
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchStartMenuScreen_H__*/
