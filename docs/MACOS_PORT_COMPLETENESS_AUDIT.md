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
| Keyboard/mouse/gamepad | XInput/Win32 `ControlManager` | source `VirtualKey`/action state поверх SDL3 | Перенесено с backend-адаптацией | Точные constructor/user bindings, raw menu keys, repeat, focus release, `XUSER_INDEX_ANY` event delivery, hot-plug, 30/255 и 7849/8689 thresholds перенесены; Win32/XInput polling API заменён SDL3 events |
| Audio device/mixer | XAudio2/X3DAudio | SDL3/CoreAudio | Замена платформы | Backend полноценный, но весь исходный game-side `Audio.cpp` object graph не перенесён |
| MusicCat/menu music | `MusicCat`, `DialogMenu2::MusicDialog`, три menu Ogg и 11 game Ogg | `game.xml` catalog, background decode, source shuffle/Play/Stop/Next, in-process pause cursor, `user.xml` playlist + source music popup | Перенесено | Clean profile начинает с пустых очередей; дубликаты/invalid user indices сохраняются до `Play`; Pause выполняет source `StopMusic` и recreation с PCM cursor. Game track извлекается только в `DoStartRace` с кадра 0; `ExitRace` не продвигает очередь, PCM cursor между запусками не сохраняется; MusicDialog использует исходный `dlgFrame2`, serialized metadata, layout и timing |
| Menu SoundSheme | `Menu::SoundSheme`, один Effects source | девять source UI cues и один interrupting SDL voice | Перенесено | `ssButton1..5`, `ssStepper`, Accept/Info и Workshop drag используют исходные click/hover/pickup/repaint/planet/option/accept/warning события без наложения |
| Spatial race audio | X3DAudio game integration | source-derived `m3dFlat` voices поверх SDL | Частично | Перенесены fixed master 0.1, category/source/resource multiplication, 30/45 м, отсутствие pan/Doppler, motor/wheel/ShotEffect, pair-owned `PairPxContactEffect` и lifetime/target-child `LifeEffect` Source3d; общий legacy emitter/priority object graph ещё не компилируется |
| Commentator | `GameMode::Commentator`, serialized `game.xml/commentator/comments` | единый `OriginalGameData` descriptor + source state machine поверх SDL Voice bus | Перенесено | Все 37 comments и voices читаются общим `GameMode::LoadGameData`-совместимым loader; доступные файлы выбранного style связываются с descriptors, а chance/delay/busy/repeatPlayer, weighted choice, prefix/suffix, настоящий битовый `playerId` и все offline race events повторяют source semantics |
| Главное меню, внешний вид | `MainMenu2.cpp` | source-owned `FrameController` поверх bgfx | Частично | Фон/панели/selection и source layout Main/GameMode/Tournament/Difficulty/Profile активны; полный network/credits widget object graph ещё не завершён |
| Навигация меню | `Menu`, `MenuSystem`, `MainMenu2`, `OptionsMenu`, `RaceMenu2`, `FinishMenu`, `FinalMenu`, `GameMode` | source-owned stack, frame policy, Profile/Options/RaceMenu/Finish/Final graphs | Частично | Main/GameMode/Tournament/Difficulty/Profile, Options/StartOptions, RaceMain/Gamers/Garage/Workshop/Angar/Achievment, Finish и Final имеют source availability, navigation, lifecycle, layout и команды; остаются concrete network callbacks и legacy Widget backend graph |
| Dialog/Profile UI | `DialogMenu2.cpp`, `MainMenu2.cpp`, `RaceMenu2.cpp` | source-owned `DialogSystem` + `ProfileFrameState` и backend visuals | Частично | ProfileFrame владеет четырьмя rows, scroll/item-close focus и select/delete; common dialogs source-owned. RaceMenu-specific dialog/widget graph ещё не завершён |
| Race menu | `RaceMenu2.cpp` | source-owned `RaceMenuState` и восемь concrete frame owners | Перенесено с backend-адаптацией | Все шесть RaceMenu states имеют source availability/navigation/lifecycle/layout/commands. Angar включает doors/travel/Spaceship lamp, Achievment — exact definitions, initial no-focus, recursive disabled traversal и Buy modal; bgfx/CoreText и profile transaction остаются backend/application boundaries |
| Options UI | `OptionsMenu.cpp`, `GameMode::LoadGameData` | `originaloptions::OptionsMenuState`/`StartOptionsMenuState` + modal bgfx view | Перенесено с backend-адаптацией | Source owner содержит четыре вкладки, 12/8/5/18 строк, availability, scroll/layout, cyclic steppers, volume bars, control columns/bindings и Apply/Cancel draft. Все шесть languages и оба commentator styles читаются из `game.xml`; first-run owner содержит `Select` sentinel, camera-only Apply gate и persistence command. SDL/CoreText/bgfx заменяют legacy Widget/input/render API |
| Finish/final UI | `FinishMenu.cpp`, `FinalMenu.cpp`, `Menu::OnFinishClose` | source-owned Finish/Final frames и finish transition | Перенесено с backend-адаптацией | Finish владеет точным `Race::Results` order, индивидуальными `voiceNameDur`, reveal/events/last-result/close/layout; Final владеет credit sections, 107 s clock, slide alpha, Back и layout. Profile/video transitions сопоставлены; CoreText/bgfx/SDL audio заменяют legacy Widget/D3D9/XAudio payload |
| Profile serialization | `GameMode::SaveGameOpt`, `Race::SaveGame`, `SnProfile/SkProfile` | `OriginalProfile.cpp`, source-compatible XML | Перенесено | Все поля `user.xml`, `race.xml`, `Profile/*.xml` и `achievment.xml` проходят disk round-trip; сохранены misspelled `dfficulty`, absent-camera first-run gate, offline/network cursors, temporary `skirmish`, ten slots/charges и `CompletePlanet` expansion скрытых планет |
| Tournament/progression | `GameMode.cpp`, `Race.cpp`, menus | parser `tournamet.xml` + source-derived entry flow, advance и finish transitions | Частично | Continue/New/Load/Difficulty, отдельный SkProfile, `GamersFrame`, gamerId selection, pass/planet completion и final branch перенесены; legacy object/event graph ещё не компилируется |
| Race loading transition | `GameMode::StartRace/DoStartRace`, `Menu::msInfo`, `InfoMenu` | deferred bgfx loading state | Перенесено | Оригинальный `loadingFrame.dds` показывается не менее двух кадров до синхронной загрузки world/Jolt/render/audio state; modal input, aspect fit и последующий переход в HUD покрыты integrated race smoke |
| Garage/workshop/tournament data | `RaceMenu2`, `DataBase`, `garage.xml`, `workshop.xml`, `tournamet.xml`, `achievment.xml` | `OriginalGarage.cpp` + source-derived Gamers/Garage/Workshop/Angar/Achievment frames | Частично | Каталог, семь gamers и Viper achievement gate, source available/secret/locked order, buy/sell/install/swap/recharge/upgrade, reward purchase, campaign confirmations, charge-inclusive 50% resale, colors, stats фактической комплектации и bonus preview перенесены. 3D goods/slots/planets/boss cars читают исходные mesh/texture/vehicle transforms и повторяют `ViewPort3d` fitting/rotation. Legacy widget objects ещё отсутствуют |
| Map/catalog loading | `Map`, `MapObj`, `DataBase` | `OriginalRace.cpp`, `OriginalMap.cpp`, `OriginalMapObj.cpp`, `OriginalTrace.cpp`, `OriginalLogic.cpp` | Частично | 88 записей и исходные placements читаются вместе с XML instance name/transform/life/maxTimeLife/timeLife; runtime `Map` владеет семью category lists, стабильными `MapObjRec/MapObjLib` catalogs с RecordNode hierarchy/canonical-relative lookup, ID registry, ground/TouchDeath и Trace. Восстановлены record-based `Map::AddMapObj(ref)` с recursive include proxy clone, detached ownership transfer `Map::InsertMapObj`, source `itemN`/leaf/global `base0/base1` naming, `_mapObj`/`_player`, parent/children/include graph, hierarchical world transforms, narrow `CreateGameObj/Assign`, отдельный `Misc/Crush` special-list с callback container-lock и точный `Logic::OnProgress`: special Decoration → Effects → Car → Bonus → transient. Type replacement сохраняет Player/record/ID, а concrete state сбрасывается до proxy load. Legacy `SerialNode` owner/writer остаётся parser boundary |
| Track collision | PhysX triangle meshes | Jolt triangle meshes из исходных shapes | Перенесено | Используемый race path получает исходные triangles/material groups |
| Vehicle descriptions | `DataBase::CarDesc`, `RockCar` | XML/source constants → `VehicleDescription` | Частично | Mass, body, wheels, motor/gears/suspension перенесены; весь `RockCar`/PhysX state и contact callbacks не перенесены |
| Vehicle simulation | PhysX 2.8.4 `NxWheelShape` | Jolt custom vehicle adapter | Частично | Нативная замена работает; `GameCar` снова является source `GameObject`, serialized `SoundMotor` живёт в его owner, каждый `CarWheel` — child с type-9 `PxWheelSlipEffect`, а included actors гусеницы/подушки владеют exact type-13/type-14 behaviors. Session один раз преобразует Jolt contacts/axle speed в общий результат для Metal и SDL. `GameCar::LockSpring` подавляет airborne pitch как в source, но полная численная эквивалентность PhysX tire/suspension/solver ещё не доказана |
| Car-to-track/car contacts | `GameCar::OnContactModify`, `GameCar::OnContact`, PhysX reports | Jolt contacts → `source::GameCar::OnContact` commands | Перенесено с backend-адаптацией | Source `GameCar` владеет body-contact flag, damage thresholds, shot-transparent border gate, clutch release, exact spring redirect и kinetic-energy attribution; session только подаёт Jolt normal/friction/velocity/energy snapshot и применяет результат. PhysX solver заменён Jolt, но `sumFrictionForce` передаётся и как вектор, а не только как величина |
| Bonus/mine/crater contacts | `Proj::ComputeAABB`, `MineContact`, `MasloContact`, `MineRipUpdate` | source AABB/OBB, lock/contact state и nested-projectile runtime | Частично | Удалены сферы и hardcode осколков; source boxes, 0.25/0.4 lock rules, `ptMineProton`, impulse, oil clutch, nested lifetime/death effects перенесены. Map `AutoProj` снова наследует `GameObject` и готовится/освобождается через `LogicInited/Released`. Динамика осколков остаётся адаптацией к Jolt, не численной копией PhysX |
| Mine placement | `Proj::MinePrepare` PhysX track raycast | source triangle raycast в `OriginalRaceSession` | Перенесено | Используются serialized `proj.pos`, ray `+2/-Z`, только `TrackPlane`, `max(-AABB.min.z, 0.01)`, hit normal; miss не расходует заряд |
| Countdown/checkpoints/laps/place | `GameMode::GoRace`, `Race.cpp`, `Trace.cpp`, `Player.cpp`, `StringLibrary` | active `GameModeRaceState`, `RaceLifecycle`, `RacePlaceModel`, `Player::CarState` + HUD/renderer adapters | Частично | `GameMode` владеет точными offline/network wait/1/2/3/go и finish clocks, включая pause и `>3.0f`; `Race`/`Player` владеют lap/result/place. Исходные `tablo0..tablo4`, block и semaphore перенесены; HUD/minimap/finish labels используют общий source UTF-16LE StringLibrary для всех шести языков. Все special race modes/edge cases ещё не сопоставлены |
| Reset/respawn | `Player::OnProgress`, `ResetCar`, map `TouchDeath` | active `source::Player::ResetCar` + Jolt ray-query adapter | Перенесено с backend-адаптацией | Death plane уничтожает любую машину, сохраняет 3-second touch attribution и ждёт source 2 seconds; `Player` хранит `lastNodeCoordX`, проверяет source `0/-2/+2` rays, до пяти раз отступает на 6 м и переходит на предыдущий tile. PhysX closest-shape заменён тем же запросом к portable collision data |
| AI | `AICar.cpp`, `AIPlayer.cpp`, `Player::CheatUpdate` | source-derived path/control/attack states в session | Перенесено с backend-адаптацией | Перенесены four-track chain/lock masks, `ComputeTrackInd`, `edgeLine/edgeNorm`, turn braking, blocked recovery/reset, retained targets, line/Z shot gates, range/ammo/random/readiness, hyper, mines и difficulty rubber-banding. Неигровая debug visualization исключена, secret-path branch в Windows закомментирован |
| Weapons/projectiles | `Weapon.cpp`, `Player.cpp`, `Logic.cpp` | resident `Weapon`/`Proj` GameObjects + Jolt projectile adapter | Перенесено с backend-адаптацией | Сопоставлены все enum types 0–24 и активные workshop/projectile records: source boxes/rays, forces, timing, groups, homing, attached/ray weapons, mines, nested projectiles, `ptHyper`, `ptSpring`. Fast/attached projectiles владеют source `Proj : GameObject`, проходят общий progress и concrete type-6 DeathEffect listener; target-child и ignore-sender spawn-plan создаётся из GameObject::Death. Rigid-body solver остаётся Jolt; перевод AutoProj/mine runtime на тот же death graph продолжается отдельным блоком |
| Weapon shot effects | `Weapon::CreateShot`, `ShotEffect`, serialized `ctWeapon` behaviors | resident `Weapon : GameObject` → concrete type-10 behavior → source effect graph | Перенесено | Все slot-owned Weapon имеют собственные listener/behavior graph; каждый успешный PrepareProj передаёт serialized `Proj::pos` через `Behaviors::OnShot`, после чего создаётся отдельный WeaponShotEffect с record/local position/ignore-rotation/effective nested lifetime из `db.xml`. Multi-projectile trigger вызывает behavior для каждого принятого actor, как Windows |
| Weapon shot sounds | `Weapon::CreateShot`, `ShotEffect::GiveSource3d`, serialized sound refs | source `ctWeapon/behaviors/items/*[@type=10]/sounds` | Перенесено | Все 24 refs предзагружаются; случайный вариант выбирается на каждый успешно prepared projectile, `drobilka` остаётся без звука, а отдельный Source3d на машину/слот/вариант сохраняет ignore-while-playing и дальний delayed start/resume |
| Damage/support/shield | `GameObject::Damage`, `Logic::Damage`, `TouchDeath`, `DroidItem`, `ReflectorItem`, behaviors | `OriginalLogic` + active `Behavior/Behaviors` + source-typed session adapters | Частично | Перенесены damage types, reflector rules, immortality, touch attribution, death plane, mine exclusion и Droid heal. `Logic` владеет contact behavior/ranges; runtime MapObj получают исходную `GameObject::_logic` связь, которая наследуется parent/children/include graph. Общий 15-type `Behaviors` owner владеет listener registration, deferred removal, progress и shot/motor/immortality dispatch; Player `LowLifePoints`, `ImmortalEffect`, energy `DamageEffect` и динамический Frost Ray `SlowEffect` уже подключены к нему вместо прямых virtual/session вызовов. Подключение остальных concrete backend behaviors продолжается |
| Effect resurrection/lifetime | `ResurrectObj`, `FxSystemWaitingEnd`, `FxSystemSrcSpeed`, `LifeEffect`, `GameObject::OnProgress` | concrete type-2/type-3/type-7 behaviors + active MapObj world detach + recursive include progress | Перенесено с backend-адаптацией | Каждый RaceEffect владеет стабильным source GameObject и listener-зарегистрированными behaviors: positive `maximumTimeLife` посылает Death, type 2 воскрешает/fades и завершает объект после нулевого particle count, type 3 получает Jolt actor velocity и выполняет parent WorldToLocalNorm, type 7 делает один delayed Play. Вложенный effect сохраняет world pose/последнюю скорость при detach. bgfx и SDL только потребляют готовые particle/audio границы вместо создания игровых state machines в render/audio loops |
| Bonuses | `Proj` types 4–10, `Player::TakeBonus` | source boxes, serialized values/DeathEffect и сопоставленные contact branches | Частично | Перенесены persistent speed/lusha/oil, одноразовый `Death()`, medpack/charge/money/immortal, Windows `Round((N-1)*Random())`, charge truncation и source pickup sounds; остаётся ручной portable dispatch вместо исходных объектов/PhysX callbacks |
| Destructible decorations | `DestrObj`, `GameBase`, `GameCar::OnContact` | life flags, active runtime `DestrList`, source fragments/debris и collision meshes | Перенесено с backend-адаптацией | Все map destructibles инстанцируют serialized `_destrList`; `NX_AF_DISABLE_RESPONSE` представлен Jolt sensor с identity владельца, нулевой `dtTouch` разрушает `maxLife == 0`, parent actor удаляется, а `DestrObj::OnProgress` реально вынимает каждый дочерний MapObj, передаёт его в глобальную Map через `InsertMapObj` и даёт world pose родителя. Static/dynamic shapes этих же объектов заменены Jolt bodies, распад одноразовый; в source нет impulse или lifetime для этих частей |
| Achievements | все 9 `AchievmentCondition*` classes, `AchievmentModel`, `AchievmentFrame`, `PlayerStateFrame` | definitions + source-matched counters + source reward frame | Частично | Сопоставлены Bonus/SpeedKill/RaceKill/LapPass/Dodge/LapBreak/Survival/FirstKill/TouchKill и exact record counts; campaign начисляет `Floor(reward × 1/1.2/1.5)`, skirmish не начисляет points и скрывает points HUD. Девять reward cards, state/price, purchase/points и навигация перенесены; generic legacy event/model object graph не компилируется |
| HUD | `PlayerStateFrame`, `MiniMapFrame`, `HudMenu` | `OriginalRaceHud.cpp` с исходными images/strings | Перенесено для offline race | Сопоставлены единственное активное состояние `msMain`, slots/life/place/lap, точная последовательность countdown `tablo0..tablo4`, pick/kill/achievement, opponent/life overlays и finish. `enableHUD` скрывает только `_raceState` и lap, сохраняя map/event siblings как Windows |
| Mini-map | `MiniMapFrame`, `Trace` | source trace-path geometry | Перенесено | Перенесены все pathes, Align/ComputeNode/smoothing, 320×320 align, start marker, 20×20 car markers/colors и `CarState::GetMapPos`-совместимая удерживаемая trace projection |
| Debug trace visualization | `TraceGfx` | `source::TraceGfx` + transient bgfx triangles | Перенесено с backend-адаптацией | Source owner владеет waypoint boxes, path grayscale, selected point/path/tile/link и alpha/material flags. D3D9 Box/Sprite/DrawPrimitiveUP заменены transient Metal triangles; упрощённая зелёная ribbon-заглушка удалена |
| Camera | `CameraManager.cpp`, `View.cpp`, `ActorManager::PullInRayTargetGroup` | `source::CameraManager`, `source::AutoObserver` и cull-opacity backend | Перенесено для offline race/presentation | Перенесены все пять release/debug styles, velocity pose, pull-back, ortho lead/teleport compensation, FlyTo, Garage/Angar AutoObserver, screen/world/ray/XY-plane policy и 0.25 s `gpCullOpacity`; bgfx оставляет matrices, SDL — pointer translation |
| Scene graph/render queues | `GraphManager`, `Actor`, `SceneManager` | custom queues в `OriginalRaceRenderer` | Частично | Основные order buckets есть; generic actor/proxy/octree graph не перенесён |
| Materials | `MaterialLibrary`, `MappingShaders`, `DataBase` | source-derived material catalog + bgfx mappings | Перенесено с renderer-адаптацией | Проверены все 238 активных `Load*LibMat` records; два отсутствующих texture records являются source no-texture projectiles, ещё два — закомментированные `World2/track2` calls. Opaque/alpha/additive/bump/reflection/refraction и material flags сопоставлены без active name fallback; `refract.fx` использует исходные LINEAR/WRAP/MIRROR samplers |
| Lighting/shadows/HDR | D3D9 graph effects | bgfx/Metal passes + active `Player` light attachment | Частично | Реализованы directional race passes, source Player spot/night flare lifecycle, shadow maps Garage/Angar, HDR/bloom/tone map, Middle+ `goRefr` clean-scene/refraction и High-quality perspective SunShaft; flare без `gpReflScene/gpReflWater` исключён из reflection passes. Bit-for-bit и полное graph state parity не доказаны |
| Particles/effects/trails | `FxManager`, effect records | portable emitter/trail renderer | Перенесено для active catalog | В фактическом `db.xml` покрыты все 8 manager classes, все 37 `ntParticleSystem`, единственный активный `FxFlowEmitter`, все 14 `partDesc` и 5 `flowDesc` fields, child systems, distributions, lifetime/fading и `ntIVBMesh`/`ntSprite`/`ntPlane`. Flattened include сохраняет собственные lighting/order/lifetime каждого child Actor; D3D sorting заменён bgfx |
| Weather/water/magma/sky | `Environment.cpp`, `GraphManager`, `WaterPlane`, `FogPlane`, `GrassField`, source `.fx` | source graph + bgfx/Metal shader equivalents | Перенесено с backend-адаптацией | Перенесены шесть world branches, weather fog/ambient/sky/far, quality gates, rain/isometric exclusions, scene AABB +300, UV scale 4/25/50, Low/volume paths, water reflection, depth reconstruction, cloud animation/color/intensity, source grass atlas/density/scale и sky без camera translation; D3D9 заменён Metal |
| Intro/video | `VideoPlayer.cpp`, DirectShow playback | AVPlayer/AVPlayerLayer, 14 lossless-remuxed MP4 | Замена платформы | Все исходные H.264/MP3 потоки проигрываются нативно; Difficulty `Main`, Gamers `Intaria`, planet и final transitions подключены |
| LAN/network | `NetGame`, `NetRace`, `NetPlayer`, NetLib | native Boost.Asio transport + `OriginalNetworkSession` в game runtime | Частично | Исходный `NetLib`, TCP/UDP, reconnect, adapters, class ID 1/2, match/player/vehicle state, исправленная zero-slot `BitStream` сериализация, одноразовый dispatch каждого принятого `ResponseStream`, `NetPlayer::Process` control freshness, generated gamer/color и menu filtering, identity authority и failed rollback, gameplay authority, finish/results, `PushLine`, восемь host option RPC, `ChangePlanet`, двусторонний `StartRace` AI-count reconciliation, `RaceMainFrame` ready/start/kick/leaver UI и полный `DoExitMatch`/`OnExitMatch` lifecycle подключены и проверены loopback. Придуманный Pause sender удалён: в Windows `NetRace::Pause` закомментирован; сохранён только зарегистрированный source receive-handler. Steam backend и полный legacy model/event object graph ещё не перенесены |
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
5. `OnProgress` повторён с delay 0.15 s, reveal 0.5 s и индивидуальным
   `Race::Result::voiceNameDur`: чётные строки въезжают слева, нечётная справа.
   После закрытия восстанавливается соответствующий campaign/skirmish
   `RaceMenu2`, без прежней суррогатной ветки выхода в Main.
