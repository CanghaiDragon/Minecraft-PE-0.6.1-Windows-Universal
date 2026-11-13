# Update Log/更新日志

This changelog covers the Win32 project. It is separate from the UWP changelog.

## 1.1

New features:

- Added the Alpha world type, including Alpha and Winter variants.
- Added spectator-mode X-ray viewing inside blocks (toggle with F in Creative mode).
- Enabled cave generation for the non-finite world types.
- Made Java-style and original-style UI selectable independently of the input mode.

Bug fix:

- Fixed sign-related save/reload crashes in Infinite and Sky worlds.
- Fixed snow and trees at chunk boundaries being lost after saving and reloading.
- Improved lighting under cliffs and trees in extended worlds, including saved lighting under trees.
- Increased gold-ore generation in Sky worlds for newly generated chunks.

Improvements:

* Improved touch D-pad behavior, including jumping while moving forward.
* Restore the ability to swim when using original D-Pad;
* Improved rendering and visual behavior, including translucent ice under Fancy graphics with Java Beta visuals.
* Added flowing water, flowing lava, ice and snow layers to the Creative inventory.
* Improved options and input handling.

## Earlier Win32 development (through 1.0)

### Platform and input

- Restored sound output.
- Replaced PowerVR OpenGL ES emulation with native WGL-based desktop OpenGL.
- Restored text input for world names, seeds and player names.
- Fixed eating food in keyboard mode.
- Improved mouse targeting, block breaking and placement, and screen-button hover feedback.
- Added keyboard shortcuts including F1, F5 and Q, and F3 debugging support.

### World generation, rendering and gameplay fixes

- Corrected infinite-world coordinates and spawn handling inherited from finite-world assumptions (from Project B).
- Introduced multi-region storage for infinite worlds (from Project B).
- Fixed missing chunk-boundary faces after adjacent chunks loaded (from Project B).
- Fixed crashes when loading existing Infinite worlds with Java Beta visuals enabled.
- Fixed snow and tree features disappearing after save/reload.
- Fixed mob spawning in Infinite and Sky worlds and unsafe Sky spawn positions.
- Corrected sun movement relative to the F3 compass and Java Beta behavior.
- Added biome-dependent grass and oak/birch leaf coloring for Java Beta visuals.
- Fixed Nether Reactor crashes and non-responsiveness, including in Infinite and Sky worlds.
