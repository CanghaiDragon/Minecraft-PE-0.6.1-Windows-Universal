#ifndef MAIN_WIN32_H__
#define MAIN_WIN32_H__

/*
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
*/

#include "client/renderer/gles.h"
#if !defined(WIN32_WGL)
#include <EGL/egl.h>
#else
#include "platform/WglContext_win32.h"
#include "platform/ExitTrace.h"
#endif
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <windowsx.h>

#include <WinSock2.h>
#include <process.h>

#include <cstdio>
#include <cstring>
#include "platform/input/Mouse.h"
#include "platform/input/Multitouch.h"
#include "platform/input/TouchTrace.h"
#include "util/Mth.h"
#include "AppPlatform_win32.h"

static App* g_app = 0;
static volatile bool g_running = true;
bool g_win32MouseCaptured = false;  // shared with MouseHandler.cpp
HWND g_win32Hwnd = NULL;             // shared with MouseHandler.cpp
// The selected control mode.  WndProc owns the boundary so mouse promotion
// and real touch can never leak into the other input system.
bool g_win32TouchInputEnabled = true;
// Menus are intentionally mode-agnostic so a user can always change modes
// again with either a mouse or a finger.
bool g_win32GuiInputEnabled = true;
static int g_win32MouseMoveTraceBudget = 0;

// Write a small crash report beside the executable.  This is intentionally
// self-contained and uses only Windows APIs so it also works on ARM64 test
// devices without a debugger or extra runtime DLLs.
static LONG WINAPI mcpeWin32CrashHandler(EXCEPTION_POINTERS* info) {
	FILE* f = std::fopen("MinecraftWin32-crash.log", "ab");
	if (f) {
		std::fprintf(f, "\n--- unhandled exception ---\n");
		if (info && info->ExceptionRecord) {
			std::fprintf(f, "code=0x%08lX address=%p flags=%lu\n",
				(unsigned long)info->ExceptionRecord->ExceptionCode,
				info->ExceptionRecord->ExceptionAddress,
				(unsigned long)info->ExceptionRecord->ExceptionFlags);
		}
		void* frames[32] = {};
		USHORT count = CaptureStackBackTrace(0, 32, frames, NULL);
		for (USHORT i = 0; i < count; ++i)
			std::fprintf(f, "frame[%u]=%p\n", (unsigned)i, frames[i]);
		std::fflush(f);
		std::fclose(f);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

// True while one or more real touchscreen contacts are active.
// Used to prevent Windows-promoted mouse messages from contaminating Multitouch.
static bool g_win32TouchActive = false;
static int g_win32TouchCount = 0;
static DWORD g_win32TouchIds[Multitouch::MAX_POINTERS] = {0};
static int g_win32PrimaryTouch = -1;
// Some Windows touch keyboards emit a key-down reliably but intermittently
// omit the following WM_CHAR.  Keep the pending numeric character so the
// key-down can be used as a fallback without duplicating a normal WM_CHAR.
static char g_win32PendingNumericChar = 0;

// The legacy x86 libEGL/libGLES_CM pair is Imagination's PowerVR PVRVFrame
// emulator.  Its optional control panel is useful for SDK development but is
// not part of the game and steals focus at every launch.  Resolve the private
// control function dynamically so a future EGL backend (ANGLE, Mesa, UWP,
// etc.) has no compile- or link-time dependency on PVRVFrame.
#if !defined(WIN32_WGL)
static void disableLegacyPvrVFrameControlWindow() {
	HMODULE eglModule = GetModuleHandleA("libEGL.dll");
	if (!eglModule) return;

	typedef void (__stdcall *PvrVFrameEnableControlWindowFn)(bool);
	PvrVFrameEnableControlWindowFn setEnabled =
		reinterpret_cast<PvrVFrameEnableControlWindowFn>(
			GetProcAddress(eglModule, "PVRVFrameEnableControlWindow"));
	if (setEnabled) setEnabled(false);
}
#endif

// Windows can promote a touch contact to WM_LBUTTON* messages.  Filtering
// only by g_win32TouchActive is racy because the promoted message can arrive
// before/after WM_TOUCH.  The documented signature identifies it directly.
static bool isPromotedWin32TouchMouseMessage() {
	const ULONG_PTR signature = 0xFF515700;
	const ULONG_PTR signatureMask = 0xFFFFFF00;
	return (GetMessageExtraInfo() & signatureMask) == signature;
}

static char numericCharFromWin32Key(WPARAM wParam) {
	if (wParam >= '0' && wParam <= '9') return (char)wParam;
	if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9)
		return (char)('0' + wParam - VK_NUMPAD0);
	if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT) return '-';
	return 0;
}

