# Ревизия полноты порта Motor Rock на macOS

Дата ревизии: 2026-08-02

Ветка: `macos-arm64`

Базовый commit перед ревизией: `4226f87`

## Итог

Текущая программа является нативным arm64-приложением, читает оригинальные
ресурсы и содержит большой source-driven vertical slice гонки. Но она пока не
является полным переносом Windows-игры.

Главная архитектурная причина расхождения: на macOS не компилируются 34
оригинальных файла `Rock3dGame/source/game`, пять файлов `source/net`, три
файла `source/video` и исходный `source/snd/Audio.cpp`. Вместо них финальный
preset собирает вручную написанные:

- `OriginalMainMenu.cpp`;
- `OriginalProfile.cpp`;
- `OriginalGarage.cpp`;
- `OriginalRace.cpp`;
- `OriginalRaceSession.cpp`;
- `OriginalRaceRenderer.cpp`;
- `OriginalRaceHud.cpp`;
- `OriginalRaceCommentator.cpp`;
- `main_bgfx_original_menu.cpp`;
- `JoltVehiclePhysics.cpp`.

Эти файлы используют исходные XML, карты, модели, текстуры и многие константы
Windows-кода, но не являются автоматически эквивалентными исходным классам.
Название `Original*` означает происхождение данных, а не доказанную полноту
переноса.

## Обозначения

| Статус | Значение |
|---|---|
| Перенесено | Поведение имеет исходный источник, используется в финальном runtime и проверяется |
| Замена платформы | Windows API заменён нативным macOS API без намеренного изменения игровой семантики |
| Частично | Исходные данные/правила используются, но исходный класс или все ветви поведения не перенесены |
| Суррогат | Рабочее поведение написано заново, упрощено или основано на эвристике |
| Не перенесено | Подсистема выключена или её runtime отсутствует |
| Историческое | Код есть в дереве, но финальный M10 runtime его не компилирует |

## Фактический build graph

`src/Rock3dGame/CMakeLists.txt` на non-Windows собирает `source/stub/*`, а не
`source/game/*`. `src/Rock3dEngine/CMakeLists.txt` собирает bgfx/Jolt adapters,
но не D3D9/PhysX engine. Финальная конфигурация оставляет:

- `RRR3D_ENABLE_NETWORK=OFF`;
- `RRR3D_ENABLE_VIDEO=ON` в M9, M10 и Release (`OFF` в ранних presets);
- `RRR3D_ENABLE_STEAM=OFF`;
- `RRR3D_PORTABLE_GAME_STUBS=1`;
- `RRR3D_PORTABLE_ENGINE_STUBS=1`.

Последние два define являются историческими именами target-режима. Они не
означают, что весь активный код — заглушка, но подтверждают, что исходный
Windows target не компилируется.

## Матрица подсистем

