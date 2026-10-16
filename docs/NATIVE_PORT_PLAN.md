# WWE 2K15 native PC port

## Goal

Create a Windows x64 game that runs without an Xbox emulator, Xbox kernel layer, or runtime recompilation of Xbox code. Reuse only legally obtained game assets where their formats can be read; provide PC-native input, rendering, audio, file loading, and game logic.

## Current facts

- The existing `port/` build uses ReXGlue to recompile the Xbox 360 PowerPC executable and emulate Xbox services. It is a useful research prototype, but it is not the requested deliverable.
- The available Xbox build is `default.xex` (Title ID `545408B6`). The project does not have WWE 2K15's original engine source code.
- Without source code, a true native implementation means reverse engineering the executable and rebuilding the engine/gameplay systems. Allocation patches in ReXGlue cannot accomplish that.
- WWE 2K15 also had an official Windows release in 2015. It was announced as matching the PS4/Xbox One edition, so it is distinct from the Xbox 360 build this project has been studying. [Official WWE announcement](https://www.wwe.com/inside/2k/wwe-2k15/wwe-2k15-windows-pc-announcement-this-spring).

## Work sequence

1. **Freeze the recompilation prototype as a reference.** Preserve its logs, manifests, and title metadata. Do not treat emulator fixes as native-port milestones.
2. **Inventory the Xbox game data.** Record file names, sizes, hashes, and formats (`.arc`, `.def`, `.pck`, `.bk2`, and other containers) without checking copyrighted contents into Git. Identify which assets are reusable and which codecs/container readers need to be implemented.
3. **Establish a native application shell.** Create a Windows x64 executable with native windowing, Direct3D 12 or Vulkan rendering, audio, input, and asset-mount interfaces. It must not link ReXGlue or load the Xbox XEX at runtime.
4. **Reverse engineer formats and data models.** Document container layouts, animation/skeleton formats, materials, arenas, audio banks, and video. Add small offline inspection/conversion tools where legally appropriate.
5. **Build a vertical slice from original data.** Load one arena, one wrestler model, its textures and animations, and render a controllable training scene. Add input and audio before expanding the roster or modes.
6. **Recreate gameplay systems incrementally.** Implement match state, movement, grapples, collisions, camera, HUD, entrances, menus, and save data behind testable interfaces. Compare behavior with the original game using user-observed references.
7. **Expand content and packaging.** Add asset compatibility, configuration, controller support, performance work, and setup documentation. Keep game executables and assets out of Git.

## Initial local inventory (2026-10-16)

The extracted game folder contains 674 files totaling about 8.2 GB. The largest groups are 370 `.pac` files (about 3.8 GB), 144 `.bk2` videos (about 2.7 GB), 115 `.xzp` files, 25 `.xex` files, and three `.pck` sound banks (about 1.0 GB). There are also two `.arc` and two `.def` files. The main `default.xex` is 24,133,632 bytes with SHA-256 `1AAF88A63A57AD3435E5DB6B228809D06929A07BD8CB7296CFBA0D5A903E0503`.

Initial header samples show several immediately distinguishable formats: `default.xex` and `.lex` modules start with `XEX2`; one `.pac` starts with `EPAC`, `_startup.pac` starts with `PACH`, and another PAC starts with XCompress/LZX magic `0x0FF512ED`; BK2 video starts with `KB2i`; sound banks start with `AKPK`; `.xzp` starts with `BDES`; the `.def` file is text beginning `;PAC_LIST_ARC_EPAC_BIG`; the `.arc` begins with a table of four-character tags such as `AUDE` and `BORD`; `.xtt` begins with `xttf`. These are signature observations, not complete format specifications. PAC needs separate unpackers for its variants.

`native/tools/inspect_game_data.py` performs the read-only inventory and prints samples grouped by detected signature. For EPAC and EPK8 it reports candidate list/data sizes and checks bounds; for PACH it checks the table length, scans relative entry ranges against file size, and prints the first five entries. The PAC layout hypothesis is based on community reverse-engineered definitions in [Keshire's WWE2K15-Tools](https://github.com/Keshire/WWE2K15-Tools/blob/master/structures/WWE2K15-PAC.hsl) and needs validation against all variants. Keep the inventory as metadata only; do not copy game data into Git.

The header scan currently classifies 337 files as XCompress/LZX streams, 19 as EPAC, 13 as EPK8, seven as PIRS, and one as PACH; it also finds 144 Bink 2 videos and three AKPK sound banks. Signature classification is preliminary: a compressed container may itself contain another PAC/PACH structure after decompression.

Next, build bounded parsers for the EPAC index and PACH table, then investigate decompression of LZX/XCompress and the other PAC variants. Inspect BK2 and AKPK as separate subsystems. Do not start by changing the ReXGlue allocator: those failures belong to the prototype path.

## Constraints and risks

- This is a large reverse-engineering project, not a straightforward recompilation fix. A faithful full game requires recreating substantial proprietary engine and gameplay behavior.
- The original executable and extracted game files stay local and untracked. Commit only original project code and documentation.
- The presence of the official Windows edition does not mean its internal source code is available or that it is a drop-in conversion of the Xbox 360 release.