// WM_TOUCH IDs are device-defined 32-bit values, not compact pointer indexes.
// Keep a stable mapping for the lifetime of each contact instead of using
// dwID % MAX_POINTERS (which aliases unrelated fingers on many touch panels).
static int findWin32TouchSlot(DWORD touchId) {
	for (int i = 0; i < Multitouch::MAX_POINTERS; ++i)
		if (g_win32TouchIds[i] == touchId)
			return i;
	return -1;
}

static int allocateWin32TouchSlot(DWORD touchId) {
	int slot = findWin32TouchSlot(touchId);
	if (slot >= 0) return slot;
	for (int i = 0; i < Multitouch::MAX_POINTERS; ++i) {
		if (g_win32TouchIds[i] == 0) {
			g_win32TouchIds[i] = touchId;
			return i;
		}
	}
	return -1; // MCPE itself only supports 12 simultaneous contacts.
}

static void releaseWin32Touches() {
	for (int i = 0; i < Multitouch::MAX_POINTERS; ++i) {
		if (g_win32TouchIds[i] != 0) {
			Multitouch::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
			                 Multitouch::getX(i), Multitouch::getY(i), (char)i);
			g_win32TouchIds[i] = 0;
		}
	}
	if (g_win32PrimaryTouch >= 0)
		Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
		            Mouse::getX(), Mouse::getY());
	g_win32PrimaryTouch = -1;
	g_win32TouchCount = 0;
	g_win32TouchActive = false;
}

// Called from Minecraft when the Controls screen changes Input Mode.
void setWin32TouchInputEnabled(bool enabled) {
	if (g_win32TouchInputEnabled == enabled) return;
	releaseWin32Touches();
	g_win32TouchInputEnabled = enabled;
}

void setWin32GuiInputEnabled(bool enabled) {
	g_win32GuiInputEnabled = enabled;
}

// Raw input supplies relative look deltas.  Re-centering prevents the system
// cursor drifting to an edge while the player turns, like Java Edition.
void recenterWin32MouseCursor() {
	if (!g_win32Hwnd || !g_win32MouseCaptured) return;
	RECT r;
	GetClientRect(g_win32Hwnd, &r);
	POINT p;
	p.x = (r.right - r.left) / 2;
	p.y = (r.bottom - r.top) / 2;
	ClientToScreen(g_win32Hwnd, &p);
	SetCursorPos(p.x, p.y);
}

void resetWin32MouseTraceBudget() {
	g_win32MouseMoveTraceBudget = 80;
}

static int getBits(int bits, int startBitInclusive, int endBitExclusive, int shiftTruncate) {
	int sum = 0;
	for (int i = startBitInclusive; i<endBitExclusive; ++i)
		sum += (bits & (2<<i));
	return shiftTruncate? (sum >> startBitInclusive) : sum;
}

// Map Win32 virtual keys to the game's internal key codes.
static unsigned char transformKey_win32(WPARAM wParam) {
	if (wParam == VK_LSHIFT || wParam == VK_SHIFT) return Keyboard::KEY_LSHIFT;
	if (wParam == VK_LCONTROL || wParam == VK_RCONTROL || wParam == VK_CONTROL) return Keyboard::KEY_LEFT_CTRL;
	if (wParam == VK_TAB) return 250;  // internal Tab code (same as macOS/Linux)
	return (unsigned char)wParam;
}

void resizeWindow(HWND hWnd, int nWidth, int nHeight) {
   RECT rcClient, rcWindow;
   POINT ptDiff;
     GetClientRect(hWnd, &rcClient);
     GetWindowRect(hWnd, &rcWindow);
   ptDiff.x = (rcWindow.right - rcWindow.left) - rcClient.right;
   ptDiff.y = (rcWindow.bottom - rcWindow.top) - rcClient.bottom;
   MoveWindow(hWnd,rcWindow.left, rcWindow.top, nWidth + ptDiff.x, nHeight + ptDiff.y, TRUE);
}

