# MELE Discord Rich Presence Mod

Native (ASI) mod that gives **Mass Effect: Legendary Edition** a proper Discord Rich Presence:
which game you're playing (LE1/LE2/LE3), main-menu vs. current location, and a session playtime timer — rendered with official game art.

- **Ship shape**: single `MELEDiscordRPC.asi` works in all three games (process-name detection at load).
- **No dependency on closed SDKs**: talks straight to the Discord desktop app over its local IPC pipe (`\\?\pipe\discord-ipc-*`). No OAuth, no background service, no DLL shipping.
- **Requires**: the MELE ASI loader (bink2w64 proxy, standard for MELE mods) and Discord desktop running.

## How it works

1. Loaded at game start by the ASI loader.
2. Detects the game (`MassEffect1/2/3.exe`), pattern-scans `GWorld`, the SFX name pools and `ProcessEvent` (patterns sourced from the public LExSDKv2/LExASIs projects — stable since BioWare no longer patches the trilogy).
3. Hooks `ProcessEvent` (via MinHook) so all game-state reads happen on the game thread.
4. Every 500 ms reads the current world's root package name (via `SFXName` pools) and publishes presence state.
5. A worker thread keeps one Discord IPC pipe connection alive: sends `SET_ACTIVITY` when state changes, ping keep-alives otherwise, reconnects if Discord restarts.

Unknown maps show as `Exploring` (or raw package name if enabled) — see [Configuration](#configuration).

## Project layout

| Path | Purpose |
|---|---|
| `src/` | ASI mod source (C++, MSVC, x64) |
| `toolkit/rpc_test/` | Standalone Discord IPC tester (no game needed) |
| `assets/art/` | Art to upload into the Discord developer portal (`le1`, `le2`, `le3`, `menu`) — official icons extracted from the game exes + official key art |
| `scripts/package.ps1` | Builds the distribution zip into `dist/` |
| `dist/` | Release output |
| `External/MinHook/` | Hooking library (git submodule, MIT) |

## Discord application setup

Rich presence needs a Discord **application** (free):

1. Create an app at `https://discord.com/developers/applications` named `Mass Effect Legendary Edition` (this name is the top line your friends see).
2. Note the **Application ID**.
3. Under **Rich Presence → Art Assets** upload:
   - `assets/art/le1.png` as asset name `le1`
   - `assets/art/le2.png` as asset name `le2`
   - `assets/art/le3.png` as asset name `le3`
   - your launcher/menu icon as asset name `menu`
4. Nothing else (no bot, no OAuth redirect) is needed.

## Build (one-time setup)

Requires VS Build Tools 2022 with the "Desktop development with C++" workload.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Outputs: `build\out\MELEDiscordRPC.asi` and `build\out\rpc_test.exe`.

## Configuration

Copy `MELEDiscordRPC.ini.example` to `MELEDiscordRPC.ini`, next to the `.asi`, and set `client_id=`.

| Key | Meaning | Default |
|---|---|---|
| `client_id` | Discord application ID | compiled-in value |
| `enabled` | Mod on/off (without uninstalling) | `1` |
| `debug` | Opens a debug console window while the game runs | `0` |
| `show_raw_packages` | Show internal map names ("Exploring BioA_Xxx") instead of "Exploring" | `0` |
| `show_game_number` | State line says "Mass Effect 2" vs "Mass Effect" | `1` |

## Installation

### Manual

1. Have the ASI loader installed (ME3Tweaks Mod Manager installs it for you: **Mod Management → ASI Mod Manager**).
2. Copy `MELEDiscordRPC.asi` + `MELEDiscordRPC.ini` into `Game\ME1\Binaries\Win64\` (repeat for ME2/ME3 as desired).
3. Launch the game with Discord running.

Full details in `dist\INSTALL.txt`, bundled in every release zip.

### Via ME3Tweaks Mod Manager (planned)

M3's **ASI Mod Manager** installs .asi files from the ME3Tweaks manifest for the community; its rule: **listed mods must publish source code + a direct download link**. Publishing path:

1. Push this repo to GitHub (`https://github.com/toothterrific/MELE-Discord-RPC`).
2. Tag a GitHub Release (e.g. `v1.0`) with `scripts/package.ps1` output (`dist\MELEDiscordRPC_v1.0.zip`) attached as an asset.
3. Submit the repo + release link to ME3Tweaks (forums or the ME3Tweaks Discord) to request manifest listing. Users then install one-click from M3's ASI Mod Manager.

Until listed, manual install works identically.

## Diagnosing

`debug=1` in the ini opens a console panel; logs always go to `%LOCALAPPDATA%\MELEDiscordRPC\`.

Watched failure modes:
- Patterns not found → game updated in a future re-release (very unlikely; last game patch predates this mod).
- Lookups fail → check log for `Name lookup failed` lines.
-presence not visible → Discord desktop must be running; verify client_id.

## Credits

- ME3Tweaks / SirCxyrtyx / Mgamerz — ASI ecosystem, patterns, LExSDKv2 knowledge base.
- Erik-JS — ASI loader ecosystem for MELE.
- TsudaKageyu — MinHook.
- BioWare/EA — the games, the official art (extracted from the games' own resources).
