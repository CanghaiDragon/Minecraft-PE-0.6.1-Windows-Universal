#include "MouseHandler.h"
#include "player/input/ITurnInput.h"

#ifdef RPI
#include <SDL/SDL.h>
#endif
#if defined(MACOS) || defined(LINUX)
#include <SDL.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
extern bool g_win32MouseCaptured;
extern HWND g_win32Hwnd;
extern void recenterWin32MouseCursor();
extern void resetWin32MouseTraceBudget();
#include "../platform/input/TouchTrace.h"
#endif

MouseHandler::MouseHandler( ITurnInput* turnInput )
:	_turnInput(turnInput)
{}

MouseHandler::MouseHandler()
:	_turnInput(0)
{}

MouseHandler::~MouseHandler() {
}

void MouseHandler::setTurnInput( ITurnInput* turnInput ) {
	_turnInput = turnInput;
}

void MouseHandler::grab() {
	xd = 0;
	yd = 0;

#if defined(RPI)
	//LOGI("Grabbing input!\n");
	SDL_WM_GrabInput(SDL_GRAB_ON);
	SDL_ShowCursor(0);
#elif defined(MACOS) || defined(LINUX)
	SDL_SetRelativeMouseMode(SDL_TRUE);
	SDL_ShowCursor(0);
#elif defined(_WIN32)
	g_win32MouseCaptured = true;
	resetWin32MouseTraceBudget();
	TOUCH_TRACE("[MOUSE] capture begin hwnd=%p\\n", g_win32Hwnd);
	ShowCursor(FALSE);
	if (g_win32Hwnd) {
		RECT r;
		GetClientRect(g_win32Hwnd, &r);
		// Lock to a one-pixel rectangle at the crosshair.  Repeatedly warping
		// the pointer back to the centre generates synthetic RAWINPUT on some
		// drivers, which feeds back into camera movement after loading a world.
		POINT p;
		p.x = (r.right - r.left) / 2;
		p.y = (r.bottom - r.top) / 2;
		ClientToScreen(g_win32Hwnd, &p);
		RECT lock = { p.x, p.y, p.x + 1, p.y + 1 };
		ClipCursor(&lock);
		recenterWin32MouseCursor();
	}
#endif
}

void MouseHandler::release() {
#if defined(RPI)
	//LOGI("Releasing input!\n");
	SDL_WM_GrabInput(SDL_GRAB_OFF);
	SDL_ShowCursor(1);
#elif defined(MACOS) || defined(LINUX)
	SDL_SetRelativeMouseMode(SDL_FALSE);
	SDL_ShowCursor(1);
#elif defined(_WIN32)
	TOUCH_TRACE("[MOUSE] capture release\\n");
	g_win32MouseCaptured = false;
	ShowCursor(TRUE);
	ClipCursor(NULL);
#endif
}

void MouseHandler::poll() {
	if (_turnInput != 0) {
		TurnDelta td = _turnInput->getTurnDelta();
		xd = td.x;
		yd = td.y;
	}
}
