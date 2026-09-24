#include "TouchscreenInput.h"
#include "../../../Options.h"
#include "../../../../platform/input/Multitouch.h"
#include "../../../gui/Gui.h"
#include "../../../renderer/Tesselator.h"
#include "../../../../world/entity/player/Player.h"

#include "../../../Minecraft.h"
#include "../../../../platform/log.h"
#include "../../../../platform/input/TouchTrace.h"
#include "../../../renderer/Textures.h"
#include "../../../sound/SoundEngine.h"

static const int AREA_DPAD_FIRST = 100;
static const int AREA_DPAD_N = 100;
static const int AREA_DPAD_S = 101;
static const int AREA_DPAD_W = 102;
static const int AREA_DPAD_E = 103;
static const int AREA_DPAD_C = 104;
static const int AREA_DPAD_J = 105;
static const int AREA_PAUSE = 106;
static const int AREA_FLIGHT_UP = 107;
static const int AREA_FLIGHT_DOWN = 108;

static int cPressed = 0;
static int cReleased = 0;
static int cDiscreet = 0;
static int cPressedPause = 0;
static int cReleasedPause = 0;
//static const int AREA_DPAD_N_JUMP = 105;

//
// TouchscreenInput_TestFps
//

static void Copy(int n, float* x, float* y, float* dx, float* dy) {
	for (int i = 0; i < n; ++i) {
		dx[i] = x[i];
		dy[i] = y[i];
	}
}

static void Translate(int n, float* x, float* y, float xt, float yt) {
	for (int i = 0; i < n; ++i) {
		x[i] += xt;
		y[i] += yt;
	}
}

static void Scale(int n, float* x, float* y, float xt, float yt) {
	for (int i = 0; i < n; ++i) {
		x[i] *= xt;
		y[i] *= yt;
	}
}

static void Transformed(int n, float* x, float* y, float* dx, float* dy, float xt, float yt, float sx=1.0f, float sy=1.0f) {
	Copy(n, x, y, dx, dy);
	Scale(n, dx, dy, sx, sy);
	Translate(n, dx, dy, xt, yt);

	//for (int i = 0; i < n; ++i) {
	//	LOGI("%d. (%f, %f)\n", i, dx[i], dy[i]);
	//}
}

TouchscreenInput_TestFps::TouchscreenInput_TestFps( Minecraft* mc, Options* options )
:	_minecraft(mc),
	_options(options),
	_northJump(false),
	_forward(false),
	_boundingRectangle(0, 0, 1, 1),
	_pressedJump(false),
	_pauseIsDown(false),
	_sneakTapTime(-999),
	aLeft(0),
	aRight(0),
	aUp(0),
	aDown(0),
	aJump(0),
	aSneak(0),
	aJumpRight(0),
	aFlightUp(0),
	aFlightDown(0),
	aUpLeft(0),
	aUpRight(0),
	_allowHeightChange(false),
	_dpadPointerId(-1),
	_legacyFlightHeightMode(false),
	_legacyFlightPointer(-1)
{
	TOUCH_TRACE("touch-lifecycle: ctor this=%p options=%p\n", this, options);
	releaseAllKeys();
	onConfigChanged( createConfig(mc) );

	Tesselator& t = Tesselator::instance;
	const int alpha = 128;
	t.color( 0xc0c0c0, alpha); cPressed  = t.getColor();
	t.color( 0xffffff, alpha); cReleased = t.getColor();
	t.color( 0xffffff, alpha / 4); cDiscreet = t.getColor();
    t.color( 0xc0c0c0, 80); cPressedPause=t.getColor();
    t.color( 0xffffff, 80); cReleasedPause=t.getColor();
}

TouchscreenInput_TestFps::~TouchscreenInput_TestFps() {
	TOUCH_TRACE("touch-lifecycle: dtor this=%p\n", this);
	clear();
}

