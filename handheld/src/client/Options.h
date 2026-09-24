#ifndef NET_MINECRAFT_CLIENT__Options_H__
#define NET_MINECRAFT_CLIENT__Options_H__

//package net.minecraft.client;

//#include "locale/Language.h"

#include <string>
#include <cstdio>
#include "KeyMapping.h"
#include "../platform/input/Keyboard.h"
#include "../util/StringUtils.h"
#include "OptionsFile.h"

class Minecraft;
typedef std::vector<std::string> StringVector;

class Options
{
public:
    class Option
	{
        const bool _isProgress;
        const bool _isBoolean;
        const std::string _captionId;
		const int _ordinal;

	public:
		static const Option MUSIC;
		static const Option SOUND;
		static const Option INVERT_MOUSE;
		static const Option SENSITIVITY;
		static const Option RENDER_DISTANCE;
		static const Option VIEW_BOBBING;
		static const Option ANAGLYPH;
		static const Option LIMIT_FRAMERATE;
		static const Option DIFFICULTY;
		static const Option GRAPHICS;
		static const Option AMBIENT_OCCLUSION;
		static const Option GUI_SCALE;
        
		static const Option THIRD_PERSON;
		static const Option HIDE_GUI;
		static const Option SERVER_VISIBLE;
		static const Option LEFT_HANDED;
		static const Option USE_TOUCHSCREEN;
		static const Option USE_TOUCH_JOYPAD;
		static const Option DESTROY_VIBRATION;
		static const Option INFINITE_WORLDS;
		static const Option DPAD_SIZE;
		static const Option INPUT_MODE;
		static const Option BETA_VISUALS;
		static const Option SKIN_ARM_TYPE;
		static const Option SKIN_MENU;
		static const Option TOUCH_SNEAK;
		static const Option DEBUG_SCREEN;

		static const Option PIXELS_PER_MILLIMETER;
		static const Option FOV;

		/*
        static Option* getItem(int id) {
            for (Option item : Option.values()) {
                if (item.getId() == id) {
                    return item;
                }
            }
            return NULL;
        }
		*/

        Option(int ordinal, const std::string& captionId, bool hasProgress, bool isBoolean)
		:	_captionId(captionId),
			_isProgress(hasProgress),
			_isBoolean(isBoolean),
			_ordinal(ordinal)
		{}

        bool isProgress() const {
            return _isProgress;
        }

        bool isBoolean() const {
            return _isBoolean;
        }

		bool isInt() const {
			return (!_isBoolean && !_isProgress);
		}

        int getId() {
            return _ordinal;
        }

        std::string getCaptionId() const {
            return _captionId;
        }
    };

private:
	static const float SOUND_MIN_VALUE;
	static const float SOUND_MAX_VALUE;
	static const float MUSIC_MIN_VALUE;
	static const float MUSIC_MAX_VALUE;
	static const float SENSITIVITY_MIN_VALUE;
	static const float SENSITIVITY_MAX_VALUE;
	static const float PIXELS_PER_MILLIMETER_MIN_VALUE;
	static const float PIXELS_PER_MILLIMETER_MAX_VALUE;
	static const float FOV_MIN_VALUE;
	static const float FOV_MAX_VALUE;
	static const int DIFFICULY_LEVELS[];
public:
    static const char* RENDER_DISTANCE_NAMES[];
    static const char* DIFFICULTY_NAMES[];
    static const char* GUI_SCALE[];
	static const char* DPAD_SIZE[];
	static bool debugGl;

	float music;
    float sound;
    float sensitivity;
    bool invertYMouse;
    int viewDistance;
    bool bobView;
    bool anaglyph3d;
    bool limitFramerate;
    bool fancyGraphics;
    bool ambientOcclusion;
	bool useMouseForDigging;
	bool isLeftHanded;
	bool destroyVibration;
    //std::string skin;

