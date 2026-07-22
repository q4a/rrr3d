# Статус порта RRR3D / Motor Rock на macOS

> **Исправленный активный статус:** Milestone 5–8 переделаны от исходных
> ресурсов. M5 отображает штатный Buggi через общий `.r3d` decoder и
> bgfx/Metal. M6 загружает оригинальные `MainMenu2` изображения и строки.
> M7 подключает к этому же меню SDL3 keyboard/mouse/gamepad input и общий с
> legacy `MainMenu2.cpp` порядок команд. M8 воспроизводит исходную музыку
> меню и штатный `ssButton1/click.ogg` через SDL3/CoreAudio, а также проверяет
> реальные игровые эффекты. Старые M6–M10 ниже остаются историей отменённого
> самостоятельного vertical slice и не являются acceptance status.

## Активный этап

Milestone 8: SDL3/CoreAudio для оригинального главного меню на bgfx/Metal.

## Активный статус

Preset `macos-arm64-m8` продолжает исправленный M7-путь и не использует
`PortableMenu`, `menu/menu.cfg` или `font5x7`. Навигация, pointer hit-testing,
gamepad hot-plug и dispatch команд продолжают работать через SDL-независимые
actions. Меню играет исходный `Track1.ogg`, а подтверждение использует тот же
`Sounds\\UI\\click.ogg`, что и legacy `Menu::ssButton1`. Подробный отчёт —
`docs/ORIGINAL_AUDIO_M8.md`. Игровой `fireGun.ogg` декодируется и проверяется,
но не проигрывается искусственно в меню: его настоящий consumer появится
вместе с гонкой.

## Исторический отчёт отменённого vertical slice

Milestone 4 был выполнен как аргументированный отказ от DXVK Native + MoltenVK backend.
Отдельный `rrr3d_dxvk_moltenvk_test` собирается и запускается на Apple Silicon,
но штатный DXVK 2.7.1 отклоняет MoltenVK device из-за отсутствия обязательного
`geometryShader`. Полный результат, extensions, backtrace, оценка и
альтернативы находятся в `docs/DXVK_MOLTENVK_FEASIBILITY.md`.

Portable core Milestone 3 не изменил поведение обычного
`macos-arm64-debug`: там renderer, physics, network, video и Steam остаются
явно disabled. Renderer включается только отдельными preset M5–M10.

Ветка `macos-arm64` основана на `dxvk-glm` commit
`bfbde0d245dcdb201640be9219f7bad58b2b0ed2`.

## Что сделано

- CMake создаёт настоящий `MACOSX_BUNDLE` с identifier
  `org.rrr3d.motorrock`, version 1.3.1, minimum macOS 13.0, Retina metadata,
  Bluetooth gamepad description и native arm64 priority.
- Полные original resources копируются в
  `Contents/Resources/game-data`; приложение продолжает писать только в
  Application Support и Logs. Bundle успешно запускается после перемещения в
  `/tmp` и получает resource root относительно собственного executable.
- Добавлена original-derived многоразмерная `RRR3d.icns`; статические
  SDL3/Ogg/Vorbis/bgfx/bx/bimg оставляют `Contents/Frameworks` пустым.
- Post-build очищает extended attributes и выполняет ad-hoc codesign. CMake
  также принимает Developer ID identity и hardened-runtime option для будущей
  notarized сборки.
- `verify_macos_bundle.sh` проверяет plist, arm64/minos, resources, strict
  signature и запрещённые dependencies на чистой transport-копии;
  `package_macos_bundle.sh` создаёт проверенный ZIP.
- Добавлен canonical Release preset и инструкция `docs/BUILDING_MACOS.md`.

- Добавлен SDL/Metal-независимый `physics/PhysicsBackend.h` с vehicle input,
  state, track sample и `PhysicsWorld`; portable capability теперь честно
  сообщает включённую физику только в preset M9.
- Проведён аудит PhysX 2.8.4: 717 Nx/PhysX usages в 27 engine/game files,
  scene/material/filter callbacks, runtime triangle/convex cooking,
  `NxWheelShape` suspension/tire API и прямой `NxActor` coupling gameplay.
- Выбран предусмотренный заданием minimal custom vehicle backend: fixed step
  120 Hz, acceleration/braking/drag, steering, corridor collision response,
  gravity/ramp/landing, reset и deterministic replay. Windows PhysX `.lib` в
  macOS link graph не входят.
- `PortableRace` читает 18 `ctTrack` placements и `numLaps` из оригинального
  `debugTrack.r3dMap`, проверяет track/car/pxTrack meshes и ведёт три
  checkpoints до финиша.
- `SINGLE PLAYER` запускает bgfx/Metal race scene с follow camera, road,
  barriers, car, start/checkpoint markers и scenery. Window title показывает
  скорость, progress, checkpoint и finish.