| Подсистема | Источник Windows | Текущий macOS runtime | Статус | Что отсутствует или заменено |
|---|---|---|---|---|
| CMake, arm64 `.app`, bundle, signing | VS projects/resources | CMake presets, bundle scripts | Перенесено | Notarization/Developer ID не входят в текущую локальную сборку |
| Platform filesystem/logging | Win32/XPlatform | XPlatform + macOS directories | Замена платформы | Игровой логике это не должно давать различий |
| Resource filesystem | `ResourceManager`, Windows paths | `ResourceFileSystem`, exact-case catalog | Перенесено | Сам `ResourceManager.cpp` не компилируется; его игровые lifetime/cache semantics покрыты не полностью |
| `.r3d`, DDS/PNG, XML | Исходные loaders | portable decoders + TinyXML | Перенесено | Поддержаны используемые форматы; это не перенос всего legacy resource object graph |
| Окно и event loop | Win32/D3D window | SDL3/Cocoa | Замена платформы | Нативный путь работает |
| Keyboard/mouse/gamepad | XInput/Win32 `ControlManager` | SDL3 action adapter | Частично | Actions и hot-plug есть; Options показывает обе исходные колонки и все 18 user actions, но сам `ControlManager.cpp` и полный legacy input object graph не компилируются |
| Audio device/mixer | XAudio2/X3DAudio | SDL3/CoreAudio | Замена платформы | Backend полноценный, но весь исходный game-side `Audio.cpp` object graph не перенесён |
| MusicCat/menu music | `MusicCat`, `DialogMenu2::MusicDialog`, три menu Ogg и 11 game Ogg | background decode, shuffle, next, pause/state + source music popup | Перенесено | MusicDialog использует исходный `dlgFrame2`, metadata, layout и timing; поведение покрыто menu/race smoke |
| Menu SoundSheme | `Menu::SoundSheme`, один Effects source | девять source UI cues и один interrupting SDL voice | Перенесено | `ssButton1..5`, `ssStepper`, Accept/Info и Workshop drag используют исходные click/hover/pickup/repaint/planet/option/accept/warning события без наложения |
| Spatial race audio | X3DAudio game integration | source-derived `m3dFlat` voices поверх SDL | Частично | Перенесены fixed master 0.1, category/source/resource multiplication, 30/45 м, отсутствие pan/Doppler, motor/wheel/ShotEffect, pair-owned `PairPxContactEffect` и lifetime/target-child `LifeEffect` Source3d; общий legacy emitter/priority object graph ещё не компилируется |
| Commentator | `GameMode::Commentator`, serialized `game.xml/commentator/comments` | исходная таблица и state machine поверх SDL Voice bus | Перенесено | Загружаются доступные файлы выбранного языка; chance/delay/busy/repeatPlayer, weighted choice, prefix/suffix имён и все offline race events повторяют source semantics |
| Главное меню, внешний вид | `MainMenu2.cpp` | source-derived shared frame поверх bgfx | Частично | Фон, панели, selection, координаты GameMode/Tournament/Difficulty и отдельная позиция Back перенесены; полный widget tree и animation object graph не компилируются |
| Навигация меню | `Menu`, `MenuSystem`, `MainMenu2`, `GameMode` | source-matched shared/profile/gamer/final navigation поверх `MenuScreen` | Частично | GameMode/Tournament/Difficulty, Profile, Gamers, Network и FinalMenu имеют исходные item order, input branches, disabled skip и actions; общий legacy event/widget object graph ещё не завершён |
| Dialog/Profile UI | `DialogMenu2.cpp`, `MainMenu2.cpp`, `RaceMenu2.cpp` | source-derived `ProfileFrame`, `AcceptDialog`, `MusicDialog`, `WeaponDialog`, `UserChat` и offline `InfoDialog` | Частично | Четыре visible rows, scroll arrows, per-row close, вызываемые confirmations и workshop warnings перенесены. `UserChat` имеет source input, player color/name, newest-first 50-line history и 10+1 s fade; offline-профили по исходнику автоматически называются `profileN` |
| Race menu | `RaceMenu2.cpp` | source-derived `GamersFrame`/`RaceMainFrame`/`GarageFrame`/`CarFrame`/`WorkshopFrame`/`SpaceshipFrame`/`AngarFrame`/`AchievmentFrame` | Частично | Gamers, главный экран, Garage, Workshop, Angar и Achievment используют исходные panels/buttons/icons/portraits/slots/stats, `Misc/garage`, `Misc/space2`, `Misc/angar`, все 17 машин, семь gamer planets, шесть tournament planets, девять reward cards, camera/lamp/HDR transforms и shadow maps, `csSlots`/`csAutoObserver`, исходные View3d meshes и source data/transactions. Legacy widget/animation object graph ещё не воспроизведён |
| Options UI | `OptionsMenu.cpp` | source-derived modal bgfx view | Частично | Перенесены исходные четыре вкладки, координаты, PNG, 12/8/5/18 строк, scroll, steppers, volume bars, обе control-колонки и Apply/Cancel draft semantics. Первый запуск также использует отдельный `StartOptionsMenu`: sentinel `Select`, четыре source stepper, camera gate и Apply persistence. Legacy widget animation/event objects не компилируются; визуальная проверка на разблокированном Mac ещё нужна |
| Finish/final UI | `FinishMenu.cpp`, `FinalMenu.cpp`, `Menu::OnFinishClose` | source-derived FinishMenu, finish transition и FinalMenu | Перенесено | Активные экраны используют исходные assets/layout/timing/input; pass fail/complete, planet unlock и final movie branches сопоставлены с Windows source |
| Profile serialization | исходный profile/config code | `OriginalProfile.cpp`, user XML | Частично | Перенесены нужные поля и различие между default `pcIsometric` и отсутствующим `prefCamera`, которое запускает first-run UI; source-инвариант планет, `Race::MakeProfileName/NewProfile` и `DelProfile/SaveLib`: New Game создаёт `profileN`, `skirmish` временный, удаление последнего профиля сохраняет пустой library и не воскрешает XML reference. Полная схема ещё не доказана |
| Tournament/progression | `GameMode.cpp`, `Race.cpp`, menus | parser `tournamet.xml` + source-derived entry flow, advance и finish transitions | Частично | Continue/New/Load/Difficulty, отдельный SkProfile, `GamersFrame`, gamerId selection, pass/planet completion и final branch перенесены; legacy object/event graph ещё не компилируется |
| Race loading transition | `GameMode::StartRace/DoStartRace`, `Menu::msInfo`, `InfoMenu` | deferred bgfx loading state | Перенесено | Оригинальный `loadingFrame.dds` показывается не менее двух кадров до синхронной загрузки world/Jolt/render/audio state; modal input, aspect fit и последующий переход в HUD покрыты integrated race smoke |
| Garage/workshop/tournament data | `RaceMenu2`, `DataBase`, `garage.xml`, `workshop.xml`, `tournamet.xml`, `achievment.xml` | `OriginalGarage.cpp` + source-derived Gamers/Garage/Workshop/Angar/Achievment frames | Частично | Каталог, семь gamers и Viper achievement gate, source available/secret/locked order, buy/sell/install/swap/recharge/upgrade, reward purchase, campaign confirmations, charge-inclusive 50% resale, colors, stats фактической комплектации и bonus preview перенесены. 3D goods/slots/planets/boss cars читают исходные mesh/texture/vehicle transforms и повторяют `ViewPort3d` fitting/rotation. Legacy widget objects ещё отсутствуют |
| Map/catalog loading | `Map`, `MapObj`, `DataBase` | `OriginalRace.cpp` | Частично | 88 записей и исходные placements читаются; generic GameObject/behavior/include lifecycle воспроизведён только для известных типов |
| Track collision | PhysX triangle meshes | Jolt triangle meshes из исходных shapes | Перенесено | Используемый race path получает исходные triangles/material groups |
| Vehicle descriptions | `DataBase::CarDesc`, `RockCar` | XML/source constants → `VehicleDescription` | Частично | Mass, body, wheels, motor/gears/suspension перенесены; весь `RockCar`/PhysX state и contact callbacks не перенесены |
| Vehicle simulation | PhysX 2.8.4 `NxWheelShape` | Jolt custom vehicle adapter | Частично | Нативная замена работает; `GameCar::LockSpring` теперь подавляет airborne pitch как в source, но полная численная эквивалентность PhysX tire/suspension/solver не доказана |
| Car-to-track/car contacts | `GameCar::OnContactModify`, `GameCar::OnContact`, PhysX reports | Jolt contacts → source-derived session dispatch | Перенесено с backend-адаптацией | Сохранены surface groups, normal/friction force, до двух manifold points, border/car damage, energy owner и spring-border redirect. PhysX solver заменён Jolt, но `sumFrictionForce` теперь передаётся и как вектор, а не только как величина |
| Bonus/mine/crater contacts | `Proj::ComputeAABB`, `MineContact`, `MasloContact`, `MineRipUpdate` | source AABB/OBB, lock/contact state и nested-projectile runtime | Частично | Удалены сферы и hardcode осколков; source boxes, 0.25/0.4 lock rules, `ptMineProton`, impulse, oil clutch, nested lifetime/death effects перенесены. Динамика осколков остаётся адаптацией к Jolt, не численной копией PhysX |
| Mine placement | `Proj::MinePrepare` PhysX track raycast | source triangle raycast в `OriginalRaceSession` | Перенесено | Используются serialized `proj.pos`, ray `+2/-Z`, только `TrackPlane`, `max(-AABB.min.z, 0.01)`, hit normal; miss не расходует заряд |
| Countdown/checkpoints/laps/place | `Race.cpp`, `Trace.cpp`, `Player.cpp` | `OriginalRaceSession.cpp` | Частично | Основная гонка работает; это ручной state machine, все special race modes/edge cases не сопоставлены |
| Reset/respawn | `Player::OnProgress`, `ResetCar`, map `TouchDeath` | source tile-coordinate/multi-ray requests + death/restore lifecycle | Перенесено с backend-адаптацией | Death plane уничтожает любую машину, сохраняет 3-second touch attribution и ждёт source 2 seconds; `ResetCar` хранит `lastNodeCoordX`, проверяет source `0/-2/+2` rays, до пяти раз отступает на 6 м и переходит на предыдущий tile. PhysX closest-shape заменён тем же запросом к portable collision data |
| AI | `AICar.cpp`, `AIPlayer.cpp`, `Player::CheatUpdate` | source-derived path/control/attack states в session | Перенесено с backend-адаптацией | Перенесены four-track chain/lock masks, `ComputeTrackInd`, `edgeLine/edgeNorm`, turn braking, blocked recovery/reset, retained targets, line/Z shot gates, range/ammo/random/readiness, hyper, mines и difficulty rubber-banding. Неигровая debug visualization исключена, secret-path branch в Windows закомментирован |
| Weapons/projectiles | `Weapon.cpp`, `Player.cpp`, `Logic.cpp` | source type-switch runtime в `OriginalRaceSession` | Перенесено с backend-адаптацией | Сопоставлены все enum types 0–24 и все активные workshop/projectile records: source boxes/rays, forces, timing, groups, homing, attached/ray weapons, mines, nested projectiles, `ptHyper`, `ptSpring` и death effects; rigid-body solver остаётся Jolt |
| Weapon shot effects | `Weapon::CreateShot`, `ShotEffect`, serialized `ctWeapon` behaviors | `mapObj` → behavior type 10 → source effect graph | Перенесено | Effect record, local position, ignore-rotation и effective nested lifetime читаются из `db.xml`; отдельный `WeaponShotEffect` создаётся один раз для каждого созданного projectile |
| Weapon shot sounds | `Weapon::CreateShot`, `ShotEffect::GiveSource3d`, serialized sound refs | source `ctWeapon/behaviors/items/*[@type=10]/sounds` | Перенесено | Все 24 refs предзагружаются; случайный вариант выбирается на каждый успешно prepared projectile, `drobilka` остаётся без звука, а отдельный Source3d на машину/слот/вариант сохраняет ignore-while-playing и дальний delayed start/resume |
| Damage/support/shield | `GameObject::Damage`, `Logic::Damage`, `TouchDeath`, `DroidItem`, `ReflectorItem`, behaviors | source-typed central dispatch в session | Частично | Перенесены damage types, first-reflector rule, reflector-before-immortality, immortal incoming-damage event, 3-second touch attribution, Z=0 death plane, mine kill exclusion и фактический Droid heal 5; полный object listener graph ещё не закрыт |
| Bonuses | `Proj` types 4–10, `Player::TakeBonus` | source boxes, serialized values/DeathEffect и сопоставленные contact branches | Частично | Перенесены persistent speed/lusha/oil, одноразовый `Death()`, medpack/charge/money/immortal, Windows `Round((N-1)*Random())`, charge truncation и source pickup sounds; остаётся ручной portable dispatch вместо исходных объектов/PhysX callbacks |
| Destructible decorations | `DestrObj`, `GameBase`, `GameCar::OnContact` | life flags, source fragments/debris и collision meshes | Перенесено с backend-адаптацией | Все map destructibles имеют serialized `destrList` и source collider; `NX_AF_DISABLE_RESPONSE` представлен Jolt sensor с identity владельца, нулевой `dtTouch` разрушает `maxLife == 0`, parent actor удаляется, `OnProgress` даёт каждой части world pose родителя, static/dynamic shapes заменены Jolt bodies, распад одноразовый; в source нет impulse или lifetime для этих частей |
| Achievements | все 9 `AchievmentCondition*` classes, `AchievmentModel`, `AchievmentFrame`, `PlayerStateFrame` | definitions + source-matched counters + source reward frame | Частично | Сопоставлены Bonus/SpeedKill/RaceKill/LapPass/Dodge/LapBreak/Survival/FirstKill/TouchKill и exact record counts; campaign начисляет `Floor(reward × 1/1.2/1.5)`, skirmish не начисляет points и скрывает points HUD. Девять reward cards, state/price, purchase/points и навигация перенесены; generic legacy event/model object graph не компилируется |
| HUD | `PlayerStateFrame`, `MiniMapFrame`, `HudMenu` | `OriginalRaceHud.cpp` с исходными images/strings | Перенесено для offline race | Сопоставлены единственное активное состояние `msMain`, slots/life/place/lap, countdown, pick/kill/achievement, opponent/life overlays и finish. `enableHUD` скрывает только `_raceState` и lap, сохраняя map/event siblings как Windows |
| Mini-map | `MiniMapFrame`, `TraceGfx` | source trace-path geometry | Перенесено | Перенесены все pathes, Align/ComputeNode/smoothing, 320×320 align, start marker, 20×20 car markers/colors и `CarState::GetMapPos`-совместимая удерживаемая trace projection |
| Camera | `CameraManager.cpp`, `View.cpp`, `ActorManager::PullInRayTargetGroup` | source formulas и cull-opacity runtime в renderer | Перенесено для offline race | Release-переключение содержит только `csThirdPerson`/`csIsometric`; перенесены velocity pose, pull-back, ortho lead/teleport compensation, profile distance, projection и 0.25 s `gpCullOpacity` transitions. Debug/editor modes исключены |
| Scene graph/render queues | `GraphManager`, `Actor`, `SceneManager` | custom queues в `OriginalRaceRenderer` | Частично | Основные order buckets есть; generic actor/proxy/octree graph не перенесён |
| Materials | `MaterialLibrary`, `MappingShaders`, `DataBase` | source-derived material catalog + bgfx mappings | Перенесено с renderer-адаптацией | Проверены все 238 активных `Load*LibMat` records; два отсутствующих texture records являются source no-texture projectiles, ещё два — закомментированные `World2/track2` calls. Opaque/alpha/additive/bump/reflection/refraction и material flags сопоставлены без active name fallback; `refract.fx` использует исходные LINEAR/WRAP/MIRROR samplers |
| Lighting/shadows/HDR | D3D9 graph effects | bgfx/Metal passes | Частично | Реализованы directional race passes, source shadow maps Garage/Angar, HDR/bloom/tone map, Middle+ `goRefr` clean-scene/refraction и High-quality perspective SunShaft с prepare mask, фиксированными 640×512/320×256 targets, восемью ping-pong resample и исходным radial composite; bit-for-bit и полное graph state parity не доказаны |
| Particles/effects/trails | `FxManager`, effect records | portable emitter/trail renderer | Перенесено для active catalog | В фактическом `db.xml` покрыты все 8 manager classes, все 37 `ntParticleSystem`, единственный активный `FxFlowEmitter`, все 14 `partDesc` и 5 `flowDesc` fields, child systems, distributions, lifetime/fading и `ntIVBMesh`/`ntSprite`/`ntPlane`. Flattened include сохраняет собственные lighting/order/lifetime каждого child Actor; D3D sorting заменён bgfx |
| Weather/water/magma/sky | `Environment.cpp`, `GraphManager`, `WaterPlane`, `FogPlane`, `GrassField`, source `.fx` | source graph + bgfx/Metal shader equivalents | Перенесено с backend-адаптацией | Перенесены шесть world branches, weather fog/ambient/sky/far, quality gates, rain/isometric exclusions, scene AABB +300, UV scale 4/25/50, Low/volume paths, water reflection, depth reconstruction, cloud animation/color/intensity, source grass atlas/density/scale и sky без camera translation; D3D9 заменён Metal |
| Intro/video | `VideoPlayer.cpp`, DirectShow playback | AVPlayer/AVPlayerLayer, 14 lossless-remuxed MP4 | Замена платформы | Все исходные H.264/MP3 потоки проигрываются нативно; Difficulty `Main`, Gamers `Intaria`, planet и final transitions подключены |
| LAN/network | `NetGame`, `NetRace`, `NetPlayer`, NetLib | native Boost.Asio transport + `OriginalNetworkSession` в game runtime | Частично | Исходный `NetLib`, TCP/UDP, reconnect, adapters, class ID 1/2, match/player/vehicle state, generated gamer/color и menu filtering, identity authority и failed rollback, gameplay authority, finish/results, `PushLine`, восемь host option RPC, `ChangePlanet` и `RaceMainFrame` ready/start/kick/leaver UI подключены и проверены loopback. Steam backend и полный legacy model/event object graph ещё не перенесены |
| Steam | `SteamService`, auth | выключено | Не перенесено | Не относится к offline race, но не должно называться перенесённым |
| Editor | `source/edit`, MapEditor | не входит в `.app` | Не переносился | Редактор не является обязательной частью пользовательской игры |

