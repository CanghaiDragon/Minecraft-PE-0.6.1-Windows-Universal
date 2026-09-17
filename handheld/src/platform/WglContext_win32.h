#ifndef WGL_CONTEXT_WIN32_H__
#define WGL_CONTEXT_WIN32_H__

// The legacy renderer uses OpenGL's fixed-function compatibility API.  This
// Win32-only layer owns all native GL context lifetime operations.
#include <windows.h>

struct Win32WglContext {
	HWND window;
	HDC deviceContext;
	HGLRC renderContext;
};

static bool createWin32WglContext(HWND window, Win32WglContext* context) {
	if (!window || !context) return false;

	context->window = window;
	context->deviceContext = GetDC(window);
	context->renderContext = NULL;
	if (!context->deviceContext) return false;

	PIXELFORMATDESCRIPTOR format = {};
	format.nSize = sizeof(format);
	format.nVersion = 1;
	format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	format.iPixelType = PFD_TYPE_RGBA;
	format.cColorBits = 32;
	format.cDepthBits = 24;
	format.cStencilBits = 8;
	format.iLayerType = PFD_MAIN_PLANE;

	const int pixelFormat = ChoosePixelFormat(context->deviceContext, &format);
	if (!pixelFormat || !SetPixelFormat(context->deviceContext, pixelFormat, &format)) {
		ReleaseDC(window, context->deviceContext);
		context->deviceContext = NULL;
		return false;
	}

	// wglCreateContext deliberately creates the driver's compatibility context.
	context->renderContext = wglCreateContext(context->deviceContext);
	if (!context->renderContext ||
		!wglMakeCurrent(context->deviceContext, context->renderContext)) {
		if (context->renderContext) wglDeleteContext(context->renderContext);
		context->renderContext = NULL;
		ReleaseDC(window, context->deviceContext);
		context->deviceContext = NULL;
		return false;
	}

	return true;
}

static void swapWin32WglBuffers(void* opaqueContext) {
	Win32WglContext* context = static_cast<Win32WglContext*>(opaqueContext);
	if (context && context->deviceContext) SwapBuffers(context->deviceContext);
}

static void destroyWin32WglContext(Win32WglContext* context) {
	if (!context) return;
	if (wglGetCurrentContext() == context->renderContext)
		wglMakeCurrent(NULL, NULL);
	if (context->renderContext) wglDeleteContext(context->renderContext);
	if (context->deviceContext && context->window)
		ReleaseDC(context->window, context->deviceContext);
	context->renderContext = NULL;
	context->deviceContext = NULL;
	context->window = NULL;
}

#endif // WGL_CONTEXT_WIN32_H__
