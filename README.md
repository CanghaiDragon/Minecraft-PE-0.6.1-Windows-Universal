# **Minecraft PE 0.6.1 Windows Universal**

This project came from a leaked source code of **Minecraft Pocket Edition v0.6.1**. It rebuild the game to let it works on modern Windows platforms, including x64, x86, arm64 and arm architecture, with several bug fixes, optimization and additional contents.

Feel free to fork this project, but **DO NOT** use for commercial purposes.

(待补充一版中文的readme)

---

## References & Quote

In March 2026, the source code of Minecraft Pocket Edition v0.6.1 was leaked on the Internet. You can find it at [Minecraft PE Source Code : mojang : Free Download, Borrow, and Streaming : Internet Archive](https://archive.org/details/Minecraftpesorucecode). This is the reason why several revisions of MCPE0.6.1 appears on github and other sites. All the revisions **DO NOT** have direct authorization from Mojang. This project is also a revision of the origin code. If it infring your intellectual property, please contact me and I will takedown this repository.

This project was originally forked from programmer1o1's repository [GitHub - programmer1o1/MinecraftPE-v0.6.1: Leaked Minecraft Pocket Edition v0.6.1 source code, ported to macOS, Linux, and modernized for Android 10-14+ and modern iOS. For educational and preservation purposes only. · GitHub](https://github.com/programmer1o1/MinecraftPE-v0.6.1). (I will call it **Project B** below for convenience) However it became more and more distinguished after several revisions and updates. Unlike programmer1o1's respository, which transplant the game to several operating system, this project only focus on Windows, but transplant the game on all four architectures instead of only x86. Overall, this project is originated from programmer1o1's repository, with several features such as localized options and infinite world, but have done far more optimizations and new features.

Some features in this project use code from [GitHub - Minecraft-PE-0-6-1/minecraft-pe-0.6.1-on-all: Dont do pull requests or issue to this repo. Maintaining repo is gitea repo · GitHub](https://github.com/Minecraft-PE-0-6-1/minecraft-pe-0.6.1-on-all) (I will call it **Project A** below for convenience) for reference especially on code related to Java Beta Visuals.

The additional feature "Sky" world type was originated from the unused feature in Minecraft Java Edition Beta 1.7.3. The original source code of b1.7.3 was used for reference, with the help of Mod Coder Pack. You can download MCP at [mcp43](https://www.mediafire.com/file/03d94f13c9ulj5a/mcp43.zip).

(待补充四个依赖.dll的引用)

The main work of this project is assisted with the help of Codex.

--- 

# Features

## Basic feature of MCPE0.6.1

This project basically keep all the features of MCPE0.6.1 on win32 platform, including old world generation, mobs, sky and light rendering, nether reactor(待验证), as well as the whole touch screen control mode.

### New Features

There are six mian new features that are neither in original MCPE0.6.1 nor Project B.

- Multi architecture versions on Windows platform, with native dependencies, including x86, x64, arm and arm64(现在还差arm和x86)

- Two types of new world, Infinite world (exist in Project B) and Sky world(会在下面单独介绍)

- A brand new graphic visual effect that originated from Java Edition Beta's visual(下面单独介绍)

- Optimization of skin

- Sneak button on touch mode, similar to MCPE 0.12 and later

- A debug screen (F3), open to both touch and keyboard mode

### Optimizations

I've made several optimizations based on MCPE0.6.1 and Project B.

- Fixing the issue of no sound in Project B

- Transform Graphic API from PowerVR OpenGL ES emulation to Native WGL-based desktop OpenGL rendering

- Recover the touch screen control mode on win32 comparing to Project B

- Keep the keyboard and mouse control mode from Project B. Players can switch their input mode between touch and keyboard

- Immobilize the mouse to crosshair and make it easier to move viewing angle on keyboard mode

- Solving the problem of mistakenly breaking blocks on touch/keyboard mode

- Solving the problem of cannot place blocks in some situations on touch/keyboard mode

- Highlight reaction when mouse or finger moves to a button

- Java Edition style button on keyboard mode

- Original input textbox, so players can input world name, seed and player name

- More options for GUI and D-Pad size

- A brand new welcome page, allowing players to choose their input mode and GUI size

- Recover the ability to eat on keyboard mode, comparing to Project B

- Fix the bug of mobs hardly spawn at night in infinite world, comparing to Project B

- Adding running in creative mode, but not survival mode for gaming balance

- Fixing the bug of random chunk's feature (trees and snow) cannot be saved, which mostly affect infinite world

- Adding the shortcut of F1 (Hide HUD) and F5 (switch to third-person view) on keyboard mode

### Sky World

### Java Beta Visual

---

# Building

(待补充)

---

# Future plans

- Chinese support

- Better saving world

- Converter for Infinite and Sky world, so they can be opened in later versions of original MCPE

- UWP version