## Активные суррогаты и заглушки

### 1. Меню

`main_bgfx_original_menu.cpp` всё ещё вручную объявляет `MenuScreen`.
`GameModeFrame`, `TournamentFrame`, `DifficultyFrame` и активный
`OptionsMenu` больше не являются произвольными generic-списками: их
компоновка, доступность и переходы сопоставлены с исходником. Главные
оставшиеся generic-блоки — Network и часть переходов.
Активные `FinishMenu` и `FinalMenu` уже используют исходные игровые ресурсы,
layout, timing и ControlEvent semantics. Legacy `MenuSystem`
animation/event object graph также не компилируется.

### 2. Игровая логика

`OriginalRaceSession.cpp` объединяет обязанности исходных `Race`, `Player`,
`AIPlayer`, `AICar`, `Weapon`, `Logic`, `GameObject` и achievements в один
ручной state machine. Он уже существенно source-driven, но любое реализованное
им поведение должно считаться частичным до сопоставления каждой ветви с
Windows-кодом.

Type-specific projectile dispatch, AI control/attack branches и все девять
achievement conditions уже сопоставлены с активным Windows call graph.
Оставшаяся граница здесь архитектурная: legacy component/listener objects
скомпилированы в typed portable state machine, а PhysX rigid bodies/callback
order заменены Jolt. Это не следует повторно описывать как отсутствующие
игровые ветви без конкретного source counterexample.

### 3. Render/audio mapping

Эвристики `weaponEffectTexture` и `weaponSoundPath` удалены. Workshop
`mapObj` теперь ведёт к исходной записи `ctWeapon`; из behavior type `10`
переносятся effect record, local position, ignore-rotation, nested lifetime и
sound refs. Если source behavior или visual отсутствует (`drobilka`, support
`droid`/`reflector`), порт больше не создаёт fallback-вспышку, луч, сферу или
звук.

Отдельная ревизия material catalog сопоставила все активные литералы
`ResourceManager`/`DataBase`; неразрешённых active fallback/direct mappings
в normal race path не осталось.

Общие звуки подбора `pickup_up`/`acception`, ранее подставлявшиеся вместо
игрового эффекта бонуса, удалены из race path. `Proj` model record теперь
ведёт к его сериализованному `DeathEffect`, а behavior type `7` этого эффекта
задаёт фактический звук (`klicka5` либо `shieldOn`). Как и в
`Player::TakeBonus`, ammo выбирает только неполный слот через
`Round((N-1)*Random())`; количество заряда усекается к `int`, а не округляется
вверх.

### 4. Отключённые системы

`PortableGame.cpp::CreateWorld` бросает исключение и сообщает, что legacy
`IWorld` недоступен. Финальный entry point обходит этот API и запускает свой
runtime, поэтому гонка работает, но API-заглушка остаётся фактическим
свидетельством неперенесённого исходного World object graph.

Network, video и Steam явно выключены.

### 5. Неактивный исторический код

Следующие файлы не входят в финальный M10 путь и не определяют результат:

- `PortableMenu.cpp`;
- `PortableRace.cpp`;
- `MinimalVehiclePhysics.cpp`;
- `PortableRaceRenderer.cpp`;
- ранние `main_sdl.cpp`, `main_bgfx.cpp`, `main_bgfx_menu.cpp`.

Их наличие не является дефектом runtime, но milestone-документация не должна
смешивать их с активным портом.

## Исправление, начатое этой ревизией

Первым удалён активный игровой суррогат коллизий map bonuses/mines:

1. `proj/modelSize`, `proj/size` и `proj/offset` теперь читаются из исходных
   records.
2. Локальный AABB модели строится из оригинальных `.r3d` bounds с
   трансформациями `grActor/nodes`.
3. Повторена семантика Windows `Proj::ComputeAABB(false)`:
   serialized size/offset объединяется с model AABB.
4. Повторена семантика `Proj::CreatePxBox`: сохраняются local center и half
   extents.
5. Контакт vehicle/bonus выполняется oriented-box SAT, а не сферой `3.5`.
6. Smoke проверяет separating-axis случай, который старая сфера ошибочно
   считала контактом.

Следующим collision-блоком также удалён `MineRuntime::triggerRadius`:

1. Primary weapon mine получает `ProjectileDefinition::collision`.
2. `mineRipKern` и `mineRipPiece` читают собственные вложенные
   `proj/size`, `offset`, `modelSize` и model AABB из `model2/model3`.
3. Mortar death-projectile `ptCrater` получает исходный box `6×6×0.1`,
   а не круг радиусом `3`.
4. Mine/car contact использует тот же OBB SAT.
5. Осколки сохраняют rotation родительской мины, как `MineRipUpdate`, вместо
   придуманного поворота модели по velocity.

Общий source-box pipeline теперь используется и летящими projectile types;
открытыми остаются их type-specific callbacks/contact groups.

Следующим collision-блоком перенесён `Proj::MinePrepare`:

1. Исходные triangle meshes сохраняются в `Race` и совместно используются
   Jolt backend и gameplay raycasts.
2. Ray начинается в преобразованном serialized `proj.pos + Z*2` и идёт по
   `-Z`, как в Windows.
3. Фильтр принимает только source `TrackPlane`, не borders/decorations.
4. Mine position получает `worldImpact + Z*max(-AABB.min.z, 0.01)`, rotation
   выравнивает local up по `worldNormal`.
5. При отсутствии hit `PrepareProj` считается неуспешным: mine не создаётся,
   заряд и cooldown не меняются.
6. Physics smoke проверяет hit transform/normal и отдельный miss за пределами
   карты.

Следующим collision-блоком удалены segment/radius суррогаты:

1. Перенесено поле `Proj::Desc::sizeAddPx`; `tankLaser` сохраняет исходное
   смещение луча `0 0 -0.3`.
2. `LaserUpdate`-ветви используют closest-shape ray по source OBB кузовов и
   исходным triangle meshes трассы/декораций, поэтому луч поражает только
   ближайшую машину и останавливается на геометрии.
3. Летящие и attached contact-projectiles используют свой serialized
   `ComputeAABB(false)` OBB, а не радиус кузова/снаряда.
4. Collision meshes сохраняют владельца `ctDecoration`; разрушение теперь
   вызывается реальным OBB–triangle либо OBB–OBB контактом.
5. Smoke ставит автомобиль на исходный triangle `crush1`, а не в
   придуманный proximity-radius около origin объекта.

Следующим type-specific блоком исправлены `ptHyper` и `ptSpring`:

