#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__

#include "../Screen.h"
#include "../components/Button.h"

class ImageButton;
class OptionsPane;
class TextBox;

class OptionsScreen: public Screen
{
	typedef Screen super;
	void init();

	void generateOptionScreens();

public:
	OptionsScreen();
	~OptionsScreen();
	void setupPositions();
	void buttonClicked( Button* button );
	void render(int xm, int ym, float a);
	void removed();
	void selectCategory(int index);

	virtual void mouseClicked( int x, int y, int buttonNum );
	virtual void mouseReleased( int x, int y, int buttonNum );
	virtual void mouseScrolled(int x, int y, int delta);
	virtual void tick();
	virtual void keyPressed(int eventKey);
	virtual void keyboardNewChar(char inputChar);
private:
	void saveUsername();
	Touch::THeader* bHeader;
	ImageButton* btnClose;
	std::vector<Touch::TButton*> categoryButtons;
	std::vector<OptionsPane*> optionPanes;
	OptionsPane* currentOptionPane;
	TextBox* usernameBox;
	int selectedCategory;

	// Sidebar scroll state
	float _catScrollY;
	float _catScrollVelocity;
	int _catContentHeight;
	int _catVisibleHeight;
	bool _catDragging;
	int _catDragStartY;
	float _catDragStartScroll;
	int _catLastDragY;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__*/
