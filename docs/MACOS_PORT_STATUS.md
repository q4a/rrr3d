# Статус порта RRR3D / Motor Rock на macOS

> **Актуальное заключение ревизии:** milestones 5–10 создали нативный arm64
> bundle и source-driven vertical slice на оригинальных ресурсах, но не
> доказали полный перенос Windows-игры. Исходные `Rock3dGame/source/game`
> классы в macOS target не компилируются; меню, session, AI, weapons, HUD и
> renderer частично воспроизведены новыми adapters, а network/video/Steam
> выключены. Каноническая матрица «перенесено / частично / суррогат / не
> перенесено» находится в
> [`MACOS_PORT_COMPLETENESS_AUDIT.md`](MACOS_PORT_COMPLETENESS_AUDIT.md).
> Приведённые ниже milestone-отчёты описывают реализованное покрытие и историю
> сборки, но больше не считаются заявлением о feature parity.

## Активный этап

Milestone 10: автономный arm64 Debug/Release `RRR3d.app` на исправленном
M5–M9.5 original-data пути.

## Активный статус

Preset `macos-arm64-m9` продолжает исправленный M8-путь и не компилирует
`PortableMenu`, `PortableRace` или `MinimalVehiclePhysics`. `Single Player`
по умолчанию читает map1/Marauder из `tournamet.xml`, `map1.r3dMap`, `db.xml`
и `garage.xml`; bgfx/Metal рисует 52 `ctTrack`, 234 `ctDecoration`, 7 bonuses,
шесть машин и их колёса. Jolt получает 1175 collision triangles и параметры
каждого автомобиля. Countdown, trace/laps/finish, reset/respawn, original
HUD/mini-map/camera, полный workshop catalog, damage/bonuses/achievements,
projectiles/mines/hyper/support, AI и source effect graph работают в portable
race session. MusicCat ставится на паузу, а race sound graph и idle/RPM loops
получают spatial attenuation/pan/pitch. `--track`, `--car` и `--weather`
выбирают остальные исходные данные.
Подробности M9.1 находятся в `docs/PHYSICS_PORT_PLAN.md`; последующие
renderer stages описаны в `docs/MILESTONE_9_2.md`,
`docs/MILESTONE_9_3.md`, `docs/MILESTONE_9_4.md` и
`docs/MILESTONE_9_5.md`; упаковка и приёмка — в
`docs/MILESTONE_10.md`.

Follow-up полной ревизии от 2026-07-30 заменил generic Options pages
исходной структурой `OptionsMenu.cpp`: единый модальный экран использует
оригинальные фон, строки, стрелки, полосы, key/button backgrounds и координаты
четырёх вкладок. Работают все Game/Media/Network/Controls строки, реальный
список display modes, обе колонки 18 control actions, scroll, live volume
preview и исходные Apply/Cancel draft semantics. Этот блок не означает, что
остальные generic `GameMode`/`RaceMenu`/`FinishMenu` уже перенесены.

Следующий follow-up заменил вертикальный generic `RaceMenu` главным экраном
`RaceMenu2::RaceMainFrame`: исходные верхняя/нижняя панели, семь горизонтальных
icon-кнопок, money/stat/image frames, weather и charge bars используют
оригинальные координаты и tournament/profile data. Это пока частичный перенос:
на этом шаге исходная garage 3D scene `CarFrame` и subframes
Garage/Workshop/Angar/Achievements ещё отсутствовали.

Следующий follow-up заменил generic garage page двумерным
`RaceMenu2::GarageFrame`. Загружаются исходные панели, кнопки, стрелки,
карточки всех машин, lock/car/color boxes и обе палитры `Player.cpp`;
доступные, secret и locked машины сортируются и фильтруются по исходным
tournament/achievement rules. Покупка использует исходный confirm flow,
skirmish открывает полный допустимый каталог, выбранный цвет сохраняется.
В `OriginalGarage` также перенесены из `Race.cpp` точные вычисления
armor/damage/speed на данных `workshop.xml`, а не условные полосы. Блок
оставался частичным до переноса 3D `CarFrame`.

Следующий follow-up перенёс исходный 3D `RaceMenu2::CarFrame`. Runtime
загружает `Misc/garage`, `Misc/question`, все 17 source `ctCar`, штатные
колёса и default Weapon1–4; цвет берётся из профиля, а locked car заменяется
вращающимся `question.r3d`. Camera quaternion/FOV/near/far, две spot-lamps,
ambient/sky/fog flags и HDR constants взяты буквально из Windows-кода.
Добавлена поддержка source GUI materials: `GUI/question` использует
`question.png` и точный `LoadSpecLibMat` state. Metal smoke требует реальную
отрисовку 3D scene. Остаются Angar/Achievement subframes и отдельные
shadow maps двух garage spot-lamps.