void toggleResolutions(HWND hwnd, int direction) {
	static int n = 0;
	static int sizes[][3] = {
		{854, 480, 1},
		{800, 480, 1},
		{480, 320, 1},
		{1024, 768, 1},
		{1280, 800, 1},
		{1024, 580, 1}
	};
	static int count = sizeof(sizes) / sizeof(sizes[0]);
	n = (count + n + direction) % count;
	
	int* size = sizes[n];
	int k = size[2];
	
	resizeWindow(hwnd, k * size[0], k * size[1]);
}

LRESULT WINAPI windowProc ( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
	LRESULT retval = 1;
	
	switch (uMsg)
	{
	case WM_KEYDOWN: {
		if (wParam == 33) toggleResolutions(hWnd, -1);
		if (wParam == 34) toggleResolutions(hWnd, +1);
		unsigned char k = transformKey_win32(wParam);
		if (k) Keyboard::feed(k, 1);
		char numericChar = numericCharFromWin32Key(wParam);
		if (numericChar) {
			Keyboard::feedText(numericChar);
			g_win32PendingNumericChar = numericChar;
		}
		return 0;
	}
	case WM_KEYUP: {
		unsigned char k = transformKey_win32(wParam);
		if (k) Keyboard::feed(k, 0);
		return 0;
	}
	case WM_CHAR: {
		//LOGW("WM_CHAR: %d\n", wParam);
		if (wParam >= 32) {
			char character = (char)wParam;
			if (character == g_win32PendingNumericChar)
				g_win32PendingNumericChar = 0;
			else
				Keyboard::feedText(character);
		}
		return 0;
	}
	case WM_UNICHAR:
	case WM_IME_CHAR: {
		// Text input from IMEs and newer touch keyboards is often delivered via
		// one of these messages instead of WM_CHAR.
		if (uMsg == WM_UNICHAR && wParam == UNICODE_NOCHAR)
			return TRUE;
		if (wParam >= 32 && wParam <= 0x7f)
			Keyboard::feedText((char)wParam);
		return 0;
	}
case WM_TOUCH: {
	if (!g_win32TouchInputEnabled && !g_win32GuiInputEnabled) {
		CloseTouchInputHandle((HTOUCHINPUT)lParam);
		return 0;
	}
	UINT numInputs = LOWORD(wParam);
	TOUCHINPUT* inputs = new TOUCHINPUT[numInputs];

	if (GetTouchInputInfo((HTOUCHINPUT)lParam, numInputs, inputs, sizeof(TOUCHINPUT))) {
		for (UINT i = 0; i < numInputs; ++i) {
			TOUCHINPUT& ti = inputs[i];

			// WM_TOUCH coordinates are in 1/100 pixel screen coordinates.
			POINT pt;
			pt.x = ti.x / 100;
			pt.y = ti.y / 100;

			// Convert screen coordinates to this game's client-area coordinates.
			ScreenToClient(hWnd, &pt);

			int pointerId = findWin32TouchSlot(ti.dwID);

			if (ti.dwFlags & TOUCHEVENTF_DOWN) {
				pointerId = allocateWin32TouchSlot(ti.dwID);
				if (pointerId < 0) continue;
				++g_win32TouchCount;
				g_win32TouchActive = true;
				if (g_win32PrimaryTouch < 0)
					g_win32PrimaryTouch = pointerId;

				Multitouch::feed(
					MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN,
					(short)pt.x,
					(short)pt.y,
					(char)pointerId
				);
				if (pointerId == g_win32PrimaryTouch)
					Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN, (short)pt.x, (short)pt.y);
				else
					Mouse::feedEventOnly(MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN, (short)pt.x, (short)pt.y);
			}
			else if (ti.dwFlags & TOUCHEVENTF_UP) {
				if (pointerId < 0) continue;
				Multitouch::feed(
					MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
					(short)pt.x,
					(short)pt.y,
					(char)pointerId
				);
				if (pointerId == g_win32PrimaryTouch) {
					Mouse::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP, (short)pt.x, (short)pt.y);
					g_win32PrimaryTouch = -1;
				} else {
					Mouse::feedEventOnly(MouseAction::ACTION_LEFT, MouseAction::DATA_UP, (short)pt.x, (short)pt.y);
				}
				g_win32TouchIds[pointerId] = 0;

				if (g_win32TouchCount > 0)
					--g_win32TouchCount;

				if (g_win32TouchCount == 0)
					g_win32TouchActive = false;
			}
			else if (ti.dwFlags & TOUCHEVENTF_MOVE) {
				if (pointerId < 0) continue;
				Multitouch::feed(
					MouseAction::ACTION_MOVE, MouseAction::DATA_UP,
					(short)pt.x,
					(short)pt.y,
					(char)pointerId
				);
				if (pointerId == g_win32PrimaryTouch)
					Mouse::feed(MouseAction::ACTION_MOVE, MouseAction::DATA_UP, (short)pt.x, (short)pt.y);
			}
		}
	}

	CloseTouchInputHandle((HTOUCHINPUT)lParam);
	delete[] inputs;

	return 0;
}
	case WM_LBUTTONDOWN: {
	if ((!g_win32TouchInputEnabled || g_win32GuiInputEnabled) && !g_win32TouchActive && !isPromotedWin32TouchMouseMessage()) {
	TOUCH_TRACE("[MOUSE] left-down at=(%d,%d) captured=%d\\n", GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), g_win32MouseCaptured);
	Mouse::feed(
		MouseAction::ACTION_LEFT,
		1,
		GET_X_LPARAM(lParam),
		GET_Y_LPARAM(lParam)
	);

	}

	break;
	}

	case WM_LBUTTONUP: {
	if ((!g_win32TouchInputEnabled || g_win32GuiInputEnabled) && !g_win32TouchActive && !isPromotedWin32TouchMouseMessage()) {
	TOUCH_TRACE("[MOUSE] left-up at=(%d,%d) captured=%d\\n", GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), g_win32MouseCaptured);
	Mouse::feed(
		MouseAction::ACTION_LEFT,
		0,
		GET_X_LPARAM(lParam),
		GET_Y_LPARAM(lParam)
	);

	}

	break;
	}
	case WM_RBUTTONDOWN: {
		if ((!g_win32GuiInputEnabled && g_win32TouchInputEnabled) || g_win32TouchActive || isPromotedWin32TouchMouseMessage()) return 0;
		// A stale left-button state turns a right-click placement into one
		// destroy tick followed by a placement.  Desktop controls treat the
		// two buttons as mutually exclusive actions.
		TOUCH_TRACE("[MOUSE] right-down at=(%d,%d) captured=%d leftWas=%d\\n", GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), g_win32MouseCaptured, Mouse::isButtonDown(MouseAction::ACTION_LEFT));
		Mouse::feed(MouseAction::ACTION_LEFT, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		Mouse::feed( MouseAction::ACTION_RIGHT, 1, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	}
	case WM_RBUTTONUP: {
		if ((!g_win32GuiInputEnabled && g_win32TouchInputEnabled) || g_win32TouchActive || isPromotedWin32TouchMouseMessage()) return 0;
		TOUCH_TRACE("[MOUSE] right-up at=(%d,%d) captured=%d\\n", GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), g_win32MouseCaptured);
		Mouse::feed( MouseAction::ACTION_RIGHT, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	}
	case WM_MOUSEMOVE: {
		if ((!g_win32TouchInputEnabled || g_win32GuiInputEnabled) && !g_win32TouchActive && !isPromotedWin32TouchMouseMessage()) {
			if (!g_win32MouseCaptured) {
				Mouse::feed(MouseAction::ACTION_MOVE, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			}
		}
		break;
	}
	case WM_MOUSEWHEEL: {
		if ((!g_win32GuiInputEnabled && g_win32TouchInputEnabled) || isPromotedWin32TouchMouseMessage()) return 0;
		short delta = GET_WHEEL_DELTA_WPARAM(wParam);
		// WM_MOUSEWHEEL carries screen coordinates; GUI hit testing uses client
		// coordinates.  This matters for panes that only scroll under the cursor.
		POINT pt;
		pt.x = GET_X_LPARAM(lParam);
		pt.y = GET_Y_LPARAM(lParam);
		ScreenToClient(hWnd, &pt);
		Mouse::feed(MouseAction::ACTION_WHEEL, (delta != 0) ? 1 : 0,
		            (short)pt.x, (short)pt.y, 0, (short)(delta / 120));
		break;
	}
	case WM_INPUT: {
		if ((!g_win32TouchInputEnabled || g_win32GuiInputEnabled) && g_win32MouseCaptured) {
			UINT size = sizeof(RAWINPUT);
			RAWINPUT raw;
			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
				raw.header.dwType == RIM_TYPEMOUSE &&
				!(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
				const short dx = (short)raw.data.mouse.lLastX;
				const short dy = (short)raw.data.mouse.lLastY;
				if (dx != 0 || dy != 0) {
					RECT r;
					GetClientRect(hWnd, &r);
					const short cx = (short)((r.right - r.left) / 2);
					const short cy = (short)((r.bottom - r.top) / 2);
					if (g_win32MouseMoveTraceBudget-- > 0)
						TOUCH_TRACE("[RAW] move dx=%d dy=%d center=(%d,%d)\\n", dx, dy, cx, cy);
					Mouse::feed(MouseAction::ACTION_MOVE, 0, cx, cy, dx, dy);
				}
			}
		}
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	case WM_KILLFOCUS: {
		releaseWin32Touches();
		if (g_win32MouseCaptured) {
			g_win32MouseCaptured = false;
			ShowCursor(TRUE);
			ClipCursor(NULL);
		}
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	case WM_CANCELMODE:
		releaseWin32Touches();
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	default:
		if (uMsg == WM_NCDESTROY) g_running = false;
		else {
			if (uMsg == WM_SIZE) {
				if (g_app) g_app->setSize( GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) );
			}
		}
		retval = DefWindowProc (hWnd, uMsg, wParam, lParam);
		break;
	}
	return retval;
}

void platform(HWND *result, int width, int height) {
	WNDCLASS wc;
	RECT wRect;
	HWND hwnd;
	HINSTANCE hInstance;

	wRect.left = 0L;
	wRect.right = (long)width;
	wRect.top = 0L;
	wRect.bottom = (long)height;

	hInstance = GetModuleHandle(NULL);

	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC)windowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "OGLES";

	RegisterClass(&wc);

	AdjustWindowRectEx(&wRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);

	hwnd = CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, "OGLES", "main", WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, wRect.right-wRect.left, wRect.bottom-wRect.top, NULL, NULL, hInstance, NULL);
	RegisterTouchWindow(hwnd, 0);
	*result = hwnd;
}

