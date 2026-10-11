# Spammy

**Hold a key — it fires on its own. Autofire for any game on Windows.**

![Spammy](https://raw.githubusercontent.com/FrostAtom/Spammy/master/docs/assets/app.png)

## Why

Some games want you to mash the same key over and over: ability spam in MMOs, semi-auto weapons, clicker grinds. It's tiring and hard on your fingers.

With Spammy you just hold the key, and it keeps pressing for you as fast as you like. It works in any game, with no mods or scripts. Each game gets its own keys and speed, and Spammy switches them by itself when you switch games.

## What it does

- **Autofire**: hold a key and it repeats, from 5 to 100 presses per second
- **One tap**: a key sends a single quick press, no matter how long you hold it
- **Combos**: Shift, Ctrl and Alt + a key can do something different, or nothing at all
- **Locks**: stop the Win key and Alt+F4 from throwing you out of the game
- **Mouse buttons**: side buttons and the wheel click work too
- **Pause key**: switch Spammy off and on in one press, right from the game

## Getting started

1. Download `Spammy.exe` from [Releases](https://github.com/FrostAtom/Spammy/releases) and run it. Nothing to install.
2. Start your game and pick it from the list.
3. Click the keys you want on autofire, set the speed with the knob, and play.

## Building from source

Needs CMake, clang-cl or MSVC (x86) and [vcpkg](https://vcpkg.io) with Boost:

```powershell
cmake --preset release
cmake --build --preset release
```

## Credits

- [nlohmann/json](https://github.com/nlohmann/json) ([MIT](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT))
- [Boost.Container](https://github.com/boostorg/container) ([BSL-1.0](https://www.boost.org/LICENSE_1_0.txt))
- [Dear ImGui](https://github.com/ocornut/imgui) ([MIT](https://github.com/ocornut/imgui/blob/master/LICENSE.txt))
- [Archivo](https://github.com/Omnibus-Type/Archivo) font ([OFL](https://openfontlicense.org))

## License

[GPL-3.0](../LICENSE)
