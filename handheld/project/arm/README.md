# Windows ARM32 build

This directory is reserved for a future Windows ARM32 build of Minecraft PE 0.6.1.

The ARM32 Visual Studio project has not been created yet. Do not treat this directory as a currently supported target.

## WGL/GLEW probe

`WglGlewProbe.cpp` is an independent test program. It creates a Win32 window,
selects a pixel format, creates a WGL context, initializes GLEW, and clears the
window with OpenGL. It is intentionally separate from the game project.

To test ARM32 support, create a small Visual Studio Desktop C++ project with
platform target `ARM`, add `WglGlewProbe.cpp`, and link the ARM32 versions of
`opengl32.lib` and `glew32.lib`. A successful launch with a blue window confirms
that the basic ARM32 WGL/GLEW path is available.
