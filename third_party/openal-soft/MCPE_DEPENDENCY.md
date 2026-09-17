# OpenAL Soft dependency

This directory vendors OpenAL Soft 1.25.1 (source revision
`c41d64c6a35f6174bf4a27010aeac52a8d3bb2c6`). Its own license files, including
`COPYING` and `BSD-3Clause`, remain in this directory.

`handheld/project/win32/OpenAL.props` is the only Visual Studio integration
point. It uses paths relative to the repository and chooses a binary set from:

```
prebuilt/<platform-family>/<architecture>/{lib,bin}
```

The current verified Win32/x86 package follows the existing `handheld/lib`
dependency layout:

```
handheld/lib/include/AL/*.h
handheld/lib/lib/OpenAL32.lib
handheld/lib/bin/OpenAL32.dll
```

The Visual Studio project links this import library but does not copy the DLL
to the output directory. Package `OpenAL32.dll` deliberately with the game
only when the target distribution requires sound support.

Future desktop builds should be built from this checked-in source and placed
under `prebuilt/win32/x64`, `prebuilt/win32/arm64`, or
`prebuilt/win32/arm32`. The Visual Studio platform mappings are `x64`, `ARM64`,
and `ARM`, respectively. Do not copy a binary from another architecture.

For a future UWP project, set `ApplicationType` to `Windows Store`; the same
property sheet then selects `prebuilt/uwp/<architecture>`. Build OpenAL Soft
with the UWP toolchain/configuration appropriate to that project and package
its runtime according to UWP rules. The property sheet deliberately does not
attempt desktop DLL deployment for UWP.