6. Отдельный 360-frame Metal fixture проверяет три source rows и не вызывает
   profile save/tournament advance. Ручная arm64 Debug проверка выявила и
   устранила склейку multiline CoreText label; итоговый кадр и
   Return → `RaceMenu2` проверены визуально.

### FinalMenu

1. Generic Credits page и единый многострочный CoreText bitmap удалены.
   `mainmenu2::FinalMenuFrameState` переносит `FinalMenu.cpp::OnInvalidate`
   как девять оригинальных
   `GUI/Slides/slide1..9.dds` и секции `svCredits`, разделённые сначала по
   `\n\n`, затем на красный caption и светлые body-lines.
2. Экран очищается исходным чёрным цветом. Source owner выдаёт clock, alpha
   и layout. Слайды сохраняют собственный aspect каждого DDS в bounds
   `vp - (500, 300)`, находятся в `(vp.x - 400) / 2, vp.y / 2` и получают
   source segment alpha на общей шкале 107 секунд. Титры движутся из
   `vp.y` к `-linesSizeY` при корне `vp.x - 250`.
3. Back command и pointer gate принадлежат source owner; view использует
   `buttonBg2/buttonBgSel2`, Header font, цвет
   `214/255` и позицию `(width/2, vp.y - 60)`. Action/Escape/left click
   внутри кнопки возвращают непосредственно в `MainMenu2`; по истечении
   107 секунд выполняется тот же переход автоматически.
