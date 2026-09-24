#include "TextBox.h"
#include "../Gui.h"
#include "../Font.h"
#include "../../Minecraft.h"
#include "../../../AppPlatform.h"
#include "../../../platform/input/Mouse.h"

TextBox::TextBox(int id, const std::string& msg)
 : TextBox(id, 0, 0, msg) {}

TextBox::TextBox(int id, int x, int y, const std::string& msg)
 : TextBox(id, x, y, 24, Font::DefaultLineHeight + 4, msg) {}

TextBox::TextBox(int id, int x, int y, int w, int h, const std::string& msg)
 : GuiElement(true, true, x, y, w, h), id(id), hint(msg), focused(false), blink(false), blinkTicks(0) {}

void TextBox::setFocus(Minecraft* minecraft) {
	if(!focused) {
		focused = true;
		blinkTicks = 0;
		blink = false;
	}
}

bool TextBox::loseFocus(Minecraft* minecraft) {
	if(focused) {
		focused = false;
		return true;
	}
	return false;
}

void TextBox::mouseClicked(Minecraft* minecraft, int x, int y, int buttonNum) {
	if (buttonNum != MouseAction::ACTION_LEFT || !visible) return;
	if (pointInside(x, y)) setFocus(minecraft);
	else loseFocus(minecraft);
}

void TextBox::keyPressed(Minecraft* minecraft, int key) {
	if (focused && key == Keyboard::KEY_BACKSPACE && !text.empty())
		text.erase(text.length() - 1);
}

void TextBox::charPressed(Minecraft* minecraft, char c) {
	if (focused && c >= 32 && c < 127 && text.length() < 48)
		text += c;
}

void TextBox::tick(Minecraft* minecraft) {
	if (++blinkTicks >= 5) {
		blink = !blink;
		blinkTicks = 0;
	}
}

void TextBox::render( Minecraft* minecraft, int xm, int ym ) {
	if (!visible) return;
	// The reference touch UI uses a thin light frame around a black input area.
	fill(x, y, x + width, y + height, focused ? 0xffffffff : 0xffa0a0a0);
	fill(x + 1, y + 1, x + width - 1, y + height - 1, 0xff000000);

	glEnable2(GL_SCISSOR_TEST);
	glScissor((int)(Gui::GuiScale * (x + 2)),
		minecraft->height - (int)(Gui::GuiScale * (y + height - 2)),
		(int)(Gui::GuiScale * (width - 4)), (int)(Gui::GuiScale * (height - 4)));
	int textY = y + (height - Font::DefaultLineHeight) / 2;
	if (text.empty() && !focused)
		drawString(minecraft->font, hint, x + 2, textY, 0xff5e5e5e);
	std::string displayed = text;
	if (focused && blink) displayed += "_";
	drawString(minecraft->font, displayed, x + 2, textY, 0xffffffff);
	glDisable2(GL_SCISSOR_TEST);
}