void TouchscreenInput_TestFps::clear() {
	TOUCH_TRACE("touch-lifecycle: clear-begin this=%p\n", this);
	_model.clear();
	// These areas are owned by TouchAreaModel.  Clear the non-owning pointers
	// immediately so a render that races a layout rebuild cannot use freed
	// storage.
	aLeft = aRight = aUp = aDown = aPause = NULL;
	aJump = aSneak = aJumpRight = NULL;

	delete aUpLeft; aUpLeft = NULL; // @todo: SAFEDEL
	delete aUpRight; aUpRight = NULL;
	// Flight buttons are owned by TouchAreaModel after addArea().  Do not
	// delete them here after _model.clear(), otherwise changing touch options
	// causes a double free during the next game initialization.
	aFlightUp = NULL;
	aFlightDown = NULL;
	TOUCH_TRACE("touch-lifecycle: clear-end this=%p\n", this);
}

bool TouchscreenInput_TestFps::isButtonDown(int areaId) {
	return _buttons[areaId - AREA_DPAD_FIRST];
}


void TouchscreenInput_TestFps::onConfigChanged(const Config& c) {
	TOUCH_TRACE("touch-lifecycle: config-begin this=%p size=(%d,%d)\n", this, c.width, c.height);
	clear();

	const float w = (float)c.width;
	const float h = (float)c.height;

	/*
	// Code for "Move when touching left side of the screen"
	float x0[] = {  0,  w * 0.3f,  w * 0.3f,     0 };
	float y0[] = {	0,	       0,      h-32,  h-32 };

	_model.addArea(AREA_MOVE, new RectangleArea(0, 0, w*0.3f, h-32));
	*/

	// Code for "D-pad with jump in center"
	float Bw = w * 0.11f;//0.08f;
	float Bh = Bw;//0.15f;

    // If too large (like playing on Tablet)
    PixelCalc& pc = _minecraft->pixelCalc;
    if (pc.pixelsToMillimeters(Bw) > 14) {
        Bw = Bh = pc.millimetersToPixels(14);
    }
	// Apply the selected size after the device-size cap.  Applying it before
	// the cap made all three choices collapse to the same 14mm control.
	if (_options->dpadSize == 1) {
		Bw *= 1.25f;
		Bh *= 1.25f;
	} else if (_options->dpadSize == 2) {
		Bw *= 1.50f;
		Bh *= 1.50f;
	}
	// temp data
	float xx;
	float yy;

	const float BaseY = -8 + h - 3.0f * Bh;
	const float BaseX = _options->isLeftHanded? -8 + w - 3 * Bw
											:	8 + 0;
	// Setup the bounding rectangle
	_boundingRectangle = RectangleArea(BaseX, BaseY, BaseX + 3 * Bw, BaseY + 3 * Bh);

	xx = BaseX + Bw; yy = BaseY;
	_model.addArea(AREA_DPAD_N, aUp = new RectangleArea(xx, yy, xx+Bw, yy+Bh));
	xx = BaseX;
	aUpLeft = new RectangleArea(xx, yy, xx+Bw, yy+Bh);
	xx = BaseX + 2 * Bw;
	aUpRight = new RectangleArea(xx, yy, xx+Bw, yy+Bh);

	xx = BaseX + Bw; yy = BaseY + Bh;
	_model.addArea(AREA_DPAD_C, aSneak = new RectangleArea(xx, yy, xx+Bw, yy+Bh));
	if (_options->touchSneak) {
		// Keep the jump/flight control clear of the edge so its full hit area is
		// visible and usable on narrow screens.
		const float jumpX = _options->isLeftHanded ? 8.0f : (w - 8.0f - 2.0f * Bw);
		_model.addArea(AREA_DPAD_J, aJumpRight = new RectangleArea(jumpX, BaseY + Bh, jumpX + Bw, BaseY + 2 * Bh));
		// The flight render path uses aJump as its generic jump surface.
		// Bind it to the valid right-side control in the new layout instead of
		// leaving it null or pointing at an area freed by a previous rebuild.
		aJump = aJumpRight;
		_model.addArea(AREA_FLIGHT_UP, aFlightUp = new RectangleArea(jumpX, BaseY,
			jumpX + Bw, BaseY + Bh));
		_model.addArea(AREA_FLIGHT_DOWN, aFlightDown = new RectangleArea(jumpX,
			BaseY + 2 * Bh, jumpX + Bw, BaseY + 3 * Bh));
	} else {
		aJump = aSneak;
		aJumpRight = aSneak;
	}

	xx = BaseX + Bw; yy = BaseY + 2 * Bh;
	_model.addArea(AREA_DPAD_S, aDown = new RectangleArea(xx, yy, xx+Bw, yy+Bh));

	xx = BaseX; yy = BaseY + Bh;
	_model.addArea(AREA_DPAD_W, aLeft = new RectangleArea(xx, yy, xx+Bw, yy+Bh));

	xx = BaseX + 2 * Bw; yy = BaseY + Bh;
	_model.addArea(AREA_DPAD_E, aRight = new RectangleArea(xx, yy, xx+Bw, yy+Bh));

	// The pause control follows the GUI Scale setting directly, independently
	// of the D-Pad size option.  Small is the original PE-sized button; larger
	// GUI settings progressively reduce the logical button size.
	float pauseLogicalSize = 16.0f; // Auto
	switch (_options->guiScale) {
		case 1: pauseLogicalSize = 18.0f; break; // Small: original size
		case 2: pauseLogicalSize = 16.0f; break; // Medium
		case 3: pauseLogicalSize = 14.0f; break; // Normal
		case 4: pauseLogicalSize = 12.0f; break; // Large
		default: break; // Auto
	}
	const float btnSize = pauseLogicalSize * Gui::GuiScale;
	_model.addArea(AREA_PAUSE, aPause = new RectangleArea(w - 4 - btnSize,
                                                          4,
                                                          w - 4,
	                                                          4 + btnSize));
	TOUCH_TRACE("touch-layout: screen=(%.0f,%.0f) sneak=%d dpad=(%.1f,%.1f,%.1f,%.1f) sneakArea=(%.1f,%.1f,%.1f,%.1f) jumpArea=(%.1f,%.1f,%.1f,%.1f) flightUp=(%.1f,%.1f,%.1f,%.1f) flightDown=(%.1f,%.1f,%.1f,%.1f)\n",
		w, h, _options->touchSneak ? 1 : 0,
		_boundingRectangle._x0, _boundingRectangle._y0, _boundingRectangle._x1, _boundingRectangle._y1,
		aSneak->_x0, aSneak->_y0, aSneak->_x1, aSneak->_y1,
		aJumpRight ? aJumpRight->_x0 : -1, aJumpRight ? aJumpRight->_y0 : -1,
		aJumpRight ? aJumpRight->_x1 : -1, aJumpRight ? aJumpRight->_y1 : -1,
		aFlightUp ? aFlightUp->_x0 : -1, aFlightUp ? aFlightUp->_y0 : -1,
		aFlightUp ? aFlightUp->_x1 : -1, aFlightUp ? aFlightUp->_y1 : -1,
		aFlightDown ? aFlightDown->_x0 : -1, aFlightDown ? aFlightDown->_y0 : -1,
		aFlightDown ? aFlightDown->_x1 : -1, aFlightDown ? aFlightDown->_y1 : -1);
	TOUCH_TRACE("touch-lifecycle: config-end this=%p\n", this);

	//rebuild();
}

