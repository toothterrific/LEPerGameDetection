# MELE Discord Rich Presence Mod

Discord Rich Presence (the activity box under your Discord name) for **Mass Effect: Legendary Edition**.

- Works in **LE1, LE2 and LE3** (one file for all three).
- Shows which game you are playing, main menu vs in-game, current area, and a playtime timer.
- Talks directly to the Discord desktop app over its local IPC pipe — no OAuth, no background services.

## Status

In development. See `README.md` growth over milestones in git history.

## Project layout

| Path | Purpose |
|---|---|
| `src/` | ASI mod source code (C++, MSVC) |
| `toolkit/rpc_test/` | Standalone Discord IPC test tool (no game needed) |
| `assets/art/` | Art assets to upload in the Discord developer portal |
| `External/MinHook/` | Hooking library (submodule) |
| `build/out/` | Build output (`MELEDiscordRPC.asi`, `rpc_test.exe`) |

## Build (one-time setup)

Requires VS Build Tools 2022 with the "Desktop development with C++" workload (already installed on this machine).

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Outputs land in `build\out\`.