Следующий follow-up заменил придуманные раздельные Workshop pages исходным
`RaceMenu2::WorkshopFrame`. Один экран использует `topPanel3`,
`bottomPanel3`, `leftPanel3`, 3×4 goods grid, десять source slots, money/stat
panels, slot/level/charge controls и `dlgFrame3`. Все goods/installed/drag
preview загружают настоящие `.r3d` и DDS/PNG из `workshop.xml`; fitting,
`Menu::GetIsoRot`, Y-flip, depth clear и вращение повторяют `ViewPort3d`.
Перенесены tournament assortment, price sort/scroll, `CarFrame::csSlots`,
совместимость placements, buy/sell confirmations, drag/install/swap/refund,
50% resale с charge value, recharge и mobility upgrades. Stats теперь
считаются по фактически установленным profile slots и показывают source
bonus preview. Physics smoke проверяет транзакции, а 240-frame Metal smoke
обязан посетить Workshop и подтвердить его 2D/3D draw до Garage и гонки.
Новая кампания снова проходит исходный `Profile::Reset →
SnProfile::EnterGame`: первая планета становится `psOpen/pass 1`, поэтому
`CompletePass(0)` формирует стартовый ассортимент. Профили, испорченные
ранним macOS-кодом сочетанием `psUnavailable/pass>0`, при загрузке
нормализуются в `psOpen` без потери прогресса.

Следующий follow-up перенёс `RaceMenu2::SpaceshipFrame/AngarFrame`: сцена
ангара и космоса читается из `db.xml`, шесть планет и boss data — из
`tournamet.xml`; восстановлены исходные lamps/HDR/camera, doors, planet
ViewPort3d, photo/car info и Stay/Fly transitions.

После него generic-список наград заменён исходным
`RaceMenu2::AchievmentFrame`. Экран использует `achievmentBg.dds`,
`achievmentPanel.png`, нижнюю панель, девять точных locked/open reward
изображений и координаты из `UpdateAchievments`. `armor4` намеренно
показывается оригинальной карточкой `musicTrack`. Перенесены состояния
`asLocked/asUnlocked/asOpened`, цены и points из `achievment.xml`, исходный
четырёхнаправленный navigation graph с пропуском locked-кнопок, подтверждение
покупки, `ConsumePoints`, предупреждение о нехватке очков и сохранение
`asOpened`. Интеграционный Metal smoke обязан посетить этот frame до гонки;
ручная проверка arm64 Debug подтвердила геометрию, диалог и warning.

Финальная проверка M9.5: arm64 Debug build и новые Metal shaders прошли без
новых warnings. World1/World2/World5/World4 Cocoa smokes прошли по 240 кадров;
каждый подтвердил оба 2048 shadow split, шесть cube faces, `glRefl` и FxTrail,
а World2 дополнительно дал 768 draw submissions с исходными normal maps.
Physics smoke проверил исходные idle RPM, torque/transmission/rest-brake и
steering rules. Parallels не использовался. Подробные результаты находятся в
`docs/MILESTONE_9_5.md`.

Bundle M9.5 формируется строго по `legacy-assets.catalog`: 1196 исходных и
четыре portable-файла. Незатреканные File Provider-копии с суффиксом ` 2` в
`.app` и ZIP не попадают; verifier требует ровно 1200 файлов.

Follow-up проверен вручную через штатный путь `Single Player -> Tournament ->
Continue -> Start race`: колёса Marauder и AI сохраняют исходный масштаб и
вращаются без увеличенных preview meshes. Исправление additive blending убрало
белые грани от прозрачных DDS-texels; после старта и на первом повороте кадр
чистый, а тёмные wheel trails отображаются корректно. Physics, resource,
240-frame race-render и bundle verification завершились с exit code 0.

Исправленный M10 собран в Debug и Release. Оба bundle проходят усиленный
verifier: `APPL`, version 1.3.1, arm64-only, minos 13.0, strict ad-hoc
signature, пустой `Frameworks`, системные runtime dependencies и ровно 1200
разрешённых game-data файлов. Оба варианта прошли resource, physics,
input/audio/MusicCat и 240-frame M9.5 race smoke. Release ZIP извлечён в
`/private/tmp`; перемещённый `.app` нашёл ресурсы внутри себя, прошёл гонку и
отдельно успешно завершил запуск через LaunchServices. Хеши и полный протокол
находятся в `docs/MILESTONE_10.md`.

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