4. `TrackFinal.ogg` подключён отдельным неперсистентным `OriginalMenuMusic`:
   декодирование идёт в фоне, menu MusicCat ставится на паузу, final track
   начинается с нуля и при выходе menu MusicCat возобновляется.
5. Для material alpha общий static UI shader теперь умножает texture/vertex
   color на `u_materialColor`; белый default сохраняет прежнюю отрисовку.
6. `--final-menu-smoke-test` ждёт фактического запуска асинхронно
   декодированного Ogg и затем проверяет все девять слайдов, движение секций,
   Back, фоновый TrackFinal и автоматический возврат. M9/M10 Debug, resource,
   physics и 360-frame race-render regressions прошли. Ручная arm64 Debug
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
8. Эти правила больше не распределены между loader, 22k-line host и
   6.9k-line renderer. `source::Environment` является активным владельцем
   `ApplyWheater`, world profiles, Garage/Angar presentation, quality graph,
   camera far и `StartScene/ProcessScene/ReleaseScene`; renderer получает
   готовую policy и исполняет только bgfx/Metal passes. Отдельный regression
   проверяет weather/world/quality/rain state machine, а race smoke — её
   фактическое использование всеми Metal passes.

### Source Player::ApplyMobility armor-role follow-up

- Повторно сопоставлен активный `Player::ApplyMobility`: mobility-параметры
  суммируются у всех участников, но `cHumanArmorK[difficulty]` применяется
  только при `IsHuman() || IsOpponent()`. Прежний portable-код ошибочно
  умножал `maximumLife` также у `Computer1..Computer5`, делая AI существенно
  прочнее исходной турнирной конфигурации.
- `applyOriginalPlayerProfile` теперь передаёт source-роль каждого racer.
  Обычные campaign/skirmish AI сохраняют множитель `1.0`; локальный Human и
  сетевые Opponent используют `2.0`, `1.75` или `1.5`. Это восстанавливает
  исходную зависимость damage/death/respawn от уровня сложности.
- Regression вычисляет все три human-варианта из одних и тех же serialized
  slots и одновременно доказывает независимость Computer armor от сложности.

### Source Player::TakeBonus value/slot-order follow-up

- Активная ветка `Player::TakeBonus` повторно сверена с `GameObject::Healt`
  и `WeaponItem` state. Удалён synthetic full-heal для medpack с нулевым
  `proj/damage`: порт теперь применяет serialized value буквально.
- Ammunition candidates снова строятся в enum-порядке
  `stHyper, stMine, stWeapon1..stWeapon4`. Ранее основные weapons шли до
  Hyper/Mine, поэтому исходный rounded-random index выбирал другой slot.
- Поведенческий regression проверяет повреждённую машину, точное лечение,
  pickup death/event и тот же результат через `NetPlayer::OnTakeBonus` replay.

### Source Player runtime object block

- `RacerRuntime` больше не является session-local суррогатной структурой:
  public adapter name теперь ссылается на отдельный `source::Player`,
  компилируемый из `OriginalPlayer.h/.cpp` и используемый active race/HUD/net
  path.