    KeyMapping keyUp;
    KeyMapping keyLeft;
    KeyMapping keyDown;
    KeyMapping keyRight;
    KeyMapping keyJump;
    KeyMapping keyBuild;
    KeyMapping keyDrop;
    KeyMapping keyChat;
    KeyMapping keyFog;
    KeyMapping keySneak;
	KeyMapping keyCraft;
	KeyMapping keyDestroy;
	KeyMapping keyUse;

	KeyMapping keyMenuNext;
	KeyMapping keyMenuPrevious;
	KeyMapping keyMenuOk;
	KeyMapping keyMenuCancel;

    KeyMapping* keyMappings[16];

	/*protected*/ Minecraft* minecraft;
    ///*private*/ File optionsFile;

    int difficulty;
    bool hideGui;
    bool thirdPersonView;
    bool renderDebug;
	// Enables the F3 debug screen and its touch/pause-screen button.
	bool debugScreenEnabled;

    bool isFlying;
    bool smoothCamera;
    bool fixedCamera;
    float flySpeed;
    float cameraSpeed;
    int guiScale;
	int dpadSize;
	std::string username;

	bool serverVisible;
	bool isJoyTouchArea;
	bool useTouchScreen;
	bool infiniteWorlds;
	// Optional Java Beta-inspired presentation.  This is deliberately a
	// renderer-only preference: it never changes a world's data or generation.
	bool betaVisuals;
	// false = classic 4px arms, true = Java slim 3px arms.
	bool slimSkin;
	bool skinMenu;
	bool touchSneak;
	float pixelsPerMillimeter;
	float fieldOfView;
    Options(Minecraft* minecraft, const std::string& workingDirectory)
	:	minecraft(minecraft)
	{
        //optionsFile = /*new*/ File(workingDirectory, "options.txt");
        initDefaultValues();

		load();
    }

	Options()
	:	minecraft(NULL)
	{
		
	}

	void initDefaultValues();

    std::string getKeyDescription(int i) {
        //Language language = Language.getInstance();
        //return language.getElement(keyMappings[i].name);
		return "Options::getKeyDescription not implemented";
    }

    std::string getKeyMessage(int i) {
        //return Keyboard.getKeyName(keyMappings[i].key);
		return "Options::getKeyMessage not implemented";
    }

    void setKey(int i, int key) {
        keyMappings[i]->key = key;
        save();
    }

    void set(const Option* item, float value) {
        if (item == &Option::MUSIC) {
            music = value;
            //minecraft.soundEngine.updateOptions();
        } else if (item == &Option::SOUND) {
            sound = value;
            //minecraft.soundEngine.updateOptions();
        } else if (item == &Option::SENSITIVITY) {
            sensitivity = value;
		} else if (item == &Option::PIXELS_PER_MILLIMETER) {
			 pixelsPerMillimeter = value;
		} else if (item == &Option::FOV) {
			fieldOfView = value;
		}
		notifyOptionUpdate(item, value);
		save();
    }
	void set(const Option* item, int value) {
		if(item == &Option::DIFFICULTY) {
			difficulty = value;
			if (difficulty != DIFFICULY_LEVELS[0] && difficulty != DIFFICULY_LEVELS[1]) {
				difficulty = DIFFICULY_LEVELS[1];
			}
		} else if (item == &Option::RENDER_DISTANCE) {
			viewDistance = value & 7;
		} else if (item == &Option::GUI_SCALE) {
			guiScale = value;
			if (guiScale < 0) guiScale = 0;
			if (guiScale > 4) guiScale = 4;
		} else if (item == &Option::DPAD_SIZE) {
			dpadSize = value;
			if (dpadSize < 0) dpadSize = 0;
			if (dpadSize > 2) dpadSize = 2;
		} else if (item == &Option::INPUT_MODE) {
			useTouchScreen = value != 0;
		} else if (item == &Option::SKIN_ARM_TYPE) {
			slimSkin = value != 0;
		} else if (item == &Option::SKIN_MENU) {
			skinMenu = value != 0;
		} else if (item == &Option::GRAPHICS) {
			fancyGraphics = value != 0;
		}
		notifyOptionUpdate(item, value);
		save();
	}