1. `ptHyper` прикладывает `NX_SMOOTH_VELOCITY_CHANGE` вдоль local X машины,
   использует serialized speed `10` и source `shotDelay=1.5`.
2. `accelEff` теперь живёт как linked projectile в исходном weapon/projectile
   transform и следует за машиной весь `minTimeLife=2`; временный
   `HyperActivated` render effect с придуманным vertical transform удалён.
3. `ptSpring` допускается только при контакте всех колёс, прикладывает local Z
   impulse `17` и при неуспешном Prepare не расходует charge.
4. `GameCar::LockSpring` (`1.5` секунды) проведён до Jolt input и отключает
   автоматический airborne pitch torque на время блокировки.
5. Smoke отдельно проверяет linked hyper visual, cooldown, grounded/airborne
   spring branches и подавление pitch в physics backend.

Следующим type-specific блоком исправлен `ptDrobilka`:

1. `Proj::DrobilkaUpdate` вращает сам установленный weapon actor вокруг local
   X на serialized `angleSpeed`; один per-slot угол теперь используется и
   renderer, и contact transform.
2. `spark2` больше не рисуется постоянно как придуманный projectile visual:
   исходный `DrobilkaContact` создаёт его только при реальном OBB contact.
3. Контактная модель перемещается в вычисленную точку соприкосновения, один
   source instance переиспользуется при следующих contacts и после последнего
   contact живёт ровно `_time1 = 0.5`.
4. Непрерывный damage остаётся `desc.damage * contact.deltaTime`; тот же
   contact path применяется к vehicle и destructible decoration actors.
5. Physics smoke проверяет serialized type/angle/lifetime, вращение mount,
   continuous damage, создание primary contact model и её исчезновение.

Следующим projectile-base блоком исправлены `RocketPrepare`,
`RocketUpdate`, `ptThunder`, `ptResonanse` и legacy `ptSonar`:

1. Moving projectile lifetime снова равен
   `max(maxDist / desc.speed, minTimeLife)`. Прежний clamp по фактически
   пройденному `maxDist` удалён: relative-speed projectile может пролететь
   дальше, но живёт исходное время.
2. `RocketUpdate` делает исходный vertical ray из `pos + Z*4` только в
   `cdgTrackPlane`, сохраняет `_vec1.z` clearance и опускает его при
   приближении рельефа.
3. Rocket contact point берётся из пересечения source OBB; local
   `NX_VELOCITY_CHANGE` torque больше не вычисляется от суррогатного
   projectile position/world origin.
4. Serialized предмет `sonar` этой версии фактически содержит `ptThunder`
   (`type 22`, `Misc\thunder`). Его отражение теперь использует реальные
   `materialGroup 1` / `cdgShotTransparency` triangle contacts, patch normal
   и source cooldown `0.1`, а не ширину trace path.
5. `ptResonanse` вращает runtime actor через `rot * angleAxis(local X)`;
   renderer и contact OBB используют эту же rotation, без отдельной
   возрастной render-анимации.
6. Неиспользуемая текущим workshop, но существующая в Windows enum
   `ptSonar` сохраняет живой projectile, наносит
   `damage * contact.deltaTime` и прикладывает off-centre
   `mass * linearVelocity` impulse вместо разового full damage.
7. `ptImpulse` получает initial target через исходный
   `Player::FindClosestEnemy(pi/5.5)` (`sphereGun` использует viewAngle 0).
   После контакта поиск начинается от поражённого Player с `pi/2` и
   минимизирует абсолютную дистанцию до его forward-plane; прежний
   Euclidean/projectile-direction heuristic удалён. Сохранены три удара,
   damage `D`, `D/2`, `D/3`, прекращение chain при kill/no-target и
   игнорирование non-target contacts.
8. `TorpedaUpdate` для `sphereGun`, `torpedaWeapon` и `ptImpulse` теперь
   использует `QuatShortestArc(X, targetDir)` и quaternion `slerp`, а не
   invented normalized direction lerp. После поворота `_vec1` и actor
   velocity пересчитываются по исходным `speedRelative` /
   `max(dot(_vec1, dir), desc.speed)` branches.
9. Smoke проверяет serialized records
   `sonar`/`rezonator`/`rocketLauncher`/`phaseImpulse`,
   source border reflection, relative speed, time-based lifetime после
   прохождения `maxDist`, actor rotation, TrackPlane clearance и
   различимый `FindClosestEnemy(pi/2)` target handoff. Отдельный
   `sphereGun` regression проверяет viewAngle 0, 0.4-second homing delay,
   shortest-arc slerp и non-relative speed projection.

Следующим attached-projectile блоком исправлены `ptLaser`, `ptFrostRay` и
`ptFire`:

1. Attached actor живёт ровно serialized `minTimeLife`; прежнее
   искусственное `max(minTimeLife, shotDelay)` удалено. Это возвращает
   `tankLaser`/`asyncFrost` время 1.0 при `shotDelay=1.1`, не меняя
   `fireGun` 1.6.
2. Обычный `LaserUpdate(distort=true)` использует исходную кусочно-линейную
   fade-кривую толщины sprite от `timeLife/maxTimeLife`; beam length
   по-прежнему определяется ближайшим source ray hit.
3. `FrostRay` `model3` больше не рисуется как invented impact в конце луча.
   Как в Windows `SlowEffect`, `frost` создаётся child-эффектом поражённой
   машины, следует за её body transform и живёт serialized
   `frost.maxTimeLife=1`.
4. Повторный ray contact не сбрасывает время уже существующего
   `SlowEffect`; model ownership и ограничение скорости заканчиваются
   одновременно.
5. Smoke проверяет serialized lifetimes всех трёх типов, точное создание,
   owner/asset identity, отсутствие contact reset и удаление frost behavior.

Следующим mine/hazard-блоком восстановлены ветви `Proj::MinePrepare`,
`MineContact`, `MasloContact`, `MineRipUpdate`, `MinePieceContact` и
`MineProtonContact`:

1. Успешный `MinePrepare` ставит исходный `GameCar::LockMine(0.4)` и
   сбрасывает готовность оружия на serialized `shotDelay`; прежний общий
   cooldown `0.75` удалён.
2. `_time1/MineUpdate(0.25)` защищает только linked owner. Чужая машина может
   задеть обычную/разрывную мину сразу; `enableMineBug` отдельно проверяет
   `target->IsMineLocked()`. `ptMinePiece` и `ptMineProton` не используют эту
   bug-проверку, как в Windows.
3. Масло масштабируется во время arming, игнорирует любой car с mine lock,
   требует полную скорость `>3`, вызывает исходный `LockClutch(0.38)` и не
   исчезает после контакта.
4. Mine contact использует реальную точку OBB-контакта и прикладывает
   `addForceAtPos((0,0,desc.speed), NX_IMPULSE)`, включая линейную и
   off-centre угловую составляющую. Та же ветвь восстановлена для
   map `mineSpike`.
5. `MineRip` больше не создаёт радиальный synthetic fan с hardcode
   `10/4/4.25`. Вложенные `mineRipKern`/`mineRipPiece` читают из `db.xml`
   собственные type, damage, speed, collision, диапазон `minTimeLife=4..4.5`
   и model DeathEffect. Пять направлений воспроизводят исходный
   `Vec3Range(..., vdVolume)` и `dir*10` impulse.
6. Истечение жизни core/piece запускает их собственный `death3`; renderer
   держит отдельные nested death assets. Уничтожение машины типом `dtMine`
   остаётся death event, но больше не выдаёт ложный `cPlayerKill` credit.
7. Smoke отдельно проверяет ранний non-owner contact, owner/proton arming,
   вертикальный impulse, масло, шесть source MineRip объектов, случайный
   диапазон жизни и оба nested DeathEffect.

Следующим render/audio-блоком удалены эвристики оружия:

1. `workshop.xml/item/mapObj` связывает каталог с исходным `ctWeapon`.
2. Behavior type `10` (`ShotEffect`) переносит effect graph, position,
   impulse/ignore-rotation metadata, nested lifetime и sound refs.
3. `WeaponShotEffect` отделён от движения projectile и создаётся на каждый
   успешно созданный projectile, как `Weapon::CreateShot`.
4. Renderer загружает полные source object/effect assets; синтетические
   projectile-type textures, beam и hyper sphere удалены.
5. Audio использует serialized sound refs. Отсутствие source sound означает
   тишину, а не fallback.
6. Resource/physics smoke проверяют `bulletGun`, `sphereGun`, `turel`,
   `drobilka` и фактическое создание source ShotEffect.

Также удалён общий decoration fallback `explosion2.dds`. В исходном каталоге
каждая destructible map decoration содержит `destrList`; renderer использует
эти source fragments. Resource smoke теперь запрещает destructible definition
без serialized pieces, поэтому невозможный branch не маскируется придуманной
вспышкой.

Следующим menu-блоком заменён generic `Options`:

1. Вход из `MainMenu2` и `RaceMenu2` теперь сразу открывает единый модальный
   `OptionsMenu`, а не промежуточный придуманный список категорий.