- SDL actions управляют машиной; P ставит physics/audio на паузу, Tab делает
  reset, Escape возвращает в меню. Engine loop, collision, weapon и finish
  используют оригинальные Ogg resources.
- `--physics-smoke-test` проверяет разгон, торможение, steering, collision,
  jump/landing, reset, replay и autonomous one-lap finish, затем 240 кадров
  полного SDL3/audio/bgfx/Metal runtime.

- В `Rock3dEngine` добавлен SDL-независимый `AudioBackend` с sound/voice
  handles, music/effects buses, volume, pause/resume, loop, unload и
  playback-device notifications. Legacy XAudio2/X3DAudio API в macOS target
  не попадает.
- `SdlAudioBackend` открывает default SDL playback stream, микширует voices в
  48 kHz stereo F32 и выводит через CoreAudio. SDL default stream сохраняется
  при смене системного устройства; added/removed/format events учитываются и
  логируются.
- Оригинальные Ogg/Vorbis декодируются через статические pinned libogg 1.3.5
  и libvorbis/libvorbisfile 1.3.7, затем SDL AudioStream выполняет channel
  conversion/resample. Homebrew audio dylib не используется.
- Меню запускает `Track1.ogg` в loop; hover/navigation/confirm используют
  исходные UI sounds, `UseWeapon` — `fireGun.ogg`, `Pause` управляет global
  audio state.
- Добавлены `--audio-smoke-test` и preset `macos-arm64-m8`. Тест проверяет
  decode музыки/UI/gameplay effects, callback mixing, volume, pause/resume,
  one-shot/loop, device events, stop/unload и 180 кадров общего runtime.

- В публичный portable API `Rock3dGame` добавлены SDL-независимые действия
  `Accelerate/Brake/TurnLeft/TurnRight/UseWeapon/ChangeWeapon/Pause` и
  `MenuUp/MenuDown/MenuConfirm/MenuBack`. Gameplay/menu code больше не должен
  получать SDL scancode или XInput напрямую.
- `SdlInputManager` переводит keyboard, mouse и SDL Gamepad events в action
  events с value, active/release, repeat, source и device id.
- M7 SDL собирается с нативными macOS joystick backends `hidapi`, `iokit`,
  `mfi`, `virtual`; libusb не используется. Existing controllers открываются
  при старте, а added/removed events поддерживают hot-plug.
- Left stick использует press threshold 0.55 и release/dead zone 0.25;
  gameplay steering нормализуется после dead zone, triggers имеют dead zone
  0.12. Добавлен безопасный rumble API.
- Mouse coordinates переводятся в virtual menu canvas: hover выбирает пункт,
  left click подтверждает, right click возвращает, wheel перемещает selection.
  Focus loss освобождает actions и сбрасывает stick hysteresis.
- Добавлены `--input-smoke-test` и preset `macos-arm64-m7`. Virtual-controller
  test проверяет keyboard/mouse/focus, hot-plug, axes, dead zones, D-pad,
  face buttons, triggers, rumble, hot-unplug и 120 кадров меню.

- Добавлен `ResourceFileSystem` с relative-only virtual paths, exact-case
  lookup, canonical/symlink containment, file-size bounds и конкретными
  ошибками отсутствующих/неверно названных ресурсов.
- Resource root больше не зависит от current working directory: обычный build
  использует каталог executable, будущий `.app` — `Contents/Resources`, а
  `--data-dir` оставлен как явный developer/test override.
- XPlatform создаёт writable каталоги `~/Library/Application Support/RRR3d`
  и `~/Library/Logs/RRR3d`; записи внутрь executable/resources не нужны.
- Добавлен воспроизводимый importer `scripts/import_game_resources.sh`. Он
  копирует `Data` и статические XML из установленной игры, исключает Windows
  binaries, логи и пользовательский профиль, затем создаёт точный каталог
  путей и размеров `legacy-assets.catalog`.
- `resources/game-data` теперь содержит 1196 оригинальных файлов игры
  (553107397 байт), versioned manifest, menu model и portable 5×7 bitmap font.
  Все parsers fail-fast валидируют формат, exact-case paths, размеры и
  cross-resource references до SDL init.
- Добавлен M6 bgfx UI pass с virtual canvas 1920×1100: оригинальные
  `mainFrame.dds`, `topPanel5.png`, `bottomPanel5.png` и `mainItemSel5.png`
  декодируются через bimg, загружаются в Metal, а пункты меню повторяют layout
  legacy `MainMenu2`. Работают Up/Down/Enter/Escape и Retina resize.
- `EXIT` работает; с M9 `SINGLE PLAYER` запускает portable race, остальные
  неперенесённые пункты честно сообщают о недоступности. Legacy `MainMenu2` не
  подменён фиктивными D3D9/audio/world объектами.
- Добавлены `--verify-resources`, `--smoke-test-frames` и отдельный preset
  `macos-arm64-m6`; подробности — `docs/PORTABLE_RESOURCES_AND_MENU.md`.