- Добавлен SDL/Metal-независимый `physics/OriginalVehiclePhysics.h`; наружу из
  engine не выходят Jolt-типы, а Windows продолжает использовать PhysX 2.8.4.
- Проведён аудит PhysX 2.8.4: 717 Nx/PhysX usages в 27 engine/game files,
  scene/material/filter callbacks, runtime triangle/convex cooking,
  `NxWheelShape` suspension/tire API и прямой `NxActor` coupling gameplay.
- Jolt Physics 5.5.0 зафиксирован commit и digest, собирается статически только
  для non-Windows original M9. Adapter использует legacy Z-up gravity `-20`,
  fixed step 120 Hz, triangle collision и четыре wheel/suspension constraints.
- `OriginalRace` читает 88 tracks, planet/pass rosters и 17 garage cars. Map1
  содержит 52 track, 234 decoration и 7 bonus placements; visual/collision
  paths и vehicle parameters разрешаются из `db.xml`, `garage.xml` и `.r3d`.
- `Single Player` запускает bgfx/Metal scene с настоящими track/decor/bonus/car
  meshes, DDS materials, map lighting/fog/sky/rain и камерой по исходным
  `CameraManager` константам.
- Race runtime не применяет `Data/Upgrade/wheel*.r3d` как колёса автомобиля:
  как и в legacy `WheelItem`, эти крупные meshes используются только для
  workshop preview. Контакт, slip и wheel trails получают состояние каждого
  Jolt-колеса.
- Bgfx additive pipeline повторяет исходный `Material::ApplyBlending`:
  `SRC_ALPHA, ONE`, а не bgfx shortcut `ONE, ONE`. Поэтому RGB под прозрачными
  texels исходных DDS больше не проявляется белыми полигонами.
- `OriginalRaceSession` обслуживает countdown, checkpoints/laps/place/finish,
  wrong-way, reset/respawn, пять AI, life/damage/shield, bonuses,
  achievements, четыре workshop slots, projectiles/mines/hyper/support и
  destructible decoration state.
- Оригинальные HUD images/layout, mini-map trace, обе race cameras и
  notifications работают через bgfx/Metal. SDL actions управляют машиной,
  четырьмя слотами, mine/hyper, камерой, reset и pause.
- MusicCat приостановлена, а original race cues и idle/RPM loops всех машин
  следуют физическим RPM, distance attenuation, stereo pan и pitch.
- `--physics-smoke-test` проверяет provenance, suspension/contact/handling и
  race state. `--race-render-smoke-test` проходит реальный menu dispatch и
  240 кадров SDL3/audio/bgfx/Metal runtime. Все 88 tracks и 17 cars прошли
  отдельный resource/physics-description catalog audit.

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
- Original-menu путь использует перенесённую MusicCat-очередь для всех трёх
  menu Ogg: shuffle без повторения на границе цикла, automatic/explicit Next,
  pause/resume с mixer cursor и атомарное user-state persistence. Декодирование
  и preload выполняются последовательно в фоне, не в render/input loop.
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
- Documentation: `docs/MILESTONE_10.md`, `docs/BUILDING_MACOS.md`,
  resource/renderer/dependency docs и этот файл.

## Изменённые файлы Milestone 9.1

- Profile/game data: `src/Rock3dGame/include/OriginalProfile.h`,
  `src/Rock3dGame/source/stub/OriginalProfile.cpp`,
  `src/Rock3dGame/include/OriginalRace.h`,
  `src/Rock3dGame/source/stub/OriginalRace.cpp`.
- Race runtime: `src/Rock3dGame/include/OriginalRaceSession.h`,
  `src/Rock3dGame/source/stub/OriginalRaceSession.cpp`,
  `src/Rock3dGame/include/InputActions.h`.
- HUD/render/audio: `src/RRR3d/OriginalRaceHud.*`,
  `src/RRR3d/OriginalRaceRenderer.*`,
  `src/RRR3d/OriginalRaceCommentator.*`,
  `src/RRR3d/main_bgfx_original_menu.cpp`, mesh shaders и portable
  renderer/material boundary.
- Physics/input/music: Jolt vehicle adapter, SDL action mapping и MusicCat
  state integration.
- Documentation: `docs/PHYSICS_PORT_PLAN.md`, `docs/PORTABLE_INPUT.md`,
  `docs/PORTABLE_AUDIO.md` и этот файл.

## Изменённые файлы Milestone 9

- Build: `CMakeLists.txt`, `CMakePresets.json`,
  `src/Rock3dEngine/CMakeLists.txt`, `src/Rock3dGame/CMakeLists.txt`,
  `src/RRR3d/CMakeLists.txt`.
