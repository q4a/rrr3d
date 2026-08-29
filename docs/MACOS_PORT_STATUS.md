# Статус порта RRR3D / Motor Rock на macOS

## Полный обратный аудит относительно pristine Windows source (2026-08-24)

За эталон зафиксирован `eff933868c1fbdfd266738a403fac80084f2b51e`
(`origin/master`), а не `dxvk-glm`. Аудит подтвердил, что `dxvk-glm` не
отключает основную PhysX vehicle physics, но массово меняет D3DX math и
комментирует `FxPhysicsEmitter`. Более важный факт: финальная arm64-сборка не
компилирует ни один из 34 legacy game `.cpp` и исполняет отдельную
`Original*`/bgfx/Jolt/SDL реализацию на 1196 оригинальных ресурсах.
Первая зафиксированная точка этого архитектурного ответвления — самый первый
macOS commit `59fc40f` (его parent непосредственно `bfbde0d`): в нём
non-Windows CMake сразу переключён на `source/stub` и portable entry points.

Найдены пять подтверждённых расхождений игровой семантики: AI campaign
reward accumulation, oil angular momentum, low-speed border damage,
отсутствующий в Windows 0.25-second car-contact cooldown и игнорирование
rotational kinetic energy. Ещё одно расхождение относится к устаревшему
capability API. Полная lineage-карта, build graph, матрица подсистем,
перепроверка прежних findings и план перехода к исполняемому Windows oracle
находятся в
[`HISTORICAL_WINDOWS_REVERSE_AUDIT.md`](HISTORICAL_WINDOWS_REVERSE_AUDIT.md).

## Windows `_DEBUG` compatibility runtime (2026-08-24)

Глобальные сценарные ветки Windows `_DEBUG`/`DEBUG_PX` теперь доступны только
через отдельный флаг `--legacy-windows-debug`. Режим пропускает release
startup и три типа кампанийных intro-роликов, добавляет исходные debugTrack и
World5/map0 с 99 кругами, принудительно выбирает `ewClody`, сразу переходит в
`cGoRace`, подключает AIDebug к существующей машине игрока, включает полный
debug-цикл из пяти камер и Windows Debug LAN timing. Состав участников не
увеличивается. Исходные `#if !_DEBUG` также соблюдены: skid/contact effects в
этом сравнительном режиме отключены. Обычный запуск и `--game-debug` не
получают этих изменений.

Ранее написанное «debug меняет состав гонки» исправлено: дополнительный
`AIPlayer` ссылается на уже существующего human `Player`, новой машины или
строки результатов он не создаёт. Полная матрица режимов и инструкция для
будущего сравнения с пересобранной Windows Debug-версией находятся в
[`ORIGINAL_GAME_DEBUG.md`](ORIGINAL_GAME_DEBUG.md).

## Original game debug instrumentation (2026-08-24)

Полезная часть исходного Windows debug-кода перенесена как отдельный режим
`--game-debug`; обычный запуск полностью его обходит. Сохранённые в профиле
`gaDebug1..gaDebug7` теперь проходят через SDL-ввод. F1 сбрасывает все машины
на исходные стартовые позиции, F2 временно переключает Bloom/HDR, F3 —
fullscreen, F6 — исходную AI trace, F7 передаёт существующую машину игрока
перенесённому AI-контроллеру. F10 скрывает overlay, Page Up/Page Down выбирают
страницы race/engine, wheel/contact и vehicle/suspension telemetry.

`--game-debug` по-прежнему является только инструментальным режимом и не
меняет кампанию. Для глобального поведения используется отдельный
`--legacy-windows-debug`.

> **Windows-эталон для всех следующих сравнений:** Parallels VM `Windows 11`,
> `\\Mac\Home\Downloads\Motor Rock\MR.exe` (`v. 1.2.0`, fullscreen
> `1920x1080`). Запуск, фокус, ввод и framebuffer capture описаны в
> [`WINDOWS_REFERENCE.md`](WINDOWS_REFERENCE.md). CrossOver для визуального
> эталона не использовать.

> **Актуальное заключение ревизии:** milestones 5–10 создали нативный arm64
> bundle и source-driven vertical slice на оригинальных ресурсах, но не
> доказали полный перенос Windows-игры. Исходные `Rock3dGame/source/game`
> классы в macOS target не компилируются; меню, session, AI, weapons, HUD и
> renderer частично воспроизведены новыми adapters. Video имеет native
> backend; NetLib lifecycle, исходные LAN menu/browser/IP frames и
> `NetRace`/`NetPlayer` replication подключены к финальному runtime. Steam
> выключен. Каноническая матрица «перенесено /
> частично / суррогат / не
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

Следующий крупный блок вернул владельца `FinishMenu`: порядок `Race::Results`,
индивидуальные длительности именных реплик, три поочерёдно въезжающие строки,
отдельное событие последнего участника, close input и layout теперь находятся
в `FinishMenuFrameState`. Удалены константная длительность 1.5 s и поиск
последнего игрока по максимальному месту; Money/Points снова рисуются
двухстрочными source labels.

Следом `FinalMenuFrameState` вернул concrete owner финального экрана: разбор
секций `svCredits`, 107-секундный clock, девять slide alpha intervals, Back
input и layout больше не живут в renderer. CoreText/bgfx остаются payload
backend, `TrackFinal.ogg` запускается с нулевого кадра после фонового decode,
а каждый DDS масштабируется по собственному aspect вместо пропорций slide1.

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
- Post-build очищает extended attributes, выполняет ad-hoc codesign и строго
  проверяет подписанное содержимое через xattr-free transport copy, поэтому
  повторно добавленный File Provider атрибут `com.apple.FinderInfo` не ломает
  корректную Release-сборку. CMake также принимает Developer ID identity и
  hardened-runtime option для будущей notarized сборки.
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
- Countdown больше не сводится к придуманным трём секундам: восстановлены
  `cGoRaceWait`, `cGoRace1..3`, финальный `cGoRace`, соответствующие
  `tablo0..tablo4` и исходное переключение submesh 1/2/3 стартового семафора
  красным, жёлтым и зелёным цветом.
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
- `RRR3D_ENABLE_VIDEO=OFF` в M3–M8 и `ON` в M9/M10/Release;
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
  implementation и полные игровые consumers всё ещё отключены. DirectShow
  заменён AVFoundation adapter в актуальных presets. Узкие
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
- Исправлен жизненный цикл исходного `GameMode::Commentator`: его таймер и
  очередь по окончании потока теперь обновляются каждый кадр, включая
  `FinishMenu`, поэтому составные реплики и очередь мест не обрываются после
  первого OGG. Перенесён отдельный `cPlayerFinishLast`, который Windows
  отправляет после анимации трёх призовых строк для последнего результата.
  Finish smoke теперь завершает весь исходный состав и проверяет отправку
  этой четвёртой, не привязанной к видимой строке, реплики.
- Исправлена отдельная ошибка `Race::OnLapPass`: для 4-го и 5-го места из
  шести порт раньше ошибочно повторял `cPlayerLeadFinish`. Теперь, как в
  Windows if/else-if chain, события существуют только для первых трёх и
  последнего участника; physics regression проверяет последовательность
  `Lead/Second/Third/none/none/Last`.
- `GameMode::ExitRace` теперь также буквально вызывает переносной
  `Commentator::Stop`: активная гоночная фраза и её очередь очищаются до
  `FinishMenu`, но per-comment delay/repeat state сохраняется. Раньше порт
  ставил голос на pause и сразу возобновлял его поверх результатов; на
  network client остановка дополнительно ошибочно зависела от записи профиля.
- Generic Authors/Credits заменён исходным `FinalMenu.cpp` и concrete owner
  `FinalMenuFrameState`: чёрный фон,
  девять `GUI/Slides/slide1..9.dds`, секции `svCredits` с красными captions
  и светлыми body-lines, source root `vp.x - 250`, 107-second scroll и
  per-slide alpha.
- Back использует исходные `buttonBg2/buttonBgSel2`, Header font и позицию
  `vp.y - 60`. Return/Escape и left click по кнопке возвращают MainMenu2;
  по истечении 107 секунд выполняется тот же автоматический переход.
- `TrackFinal.ogg` декодируется отдельным фоновым `OriginalMenuMusic` без
  сохранения состояния. При входе menu MusicCat приостанавливается, final
  track запускается с нуля, при выходе menu MusicCat возобновляется.
- Новый `--final-menu-smoke-test` ждёт реального background decode/playback,
  затем проверяет девять slides, секционную прокрутку, Back и auto-return.
  M9/M10, resource, physics и последовательный 360-frame race-render прошли.
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
- Старые предупреждения Workshop/Angar/Achievement больше не рисуются
  суррогатным `AcceptDialog`. Перенесён `DialogMenu2::InfoDialog` с
  `dlgFrame4`, `dlgButton2/dlgButtonSel2`, Verdana 44/24/32, исходными
  label offsets, word-wrap областью 245×135 и 15-pixel clamp.
- `svHintWeaponNotSupport`/`svHintCantMoney` используют исходный
  `waLeftBottom` от good/slot control, `svHintCantFlyPlanet` — `waBottom` от
  door slot, `svHintCantPoints` — center. Модальный input закрывает окно
  только через OK/Action и не пропускает mouse в underlying frame.
- 300-frame M9 regression теперь отдельно требует фактическую отрисовку и
  input-close `InfoDialog`; M8 MusicCat и M10 verification подтвердили
  отсутствие regressions и все 1196 оригинальных файлов.
- Все вызываемые offline подтверждения сведены к перенесённому
  `DialogMenu2::AcceptDialog`: `dlgFrame1` 384×156,
  `dlgButton1/dlgButtonSel1` 90×38, centered word-wrap Verdana 32 gray,
  source offsets `(-70,32)/(70,32)`, начальный focus Yes и clamp 15 px.
- Восстановлены `maxMode`, `maxButtonsSize` и `disableFocus`, поэтому
  `OptionsMenu::Press key` использует исходный modal Delete/Cancel path, а не
  специальную строку состояния. Любая клавиша назначается через тот же
  callback; Delete очищает binding, Cancel оставляет его прежним.
- HUD exit, Profile delete, Garage/Achievement buy остаются центрированными.
  Workshop Buy/Sell используют исходные quarter-size sender offset и
  `waLeftBottom`; Angar Stay/Fly — half-height offset и `waBottom`.
  Ошибка Garage buy теперь показывает отдельный source `InfoDialog`.
- 300-frame M9 smoke проверил точные размеры frame/info/buttons и offsets в
  Profile/HUD paths. M8/M9/M10 Debug, audio/race-render, resource и bundle
  verification прошли.
- Устранён безусловный `FinishMenu → RaceMenu2`: новая развилка повторяет
  `Menu::OnFinishClose` для pass failed/completed, planet unlock и final.
  `Race::CompletePlanet(4)` открывает также скрытые planets 5+, как в source;
  deterministic physics regression проверяет все transition variants.
- Legacy DirectShow player заменён `AVPlayer`/`AVPlayerLayer`. Все 14
  оригинальных AVI remux-ятся на этапе сборки в MP4 без перекодирования
  H.264/MP3, копируются в M9/M10 runtime и проверяются bundle verifier.
  Во время ролика MusicCat и game/menu time приостановлены; Escape/Pause,
  resize, normal completion и переходы к Angar/RaceMenu/FinalMenu совпадают
  с исходным control flow.
- `DifficultyFrame` новой кампании проигрывает `Main/Main_eng` до создания
  профиля и запускает `StartMatch` только через source `cVideoStopped`
  callback. `--video-smoke-test` подтвердил отображённый кадр `Main_eng`,
  near-end seek, completion и этот callback.
- В исходном offline-коде нет ввода имени профиля:
  `Race::MakeProfileName` создаёт `profileN`; `NetIPAddress` принадлежит
  сетевому экрану, а `UserChat` — race menu/HUD. Оба теперь перенесены, но не
  используются как выдуманный редактор имени offline-профиля.
- Перенесён `RaceMenu2::GamersFrame`, который раньше полностью пропускался.
  Новый championship и Skirmish теперь проходят выбор одного из семи
  персонажей из `tournamet.xml`; Tyler выбран по source default `gamerId=10`,
  Viper закрыт `AchievementModel::CheckGamerId` до `asOpened`, а выбор
  записывается в `PlayerProfile.gamerId` и применяется к race data.
- Экран использует исходные `space1`, `bottomPanel4`, `wndLight4`, обе пары
  arrows, gamer photos/texts и вращающийся `planet.r3d` с source ViewPort3d
  fitting/layout/navigation. Campaign запускает `Intaria/Intaria_eng` и
  переходит в Garage только по `cVideoStopped`; Skirmish идёт в Garage сразу.
- Отдельный `--gamers-frame-smoke-test` подтвердил catalog/gate, 3D draw,
  Tyler → Snake, `gamerId=4` и Garage transition без записи профиля.