    void toggle(const Option* option, int dir) {
        if (option == &Option::INVERT_MOUSE)	invertYMouse = !invertYMouse;
        if (option == &Option::RENDER_DISTANCE) viewDistance = (viewDistance + dir) & 7;
		if (option == &Option::GUI_SCALE) {
			guiScale = (guiScale + dir) % 5;
			if (guiScale < 0) guiScale += 5;
		}
		if (option == &Option::DPAD_SIZE) {
			dpadSize = (dpadSize + dir) % 3;
			if (dpadSize < 0) dpadSize += 3;
		}
		if (option == &Option::INPUT_MODE)
			useTouchScreen = !useTouchScreen;
		if (option == &Option::SKIN_ARM_TYPE) slimSkin = !slimSkin;
		if (option == &Option::SKIN_MENU) skinMenu = !skinMenu;
		if (option == &Option::TOUCH_SNEAK) touchSneak = !touchSneak;
		if (option == &Option::DEBUG_SCREEN) {
			debugScreenEnabled = !debugScreenEnabled;
			if (!debugScreenEnabled) renderDebug = false;
		}
        if (option == &Option::VIEW_BOBBING)	bobView = !bobView;
		if (option == &Option::THIRD_PERSON)	thirdPersonView = !thirdPersonView;
		if (option == &Option::HIDE_GUI)		hideGui = !hideGui;
		if (option == &Option::SERVER_VISIBLE)		serverVisible = !serverVisible;
		if (option == &Option::LEFT_HANDED) isLeftHanded = !isLeftHanded;
		if (option == &Option::USE_TOUCHSCREEN) useTouchScreen = !useTouchScreen;
		if (option == &Option::USE_TOUCH_JOYPAD) isJoyTouchArea = !isJoyTouchArea;
		if (option == &Option::DESTROY_VIBRATION) destroyVibration = !destroyVibration;
		if (option == &Option::INFINITE_WORLDS) infiniteWorlds = !infiniteWorlds;
		if (option == &Option::BETA_VISUALS) betaVisuals = !betaVisuals;
		if (option == &Option::ANAGLYPH) {
            anaglyph3d = !anaglyph3d;
            //minecraft->textures.reloadAll();
        }
        if (option == &Option::LIMIT_FRAMERATE) limitFramerate = !limitFramerate;
        if (option == &Option::DIFFICULTY) {
			const int difficultyLevelsNum = 2;
			int difficultyIndex = 0;
			for (int i = 0; i < difficultyLevelsNum; ++i) {
				if (DIFFICULY_LEVELS[i] == difficulty) {
					difficultyIndex = i;
					break;
				}
			}
			difficultyIndex += dir;
			while (difficultyIndex < 0) difficultyIndex += difficultyLevelsNum;
			difficulty = DIFFICULY_LEVELS[difficultyIndex % difficultyLevelsNum];
		}
        if (option == &Option::GRAPHICS) {
            fancyGraphics = !fancyGraphics;
            //minecraft->levelRenderer.allChanged();
        }
        if (option == &Option::AMBIENT_OCCLUSION) {
            ambientOcclusion = !ambientOcclusion;
            syncAmbientOcclusion();
        }
		if (option->isBoolean()) {
			notifyOptionUpdate(option, getBooleanValue(option));
		} else {
			notifyOptionUpdate(option, getIntValue(option));
		}
        save();
    }

	int getIntValue(const Option* item) {
		if(item == &Option::DIFFICULTY) return difficulty;
		if(item == &Option::RENDER_DISTANCE) return viewDistance;
		if(item == &Option::GUI_SCALE) return guiScale;
		if(item == &Option::DPAD_SIZE) return dpadSize;
		if(item == &Option::INPUT_MODE) return useTouchScreen ? 1 : 0;
		if(item == &Option::SKIN_ARM_TYPE) return slimSkin ? 1 : 0;
		if(item == &Option::GRAPHICS) return fancyGraphics ? 1 : 0;
		return 0;
	}