- Physics/game: `src/Rock3dEngine/header/physics/OriginalVehiclePhysics.h`,
  `src/Rock3dEngine/source/physics/JoltVehiclePhysics.cpp`,
  `src/Rock3dGame/include/OriginalRace.h`,
  `src/Rock3dGame/source/stub/OriginalRace.cpp`,
  `src/Rock3dGame/include/OriginalRaceSession.h`,
  `src/Rock3dGame/source/stub/OriginalRaceSession.cpp`.
- Runtime/render: `src/RRR3d/OriginalRaceRenderer.h`,
  `src/RRR3d/OriginalRaceRenderer.cpp`,
  `src/RRR3d/OriginalRaceHud.h`, `src/RRR3d/OriginalRaceHud.cpp`,
  `src/RRR3d/main_bgfx_original_menu.cpp`.
- Notices: `resources/macos/Licenses/JoltPhysics.txt`,
  `resources/macos/Licenses/THIRD_PARTY_NOTICES.md`.
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
- `RRR3D_ENABLE_PHYSICS=OFF` в M3–M8 и `ON` в исправленном M9;
- `RRR3D_ENABLE_STEAM=OFF`;
- `RRR3D_ENABLE_GAMEPAD=OFF` в обычных M3/M5/M6 preset и `ON` в M7–M9;
- `RRR3D_ENABLE_AUDIO=OFF` в M3/M5/M6/M7 и `ON` в M8–M9;
- `RRR3D_ENABLE_DXVK_NATIVE=OFF`;
- `RRR3D_BUILD_DXVK_MOLTENVK_TEST=OFF` в обычном preset; target включается
  только отдельной feasibility-конфигурацией;
- в `macos-arm64-m5` renderer включён как `RRR3D_RENDERER_BACKEND=bgfx`, но
  legacy D3D9 renderer и gameplay scene graph не компилируются;
- legacy world/gameplay source set, D3D9/D3DX renderer, PhysX 2.8.4, полный
  `.r3d` material/effect scene graph, DirectShow, WinMM/XAudio, legacy XInput
  implementation и полные игровые consumers всё ещё отключены. Узкие
  decoders для original menu, `map1`, track/car `.r3d`, DDS/Ogg и localization
  включены только в исправленные milestone targets.

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
build/macos-arm64-m9/Debug/RRR3d --physics-smoke-test
build/macos-arm64-m9/Debug/RRR3d --race-render-smoke-test
build/macos-arm64-m9/Debug/RRR3d \
  --track=1 --car=dirtdevil --weather=rainy \
  --race-render-smoke-test
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
  --physics-smoke-test
SDL_AUDIO_DRIVER=dummy \
  build/macos-arm64-release/Release/RRR3d.app/Contents/MacOS/RRR3d \
  --audio-smoke-test
SDL_AUDIO_DRIVER=dummy \
  build/macos-arm64-release/Release/RRR3d.app/Contents/MacOS/RRR3d \
  --race-render-smoke-test --smoke-test-frames=240
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
- M8 follow-up smoke продолжил bgfx/Metal rendering во время фонового decode
  `Track1/Track14/Track15`, затем проверил pause/resume cursor, естественное
  окончание voice -> automatic Next, explicit Next, три различные selection и
  точный save/load MusicCat state; exit code 0 после 628 кадров.
- исправленный M9 headless smoke загрузил `map1.r3dMap`, 52 `ctTrack`,
  234 `ctDecoration`, 7 bonuses и 1175 collision triangles, затем прошёл
  suspension contacts, acceleration, braking, steering, countdown,
  checkpoint/lap/finish, weapon/damage, bonus и respawn state;
- исправленный M9 integration smoke активировал `Single Player` через SDL
  action path, отрисовал шесть машин и полное окружение через bgfx/Metal,
  достиг движения и контакта четырёх колёс; exit code 0;
- catalog audit разрешил visual/material/collision data всех 88 tournament
  tracks и выбрал все 17 garage cars; отдельный rainy map2/Dirtdevil Metal
  smoke также завершился с exit code 0;
- текущие M10 Debug и Release configure/build завершены с exit code 0;
  Release перестроен за 173 шага. Остаются только два legacy warning в
  `lslObject.h` о volatile increment/decrement;
- оба `RRR3d.app` содержат arm64-only Mach-O с `minos 13.0`, version 1.3.1,
  identifier `org.rrr3d.motorrock`, Retina metadata, `.icns`, пустой
  `Frameworks` и 1200 файлов в Resources;
- M10 verifier подтвердил strict ad-hoc signature и отсутствие ссылок на
  Homebrew, `/usr/local`, build directory, Windows `.lib/.dll` и любую
  внешнюю абсолютную runtime dependency;