- Повторная source-ревизия обнаружила, что ранний перенос
  `RaceMainFrame` был неполным: вместо `Player::GetPhoto` и boss photo в двух
  `imageFrame1` рисовались придуманные имена, boss-car viewport, шесть
  loadout viewports и `statBar` отсутствовали, а `CarFrame` не включался в
  состоянии `msMain`.
- Разрыв закрыт по `RaceMainFrame::OnInvalidate/OnAdjustLayout` и
  `CarFrame::SetSlots`: используются photo выбранного gamer и босса текущей
  планеты, вращающийся boss car, реальные Weapon1–4/Hyper/Mine meshes,
  charge и Damage/Armor/Speed values. `msMain` теперь показывает текущую
  машину с profile slots в `Misc/garage`, а `csAutoObserver` вращается с
  исходной скоростью `pi/48`.
- 300-frame Metal regression теперь требует все эти элементы и отдельный
  lighting draw `CarFrame` до перехода в Garage/Workshop; World4/map1 тест
  прошёл с шестью машинами, четырьмя wheel contacts и max speed `22.3117`.

## Следующий рекомендуемый этап

Основные offline subframes `RaceMenu2` и ветка GameMode/Tournament/
Difficulty/Profile/FinishMenu/FinalMenu, а также `MusicDialog` и workshop
`WeaponDialog`, вызываемые offline `InfoDialog`/`AcceptDialog` и `GamersFrame` теперь
source-derived. Source finish progression, planet/final movies и нативный
video backend, включая Intaria transition, также подключены; projectile,
material graph и game-side audio follow-ups ниже закрыты. Следующий большой
продуктовый разрыв после подключения NetLib transport/session, LAN UI,
`NetRace`/`NetPlayer` class ID, race replication и `UserChat` — оставшиеся
source RPC/UI branches и ручной двухмашинный LAN acceptance. Release hardening
(Developer ID, notarization, clean-Mac test) нужен только после закрытия этих
функциональных расхождений; он не является заменой переноса.

### Source material mapping follow-up

- `pixLight.fx`, `bumpMap.fx`, `reflMapp.fx` и
  `planarReflMapp.fx` теперь сведены в Metal shader с исходным порядком
  операций: reflection/planar color формируется до `CompSpotLight`,
  reflectivity равна `0.4`, planar alpha участвует в коэффициенте, а
  `model.fx` reflection vector сохраняет знак `viewPos - worldPos`.
- `Player::SetColor` больше не умножает всю текстуру кузова: цвет передаётся
  как независимый `D3DTSS_CONSTANT`/`alphaBlendColor` и заполняет только
  прозрачную долю car texture, как в Windows.
- Перенесены `IActor::vec1/vec3` и вычисление `GraphManager::BuildOctree`
  `texDiffK` для наклонных track actors. Spotlight использует несжатый
  `spotK` и исходное дальнее затухание после `range`; солнечные ambient и
  diffuse равны `clrGray60`, specular остаётся белым.
- Light quality снова управляет исходным графом: Low отключает pixel/refl/
  bump/planar mapping, Middle оставляет pixel и static-sky reflection,
  High включает bump, planar и true cube reflection.
- arm64 Debug build, resource audit и physics smoke прошли. Последовательные
  240-frame Metal smokes подтвердили World1 standard/reflection, World2
  water/reflection и 768 normal-map submissions, World5 planar reflection,
  а также World4 magma/volume-surface; во всех случаях сохранились четыре
  wheel contacts, шесть машин и полный HDR/HUD render graph.

### Source include-instance effect graph follow-up

- `MapObj::includeList` теперь переносит не только ссылки на DB records, но
  и полностью сериализованные anonymous objects. Это вернуло потерянный
  `rifleProj/obj0`: локальный distance-triggered smoke emitter с исходными
  `-0.4` offset, `0.25` start interval, `0.5` lifetime и `flare1.dds`.
- Behavior list принадлежит конкретному include instance. Inline
  `FxSystemWaitingEnd` теперь применяется к flattened emitters у
  `rocket/smoke2`, `rocketAir/smoke4`, `thunder/smoke5`,
  `phaserBolt/smoke8` и `shotBall/fireTrail`, поэтому их уже испущенные
  частицы доживают после смерти owner object.
- Звуки object graph больше не собираются без разбора из каждого behavior:
  автозапуск оставлен только за исходным `LifeEffect` (type 7), тогда как
  Death/Shot/Immortal dispatch остаётся в своих callbacks. Благодаря чтению
  inline behaviors `mortiraBallDeath/death30` снова создаёт оригинальный
  spatial `carcrash05.ogg`.
- Новые provenance assertions, arm64 Debug build, physics smoke и 300-frame
  World1 Metal/audio smoke прошли; последний завершился с шестью машинами,
  четырьмя wheel contacts и 1414 FxTrail submissions.

### Source ComplexMatLib sampler follow-up

- Прямая material table подтверждена как перенос создаваемой в рантайме
  `ResourceManager::ComplexMatLib`, а не как замена отсутствующего файла.
  При сверке найдены и удалены ошибочно придуманные render states:
  `Bonus\\shield` и `Car\\blend` снова освещаются и туманятся так же, как
  в Windows.
- Перенесены пропущенные source sprite states пяти Bonus-материалов и
  отключение lighting/fog у `GUI\\space2`; у sprite-бонусов также сохранён
  независимый от blending флаг `moZWrite=false`.
- `Sampler2d::BuildAnimByOff` теперь представлен полным UV region и
  half-texel inset из размеров исходного DDS. Это исправляет все атласные
  эффекты и отдельно `gunEff2`, который использует только верхнюю четверть
  256x256 texture, а не всю её высоту.
- Resource provenance, physics/session smoke и последовательный 300-frame
  World1 bgfx/Metal smoke прошли: четыре wheel contacts, шесть машин,
  1414 FxTrail submissions и полный HDR/HUD/refraction render graph.

### Source GameMode startup follow-up

- Обычный запуск больше не открывает `MainMenu2` немедленно. Перенесён
  release-путь `World::RunGame -> GameMode::Run(true)` и временной автомат
  `GameMode::OnFrame` с оригинальными `yardLogo.png`, `laboratoria24.png` и
  `startLogo.dds`.
- Сохранены исходные интервалы: начальная чёрная секунда, fade-in/hold/
  fade-out первой заставки, секундная пауза, такой же цикл второй заставки и
  отдельный полноэкранный кадр `startLogo` перед созданием `MainMenu2`.
  Логотипы центрируются без растяжения, а стартовый кадр сохраняет aspect.
- Во время заставок игровой ввод не протекает в меню. `Escape` повторяет
  Windows-контракт: пропускает оставшуюся анимацию, но не пропускает
  обязательный кадр `startLogo`; музыка меню запускается только при реальном
  переходе в `MainMenu2`.
- Новый `--startup-smoke-test` проверяет все временные состояния, обе чёрные
  паузы, обработку `Escape`, кадр `startLogo` и переход в меню. После него
  прошли resource audit, physics/session smoke и 120-frame bgfx/Metal
  menu/audio regression.

### Source StartOptionsMenu follow-up

- `OriginalProfile` больше не смешивает default `pcIsometric` с реально
  прочитанным `prefCamera`. При отсутствии поля обычный запуск повторяет
  `GameMode::CheckStartupMenu` и после заставок открывает обязательный
  `StartOptionsMenu`, а не молча принимает придуманное значение.
- Перенесены `startMenuBg.png`, `labelBg1.png`, `buttonBg5.png`, source
  coordinates и четыре циклических stepper: камера с начальным `Select`,
  системные display modes, все шесть языков из `game.xml` и два стиля
  комментатора. Apply остаётся недоступным, пока игрок явно не выбрал камеру;
  Escape и остальные игровые действия поглощаются модальным экраном.
- Apply записывает исходные `prefCamera`, resolution, language,
  commentatorStyle и compatibility-флаг GPU; смена языка сохраняет исходное
  предупреждение `svHintNeedReload` до возврата в главное меню. Apple Silicon
  считается одним пригодным unified GPU: последовательная discrete-video проверка сохраняет
  `sfrFixed`, не показывая неверное Windows-предупреждение о hybrid GPU.
- `--start-options-smoke-test` проверяет camera gate, все четыре визуальные
  строки, SDL-навигацию, сохранение и повторную загрузку XML во временном
  каталоге и переход в `MainMenu2`. Также прошли resource audit,
  physics/session smoke и повторный startup bgfx/Metal smoke.

### Source InfoMenu / race loading follow-up

- Кнопка Start race больше не выполняет тяжёлый `reloadCurrentRace` внутри
  обработчика ввода. Перенесена последовательность
  `GameMode::StartRace -> Menu::msInfo -> (++_startRace)>1 -> DoStartRace`:
  сначала модальный экран получает два представленных кадра, затем загружается
  world/Jolt/renderer/audio state и только после этого включается HUD.
- `InfoMenu::msLoading` использует оригинальный 1920×900
  `Data/GUI/loadingFrame.dds`, фильтрацию DDS backend-а, чёрный clear и
  `Menu::GetImageAspectSize` fit без растяжения. Во время загрузки mouse,
  keyboard и gamepad actions не протекают в скрытый `RaceMenu2`.
- 300-frame integrated bgfx/Metal smoke теперь требует и loading-frame draw,
  и фактическую двухкадровую отсрочку. После неё подтверждены шесть машин,
  четыре wheel contacts, физика/звук/HUD, оба camera mode и полный
  cube6/shadow2/scene/HDR/refraction render graph. Resource и physics smokes
  также прошли.

### Source audio graph / ShotEffect follow-up

- Исправлен десятикратный уровень всего portable mix: как и
  `snd::Engine::Init`, SDL mastering stage теперь равен `0.1`, после чего
  применяются исходные Music/Effects/Voice, Source и resource gains.
- Удалена придуманная схема «первый звук на одно событие WeaponFired».
  `Weapon::CreateShot -> Behaviors::OnShot -> ShotEffect::GiveSource3d`
  теперь выполняется для каждого успешно созданного projectile и выбирает
  один из всех сериализованных sound refs по исходному `RandomRange`.
- Для каждой машины, установленного weapon-slot и sound variant хранится
  собственный ShotEffect Source3d: повторный `Play` во время активности
  игнорируется, далёкий запрос ждёт входа в радиус 30 м, после выхода за 45 м
  PCM cursor приостанавливается и затем продолжается без перемотки.
- Spatial smoke фиксирует итоговые коэффициенты master/category/resource и
  30/45-метровую state machine; race-session smoke требует реального
  сериализованного ShotEffect sound event вместе с визуальным эффектом.

### Source PairPxContactEffect / LifeEffect audio follow-up

- Контактный звук больше не является безымянным one-shot. Перенесён
  `PairPxContactEffect::ContactMap`: отдельный Source3d на actor pair,
  исходный `floor(soundCount * Random())`, позиция первого manifold point и
  освобождение источника вместе с парой через 0.1 секунды без контакта.
- Звуки `LifeEffect` у vehicle/projectile/mine death и bonus pickup теперь
  принадлежат породившему их effect object. Далёкий источник может стартовать
  только пока объект жив, активный voice принудительно заканчивается при его
  смерти, а pause гонки замораживает и PCM cursor, и остаток lifetime.
- Для `DeathEffect::targetChild` сохранена полная target-local позиция:
  world point переводится в local scale/rotation машины и каждый кадр снова
  собирается из актуального body transform, поэтому звук движется вместе с
  прикреплённым визуальным эффектом без телепорта к центру автомобиля.
- Race-session smoke теперь проверяет identity/surface контактного audio
  source и положительный source lifetime pickup/death sounds.

### Source NetLib TCP/UDP transport follow-up

- Исходный Boost.Asio `NetLib` теперь компилируется в arm64 static library,
  не используя Windows `extern/boost`/`extern/glm` или Homebrew dylib. Для
  network preset закреплены Boost 1.85.0 и GLM 1.0.1 с SHA-256.
- Перенесены modern Asio executor/restart semantics и `getifaddrs` adapter
  enumeration. TCP control channel, UDP state datagram, reconnect и полный
  service player-id/command handshake выполняются на macOS.
- Исправлены выявленные регрессией исходные ошибки: dynamic `BitStream`
  записывал адрес вместо данных и имел shallow ownership, Windows `long`
  расширялся до восьми байт на LP64, command callback захватывал следующий
  datagram header, client state не сбрасывался, а часть wire fields была
  неинициализирована.
- `macos-arm64-network` и `rrr3d_net_loopback_smoke` проверяют TCP/UDP,
  повторное подключение, dynamic state и Windows-compatible 4/8-byte header
  layout.
- M9/M10/Release включают `OriginalNetworkSession`: финальный executable
  вызывает source `Initializate/Process/Close/Finalizate`, использует порт
  58213 и sync rate 70, показывает adapters, ServerType/ClientType,
  broadcast browser и ручной IP. Отдельные session и Metal menu smoke прошли.
- `NetRace`/`NetPlayer` перенесены поверх connected transport с исходными
  class ID 1/2, порядком RPC, сериализацией match/player state и профилями.
  Меню хоста и клиента использует реплицированный roster, а не fake match.

### Source network gameplay / finish follow-up

