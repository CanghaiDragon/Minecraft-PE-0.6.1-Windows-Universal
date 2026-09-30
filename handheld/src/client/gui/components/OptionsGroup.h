#ifndef NET_MINECRAFT_CLIENT_GUI_COMPONENTS__OptionsGroup_H__
#define NET_MINECRAFT_CLIENT_GUI_COMPONENTS__OptionsGroup_H__

//package net.minecraft.client.gui;

#include <string>
#include "GuiElementContainer.h"
#include "../../Options.h"

class Font;
class Minecraft;
class TextBox;

class OptionsGroup: public GuiElementContainer {
	typedef GuiElementContainer super;
public:
	OptionsGroup(std::string labelID);
	virtual void setupPositions();
	virtual void render(Minecraft* minecraft, int xm, int ym);
	virtual OptionsGroup& addOptionItem(const Options::Option* option, Minecraft* minecraft);
	TextBox* addTextInput(const std::string& label, const std::string& hint, const std::string& value);
	OptionsGroup& addDisabledItem(const std::string& label, const std::string& value);
	OptionsGroup& addDescription(const std::string& text);
	OptionsGroup& addClearSkinCacheItem(Minecraft* minecraft);
	OptionsGroup& addImportSkinItem(Minecraft* minecraft);
protected:
	virtual void createToggle(const Options::Option* option, Minecraft* minecraft);
	virtual void createProgressSlider(const Options::Option* option, Minecraft* minecraft);
	virtual void createStepSlider(const Options::Option* option, Minecraft* minecraft);
	std::string label;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_COMPONENTS__OptionsGroup_H__*/