- Debug bundle прошёл resource/physics, virtual input + audio/MusicCat
  480-frame smoke и отдельный 240-frame Metal race smoke. Release прошёл те же
  resource/physics/input/audio/MusicCat проверки и 240-frame Metal race smoke;
- Release ZIP извлечён в `/private/tmp`: game-data найден внутри перемещённого
  `Contents/Resources`, прямой 240-frame race smoke завершился с кодом 0.
  Отдельный запуск этого `.app` через macOS LaunchServices (`open -W -n`)
  также завершился с кодом 0.
- M10 wheel/sky follow-up устранил блокировку неприводных задних колёс:
  исходный `restTorque` больше не преобразуется в Jolt brake при поданном газе,
  а physics smoke теперь проверяет angular velocity каждого контактирующего
  заднего колеса. Исправлен и повёрнутый World4/hell cubemap — Metal renderer
  применяет исходное преобразование системы координат из `SkyBox.cpp`;
  World4 smoke прошёл на 240 и 1200 кадрах, горизонт проверен по захваченному
  кадру.
- Полная ревизия vehicle physics сопоставила активный Jolt adapter с
  `GameCar.cpp`, `Player.cpp`, `DataBase.cpp` и `Physx.cpp`: убрано деление
  полного source torque между колёсами, перенесены reverse и source gear
  state, `JumpProgress`, `StabilizeForce`, `tireSpring`, clutch immunity,
  исходные материалы и axial-vector transform. Подробная таблица и граница
  solver parity находятся в `docs/PHYSICS_ENGINE_REVISION.md`.

## Известные проблемы

- `Rock3dEngine` пока не является полным legacy engine target: активный M9.1
  включает renderer, resources, audio boundary и Jolt adapter, но D3D9/PhysX
  части остаются изолированы.
- `Rock3dGame` предоставляет original-data menu/race runtime, но Windows-only
  network/Steam/video и прямой D3D9/PhysX 2.8.4 код в portable target не
  включаются.
- `RecordLib` не включён в portable game target. Пробное подключение выявило
  MSVC-зависимые template lookup и спорную protected reference-counting
  границу в legacy serialization; это следует исправлять отдельным небольшим
  изменением, а не ослаблять Clang через `-fpermissive`.
- В обычном M3 preset SDL events заканчиваются в diagnostic shell; M7 menu и
  исправленный M9 race используют action layer, но legacy `ControlManager` ещё не
  является его consumer.
- DXVK Native/MoltenVK проверен и отклонён. bgfx backend покрывает
  исправленные M5–M9.5 slices, включая HDR, planar reflection, shadow map,
  true cube reflection, normal mapping и FxTrail.
- Оригинальные game assets импортированы в `game-data`; M9.5 декодирует binary
  `.r3d` visual/collision meshes, materials, DDS, nested effect graph и
  particle emitters, воспроизводит source scheduler/sorting и два projected
  shadow split. Не-trail distance emitters восстанавливают пройденный путь из
  скорости, а Metal depth precision не является побитовой копией D3D9.
  Перед публикацией нужно отдельно проверить права на распространение данных.
- Длинная музыка M8 пока декодируется целиком и сохраняется в памяти, но decode
  и preload всех трёх menu-треков выполняются последовательно фоновым worker;
  render/input loop не ждёт их. Streaming/ring buffer остаётся оптимизацией
  памяти. M9.1 race audio имеет attenuation, stereo pan и doppler-like pitch,
  но не точную X3DAudio DSP-матрицу с cones/obstruction.
- Физическое переключение Bluetooth/USB playback device не выполнялось;
  SDL default-device migration и event path нужно повторить на release hardware.
- Численная parity с legacy PhysX не заявляется: Jolt имеет другой solver.
  При этом game-side motor/gear/reverse/stabilization/tire/material semantics
  перенесены по исходникам; оставшаяся backend-граница описана в
  `docs/PHYSICS_ENGINE_REVISION.md`.
- AI/workshop/projectile/mine/support/material/effect data перенесены из
  исходников, но порядок PhysX contacts и D3D9 multipass rendering не могут
  быть численно идентичны portable backend.
- `ptDrobilka` использует исходный вращающийся weapon actor и контактный
  lifecycle `spark2`: модель отсутствует вне контакта, переезжает в contact
  point и удаляется через исходные 0,5 секунды; постоянный surrogate visual
  удалён.
- Общий flying-projectile runtime использует source `maxDist/speed`
  lifetime и `RocketUpdate` TrackPlane clearance. `sonar` из фактического
  `workshop.xml` распознан как `ptThunder` и отражается от реальных border
  triangles; `ptResonanse` вращает один actor transform для collision и
  renderer.