- Перенесены gameplay RPC исходных `NetRace`/`NetPlayer`: `Shot`, `Damage`,
  `Bonus`, оба варианта `MineContact`, UTF-16 `PushLine`, `RaceFinish` и
  `ExitRace`. Wire layout сохраняет Windows-размеры полей, slot mask и
  исходный порядок координат; входящий client `Damage` сервер отбрасывает,
  как в оригинале, вместо доверия неавторитетному изменению жизни.
- `StartRace` теперь сбрасывает готовность, ожидание старта и финиш всех
  моделей. Флаги локального игрока сохраняются в последующих UDP state
  update, поэтому движение машины больше не отменяет уже отправленные
  `RaceGoWait`/`RaceFinish`.
- Перенесён сервер-авторитетный финиш: хост ждёт `RaceFinish` только живых
  human-моделей, запускает исходный трёхсекундный finish timer, формирует
  результаты в model order и рассылает `ExitRace`; клиент применяет места,
  деньги и очки из серверного результата и открывает FinishMenu без локальной
  подмены прогресса.
- Loopback regression проверяет полный цикл ready/start/countdown, все новые
  packet types, кириллический chat, finish/result, повторный `StartRace` и
  reset флагов. Все три CTest и 180-кадровый LAN `bgfx/Metal` smoke проходят.

### Source network gameplay authority follow-up

- RPC больше не заканчиваются в диагностическом snapshot: активная гонка
  потребляет `Shot`, `Damage` и `Bonus` по монотонному event sequence. Для
  `ShotSlots` сохранён исходный bitfield `Hyper/Mine/Weapon1..4`, projectile
  id и мировая позиция первого созданного projectile; удалённая сторона
  повторно проходит реальные Prepare/projectile/effect/audio пути с этой
  координатой и не отправляет echo-пакет.
- Перенесена авторитетность `Logic::Damage -> NetRace::Damage1`: клиент
  применяет reflector, но не меняет life до ответа; хост применяет значение,
  вычисляет `targetLife/death` после каждого отдельного попадания и рассылает
  результат. Клиент вызывает эквивалент overload
  `GameObject::Damage(value,newLife,death,type)`, включая DamageEffect,
  destruction и kill semantics.
- Сетевой ownership теперь отделён от порядка racer list. Remote human больше
  не получает локальный `AIInput`, AttackState, mine/hyper или автоматический
  reset. На клиенте AI и чужие human управляются только NetPlayer state, а на
  хосте source AI остаются авторитетными.
- Обычный pickup выполняется только владельцем коснувшейся машины и затем
  повторяется через `OnTakeBonus`; это исключает локальное исчезновение
  бонуса под удалённой машиной. Money/charge/medpack/immortal используют ту
  же `Player::TakeBonus` state machine, death effect и HUD event.
- Race-session regression проверяет deferred/authoritative damage, точный
  targetLife, отсутствие AI input у remote human, сериализацию/replay
  projectile origin и сетевой pickup lifecycle.

### Source MapObj identity / Damage2 / MineContact follow-up

- Удалён временный сетевой адаптер `bonusIndex + 1`. Перенесён исходный
  `Map::MapObjList::InsertItem`: единый ID растёт в порядке категорий
  `ctEffects, ctDecoration, ctTrack, ctWeapon, ctCar, ctWaypoint, ctBonus`.
  На исходной `World1/map1` это даёт decoration `1..234`, track `235..286`,
  bonus `287..293` и первый car ID `294`; resource smoke фиксирует эти
  значения непосредственно из оригинального `r3dMap`.
- `NetPlayer::Shot` теперь передаёт MapObj ID цели-машины, а
  `OnTakeBonus` разрешает глобальный MapObj ID обратно в оригинальный bonus.
  Индексы локальных renderer/runtime массивов больше не выходят в wire
  protocol.
- Подключён активный `Logic::Damage -> NetRace::Damage2` для разрушаемых
  `ctDecoration`: клиент посылает запрос без локального изменения, хост
  вычисляет точные `targetLife/death`, остальные peer применяют их без
  повторного reflector/damage и в тот же кадр отключают collision body и
  создают исходные destruction-list fragments.
- Перенесён порядок `Logic::MineContact -> NetPlayer::OnMineContact1/2`.
  Контакт публикует владелец поражённой машины; placed mine идентифицируется
  парой owner-model/projectile-id, map mine — глобальным MapObj ID. Взрыв,
  вертикальный impulse, death effect и последующий host-authoritative
  `Damage1` выполняются только после надёжного RPC, без раннего локального
  уничтожения и повторных contact packets.
- Race-session regression проверяет MapObj Damage2 host/client state,
  target-owned MineContact request и replay; три NetLib CTest проходят.
- TCP acceptor сохраняет Windows-поведение немедленного повторного запуска
  хоста через `reuse_address`; macOS `TIME_WAIT` больше не превращает второй
  CreateHost/resource-test в ложный `Address already in use`.

### Source UserChat / network HUD follow-up

- Перенесён renderer-independent `DialogMenu2::UserChat`: newest-first
  история, объявленный предел 50 строк, исходные 10 секунд показа и одна
  секунда alpha fade. CoreText/bgfx labels используют `VerySmall`, белый
  текст, цвет имени игрока и исходное правое расположение у mini-map.
- `Menu::OnHandleInput` подключён к SDL: Enter открывает ввод, повторный Enter
  локально добавляет непустую строку и вызывает исходный UTF-16
  `NetRace::PushLine`; Backspace удаляет один UTF-8 code point. Во время
  ввода `HumanPlayer`-эквивалент блокирует движение, оружие, mine/hyper,
  reset и pause actions.
- Входящий `cNetRacePushLine` разрешает отправителя по `ownerId`, цвету
  `NetPlayer` и точному `Tournament::GetPlayerData(gamerId)` из семи
  оригинальных gamer-записей. Локальная строка не получает сетевого echo,
  как в Windows.
- Добавлен отдельный regression модели чата. Loopback проверяет owner sender
  и кириллический wire text, а integrated Metal race smoke открывает поле,
  вводит/отправляет строку, проверяет gamer name, labels и возвращение
  управления машине.

### Source NetRace options / planet authority follow-up

- Восстановлен отсутствовавший исходящий путь восьми RPC из
  `NetRace.cpp`: `SetUpgradeMaxLevel`, `SetWeaponMaxLevel`,
  `SetCurrentDifficulty`, `SetLapsCount`, `SetMaxPlayers`,
  `SetMaxComputers`, `SetSpringBorders` и `SetEnableMineBug`. Хост меняет
  локальное source-state только при новом значении и отправляет тот же
  scalar wire payload в исходном RPC order; клиентские вызовы отклоняются.
- `OptionsMenu::GameFrame::LoadCfg/ApplyChanges` снова соблюдает source
  ownership: на клиенте host-owned строки 3..10 отключены, приглушены и
  пропускаются навигацией, а Apply хоста публикует все восемь значений через
  активный `OriginalNetworkSession`. Входящие значения обновляют profile,
  race borders/mine behavior и открытый OptionsMenu без локальной подмены.
- Подключены `NetRace::ChangePlanet/OnSetPlanet`: клиент не может нажать
  planet travel slot, хост передаёт исходные planet/track/weather indices,
  клиент разрешает их через оригинальный `trackCatalog`, применяет weather
  override и только затем перезагружает выбранный source world.
- Loopback теперь проверяет все восемь RPC, host-only gate, динамические
  лимиты AI/игроков и последующий `StartRace`; четыре CTest проходят.

### Source RaceMainFrame network ready / kick follow-up

- Перенесены `RaceMainFrame::AddPlayer/AdjustPlayer/UpdatePlayers`: пять
  оригинальных `netPlayer*.png`, двухколоночный layout, gamer photo/name,
  вращающаяся исходная машина, `svHostLabel`, ready label/state и доступная
  только хосту кнопка kick.
- `RaceMainFrame::RaceRady/OnInvalidate/OnClick` снова соблюдают Windows
  правила. Клиентский Start переключает точный `NetPlayer::RaceReady` и на
  время готовности выбирает Start, блокирует и приглушает остальные шесть
  кнопок. Периодическая публикация car/profile теперь сохраняет этот флаг.
- Хост требует непустой `netOpponents`, проверяет `AllPlayersReady`, выводит
  исходный warning и перед повторной гонкой подтверждает удаление leavers.
  Kick разрешает `ownerId` через исходный `NetService::GetConnectionById` и
  вызывает `Disconnect`, как `NetGame::DisconnectPlayer`.
- Loopback regression завершает активного клиента со стороны хоста и
  проверяет удаление его `NetPlayer`; session smoke проверяет ошибку для
  отсутствующего peer.

### Source NetPlayer gamer/color authority follow-up

- Разделены исходные `cNetPlayerSetGamerId` и `cNetPlayerSetColor`; поле
  `NetEventData::failed` больше не теряется между RPC и portable menu event.
- `NetPlayer::OnSetGamerId` повторяет обе Windows-ветки конфликта: remote
  owner получает направленный ответ с прежним gamer ID, а локальный host
  применяет прежнее значение и публикует failed-event без рекурсивной
  отправки самому себе. `OnSetColor` возвращает клиенту прежний host-owned
  цвет.
- Сетевой `GamersFrame` снова асинхронный: после confirm показывает исходные
  `svWarning/svHintPleaseWait` без кнопки, ждёт авторитетный event и только
  после успеха открывает Garage. Отказ оставляет пользователя в выборе и
  показывает `svHintSetGamerFailed`.
- `RaceMenu::OnProcessNetEvent`-эквивалент синхронизирует профиль с
  авторитетным `NetPlayer`; конфликт цвета немедленно возвращает окраску
  Garage/CarFrame и показывает `svHintSetColorFailed`.
- Loopback с двумя игроками проверяет одновременный конфликт gamer/color,
  оба failed-event, откат состояния на клиенте и host, а также отдельную
  локальную конфликтную попытку host.

### Source NetPlayer generated identity / menu filtering follow-up

- Конструктор human `NetPlayer` больше не начинает с придуманной пары
  `gamerId=-1`/white: до первой синхронизации перенесены исходные
  `GenerateGamerId` и `GenerateColor`. Персонаж выбирается в порядке
  `tournamet.xml`, цвет — в точном порядке 14 констант двух палитр
  `Player.cpp`; уже занятые другими human-моделями значения пропускаются.
- `GamersFrame::GetNext/GetPrev` и `GarageFrame::RefreshColorList` получили
  сетевые `CheckGamerId`/`CheckColor`: занятые персонажи не участвуют в
  навигации, занятые color-box скрываются с сохранением исходной геометрии и
  пропускаются клавиатурой, gamepad и mouse hit-test.
- `NetPlayer::SetGamerId` вынесен из общего state-diff пути и, как в Windows,
  всегда отправляет RPC даже для совпадающего с generated-state ID. Поэтому
  `GamersFrame` гарантированно получает success-event и не остаётся навсегда
  в `svHintPleaseWait`.
- Двухсторонний loopback фиксирует автоматически выбранный следующий gamer,
  первую свободную source-палитру и успешный same-value gamer round-trip.

### Source network failure lifecycle follow-up

- `OriginalNetworkSession` больше не схлопывает три Windows callback в один
  безымянный `Failed`: snapshot различает `OnConnectionFailed`, отключение
  host-owner и критический `OnFailed`; `Close` очищает как OS error, так и
  тип причины.
- LAN browser и ручной IP показывают исходный недоступный для закрытия
  `svHintPleaseWait` на время асинхронного connect. Создание owner `NetPlayer`
  снимает его через `MainMenu::OnConnectedPlayer`; отказ/раннее отключение
  заменяет его на `svHintHostConnectionFailed`.
- Потеря хоста в RaceMenu/HUD использует `svHintDisconnect`, а критическая
  ошибка — `svCriticalNetError`. Гонка ставится на паузу; подтверждение
  повторяет `Menu::MyDisconnectEvent`: снимает паузу, завершает race/match,
  закрывает NetLib, очищает replicated state и возвращает MainMenu.
- После failure portable loop больше не публикует player/options events в уже
  закрывающийся transport. Session regression проверяет реальный
  asynchronous connection refusal, а 330-кадровый Metal smoke — wait-dialog,
  source failure hint и финализацию.

### Source NetPlayer disconnect / race removal follow-up

- Перенесён активный путь `NetPlayer::~NetPlayer`: удаление remote model на
  host теперь вызывает эквивалент `Player::FreeCar(true)`/`Race::DelPlayer`,
  а не оставляет последнюю принятую машину в мире. Для сохранения стабильных
  model-to-racer индексов slot помечается disconnected, но его Jolt body
  действительно удаляется из simulation/contact solver.
- Остановлены car-owned idle/RPM, wheel-slip, contact и attached effect
  voices. Независимые уже выпущенные projectiles/mines сохраняются, как и в
  исходном `FreeCar`, который удаляет MapObj машины, но не очищает список
  bonus projectiles.
