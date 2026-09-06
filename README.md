# GTA-KillSwitch

A lightweight Windows tool for GTA Online that triggers an empty solo public session on demand using a keyboard shortcut. Built to save business sales, cargo deliveries, and heist preps from griefers instantly, without needing to Alt-Tab to Resource Monitor or Task Manager.

Written in C++ using the native Win32 API. Standalone binary (~300 KB), no .NET or external dependencies required.

## Why use this? (Anti-griefing & cargo protection)

Selling business stock in populated public sessions grants a High Demand bonus (up to +50%), but puts hours of grinding at risk from Oppressor Mk IIs, jets, and trolls.

If an attack happens:
- **Finding a new session or force-quitting** aborts the sale and destroys part of your inventory as a penalty.
- **Alt-Tabbing to Resource Monitor** (`resmon.exe`) takes 10 to 15 seconds, leaving your vehicle defenseless while you navigate Windows menus.

This tool acts as an immediate in-game panic button:
1. A hostile player attacks or locks onto your delivery vehicle.
2. Tap `Pause / Break` (or your chosen key) directly in-game.
3. The game freezes for 10 seconds, cutting P2P network traffic to all other players in the lobby.
4. The process resumes: every other player drops from your session ("Player left"), but your connection to Rockstar Cloud remains intact.
5. Your delivery mission continues without penalty in your now-empty public lobby.

```
[Busy Public Lobby] ---> Griefer attacks delivery vehicle
                                 │
                       [Press Pause / Break]
                                 │
                   10-second P2P connection drop
                                 │
[Solo Public Session] -> Griefers disconnected, cargo intact, sale continues!
```

## Features

- **Global hotkey**: Works directly in-game in fullscreen, borderless, and windowed modes. Defaults to `Pause / Break`, rebindable to any key or combination.
- **Audio cues**: Plays beeps when the freeze starts and finishes so you know when the session has cleared without looking away.
- **Configurable duration**: Default 10 seconds (adjustable between 7 and 12 seconds in the UI).
- **Process support**: Automatically detects `GTA5_Enhanced.exe` (PC Enhanced) and `GTA5.exe` (legacy), ignoring BattlEye watchdog processes.
- **System tray**: Minimizes to the notification area to stay out of the way.
- **Lightweight**: Consumes under 4 MB RAM with an executable size of ~300 KB.

## How it works & Safety

GTA Online uses peer-to-peer (P2P) networking for player synchronization in sessions. Suspending the game process for ~10 seconds forces the P2P connection to time out for every peer in the lobby. Because your connection to the Rockstar matchmaking servers remains open, the game simply places you into your own isolated public instance.

- **No memory modification**: Does not read, write, or inject code into game memory.
- **Native OS operation**: Performs the exact same system calls (`NtSuspendProcess` / `NtResumeProcess`) as Windows Resource Monitor (`resmon.exe`).
- **BattlEye compatible**: OS-level thread suspension is completely transparent to anti-cheat integrity scans.

## Usage

1. Download `GTA-KillSwitch.exe` from [Releases](https://github.com/FableShift/GTA-KillSwitch/releases).
2. Run the executable (no admin rights or installation needed).
3. Start GTA Online. The status panel in the app will indicate when the game is detected.
4. Press `Pause / Break` at any time while playing to clear your lobby.

## Settings

Settings are automatically saved to `config.json` next to the executable:

- **Hotkey**: Click the hotkey box and press any key combination to rebind.
- **Freeze Duration**: 7 to 12 seconds (10s recommended; durations over 12s can kick the game back to single-player mode).
- **Sound**: Toggle start and completion beeps.

## Building from source

Requirements:
- Windows 10 or 11 (x64)
- Visual Studio 2022 (MSVC toolset)

Run `build_release.bat` or compile manually from a Developer Command Prompt:

```cmd
rc /nologo resource.rc
cl /nologo /utf-8 /O2 /MT /GL /EHsc /DUNICODE /D_UNICODE main.cpp resource.res /link /SUBSYSTEM:WINDOWS /LTCG /OPT:REF /OPT:ICF /OUT:GTA-KillSwitch.exe
```

## License

[MIT](LICENSE)