/** Thread that reads input data via UDP network datagrams
    and fills Mouse and Keyboard structures accordingly.
	@note: The bound local net address is unfortunately
	       hard coded right now (to prevent wrong Interface) */
void inputNetworkThread(void* userdata)
{
	// set up an UDP socket for listening
	WSADATA wsaData;
	if (WSAStartup(0x101, &wsaData)) {
		printf("Couldn't initialize winsock\n");
		return;
	}

	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET) {
		printf("Couldn't create socket\n");
		return;
	}
	
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9991);
	addr.sin_addr.s_addr = inet_addr("192.168.0.119");

	if (bind(s, (sockaddr*)&addr, sizeof(addr))) {
		printf("Couldn't bind socket to port 9991\n");
		return;
	}
	
	sockaddr fromAddr;
	int fromAddrLen = sizeof(fromAddr);

	char buf[1500];
	int* iptrBuf = (int*)buf;

	printf("input-server listening...\n");

	while (1) {
		int read = recvfrom(s, buf, 1500, 0, &fromAddr, &fromAddrLen);
		if (read < 0)
		{
			printf("recvfrom failed with code: %d\n", WSAGetLastError());
			return;
		}
		// Keyboard
		if (read == 2) {
			Keyboard::feed((unsigned char) buf[0], (int)buf[1]);
		}
		// Mouse
		else if (read == 16) {
			Mouse::feed(iptrBuf[0], iptrBuf[1], iptrBuf[2], iptrBuf[2]);
		}
	}
}