- Добавлен D3D9-независимый `r3d::renderer::GraphicsDevice` и ресурсные типы
  swap chain, shader, vertex/index buffers, texture, mesh, pipeline state,
  camera/transform, render/depth target, sampler, font renderer и effect.
- Реализован `BgfxGraphicsDevice`: Metal init/backend check, reset, shaders,
  indexed meshes, RGBA8 textures, view/model matrices, clear, depth/cull/blend
  state и frame submit.
- `RRR3d` получил M5 entry point со статической сценой из трёх объектов и
  пола, perspective camera, checker texture и resize-aware Retina backbuffer.
- Metal shaders собираются pinned `shaderc` во время build и встраиваются в
  executable; runtime shader files не требуются.
- bgfx/bx/bimg и bimg_decode зафиксированы точными commits и собираются для arm64/macOS 13
  воспроизводимым `scripts/build_bgfx_macos.sh`.
- Добавлен preset `macos-arm64-m5`; обычный `macos-arm64-debug` сохраняет
  renderer выключенным и C++17, а bgfx-конфигурация локально использует C++20,
  требуемый актуальным bx.
- Полная архитектура, mapping D3D9 API и границы следующей миграции описаны в
  `docs/BGFX_METAL_RENDERER.md`.

- Добавлен отдельный target `rrr3d_dxvk_moltenvk_test`; весь engine/game в
  experiment не втягивается.
- DXVK v2.7.1 зафиксирован commit-ом
  `c3dd74be6baec53786d4e064a572185b70347a17`, собирается Meson в отдельном
  каталоге скриптом `scripts/build_dxvk_macos.sh` только с D3D9 и SDL3 WSI.
- Минимальный Darwin patch исправляет loader names, POSIX compatibility,
  executable/thread APIs, Apple linker и portability enumeration/subset. Он не
  отключает обязательные DXVK features.
- Target печатает Vulkan version, полный список instance/device extensions и
  ключевые feature bits, затем пытается создать D3D9 device, clear и треугольник.
- SDL3, DXVK и MoltenVK dylib кладутся рядом с executable, install names
  переводятся на `@rpath`, комплект ad-hoc подписывается.
- Зафиксирован штатный отказ `geometryShader`; экспериментальное снятие проверок
  выявило `shaderCullDistance`, два robustness2 feature и
  `VK_KHR_pipeline_library`, после чего первый draw падает в MoltenVK.
- Принято решение не интегрировать DXVK в renderer; следующий кандидат — узкая
  abstraction с bgfx/Metal или SDL_GPU/Metal.

- В preset `macos-arm64-debug` включён `RRR3D_BUILD_GAME=ON`; стандартная
  preset-сборка теперь включает `Rock3dEngine`, `Rock3dGame` и `RRR3d`.
- Опция `RRR3D_ENABLE_RENDERER` по умолчанию остаётся `OFF`. На non-Windows
  её включение требует явного `RRR3D_RENDERER_BACKEND=bgfx`; отсутствие
  backend или pinned artifacts останавливает CMake понятной ошибкой.
- `Rock3dEngine` получил явный non-Windows source manifest. В него входят
  настоящий portable `ProgressTimer` и небольшой capability backend; D3D9 и
  PhysX каталоги в macOS target не попадают.
- `Rock3dGame` получил отдельный non-Windows source manifest и API-заглушку.
  Она сообщает, что world, network, video, Steam и audio недоступны, не создаёт
  поддельный `IWorld` и не маскирует отсутствие renderer/physics.
- `RRR3d` линкует `Rock3dGame`, вызывает capability API при запуске и продолжает
  использовать отдельную SDL3/Cocoa точку входа Milestone 2.
- Из публичной границы `IView`/`IWorld` убран обязательный `HWND`: оконный
  дескриптор представлен `rrr3d::platform::NativeWindowHandle` (`HWND` на
  Windows, opaque pointer на остальных платформах).
- Windows-only includes в precompiled headers защищены платформенными
  условиями. Export macro `Rock3dGame` поддерживает Windows DLL и обычную
  symbol visibility Clang/GCC.
- В публичных game headers исправлены недостающие namespace qualifiers и
  прямые зависимости, чтобы их можно было проверять без Windows `stdafx.h`.
- Homebrew GLM используется строго через `glm::glm-header-only`. Его optional
  dylib с minimum macOS 26 не попадает в link graph, поэтому target 13.0
  сохранён.
- Общие предупреждения остаются включены: `-Wall -Wextra -Wpedantic
  -Wno-unused-parameter`; `-fpermissive` не используется.

## Изменённые файлы Milestone 10

- Build/bundle: `CMakeLists.txt`, `CMakePresets.json`,
  `src/RRR3d/CMakeLists.txt`, `cmake/macos/Info.plist.in`.
