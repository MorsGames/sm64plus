# Super Mario 64 Plus

Super Mario 64 Plus is a fork of [sm64-port](https://github.com/sm64-port/sm64-port) that focuses on customizability and aims to add features that not only fix some of the issues found in the base game but also enhance the gameplay overall with extra options.

These features include (but not limited to):
- More responsive controls
- Improved camera
- Extended moveset
- The ability to continue the level after getting a star
- Optional extra modes
- 60FPS support via interpolation
- Various bug fixes

Download the launcher from MFGG: https://www.mfgg.net/?act=resdb&param=02&c=2&id=38190
If you need support, please head to the Super Mario Flashback official Discord server: http://discord.mors-games.com/

## FAQ

Q: Will there be actual questions here soon?

A: Yes.

## Keep in mind...

This repo does **not** include all the assets necessary for compiling the game. A prior copy of the game is required to extract the assets. Binaries of the game will **not** be distributed for this very reason.

That being said, there is an official launcher that will help you build the game and configure its settings easily. You can download it from [MFGG](https://www.mfgg.net/?act=resdb&param=02&c=2&id=38190). It is for 64-bit Windows systems only.

You can also build the game manually if you prefer to do so, or if you're not on a Windows platform. To do that, follow the following instructions.

Features that _might_ get added in the future:
- Discord Rich Presence support
- Smoother vertical camera movement
- Rumble
- Odyssey-like rolling
- Custom texture pack support
- Full OpenGL and SDL support

## Manual Building

### Windows

1. Install and update MSYS2, following the directions listed on https://www.msys2.org/.
2. Launch MSYS2 MinGW and install required packages depending on your machine (do **NOT** launch "MSYS2 MSYS"):
  * 64-bit: Launch "MSYS2 MinGW 64-bit" and install: `pacman -S git make python3 mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-glew`
  * 32-bit (untested, but should also work on 64-bit machines): Launch "MSYS2 MinGW 32-bit" and install: `pacman -S git make python3 mingw-w64-i686-gcc mingw-w64-i686-SDL2 mingw-w64-i686-glew`
  * Do not install `gcc`.
3. Clone the repo with `git clone https://github.com/MorsGames/sm64-port.git`, then enter it with `cd sm64-port`.
4. Place a *Super Mario 64* ROM called `baserom.us.z64` into the project folder for asset extraction.
5. Run `make` to build. You can add `-j4` to improve build speed (hardware dependent based on the amount of CPU cores available).
6. The executable binary will be located at `build/us_pc/sm64.us.f3dex2e.exe`.

## Credits.
- **Mors:** Most new things you see here.
- **Benial:** Logo.
- **[sm64-port](https://github.com/sm64-port/sm64-port) Team:** The port that was used as a base for this project.
- **[A bunch of clever folks](https://github.com/n64decomp/sm64):** The original decompilation used for the port.
- **Emil:** The original 60FPS patch.
- **Kaze Emanuar:** Providing certain bug fixes.

Parts of [sm64ex](https://github.com/sm64pc/sm64ex) were also used as a reference for this project.

Special thanks to Superstarxalien, Benial, Triforce141, MrMovie, and Shubs for testing and feedback.

Please let me know if I'm forgetting to credit you.