int main(void) {
	MCPE_EXIT_TRACE("main: begin");
	SetUnhandledExceptionFilter(mcpeWin32CrashHandler);
	AppContext appContext;
	MSG sMessage;
#ifndef STANDALONE_SERVER
	HWND hwnd;
	g_running = true;

	// Window and input initialization are shared by the EGL and WGL variants.
	appContext.platform = new AppPlatform_win32();
	platform(&hwnd, appContext.platform->getScreenWidth(), appContext.platform->getScreenHeight());
	ShowWindow(hwnd, SW_SHOW);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);
	g_win32Hwnd = hwnd;
	{
		RAWINPUTDEVICE rid;
		rid.usUsagePage = 0x01;
		rid.usUsage = 0x02;
		rid.dwFlags = 0;
		rid.hwndTarget = hwnd;
		RegisterRawInputDevices(&rid, 1, sizeof(rid));
	}

#if defined(WIN32_WGL)
	Win32WglContext wglContext = {};
	if (!createWin32WglContext(hwnd, &wglContext)) {
		printf("Unable to create the Win32 WGL compatibility context (error %lu)\n", GetLastError());
		appContext.platform->finish();
		delete appContext.platform;
		return 1;
	}
	appContext.graphicsContext = &wglContext;
	appContext.swapGraphicsBuffers = swapWin32WglBuffers;
	appContext.doRender = true;

	// The context must be current before GLEW loads desktop GL extensions.
	glInit();