- Из `OriginalRaceSession` удалены дублирующие реализации weapon selection,
  lap reload, ammunition bonus target ordering/rounding, money/medpack/shield,
  finish block, campaign rewards, destroy/restore и disconnect state reset.
  Сессия вызывает методы `Player`, оставляя у себя только orchestration
  событий, Jolt requests и network authority.
- Перенесены оригинальные tuning arrays `cHumanEasing*`, `cCompCheat*` и
  `cHumanArmorK`; AI catch-up и `ApplyMobility` используют единый source
  definition вместо повторных literal arrays.
- Новый `OriginalPlayerSmoke` является прямым counterexample regression для
  slot order `Hyper -> Mine -> Weapon1..4`, rounded random, minimum-one ammo,
  reload, exact medpack, immortal, 0.3-second finish brake, 2-second respawn,
  rewards и disconnect. Полный набор теперь содержит 8/8 проходящих CTest.
- Не заявляется перенос renderer/physics ownership исходного Player:
  headlights/color material остаются bgfx-ответственностью, PhysX pointers
  заменены Jolt requests, а `Player::CarState` будет следующим отдельным
  source-class блоком поверх уже перенесённого `source::Trace`.

### Source Player::CarState object block

- `Player::CarState` теперь является вложенным active source-классом и держит
  raw references на стабильный `source::Trace` graph. Session-local массивы
  current/last node, coordinate, inverse start, map position и maximum-speed
  window полностью удалены.
- `CarState::Update` выполняет Windows order: pose/direction, preferred-path
  tile lookup, `curNode` sphere, linked `lastNode`, lap edge, last coordinate,
  wrong-way и lost-control. `GetDist/GetLap/GetMapPos` больше не повторяются
  helper-формулами в `OriginalRaceSession`.
- AI lane occupation использует точные `CarState::track` и `curNode`, а не
  повторно вычисляет оба значения приближённой segment geometry. Respawn,
  Hyper/weapon progress, place/commentator state, HUD/minimap и debug overlay
  также переведены на тот же объект.
- Из сессии удалены `TraceTileProjection`, `findTraceTile`,
  `linkedTraceTransition` и `traceDistance`; `source::Trace::NodeRef` получил
  только equality для существующей boundary-идентификации.
- Прямой `OriginalPlayerSmoke` проверяет main/alternate WayPath, удержание
  map position вне trace, исходный 20-metre moveInverse и 80-unit
  lost-control. Resource-based race smoke сохраняет checkpoint/lap/finish и
  reset coverage.
- Следующим decomposition-блоком должен стать `AICar::PathState`/
  `ControlState`; это позволит убрать оставшиеся AI state arrays из сессии.

### Source AICar path/control object block

- `AICar::PathState` и `ControlState` теперь являются отдельным active
  `source::AICar`, а не длинной формулой внутри `OriginalRaceSession::aiInput`.
  Девять массивов track, lock, brake, blocking, back-moving и reset state
  удалены из сессии.
- Перенесены четыре source lane-search метода, `curTile/nextTile/curNode`,
  five-unit last-node fallback, inner-corner lane target,
  `ComputeTrackNormOff`, turn-brake hysteresis, `pi/128` steering dead zone,
  blocked reverse/forward alternation и трёхсекундный reset.
- `AISystem` lane-chain adapter теперь заполняет `PathState::lockTracks`;
  `source::AICar` возвращает backend-neutral move/steer command, которую
  существующая граница переводит в Jolt `VehicleInput`.
- Terminal-node random selection использует callback и вызывает source RNG
  только тогда, когда Windows действительно вызвал бы `GetRandomNode`.
- Новый девятый CTest `OriginalAICarSmoke` независимо проверяет lane search,
  corner braking, off-trace retention, blocking recovery и reset; полный
  resource regression сохраняет AI attack/Hyper/mine/finish coverage.
- Этот path/control этап оставлял `AttackState` в session; следующий раздел
  фиксирует завершение и этой границы.

### Source AICar attack object block

- `AICar::AttackState` теперь находится рядом с уже перенесёнными
  `PathState/ControlState`. Из сессии удалены три retained AI arrays:
  front target, back target и mine random; `PlayerDispose` очищает ссылки
  через source state.
- Перенесены точные `FindEnemy` и `ShotByEnemy`: front/back `pi/4`, live-tile
  Z level, target-size hysteresis, lateral/Z alignment, all-weapon readiness,
  max-distance sort, 25% source RNG, ammo distribution и back-only torpedo.
- Hyper decision использует live `WayNode` coordinate, turn `pi/6`, текущий
  `PathState::brake` и source charge curve. Mine decision сохраняет
  `placeMineRandom`, 5–95% progress, nearby-back bonus и отдельный maslo
  лимит; фактический shot readiness остаётся у Weapon boundary, как в
  `Logic::Shot` Windows.
- Удалён ставший лишним второй `mineCooldown_`: единственным source clock
  снова является эквивалент `Weapon::_shotTime` (`mineShotAge_`). Per-frame
  target snapshot переиспользует буфер без heap allocation, а четыре weapon
  candidates обрабатываются fixed-size массивом.
- `OriginalAICarSmoke` расширен front-retention, readiness-abort, back
  torpedo, Hyper, Mine и dispose regressions; resource race regression
  проверяет связку с projectile/Jolt/effect backend.

### Source AISystem lane-chain block

- `AISystem::ComputeTracks` теперь является отдельным active
  `source::AISystem`; длинный session-local `AiTrackState`/`tileStripContains`
  adapter и BFS удалены.
- Система читает прямые `Player::CarState::GetLiveTile/GetCurNode` и
  `WayNode::Tile`, сохраняет source insertion order, signed longitudinal
  overlap, radius gates, chain merge, lateral stable sort и lane locking.
- Историческая функция `lsl::ClampValue` перенесена буквально для случая
  цепочки длиннее `cTrackCnt`; она намеренно не заменена на `std::clamp` с
  недопустимым диапазоном.
- `OriginalRaceSession` передаёт только non-owning записи с Jolt position и
  resource-derived car radius. Переиспользуемый scratch buffer не выделяет
  память в каждом кадре.
- Прямой `OriginalAICarSmoke` проверяет ожидаемые lock masks цепочки из трёх
  машин и обязательный reset полос у одиночной машины.

### Source AIPlayer owner block

- `source::AIPlayer` теперь владеет отношением portable `Player`/`AICar` и
  сохраняет source `CreateCar`, `FreeCar`, `OnProgress`, AI enable и
  target-dispose lifecycle. Session-owned `std::vector<AICar>` удалён.
- Lane-chain, steering/control, attack, blocked reset и disconnect проходят
  через owner вместо прямого доступа к вложенным состояниям.
- Computer owners получают source faster+slower cheat bits; human owner не
  получает их. Catch-up/easing использует этот mask, а network HumanPlayer
  остаётся отдельной faster-only веткой.
- Human owner создаётся dormant для отдельного legacy debug режима; F7
  меняет его source enable gate, не создавая синтетического участника.
- Прямой smoke проверяет ownership, masks, disabled command и полное
  освобождение AICar state.

### Source Weapon runtime block

- Новый active `source::Weapon` переносит `Desc`, растущий `_shotTime`,
  strict `IsReadyShot`, `IsMaslo` и reset только после успешного
  `PrepareProj`. На каждого игрока создан source `WeaponRack` из четырёх
  primary slots, Hyper и Mine.
- Session arrays `weaponCooldown_`, `mineShotAge_`, `hyperCooldown_` удалены.
  Weapon timers прогрессируют и во время countdown, но останавливаются на
  pause, как зарегистрированные Windows `GameObject`.
- Удалён synthetic player floor 0.03. Повторная сверка с оригинальным
  `AICar.cpp::ShotByEnemy` подтвердила отдельное исходное правило AI
  `max(shotDelay, 0.25)`; оно восстановлено после ошибочного удаления.
- Charge и timer меняются лишь после наличия live projectile; failed mine
  track ray и failed Hyper wheel gate не расходуют выстрел.
- Analog mine threshold и AI readiness используют тот же source timer;
  `ptMaslo` определяется первым projectile type.
- Новый `OriginalWeaponSmoke` доводит CTest-набор до десяти тестов и отдельно
  проверяет strict time boundary и успешный/неуспешный shot commit.

### Source WeaponItem/Logic execution block

- `source::WeaponItem` связывает `Weapon` с profile-backed current/capacity
  полями `source::Player`; отдельная расходящаяся копия charge не создана.
- Primary, Hyper и Mine используют исходную `WeaponItem::Shot` транзакцию:
  backend возвращает результат projectile preparation, затем одним commit
  меняются charge и weapon timer. Lap reload вызывает `WeaponItem::Reload`.
- Восстановлен sentinel `maxCharge == 0` для бесконечного боезапаса.
  Сетевой явный `newCharge` применяется и при failed preparation, как в
  `NetPlayer::DoShot` Windows.
- `source::Logic::Shot/ShotAll` формирует маску в порядке Hyper, Mine,
  Weapon1..4, проверяет readiness и выдаёт отдельный `HumanShot` event даже
  для dry primary/mine request; Hyper в это событие не входит.
- `source::HumanPlayer::SelectWeapon` переносит поиск следующего primary с
  положительным `curCharge`, автоматическое переключение после последнего
  заряда и отсутствие shot request, когда заряженных слотов нет.
- Unit и resource smoke покрывают infinite ammo, failed prepare, replicated
  charge commit, selection, multi-slot fire и dry human event.