void TouchscreenInput_TestFps::setKey( int key, bool state )
{
	#ifdef WIN32
        //LOGI("key: %d, %d\n", key, state);

		int id = -1;
		if (key == _options->keyUp.key) id = KEY_UP;
		if (key == _options->keyDown.key) id = KEY_DOWN;
		if (key == _options->keyLeft.key) id = KEY_LEFT;
		if (key == _options->keyRight.key) id = KEY_RIGHT;
		if (key == _options->keyJump.key) id = KEY_JUMP;
		if (key == _options->keySneak.key) id = KEY_SNEAK;
		if (key == _options->keyCraft.key) id = KEY_CRAFT;
		if (id >= 0) {
			_keys[id] = state;
		}
	#endif
}

void TouchscreenInput_TestFps::releaseAllKeys()
{
	TOUCH_TRACE("touch-lifecycle: release-all this=%p\n", this);
	xa = 0;
	ya = 0;

	for (int i = 0; i<10; ++i)
		_buttons[i] = false;
#ifdef WIN32
	for (int i = 0; i<NumKeys; ++i)
		_keys[i] = false;
#endif
	_pressedJump = false;
	_allowHeightChange = false;
	_legacyFlightHeightMode = false;
	_legacyFlightPointer = -1;
}

void TouchscreenInput_TestFps::tick( Player* player )
{
	xa = 0;
	ya = 0;
	jumping = false;
	// These are derived from the current frame's contacts only.  Explicitly
	// clear them before processing pointers so a world exit/re-entry cannot
	// inherit a stale flight direction.
	wantUp = false;
	wantDown = false;

	//bool gotEvent = false;
	bool heldJump = false;
	bool tmpForward = false;
	bool tmpNorthJump = false;

	// Clear every touch-control slot, including the right-side flight buttons
	// (indices 7 and 8).  Leaving those slots set makes ascent/descent persist
	// after the finger is released and can destabilize the next world entry.
	for (int i = 0; i < 10; ++i)
		_buttons[i] = false;

	const int* pointerIds;
	int pointerCount = Multitouch::getActivePointerIdsThisUpdate(&pointerIds);

	// Lock movement to the finger which started on the DPad.  The active list
	// is ordered by pointer slot, not by touch-down time, so without this a
	// second camera finger can change which contact drives movement.
	if (_dpadPointerId >= 0 && !Multitouch::isPointerDown(_dpadPointerId))
		_dpadPointerId = -1;
	if (_dpadPointerId < 0) {
		for (int i = 0; i < pointerCount; ++i) {
			int p = pointerIds[i];
			int areaId = _model.getPointerId(Multitouch::getX(p), Multitouch::getY(p), p);
			if (Multitouch::isPressed(p) && areaId >= AREA_DPAD_N && areaId <= AREA_FLIGHT_DOWN) {
				_dpadPointerId = p;
				break;
			}
		}
	}
	if (_legacyFlightPointer >= 0 && !Multitouch::isPointerDown(_legacyFlightPointer)) {
		_legacyFlightPointer = -1;
		_legacyFlightHeightMode = false;
	}

	for (int i = 0; i < pointerCount; ++i) {
		int p = pointerIds[i];
		int x = Multitouch::getX(p);
		int y = Multitouch::getY(p);

		if (_boundingRectangle.isInside((float)x, (float)y) && _forward && !isChangingFlightHeight)
		{
			float angle = Mth::PI + Mth::atan2(y - _boundingRectangle.centerY(), x - _boundingRectangle.centerX());
			ya = Mth::sin(angle);
			xa = Mth::cos(angle);
			tmpForward = true;
		}

		int areaId = _model.getPointerId(x, y, p);
		if (Multitouch::isPressed(p) || Multitouch::isReleased(p))
			TOUCH_TRACE("touch-area: pointer=%d event=%s pos=(%d,%d) area=%d flying=%d\n",
				p, Multitouch::isPressed(p) ? "down" : "up", x, y, areaId,
				player->abilities.flying ? 1 : 0);
		if (areaId < AREA_DPAD_FIRST)
		{
			continue;
		}
		// Flight uses the right-side jump key together with the D-Pad's
		// vertical directions, so allow multiple D-Pad contacts in that mode.
		if (areaId != AREA_PAUSE && p != _dpadPointerId &&
			!player->abilities.flying)
			continue;

		bool setButton = false;

		if (Multitouch::isPressed(p))
			_allowHeightChange = _options->touchSneak ? (areaId == AREA_DPAD_J) : (areaId == AREA_DPAD_C);

		if (areaId == AREA_DPAD_C)
		{
			setButton = true;
			if (_options->touchSneak) {
				if (Multitouch::isPressed(p)) {
					// Match the jump button's double-tap behavior: a single tap
					// arms the action, the second tap toggles sneaking.
					float now = getTimeS();
					if (now - _sneakTapTime < 0.4f) {
						sneaking = !sneaking;
						player->setSneaking(sneaking);
						_sneakTapTime = -1;
					} else {
						_sneakTapTime = now;
					}
				}
			} else {
				heldJump = true;
				if (player->abilities.flying && !_options->touchSneak && Multitouch::isPressed(p)) {
					_legacyFlightHeightMode = true;
					_legacyFlightPointer = p;
				}
				// If we're in water or pressed down on the button: jump
				if (player->isInWater()) {
					jumping = true;
				}
				else if (Multitouch::isPressed(p)) {
					jumping = true;
				}
			}
		}
		if (areaId == AREA_DPAD_J) {
			setButton = true;
			heldJump = true;
			if (player->isInWater() || Multitouch::isPressed(p)) jumping = true;
			else if (!isChangingFlightHeight) jumping = true;
		}
		if (areaId == AREA_FLIGHT_UP || areaId == AREA_FLIGHT_DOWN) {
			setButton = true;
			if (!player->abilities.flying) setButton = false;
		}

		if	(areaId == AREA_DPAD_N)
		{
			setButton = true;
			// Legacy flight keeps forward/back movement on the normal D-pad.
			// Holding the jump button acts as the height modifier.
			if (player->abilities.flying && !_options->touchSneak && _legacyFlightHeightMode)
				ya += 1;
			else {
				if (!isChangingFlightHeight) tmpForward = true;
				ya += 1;
			}
		}
		else if (areaId == AREA_DPAD_S && !_forward)
		{
			setButton = true;
			ya -= 1;
			/*
            if (Multitouch::isReleased(p)) {
                float now = getTimeS();
                if (now - _sneakTapTime < 0.4f) {
                    ya += 1;
                    sneaking = !sneaking;
                    player->setSneaking(sneaking);
                    _sneakTapTime = -1;
                } else {
                    _sneakTapTime = now;
                }
            }
			*/
        }
		else if (areaId == AREA_DPAD_W && !_forward)
		{
			setButton = true;
			xa += 1;
		}
		else if (areaId == AREA_DPAD_E && !_forward)
		{
			setButton = true;
			xa -= 1;
		}
		else if (areaId == AREA_FLIGHT_UP) {
			ya += 1;
		}
		else if (areaId == AREA_FLIGHT_DOWN) {
			ya -= 1;
		}
		else if (areaId == AREA_PAUSE) {
			if (Multitouch::isReleased(p)) {
                _minecraft->soundEngine->playUI("random.click", 1, 1);
				_minecraft->screenChooser.setScreen(SCREEN_PAUSE);
            }
		}
		_buttons[areaId - AREA_DPAD_FIRST] = setButton;
	}

	_forward = tmpForward;
	if (player->abilities.flying && !_options->touchSneak && _legacyFlightHeightMode)
		_forward = false;

	// Only jump once at a time
	if (tmpNorthJump) {
		if (!_northJump)
			jumping = true;
		_northJump = true;
	}
	else _northJump = false;

	isChangingFlightHeight = false;
	// In creative flight, hold the jump button as a modifier and use the
	// vertical D-pad directions for ascent/descent.  Do not treat the same
	// contacts as ordinary forward/back movement while changing height.
	if (player->abilities.flying && _options->touchSneak) {
		// The jump key is the flight modifier.  Accept both a held contact and
		// the immediately preceding tap so a second finger can select a height.
		const bool flightModifier = heldJump || _pressedJump;
		wantUp = flightModifier && isButtonDown(AREA_DPAD_N);
		wantDown = flightModifier && isButtonDown(AREA_DPAD_S);
	} else {
		// Legacy center-jump flight mode: only the current/previous jump
		// contact may enable vertical movement.  Do not feed wantUp/wantDown
		// back into their own calculation, otherwise the state sticks forever.
		const bool jumpModifier = heldJump || _pressedJump;
		if (player->abilities.flying && !_options->touchSneak) {
			wantUp = _legacyFlightHeightMode && isButtonDown(AREA_DPAD_N);
			wantDown = _legacyFlightHeightMode && isButtonDown(AREA_DPAD_S);
		} else {
			wantUp   = isButtonDown(AREA_DPAD_N) && _allowHeightChange && jumpModifier;
			wantDown = isButtonDown(AREA_DPAD_S) && _allowHeightChange && jumpModifier;
		}
	}
	// Keep the flight controls functional even when the jump modifier is not
	// held: the D-pad's up/down buttons are explicit ascent/descent controls.
	if (player->abilities.flying && _options->touchSneak) {
		wantUp = isButtonDown(AREA_FLIGHT_UP);
		wantDown = isButtonDown(AREA_FLIGHT_DOWN);
	}
	if (player->abilities.flying && (wantUp || wantDown || (heldJump && !_forward)))
	{
		isChangingFlightHeight = true;
		ya = 0;
	}
	_renderFlightImage = player->abilities.flying;

#ifdef WIN32
	if (_keys[KEY_UP]) ya++;
	if (_keys[KEY_DOWN]) ya--;
	if (_keys[KEY_LEFT]) xa++;
	if (_keys[KEY_RIGHT]) xa--;
	if (_keys[KEY_JUMP]) jumping = true;
	//sneaking = _keys[KEY_SNEAK];
	if (_keys[KEY_CRAFT])
		player->startCrafting((int)player->x, (int)player->y, (int)player->z, Recipe::SIZE_2X2);
#endif

	if (sneaking) {
		// Keep touch sneak movement in line with the keyboard Shift speed.
		xa *= 0.2f;
		ya *= 0.2f;
	}
	static int lastFlying = -1, lastForward = -1, lastJump = -1, lastSneak = -1;
	static int lastUp = -1, lastDown = -1, lastHeight = -1;
	const int nowFlying = player->abilities.flying ? 1 : 0;
	const int nowForward = _forward ? 1 : 0;
	const int nowJump = jumping ? 1 : 0;
	const int nowSneak = sneaking ? 1 : 0;
	const int nowUp = wantUp ? 1 : 0;
	const int nowDown = wantDown ? 1 : 0;
	const int nowHeight = isChangingFlightHeight ? 1 : 0;
	if (nowFlying != lastFlying || nowForward != lastForward || nowJump != lastJump ||
		nowSneak != lastSneak || nowUp != lastUp || nowDown != lastDown || nowHeight != lastHeight) {
		TOUCH_TRACE("touch-input: flying=%d forward=%d jump=%d sneak=%d up=%d down=%d xa=%.2f ya=%.2f height=%d\n",
			nowFlying, nowForward, nowJump, nowSneak, nowUp, nowDown, xa, ya, nowHeight);
		lastFlying = nowFlying; lastForward = nowForward; lastJump = nowJump;
		lastSneak = nowSneak; lastUp = nowUp; lastDown = nowDown; lastHeight = nowHeight;
	}
	//printf("\n>- %f %f\n", xa, ya);
	_pressedJump = heldJump;
}