2. Загружаются оригинальные `optionsBg`, `labelBg1/2`, `arrow3/arrowSel3`,
   `optBarBg/optBar`, `buttonBg4/buttonBgSel4`, `keyBg/keyBgSel` и
   `ctKeyboard/ctGamepad`; позиции взяты из `AdjustLayout`,
   `GameFrame::OnAdjustLayout`, `MediaFrame::OnAdjustLayout`,
   `NetworkTab::OnAdjustLayout` и `ControlsFrame::OnAdjustLayout`.
3. Восстановлены четыре source-вкладки Game/Graphic/Network/Controls,
   семь видимых строк Game и шесть Controls, отдельные steppers, три
   volume bars и обе control-колонки для всех 18 `cGameActionUserEnd`.
4. Возвращён отсутствовавший выбор реального SDL display mode. Порядок
   строк и даже необычное исходное соответствие `aaLevel`/`msLevel`
   сохранены буквально из `OptionsMenu.cpp`.
5. Изменения живут в draft, как в исходных Frame widgets. Громкости
   прослушиваются сразу, Back откатывает их и все bindings, Apply применяет
   fullscreen/resolution, race settings, camera, commentator, input maps и
   только затем атомарно сохраняет профиль.
6. arm64 build, resource verifier, physics smoke и 240-frame
   bgfx/Metal race-render smoke проходят. Финальная визуальная проверка
   Options отложена только потому, что macOS session была заблокирована.

Следующим audio-блоком удалён ручной суррогат диктора:

1. `OriginalRaceCommentator` теперь читает полную исходную таблицу
   `commentator/comments` из `game.xml`, а не содержит отдельный список Ogg.
2. Перенесены `chance`, per-comment/global `delay`, `baSkip`/`baQueue`/
   `baReplace`, `repeatPlayer`, weighted random и фильтр `forHuman` из
   `GameMode::Commentator::Generate`.
3. Имена гонщиков также являются serialized comments. Поэтому source
   `sPlayer`/`ePlayer` работает для всего каталога персонажей, а не для
   четырёх вручную выбранных ключей.
4. Подключены пропущенные second/third/last finish, third changed/far,
   domination и speed-arrow события. FinishMenu различает все четыре
   исходных place-comment, включая настоящий `playerFinishThird`.
5. `ResetState` больше не проигрывает раннюю придуманную реплику: старт
   приходит от `raceStartTime2`. Таймер Voice продолжает идти при открытом
   race pause dialog, как отдельная от Effects source-категория Windows.

Следующим menu-audio блоком перенесён `Menu::SoundSheme`:

1. Загружаются все девять исходных UI Ogg: click, navedenie, pickup down/up,
   repaint, showPlanet, changeOption, acception и warning.
2. Как `Menu::_audioSource`, portable runtime держит один Effects voice и
   останавливает предыдущую UI-реплику перед каждой новой — быстрые действия
   больше не накладывают несколько `click.ogg`.
3. Восстановлены `ssButton2::mouseEnter`, `ssButton3`, `ssButton4`,
   `ssButton5::focused/clickDown`, `ssStepper::selectItem`, а также звуки
   `ShowAccept`, `ShowMessage`, `WorkshopFrame::StartDrag/ResetDrag`.

Следующим подблоком начат перенос `RaceMenu2::RaceMainFrame`:

1. Вертикальный generic список больше не рисуется на главном race-menu
   экране. Семь действий расположены горизонтально по формуле
   `RaceMainFrame::OnAdjustLayout` с `menuItemSpaceX=50`.
2. Загружаются исходные `topPanel`, `bottomPanel`, `buttonBg1`,
   `buttonBgSel1`, семь `ico*`, `moneyBg`, `statFrame`, `imageFrame1`,
   `chargeBar1`, `statBar` и четыре weather icons.
3. Вызов `OnInvalidate` теперь повторён без придуманной подмены: первая рамка
   содержит photo выбранного через `gamerId` персонажа, вторая — photo босса
   текущей планеты, третья — вращающийся source boss car. Имена, которые
   прежний macOS-код ошибочно рисовал вместо двух photo, удалены.
4. Charge indicators берут исходный порядок Weapon1–4/Hyper/Mine и правило
   `ClampValue(charge/7, 0, 1)`. Рядом восстановлены шесть 50×50
   `ViewPort3d` установленного loadout и три реальные Damage/Armor/Speed
   полосы со значениями `current/maximum` и шкалой скорости `300`.
5. Mouse hit boxes и Left/Right navigation соответствуют горизонтальному
   меню. Интеграционный smoke отдельно требует frame, portraits, boss car,
   loadout, stat bars и активный 3D `CarFrame` в состоянии `msMain`.

Следующим отдельным коммитом перенесена двумерная часть
`RaceMenu2::GarageFrame`:

1. Generic вертикальная страница заменена исходными `topPanel2`,
   `bottomPanel2`, двумя `rightPanel2`, `moneyBg`, `statFrame2`,
   `statBar2`, `buttonBg2`, локализованной `buyButton_*`, `arrow1`,
   `colorBox*`, `carBox*`, `lock` и всеми 17 исходными `GUI/Cars/*.png`.
2. Воспроизведён `UpdateCarList`: доступные, открытые secret и закрытые
   машины образуют три последовательные группы; secret cars скрываются в
   campaign и доступны в skirmish только после achievement gate.
3. Окно из восьми карточек следует логике `AdjustCarList`, стрелки не
   зацикливаются, закрытая карточка показывает `lock.png`, а выбор машины,
   покупка с подтверждением, недостаток денег и выход используют исходную
   модель профиля/гаража.
4. Перенесены обе палитры по семь цветов буквально из `Player.cpp`.
   Выбранный цвет записывается в исходное поле профиля.
5. Из `workshop.xml` теперь читаются `carFuncMap` и projectile damage.
   `OriginalGarage.cpp` воспроизводит формулы `GetArmorSkill`,
   `GetDamageSkill`, `GetSpeedSkill`, включая максимумы по всему каталогу,
   default slots выбранной машины и source scaling скорости к `300`.
6. Build, resource verifier, physics smoke (включая новый stat audit),
   отдельный input smoke и 240-frame race-render smoke проходят. На момент
   этого коммита фон GarageFrame ещё оставался menu scene; следующий блок
   заменил его исходным 3D `CarFrame`.

Следующим отдельным блоком перенесён `RaceMenu2::CarFrame`:

1. `loadOriginalGarageScene` читает из `db.xml` исходные
   `ctDecoration/Misc/garage` и `Misc/question`, все 17 уже разобранных
   `ctCar` и source weapon records; отдельной придуманной garage-модели нет.
2. Статическая постановка кузова и колёс повторяет `CarFrame::SetCar`:
   половина suspension travel, serialized wheel offsets/radius, последнее
   колесо для body Z и `invertWheel` rotation. Preview meshes upgrades не
   подменяют штатные колёса машины.
3. Открытая машина в Garage использует цвет профиля и default Weapon1–4 из
   `garage.xml`; в `RaceMainFrame` тот же `CarFrame` использует фактически
   установленные profile slots, как `SetSlots(player, false)`. Закрытая
   скрывает машину и показывает вращающийся `question.r3d` в source position
   со скоростью `0.1` оборота/с.
4. Камера, near/far/FOV, обе позиции/quaternion spot-lamps, ambient/fog/sky
   flags и четыре HDR-параметра перенесены буквально из
   `RaceMenu2.cpp`/`Environment`; `csAutoObserver` вращает камеру вокруг
   target со скоростью `pi/48`, а Workshop сохраняет отдельный `csSlots`.
5. В legacy mesh shader добавлены source D3D spot cone/range/attenuation,
   diffuse и specular. `GUI/question` загружается как
   `LoadSpecLibMat(question.png)` со specular `1`/power `64`; общий renderer
   теперь декодирует PNG-материалы, а не передаёт их как DDS container.
6. Build, resource verifier, physics smoke и bgfx/Metal menu/race smoke
   проходят; telemetry требует реальный lighting draw и оба source shadow
   pass 3D garage scene как в Garage, так и в `RaceMainFrame`.
7. `Environment::EnableLamp` перенесён отдельными 2048² single-split shadow
   maps: две лампы Garage используют near/far `1/20`, Angar поддерживает до
   трёх карт с `1/80`, `1/80`, `1/100`, включая мигающую красную лампу.
   `lighting.fx` semantics сохранены: карта каждой лампы умножает только её
   diffuse/specular, не global ambient и не вклад остальных источников.

### WorkshopFrame

1. Удалены придуманные раздельные страницы «slots/items». Один экран повторяет
   `RaceMenu2::WorkshopFrame`: `topPanel3`, `bottomPanel3`, `leftPanel3`,
   3×4 goods grid, десять слотов, money/stat panels, slot icons, level и
   charge controls.
2. Ассортимент восстанавливается из tournament pass rewards, сортируется по
   исходной цене и показывает 12 элементов с тем же построчным scroll.
   Mobility families 1–4, как в Windows, доступны через level-button, а не
   добавляются в goods как выдуманный inventory.
3. `OriginalWorkshopRenderer` загружает `<mesh item>`/`<texture item>` каждой
   записи `workshop.xml`, применяет `Menu::GetIsoRot`,
   `Context::DrawView3d` AABB fitting, Y inversion, depth clear и исходную
   скорость вращения `pi/2`.
4. Перенесены drag/drop, проверка совместимых `garage.xml` placements,
   swap установленной детали, возврат купленной детали, campaign buy/sell
   confirmations, продажа установленной детали за 50% с учётом оставшегося
   charge, recharge step/cost и последовательные mobility upgrades.