### Source HumanPlayer owner/control block

- Отдельный `source::HumanPlayer` хранит Windows `_curWeapon` и переносит
  `GetWeaponByIndex`, contiguous `GetWeaponCount`, bounded next/previous и
  повторный `SelectWeapon` после последнего заряда.
- SDL device state теперь проходит через source `Control::OnInputProgress`
  command: accel приоритетнее reverse, оба движения бинарны, left
  приоритетнее right вместо синтетического взаимного вычитания.
- Session-local dynamic usable-slot list и ручной direct-slot ordinal search
  удалены; current/direct/all weapon paths используют один owner.
- Новый одиннадцатый `OriginalHumanPlayerSmoke` проверяет selection owner,
  direct ordinal, границы переключения и driving priority.

### Source DroidItem/ReflectorItem physical-slot block

- `workshop.xml` сохраняет исходный `Slot::Type` как `WeaponItemType`:
  Hyper=5, Mine=6, Weapon=7, Droid=8, Reflector=9. Support behavior больше
  не определяется по ненулевым полям описания.
- Active `source::DroidItem` переносит car create/destroy progress
  registration, strict `time > repairPeriod`, reset таймера при полном
  здоровье/death и исходный literal `Healt(5.0f)`. Сериализованный
  `repairValue` сохранён, хотя Windows branch его не использует.
- Active `source::ReflectorItem` переносит clamped коэффициент урона.
  `Logic::Damage`-adapter обходит touch damage и применяет только первый
  физический слот типа Reflector, как `Player::GetSlotInst(stReflector)`.
- Четыре physical `Slot` самого Player владеют полиморфными Droid/Reflector.
  Несколько Droid имеют независимые таймеры; death, respawn и
  disconnect вызывают исходный item lifecycle.
- Session-local `repairSeconds_`, description scan и ручной reflector math
  удалены. Unit smoke и resource race smoke проверяют реальные записи
  `droid`/`reflector`, профильную установку, 40% отражение и active healing.

### Source continuous-contact and PairPxContactEffect lifecycle block

- `Proj::SonarContact` теперь передаёт уже рассчитанный исходником
  `damage * contact.deltaTime` в `Logic::Damage` ровно один раз. Повторное
  умножение на frame delta в session adapter удалено; тот же результат
  одновременно используется для линейного и углового импульса.
- `PairPxContactEffect::ReleaseContact` снова допускает сосуществование
  старого затухающего `spark2` и нового эффекта той же actor-pair/slot.
  Новый контакт больше не реанимирует старый particle object.
- Release lookup выбирает живой effect instance. Ранее затухающий instance
  перехватывал release нового объекта, из-за чего новый `RaceEffect`
  оставался в контейнере навсегда и накапливался во время гонки.
- Resource race regression проверяет точный single-delta `ptSonar` damage и
  impulse, повторное создание contact effect до завершения старых частиц и
  полное освобождение обоих поколений без роста `effects_`.

### Source GameObject network frame-synchronization block

- Перенесены `GameObject::SetPosSync`, `SetRotSync`, `SetPosSync2`,
  `SetRotSync2` и последовательность их `OnFrame`: translation error
  выбирается со скоростью `5 units/s`, rotation — `1.3*pi rad/s`, а
  correction длиной `>=5` не сглаживается, как в исходнике.
- Активный `NetPlayer::ResponseStream` использует исходные пороги: position
  snap только при расхождении `>4`, rotation snap — при `>pi/24`. Jolt
  получает авторитетную physics pose/momenta, а отдельный graph pose
  сохраняет старый кадр и плавно догоняет body.
- Graph correction применяется к кузову и всем четырём world-space wheel
  transforms; renderer и HUD opponent projections используют эту позу,
  gameplay collisions/AI/trace продолжают получать фактический Jolt state.
- `OriginalGameObjectSmoke` проверяет первый кадр после snap, source rates,
  strict пятиединичную границу и второй position-sync channel.

### Source GameObject listener/behavior-dispatch block

- Перенесён backend-neutral контракт `GameObjListener` и исходные операции
  `InsertListener`, `RemoveListener`, `ClearListenerList`: listener не
  регистрируется дважды, container остаётся non-owning, а dispatch работает
  по snapshot списка и допускает удаление listener из собственного callback.
- `GameObject::Damage` снова сначала записывает авторитетный `newLife`, затем
  вызывает object/behavior и внешние `OnDamage`, после этого обновляет touch
  attribution и только затем рассылает `OnDeath`. Прямой `Death` сохраняет
  target и уже существующую touch attribution, как Windows source.
- `Player` больше не запускает `ImmortalEffect` и `EnergyDamageEffect`
  вручную из `OriginalRaceSession`. Оба behavior получают damage и смену
  immortality через виртуальный GameObject event graph; одноразовое создание
  energy effect лишь считывается session adapter для визуального события.
- Восстановлены отдельный final `DestroyObject`/`OnDestroy`, автоматический
  `OnImmortalStatus(false)` при окончании таймера и `OnLowLife` перед первым
  `LowLifePoints::MakeEffect`. Копирование/сброс игрового объекта не переносит
  ссылки на behavior/listener другого экземпляра.
- `OriginalGameObjectSmoke` фиксирует порядок callbacks, уже присвоенную life,
  target death, low-life, одноразовый destroy и удаление listener;
  `OriginalPlayerSmoke` проверяет автоматические immortal/energy callbacks.

### Source Player::ResetCar ownership/correctness block

- Полный поиск точки восстановления перенесён из
  `OriginalRaceSession::queueRespawn` в active `source::Player::ResetCar`.
  Player теперь сам владеет `GetLastNode` fallback, сохранённой tile
  coordinate, последовательностью `0/-2/+2`, пятью попытками с шагом 6 и
  переходом через `WayNode::GetPrev`; session передаёт только результат
  Jolt/world raycast и исполняет готовую pose.
- Подтвердилось числовое расхождение: старый adapter поднимал каждый ray на
  `ComputeHeight(currentCoordinate)/2`, тогда как Windows использует
  `ComputeHeight(0.5)/2` для всех samples. На сужающейся/расширяющейся tile
  это меняло ближайший collision shape.
- Подтвердилась lifecycle-ошибка: неуспешная попытка перезаписывала итоговую
  pose своим sample 0. Если все пять мест заняты, машина получала последнюю
  заведомо заблокированную позицию; source сохраняет первоначальный fallback
  и меняет pose только после прохождения всей тройки rays.
- Удалён отсутствующий в source fallback через session `nextPathNode`: при
  пустом `lastNode` Windows выбирает первый node главного path.
  `OriginalPlayerSmoke` проверяет variable-width tile, все три rays,
  death-plane restart, полностью blocked search и неизменный fallback;
  resource physics smoke продолжает проверять active map reset.

### Source damage events / AchievmentModel ownership block

- `GameObject` теперь владеет исходными dispatch points `cPlayerDamage` и
  `cPlayerKill`; `Player::OnDeath` формирует Overboard/DeathMine/Death в
  Windows-порядке. Session больше не создаёт synthetic Kill для любого
  уничтожения машины.
- Девять `AchievmentCondition` и их persistent/race-local state перенесены в
  отдельный active `source::AchievmentModel`. В session остался только
  backend adapter событий и выдача HUD-события завершённого условия.
- Подтверждённое отклонение финального круга исправлено: LapBreak может
  завершиться на последнем `cRacePassLap` до finish transition. При этом
  исходная NULL-data ветвь `cRaceFinish` для LapPass не заменяется придуманным
  player finish event.

### Source Race lifecycle owner

- `Player::OnLapPass`, `Race::OnLapPass` и обе формы `Race::CompleteRace`
  перенесены в active source owners. Новый `RaceLifecycle` владеет списком
  результатов, planet rewards, picked money и сортировкой оставшихся машин;
  session применяет решения к Jolt/renderer/network adapters.
- Удалена synthetic рассылка `cRacePassLap` компьютерным игрокам. Она
  расходилась с `Player::IsHuman()` и могла ошибочно двигать achievement/HUD
  state. События 1/2/3/last и их исходный if/else-if порядок проверяются
  отдельным regression.
- Восстановлена редко видимая Windows-ветвь гонки без Human: последний AI
  отправляет `cRaceFinish`. Finish UI и network serialization теперь читают
  захваченный `Race::Result`, а не уже сброшенный `Player::pickMoney`.
- Проверка: 13/13 non-network CTest, original resource verifier и полный
  map1 Jolt physics smoke.

### Source Race place/late-progress owner

- `Race::OnLateProgress` больше не является session-side imitation. Active
  `RacePlaceModel` хранит прежний `_playerPlaceList`, сортирует finished по
  source `place`, остальных по `CarState::GetLap`, переназначает места и
  выдаёт LeadChanged/ThirdChanged/LastFar/Domination/ThirdFar.
- Подтвердился баг сетевых результатов: surrogate `100000-finishTime` мог
  уничтожить полученный по сети порядок мест. Он удалён; regression подаёт
  одинаковое время с порядком 2/1/3 и требует source order 1/2/3.
- Membership change очищает previous list, как `Race::DelPlayer`, поэтому
  disconnect не становится выдуманной сменой лидера. Thresholds и
  result-suppression перенесены без изменения.