- `HudMenu::PlayerStateFrame::RemoveOpponent` и
  `MiniMapFrame::DelPlayer` восстановлены в active bgfx HUD: исчезают имя,
  life overlay и точка карты. Renderer не рисует body/wheels/shadow, session
  не запускает AI/respawn и исключает игрока из place/finish/result logic.
- Race-session regression проверяет idempotent removal, нулевой control и
  отсутствие respawn. Существовавшая проверка `Damage2` также исправлена:
  death теперь ожидается из фактического serialized maximum life, а не из
  неверного предположения, что первый destructible обязательно переживёт
  единицу урона.

### Source NetRace ExitMatch / orderly match exit follow-up

- Вместо прежней смены `matchActive/raceActive` перенесён
  `NetRace::DoExitMatch`: список class-ID-2 `NetPlayer` копируется, все модели
  локально удаляются штатным `DeleteModel(..., true)`, очищаются race flags и
  results, после чего отправляется исходный reliable `OnExitMatch` RPC.
- Active runtime обрабатывает `MatchExited` отдельно от сетевой ошибки,
  очищает Jolt/renderer/HUD/audio state и возвращает оба процесса в MainMenu
  без ложного `svHintDisconnect`.
- Восстановлено различие `HudMenu::OnClick`: client делает локальный
  `ExitRace` и завершает match, host отправляет `ExitRace` с текущим списком
  результатов и возвращается в `RaceMenu2`; пункт Exit сетевого RaceMenu
  отправляет `ExitMatch` до `FinalizateNet`.
- Loopback проверяет client-to-host `OnExitMatch`, отсутствие оставшихся
  player models на обеих сторонах и повторный StartMatch/StartRace тем же
  живым class-ID-1 `NetRace`.

### Source NetPlayer control stream / freshness follow-up

- Исправлен общий `NetLib::BitStream`: первый нулевой scalar/vector теперь
  получает фактический wire type. Раньше zero-valued angular momentum мог
  остаться `cBitTypeEnd`, из-за чего reader завершал пакет до `moveState`,
  `steerState` и `steerWheelsAngle`; это давало неподвижные/телепортирующиеся
  remote-машины и скачки точек mini-map.
- Перенесён `NetPlayer::Process` с исходным `_dAlpha=1.0`: через секунду без
  нового `ResponseStream` удалённая машина отпускает прежние gas/reverse и
  steering. Jolt adapter больше не восстанавливает steering из старого
  `steerWheelsAngle`, когда `steerState == swNone`.
- `ResponseStream` снова игнорирует server echo для client owner и входящие
  vehicle states завершившего гонщика, как условие Windows-кода. Формат семи
  полей не изменён.
- Закрыт старый use-after-free teardown: поздние деструкторы моделей NetLib
  не обращаются к уже уничтоженному `OriginalNetworkModels` context.
- `lsl::appLog` на macOS перенаправлен из текущего каталога bundle в
  `~/Library/Logs/RRR3d/appLog.txt`; runtime больше не изменяет sealed
  `Contents/Resources` и не инвалидирует подпись после сетевого smoke/run.
  Loopback фиксирует zero-before-control round-trip, реальную UDP-репликацию,
  секундный timeout и безопасную финализацию; четыре CTest и Metal menu/race
  smoke проходят.

### Source NetRace repeated-race reconciliation follow-up

- `NetRace::StartRace` теперь не только добавляет недостающие AI-модели, но и
  повторяет второй Windows-цикл: при уменьшении `MaxComputers`/`MaxPlayers`
  удаляет последние лишние class-ID-2 модели через исходный reliable
  `cDelModelRPC`. Поэтому между повторными заездами не остаются лишние Jolt
  машины, HUD-строки и точки мини-карты.
- Удалён придуманный portable Pause sender. В Windows `NetRace::Pause`
  закомментирован целиком; порт сохраняет его зарегистрированный RPC slot и
  receive-handler для совместимости wire order, но больше не выдаёт
  отсутствующую в исходнике отправку за перенесённую механику.
- Двухсторонний loopback уменьшает число компьютеров между двумя гонками и
  требует одинаковый состав моделей на host/client до следующего старта.

### Source NetPlayer one-shot ResponseStream dispatch follow-up

- Каждый реально прочитанный UDP `ResponseStream` теперь получает локальную,
  не входящую в wire format ревизию. Jolt применяет source snap/correction,
  rotation и оба momentum только при изменении этой ревизии — ровно в момент
  нового `NetPlayer::OnSerialize(read)`, а не повторно на каждом render-frame.
- Устранено постоянное притягивание remote-машины к последнему сетевому
  снимку между пакетами. Оно усиливало near-position поправку импульса и
  проявлялось рывками машины и телепортацией точки мини-карты.
- Loopback требует неизменную revision между snapshot без transport dispatch
  и её увеличение после следующего фактически принятого состояния.

### Source NetPlayer graph-rotation authority follow-up

- Устранено расхождение между `NetPlayer::ResponseStream` и Jolt-границей.
  Windows сравнивает сетевой quaternion с текущим `GameCar` graph actor,
  тогда как Jolt-адаптер повторно сравнивал его с physics body и мог отменить
  уже принятое игровым слоем решение о snap. Теперь результат исходного
  порога `pi/24` явно передаётся в physics backend.
- Поздний UDP pose больше не применяется, если локальный `Player` уже
  финишировал, даже когда replicated `_raceFinish` ещё не дошёл. Это точно
  повторяет проверку `!_player->GetFinished()` до изменения машины.
- Physics smoke покрывает случай, когда graph actor требует snap, а новая
  разница физического кузова меньше `pi/24`; Jolt обязан сохранить решение
  `GameObject/NetPlayer`, а не пересчитать его по другой позе.

### Source Player::ApplyMobility armor-role follow-up

- Удалено portable-отклонение, из-за которого коэффициент
  `Player::cHumanArmorK` выбранной сложности применялся ко всем машинам.
  Windows `Player::ApplyMobility` умножает суммарный `maxLife` только для
  `IsHuman() || IsOpponent()`; обычные `Computer1..Computer5` теперь снова
  получают базовую броню из своих tournament slots.
- В сетевом roster существующий `Racer::human` сохраняет обе управляемые
  человеком source-роли: owner Human и remote Opponent. Поэтому исправление
  не меняет wire format и не лишает удалённых игроков коэффициента сложности.
- Resource smoke отдельно прогоняет Easy/Normal/Hard для Human и Easy/Hard
  для Computer: ожидает точные отношения `2.0/1.75/1.5` только у человека и
  неизменный `maximumLife` обычного AI.

### Source Player::TakeBonus value/slot-order follow-up

- Удалена придуманная семантика medpack: portable-код при serialized
  `damage <= 0` полностью восстанавливал машину. Windows всегда вызывает
  `GameObject::Healt(value)`, поэтому теперь прибавляется ровно исходное
  значение, включая ноль из штатного `ctBonus/medpack`.
- Восстановлен порядок кандидатов ammunition pickup из непрерывного source
  диапазона `stHyper..stWeapon4`: Hyper, Mine, затем Weapon1–Weapon4. Это
  существенно для `Round((count-1)*Random())`, поскольку перестановка списка
  меняла конкретный пополняемый слот при том же результате RNG.
- Session smoke повреждает машину перед offline и сетевым medpack pickup и
  проверяет точный `min(life + value, maxLife)`, уничтожение pickup и событие
  HUD/achievement; сетевой enum `Money/Charge/Medpack/Immortal = 0/1/2/3`
  сохранён без изменений.

### Source GameObject listener/behavior dispatch follow-up

- Добавлен исходный `GameObjListener` lifecycle: уникальная non-owning
  регистрация, безопасная рассылка по snapshot, `OnDamage`, `OnDeath`,
  `OnLowLife` и отдельный финальный `OnDestroy`.
- Порядок Windows-кода сохранён: authoritative life записывается до damage
  callbacks, touch attribution меняется после них, death callbacks получают
  исходный `DamageType` и target. Копирование объекта очищает listener links.
- `Player` теперь доставляет damage/immortality своим `ImmortalEffect` и
  `EnergyDamageEffect` через GameObject event overrides. Ручной параллельный
  вызов из race session удалён, поэтому behavior не может сработать дважды.
- `LowLifePoints` рассылает `OnLowLife` перед первым source effect, а окончание
  timed immortality автоматически рассылает status=false. Два unit smoke и
  integrated physics smoke покрывают активные переходы.

### Source Player::ResetCar owner/fallback follow-up

- Алгоритм `Player::ResetCar` вынесен из race-session adapter в active
  `source::Player`: `GetLastNode`, сохранённая coordinate, rays `0/-2/+2`,
  максимум пять шагов назад по 6 единиц и переход на предыдущую tile теперь
  исполняются одним исходным owner. Session оставляет только world/Jolt
  ray-query и применение возвращённого transform.
- Исправлены два подтверждённых расхождения. Ray origin использует source
  `ComputeHeight(0.5)/2`, а не высоту в текущей coordinate. Полностью
  неуспешный поиск сохраняет первоначальную fallback pose и больше не
  телепортирует машину в последнюю заведомо заблокированную точку.
- При отсутствующем `lastNode` выбирается первый node main path, как
  `Player::GetLastNode`; invented fallback по `nextPathNode` удалён. Unit
  regression покрывает variable-width trace, initial death-plane restart и
  blocked search, resource physics smoke — реальный reset на map1.

### Source GameObject damage-event ownership follow-up

- `cPlayerDamage` и `cPlayerKill` больше не конструируются race-session
  вручную. `GameObject::Damage` рассылает их в исходных точках: Damage после
  listener callbacks, Kill после перехода в death и до `Player::OnDeath`.
- Восстановлена смертельная последовательность Windows:
  `Damage -> Kill -> Overboard/DeathMine -> Death`. Для mine нет ложного
  Kill, а death-plane сохраняет трёхсекундного touch attacker только в
  `cPlayerDeath`.
- `Player::TakeBonus` снова владеет `bonus->Death()` и выполняет его до
  изменения денег, здоровья, бессмертия или зарядов. Listener regression
  фиксирует этот порядок.

### Source AchievmentModel owner block

- Все девять `AchievmentCondition` вынесены из монолитного
  `OriginalRaceSession` в active `source::AchievmentModel`: состояние
  условий, SpeedKill timer, iterations, campaign multiplier, first-kill и
  reset lifecycle теперь принадлежат исходному классу.
- Race-session только переводит уже source-owned игровые события в
  backend-neutral `EventData`. Damage сохраняет sender/target orientation,
  а TouchKill теперь потребляет `cPlayerDeath`, как Windows, не synthetic
  Kill death-plane marker.
- Сохранена фактическая ветвь исходника: `Race::OnLapPass` отправляет
  `cRaceFinish` с NULL data, поэтому общий human-data guard класса LapPass
  отвергает его. Одновременно final-lap LapBreak обрабатывается до перехода
  модели в finish state, а не теряется из-за прежнего session-флага.
- Отдельный unit smoke покрывает классы 1–9, timeout, persistence,
  difficulty scoring и NULL finish event; 12 non-network CTest, resource,
  map1 physics и 240-frame Metal/Jolt smoke проходят.
- Follow-up закрыл reward layer из того же Windows translation unit:
  `Achievment/AchievmentMapObj/AchievmentGamer`, три состояния, purchase с
  атомарным `ConsumePoints`, map-object/gamer gates и item persistence теперь
  принадлежат active `source::AchievmentModel`. AchievmentFrame и Garage
  больше не содержат собственные копии этих правил.

### Source Race::OnLapPass/CompleteRace owner block

- `Player::OnLapPass` снова сам увеличивает `CarState::numLaps` и полностью
  перезаряжает source slots перед делегированием в Race. Session больше не
  повторяет эти две операции вручную.
- Добавлен active `source::RaceLifecycle`, владеющий эквивалентом
  `Race::_results`, назначением мест/planet reward, `voiceNameDur=1.5`,
  сортировкой незавершивших машин и исходным порядком событий
  Lead/Second/Third/Last, RaceFinish, PassLap, LastLap.
- Исправлено подтверждённое отклонение: `cRacePassLap` теперь создаётся только
  для локального Human, а не для каждого AI. AI-only гонка завершает race,
  когда финишировал весь её фактический состав, как ветвь
  `GetPlayerById(cHuman) == NULL` в Windows.
- `pickMoney` захватывается в `RaceResult`, после чего сбрасывается у Player.
  Finish HUD, FinishMenu, network results и campaign settlement читают
  source-owned result, поэтому сумма не теряется и не начисляется дважды.
- Новый unit smoke проверяет event order, отсутствующие события средних мест,
  AI-only completion и принудительную сортировку Human/Opponent после AI.
  13 non-network CTest, resource verifier и полный map1 physics smoke проходят.

### Source Race::OnLateProgress place owner block

- Перенесён отдельный `source::RacePlaceModel`, владеющий эквивалентом
  `Race::_playerPlaceList`, прошлым Leader/Third и точной state machine
  `Race::OnLateProgress`. Session теперь только подаёт source `CarState::GetLap`
  и применяет вычисленные места/события к HUD и Commentator.