static void drawRectangleArea(Tesselator& t, RectangleArea* a, int ux, int vy, float ssz = 64.0f) {
	const float pm = 1.0f / 256.0f;
	const float sz = ssz * pm;
	const float uu = (float)(ux) * pm;
	const float vv = (float)(vy) * pm;
	const float x0 = a->_x0 * Gui::InvGuiScale;
	const float x1 = a->_x1 * Gui::InvGuiScale;
	const float y0 = a->_y0 * Gui::InvGuiScale;
	const float y1 = a->_y1 * Gui::InvGuiScale;

	t.vertexUV(x0, y1, 0, uu,	vv+sz);
	t.vertexUV(x1, y1, 0, uu+sz,vv+sz);
	t.vertexUV(x1, y0, 0, uu+sz,vv);
	t.vertexUV(x0, y0, 0, uu,	vv);
}

static void drawRectangleAreaScaled(Tesselator& t, RectangleArea* a, int ux, int vy,
	float sourceSize, float scale) {
	const float cx = (a->_x0 + a->_x1) * 0.5f;
	const float cy = (a->_y0 + a->_y1) * 0.5f;
	const float hw = (a->_x1 - a->_x0) * scale * 0.5f;
	const float hh = (a->_y1 - a->_y0) * scale * 0.5f;
	RectangleArea visual(cx - hw, cy - hh, cx + hw, cy + hh);
	drawRectangleArea(t, &visual, ux, vy, sourceSize);
}