### Source Race::ExitRace completion path

- Подтвердилось расхождение досрочного выхода. Windows всегда вызывает
  `CompleteRace(results)` до teardown/save, а порт сохранял только текущий
  Human state и возвращался прямо в RaceMenu. Теперь все активные машины
  получают Results/rewards, Tournament обрабатывается тем же путём, и
  `Menu::ExitRace` открывает FinishMenu через эквивалент
  `ExitRaceGoFinish`.
- Network host завершает source result graph до формирования ExitRace RPC;
  captured picked money берётся из `RaceLifecycle`, а не из уже сброшенного
  Player. Повторный exit не дублирует награды или результаты.

### Source Player::CheatUpdate owner

- `Player::CheatUpdate` перенесён из session adapter в `source::Player` вместе
  с `CarState::cheatSlower/cheatFaster`. Fixed-step order теперь совпадает с
  Windows: Player updates предшествуют `AISystem::OnProgress`.
- Исправлена ошибочная portable-гипотеза «сравнивать с любым самым дальним
  racer». Source filter допускает только Human/Opponent; обычный Computer не
  может быть reference для rubber-banding другого Computer. Старый integrated
  regression фактически закреплял ошибку и заменён прямой source-проверкой.
- Jolt по-прежнему является backend boundary: вычисленные исходным owner
  torqueK/steerK отображаются на `motorTorqueScale/lateralGripScale`, а
  `cheatSlower` потребляется активным `AICar::ControlState` в тот же кадр.

### Source Player::OnProgress owner and countdown order

- Подтвердилось, что portable countdown пропускал весь `Player::OnProgress`,
  хотя исходный `Race::OnFixedStep` ограничивает флагом `_goRace` только AI.
  Player/CarState, cheat cleanup и start block теперь обновляются и до
  зелёного сигнала.
- Restore и block возвращены внутрь одного `source::Player::OnProgress` owner
  и выполняются перед AI. Session лишь адаптирует pose/raycast/input на Jolt.
- Правильный порядок обнаружил вторую ошибку: session вызывал `aiInput` даже
  после `AIPlayer::FreeCar` и пустой командой стирал финишный brake. Условие
  `HasCar()` восстановило исходную семантику `AIPlayer::OnProgress` и убрало
  движение компьютеров после финиша.

### Source Player::FindClosestEnemy owner

- Подтвердилось, что оружейный runtime оставлял поиск цели в session surrogate.
  Он поддерживал только нулевой/положительный cone и не имел `zTest`.
  Лямбда удалена; projectile/homing paths вызывают active
  `Player::FindClosestEnemy`.
- Перенесены negative rear cone, source forward-plane metric и Z-level test.
  `CarState` хранит полный `dir3`, не только XY trace direction, поэтому
  наведение на наклонных участках не использует выровненную по горизонту
  подмену.

### Source Player bonus-projectile identity owner

- Подтвердилось, что сетевой id счётчик мин был придуман как общий session
  projectile counter. Он увеличивался на primary и hyper, в отличие от
  Windows `Player::InsertBonusProj`, вызываемого только для `stMine`.
- Registry и sequence перенесены в Player. Mine destroy/timeout/MineRip
  удаляют live id, replicated MineContact проверяет owner registry, а обычный
  выстрел больше не сдвигает следующий mine id.

### Source Player::SetCar active record owner

- Перенесены исходные `Player::GetCar/SetCar` semantics для ссылки на
  выбранный `ctCar`: active Player хранит персонально настроенный `Vehicle`,
  а замена record сначала исполняет `FreeCar(true)` и сбрасывает прежний
  `CarState`.
- Session physics/AI/weapon/effect branches больше не возвращаются к
  неизменяемому `Race::Racer` descriptor. Один Player record определяет
  collision shape, mass, mounts, damage/death/shield graphs и car bounds.
- bgfx/Metal остаётся adapter этой записи: основной и shadow passes, wheel/
  track animation, tire trails и camera cull target берут ту же active
  definition. Descriptor используется только до создания Players для
  загрузки GPU assets и как защитный fallback.
- `OriginalPlayerSmoke` проверяет, что смена записи отсоединяет существующую
  машину, очищает lap state и требует нового `CreateCar`, как Windows
  `Player::SetCar(MapObjRec*)`.

### Source Player physical item-slot owner

- `DroidItem` и `ReflectorItem` больше не хранятся ни в session-массиве, ни в
  промежуточном `PlayerItemRack`: это полиморфные предметы исходного
  `_slot[stWeapon1..stWeapon4]`.
- `Player::CreateCar/FreeCar` владеют `OnCreateCar/OnDestroyCar`, поэтому
  death, restore, disconnect и race exit не требуют дублирующих adapter
  callbacks. Droid progress также выполняется внутри Player behavior owner.
- `Logic::ResolveDamage` получает Reflector из самого target Player. Touch
  damage по-прежнему обходит reflector, а остальные типы используют первый
  физический reflector в порядке слотов.
- Session parallel `playerItemRacks_` удалён. Regression проверяет полный
  bind/create/progress/free lifecycle активного Player.

### Source Player weapon-object owner

- `WeaponRack` четырёх primary slots, Hyper и Mine встроен в active Player;
  отдельный `OriginalRaceSession::weaponRacks_` удалён.
- Readiness/cooldown, `IsMaslo`, успешные projectile callbacks и
  `ShotEffect` принадлежат тому же Player, что charge, selected slot и
  physical Droid/Reflector items. AI и Human используют один owner.
- Как зарегистрированные Windows GameObjects, Player-owned Weapon продолжают
  progress во время countdown; session только обходит активных Players и
  исполняет backend-neutral tick.
- Player regression покрывает fired/not-ready/progress/ready и привязку Droid
  к встроенному primary Weapon.

### Source Player::_slot[] physical layout and mobility owner

- Исправлена прежняя модель `PlayerSlotRack`: serialized `Slot::Type`
  (`Weapon`/`Droid`/`Reflector`) отделён от physical `Player::SlotType`
  (`stWeapon1..stWeapon4`). Четыре оружейных места больше не схлопываются.
- Active Player владеет полным physical rack и предоставляет source-derived
  `GetSlot`/`GetSlotInst`; поиск по class type возвращает первое совпадение в
  исходном порядке физических слотов.
- Полный workshop catalog сохраняется внутри `Race` как стабильное хранилище
  Record-ссылок. Каждый racer связывает свой loadout до `CreateCar`, а
  mobility accumulation вызывается через `Player::ApplyMobility`.
- Slot regression покрывает раздельные Weapon1/Weapon2/Weapon3 с
  Droid/empty/Reflector и class lookup, resource audit — наличие полного
  workshop catalog в active Race.

### Source persistent Player WeaponItem owner

- Четыре primary `WeaponItem`, Hyper и Mine теперь постоянно принадлежат
  active Player, как предметы физических `Player::_slot[]` в Windows.
- Bind после окончательного loadout связывает каждый предмет с тем же
  Player-owned `Weapon`, profile-backed charge и исходными maximum/count/
  step/damage полями; Droid/Reflector настраиваются в той же операции.
- Human selection, Shot/ShotAll, AI, mine/hyper, network replication и
  `Player::ReloadWeapons` больше не создают временные `WeaponItem` wrappers в
  session. Readiness, cooldown и charge transaction читают единый объект.
- Player/lifecycle regressions проверяют устойчивую identity, общий charge
  storage и исходный reload через постоянные items.

### Source Slot-owned polymorphic weapon items

- `WeaponItem` наследует portable `SlotItem`; `Slot::CreateItem` создаёт
  `HyperItem`, `MineItem`, `WeaponItem`, `DroidItem` и `ReflectorItem` по
  исходному serialized class id. Второго runtime item рядом со Slot больше
  нет.
- Player reload, Human/Logic selection, network/local shot, Droid lifecycle
  и Reflector lookup получают один предмет непосредственно из physical Slot.
  Временные Player-массивы items и `PlayerItemRack` удалены.
- Применение human profile теперь перестраивает все десять физических слотов
  из `slot0..slot9` (`stWheel..stWeapon4`) до binding weapon backend; запись,
  class identity и charge больше не расходятся со стартовым race loadout.
- Weapon/Player/resource regressions покрывают Slot identity, несколько
  Droid, first Reflector, профильную установку и полный create/free/reload
  lifecycle.

### Source WeaponItem current-charge owner

- Текущее количество зарядов перенесено буквально из Windows-поля
  `WeaponItem::_curCharge`; staging-массивы `RacerRuntime` используются лишь
  для первоначального resource/profile binding и после него не являются
  runtime-состоянием.
- Human, AI, HUD, profile writer, bonuses, lap reload, Hyper/Mine, ShotAll и
  network replication читают или меняют установленный Slot-owned item через
  исходные `GetCurCharge/SetCurCharge/Reload/Shot`.
- Regression проверяет, что live выстрелы и reload не мутируют входную
  staging-копию, включая failed replicated shot с явным `newCharge`.

### Source WeaponItem projectile descriptor owner

- Перенесён исходный `_wpnDesc`: каждый непосредственный projectile хранит
  type, speed, maxDist и damage; `SetWpnDesc` применяется к live Weapon, а
  detached `GetDesc` возвращает сохранённое описание предмета.