- `ptImpulse` сохраняет исходный трёхконтактный chain, деление damage и
  `Player::FindClosestEnemy` plane/view-angle selection; попадание в
  non-target больше не завершает и не повреждает цель суррогатным generic
  contact path.
- `ptTorpeda`/`ptImpulse` homing использует source shortest-arc quaternion
  slerp и пересчёт `_vec1`, включая `sphereGun` viewAngle 0 и
  non-relative speed projection.
- Attached `ptLaser`/`ptFrostRay`/`ptFire` живут serialized
  `minTimeLife`. Frost `model3` перенесён из impact surrogate в исходный
  target-child `SlowEffect`; его model lifetime больше не продлевается
  каждым кадром beam contact.
- `ptMaslo`, `ptMine`, `ptMineRip`, `ptMinePiece` и `ptMineProton`
  используют исходные arming/mine-lock branches. Удалены cooldown `0.75`,
  радиальный hardcode осколков и фиксированная жизнь `4.25`; nested
  `mineRipKern`/`mineRipPiece` теперь читают gameplay/death data из `db.xml`.
  Mine impulse прикладывается в OBB contact point, а `dtMine` death не
  засчитывается как обычный kill.
- Map pickups следуют `Player::TakeBonus`: одноразовые объекты вызывают
  serialized `DeathEffect`, race audio берётся из его behavior type `7`,
  medpack лечит на source value, а ammo использует исходные
  `Round((N-1)*Random())` и усечение charge. Придуманный общий
  `pickup_up` для игрового подбора удалён; persistent speed/lusha/oil
  по-прежнему не уничтожаются.
- Damage dispatch хранит исходные `dtSimple/dtEnergy/dtMine/dtTouch`:
  применяется только первый `stReflector`, затем проверяется immortality;
  поэтому shield сохраняет жизнь, но `cPlayerDamage` несёт входящий урон, как
  в `GameObject::Damage`. Нулевые fake-damage events от `ptMaslo` удалены,
  Droid лечит на фактически зашитые в Windows `5.0f`.
- Глобальная plane `Z=0` из `Map::Map` больше не заменяется тихим reset ниже
  `Z=-5`: crossing вызывает `dtDeathPlane`, vehicle death effects и
  двухсекундное восстановление для любой машины. `_touchPlayerId` хранится
  исходные 3 секунды; touch-kill achievement теперь засчитывает человека,
  вытолкнувшего соперника, а не обратный случай.
- Все девять `AchievmentCondition` сопоставлены с Windows event filters.
  `AchievmentModel::AddPoints` перенесён с точной mode/difficulty
  семантикой: campaign начисляет `Floor(reward × 1.0/1.2/1.5)`,
  skirmish не начисляет points; completion event сохраняет serialized
  `reward`, а points-плашка HUD в skirmish скрыта. Bonus condition считает
  точный `MapObjRec`, а `LapPass` и `Dodge` сохраняют фактические
  особенности исходной реализации.
- `AICar::ControlState` больше не заменён остановкой до respawn: после
  исходной секунды блокировки AI чередует задний и передний ход, при движении
  назад инвертирует steering и по-прежнему вызывает reset через три секунды.
  `AttackState` получил исходные minimum readiness 0,25 с, 25%-й случайный
  выбор из подходящих weapons и `placeMineRandom` в диапазоне `[-0.5, 0]`.
- `Player::CheatUpdate` перенесён с исходными easy/normal/hard таблицами:
  AI впереди человека отпускает газ по speed limit, отстающий AI получает
  коэффициент `1.05..1.85` одновременно для motor torque и lateral tire
  force. Jolt применяет их на тех же границах, где PhysX-код вызывал
  `SetMotorTorqueK` и `SetWheelSteerK`.
- `Map`-константа `Trace(4)` и `AISystem::ComputeTracks` больше не заменены
  наведением всех AI на ось трассы. Portable session воспроизводит исходный
  `ComputeTrackInd`, формирует цепочки продольно пересекающихся AI и
  per-car `lockTracks`, затем ведёт к доступному центру одной из четырёх
  полос с исходным look-ahead
  `5 + |speed| × kSteerControl × 10`; `kSteerControl` читается из `db.xml`.
- `AICar::PathState::ComputeMovDir` больше не пропускает подготовку к
  повороту: из трёх исходных trace points вычисляются те же `midDir`,
  `nodeRadius`, `edgeNorm` и `edgeLine`; AI заблаговременно выбирает
  внутреннюю полосу, обходит занятые полосы и после пересечения границы
  переключается на следующий tile. Отдельная regression использует
  фактический поворот `map1`, а не синтетическую трассу.