- Исправлено фактическое расхождение сетевого финиша: прежний score
  `100000-finishTime` перезаписывал authoritative `Result::place`, особенно
  когда все принятые результаты имели одинаковое локальное finish time.
  Завершившие игроки теперь сортируются строго по сохранённому place, как
  Windows comparator.
- LeadChanged/ThirdChanged используют предыдущий упорядоченный список, а не
  поиск по уже изменяемому полю `Player::place`. Thresholds 300/70 и запрет
  Lead/Third/Domination после первого Result находятся в source owner.
- Tombstone удалённого network racer приводит к очистке предыдущего place
  list, повторяя `Race::DelPlayer`; ложная реплика о смене лидера против уже
  удалённого участника больше не возникает.
- Unit regression покрывает authoritative finish ordering, lead swap и
  roster removal. 13/13 non-network CTest и полный map1 physics smoke проходят.

### Source Race::ExitRace early-completion block

- Восстановлен обязательный первый вызов `Race::CompleteRace(results)` из
  Windows `Race::ExitRace`. При подтверждённом досрочном выходе session теперь
  ранжирует всех оставшихся игроков по source правилам, фиксирует Results,
  сбрасывает picked money, начисляет campaign rewards и только затем сохраняет
  профиль либо сериализует сетевой результат.
- Исправлена неверная UI-ветвь portable runtime: offline
  `HudMenu -> Menu::ExitRace -> GameMode::ExitRaceGoFinish` ведёт в исходный
  FinishMenu, а не прямо в RaceMenu2. Таблица результатов получает даже
  досрочно завершённую гонку; переход после её закрытия остаётся в общей
  source-derived `originalFinishTransition`.
- Для network host порядок также восстановлен: Complete/Save выполняются до
  `OnExitRace` RPC, поэтому пакет содержит окончательные place, money, points
  и captured pickMoney, как `NetRace.cpp`.
- Physics regression завершает гонку из countdown/ранней racing state,
  проверяет результат каждого активного игрока, немедленную готовность
  FinishMenu и идемпотентность наград. 13/13 CTest и map1 smoke проходят.

### Source Player::CheatUpdate owner correction

- Таблицы rubber-banding больше не исполняются внутри race-session. Active
  `Player::CheatUpdate` владеет поиском reference player, fractional lap
  distance, Easy/Normal/Hard speed limit и torque/steering coefficients;
  `CarState` хранит исходные `cheatSlower/cheatFaster`.
- Подтвердилось функциональное расхождение прежнего порта: reference выбирался
  среди всех машин, хотя Windows принимает только `cHuman` и роли из
  `cOpponentMask`. Далёкий Computer больше не заставляет другой Computer
  необоснованно ускоряться или замедляться; offline AI сравнивается с Human,
  network AI — с наиболее впереди идущим Human/Opponent.
- Восстановлен fixed-step order: все `Player::CheatUpdate` выполняются до
  `AISystem::OnProgress`, поэтому `AICar::ControlState` видит
  `cheatSlower` в текущем кадре. Jolt adapter получает только готовые source
  torque/steering scales.
- Player unit regression проверяет faster, slower и исключение Computer из
  reference set; integrated AI catch-up/role regressions и map1 physics smoke
  проходят вместе с 13/13 CTest.

### Source Player::OnProgress fixed-step owner block

- Остатки `Player::OnProgress` больше не разбросаны между началом и концом
  session update. Active `source::Player` одним вызовом владеет
  `CheatUpdate`, destroy/restore branch, сбросом cheat flags без машины и
  `_block` countdown; Jolt adapter оставляет у себя только `CarState::Update`
  из фактической physics pose и применение готовой команды к vehicle input.
- Исправлено подтверждённое расхождение стартового отсчёта: Windows вызывает
  `Player::OnProgress` на каждом fixed step и ограничивает `_goRace` только
  для `AISystem`. Порт раньше полностью пропускал Player/CarState во время
  countdown. Теперь trace ownership, start brake и cheat state готовы до
  первого AI frame.
- Restore и block снова исполняются до `AISystem::OnProgress`. При этом
  выявлен скрытый adapter bug: после source `AIPlayer::FreeCar` порт всё равно
  присваивал пустой результат `aiInput`, стирая уже выставленный финишный
  brake. Запись AI input теперь выполняется только при `HasCar()`, поэтому
  финишировавшие компьютеры остаются заторможенными и не продолжают гонку.
- Player unit regression покрывает общий owner и destroyed branch;
  campaign AI finish regression, 13/13 non-network CTest, map1 physics и
  FinishMenu smoke проходят.

### Source Player::FindClosestEnemy owner block

- Удалена session-лямбда наведения оружия. `source::Player` теперь владеет
  исходным `FindClosestEnemy`: фильтром живых map objects, минимальной
  абсолютной дистанцией до forward plane и проверкой view cone.
- Исправлены два потерянных branch. Отрицательный `viewAngle` снова означает
  задний сектор через `angle <= cos(pi/2-viewAngle)`, а `zTest` использует
  `curTile->IsZLevelContains` для многоуровневых участков трассы. Нулевой угол
  по-прежнему отключает cone filter для `sphereGun`.
- `CarState` теперь отдельно хранит полный нормализованный `dir3`; прежний
  session surrogate передавал только XY-направление и терял наклон машины при
  3D plane/cone targeting. Trace-направление остаётся отдельной XY-проекцией.
- Прямой Player regression различает plane distance от Euclidean distance,
  передний/задний sectors и нижний Z-level. Существующие lethal impulse chain,
  sphereGun homing, 13/13 CTest, map1 physics и 240-frame Metal/Jolt smoke
  проходят через новый owner.

### Source Player bonus-projectile owner block

- `Player::_bonusProjs` и `_nextBonusProjId` перенесены в active
  `source::Player`. Session-вектор `nextNetworkProjectileIds_` удалён;
  `NetPlayer::DoShot`, replicated shot и MineContact теперь используют
  Player-owned identity/liveness registry.
- Исправлено подтверждённое расхождение: portable counter увеличивался после
  любого primary/hyper shot. В Windows id меняется только после успешного
  `Player::Shot(stMine) -> InsertBonusProj`; обычные выстрелы используют
  текущий id, не занимая его.
- Уничтожение, timeout и MineRip снимают id тем же listener lifecycle, который
  выполняет `Player::OnDestroy -> RemoveBonusProj`; disconnect очищает
  оставшиеся ссылки без перемотки sequence. Входящий MineContact принимается
  только если id ещё жив в owner Player.
- Unit regression проверяет insert/remove/clear/next-id. Integrated network
  regression фиксирует ordinary id=1 без инкремента, replicated mine id=77,
  next=78 и удаление записи после контакта; 13/13 CTest, map1 physics и
  240-frame render smoke проходят.

### Source Player CreateCar/FreeCar state lifecycle follow-up

- `source::Player::CreateCar/FreeCar` теперь владеют полным состоянием
  исходных Windows-методов. Каждый create сбрасывает `moveInverse`, его
  таймер, накопленный максимум скорости и timer `LostControl`; restore больше
  не наследует эти значения от взорванной машины.
- `CreateCar(true)` устанавливает первый main-path node, очищает registry мин,
  возвращает следующий bonus projectile id к 1 и обнуляет restore timer.
  `FreeCar(true)` очищает trace nodes и число кругов.
- Удалён отсутствующий в исходнике fallback mini-map на первую точку трассы:
  без current tile и last node возвращается NullVector. Unit, 13/13 CTest,
  resource, map1 physics и 240-frame Metal smoke проходят.

### Source HumanPlayer control-gate follow-up

- Перенесён точный порядок `HumanPlayer::Control`: block/отсутствующий car
  запрещают весь control; chat запрещает event actions и continuous
  Hyper/Mine, но не уже удерживаемые gas/steering; debug human AI подавляет
  continuous progress отдельно от `OnHandleInput`.
- Исправлён обход gate непрерывной миной из SDL held state, а finish/countdown
  block больше не пропускает weapon, mine, hyper или reset в gameplay.
- Контактное правило `HumanPlayer::ResetCar` вынесено в source owner: нужен
  существующий car и wheel либо body contact. Main передаёт реальный
  `UserChat::inputVisible`; 13/13 CTest, map1 physics и 240-frame Metal smoke
  проходят.

### Source Race start/go state follow-up

- Добавлен active `source::RaceRunState` для исходных `_startRace/_goRace` и
  `Race::StartRace/GoRace/ExitRace` player transitions.
- Исправлено подтверждённое физическое расхождение старта: Human теперь имеет
  `ResetBlock(true)` на всех четырёх секундах offline/network countdown, и
  `Player::OnProgress` подаёт полный brake вместо свободного качения Jolt-car.
- `GoRace` снимает block на зелёном сигнале; `DEBUG_PX` делает это немедленно,
  network — на stage 4. Unit и integrated countdown regressions, 13/13 CTest,
  map1 physics и 240-frame Metal smoke проходят.

### Source Player identity/role follow-up

- Active `source::Player` теперь владеет Windows-полями id/gamerId/netSlot,
  network/tournament name и color, а также точными битовыми предикатами
  Human/Computer/Opponent.
- Tournament и network roster назначают окончательный `Race::Player` ID по
  исходным правилам `Race::AddPlayer`/`NetPlayer`: удалённый human становится
  opponent через `netSlot << 8`, не AI и не локальным Human.
- Damage authority, AI, catch-up, lap/finish и race start consumers переведены
  с угадывания роли по vector index/definition flag на Player owner. Unit,
  resource и map1 physics regressions проходят.

### Source Race ExitRace teardown follow-up

- Автоматический и ручной finish теперь проходят через исходный
  `Race::ExitRace` до открытия FinishMenu.
- Active owner освобождает Player CarState, AI cars, Droid lifecycle,
  projectiles/mines/effects, contact state и pending physics/network queues;
  bonus/decoration map objects больше не остаются активными за меню.
- Повторный вызов idempotent, результаты/награды FinishMenu сохраняются.
  Unit, integrated map1 physics и FinishMenu renderer smoke проходят.

### Source GameMode race clocks follow-up

- `GameMode::_goRaceTime/_finishTime` перенесены в active
  `source::GameModeRaceState`; session больше не является владельцем
  countdown и finish clocks.
- Сохранены `cGoRaceLag=1`, последовательность wait/1/2/3/go, немедленный
  `DEBUG_PX`, host-controlled network stages и остановка обоих clocks при
  pause.
- Finish transition использует точное исходное условие `> 3.0f`; unit и
  13/13 non-network CTest проходят.

### Source Player light attachment follow-up

- `Player::CreateCar/ReleaseCar` теперь явно владеют attachment двух spot
  lights и `_nightFlare`; уничтоженная машина не оставляет прожекторы в
  старой физической позиции, restore присоединяет их обратно.
- `_nightFlare` исключён из cube/water reflection и refraction в соответствии
  с отсутствующими `gpReflScene/gpReflWater` в исходном `GraphDesc`.
- Renderer больше не выводит flare по одной только погоде: он читает
  `Player::HasAttachedLights()`.

### Source GameMode pause audio follow-up

- Перенесён полный `GameMode::Pause`: world/session clocks и Jolt frozen,
  `Logic::scEffects` отображается в нулевую громкость SDL Effects bus.
- Resume восстанавливает актуальный `effectsVolume`; Music/Voice остаются
  нетронутыми по исходному коду.
- Integrated smoke проверяет mute и restore вместе с уже существующей
  проверкой неподвижного автомобиля и замороженного race time.

### Source Player runtime presentation follow-up

- Подтверждён и удалён parallel presentation state: кузов машины,
  track/cushion visuals, mini-map, kill notification, opponent label и
  FinishMenu больше не читают устаревшие name/color из `Race::Racer`, если
  существует active `Player`.
- `NetPlayer::OnSetGamerId/OnSetColor` теперь отображаются в runtime session
  перед применением remote vehicle snapshot. Это возвращает исходную
  семантику `Player::GetName/GetColor` и одинаковое отображение владельца и
  удалённых участников.
- Resource descriptor сохранён как fallback и владелец статических assets;
  unit/session regression проверяет gamer id, цвет и неверный slot.

### Source Player cheat owner / AIPlayer lifecycle follow-up

- `_cheatEnable` перенесён из parallel portable `AIPlayer` в active
  `source::Player`, вместе с исходными `GetCheat/SetCheat`.
- `AIPlayer` теперь повторяет constructor/destructor side effects Windows:
  Computer получает Faster+Slower на время жизни AI owner, release/rebind
  возвращает Disabled. Move lifecycle сохраняет единственного владельца.
- Удалён отсутствующий в оригинале network-Human `cheatEnableFaster`.
  Remote Opponent больше не получает фиктивный AI controller; его машина
  остаётся под authoritative network snapshots. Reset очищает AI owners до
  замены Player storage, исключая dangling owner state.
- Player, AIPlayer, offline session и network-role regressions подтверждают
  source masks и teardown.

### Source Player::ComputeCarBBSize follow-up