static void drawPolygonArea(Tesselator& t, PolygonArea* a, int x, int y) {
	float pm = 1.0f / 256.0f;
	float sz = 64.0f * pm;
	float uu = (float)(x) * pm;
	float vv = (float)(y) * pm;

	float uvs[] = {uu, vv, uu+sz, vv, uu+sz, vv+sz, uu, vv+sz};
	const int o = 0;

	for (int j = 0; j < a->_numPoints; ++j) {
		t.vertexUV(a->_x[j] * Gui::InvGuiScale, a->_y[j] * Gui::InvGuiScale, 0, uvs[(o+j+j)&7], uvs[(o+j+j+1)&7]);
	}
}

void TouchscreenInput_TestFps::render( float a ) {
	// During the very first window bootstrap frame the viewport may still be
	// 1x1.  Avoid dereferencing touch areas until layout construction finishes.
	if (!aLeft || !aRight || !aUp || !aDown || !aSneak || !aPause ||
		(_options->touchSneak && (!aJumpRight || (_renderFlightImage && (!aFlightUp || !aFlightDown)))))
		return;
	//return;

	//static Stopwatch sw;
	//sw.start();


	//glColor4f2(1, 0, 1, 1.0f);
	//glDisable2(GL_CULL_FACE);
	glDisable2(GL_ALPHA_TEST);

	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	_minecraft->textures->loadAndBindTexture("gui/gui.png");
	
	//glDisable2(GL_TEXTURE_2D);

	rebuild();
	//drawArrayVTC(_bufferId, 5 * 2 * 3, 24);

	glDisable2(GL_BLEND);
	//glEnable2(GL_TEXTURE_2D);
	//glEnable2(GL_CULL_FACE);

	//sw.stop();
	//sw.printEvery(100, "buttons");
}

