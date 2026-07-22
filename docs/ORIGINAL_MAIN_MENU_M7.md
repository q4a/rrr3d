# Milestone 7: SDL3 input for original MainMenu2

## Acceptance path

The `macos-arm64-m7` preset extends the corrected Milestone 6 target. It keeps
`RRR3D_BUILD_ORIGINAL_MENU=ON`, leaves `RRR3D_BUILD_PORTABLE_MENU=OFF`, and
adds SDL3 keyboard, mouse and native macOS gamepad input to the original
`MainMenu2` presentation on bgfx/Metal.

Input is split at an explicit renderer/platform boundary:

```text
SDL3 events
  -> SdlInputManager
  -> renderer-independent InputActions
  -> MainMenu2 Controller
  -> MainMenu2Spec itemCommands
  -> original MainMenu2 command order
```

`InputActions.h` is the canonical SDL-independent interface. The older
`PortableInput.h` remains only as a compatibility include, so existing code
does not gain a second action model.

## Original command ownership

`MainMenu2Spec.h` defines the five commands in the exact shipped item order:

1. `SinglePlayer`
2. `Network`
3. `Options`
4. `Authors`
5. `Exit`

Both the macOS controller and the original Windows `MainFrame::OnClick` use
this same array. The Windows implementation still performs its original
state transitions (`msGameMode`, `msNetwork`, options, final/credits and
terminate); only the repeated sender-to-command mapping was moved into the
shared specification.

The macOS target changes selection and dispatches these commands. `Exit` and
`Back` close the current top-level menu. The destination screens for the
other four commands are outside Milestone 7 and are therefore reported but
not replaced by invented menus or gameplay.

## Device mappings

- Keyboard: Up/Down or W/S navigate; Return, keypad Enter or Space confirms;
  Escape or Backspace goes back.
- Mouse: motion performs hit-testing in the original 1920x1100 virtual menu
  canvas, left click confirms only an item under the pointer, right click goes
  back, and the wheel changes selection.
- Gamepad: D-pad and left-stick vertical axis navigate; South or Start
  confirms, and East or Back returns. Devices present at startup and SDL
  hot-plug events are both handled.
- The left stick uses separate 0.55 press and 0.25 release thresholds to avoid
  repeated direction changes around the dead zone. Focus loss releases active
  actions and resets axis hysteresis.
- SDL rumble is exposed safely for devices that support it. The M7 test uses
  SDL's virtual gamepad API, so hot-plug and rumble are exercised without
  requiring a physical controller in CI.

SDL3 is built with its native macOS HIDAPI, IOKit and MFi joystick drivers;
there is no XInput or DirectInput dependency in the M7 target.

## Original resources retained

This milestone uses the same verified resource path as M6:

- 1,196 copied original files / 553,107,397 bytes;
- 248 decoded `Data/GUI` images and four strict-decoded GUI meshes;
- the original `mainFrame.dds`, `topPanel5.png`, `mainItemSel5.png` and cursor;
- localized `MainMenu2` strings from the shipped UTF-16LE language file;
- Verdana through CoreText at the legacy sizes.

`PortableMenu`, `menu/menu.cfg`, `ui/font5x7.txt` and replacement artwork do
not participate in the executable.

## Build and verification

```bash
cmake --preset macos-arm64-m7
cmake --build --preset macos-arm64-m7 -j 8
build/macos-arm64-m7/Debug/RRR3d --language=russian --verify-resources
build/macos-arm64-m7/Debug/RRR3d --language=english --verify-resources
build/macos-arm64-m7/Debug/RRR3d --language=russian --input-smoke-test
```

The input smoke test covers keyboard translation, mouse wheel/button input,
focus reset, virtual gamepad hot-plug/hot-unplug, axes, hysteresis and dead
zones, D-pad and face buttons, triggers, rumble, menu selection bounds,
legacy command order, and 120 frames of the integrated bgfx/Metal menu.

Milestone 5 and Milestone 6 presets are rebuilt and smoke-tested as regression
checks before the M7 checkpoint is committed.

## Boundary

Milestone 7 proves event translation, selection, hit-testing and command
dispatch on the original M6 menu/resource path. It does not claim that the
complete D3D9-era `gui::Manager` graph or the command destination screens are
running on macOS. Future milestones must port those real systems across the
same seams; they must not reintroduce the cancelled portable vertical slice.