    float getProgressValue(const Option* item) {
        if (item == &Option::MUSIC) return music;
        if (item == &Option::SOUND) return sound;
        if (item == &Option::SENSITIVITY) return sensitivity;
		if (item == &Option::PIXELS_PER_MILLIMETER) return pixelsPerMillimeter;
		if (item == &Option::FOV) return fieldOfView;
        return 0;
    }

    bool getBooleanValue(const Option* item) {
        if (item == &Option::INVERT_MOUSE)
            return invertYMouse;
        if (item == &Option::VIEW_BOBBING)
            return bobView;
        if (item == &Option::ANAGLYPH)
            return anaglyph3d;
        if (item == &Option::LIMIT_FRAMERATE)
            return limitFramerate;
        if (item == &Option::AMBIENT_OCCLUSION)
            return ambientOcclusion;
        if (item == &Option::THIRD_PERSON)
            return thirdPersonView;
        if (item == &Option::HIDE_GUI)
            return hideGui;
		if (item == &Option::SERVER_VISIBLE)
			return serverVisible;
		if (item == &Option::LEFT_HANDED)
			return isLeftHanded;
		if (item == &Option::USE_TOUCHSCREEN)
			return useTouchScreen;
		if (item == &Option::USE_TOUCH_JOYPAD)
			return isJoyTouchArea;
		if (item == &Option::DESTROY_VIBRATION)
			return destroyVibration;
		if (item == &Option::INFINITE_WORLDS)
			return infiniteWorlds;
		if (item == &Option::BETA_VISUALS)
			return betaVisuals;
		if (item == &Option::SKIN_MENU)
			return skinMenu;
		if (item == &Option::TOUCH_SNEAK)
			return touchSneak;
		if (item == &Option::DEBUG_SCREEN)
			return debugScreenEnabled;
		return false;
	}

	float getProgrssMin(const Option* item) {
		if (item == &Option::MUSIC) return MUSIC_MIN_VALUE;
		if (item == &Option::SOUND) return SOUND_MIN_VALUE;
		if (item == &Option::SENSITIVITY) return SENSITIVITY_MIN_VALUE;
		if (item == &Option::PIXELS_PER_MILLIMETER) return PIXELS_PER_MILLIMETER_MIN_VALUE;
		if (item == &Option::FOV) return FOV_MIN_VALUE;
		return 0;
	}

	float getProgrssMax(const Option* item) {
		if (item == &Option::MUSIC) return MUSIC_MAX_VALUE;
		if (item == &Option::SOUND) return SOUND_MAX_VALUE;
		if (item == &Option::SENSITIVITY) return SENSITIVITY_MAX_VALUE;
		if (item == &Option::PIXELS_PER_MILLIMETER) return PIXELS_PER_MILLIMETER_MAX_VALUE;
		if (item == &Option::FOV) return FOV_MAX_VALUE;
		return 1.0f;
	}

	std::string getMessage(const Option* item);

	void setSettingsPath(const std::string& path);
	bool hasSavedOptions() const { return optionsFile.exists(); }
	void update();
    void load();
    void save();
	void syncAmbientOcclusion();
	void clearImportedSkin();
	void importSkinFromFile();
	void addOptionToSaveOutput(StringVector& stringVector, std::string name, bool boolValue);
	void addOptionToSaveOutput(StringVector& stringVector, std::string name, float floatValue);
	void addOptionToSaveOutput(StringVector& stringVector, std::string name, int intValue);
	void addOptionToSaveOutput(StringVector& stringVector, std::string name, const std::string& value);
	void notifyOptionUpdate(const Option* option, bool value);
	void notifyOptionUpdate(const Option* option, float value);
	void notifyOptionUpdate(const Option* option, int value);
private:
    static bool readFloat(const std::string& string, float& value);
    static bool readInt(const std::string& string, int& value);
	static bool readBool(const std::string& string, bool& value);

private:
	OptionsFile optionsFile;
	
};

#endif /*NET_MINECRAFT_CLIENT__Options_H__*/