- Удалены три AI-аппроксимации размера машины по collision half-extents.
  Оригинал использует диагональ transformed visual AABB `GrActor`, поэтому
  collision shape не является эквивалентным источником.
- Resource loader вычисляет visual `boundingSize/boundingRadius` из mesh
  bounds всех body nodes с их source transforms; active `Player::CarState`
  хранит эти значения через весь create/restore lifecycle.
- `AISystem::ComputeTracks`, path/control и `AICar::AttackState` теперь читают
  один Player-owned size/radius. Это возвращает исходные интервалы lane
  blocking, удержания цели и обгона независимо от формы Jolt backend body.
- Resource, Player и integrated session regressions проверяют формулу и
  передачу значения каждой configured vehicle.

### Source Player::GetName/GetPhoto follow-up

- Добавлен полный presentation-каталог `Planet::PlayerData` из
  `tournamet.xml`; lookup сохраняет исходный приоритет global gamers над
  игроками только текущей планеты, а не над всеми планетами сразу.
- HUD, kill popup, встроенная таблица финиша и отдельный FinishMenu выбирают
  локализованное имя и портрет по текущему `Player::GetGamerId`; сетевое имя
  по-прежнему имеет приоритет.
- `Race::StartRace` replacement дублирующегося персонажа рассматривает
  только `Tournament::GetGamers`, как Windows-код. Resource regression
  проверяет global и planet-local совпадающие ids.

### Source Player::ApplyColorMat / CarFrame color follow-up

- Удалён общий tint автомобиля: исходный `Player::ApplyColorMat` окрашивает
  sampler только первого mesh-node корневого кузова. Гусеницы, подушки,
  колёса и дополнительные body nodes сохраняют свои материалы.
- `motor/disableColor` загружается из каждого ctCar record и блокирует
  override так же, как `RockCar::GetDisableColor` в Windows.
- Garage/Workshop/RaceMain presentation теперь меняет цвет active Player;
  сетевые viewport-машины получают собственный `NetPlayer` color, а cache
  обновляется при `OnSetColor`. Source-палитра из 14 цветов сохранена.

### Source Player::Shot owner follow-up

- Все primary, Hyper и Mine транзакции перенесены с прямых session-вызовов
  `WeaponItem::Shot` в active `source::Player::Shot`.
- Сохранена точная граница Windows: backend сообщает, был ли подготовлен
  хотя бы один снаряд; `newCharge` применяется и при неуспехе, но только
  успешный `stMine` попадает в Player-owned bonus-projectile registry и
  продвигает следующий сетевой projectile id.
- Human, AI и replicated network shots используют один owner; regression
  отдельно проверяет primary, mine и failed replicated mine переходы.

### Source Player race-result/economy owner follow-up

- `money`, `points`, collected money, place и finished закрыты внутри
  active `source::Player`; перенесён полный исходный Get/Set/Reset API.
- Session lifecycle, profile adapter, place ordering, AI/fire gates, HUD,
  FinishMenu и network results больше не обходят Player прямой записью.
- `Race::CompleteRace`-эквивалент сохраняет collected money в result и затем
  вызывает `ResetPickMoney`; `SetFinished` остаётся единственным переходом,
  который одновременно включает постоянную неуязвимость финишировавшей
  машины.

### Source WeaponItem current-charge owner follow-up

- `WeaponItem` теперь буквально владеет `_curCharge`, как Windows-класс;
  resource/profile arrays служат только входом при первоначальном binding.
- AI, HUD, выбор оружия, бонусы, lap reload, primary/Hyper/Mine и network
  paths переведены на один Slot-owned предмет. Staging-копия намеренно не
  изменяется после binding, что исключает прежнее расхождение владельцев.
- Weapon, Player, lifecycle и integrated race regressions проверяют shot,
  failed replicated `newCharge`, reload и detached/attached lifecycle.

### Source WeaponItem `_wpnDesc` follow-up

- Восстановлен Player-owned полный descriptor непосредственных projectiles:
  type/speed/maxDist/damage, `GetWpnDesc/SetWpnDesc` и применение к Weapon при
  создании машины.
- `GetDamage` теперь суммирует projectile damage, как Windows, вместо чтения
  помеченного исходником invalid поля. Workshop `chargeCost` также загружается.
- AI получает характеристики primary/Hyper/Mine из live Slot item; resource,
  unit и integrated race regressions подтверждают реальные значения
  bulletGun/rifleWeapon и car lifecycle.

### Full Proj::Desc firing-owner follow-up

- Weapon item хранит полное описание непосредственных снарядов, включая
  transforms, collision, relative motion, lifetime и death/nested data.
- Primary/Hyper/Mine preparation теперь читает live Weapon descriptor; static
  race catalog остаётся только asset/index bridge для bgfx/Jolt и generated
  death projectiles.
- Regression с post-bind заменой descriptor подтверждает, что новый projectile
  получает item-owned speed `77`, maxDist `321` и damage `9.25`.

### Immutable fired-projectile descriptor follow-up

- `ProjectileRuntime` и `MineRuntime` сохраняют immutable handle на
  item-owned descriptor, соответствующий копии `Proj::Desc` в Windows.
- Update/contact/death/MineRip и renderer больше не перечитывают gameplay
  параметры из mutable slot или static race catalog после выстрела.
- Общий handle не копирует visual graph для каждого projectile; regression
  меняет live descriptor и доказывает, что уже созданный снаряд сохраняет
  прежние speed/maxDist/damage.

### Complete source profile schema follow-up

- `OriginalProfileStore` теперь покрывает полные source schemas
  `user.xml`, `race.xml`, `Profile/*.xml` и `achievment.xml`, включая обе
  библиотеки профилей, десять slots/charge и temporary `SkProfile`.
- Восстановлен вызов `Race::CompletePlanet` при загрузке: значение `4`
  раскрывает скрытую планету `5`, дубликаты удаляются, profile cursors
  проверяются через общий список, как Windows `FindProfile`.
- Расширенный disk regression меняет и возвращает все группы config,
  tournament/player и achievement полей.

### Source MusicCat lifecycle/persistence follow-up

- Game MusicCat больше не выбирает трек при запуске приложения: первый
  playlist entry только прогревается фоновым decoder и извлекается исходным
  `Play` при `DoStartRace` с позиции 0.
- `ExitRace` теперь повторяет Windows `Stop`, не выполняя прежний придуманный
  `Pause+Next`; следующая гонка сама извлекает следующий элемент.
- `GameMode::SaveConfig`-эквивалент записывает фактически оставшиеся menu/game
  очереди в `user.xml`. PCM cursor остаётся только in-process Pause/Resume и
  не восстанавливается между запусками обычной игры.
- M8 audio и 240-frame M9 Metal regressions подтверждают обе очереди, все три
  menu-трека, автоматический Next и deferred zero-frame game start.

### Source MusicCat clean-start/pause follow-up

- `ResetConfig` снова оставляет обе playlist пустыми: удалены придуманные
  стартовые снимки, из-за которых чистая установка всегда запускала заранее
  выбранную композицию вместо исходного случайного menu cycle.
- `LoadUser` сохраняет дубликаты и некорректные индексы буквально; как в
  Windows, только `Play` пропускает несуществующие треки. Background decode
  проверяет границы отдельно.
- `Pause(true)` запоминает PCM frame и останавливает backend voice;
  `Pause(false)` создаёт его заново с сохранённой позиции. Это повторяет
  `StopMusic`/`PlayMusic`, а не придуманное удержание paused mixer voice.
- Profile/M8/Finish regressions проверяют clean `user.xml`, duplicate/invalid
  queue и отсутствие активного музыкального голоса во время паузы.

### Serialized MusicCat catalog follow-up

- Перенесён `GameMode::LoadGameData -> MusicCat::LoadGame`: menu/game paths,
  title, band и group читаются из поставляемого `game.xml`.
- Удалено использование аварийных `ResetGameData` metadata в normal path;
  MusicDialog теперь показывает поставляемые `Frantick/The Ventures`, а
  game shuffle использует все 11 сериализованных записей и их группы.
- Loader проверяет ссылки на Ogg; resource audit фиксирует точные 3/11 counts
  и ключевые metadata, M8/M9 regressions используют этот же каталог.

### Serialized GameMode language/commentator follow-up

- `GameMode::LoadGameData`-совместимый loader читает из `game.xml` все шесть
  languages с `file/locale/charset/primId` и оба commentator styles в
  исходном порядке.
- StartOptions и SoundOptions больше не используют вручную повторённые
  массивы; stepper и profile values индексируют один загруженный каталог.
- MainMenu2 поддерживает English, Russian, Portuguese, French, Spain и German.
  Parser повторяет token semantics Windows `StringLibrary::Load`, включая
  незакрытую `scMaslo` во французском поставляемом файле.
- Resource verification успешно выполнена отдельно для всех шести языков и
  проверяет точные 6/2 records и их source metadata.

### Source Commentator descriptor/player-id follow-up

- Единый `OriginalGameData` catalog теперь содержит полный
  `Commentator::LoadGame`: global delay, 37 comments, busy/repeat state и все
  weighted voice descriptors; audio runtime больше не имеет отдельного
  permissive XML parser.
- Выбранный commentator style проверяется по сериализованным двум стилям, а
  недоступные в конкретном переводе Ogg остаются в descriptor и фильтруются
  при `Generate`, как после `ResourceManager::LoadCommentator/CheckSounds`.
- `forHuman`, `repeatPlayer` и prefix/suffix generation получают настоящий
  битовый `Player::GetId`; countdown/race-finish без `EventData` используют
  source `undefinedId`, а не ошибочный индекс машины 0.

### Shared StringLibrary/HUD localization follow-up

- Устранён несовместимый `readText` UTF-16LE path, из-за которого HUD всегда
  молча оставлял английские fallback labels при выбранном другом языке.
- Windows `StringLibrary::Load` теперь один раз реализован в game-side
  `OriginalGameData` и используется MainMenu2, HUD, minimap notifications и
  finish overlay; duplicate/malformed/`\\n` behavior больше не расходится.
- Russian Metal smoke печатает и отображает `Круг`/`Награда`; все player,
  place, money/points и finish-format keys берутся из той же выбранной
  сериализованной language library.

### RaceMenu command localization follow-up

- Удалены оставшиеся придуманные английские identity labels `Start race` и
  `Planets` из active RaceMenu/Angar path.
- Навигация и diagnostic trace используют поставляемые `svStartMatch` и
  `svCasePlanet`; сам `RaceMainFrame`, как Windows, продолжает рисовать семь
  icon-only кнопок без текстовых подписей.

### Source StringLibrary owner follow-up

- `ResourceManager::StringLibrary` перенесён как общий owner с точными
  `Get`/`Set`/`Has` semantics; MainMenu, HUD и dialogs больше не выполняют
  собственные варианты lookup/fallback.
- Пустой `svNull` и отсутствующий key возвращают сам id, как Windows.
  Придуманные английские network/player fallback удалены; verification
  фиксирует фактическое отсутствие `svHintLeaversWillBeRemoved` в shipped
  localization вместо маскировки дефекта ресурсов.

### Unified GameMode game-data catalog follow-up

- Один `OriginalGameData::Catalog` повторяет полный порядок
  `GameMode::LoadGameData`: languages, commentator styles, menu/game
  `MusicCat::LoadGame` и Commentator table.
- Active startup больше не разбирает `game.xml` отдельно для меню и музыки;
  MainMenu, MusicDialog, обе очереди и commentator получают один owner.
- Независимый TinyXML parser из `OriginalAudioSpec.cpp` удалён; compatibility
  API делегирует общему loader и не содержит второй интерпретации формата.

### Source language/commentator autodetection follow-up

- `ProfileState` различает отсутствующие и реально записанные `language`/
  `commentatorStyle`, поэтому clean first launch снова вызывает исходный
  autodetect вместо скрытых English defaults.
- macOS preferred locale преобразуется в serialized Windows `primId` для
  всех шести языков; неизвестный id выбирает первую запись, как
  `GameMode::AutodetectLanguage`.
- `lcRussian -> russian`, иначе `english`, иначе первый style повторяет
  `AutodetectCommentatorStyle`; обычный `SaveConfig` фиксирует выбранные
  значения в `user.xml`.

### Source ControlManager/config follow-up

- Точные 29 keyboard/gamepad `VirtualKeyInfo`, 25 `GameAction` и оба набора
  constructor defaults имеют одного portable owner вместо ручных таблиц в
  профиле, SDL adapter и ControlsFrame.
- Частичный controls XML теперь, как `GameMode::LoadGameOpt`, накладывается на
  defaults и игнорирует unknown actions; save пишет все actions в исходном
  порядке и канонизирует `GetVirtualKeyFromName`/`None` semantics.
- Захват D-pad/buttons/thumb directions/triggers сохраняет Windows-имена и
  использует XInput thresholds 7849/8689 и 30/255. Удалён несовместимый
  keyboard `Back -> Backspace` alias; источник использует `Escape` для
  `vkBack`, а Backspace сериализует через keyboard `vkButtonX` как `X`.

### Complete GameMode options serialization follow-up

