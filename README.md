# MELE Discord Rich Presence Mod

Native (ASI) mod that gives **Mass Effect: Legendary Edition** a clean per-game Discord presence: while you play, your Discord status shows **"Mass Effect"**, **"Mass Effect 2"** or **"Mass Effect 3"** — using that game's **official exe icon** — plus a session playtime timer. Nothing else is mined from the game.

- **Single file**: one `LEPerGameDetection.asi` works in all three games (process-name detection at load).
- **Minimal risk**: no memory pattern scanning, no engine hooks, no game data reads. It only talks to the Discord desktop app over its local IPC pipe (`\\?\pipe\discord-ipc-*`). No OAuth, no background services.
- **Requires**: the MELE ASI loader (bink2w64 proxy, standard for MELE mods) and Discord desktop running.

## How it works

1. The ASI loader maps the mod into the game process at startup.
2. The mod detects which game it is inside (`MassEffect1/2/3.exe`), picks that game's Discord application ID, connects to the running Discord desktop client over IPC, and pushes one static presence (icon + playtime timer).
3. A worker thread keeps the pipe alive (health checks, reconnect + re-push if Discord restarts).
4. When the game closes the pipe closes with it, and Discord clears the presence automatically.

## Project layout

| Path | Purpose |
|---|---|
| `src/` | ASI mod source (C++, MSVC, x64) |
| `toolkit/rpc_test/` | Standalone Discord IPC tester (no game needed) |
| `toolkit/rpc_probe/` | Protocol experiments tool (advanced) |
| `assets/art/` | Discord portal art (`le1/le2/le3` â€” official icons extracted from the game exes) |
| `scripts/package.ps1` | Builds the distribution zip into `dist/` |
| `External/MinHook/` | Unused since v2 (kept for history) |

## Discord application setup

Rich presence needs per-game **Discord applications** (free, no bot, no OAuth):

| App name | Built-in Application ID | Art to upload (Rich Presence â†’ Art Assets) | Asset name |
|---|---|---|---|
| `Mass Effect` | `1568689342417739806` | `assets/art/le1.png` | `le1` |
| `Mass Effect 2` | `1568722277154160791` | `assets/art/le2.png` | `le2` |
| `Mass Effect 3` | `1568722427175895060` | `assets/art/le3.png` | `le3` |

The app **name** is what your friends see as the game title â€” name them exactly like the games. (Using your own IDs? Put them in the ini, no rebuild needed.)

## Build (one-time setup)

Requires VS Build Tools 2022 with the "Desktop development with C++" workload.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Outputs: `build\out\LEPerGameDetection.asi` (+ dev tools).

## Configuration (optional)

Copy `LEPerGameDetection.ini.example` to `LEPerGameDetection.ini`, next to the `.asi`:

| Key | Meaning | Default |
|---|---|---|
| `client_id_le1/le2/le3` | Per-game Discord application IDs | compiled-in values |
| `enabled` | Mod on/off without uninstalling | `1` |
| `debug` | Opens a debug console window while the game runs | `0` |
| `show_timer` | Playtime timer under the game name | `1` |

## Installation

1. Have the ASI loader installed (ME3Tweaks Mod Manager installs it for you: **Mod Management â†’ ASI Mod Manager**; ALOT users already have it).
2. Copy `LEPerGameDetection.asi` into `Game\ME1\Binaries\Win64\ASI\` (the ASI subfolder where the loader picks plugins up — repeat for ME2/ME3 as desired).
3. Launch the game with Discord running â€” done.

## Diagnosing

`debug=1` opens a console panel; logs land in `Binaries\Win64\Logs\` next to each game (falling back to `%LOCALAPPDATA%\MELEDiscordRPC\` only if the game folder is read-only).

If nothing shows: confirm Discord desktop is running, confirm the per-game asset `le1/le2/le3` exists in the right app's Art Assets, check the log for connect errors.

## Credits

- ME3Tweaks / Erik-JS ecosystems â€” the ASI loader tooling.
- BioWare/EA â€” the games and the official art (extracted from the games' own resources).
