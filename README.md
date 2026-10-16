# WWE 2K15 PC

The project goal is a Windows PC game that runs without an Xbox emulator or Xbox compatibility runtime. The current ReXGlue build is only an experimental Xbox-code recompilation prototype; it does **not** meet that goal.

This repository contains project code and documentation only. It does not contain game data. Use files dumped from your own copy of the Xbox 360 game, and keep executables, disc images, extracted assets, generated code, and build outputs out of Git.

## Status

`port/` contains the ReXGlue prototype and its experimental diagnostics. It translates Xbox 360 PowerPC code to host code but still depends on Xbox kernel, memory, and graphics emulation. Its allocation failures and rendering glitches are symptoms of that approach, not fixes toward an emulator-free port. See [docs/PORTING_PLAN.md](docs/PORTING_PLAN.md) for the captured baseline and [docs/NATIVE_PORT_PLAN.md](docs/NATIVE_PORT_PLAN.md) for the revised project direction.

A complete native port needs the original game source or a substantial reverse-engineering and engine-reimplementation effort. The Xbox executable alone cannot be turned into a clean PC game by patching allocator errors. The project will keep the Xbox executable and extracted game data outside Git.

## Asset research

Run the metadata-only scanner against your own extracted game folder:

```powershell
python native/tools/inspect_game_data.py "C:\path\to\WWE2K15" --samples 1
```

It reports file counts, sizes, recognized signatures, and bounded PAC index candidates. It does not extract or modify game data.

## Disclaimer

This is an unofficial fan project and is not affiliated with WWE, 2K, or Yuke's. All game names and related marks belong to their respective owners.