- `LoadGameOpt` различает absent и partial `quality`/`volume`: первый случай
  вызывает исходные autodetect policy, второй сохраняет constructor values
  для отсутствующих дочерних полей.
- `frameRateMode` принимает только `sfrNone`/`sfrFixed`, `prefCamera` — только
  `pcThirdPerson`/`pcIsometric`; invalid camera token снова включает
  обязательный StartOptions path. Придуманный `sfrVSync` удалён.
- Сняты отсутствовавшие в Windows loader ограничения volume 0..2 и
  cameraDistance 0.6..2.5. UI по-прежнему предлагает штатные source ranges,
  но существующий Windows `user.xml` теперь читается и пишется без изменения
  его float state.

### Source first-launch config recovery follow-up

- Отсутствующий `user.xml` теперь отдельно отличается от файла с частично
  отсутствующими полями; после source language/commentator autodetect startup
  немедленно выполняет `GameMode::ResetConfig -> SaveConfig`.
- Узкий `saveConfig` пишет только `user.xml`: первый запуск больше не создаёт
  преждевременно `race.xml`, профиль или achievements.
- Как в Windows, записанный default `pcIsometric` не отменяет StartOptions в
  текущем процессе: camera-autodetect и discrete-GPU state завершаются только
  в `CheckStartupMenu`.
- Все automated smoke paths используют отдельный временный profile store и
  отделены как от normal shutdown persistence, так и от ранних ProfileFrame
  save operations; настоящий Application Support больше не изменяется.
- Race-render regression детерминированно seed-ит default profile во
  временном store, поэтому Tournament Load/ProfileFrame coverage не зависит
  от истории ручных запусков при пустом shipped `race.xml`.

### Source FinishMenu audio-close follow-up

- Menu MusicCat остаётся paused на своей позиции во время всей таблицы
  результатов; преждевременное проигрывание меню поверх FinishMenu удалено.
- `Menu::OnFinishClose -> GameMode::OnFinishFrameClose` останавливает очередь
  диктора, возобновляет menu track с gain 0 и повторяет односекундную source
  fade-формулу до настроенной Music volume.
- Finish smoke теперь закрывает frame и проверяет hold/stop/resume/fade, а не
  только визуальные строки и постановку last-place voice в очередь.

### Source GameMode termination follow-up

- Process exit теперь повторяет `GameMode::Terminate`: сохраняет только
  `user.xml` с актуальными MusicCat queues до audio shutdown.
- Удалён shutdown-вызов полного `saveRaceProfile`; закрытие окна больше не
  записывает текущую гонку/achievements и не продвигает tournament.
- Полное сохранение остаётся только на явных исходных Race/Menu boundaries;
  automated shutdown проверяет config path в изолированном временном store.

### Source GameMode planet-transition follow-up

- `GameMode::ChangePlanet` больше не открывает произвольный
  `psClosed`/`psUnavailable` destination: `Planet::Unlock` разрешён только для
  `GetNextPlanet` после подтверждённого planet champion.
- Перед переходом adapter восстанавливает текущий planet/pass/track в
  перенесённом `source::Tournament`, поэтому повторный выбор текущего мира и
  `Tournament::ChangePlanet` имеют тот же порядок побочных эффектов, что и
  Windows.
- Tournament regression проверяет champion-next, unavailable non-next и
  already-current branches, включая сохранённые state/pass значения.

### Source skirmish computer-loadout follow-up

- `Planet::StartPass` снова применяет к computer players исходный
  `Garage::MaxUpgradeCar` с уровнем из GameMode options.
- У всех установленных weapon items восстанавливается максимальный боезапас,
  затем primary mounts выше `weaponMaxLevel` удаляются в исходном порядке.
- Пересчитанная mobility-конфигурация поступает в Jolt spawn; human и remote
  opponent loadouts остаются профильными/сетевыми.

### Source race Source3d emitter follow-up

- Motor proxy теперь создаётся остановленным и начинает backend playback
  только внутри исходного `distScaler=30`; диапазон 30..45 м остаётся
  stop-lag, а не стартовой зоной.
- `PxWheelSlipEffect` сохраняет исходное разделение координат: след/дым
  создаётся в contact point, звук следует за world position объекта колеса.
- RPM lag, idle/RPM mix, slip thresholds 0.4/0.7, volume multiplier 4 и
  плоское затухание 30/45 м остались прямыми переносами Windows-кода.

### Source ControlManager runtime follow-up

- SDL manager сам устанавливает точные Windows constructor bindings до
  чтения `user.xml`; runtime больше не имеет отдельной invented fallback
  раскладки.
- Удалены отсутствующие в источнике WASD/Space/Backspace, mouse-wheel,
  mouse-shot и unconditional left-stick actions. Mouse left сохраняется
  только как platform-dispatch на реально найденный menu widget.
- SDL hot-plug сохраняет XInput `XUSER_INDEX_ANY` для кнопочных событий;
  disconnect освобождает все held actions конкретного устройства.
- Analog normalization повторяет `alphaMax/alphaThreshold`: trigger 30/255,
  left thumb 7849/32767 и right-thumb activation 8689/32767 с исходной
  directional-table особенностью.

### Source DataBase/RecordLib ownership follow-up

- Семь typed `MapObjRecordLibrary` перенесены из `source::Map` в отдельный
  active `source::DataBase`, повторяющий lifetime Windows `World::DataBase`.
- Единый `Configure` теперь владеет clear/load/fix-up для records текущей
  карты; `OriginalRaceSession::registerSourceDataBase` и его повторные
  source-loader lambdas удалены.
- `Map` хранит только live categories/global IDs и получает стабильные
  records от DataBase. Regression проверяет source-before-proxy загрузку,
  destructible includes, car life, lookup и безопасную смену карты.

### Source HudMenu policy follow-up

- Добавлен отдельный `source::HudMenu`, повторяющий единственное исходное
  состояние `msMain`, видимость MiniMap/PlayerState и Escape down/non-repeat
  transaction с показом либо закрытием подтверждения выхода.
- Все исходные HUD layout constants (minimap 320x320, weapons, place/life,
  lap, pick/achievement и car-life offsets) удалены из активной bgfx
  раскладки и читаются из source owner.
- Countdown снова event-driven: `tablo0..tablo3` сменяются только исходными
  race events, а `tablo4` выполняет точное 1.5-секундное fade/grow поведение
  `PlayerStateFrame::OnProgress`.
- SDL исполняет source Escape-команду через существующие pause/dialog/cursor
  adapters; bgfx только отрисовывает вычисленное source-состояние.

### Source MiniMapFrame ownership follow-up

- `MiniMapFrame::ComputeNode`, `AlignNode`, `AlignMidNodes`, `BuildPath` и
  `UpdateMap` перенесены из `OriginalRaceHud` в backend-neutral source owner.
- Сохранены точные исходные параметры 20°/2/10/2, node radius и alternating
  strip UV; source строит geometry для всех trace paths и стартовый маркер.
- Source owner хранит map bounds/scale/origin и переводит стабильный
  `CarState::GetMapPos` в HUD coordinates. Renderer больше не владеет этой
  логикой и только создаёт bgfx mesh/marker draw calls.
- Regression закрепляет topology, направление/размер start marker,
  world-to-map orientation и безопасный Clear/rebuild.

### Source PlayerStateFrame notification follow-up

- Pick/kill и achievement collections перенесены из bgfx HUD в active
  `source::PlayerStateFrame`; renderer хранит только GPU payload по stable id.
- Pick notifications повторяют insert-front, 5 s lifetime, 0.3 s fade,
  90/120 px/s slide и 85 px stack spacing.
- Achievement notifications снова используют восемь source RNG origins,
  initial lastIndex, current-position fly, 0.2–0.4 s size ping, points fade,
  0.15 s reindex и 4.7–5.0 s exit.
- Исправлены две surrogate-ошибки: отсутствующий initial lastIndex и линейный
  fly от сохранённого старта вместо Windows lerp от текущей позиции.

### Source PlayerStateFrame car-life follow-up

- Два `CarLife` slots, target/timer/timeMax/progress и visibility перенесены
  из renderer в active `source::PlayerStateFrame`.
- Восстановлены отдельные background/bar alpha, повторный hit flash,
  `StepLerp(dt/0.3)`, durations 1.5/4.0 и edge fade.
- Уничтоженный GameObject теперь немедленно скрывает/release overlay, как в
  Windows, вместо придуманного плавного fade после уничтожения.
- Projection остаётся camera adapter, но source выполняет точный clamp и
  half-size placement; opponent label suppression читает source target graph.

### Source PlayerStateFrame opponent follow-up

- Opponent collection, place sort и car-life move-to-front возвращены в
  `source::PlayerStateFrame`; CoreText cache привязан к стабильному racer id.
- Source теперь владеет dummy/point/label layout, text/car-life radius,
  edge fade и последовательным overlap alpha по исходному list order.
- Уничтоженная машина скрывает widget с сохранением состояния, а disconnect
  удаляет source item и освобождает backend label texture.

### Source PlayerStateFrame race-state follow-up

- `UpdateSlots/UpdateState/OnAdjustLayout` возвращены в source owner:
  Hyper, Mine и четыре primary boxes хранят visual/charge/selection/layout.
- Primary slots и два subweapon viewport снова компактно размещаются как в
  Windows; пустые physical slots больше не оставляют промежутков.
- Удалены ошибочные HUD icons `mineSlot.png`/`hyperSlot.png`, которые исходный
  `HudMenu.cpp` использует только в другом RaceMenu frame и в гонке не создаёт.
- Place/life и `MiniMapFrame` lap clamp также source-owned; CoreText и bgfx
  выполняют только локализацию, texture cache и draw submission.

### Source MiniMapFrame player follow-up

- `CreatePlayers`, `UpdatePlayers`, `DelPlayer` и disconnect lifetime
  перенесены в `source::MiniMapFrame` по стабильному racer id.
- Source collection использует `CarState::GetMapPos`, общий map transform и
  car color; renderer-owned `mapMarkers_` удалён.
- Marker roster больше не обрезается количеством Jolt vehicle snapshots;
  bgfx исполняет только исходный 20x20 playerPoint2 draw.

### Source PlayerStateFrame event follow-up

- `OnProcessEvent` возвращён в source owner для pick, achievement, damage,
  kill и countdown; renderer создаёт только image/text payload по result.
- Исправлена инверсия damage overlays: Windows `clOpponent=0`, `clHuman=1`,
  тогда как старый adapter назначал эти два slot наоборот.
- Bonus visual mapping и human/kill-credit filters теперь закреплены source
  regression, а `PickNotification` больше не повторяет gameplay enums.

### Single source FinishMenu follow-up

- Удалена вторая renderer-owned таблица результатов из `OriginalRaceHud`.
  Windows HUD её не имеет: финиш принадлежит отдельному `FinishMenu`.
- Устранён путь, на котором сначала показывалась придуманная таблица сразу при
  `RacePhase::Finished`, а затем правильная после `finishPresentationReady`.
- Frames/cups/results/timing теперь существуют только в active
  `FinishMenuFrameState` с source voice-duration и Last-event semantics.

### Source HumanPlayer input follow-up

- SDL race input больше не схлопывает weapon/reset события в независимые
  frame-флаги: в `source::HumanPlayer` передаётся ordered ControlManager
  `InputMessage` transaction.
- Восстановлены source gates и порядок `ShotAll/Reset/Mine/Shot/WeaponDown/
  WeaponUp/direct slots`, после которого отдельно polling-исполняются Hyper
  и analog Mine.
- Maslo удалено из digital mine edge и снова стреляет только через исходный
  continuous readiness path; empty-inventory WeaponUp сохраняет source `-1`.

### Source Weapon/Proj shot-preparation follow-up

- Полный descriptor batch теперь получает position/rotation/scale, общий
  target, playerId, sampled minimum lifetime и launch velocity в source
  `Weapon/Proj`, а не в `OriginalRaceSession`.
- Восстановлены исходные `speedRelative` и `speedRelativeMin` с проекцией
  скорости машины; Torpeda/Impulse получают правильный initial `_vec1` уже
  во время подготовки concrete projectile.
- Session передаёт только данные mounted Jolt actor и материализует backend
  body/ray после успешного `Weapon::CreateShot`; его дублирующий projectile
  transform helper удалён.
- Regression покрывает batch target/lifetime, поворот и масштаб mount, а также
  обе relative-speed формулы.

### Source attached Proj progress follow-up

- Hyper/Laser/Fire/Drobilka/FrostRay attached transforms перенесены из
  `OriginalRaceSession` в concrete `Proj::ProgressAttached`; session оставляет
  только Jolt transform/velocity input и collision/raycast execution.
- Fire снова копирует mounted actor velocity внутри source update, а
  Fire/Drobilka используют weapon world rotation без projectile local rotation.
- Удалён придуманный `Player::weaponSpinRadians`. Drobilka вращает concrete
  `Weapon`, и один source quaternion теперь читают renderer и contact path.
- Исправлен порядок вращения неединичного mount: `localRotation * deltaX`,
  как в оригинальном Windows `DrobilkaUpdate`, вместо обратного произведения.