5. Полосы используют фактически установленные profile slots. Hover/drag
   временно подставляет деталь в подходящий слот с минимальным текущим
   weapon damage и показывает исходный bonus-stat preview.
6. `CarFrame::csSlots` выбирает ближайшую из восьми исходных camera poses.
   Интеграционный Metal smoke обязан посетить Workshop и увидеть как его
   2D frame, так и освещённую 3D garage scene до перехода в Garage/Race.
7. Восстановлен исходный lifecycle `Profile::Reset → SnProfile::EnterGame`:
   новая кампания создаётся через полный portable default profile, где первая
   планета уже `psOpen/pass 1`. Ранние сборки записывали невозможное
   `psUnavailable/pass>0`; loader переводит только эту комбинацию в `psOpen`,
   сохраняя заработанный pass и возвращая соответствующий source assortment.
8. Удалена придуманная постоянная selection-панель товара. Перенесены
   `WorkshopFrame::ShowInfo/UpdateSlotInfo` и `DialogMenu2::WeaponDialog`:
   окно появляется только от mouse hover над товаром, slot plane, level или
   charge button; клавиатурный focus его не открывает. Goods используют
   исходный `cellSize=100`, slot plane — 125×100, а charge button передаёт
   свой размер 36×31, умноженный на четыре.

### WeaponDialog

1. Используется исходный `Data/GUI/dlgFrame3.png` размером 325×138. Перенесены
   `VerySmall` Verdana 18 bold серого `175/255` для word-wrapped info и
   `Small` Verdana 24 white для name/money/damage.
2. Сохранены label offsets: info `(3,-3)` в области 280×75, money `(-60,54)`,
   damage `(80,54)`, name `(0,-58)`.
3. `waLeftBottom` вычисляется буквально: к sender center добавляется
   `(slotWidth/4,-slotHeight/4)`, затем `(frameWidth/2,-frameHeight/2)`.
   `SetPos` ограничивает центр рамки половиной размера плюс исходный margin
   15 px.
4. Goods и slot plane показывают исходную цену предмета; charge button —
   `chargeCost × chargeStep`; level button — следующий mobility upgrade.
   Damage равен сумме serialized projectile damage с форматом `%0.0f`;
   для не-оружейных деталей выводится `"-"`, для support weapon с нулевым
   damage — `"0"`.
5. Интеграционный M9 smoke задерживает keyboard-сценарий в Workshop, посылает
   реальный mouse-motion на доступный source good и требует фактическую
   отрисовку диалога до продолжения пути в Garage/Angar/Race. M8/M9/M10
   Debug, audio/race-render и resource verifier прошли.

### InfoDialog

1. Удалены три раздельных warning-флага и подмена рамкой `AcceptDialog`.
   Перенесён вызываемый offline-путь `DialogMenu2::InfoDialog` с исходными
   `dlgFrame4.png` 307×306 и `dlgButton2/dlgButtonSel2.png` 132×36.
2. Title использует Verdana 44 gray `175/255` в `(-27,-105)`, сообщение —
   left-aligned Verdana 24 white в word-wrap области 245×135 с центром
   `(0,5)`, OK — Verdana 32 white в `(0,105)`.
3. `MenuFrame::SetPos` повторён с margin 15. Workshop warnings используют
   `waLeftBottom` и source quarter-size offset вызывающего good/level/charge
   control; Angar `svHintCantFlyPlanet` привязан к door slot через `waBottom`;
   Achievement `svHintCantPoints` остаётся центрированным.
4. Подключены фактические ветки `svHintWeaponNotSupport`, `svHintCantMoney`,
   `svHintCantFlyPlanet` и `svHintCantPoints`. Диалог модален: mouse не
   проходит в нижний frame, а из source navigation закрыть его может только
   OK/`gaAction`, не Escape/Pause.
5. M9 smoke требует отрисовать исходные frame/title/wrapped message/selected
   OK и закрыть их реальным input dispatch до продолжения Workshop path.
   M8/M9/M10 Debug, последовательные audio/race-render и resource verifier
   прошли.

### AcceptDialog

1. Шесть раздельных immediate-mode вариантов удалены. Все вызываемые offline
   подтверждения теперь проходят через один перенос
   `DialogMenu2::AcceptDialog` с исходными `dlgFrame1.png` 384×156 и
   `dlgButton1/dlgButtonSel1.png` 90×38.
2. Сообщение использует centered word-wrap Verdana 32 gray `175/255` в
   области 325×65 с локальной позицией `(0,-25)`. Yes/No используют тот же
   шрифт и цвет в `(-70,32)`/`(70,32)`; при открытии исходный focus всегда
   установлен на Yes.
3. Перенесены оба специальных режима конструктора: `maxMode` масштабирует
   frame/info в 1.7 раза, ширину кнопок в 1.5 раза, использует Small 24 и
   offsets `±100/72`; `maxButtonsSize` дополнительно расширяет кнопки и
   сводит их центры к `-10/+10`. `disableFocus` оставляет modal background
   владельцем navigation, как требуется диалогу `OptionsMenu::Press key`.
4. Сохранены исходные placement paths и clamp 15 px: HUD exit, Profile
   delete, Garage buy и Achievement buy центрированы; Workshop Buy/Sell
   используют quarter-size sender offset и `waLeftBottom`; Angar Stay/Fly —
   half-height offset и `waBottom`.
5. Callback-ветки выполняют фактические source actions: выход из гонки,
   удаление профиля, покупку машины/товара/reward, смену планеты и
   Delete/Cancel при переназначении controls. Ошибка покупки машины теперь
   открывает отдельный исходный `InfoDialog`, а не меняет текст
   confirmation. M9 smoke проверяет точные 384×156, 325×65, 90×38 и offsets
   в Profile/HUD paths; M8/M9/M10 и bundle verifier прошли.

### SpaceshipFrame / AngarFrame

1. Удалена generic-страница `Planets`. `loadOriginalAngarScene` создаёт
   `RaceMenu2::SpaceshipFrame` только из исходных
   `ctDecoration/Misc/space2` и `Misc/angar`; положение/масштаб/поворот
   космической plane повторяют изменения node после `AddMapObj`.
2. `Environment::ewAngar/wtAngar` перенесён с тремя точными lamp
   position/quaternion/range/color, красным циклом `1.5..3.0` секунды,
   ambient/fog flags и HDR `3/3.5/20/5`. Камера начинает с source pose,
   target radius `50` и после трёх секунд повторяет
   `csAutoObserver` со скоростью `pi/96`.
3. Из `tournamet.xml` читаются mesh/texture, request points и первый boss
   каждой из шести campaign planets. На экран возвращены
   `bottomPanel6`, `planetInfo`, `doorSlot*`, `doorUp/doorDown`,
   `buttonBg6*`, вращающиеся `planet.r3d`, фото и составная машина босса.
4. Общий menu ViewPort3d renderer теперь поддерживает multi-node car:
   body meshes остаются в origin, а четыре wheel meshes получают координаты
   из тех же serialized wheel positions, которые Windows
   `CarWheels::LoadPosTo` читает из `*Wheel.txt`; отрицательная Y-сторона
   зеркалируется. Это исключает overlap/гигантские колёса в boss preview.
5. Состояния slot text повторяют `psOpen/psClosed/psUnavailable/psCompleted`,
   `svRequestPoints`, champion-next и `svUnavailableTitulA`. Перенесены
   выбор/закрытие planet info, 0.25-секундные doors, Stay/Fly accept,
   current/next restrictions, skirmish open-planet branch, профильный
   `Unlock → Open → ChangePlanet` и mouse/keyboard navigation.
6. Overlay очищает только depth перед вложенными ViewPort3d, сохраняя HDR
   color сцены. Интеграционный Metal smoke посещает Angar и требует отдельно
   3D scene draw и 2D frame; ручная проверка arm64 Debug подтвердила шесть
   планет, boss photo/car, панели и отсутствие wheel-scale артефактов.

### AchievmentFrame

1. Удалён generic вертикальный список внутренних achievement ID.
   Загружаются исходные `achievmentBg.dds`, `achievmentPanel.png`,
   `achievmentBottomPanel.png`, `closeBut`, `okBut/okButSel` и девять пар
   `GUI/Rewards/*Lock.png`/открытых изображений.
2. Box order, координаты и draw state взяты буквально из
   `AchievmentFrame::UpdateAchievments`: `armor4` использует
   `musicTrack.png`, locked и opened не показывают цену, unlocked показывает
   serialized price и доступен для покупки.
3. Перенесён точный граф `NavElement` для left/right/up/down. При locked
   target выполняется исходный recursive skip `Menu::NavElementFind`;
   mouse selection обходит перекрывающиеся cards в обратном порядке, как
   `UpdateSelection`.
4. Покупка теперь всегда проходит `svBuyReward` Yes/No, затем точный
   `asUnlocked → ConsumePoints → asOpened`; недостаток points показывает
   `svHintCantPoints`. Изменения сохраняются в portable-копию исходной схемы
   `achievment.xml` и сразу влияют на garage/workshop unlock checks.