- `AICar::AttackState::FindEnemy/ShotByEnemy` теперь сохраняет front/back
  target между кадрами, использует исходные `±π/4`, plane-distance и
  line/Z gates. Задняя цель передаётся в projectile target, но source
  `normLine` разрешает такой выстрел только `ptTorpeda`; unlimited
  `maxDist <= 0` больше не превращается в придуманный предел 100.
- `Player::ResetCar` больше не телепортирует машину в начало предыдущего
  checkpoint-сегмента. Session хранит исходный `lastNodeCoordX`, проверяет
  вертикальными raycasts точки `0/-2/+2` м, при занятой позиции отступает на
  6 м (до пяти попыток) и переходит на предыдущий tile. В запрос включены
  `TrackPlane`, глобальный `PlaneDeath`, активные decorations и машины;
  собственный collider допускается, как в проверке `hitGameObj == _car`.
- Стандартный M10 bundle имеет только ad-hoc подпись: для распространения без
  Gatekeeper warning нужны Developer ID, hardened runtime, notarization и
  проверка на отдельной чистой машине. CMake options для подписи подготовлены,
  но реальные credentials в проект не входят.
- Рабочая папка Codex находится под file provider, который может повторно
  прикреплять Finder xattr к build artifact. Verifier поэтому проверяет
  xattr-free transport copy; ZIP packager также исключает эти metadata.
- Windows regression build требует Windows CI.
- Shared `MainMenu2` flow больше не размещает все строки как generic list:
  `GameModeFrame`, `TournamentFrame` и `DifficultyFrame` используют точные
  source coordinates `centerY - 100 + n × 53`, а Back перенесён в
  `centerY + 150`. Pointer hit-test совпадает с отрисовкой.
- Перенесены исходные enabled branches: Skirmish зависит от первого tutorial
  stage, Continue/Load — от наличия campaign profiles. Disabled items имеют
  alpha 0.25 и пропускаются cyclic keyboard/mouse navigation.
- `Race::MakeProfileName/NewProfile` больше не заменён сбросом активного
  профиля. New Game создаёт первый свободный `profileN` с source defaults;
  regression отдельно проверяет отсутствие перезаписи существующего
  campaign.
- Skirmish теперь использует временный `SkProfile` с именем `skirmish`,
  открывает planet zero и глобальные `planetsCompleted`, не добавляется в
  `race.xml`, не сохраняет временные деньги/progress поверх campaign и не
  запускает tournament advance после finish. При выходе восстанавливается
  точный campaign snapshot.
- M9 и M10 Debug собраны без warnings. Resource verification, physics/profile
  smoke и 300-frame bgfx/Metal integration прошли; последний посетил
  GameMode/Tournament, Workshop, Garage, Angar, Achievment и гонку. Ручная
  проверка arm64 Debug подтвердила shared-frame layout без артефактов.
- `ProfileFrame` больше не является generic page: перенесены vertical Grid,
  четыре visible rows, one-row scroll, исходные координаты/rotation/размеры
  `arrow1/arrowSel1`, alpha 0.25 для disabled arrows и отдельный Back.
- Каждая profile row использует shared `mainItemSel5` и исходный
  `buttonBg6/buttonBgSel6` close с scale 1.8. Перенесён двухколоночный
  NavElement graph item/close и связи с обеими arrows/Back.
- Выбор профиля сразу выполняет source `StartMatch`, загружает XML/defaults,
  пересоздаёт race/session и открывает RaceMenu. Прежний суррогатный возврат
  из Load в Tournament удалён.
- Close показывает `svHintDeleteProfile` в исходном accept dialog.
  `OriginalProfileStore::deleteProfile` повторяет `Race::DelProfile/SaveLib`:
  удаляет reference, оставляет legacy XML и корректно сохраняет даже пустой
  список. Serializer больше не добавляет удалённый последний профиль обратно.
- Отдельный temp-directory regression проверяет save → delete last → reload.
  Расширенный 300-frame Metal smoke посещает ProfileFrame/delete dialog,
  отменяет удаление и продолжает прежний маршрут. Ручная проверка arm64 Debug
  подтвердила grid, close и Yes/No dialog без артефактов.
- Придуманный finish summary с `Continue/Back` удалён. Новый экран повторяет
  `FinishMenu.cpp`: до трёх `Race::Results`, оригинальные
  `playerLeftFrame/playerLineFrame/playerRightFrame`, photo paths из
  `tournamet.xml`, `cup1..3.dds`, имена, Money/Points и picked-money
  форматирование.