- Artwork/notices: `resources/macos/AppIcon.png`,
  `resources/macos/RRR3d.icns`, `resources/macos/Licenses/*`,
  `resources/macos/README.md`.
- Verification/packaging: `scripts/verify_macos_bundle.sh`,
  `scripts/package_macos_bundle.sh`.
- Warning cleanup: `src/LexStd/header/lslAutoRef.h`.
- Documentation: `docs/BUILDING_MACOS.md`, resource/renderer/dependency docs и
  этот файл.

## Изменённые файлы Milestone 9

- Build: `CMakeLists.txt`, `CMakePresets.json`,
  `src/Rock3dEngine/CMakeLists.txt`, `src/Rock3dGame/CMakeLists.txt`,
  `src/RRR3d/CMakeLists.txt`.
- Physics/game: `src/Rock3dEngine/header/physics/PhysicsBackend.h`,
  `src/Rock3dEngine/source/physics/MinimalVehiclePhysics.cpp`,
  `src/Rock3dGame/include/PortableRace.h`,
  `src/Rock3dGame/source/stub/PortableRace.cpp`,
  `src/Rock3dGame/source/stub/PortableGame.cpp`.
- Runtime/render: `src/RRR3d/PortableRaceRenderer.h`,
  `src/RRR3d/PortableRaceRenderer.cpp`, `src/RRR3d/main_bgfx_menu.cpp`,
  `src/RRR3d/SdlInputSmoke.cpp`,
  `src/Rock3dEngine/source/stub/PortableEngine.cpp`.
- Documentation: `docs/PHYSICS_PORT_PLAN.md`, input/audio/resource docs и
  этот файл.

## Изменённые файлы Milestone 8

- Build/dependencies: `CMakeLists.txt`, `CMakePresets.json`,
  `cmake/dependencies.cmake`, `src/Rock3dEngine/CMakeLists.txt`,
  `src/RRR3d/CMakeLists.txt`.
- Portable boundary: `src/Rock3dEngine/header/audio/AudioBackend.h`.
- SDL implementation/tests: `src/RRR3d/SdlAudioBackend.h`,
  `src/RRR3d/SdlAudioBackend.cpp`, `src/RRR3d/SdlAudioSmoke.h`,
  `src/RRR3d/SdlAudioSmoke.cpp`, `src/RRR3d/main_bgfx_menu.cpp`.
- Documentation: `docs/PORTABLE_AUDIO.md`, `docs/PORTABLE_INPUT.md`,
  `docs/PORTABLE_RESOURCES_AND_MENU.md`, `docs/BGFX_METAL_RENDERER.md`,
  `docs/MACOS_DEPENDENCIES.md`, этот файл.

## Изменённые файлы Milestone 7

- Build: `CMakeLists.txt`, `CMakePresets.json`, `cmake/dependencies.cmake`,
  `src/RRR3d/CMakeLists.txt`, `src/Rock3dGame/CMakeLists.txt`.
- Portable actions: `src/Rock3dGame/include/PortableInput.h`.
- SDL implementation/tests: `src/RRR3d/SdlInputManager.h`,
  `src/RRR3d/SdlInputManager.cpp`, `src/RRR3d/SdlInputSmoke.h`,
  `src/RRR3d/SdlInputSmoke.cpp`, `src/RRR3d/main_bgfx_menu.cpp`.
- Documentation: `docs/PORTABLE_INPUT.md`,
  `docs/PORTABLE_RESOURCES_AND_MENU.md`, `docs/BGFX_METAL_RENDERER.md`,
  `docs/MACOS_DEPENDENCIES.md`, этот файл.

## Изменённые файлы Milestone 6

- Build: `CMakeLists.txt`, `CMakePresets.json`, `cmake/dependencies.cmake`,
  `cmake/bgfx.cmake`, `scripts/build_bgfx_macos.sh`,
  `scripts/import_game_resources.sh`,
  `src/CMakeLists.txt`, `src/RRR3d/CMakeLists.txt`,
  `src/Rock3dEngine/CMakeLists.txt`, `src/Rock3dGame/CMakeLists.txt`.
- Filesystem/platform: `src/XPlatform/header/xplatform.h`,
  `src/XPlatform/source/xplatform.cpp`,
  `src/Rock3dEngine/header/resource/ResourceFileSystem.h`,
  `src/Rock3dEngine/source/resource/ResourceFileSystem.cpp`.
- Menu/runtime: `src/Rock3dGame/include/PortableMenu.h`,
  `src/Rock3dGame/source/stub/PortableMenu.cpp`,
  `src/RRR3d/main_bgfx_menu.cpp`, `resources/game-data/*`.
- Documentation: `docs/PORTABLE_RESOURCES_AND_MENU.md`,
  `docs/BGFX_METAL_RENDERER.md`, этот файл.

## Изменённые файлы Milestone 5

