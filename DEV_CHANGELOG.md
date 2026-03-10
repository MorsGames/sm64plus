This is the changelog for the current development commit of the game. For the changelog for the public versions, please check the Wiki.

# 🟩v4.0.0 (10/03/2026):

## New Settings:
- Added the "V-Sync" setting to prevent screen tearing.
- Added the "Anti-Aliasing" setting, which uses MSAA to smooth out jagged edges (similarly to how the N64 does it).
- Added the "Anisotropic Filtering" setting, which improves the quality of textures at oblique angles.
- Added the "N64 Blur" setting, which adds a simple blur effect to make the game look more like the original N64 version.
- Added the "Fix Text Typos" setting, which fixes various typos in the original game's text, including (but not limited to) Yoshi's infamous "Mario!!! It that really you???".
- Added the "Improved C-Button Camera" setting, which allows for more precise camera rotation using the C-Buttons.
- Added the "Block Non-XInput Controllers" setting, which disables support for non-XInput controllers, since they can cause conflicts with certain remapping software (like BetterJoy).

### Changes:
- Dropped the Direct3D 11 and Direct3D 12 backends. The game now exclusively uses OpenGL for graphics and SDL2 for windowing.
- Replaced the clunky "Custom Fullscreen Resolution" settings with the "Fullscreen Display Mode" setting. Same idea, but way more intuitive.
- The "Internal Resolution" setting is completely overhauled. It's now a simple multiplier instead of requiring separate width and height values. In addition, it now uses nearest neighbor filtering for upscaling and works on OpenGL.
- Replaced the "Force 4:3" setting with the "Aspect Ratio" setting, which now also allows you to force a 16:9 aspect ratio.
- Improved how the game handles the 4:3 aspect ratio. The game no longer renders black bars and adjusts rendering resolution accordingly.
- The game now supports `gamecontrollerdb.txt` and comes with it by default for better controller support.
- Renamed the "Input Mapping" and "Key Mapping" settings sections to "Controller Mapping" and "Keyboard Mapping" respectively.
- Removed the "Fix Exploits" setting and merged it into the "Apply Bug Fixes" setting, which was renamed to "Fix Gameplay Bugs". Take a shot each time I say the word "setting".
- Made the "Fix Gameplay Bugs" setting also fix the slightly broken jingle that plays when you talk to Yoshi.
- Switched to a more accurate frame limiter that will hopefully reduce stuttering, at the cost of a slight CPU usage increase.
- Moved the "Make Secrets Visible" setting to the "Cheats" section.
- Moved the "Mouse" section to the end.
- Removed the "Wireframe Mode" setting, as it only worked with the Direct3D 11 backend.
- Rewrote the descriptions for a vast majority of the settings.
- Adjusted all the presets. Now, the "Recommended" preset renders the game at a lower internal resolution by default, though you can always adjust the settings to your liking. Also disabled rumble in the "Vanilla" preset, since the original North American version of the game didn't have rumble support.

### Fixes:
- Fixed the lighting being wrong on Mario's head at the beginning.
- Fixed the longstanding issue of certain UI elements being mirrored in Encore Mode.
- Fixed the console output not being present on Windows.
- Fixed the game icon not being present on the build itself on Windows.
- Fixed the game not being DPI aware on Windows (at least with the OpenGL backend).
- Fixed the mouse wheel input potentially not getting registered when spinning too fast.
- Fixed some potential performance issues with the way the game handles the mouse input.