- `GetDamage(bool)` суммирует projectile damage. Старое serialized поле
  `damage`, помеченное Windows-кодом как invalid, больше не подменяет эту
  статистику; death-effect spawned projectiles корректно исключены.
- Загружается `chargeCost`; AI attack/Hyper/Mine descriptor fields читаются
  из установленного item. Реальные bulletGun/rifleWeapon и lifecycle
  descriptor apply покрыты resource и integrated regressions.

### Full Proj::Desc shot source

- `_wpnDesc` расширен с четырёх статистических полей до полного portable
  `Proj::Desc`: transform/collision/model placement, speed/range/lifetime,
  mass/damage, visual/death/nested-projectile данные.
- Primary, Hyper и Mine создаются по live descriptor установленного
  `WeaponItem`; `WeaponDefinition` используется как стабильный asset/index
  bridge для bgfx/Jolt и generated death projectiles, но не как параллельный
  владелец параметров нового выстрела.
- Integrated regression меняет только item descriptor после binding и
  проверяет реальные speed/maxDist/damage созданного projectile.

### Proj::Desc snapshot lifetime

- Как и Windows `Proj`, каждый уже подготовленный projectile/mine удерживает
  то описание, с которым был создан: последующий `SetWpnDesc`, замена слота
  или уничтожение машины не меняют его движение, damage, collision и effects.
- Снимок immutable и разделяется между снарядами одного выстрела. Это
  сохраняет source lifetime без глубокой покадровой копии тяжёлых visual и
  particle records; static race catalog остаётся только GPU/index bridge.
- Session lifecycle, MineRip/death branches и bgfx renderer читают один
  snapshot. Regression заменяет live item descriptor после выстрела и
  подтверждает сохранение прежних `77/321/9.25` у летящего projectile.

### Complete profile/config serialization owner

- Сопоставлены все поля `GameMode::SaveGameOpt/LoadGameOpt`,
  `Race::SaveGame/LoadGame`, `SnProfile` и `SkProfile`: quality, resolution,
  volumes, gameplay options, controls, обе MusicCat playlists, profile lists,
  tutorial, tournament, human/car/color/economy и все десять slots/charges.
- Загрузка `planetsCompleted` теперь вызывает один portable
  `Race::CompletePlanet`-эквивалент. Финальная турнирная планета `4`
  автоматически добавляет скрытую планету `5`, а повторные записи не
  дублируются — именно так Windows восстанавливает `race.xml`.
- `lastProfile/lastNetProfile` разрешаются только через общий source profile
  list. Полный disk round-trip дополнительно проверяет achievement items,
  conditions/iterations и намеренно ошибочное имя `dfficulty`.

### B8a: атомарный AI progress frame

Повторная сверка с оригинальными `AICar::UpdateAI`, `AICar::OnProgress`,
`AIPlayer::OnProgress` и `AISystem::OnProgress` подтвердила один оставшийся
структурный дефект: Path/Control и Attack выполнялись двумя session-проходами
в разных точках кадра. Теперь `source::AICar` сам сохраняет точный порядок
`PathState -> AttackState -> ControlState`, а `source::AIPlayer` возвращает
единый move/attack result. Session передаёт только Jolt/weapon snapshot и
исполняет готовые команды; отдельного session-owned вызова Attack больше нет.

Regression одновременно проверяет движение и выбор цели/оружия из одного
кадра. Открытая AI-граница ограничена network authority filter и backend
отрисовкой AIDebug. Прошли arm64 build, 25/25 offline, 2/2 network, physics
и 360-frame bgfx/Metal race smoke; следующая gameplay-ревизия — `GameCar`
callback order.

### B8b: единый source GameCar contact callback

Оригинальный `GameCar::OnContact` был подтверждён как оставшаяся
session-owned реализация: его track и car branches находились в разных
частях gameplay loop. Теперь `source::GameCar` атомарно выполняет исходные
border/car пороги, clutch release, spring redirect, kinetic-energy damage
attribution и decoration touch. Jolt adapter передаёт normal/friction force,
velocity/forward и energy, затем применяет только готовую команду.

Удалена отсутствующая в Windows подстановка binormal при нулевой friction
force; проверка скорости снова использует модуль линейной скорости PhysX, а
не отдельное session-поле speed. Regression покрывает все существенные
ветви callback. Следующий B8c — следующий подтверждённый разрыв в
`Player/Race/Weapon` после прямой сверки методов.

### B8c: source-owned Player presentation graph

Восстановлены backend-neutral владельцы исходных `Player::InitLight`,
`FreeLight`, `SetLightParent`, `CreateNightLights`, `SetLightsParent`,
`ApplyReflScene`, `ApplyColorMat` и `ApplyColor`. Новый
`Player::PresentationState` хранит lifetime/attachment двух spot lights,
night flare и его garage nodes, reflection flag и первый клонированный
car-material с живым цветом.

Metal renderer больше не вычисляет число/позиции фар из enum, не читает
night-light catalog вместо Player и не решает material/reflection gameplay
gates. Он только применяет готовое source presentation state к Jolt world
pose и bgfx pass. Regression покрывает состояние до создания машины,
CreateCar, переключение Two→One, FreeCar/CreateCar, SetHeadlight(None),
цвет и `gpReflScene`. Следующая ревизия B8d — оставшиеся `Race/Weapon`
branches. Прошли arm64 build, 25/25 offline, 2/2 network, physics и
360-frame Metal race smoke.

### B8m: source Race fixed-step cadence

Подтвердилось, что `OriginalRaceSession::update` вызывал
`Player::OnProgress` и `AISystem::OnProgress` один раз с render delta, тогда
как Windows `World` вызывает `Race::OnFixedStep` перед каждым физическим
шагом. При просадках FPS это особенно сильно меняло AI state machine,
restore timers и checkpoint/lap sampling.

Теперь Jolt имеет отдельный source clock `World::cMaxSimStep == 1/60`. В нём
исполняется исходный порядок всех Player, затем AISystem и per-car GameCar.
Два внутренних Jolt substeps 1/120 используют закэшированную drive command,
не удваивая AI RNG, timers, steering и stabilization; respawn применяется до
solver. Event adapter сохраняет fixed-step события до следующего main pass.
Frame update этот код больше не дублирует. Regression отдельно проверяет два
render frames по 1/120 и единственный source callback. 29/29 CTest, physics и
720-frame Metal smokes прошли; обычная
подписанная arm64 `.app` также открыта через LaunchServices и показала главное
меню.

### B8n: separate source and backend fixed clocks

Windows константа `World::cMaxSimStep` равна 1/60, поэтому source fixed events
не должны наследовать внутреннюю частоту Jolt 1/120. Добавлен persistent
аккумулятор: на 120-Гц render первый кадр не вызывает Race/GameCar, второй
вызывает их один раз с delta 1/60. Jolt между ними использует закэшированную
drive command. Regression, physics smoke и 720-frame Metal race подтвердили
единственный reset, скорости игрока 39.36 и AI 37–42 без деградации.

### B8o: ordered AI fixed-step actions and reset bridge

Подтверждены два расхождения с Windows `AICar::OnProgress`. Порт хранил
только последний `AttackDecision` render-кадра, поэтому при catch-up ранний
выстрел мог исчезнуть. AI reset извлекался позже в frame gameplay и не мог
войти в текущий Jolt reset batch.

Добавлена упорядоченная очередь каждого Shot/Hyper/Mine результата source
fixed-step; stale команды удаляются при finish/disconnect/death, а concrete
Weapon повторно подтверждает readiness. `TakeResetCar` теперь выполняется в
той же `Race::OnFixedStep` транзакции и выдаёт reset до solver. Regression
проверяет потерю цели на втором fixed-step и same-batch off-trace reset.
Оставшаяся B8p граница: сам `Logic::Shot` пока materialize-ится в следующем
render adapter, поэтому charge/cooldown первого шага ещё недоступны второму
source шагу внутри того же редкого кадра.

### B8p: source-owned AI weapon transaction

Оставшаяся граница B8o подтверждена сравнением с Windows `Logic::Shot`:
source-объекты снарядов, charge и cooldown обязаны изменяться в том же
`Race::OnFixedStep`, где `AICar` принял решение. Перенос команды без её
`WeaponItem::Shot` транзакции оставлял второй catch-up step в неверном
состоянии.

Для обычного оружия AI fixed-step теперь вычисляет исходный transform
установленного slot/weapon, строит `ShotContext`, вызывает concrete
`Weapon::CreateShot` через `Player::Shot` и сохраняет созданные `Proj` как
временный ordered view. Frame adapter больше не вызывает `Shot` повторно:
он только создаёт backend projectile/effect/event view для уже принадлежащих
`Logic` объектов. Очистка pending-транзакций выполняется до disconnect/
finish владельца.

Regression фиксирует single-consume charge/cooldown через два fixed-step без
render update. Полные arm64 build, 29/29 CTest, physics smoke и 360-frame
Metal race smoke прошли. Оставшаяся B8q граница — выполнить такую же
source-step materialization для `Logic::Shot(stHyper)` и
`Logic::Shot(stMine)`.

### B8q: source-owned AI Hyper and Mine transactions