- Build/dependencies: `.gitignore`, `CMakeLists.txt`, `CMakePresets.json`,
  `cmake/dependencies.cmake`, `cmake/bgfx.cmake`,
  `scripts/build_bgfx_macos.sh`.
- Renderer API/backend: `src/Rock3dEngine/header/renderer/*`,
  `src/Rock3dEngine/source/renderer/bgfx/*`,
  `src/Rock3dEngine/CMakeLists.txt`.
- Static scene/shaders: `src/RRR3d/main_bgfx.cpp`,
  `src/RRR3d/shaders/*`, `src/RRR3d/CMakeLists.txt`.
- Documentation: `docs/BGFX_METAL_RENDERER.md`,
  `docs/MACOS_DEPENDENCIES.md`, этот файл.

## Изменённые файлы Milestone 3

- Build: `CMakeLists.txt`, `CMakePresets.json`, `cmake/dependencies.cmake`,
  `src/Rock3dEngine/CMakeLists.txt`, `src/Rock3dGame/CMakeLists.txt`,
  `src/RRR3d/CMakeLists.txt`.
- Engine portable boundary: `src/Rock3dEngine/header/PortableEngine.h`,
  `src/Rock3dEngine/source/stub/PortableEngine.cpp`,
  `src/Rock3dEngine/header/stdafx.h`.
- Game portable boundary: `src/Rock3dGame/include/PortableGame.h`,
  `src/Rock3dGame/source/stub/PortableGame.cpp`,
  `src/Rock3dGame/include/Rock3dGame.h`, `src/Rock3dGame/include/IView.h`,
  `src/Rock3dGame/include/IWorld.h`, `src/Rock3dGame/header/stdafx.h`.
- Platform/executable integration: `src/XPlatform/header/xplatform.h`,
  `src/RRR3d/main_sdl.cpp`.
- Documentation: `docs/MACOS_DEPENDENCIES.md`,
  `docs/MACOS_PORT_AUDIT.md`, этот файл.

Изменения Milestones 1 и 2 в `XPlatform`, `LexStd`, `MathLib` и SDL shell
сохраняются в той же незакоммиченной рабочей ветке.

Файлы Milestone 4: `CMakeLists.txt`, `cmake/dependencies.cmake`,
`cmake/patches/dxvk-2.7.1-macos.patch`, `scripts/build_dxvk_macos.sh`,
`src/CMakeLists.txt`, `src/RendererSpike/*`,
`docs/DXVK_MOLTENVK_FEASIBILITY.md`, `docs/MACOS_DEPENDENCIES.md` и этот файл.

## Что собирается

```text
rrr3d_tinyxml   static library, arm64
rrr3d_ogg/rrr3d_vorbis/rrr3d_vorbisfile  static libraries, arm64
SDL3-static     static library, arm64, built for macOS 13.0
XPlatform       static library, arm64
LexStd          static library, arm64
MathLib         static library, arm64
Rock3dEngine    static portable-core library, arm64
Rock3dGame      static portable-API library, arm64
RRR3d           Mach-O 64-bit executable arm64, minos 13.0
rrr3d_dxvk_moltenvk_test  optional Mach-O executable arm64, minos 13.0
rrr3d_bgfx/bimg/bimg_decode/bx  optional M5–M10 static libraries, arm64, minos 13.0
shadercRelease  build-only arm64 Metal shader compiler
game-data        1196 validated original files (553107397 bytes); in M10 Contents/Resources
RRR3d.app        Debug/Release autonomous arm64 bundle, version 1.3.1, minos 13.0
```

`RRR3d` зависит только от Apple system libraries/frameworks. Ссылок на
Homebrew dylib, `/opt/homebrew`, `/usr/local`, build-directory dylib или
Windows `.lib` нет.

Проверенная среда: macOS 26.5.2 arm64, AppleClang 21.0.0, CMake 4.3.1,
Ninja 1.13.2, GLM 1.0.3. SDL3 3.4.12, TinyXML 2.6.2, libogg 1.3.5 и
libvorbis 1.3.7 собираются проектом из зафиксированных исходников.

## Что временно отключено

- `RRR3D_BUILD_MAP_EDITOR=OFF`;
- `RRR3D_ENABLE_RENDERER=OFF` в обычном preset;
- `RRR3D_ENABLE_NETWORK=OFF`;
- `RRR3D_ENABLE_VIDEO=OFF`;
- `RRR3D_ENABLE_PHYSICS=OFF` в M3–M8 и `ON` в M9/M10;
- `RRR3D_ENABLE_STEAM=OFF`;
- `RRR3D_ENABLE_GAMEPAD=OFF` в обычных M3/M5/M6 preset и `ON` в M7–M10;
- `RRR3D_ENABLE_AUDIO=OFF` в M3/M5/M6/M7 и `ON` в M8–M10;
- `RRR3D_ENABLE_DXVK_NATIVE=OFF`;
- `RRR3D_BUILD_DXVK_MOLTENVK_TEST=OFF` в обычном preset; target включается
  только отдельной feasibility-конфигурацией;