- Перенесены source layout и reveal: три строки по 240 px, исходные label/
  photo/cup coordinates, цвета, delay 0.15 s, reveal 0.5 s и
  `voiceNameDur=1.5 s`. Чётные места въезжают слева, второе — справа.
- В `FinishMenu` больше нет selectable page: Action, Escape и любой left
  click выполняют `OnFinishClose` и возвращают в соответствующий
  campaign/skirmish `RaceMenu2`.
- Новый `--finish-menu-smoke-test` за 300 Metal frames проверяет три строки,
  photo/cup textures, Money/Points, picked-money и полную анимацию без записи
  профиля. Ручная проверка подтвердила итоговый кадр и Return-переход; найденная
  при ней склейка multiline CoreText label устранена отдельными line layers.
- Generic Authors/Credits заменён исходным `FinalMenu.cpp`: чёрный фон,
  девять `GUI/Slides/slide1..9.dds`, секции `svCredits` с красными captions
  и светлыми body-lines, source root `vp.x - 250`, 107-second scroll и
  per-slide alpha.
- Back использует исходные `buttonBg2/buttonBgSel2`, Header font и позицию
  `vp.y - 60`. Return/Escape и left click по кнопке возвращают MainMenu2;
  по истечении 107 секунд выполняется тот же автоматический переход.
- `TrackFinal.ogg` декодируется отдельным фоновым `OriginalMenuMusic` без
  сохранения состояния. При входе menu MusicCat приостанавливается, final
  track запускается с нуля, при выходе menu MusicCat возобновляется.
- Новый `--final-menu-smoke-test` проверяет девять slides, секционную
  прокрутку, Back, реальный background decode/playback и auto-return. M9/M10,
  resource, physics и последовательный 300-frame race-render прошли.
  Ручная arm64 Debug проверка подтвердила два последовательных source slide,
  читаемые credits и Return → MainMenu2 без визуальных артефактов.
- Перенесён вызываемый `DialogMenu2::MusicDialog`: `dlgFrame2.png`, Verdana
  32/24, исходные white/gray цвета, offsets и нижний левый anchor.
- Popup получает metadata всех трёх menu и 11 game tracks, появляется при
  startup/Next/race start и сохраняет source правило «обновить текст, но не
  перезапускать уже активную анимацию».
- Сохранена формула 1 s delay + 1 s slide-in + 3 s life + 1 s slide-out.
  Legacy widget z-order `3/2` переведён в видимую Metal overlay-полосу
  `60/59`; буквальные значения отсекались ортопроекцией.
- M8 Debug/audio smoke завершился за 625 frames и проверил menu popup; M9
  300-frame race-render проверил game popup вместе с полным renderer/physics
  regression. M10 `.app --verify-resources` подтвердил 1196 исходных файлов.
- Фактический arm64 Debug кадр через Computer Use подтвердил исходную рамку
  и metadata `Stereoside / On our Way` поверх MainMenu2.
- Придуманная selection-driven панель Workshop удалена. Перенесены
  `WorkshopFrame::ShowInfo/UpdateSlotInfo` и
  `DialogMenu2::WeaponDialog`: `dlgFrame3.png` 325×138, Verdana 18 bold
  gray word-wrap, Verdana 24 white name/money/damage, все четыре исходных
  label offsets и clamp с margin 15 px.
- Диалог теперь вызывается только mouse hover. Goods, slot plane, mobility
  level и weapon charge различаются; charge показывает
  `chargeCost × chargeStep`, level — следующий исходный upgrade, а damage
  сохраняет source `"-"` для деталей и округлённое число для оружия,
  включая `"0"` для support items.
- M9 smoke теперь задерживает keyboard navigation в Workshop, отправляет
  mouse-motion на реальный доступный товар и требует отрисованный
  `WeaponDialog`. M8/M9/M10 Debug, audio smoke, 300-frame race-render и
  `.app --verify-resources` прошли.

## Следующий рекомендуемый этап

Основные offline subframes `RaceMenu2` и ветка GameMode/Tournament/
Difficulty/Profile/FinishMenu/FinalMenu, а также `MusicDialog` и workshop
`WeaponDialog` теперь source-derived. Следующий P0-блок: перенести оставшиеся
общие `InfoDialog`/`AcceptDialog`;
после этого закрыть intro-video/final-unlock ветку.
Параллельно остаются source-by-source ревизии projectile callbacks,
material/effect graph и game-side audio behaviors. Release hardening
(Developer ID, notarization, clean-Mac test) нужен только после закрытия этих
функциональных расхождений; он не является заменой переноса.
