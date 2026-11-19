# Apparel Preview

See armour, clothing and jewellery on your character before you equip it,
straight from the menu. SKSE plugin for Skyrim Special Edition and Anniversary
Edition.

https://www.nexusmods.com/skyrimspecialedition/mods/185334

## Requirements

* [CMake](https://cmake.org/)
	* Add this to your `PATH`
* [Vcpkg](https://github.com/microsoft/vcpkg)
	* Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
* [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
	* Desktop development with C++

## User Requirements

* Skyrim SE 1.5.97, or AE 1.6.317 and later. Support for 1.7.104 is in testing. VR is not supported.
* [SKSE64](https://skse.silverlock.org/) for your runtime
* [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
	* The SE or the AE database, whichever matches your game
* [Show Player In Inventory](https://www.nexusmods.com/skyrimspecialedition/mods/178689) or [Show Player In Menus](https://www.nexusmods.com/skyrimspecialedition/mods/81291), so there is a character on screen to preview on
* [FLICK](https://www.nexusmods.com/skyrimspecialedition/mods/181603), optional, for the in-game settings panel. Everything is configurable through the INI without it.
* [Dynamic Inventory Icon Injector](https://www.nexusmods.com/skyrimspecialedition/mods/174136), optional, for the gold eye icon on previewed rows

## Follower preview (fork feature)

When a container menu is open on one of your followers (the "Trade" screen), the
preview key now shows the armour on the **follower** instead of on you. Controlled
by `bPreviewOnFollower` in `ApparelPreview.ini` (default `true`). The follower has
to be visible on screen; the preview ends when you close the menu, and the
follower's real outfit is restored.

## Building

### On GitHub (no local Visual Studio)

Open the **Actions** tab, pick **Build ApparelPreview**, press **Run workflow**.
When it finishes, download the `ApparelPreview-FollowerPreview` artifact: it has
`SKSE/Plugins/ApparelPreview.dll` ready to drop into your mod folder.

### Locally

```
git clone --recurse-submodules https://github.com/maartenharms/apparel-preview.git
cd apparel-preview
cmake --preset release
cmake --build build/release
```

## License

[GPL-3.0](LICENSE)