- в `macos-arm64-m5` renderer включён как `RRR3D_RENDERER_BACKEND=bgfx`, но
  legacy D3D9 renderer и gameplay scene graph не компилируются;
- legacy world/gameplay source set, D3D9/D3DX renderer, PhysX 2.8.4,
  full `.r3d` visual/material scene graph, DirectShow,
  WinMM/XAudio, legacy XInput implementation и полные игровые consumers
  форматов из `Data` (`.r3d`, video и UTF-16 localization); portable Ogg audio
  и узкий `debugTrack.r3dMap` race loader уже включены.

## Проверка

```bash
cmake --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug --clean-first -j 6
cmake --build --preset macos-arm64-debug --target \
  XPlatform LexStd MathLib Rock3dEngine Rock3dGame RRR3d -j 6
build/macos-arm64-debug/Debug/RRR3d --smoke-test-ms=750
lipo -info build/macos-arm64-debug/src/Rock3dEngine/libRock3dEngine.a
lipo -info build/macos-arm64-debug/src/Rock3dGame/libRock3dGame.a
lipo -info build/macos-arm64-debug/Debug/RRR3d
vtool -show-build build/macos-arm64-debug/Debug/RRR3d
otool -L build/macos-arm64-debug/Debug/RRR3d
git diff --check
```

Milestone 4 дополнительно:

```bash
scripts/build_dxvk_macos.sh
cmake --build build/macos-arm64-m4 \
  --target rrr3d_dxvk_moltenvk_test -j 6
DXVK_WSI_DRIVER=SDL3 DXVK_LOG_LEVEL=info DXVK_LOG_PATH=none \
  build/macos-arm64-m4/Debug/rrr3d_dxvk_moltenvk_test
```

Milestone 5:

```bash
scripts/build_bgfx_macos.sh
cmake --preset macos-arm64-m5
cmake --build --preset macos-arm64-m5 -j 8
build/macos-arm64-m5/Debug/RRR3d --smoke-test-frames=120
file build/macos-arm64-m5/Debug/RRR3d
vtool -show-build build/macos-arm64-m5/Debug/RRR3d
otool -L build/macos-arm64-m5/Debug/RRR3d
```

Milestone 6:

```bash
scripts/import_game_resources.sh '/Users/dmitrijcerednicenko/Downloads/Motor Rock'
cmake --preset macos-arm64-m6
cmake --build --preset macos-arm64-m6 -j 8
(cd /tmp && /absolute/path/to/build/macos-arm64-m6/Debug/RRR3d \
  --verify-resources)
build/macos-arm64-m6/Debug/RRR3d --smoke-test-frames=120
file build/macos-arm64-m6/Debug/RRR3d
vtool -show-build build/macos-arm64-m6/Debug/RRR3d
otool -L build/macos-arm64-m6/Debug/RRR3d
codesign --verify --deep --strict build/macos-arm64-m6/Debug/RRR3d
```

Milestone 7:

```bash
cmake --preset macos-arm64-m7
cmake --build --preset macos-arm64-m7 -j 8
(cd /tmp && /absolute/path/to/build/macos-arm64-m7/Debug/RRR3d \
  --verify-resources)
build/macos-arm64-m7/Debug/RRR3d --input-smoke-test
file build/macos-arm64-m7/Debug/RRR3d
vtool -show-build build/macos-arm64-m7/Debug/RRR3d
otool -L build/macos-arm64-m7/Debug/RRR3d
codesign --verify --deep --strict build/macos-arm64-m7/Debug/RRR3d
```

Milestone 8:

```bash
cmake --preset macos-arm64-m8
cmake --build --preset macos-arm64-m8 --clean-first -j 8
(cd /tmp && SDL_AUDIODRIVER=dummy \
  /absolute/path/to/build/macos-arm64-m8/Debug/RRR3d \
  --audio-smoke-test)
build/macos-arm64-m8/Debug/RRR3d --smoke-test-frames=90
file build/macos-arm64-m8/Debug/RRR3d
vtool -show-build build/macos-arm64-m8/Debug/RRR3d
otool -L build/macos-arm64-m8/Debug/RRR3d
codesign --verify --deep --strict build/macos-arm64-m8/Debug/RRR3d
```

Milestone 9:

```bash
cmake --preset macos-arm64-m9
cmake --build --preset macos-arm64-m9 --clean-first -j 8
(cd /tmp && SDL_AUDIO_DRIVER=dummy \
  /absolute/path/to/build/macos-arm64-m9/Debug/RRR3d \
  --physics-smoke-test)
build/macos-arm64-m9/Debug/RRR3d --verify-resources
file build/macos-arm64-m9/Debug/RRR3d
vtool -show-build build/macos-arm64-m9/Debug/RRR3d
otool -L build/macos-arm64-m9/Debug/RRR3d
codesign --verify --deep --strict build/macos-arm64-m9/Debug/RRR3d
```

