# Spammy

**Turn any key into autofire — a lightweight, system-wide keyboard macro assistant for Windows.**

![Spammy UI](https://raw.githubusercontent.com/FrostAtom/Spammy/master/docs/assets/app.png)

## Why

Some games and apps expect you to hammer the same key hundreds of times: spamming abilities in MMOs, semi-auto weapons, clicker grinds. That's tiring, bad for your fingers, and pointless — a machine can press a key faster and more consistently than you.

Spammy solves this at the OS input level: hold a key and it fires repeatedly at the rate you choose, in any application, with no in-game plugins, no scripting language, no setup per game. Create a profile, bind it to your game, paint the keys you want boosted — done. Profiles switch automatically when you switch apps.

## Features

- **Spammy mode** — while a key is held, its press is re-emitted rapidly (autofire) at a configurable rate (10–1000 ms)
- **Speedy mode** — a key press is released instantly
- **Mouse support** — side buttons (M4/M5) and middle click are first-class keys
- **Zero dependencies** — a single small self-contained `.exe`, config is one JSON file next to it

## Getting started

1. Grab the latest `Spammy.exe` from [Releases](https://github.com/FrostAtom/Spammy/releases) and run it — no installation
2. A default profile is created for you, active in all apps, with keys `1–5` set to Spammy
3. Bind the profile to your game via the **ENABLE ONLY IN APPS** menu (or leave it empty to keep it global)
4. Paint keys with the left mouse button, tweak the rate via right-click, set a pause hotkey — and play

## Building from source

Requires CMake ≥ 3.21 and MSVC or clang-cl (x86 target). From an **x86** developer prompt:

```powershell
cmake --preset release
cmake --build --preset release
```

The self-contained binary lands in `build/Release/Spammy.exe`.

## Credits

- [nlohmann/json](https://github.com/nlohmann/json) ([MIT](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT))
- [Dear ImGui](https://github.com/ocornut/imgui) ([MIT](https://github.com/ocornut/imgui/blob/master/LICENSE.txt))
- [Rajdhani](https://fonts.google.com/specimen/Rajdhani) & [JetBrains Mono](https://www.jetbrains.com/lp/mono/) fonts ([OFL](https://openfontlicense.org))

## License

[GPL-3.0](../LICENSE)