### Source free-projectile progress follow-up

- `Proj::ProgressFree` теперь владеет единым source dispatch для Rocket height,
  Resonanse rotation, Thunder cooldown/reflection и graph transform sync.
- Session оставляет только интеграцию Jolt и backend track/border queries;
  прямые type-specific progress вызовы из него удалены.
- Исправлен homing без текущей цели: Torpeda/Impulse всегда уменьшают исходный
  0.4-секундный `_time1`, а не замораживают его до появления target.
- Regression проверяет no-target timer, накопленный clearance, reflection
  velocity/cooldown и итоговую concrete source pose.

### Source placed-mine progress follow-up

- Maslo/Mine/MineRip/MineProton arming и split dispatch объединены в
  `Proj::ProgressPlacedMine`; session больше не выбирает concrete handler.
- Source pose теперь синхронизируется после backend fragment integration, а
  не остаётся на предыдущей позиции до следующего кадра.
- Session сохраняет только track-plane/ballistic adapter, nested MineRip
  spawning, contact queries и network authority.
- Regression закрепляет arming/model scale и финальную placed-mine pose.

### Source moving-projectile contact follow-up

- `Proj::ContactDynamic` теперь владеет единым `OnContact` switch для
  Rocket/Torpeda/Mortira/Thunder/Resonanse, Sonar и Impulse; session больше
  не собирает damage/torque/Impulse state отдельными вызовами.
- Восстановлен порядок Windows `RocketContact`: projectile death и
  DeathEffect происходят до `DamageTarget`, а torque не теряется при
  уничтожении цели самим damage callback.
- `DestroyWithEffect` сохраняет исходный `DamageType`; Impulse снова выдаёт
  `dtEnergy`, тогда как прочие текущие projectile deaths остаются `dtSimple`.
- Jolt boundary ограничен contact geometry и применением готовых linear/
  angular velocity commands. Mine/bonus RPC authority переносится следующим
  contact-блоком.

### Source mine/bonus network-contact follow-up

- `Logic::MineContact` теперь владеет offline/RPC dispatch, а
  `Proj::ResolveMineContact` сохраняет Windows-порядок `Death -> Damage ->
  impulse`; session выполняет только overlap, network authority и Jolt writes.
- Базовый `GameObject::OnContact` вызывается на первичном контакте до RPC и не
  повторяется в `NetPlayer::OnMineContact1/2` replay.
- `Logic::TakeBonus` больше не применяет pickup до сетевого подтверждения:
  request оставляет Player/MapObj без изменений, replay применяет payload на
  всех peers. Transport-only event исключён из HUD и achievements.
- arm64 Debug build, 29/29 CTest, physics smoke и 360-frame bgfx/Metal race
  smoke прошли.

### Source Player restore follow-up

- Исправлен подтверждённый разрыв после смерти: Player больше не выдаёт
  respawn с полной life, пока `carPresent` и новый MapObj ещё отсутствуют.
- Возвращён Windows fixed-step порядок со строгим `_timeRestoreCar > 2.0f`:
  `CreateCar(false)`, новый car MapObj и `ResetCar` выполняются атомарно.
- `Player::OnProgress` читает собственный `HasCar()`; session больше не
  подменяет source `_car.mapObj` условием `!IsDestroyed()`.
- Удалён portable-only `ActivateCar` frame. Regression закрепляет новый ID,
  lights/listeners, один Jolt reset и отсутствие повторного respawn.

### Source bonus-projectile listener follow-up

- `Player::_bonusProjs` снова хранит исходную пару concrete `Proj*`/network ID,
  подписывает Player на mine projectile и очищается через `OnDestroy`.
- RPC lookup исключает death-state немедленно; session-side раннее удаление ID
  убрано, deferred `Logic` destruction завершает lifetime.
- В `Proj`, `RockCar` и `GameCar` восстановлен derived-stage `DestroyObject`,
  поэтому listener callback ещё видит `IsProj()`/`IsCar()`.
- Явный source-order `Player` destructor устранил crash завершения из callback
  уже частично разрушенного объекта.

### Source MapObj destruction-order follow-up

- Owned `MapObj` теперь уничтожает старый concrete `GameObject` до обнуления
  его `MapObj`, `Logic` и parent, как Windows `delete _gameObj`.
- Тот же порядок действует при `GameObjType` replacement и при замене
  временного record object на stable `Player::gameCar`.
- `OnDestroy` снова получает полный source context и derived `IsProj()`;
  отдельная non-owning ветвь внешней машины остаётся безопасно отсоединяемой.

### Source MineRip nested-spawn follow-up

- `Proj::BuildMineRipSplitPlan` теперь владеет независимыми model2/model3
  descriptors, lifetime sampling и batch из одного core/пяти fragments.
- Дискретный исходный Vec3Range grid и `dir * 10` impulse удалены из session;
  adapter получает готовые child definitions/velocities/visual variants.
- Regression закрепляет child types 11/13, visual/death metadata, 11 RNG
  выборок, lifetime range и fragment velocity; session integration сохраняет
  полный split/death-effect lifecycle.

### Source Mortira/Crater spawn follow-up

- `Proj::BuildDeathProjectileSpawnPlan` теперь владеет Mortira child lookup,
  `ptCrater` gate, descriptor/offset, lifetime RNG и owner ignore-pair.
- Session больше не выводит autonomous crater из weapon catalog; он только
  переводит готовый source plan в Jolt world/runtime объект.
- Invalid DeathEffect/type paths не потребляют RNG; unit и полный mortar
  integration закрепляют это вместе с continuous crater damage.

### Source GameObject event/Jolt bridge follow-up

- Возвращены source reference counters frame/progress/late/fixed и точный
  `SetLogic` unregister/re-register порядок; progress остаётся не подключён к
  World, как в закомментированной Windows ветви.
- `GameCar` снова регистрируется на fixed-step в constructor/destructor, а
  Jolt per-vehicle callback адресно dispatch-ит только этот объект без
  второго physics step или N×N обновления машин.
- Jolt awake/sleep управляет исходной парой late+frame registrations;
  network correction удерживает второй frame reference. Спящее тело отдаёт
  renderer последний source graph state.
- Regression, arm64 build, physics smoke и 360-frame bgfx/Metal race smoke с
  шестью машинами прошли.

### Source Race late-progress/Jolt ordering follow-up

- `RacePlaceModel` теперь зарегистрирован как настоящий World
  `LateProgressEvent`; source sorting и place events больше не вызываются
  напрямую из session до physics.
- Active runtime выполняет late pass после завершённого Jolt solver и
  синхронизации всех vehicle poses. HUD/place/minimap consumers видят текущий,
  а не предыдущий physics frame.
- Countdown, racing и finish-wait создают один pending pass; headless tests
  используют тот же event path синхронно, exit завершает final pass до
  object-graph teardown.
- Deferred/single-consume regression, physics smoke и 360-frame bgfx/Metal
  race smoke прошли.

### Source map bonus DeathEffect follow-up

- Map-owned pickups и hazards уже создавались как concrete `AutoProj`, но
  session вручную создавал их impact effect. Этот synthetic gate удалён.
- `Player::TakeBonus(GameObject&)` снова сам вызывает `bonus.Death()` до
  reward; type-6 `DeathEffectBehavior` решает one-live spawn и отдаёт plan
  backend adapters.
- Map mine завершает тот же actor через `DestroyWithEffect` с target и
  `DamageType::Mine`; target-child attachment больше не выводится из одного
  лишь наличия serialized visual.

### Source LogicBehaviors ownership follow-up

- Возвращены отдельные `LogicBehavior` и `LogicBehaviors`; global contact
  effect больше не является прямым полем `Logic`.
- `PairPxContactEffect` получает Logic/Map через owner и регистрирует source
  progress event через `LogicBehavior::RegProgressEvent`.
- Jolt manifold теперь входит через `Logic::OnContact` и container dispatch,
  повторяя Windows `PxSceneUser → LogicBehaviors::OnContact` boundary.

### Source LogicEventEffect identity follow-up

- `PairPxContactEffect` снова наследует `LogicEventEffect`; owner хранит
  record `ctEffects/spark2`, position и live/fading effect instances.
- Каждый contact generation получает stable handle. Release и окончательный
  particle destroy адресуют его напрямую вместо поиска по actor/surface/slot.
- Одновременные старый fading и новый active `spark2` больше не могут
  перехватить lifecycle друг друга в session adapter.

### Source DataBase global behavior/catalog follow-up

- `LogicBehaviors` теперь пуст до исходного `DataBase::Init` boundary;
  `DataBase::Configure` создаёт единственный `PairPxContactEffect`.
- `spark2` и пять concrete `light_impact` paths принадлежат behavior owner;
  session больше не конфигурирует их и не индексирует параллельный Race list.
- Active `ctEffects` projectile/object records и parsed `ctWeapon` catalog
  загружаются как typed `AutoProj`/`Weapon` factories с source descriptors.

### Source Weapon ShotEffect follow-up

- Type-10 `ShotEffect` теперь хранит serialized visual, sounds, pos, impulse
  и `ignoreRot` на concrete mounted `Weapon`.
- Каждый successful `PrepareProj` создаёт один ordered source spawn request;
  failed/dry paths его не создают.
- Session больше не читает параллельный `Race::weapons[].shotEffect`, а
  только переводит owner request в bgfx effect и SDL Source3d playback.

### Source vehicle EventEffect ownership follow-up

- `Player::SetCar` теперь передаёт concrete `LowLifePoints`, energy
  `DamageEffect` и `ImmortalEffect` точные записи текущего автомобиля.
- `smoke6` существует как один persistent car-child effect: owner создаёт
  его на первом low-life переходе и удаляет немедленно при лечении/смерти.
- Energy `damageEnergy*` materialize-ится только из owner spawn-plan и живёт
  исходные 0.5 секунды без повторного старта от каждого energy hit.
- Shield bgfx path берёт `shield1` и `scaleK` у `ImmortalEffect`; Race/session
  больше не являются параллельным владельцем этих параметров.

### Source Frost SlowEffect model ownership follow-up

- `SlowEffect::SetEffect(model3)` восстановлен как точная non-owning запись
  на динамическом car behavior, а не renderer lookup по двум индексам.
- Первый FrostRay contact создаёт один car-child effect через owner plan;
  последующие контакты не заменяют record и не продлевают lifetime.
- Session отвечает только за bgfx/SDL object boundary; удаление model и
  снятие ограничения скорости происходят из одного source lifetime.

### Source PxWheelSlipEffect ownership follow-up

- `db.xml` type-9 items сохраняются по два независимых owner-а на колесо
  (`trail`, затем `smoke7`) либо как единственный smoke там, где так записано.
- Trail local Z=0.01, record identity, sound catalog и Make/Free переходы
  принадлежат concrete `CarWheel` behaviors.
- SDL больше не создаёт SkidAsphalt для silent behaviors; fading smoke
  отсоединяется в последней contact position и не едет вслед за колесом.

### Source EventEffect/LifeEffect sound ownership follow-up

- Serialized `sounds` теперь принадлежат portable `EventEffect`, как
  `_sounds` в Windows, а не соседнему session DTO.
- `LifeEffectBehavior` выполняет one-shot выбор/Play transition; session
  передаёт SDL только уже выбранный path, world position и lifetime.
- Type-7 catalog проверен целиком (6 behaviors/6 sounds); ShotEffect и
  PxWheelSlipEffect переведены на тот же общий owner API.

### Source EventEffect destroy-listener follow-up

- One-live spawn-plan сохраняет exact `EventEffect*`, создавший backend
  object; уничтожение `RaceEffect` очищает именно этого owner-а.
- Все clear paths (natural lifetime, reset, finish, respawn, disconnect)
  проходят через единый callback до teardown Player/Logic.
- У Frost model3 callback также помечает concrete `SlowEffect` behavior на
  удаление, как Windows `SlowEffect::OnDestroyEffect`.

### Source EventEffect effect-object-list follow-up

- `EventEffect` хранит полный список уникальных effect handles и отдельную
  identity distinguished `_makeEffect`, как source `_effObjList`.
- Каждый visual `ShotEffect::OnShot` регистрирует отдельный child handle;
  natural expiry/reset/finish удаляет именно соответствующий handle.
- Renderer читает exact live ShotEffect visual из `RaceEffect`, а не
  повторно выбирает его из глобального weapon descriptor.
- Копия `Weapon` переносит конфигурацию и shot counter, но не live handles и
  не pending spawn requests с указателями на исходный behavior.

### Source DeathEffect identity/lifetime follow-up

- Type-6 `DeathEffect` возвращает exact owner/handle для car, projectile,
  mine и bonus death visuals; bgfx получает visual от того же owner path.
- `Logic` не уничтожает погибший transient `Proj`, пока его death-effect
  handle жив, поэтому backend callback не обращается к освобождённой памяти.
- Respawn заменяет car behavior graph и одновременно отсоединяет callbacks
  старых vehicle-death visuals; detached particle tails остаются безопасны.