Milestone 10:

```bash
cmake --preset macos-arm64-m10
cmake --build --preset macos-arm64-m10 --clean-first -j 8
scripts/verify_macos_bundle.sh \
  build/macos-arm64-m10/Debug/RRR3d.app

cmake --preset macos-arm64-release
cmake --build --preset macos-arm64-release --clean-first -j 8
scripts/verify_macos_bundle.sh \
  build/macos-arm64-release/Release/RRR3d.app

SDL_AUDIO_DRIVER=dummy \
  build/macos-arm64-release/Release/RRR3d.app/Contents/MacOS/RRR3d \
  --physics-smoke-test --smoke-test-frames=120
```

Результаты:

- clean M7 build: 282 шага, exit code 0; новые input sources собираются без
  предупреждений, остаются два старых Clang warning в `lslObject.h` о
  volatile increment/decrement;
- clean M6 build: 249 шагов, exit code 0; новые M6 sources собираются без
  предупреждений, остаются два старых Clang warning в `lslObject.h` о
  volatile increment/decrement;
- все три Milestone 3 artifact имеют архитектуру `arm64`;
- executable содержит `LC_BUILD_VERSION minos 13.0`;
- SDL video driver — `cocoa`, smoke test завершился с exit code 0;
- при startup залогированы явные состояния renderer/physics/network/video/
  Steam/audio, без попытки создать gameplay world;
- resize, keyboard, UTF-8 text, mouse и focus smoke events Milestone 2 по-прежнему
  проходят.
- renderer spike и три staged dylib — arm64; executable и DXVK имеют minimum
  macOS 13.0; install names используют `@rpath`; codesign verification проходит;
- Vulkan/MoltenVK и SDL3 WSI инициализируются, Apple M3 Max перечисляется;
- штатный DXVK отказ воспроизводится на обязательном `geometryShader`, target
  возвращает диагностический exit code 2.
- bgfx сообщает runtime backend `Metal`; 120-frame smoke test завершается с
  exit code 0;
- статическая сцена визуально проверена: perspective, texture, indexed meshes,
  depth occlusion и clear работают корректно;
- bgfx, bimg, bx и SDL3 линкуются статически; внешних Homebrew dylib нет.
- M6 resource-only verification из `/tmp` находит пакет рядом с executable,
  подтверждает 1196 исходных файлов и 553107397 байт и завершается с exit code
  0; отсутствующий root и повреждение catalog завершаются с exit code 1 до SDL
  init;
- M6 runtime сообщает `bgfx/Metal`, 120-frame smoke test завершается с exit
  code 0, визуальная проверка подтверждает оригинальные DDS/PNG, bitmap text и
  переход selection с `SINGLE PLAYER` на `MULTIPLAYER`;
- M6 executable — `Mach-O 64-bit executable arm64`, `minos 13.0`, codesign
  verification проходит, зависимости только системные;
- повторные M3 SDL smoke и M5 scene smoke завершаются с exit code 0.
- M7 SDL configuration включает joystick/HIDAPI и backends `hidapi`, `iokit`,
  `mfi`, `virtual`, при этом `SDL_HIDAPI_LIBUSB=OFF`;
- virtual gamepad smoke проверяет hot-plug/hot-unplug, stick dead
  zone/hysteresis, steering, trigger, D-pad, confirm/back и rumble; затем M7
  menu рендерит 120 кадров через Metal с exit code 0;
- реальный Cocoa event path проверен: keyboard Down/Enter активировал
  `MULTIPLAYER`, mouse hover/click изменил selection и активировал `SETTINGS`;
- M7 resource/input smoke из `/tmp` находит пакет рядом с executable и
  завершается с exit code 0; M6, M5 и M3 regression smoke остаются зелёными.
- M7 executable — `Mach-O 64-bit executable arm64`, `minos 13.0`; codesign
  verification проходит, зависимости ограничены системными libraries и
  Apple frameworks, включая GameController/ForceFeedback/CoreHaptics.
- clean M8 build: 312 шагов, exit code 0; новые audio/codec sources собираются
  без предупреждений, остаются только два legacy warning в `lslObject.h`;
- M8 SDL configuration включает `Audio: ON` и audio backends `coreaudio`,
  `disk`, `dummy`; joystick/HIDAPI и Metal остаются включены;
- бесшумный audio smoke из `/tmp` декодирует original Track1/UI/fireGun Ogg,
  проверяет mixing, buses/volume, pause/resume, one-shot/loop, device events,
  release и 180 кадров menu с exit code 0;
- реальный runtime использовал `SDL3/coreaudio`, default output
  `MacBook Pro Speakers` и отрисовал 90 кадров bgfx/Metal menu;
- M8 executable и codec/static game archives — arm64, executable имеет
  `minos 13.0`, strict codesign проходит; link graph содержит только системные
  Apple libraries/frameworks, включая CoreAudio и AudioToolbox;