Обе оставшиеся специальные атаки перенесены из frame adapter в место их
вызова Windows `AICar::AttackState`. `stHyper` теперь создаёт concrete source
`Proj`, применяет charge/cooldown, рассчитывает `PrepareMaximumLife` и
сохраняет spring/hyper impulse в том же 60-Гц шаге. `stMine` до возврата из
AI update выполняет source track placement, `Player::InsertBonusProj`,
charge/cooldown и `GameCar::LockMine`.

Ordered queue хранит только backend view готовой транзакции. Последующий
`updateGameplay` добавляет `ProjectileRuntime`/`MineRuntime`, Jolt velocity
request, source events и shot effects без повторного `Player::Shot`.
Integration regression отдельно проверяет AI Hyper и Mine через два fixed
steps без render между ними, затем single materialization. Arm64 build,
29/29 CTest, physics и 360-frame Metal race smoke прошли.

Следующая B8r граница — убрать оставшуюся задержку Jolt-команд специальных
атак: source `Proj` уже создаётся вовремя, но velocity/runtime adapter пока
применяется после solver step следующего render frame.

### B8r: same-step backend bridge for AI specials

Задержка специальных атак устранена без смешивания частот source и backend.
Успешный Hyper fixed-step теперь сразу создаёт единственный attached
`ProjectileRuntime`, сохраняет spring lock и выдаёт адресную команду
изменения линейной скорости. Mine сразу создаёт единственный `MineRuntime` с
тем же concrete source `Proj` и network projectile id.

World callback расширен отдельным списком linear-velocity commands. Jolt
принимает его вместе с input/reset результатом `Race::OnFixedStep`, сначала
выполняет reset, затем изменение скорости и только после этого запускает
текущий solver update. Render adapter публикует события и эффекты, но больше
не создаёт второй runtime и не повторяет импульс.

Integration regression подтверждает runtime/velocity до adapter и отсутствие
повторной materialization. Physics regression подтверждает эффект команды в
том же solver interval. Следующая B8s граница — проверить обычные AI shots:
их source-транзакция уже fixed-step, но автономный backend projectile view
всё ещё создаётся последующим frame adapter.

### B8s: same-step runtime for ordinary AI projectiles

Обычная `ShotByEnemy` транзакция теперь создаёт persistent
`ProjectileRuntime` одновременно с concrete source `Proj`, charge и
cooldown. Единый builder читает состояние уже подготовленного объекта, а не
повторно выводит его из упрощённого weapon definition: точный projectile
index, transform, relative launch speed, ballistic/homing/attached route и
AI target сохраняются до следующего source шага.

Frame adapter повторно использует runtime по `sourceObject` и публикует
`WeaponFired`/shot effects без второго тела или `PrepareLaunch`. Ray-only
ветка остаётся presentation/contact boundary и не получает фиктивное
autonomous body. Session fallback пропускает только невозможный повторный
progress в кадре создания, сохраняя Windows order.

Regression расширен проверкой единственного торпедного runtime до и после
adapter и существующей 3D target-plane проверкой. Следующая B8t граница —
сопоставить движение и контакты свободных projectile runtime с
PhysX/Jolt fixed simulation: сейчас их backend-neutral интеграция всё ещё
выполняется render `updateGameplay`.

### B8t: Jolt lifecycle for free projectiles

Свободные persistent `Proj` больше не перемещаются умножением скорости на
render delta в активной macOS гонке. Session выдаёт Create/Synchronize/
Destroy команды со стабильным id, исходным collision box, mass, gravity и
velocity. Jolt создаёт dynamic sensor actor с linear CCD, применяет команды
из world fixed callback до solve и публикует завершённые pose/velocity для
обратной синхронизации runtime и renderer.

`Proj::OnProgress` и `ProgressFree` по-прежнему выполняют source lifetime,
homing и специальные Thunder/rocket rules; их изменения становятся
Synchronize-командой следующего physics interval. Headless session явно не
включает external projectile physics и сохраняет прежний source-only путь.

Regression проверяет single Create, body-id continuity, Jolt pose round-trip
и отсутствие duplicate actor. Physics regression требует same-step movement
тела, созданного непосредственно world fixed callback. Следующая B8u граница
— перенести contact ownership: sensor contacts с vehicle/decoration/world и
ray-only оружие должны приходить из Jolt query, а не из snapshot overlap в
session.

### B8u: Jolt sensor contacts for persistent projectiles

Projectile actor включён в существующий thread-safe contact listener. После
каждого solve его state содержит manifold contacts с `CollisionSurface`,
vehicle/decor instance, actor id, normal/speed и реальными contact points.
Listener не выдаёт этот sensor автомобилю как фиктивный контакт с плоскостью.

Активная session больше не использует OBB snapshot для свободного projectile:
по Jolt identity выбирается target машины или разрушаемая декорация, а
`ContactDynamic`/Sonar/damage/impact получают manifold point. Отсутствие
owner contact включает исходную возможность вернувшегося projectile попасть
в стрелявшего. Thunder использует border normal того же backend contact.
Source-only regressions без world сохраняют OBB fallback.

Physics smoke требует sensor contact с конкретной машиной и точкой, session
smoke — сохранение contact identity вместе с pose. Следующая B8v граница —
перевести ray-only Laser/FrostRay и rocket-height ray на backend narrow-phase
query, чтобы удалить оставшуюся CPU-копию world raycast активной гонки.

### B8v: live Jolt ray queries for projectile gameplay

Physics backend получил closest-hit query с двумя исходными группами:
полный projectile world для Laser/FrostRay и TrackPlane-only для MinePrepare/
RocketUpdate. Collector работает с текущими Jolt actors, возвращает surface,
vehicle/decor identity, actor id, hit point/normal/distance, пропускает тело
стрелка и собственные projectile sensors и поддерживает back-face triangles.

Session callback теперь обслуживает ray projectiles в `updateGameplay`,
обычный и AI mine placement, ballistic mine landing и rocket-height. При
пересоздании `physicsWorld` callback продолжает обращаться к актуальному
unique_ptr. CPU triangle/OBB raycaster вызывается лишь source-only smoke без
backend.

Physics regression требует корректные closest Vehicle, ignored Vehicle и
TrackPlane-only результаты. MineRip regression отдельно требует фактический
вызов backend callback. Следующий блок B8w — продолжить fixed-step аудит
оставшихся mines/bonus projectile actors: размещённые мины всё ещё хранят
position/velocity в session и используют OBB vehicle contact, а не Jolt body.

### B8w: Jolt actors for placed mines and MineRip fragments

Weapon Mine, AI same-step Mine, autonomous crater и MineRip model2/model3
children теперь имеют отдельные Jolt sensor actors. Body description берётся
из concrete source `Proj`, поэтому collision center/extents, mass, launch
velocity и gravity соответствуют фактически созданному объекту. Split child
сбрасывает parent id/contact cache перед Create.

Session читает completed pose/velocity/contact stream и маршрутизирует
Maslo, Crater, ordinary Mine, impulse и network mine request по реальному
vehicle manifold. Ручная position/gravity и OBB contact остаются только при
отсутствии external physics. После ground clamp Synchronize останавливает
velocity и gravity; lifetime/death/stale owner выдают Destroy.

AI regression требует единственный actor уже на firing fixed-step и не
допускает duplicate Create в adapter pass. Следующая B8x граница — map-owned
`AutoProj` bonuses/hazards: их source objects активны, но статические контакты
машин всё ещё вычисляются session OBB без Jolt identity.

### B8x: static Jolt sensors for map AutoProj bonuses

Каждый активный serialized Bonus MapObj при bind/reset создаёт static sensor
body из concrete `AutoProj::ProjDesc`. В отличие от свободных projectiles и
MineRip fragments он не получает dynamic motion/gravity, поэтому большой
набор pickups не добавляет непрерывную интеграцию и не сползает с исходного
map transform.

Body-id contact cache заменяет OBB loop для Money/Medpack/Ammunition/Shield,
SpeedArrow, Lusha, Maslo и map Mine. Source handlers и сетевые ветви не
изменены: им передаются реальный vehicle id и manifold point. Pickup/death
ставит Destroy до выключения `bonusActive`; новый reset строит roster заново.

Regression проверяет один actor у isolated oil AutoProj и bootstrap count до
AI Mine/ordinary projectile firing. Следующая B8y граница — source-аудит
оставшихся physics snapshot consumers (death plane, reset rays и direct
weapon Fire/Drobilka attached shapes), которые ещё не используют единый
backend actor/query stream.

## Очередь дальнейшего переноса

### P0 — offline game parity

1. Продолжить исходный menu/widget state machine с оставшимися реально
   вызываемыми `DialogMenu2`.
   GameMode/Tournament/Difficulty/Profile, основные offline subframes
   `RaceMenu2`, `FinishMenu`, `FinalMenu` и активная структура
   `OptionsMenu`/`StartOptionsMenu`, `MusicDialog`, workshop `WeaponDialog` и вызываемые offline
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
   `RaceMainFrame` ready/start warning/kick/leaver UI и раздельные
   `OnConnectionFailed`/host-disconnect/`OnFailed` dialog/exit branches и
   orderly `ExitRace`/`ExitMatch` RPC lifecycle уже
   связаны с active portable race. `NetPlayer` destruction также удаляет
   remote car из Jolt, renderer, HUD/mini-map, place и result state;
   следующий исходный разрыв — прочие legacy model/event listeners вне
   фактически подключённых branches.
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