const RectangleArea& TouchscreenInput_TestFps::getRectangleArea()
{
	return _boundingRectangle;
}
const RectangleArea& TouchscreenInput_TestFps::getPauseRectangleArea()
{
    return *aPause;
}

void TouchscreenInput_TestFps::rebuild() {
    if (_options->hideGui)
        return;
    
	Tesselator& t = Tesselator::instance;
	//LOGI("instance is: %p, %p, %p, %p, %p FOR %d\n", &t, aLeft, aRight, aUp, aDown, aJump, _bufferId);
	//t.setAccessMode(Tesselator::ACCESS_DYNAMIC);
	t.begin();

	const int imageU = 0;
	const int imageV = 107;
	const int imageSize = 26;

	bool northDiagonals = !isChangingFlightHeight && (_northJump || _forward);

	// render left button
	if (northDiagonals || isChangingFlightHeight) t.colorABGR(cDiscreet);
    else if (isButtonDown(AREA_DPAD_W)) t.colorABGR(cPressed);
	else						   t.colorABGR(cReleased);
	drawRectangleArea(t, aLeft, imageU + imageSize, imageV, (float)imageSize);

	// render right button
	if (northDiagonals || isChangingFlightHeight) t.colorABGR(cDiscreet);
	else if (isButtonDown(AREA_DPAD_E)) t.colorABGR(cPressed);
	else						   t.colorABGR(cReleased);
	drawRectangleArea(t, aRight, imageU + imageSize * 3, imageV, (float)imageSize);

	// render forward button
	if (isButtonDown(AREA_DPAD_N)) t.colorABGR(cPressed);
	else						   t.colorABGR(cReleased);
	if (isChangingFlightHeight && !_options->touchSneak)
		drawRectangleArea(t, aUp, imageU + imageSize * 2, imageV + imageSize, (float)imageSize);
	else
		drawRectangleArea(t, aUp, imageU, imageV, (float)imageSize);
	
	// render diagonals, if available
	if (northDiagonals)
	{
		t.colorABGR(cReleased);
		drawRectangleArea(t, aUpLeft, imageU, imageV + imageSize, (float)imageSize);
		drawRectangleArea(t, aUpRight, imageU + imageSize, imageV + imageSize, (float)imageSize);
	}

	// render backwards button
	if (northDiagonals) t.colorABGR(cDiscreet);
	else if (isButtonDown(AREA_DPAD_S)) t.colorABGR(cPressed);
	else						   t.colorABGR(cReleased);
	if (isChangingFlightHeight && !_options->touchSneak)
		drawRectangleArea(t, aDown, imageU + imageSize * 3, imageV + imageSize, (float)imageSize);
	else
		drawRectangleArea(t, aDown, imageU + imageSize * 2, imageV, (float)imageSize);

	// render jump / flight button
		if (_options->touchSneak) t.colorABGR(cReleased);
		else if (_renderFlightImage && northDiagonals) t.colorABGR(cDiscreet);
	else if (isButtonDown(AREA_DPAD_C)) t.colorABGR(cPressed);
	else						   t.colorABGR(cReleased);
	if (_options->touchSneak && !_renderFlightImage)
		// The sneak artwork occupies its own two 18x18 cells in gui.png.
		drawRectangleAreaScaled(t, aSneak, imageU + imageSize * 5,
			isButtonDown(AREA_DPAD_C) ? imageV + 18 : imageV,
			18.0f, 16.0f / 18.0f);
	else if (_renderFlightImage && !_options->touchSneak)
	{
		drawRectangleArea(t, aJump, imageU + imageSize * 4, imageV + imageSize, 26.0f);
	}
	if (_options->touchSneak && _renderFlightImage) {
		t.colorABGR(isButtonDown(AREA_FLIGHT_UP) ? cPressed : cReleased);
		drawRectangleArea(t, aFlightUp, imageU, imageV, 26.0f);
		t.colorABGR(isButtonDown(AREA_FLIGHT_DOWN) ? cPressed : cReleased);
		drawRectangleArea(t, aFlightDown, imageU + imageSize * 2, imageV, 26.0f);
	}
	if (_options->touchSneak) {
		if (isButtonDown(AREA_DPAD_J)) t.colorABGR(cPressed);
		else t.colorABGR(cReleased);
		drawRectangleArea(t, aJumpRight, imageU + imageSize * 4,
			isButtonDown(AREA_DPAD_J) ? imageV + imageSize : imageV, 26.0f);
	}
	else if (!_renderFlightImage)
	{
		drawRectangleArea(t, aJump, imageU + imageSize * 4, imageV, 26.0f);
	}
	

	if (!_minecraft->screen) {
		if (isButtonDown(AREA_PAUSE))  t.colorABGR(cPressedPause);
		else						   t.colorABGR(cReleasedPause);
		
        drawRectangleArea(t, aPause, 200, 64, 18.0f);
	}
//t.end(true, _bufferId);
	//return;

	t.draw();
	//RenderChunk _render = t.end(true, _bufferId);
	//t.setAccessMode(Tesselator::ACCESS_STATIC);
	//_bufferId = _render.vboId;
}