#else
	// Must run before the first EGL call; PVRVFrame otherwise opens its SDK
	// control panel while it initializes the emulated GLES context.
	disableLegacyPvrVFrameControlWindow();

	EGLint aEGLAttributes[] = {
		EGL_RED_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_BLUE_SIZE,		8,
		EGL_ALPHA_SIZE,		8,
		EGL_DEPTH_SIZE,		16,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
		EGL_NONE
	};
	EGLint aEGLContextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION, 1,
		EGL_NONE
	};

	EGLConfig m_eglConfig[1];
	EGLint nConfigs;

	// EGL init.
	appContext.display = eglGetDisplay(GetDC(hwnd));
	//m_eglDisplay = eglGetDisplay((EGLNativeDisplayType) EGL_DEFAULT_DISPLAY);

	eglInitialize(appContext.display, NULL, NULL);

	eglChooseConfig(appContext.display, aEGLAttributes, m_eglConfig, 1, &nConfigs);
	printf("EGLConfig = %p\n", m_eglConfig[0]);

	appContext.surface = eglCreateWindowSurface(appContext.display, m_eglConfig[0], (NativeWindowType)hwnd, 0);
	printf("EGLSurface = %p\n", appContext.surface);

	appContext.context = eglCreateContext(appContext.display, m_eglConfig[0], EGL_NO_CONTEXT, NULL);//aEGLContextAttributes);
	printf("EGLContext = %p\n", appContext.context);
	if (!appContext.context) {
		printf("EGL error: %d\n", eglGetError());
	}

	eglMakeCurrent(appContext.display, appContext.surface, appContext.surface, appContext.context);
	
	glInit();
#endif // WIN32_WGL
#endif
	App* app = new MAIN_CLASS();

	g_app = app;
	((MAIN_CLASS*)g_app)->externalStoragePath = ".";
	((MAIN_CLASS*)g_app)->externalCacheStoragePath = ".";
	g_app->init(appContext);
	g_app->setSize(appContext.platform->getScreenWidth(), appContext.platform->getScreenHeight());

	//_beginthread(inputNetworkThread, 0, 0);
	
	// Main event loop
	while(g_running && !app->wantToQuit())
	{
		// Do Windows stuff:
		while (PeekMessage (&sMessage, NULL, 0, 0, PM_REMOVE) > 0) {
			if(sMessage.message == WM_QUIT) {
				g_running = false;
				break;
			}
			else {
				TranslateMessage(&sMessage);
				DispatchMessage(&sMessage);
			}
		}
		app->update();
		
		//Sleep(30);
	}

	MCPE_EXIT_TRACE("main: loop ended g_running=%d wantToQuit=%d", g_running ? 1 : 0, app->wantToQuit() ? 1 : 0);
	Sleep(50);
	MCPE_EXIT_TRACE("main: before delete app");
	delete app;
	MCPE_EXIT_TRACE("main: after delete app");
	Sleep(50);
#if defined(WIN32_WGL)
	// The window and HDC must still exist while the WGL context is detached.
	MCPE_EXIT_TRACE("main: before destroy WGL");
	destroyWin32WglContext(&wglContext);
	MCPE_EXIT_TRACE("main: after destroy WGL");
#endif
	MCPE_EXIT_TRACE("main: before platform finish");
	appContext.platform->finish();
	MCPE_EXIT_TRACE("main: after platform finish");
	Sleep(50);
	MCPE_EXIT_TRACE("main: before delete platform");
	delete appContext.platform;
	MCPE_EXIT_TRACE("main: after delete platform");
	Sleep(50);
	//printf("_crtDumpMemoryLeaks: %d\n", _CrtDumpMemoryLeaks());
	
#ifndef STANDALONE_SERVER
	#if !defined(WIN32_WGL)
	// Exit.
	eglMakeCurrent(appContext.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(appContext.display, appContext.context);
	eglDestroySurface(appContext.display, appContext.surface);
	eglTerminate(appContext.display);
	#endif
#endif

	MCPE_EXIT_TRACE("main: complete");
	return 0;
}

#endif /*MAIN_WIN32_H__*/