5. 300-frame Metal smoke обязан открыть AchievmentFrame до старта гонки.
   Ручная arm64 Debug проверка подтвердила все девять карточек, цены, points,
   selected `okButSel`, purchase dialog и warning без визуальных артефактов.

### GameModeFrame / TournamentFrame / DifficultyFrame

1. `MainMenu::AdjustMenuItems` перенесён для shared frames: обычные строки
   начинаются на `centerY - 100` с шагом 53, а последний Back находится в
   отдельной исходной точке `centerY + 150`. Mouse hit-testing использует те
   же координаты.
2. `GameModeFrame::OnShow` блокирует Skirmish до первого tutorial stage.
   `TournamentFrame::OnShow` блокирует Continue и Load без профилей.
   Disabled text получает исходную alpha 0.25; cyclic keyboard navigation и
   mouse/confirm пропускают disabled items.
3. `Race::MakeProfileName/NewProfile` больше не заменены сбросом текущего
   профиля: New Game выбирает первый отсутствующий `profileN`, устанавливает
   source defaults и сохраняет отдельный XML. Continue сохраняет текущую
   difficulty, Load по-прежнему загружает выбранный исходный XML.
4. Skirmish использует отдельный runtime-профиль `skirmish`, открывает
   planet zero и `planetsCompleted`, не добавляется в campaign list и при
   любом save/shutdown сохраняет global config/achievements через snapshot
   последнего championship profile. Skirmish finish не продвигает tournament.
5. Profile-flow regression проверяет уникальное имя, reset defaults,
   временность SkProfile и open planets. 300-frame Metal smoke посещает оба
   shared frame до RaceMenu; ручная проверка arm64 Debug подтвердила
   координаты и отсутствие визуальных артефактов.

### GamersFrame

1. Удалён прямой суррогатный переход `Difficulty → RaceMenu`: как в
   `Menu::StartMatch`, новый championship и каждый Skirmish сначала открывают
   `RaceMenu::msGamers`; Continue/Load существующего профиля по-прежнему идут
   сразу в `msMain`.
2. Из раздела `<gamers>` исходного `tournamet.xml` читаются все семь записей в
   исходном порядке: mesh/texture планеты, gamer `id`, name/info/bonus и photo.
   Текущий выбор ищется по `Player::gamerId`, иначе выбирается первый доступный.
3. `AchievementModel::CheckGamerId` перенесён буквально для `classId=2`:
   Viper (`gamerId=9`) недоступен, пока achievement `viper` не перейдёт в
   `asOpened`. Prev/Next пропускают закрытого gamer без synthetic fallback.
4. Экран использует исходные `space1.dds`, `bottomPanel4.png`, `wndLight4.png`,
   `arrow1/arrowSel1`, `arrow2/arrowSel2`, исходные координаты 1280×720,
   aspect-limited portrait 190×190, word-wrap 475×160 и вращающийся
   `GUI/planet.r3d` через тот же `ViewPort3d` fitting.
5. Перенесён исходный трёхэлементный `NavElement` graph, mouse hit-testing и
   shoulder Prev/Next. Confirm записывает фактический gamer id и обновляет
   player/race data; Back отсутствует, как в Windows frame.
6. В campaign confirm проигрывает `intaria/intaria_eng`, после
   `cVideoStopped` открывает Garage; Skirmish открывает Garage сразу. Back из
   Garage возвращает RaceMenu, а не уже завершённый GamersFrame.
7. `--gamers-frame-smoke-test` проверяет source catalog/gate, 3D draw,
   навигацию Tyler → Snake, `gamerId=4` и переход в Garage без записи профиля.

### ProfileFrame / delete confirmation

1. Универсальный список заменён компоновкой `ProfileFrame::AdjustGrid`:
   grid центрирован в `centerY - 90`, видны ровно четыре строки, scroll
   сдвигается по одной строке. Up/Down находятся в исходных точках
   `centerY - 108` и `centerY + 120`.
2. Загружаются исходные `arrow1/arrowSel1` с rotation `±π/2`; normal arrow
   повторяет `StretchImage(... 30×30, fillRect)` и имеет размер 30×44.7,
   selected — 33.3×45.9. Disabled arrow получает исходную alpha 0.25.
3. Каждая строка использует shared `mainItemSel5` и close
   `buttonBg6/buttonBgSel6` с scale 1.8 в точке
   `cellWidth/2 - 40`. Перенесён двухколоночный NavElement graph
   item/close, связи с arrows/Back и skip invisible/disabled элементов.
4. Выбор профиля теперь повторяет `StartMatch`: сохраняет оставляемый
   профиль, загружает выбранный XML (при отсутствующем файле остаются
   source defaults), обновляет race/session и сразу открывает RaceMenu,
   а не возвращается суррогатно в Tournament.
5. Close показывает `svHintDeleteProfile` через исходные
   `dlgFrame1/dlgButton1/dlgButtonSel1` и Yes/No. `Race::DelProfile` удаляет
   reference из `race.xml`, не удаляя legacy profile XML; удаление последнего
   профиля оставляет пустые profiles/lastProfile и действительно блокирует
   Tournament Continue/Load.
6. Physics regression сохраняет, удаляет последний профиль и заново читает
   временный `race.xml`. 300-frame Metal smoke открывает Load, close dialog,
   отменяет его и продолжает гонку. Ручная arm64 Debug проверка подтвердила
   grid, close artwork и dialog без визуальных артефактов.

### FinishMenu

1. Удалена придуманная текстовая сводка и кнопки `Continue/Back`. Исходный
   `FinishMenu.cpp` не имеет selectable widgets: `gaAction`, `gaEscape` и
   любой left click вызывают `Menu::OnFinishClose`.
2. Отрисовываются не более трёх реально финишировавших гонщиков в порядке
   `Race::Results/place`. Используются оригинальные
   `playerLeftFrame.png`, растянутый `playerLineFrame.png`,
   `playerRightFrame.png`, `cup1..3.dds` и photo paths, прочитанные из
   `tournamet.xml`.
3. Перенесены исходные координаты для 1280×720 layout: три строки высотой
   240, photo `(128,116)`, cup `(right+160,115)`, name/reward Y 63 и
   money/points Y 154. Photo и cup сохраняют aspect и только уменьшаются до
   исходных bounds `198×193` и `190×160`.
4. Цвета подписей совпадают с source (`0xffe9a73f`, `0xffe1e1e1`,
   `0xff84bc43`). Picked money показывается как `money + pickMoney`, а points
   отдельной строкой, как `Race::Result`.
5. `OnProgress` повторён с delay 0.15 s, reveal 0.5 s и
   `voiceNameDur = 1.5 s`: чётные строки въезжают слева, нечётная справа.
   После закрытия восстанавливается соответствующий campaign/skirmish
   `RaceMenu2`, без прежней суррогатной ветки выхода в Main.
6. Отдельный 300-frame Metal fixture проверяет три source rows и не вызывает
   profile save/tournament advance. Ручная arm64 Debug проверка выявила и
   устранила склейку multiline CoreText label; итоговый кадр и
   Return → `RaceMenu2` проверены визуально.

### FinalMenu

1. Generic Credits page и единый многострочный CoreText bitmap удалены.
   `FinalMenu.cpp::OnInvalidate` перенесён как девять оригинальных
   `GUI/Slides/slide1..9.dds` и секции `svCredits`, разделённые сначала по
   `\n\n`, затем на красный caption и светлые body-lines.
2. Экран очищается исходным чёрным цветом. Слайды сохраняют aspect в bounds
   `vp - (500, 300)`, находятся в `(vp.x - 400) / 2, vp.y / 2` и получают
   source segment alpha на общей шкале 107 секунд. Титры движутся из
   `vp.y` к `-linesSizeY` при корне `vp.x - 250`.
3. Back использует `buttonBg2/buttonBgSel2`, Header font, цвет
   `214/255` и позицию `(width/2, vp.y - 60)`. Action/Escape/left click
   внутри кнопки возвращают непосредственно в `MainMenu2`; по истечении
   107 секунд выполняется тот же переход автоматически.
4. `TrackFinal.ogg` подключён отдельным неперсистентным `OriginalMenuMusic`:
   декодирование идёт в фоне, menu MusicCat ставится на паузу, final track
   начинается с нуля и при выходе menu MusicCat возобновляется.
5. Для material alpha общий static UI shader теперь умножает texture/vertex
   color на `u_materialColor`; белый default сохраняет прежнюю отрисовку.
6. `--final-menu-smoke-test` проверяет все девять слайдов, движение секций,
   Back, фоновый TrackFinal и автоматический возврат. M9/M10 Debug, resource,
   physics и 300-frame race-render regressions прошли. Ручная arm64 Debug
   проверка подтвердила первый и второй слайды, читаемые credits и
   Return → MainMenu2 без артефактов.

### Finish transition / VideoPlayer

1. Прежний безусловный возврат `FinishMenu → RaceMenu2` заменён точной
   развилкой `Menu::OnFinishClose`: незавершённый pass показывает
   `svHintYouNotCompletePass`, завершённый pass —
   `svHintYouCompletePass`, победа на planet открывает следующий Angar и
   `svHintYouCanFlyPlanet`, а последний tournament planet запускает final
   movie и `FinalMenu`.
2. `Race::CompletePlanet(4)` теперь, как Windows source, добавляет в
   `planetsCompleted` также все скрытые planets с индексом 5 и выше.
   Отдельная deterministic regression проверяет все пять вариантов перехода,
   включая Skirmish, где tournament advance не выполняется.
