#include "AppPlatform_win32.h"
#include "util/Mth.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

int AppPlatform_win32::getScreenWidth()  { return 854; }
int AppPlatform_win32::getScreenHeight() { return 480; }

float AppPlatform_win32::getPixelsPerMillimeter() {
	// assuming 24" @ 1920x1200
	const int w = 1920;
	const int h = 1200;
	const float pixels = Mth::sqrt(w*w + h*h);
	const float mm	   = 24 * 25.4f;
	return pixels / mm;
}

bool AppPlatform_win32::supportsTouchscreen()  { return true; }
bool AppPlatform_win32::hasBuyButtonWhenInvalidLicense() { return true; }
void AppPlatform_win32::showKeyboard() {
	ShellExecuteA(NULL, "open", "C:\\Program Files\\Common Files\\microsoft shared\\ink\\TabTip.exe", NULL, NULL, SW_SHOWNORMAL);
}
