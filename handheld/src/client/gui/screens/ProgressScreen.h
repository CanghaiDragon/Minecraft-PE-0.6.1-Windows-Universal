#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__ProgressScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__ProgressScreen_H__

#include "../Screen.h"

class ProgressScreen: public Screen
{
public:
	ProgressScreen(bool savingWorld = false);

	void render(int xm, int ym, float a);
	bool isInGameScreen();

	void tick();
private:
	int ticks;
	bool savingWorld;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__ProgressScreen_H__*/