3. DirectShow заменён нативным `AVPlayer`/`AVPlayerLayer` поверх Cocoa content
   view. Во время ролика menu MusicCat приостанавливается, game/menu time
   заморожено, Escape/Pause вызывает тот же completion callback, resize
   обновляет video layer, а после ролика MusicCat возобновляется.
4. AVFoundation не открывает исходный AVI-контейнер. Все 14 файлов
   `Data/Video/*.avi` поэтому на этапе сборки remux-ятся FFmpeg с `-c copy` в
   MP4 cache; H.264/MP3 elementary streams не перекодируются, и FFmpeg не
   входит в runtime. M10 verifier требует ровно 14 таких файлов.
5. `DifficultyFrame` новой кампании теперь, как source, проигрывает
   `Main/Main_eng` до создания `profileN`: сам `StartMatch` вызывается только
   callback-ом `cVideoStopped`. Подключены также language branches для planet
   movies и буквальная legacy-развилка `final_eng` для русского/`final` для
   остальных языков.
6. `--video-smoke-test` проверяет кадр `Main_eng`, near-end seek, completion
   и фактический tournament callback. `GamersFrame` использует тот же
   проверенный player для `Intaria/Intaria_eng` и отдельный completion
   `cVideoStopped → GarageFrame`.

### MusicDialog

1. Из `DialogMenu2.cpp` перенесён вызываемый `MusicDialog`: исходная рамка
   `Data/GUI/dlgFrame2.png` размером 334×99, белый Verdana 32 для band и
   серый `175/255` Verdana 24 для track title.
2. Сохранены source offsets `(-130,-21)` и `(-130,17)`, нижний левый anchor
   и полностью показанная позиция 35 px от левого и 30 px от нижнего края.
3. `ShowMusicInfo` вызывается при начальном menu track, каждой automatic/manual
   смене menu track и запуске/смене одного из 11 game tracks. Пока popup
   активен, metadata обновляется без перезапуска анимации, как в source.
4. Сохранена исходная временная функция: 1 s задержки за экраном, 1 s въезда,
   3 s жизни и 1 s выезда. UI z-order переведён в рабочую Metal overlay-полосу:
   legacy `3/2` — это порядок widgets, а не camera-space depth.
5. Popup отправляется последним в menu и race overlay. M8 audio smoke требует
   menu popup, M9 race-render smoke — game popup; обе проверки прошли.
6. Ручная arm64 Debug проверка через фактический экран подтвердила исходную
   рамку и metadata `Stereoside / On our Way` поверх MainMenu2.

### Source environment graph

1. `Environment::ApplyWheater`, `GetPerspectiveCameraFar` и quality map
   перенесены со всеми значениями Fair/Night/Cloudy/Rainy/Sahara/Hell/Snow,
   отдельным High-only shadow для Snow и отключением fog/rain в
   ортографической камере.
2. `GraphManager::BuildOctree` ground AABB теперь вычисляется по всем
   track/decoration/bonus actors до машин и расширяется на 300 м. На этой
   геометрии восстановлены ground grass, Water scale 4, World3/World6 fog
   scale 50/height 3/speed 0.02 и World4 magma scale 25/height 0.5/speed
   0.01, включая различие Low и depth-aware Middle/High.
3. `WaterPlane` и `FogPlane` используют исходное восстановление view-space
   depth через inverse projection, два движущихся texture sample, serialized
   cloud intensity/color и линейный source fog. Вода отдельно получает
   `sunPos`; diffuse/shadow сцены получают направление из `sunRot`, как в
   Windows.
4. `GrassField` перенесён с source partition limit, density 1, scale 1.5,
   displacement 2, четырьмя atlas rectangles и весами `2/1/10/1` из
   `flower2.dds`; indexed Metal mesh является эквивалентной backend-формой
   исходных шести дублированных vertices на sprite.
5. Skybox следует за фактической камерой без translation; perspective far
   равен `120/100` по weather, Garage — 20, Angar — 130, isometric — 150.
   Multi-world render smoke отдельно проверяет Water/volume-surface pass.
6. `SunShaftRender` перенесён после HDR/Bloom/ToneMapping: source depth mask,
   POINT prepare, фиксированные `1280/2` и `1280/4` targets, восемь LINEAR
   ping-pong resample, расчёт позиции солнца через world AABB и исходный
   восьмисэмпловый radial/soft-light composite. Ветка включается только на
   High post-effect, при дневном directional light и perspective camera.
7. `goRefr` перенесён отдельным Middle+ проходом между Scene и Water:
   refractive Actors исключаются из обычных `osColor` queues, чистый кадр
   копируется POINT в свободный полноразмерный scene target, а `osColorRefr`
   рисуется обратно с depth test и без depth write. Metal-эквивалент
   `refract.fx` повторяет `j_swell` distortion, `vScene = 1 - frame`, LINEAR
   sampling и MIRROR addressing чистой сцены. `death2/refr1` сохраняет
   собственные `glRefr`, `goDefault`, `amOnce`, lifetime 0.5 s и scale
   velocity 100, поэтому sprite больше не растёт все 10 s жизни родителя.

## Очередь дальнейшего переноса

### P0 — offline game parity

1. Продолжить исходный menu/widget state machine с оставшимися реально
   вызываемыми `DialogMenu2`.
   GameMode/Tournament/Difficulty/Profile, основные offline subframes
   `RaceMenu2`, `FinishMenu`, `FinalMenu` и активная структура
   `OptionsMenu`, `MusicDialog`, workshop `WeaponDialog` и вызываемые offline
   `InfoDialog`/`AcceptDialog` и `GamersFrame` уже source-derived. Следующий
   конкретный offline-разрыв нужно выбирать по source-аудиту оставшихся
   `RaceMenu2`/HUD/gameplay callbacks. Отдельного ввода имени offline-профиля
   в Windows source нет: `Race::MakeProfileName` создаёт `profileN`.
   Сетевой `NetIPAddress` и используемый гонкой `UserChat` уже перенесены.
   При этом
   legacy animation/widget classes всё ещё заменены immediate-mode bgfx
   backend.
2. Разделить `OriginalRaceSession` по исходным обязанностям и последовательно
   перенести `GameObject`, `Logic`, `Player`, `Race`, `Weapon`.
3. Искать дальнейшие gameplay-разрывы только через конкретные активные
   Windows branches или воспроизводимое отличие, не по отсутствию legacy
   class graph как такового.
4. Перенести оставшиеся исходные offline UI transitions и вызываемые dialog
   branches, найденные следующей ревизией Windows call graph.

### P1 — visual/audio parity

1. Продолжить shader-level parity для D3D9 fixed pipeline/HDR/reflection,
   где backend-замена всё ещё даёт визуально измеримое отличие.
2. Сопоставлять новые graph/effect types по активному resource catalog и
   проверять не только загрузку record, но и фактический renderer branch:
   прежняя проверка catalog не обнаружила потерю `goRefr` при flattening.
3. Проверить offline HUD/camera на всех aspect ratios; активные source states
   и culling semantics перенесены.
4. Сопоставлять новые sound behaviors только по конкретному active record;
   текущие menu/race/UI/voice/weapon/effect triggers покрыты.
5. Провести покадровое сравнение каждой world/weather/car комбинации после
   переноса логики, а не использовать сравнение как замену переносу.

### P2 — полная продуктовая функциональность

1. Оставшиеся startup/intros, обнаруженные source call-graph ревизией;
   `GameMode::Run(true)`, три startup images и последовательный
   `CheckStartupMenu -> StartOptionsMenu` уже перенесены.
2. Проверить полный двухмашинный LAN race вручную. Class ID 1/2,
   state/gameplay/finish/chat, gamer/color conflict rollback, host options,
   planet authority и
   `RaceMainFrame` ready/start warning/kick/leaver UI уже связаны с active
   portable race; следующий исходный разрыв — прочие legacy model/event
   listeners вне фактически подключённых branches.
3. Steam integration, если требуется целевая дистрибуция.

## Критерий закрытия пункта

Подсистема может получить статус «Перенесено» только если одновременно:

1. указан исходный Windows class/function/serialized record;
2. в macOS runtime нет synthetic fallback для нормального пути;
3. все существенные branches исходника либо перенесены, либо явно исключены
   из требований;
4. есть regression, проверяющий поведение, а не только наличие ресурса;
5. финальный preset действительно компилирует и вызывает этот код.

Milestone smoke, успешная сборка и наличие оригинальных ресурсов сами по себе
не доказывают полноту порта.

## Воспроизведение проверки

```sh
cmake --build --preset macos-arm64-m10 --parallel 8

build/macos-arm64-m10/Debug/RRR3d.app/Contents/MacOS/RRR3d \
  --data-dir=resources/game-data \
  --verify-resources

build/macos-arm64-m10/Debug/RRR3d.app/Contents/MacOS/RRR3d \
  --data-dir=resources/game-data \
  --physics-smoke-test

SDL_AUDIO_DRIVER=dummy \
build/macos-arm64-m10/Debug/RRR3d.app/Contents/MacOS/RRR3d \
  --data-dir=resources/game-data \
  --race-render-smoke-test --smoke-test-frames=240
```
