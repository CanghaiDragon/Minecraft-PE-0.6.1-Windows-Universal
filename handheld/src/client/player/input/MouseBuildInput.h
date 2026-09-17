#ifndef NET_MINECRAFT_CLIENT_PLAYER_INPUT_MouseBuildInput_H__
#define NET_MINECRAFT_CLIENT_PLAYER_INPUT_MouseBuildInput_H__

#include "IBuildInput.h"
#include "../../../platform/input/Mouse.h"
#include "../../../platform/input/TouchTrace.h"

/** A Mouse Build input */
class MouseBuildInput : public IBuildInput {
public:
	MouseBuildInput()
	:	buildDelayTicks(10),
		buildHoldTicks(0),
		_prevLeftDown(false)
	{}

	virtual bool tickBuild(Player* p, BuildActionIntention* bai) {
		bool leftDown = Mouse::getButtonState(MouseAction::ACTION_LEFT) != 0;
		bool firstPress = leftDown && !_prevLeftDown;
		_prevLeftDown = leftDown;

		if (Mouse::getButtonState(MouseAction::ACTION_RIGHT) != 0) {
			// Prefer placing whenever the right button is held.  This also makes
			// a stray left-down message unable to turn a placement into a dig.
			if (buildHoldTicks >= buildDelayTicks) buildHoldTicks = 0;
			if (++buildHoldTicks == 1) {
				TOUCH_TRACE("[BUILD] right intent: left=%d right=%d\\n", leftDown, Mouse::isButtonDown(MouseAction::ACTION_RIGHT));
				*bai = BuildActionIntention(BuildActionIntention::BAI_BUILD | BuildActionIntention::BAI_INTERACT);
				return true;
			}
		} else {
			buildHoldTicks = 0;
		}
		if (leftDown) {
			// On first click: trigger attack/first-remove so handleBuildAction is called
			// On hold: continuous remove for block breaking (handleMouseDown handles it)
			if (firstPress)
				TOUCH_TRACE("[BUILD] left first-remove intent: left=%d right=%d\\n", leftDown, Mouse::isButtonDown(MouseAction::ACTION_RIGHT));
			if (firstPress)
				*bai = BuildActionIntention(BuildActionIntention::BAI_FIRSTREMOVE | BuildActionIntention::BAI_ATTACK);
			else
				*bai = BuildActionIntention(BuildActionIntention::BAI_REMOVE);
			return true;
		}
		return false;
	}
private:
	int buildHoldTicks;
	int buildDelayTicks;
	bool _prevLeftDown;
};

#endif /*NET_MINECRAFT_CLIENT_PLAYER_INPUT_MouseBuildInput_H__*/