- после M8 clean M7 virtual gamepad smoke и M6 120-frame menu smoke повторно
  завершились с exit code 0; M7 SDL Audio по-прежнему `OFF`.
- M9 build: 315 шагов, exit code 0; новые physics/race sources собираются без
  предупреждений, остаются два legacy warning в `lslObject.h`;
- M9 smoke прошёл acceleration/braking/steering, forced wall collision,
  ramp jump/landing, reset, deterministic replay и one-lap finish за
  17.8581 s с max 89.7191 km/h, затем 240 Metal frames; exit code 0;
- M9 загрузил 18 placements `debugTrack`, два `pxTrack` collision resources,
  original engine/crash audio и сообщил backend `bgfx/Metal` + portable
  fixed-step physics.
- clean M10 Debug и Release builds завершены с exit code 0; финальная Release
  сборка с icon/licence resources — 327 шагов. Остаются только два legacy
  warning в `lslObject.h` о volatile increment/decrement;
- оба `RRR3d.app` содержат arm64-only Mach-O с `minos 13.0`, version 1.3.1,
  identifier `org.rrr3d.motorrock`, Retina metadata, `.icns`, пустой
  `Frameworks` и 1200 файлов в Resources;
- M10 verifier подтвердил strict ad-hoc signature и отсутствие ссылок на
  Homebrew, `/usr/local`, build directory, Windows `.lib/.dll` и любую
  внешнюю абсолютную runtime dependency;
- Debug bundle прошёл полный physics + virtual input + audio + 240 Metal frame
  smoke. Release bundle прошёл physics/audio + 120 Metal frame smoke;
- чистая копия bundle была перемещена в `/tmp` и запущена через macOS
  LaunchServices (`open -W -n`): game-data найден внутри перемещённого
  `Contents/Resources`, гонка стартовала, 120-frame smoke завершился с кодом 0.

## Известные проблемы

- `Rock3dEngine` пока не является полным legacy engine target: M10 включает
  renderer, resources, audio boundary и minimal physics, но D3D9/PhysX части
  остаются изолированы.
- `Rock3dGame` теперь предоставляет portable menu и race vertical slice, но не
  полный legacy runtime. Его world/car/object/weapon код напрямую зависит от
  D3D9 и PhysX 2.8.4.
- `RecordLib` не включён в portable game target. Пробное подключение выявило
  MSVC-зависимые template lookup и спорную protected reference-counting
  границу в legacy serialization; это следует исправлять отдельным небольшим
  изменением, а не ослаблять Clang через `-fpermissive`.
- В обычном M3 preset SDL events заканчиваются в diagnostic shell; M7 menu и
  M9/M10 portable race используют action layer, но legacy `ControlManager` ещё не
  является его consumer.
- DXVK Native/MoltenVK проверен и отклонён. Новый bgfx backend пока покрывает
  M5–M10 vertical slices; legacy textures/materials/fonts/effects требуют
  поэтапных adapters, а не прямого включения D3D9 headers.
- Оригинальные game assets импортированы в `game-data`; M9 использует карту и
  audio, но binary `.r3d` visual/material parser ещё не подключён. Перед
  публикацией нужно отдельно проверить права на распространение данных.
- Длинная музыка M8 пока декодируется целиком; `Track1.ogg` занимает примерно
  92 MiB PCM. Streaming/ring buffer остаётся оптимизацией. Spatial X3DAudio
  emitters/listener не перенесены; M9 race events используют обычный effects
  bus без 3D attenuation.
- Физическое переключение Bluetooth/USB playback device не выполнялось;
  SDL default-device migration и event path нужно повторить на release hardware.
- Численная parity с legacy PhysX не доказана: в доступной Apple Silicon среде
  нет Windows/PhysX reference replay. Отличия и обязательный baseline описаны
  в `docs/PHYSICS_PORT_PLAN.md`.
- Стандартный M10 bundle имеет только ad-hoc подпись: для распространения без
  Gatekeeper warning нужны Developer ID, hardened runtime, notarization и
  проверка на отдельной чистой машине. CMake options для подписи подготовлены,
  но реальные credentials в проект не входят.
- Рабочая папка Codex находится под file provider, который может повторно
  прикреплять Finder xattr к build artifact. Verifier поэтому проверяет
  xattr-free transport copy; ZIP packager также исключает эти metadata.
- Windows regression build требует Windows CI.

## Следующий рекомендуемый этап

Post-M10: добавить GitHub Actions macOS arm64 job с Debug/Release bundle smoke
и artifact upload, затем выполнить Developer ID/notarization на release
credentials и длительный stability run на чистой macOS 13–15 машине. Для
physics quality отдельно снять Windows reference replay и решить, достаточно
ли minimal vehicle backend или нужен Jolt adapter и полный `.r3d` importer.
