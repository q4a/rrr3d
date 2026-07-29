# Ревизия полноты порта Motor Rock на macOS

Дата ревизии: 2026-07-29

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
- `RRR3D_ENABLE_VIDEO=OFF`;
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
| Keyboard/mouse/gamepad | XInput/Win32 `ControlManager` | SDL3 action adapter | Частично | Actions и hot-plug есть; исходный `ControlManager.cpp`, переназначение всех legacy commands и UI их настройки не перенесены целиком |
| Audio device/mixer | XAudio2/X3DAudio | SDL3/CoreAudio | Замена платформы | Backend полноценный, но весь исходный game-side `Audio.cpp` object graph не перенесён |
| MusicCat/menu music | `MusicCat`, три Ogg | background decode, shuffle, next, pause/state | Перенесено | Поведение покрыто отдельным smoke |
| Spatial race audio | X3DAudio game integration | ручные attenuation/pan/pitch voices | Частично | Основные car/race sounds есть; исходные emitters/listeners, все lifetime/priority rules и все sound behaviors не перенесены |
| Главное меню, внешний вид | `MainMenu2.cpp` | часть оригинальных изображений/строк | Частично | Фон, панели и selection source-driven; полный widget tree, animation, layout и event code не перенесены |
| Навигация меню | `Menu`, `MenuSystem`, `MainMenu2`, `GameMode` | ручной `enum MenuScreen` и `createPage(...)` в одном `main` | Суррогат | Страницы GameMode/Tournament/Profile/Options/Credits создаются как универсальные текстовые списки |
| Dialog/Profile UI | `DialogMenu2.cpp`, `MainMenu2.cpp` | универсальная page + portable profile operations | Суррогат | Исходные dialogs, text input, transitions, animations и подтверждения отсутствуют |
| Race menu | `RaceMenu2.cpp` | список `Start race/Workshop/Garage/...` | Суррогат | Исходный RaceMenu widget graph и его режимы не перенесены |
| Options UI | `OptionsMenu.cpp` | generic pages | Суррогат | Часть значений сохраняется, но исходные controls/layout/apply semantics отсутствуют |
| Finish/final UI | `FinishMenu.cpp`, `FinalMenu.cpp` | generic finish page | Суррогат | Исходные panels, statistics, awards, credits/final flow не перенесены |
| Profile serialization | исходный profile/config code | `OriginalProfile.cpp`, user XML | Частично | Перенесены нужные поля tournament/workshop/options; полная схема, migration и все profile branches не доказаны |
| Tournament/progression | `GameMode.cpp`, `Race.cpp`, menus | parser `tournamet.xml` + ручное advance | Частично | Основной выбор/rewards есть; полный state machine, dialogs, unlock/final sequences не перенесён |
| Garage/workshop data | `RaceMenu2`, `DataBase`, `garage.xml`, `workshop.xml` | `OriginalGarage.cpp` | Частично | Каталог, slots, buy/install/recharge есть; исходный 3D UI, preview behavior и все restrictions/animations не перенесены |
| Map/catalog loading | `Map`, `MapObj`, `DataBase` | `OriginalRace.cpp` | Частично | 88 записей и исходные placements читаются; generic GameObject/behavior/include lifecycle воспроизведён только для известных типов |
| Track collision | PhysX triangle meshes | Jolt triangle meshes из исходных shapes | Перенесено | Используемый race path получает исходные triangles/material groups |
| Vehicle descriptions | `DataBase::CarDesc`, `RockCar` | XML/source constants → `VehicleDescription` | Частично | Mass, body, wheels, motor/gears/suspension перенесены; весь `RockCar`/PhysX state и contact callbacks не перенесены |
| Vehicle simulation | PhysX 2.8.4 `NxWheelShape` | Jolt custom vehicle adapter | Частично | Нативная замена работает; `GameCar::LockSpring` теперь подавляет airborne pitch как в source, но полная численная эквивалентность PhysX tire/suspension/solver не доказана |
| Car-to-track/car contacts | PhysX filters/reports | Jolt contacts → session | Частично | Основной damage path есть; все group/mask/callback/force branches исходного `Logic`/`GameObject` отсутствуют |
| Bonus/mine/crater contacts | `Proj::ComputeAABB` + `CreatePxBox` | после этой ревизии source AABB + OBB SAT | Перенесено | Удалены прежние сферы `3.5`; primary, `model2`, `model3` и death-projectile получают отдельные source boxes |
| Mine placement | `Proj::MinePrepare` PhysX track raycast | source triangle raycast в `OriginalRaceSession` | Перенесено | Используются serialized `proj.pos`, ray `+2/-Z`, только `TrackPlane`, `max(-AABB.min.z, 0.01)`, hit normal; miss не расходует заряд |
| Countdown/checkpoints/laps/place | `Race.cpp`, `Trace.cpp`, `Player.cpp` | `OriginalRaceSession.cpp` | Частично | Основная гонка работает; это ручной state machine, все special race modes/edge cases не сопоставлены |
| Reset/respawn | `Race`/`Player` | trace-based requests | Частично | Базовый путь есть; точное raycast/orientation/penalty поведение не полностью перенесено |
| AI | `AICar.cpp`, `AIPlayer.cpp` | steering/brake path по trace | Суррогат | Исходные AI classes, tactical state, avoidance, weapon selection и difficulty branches не компилируются |
| Weapons/projectiles | `Weapon.cpp`, `Player.cpp`, `Logic.cpp` | type-switch runtime в `OriginalRaceSession` | Частично | Source boxes/ray, homing, contacts, `ptHyper` и `ptSpring` перенесены; остальные type-specific forces, timing, groups и callbacks ещё частичны |
| Weapon shot effects | `Weapon::CreateShot`, `ShotEffect`, serialized `ctWeapon` behaviors | `mapObj` → behavior type 10 → source effect graph | Перенесено | Effect record, local position, ignore-rotation и effective nested lifetime читаются из `db.xml`; отдельный `WeaponShotEffect` создаётся один раз для каждого созданного projectile |
| Weapon shot sounds | `ShotEffect::GiveSource3d`, serialized sound refs | source `ctWeapon/behaviors/items/*[@type=10]/sounds` | Перенесено | Удалено угадывание по имени; 24 source refs читаются напрямую, `drobilka` корректно остаётся без придуманного звука |
| Damage/support/shield | `GameObject`, `Player`, `Weapon`, behaviors | ручные расчёты session | Частично | Основные transitions есть; полная damage type/force/reflect/immortality матрица не перенесена |
| Bonuses | `Proj` types 4–10 | ручной switch + исходные values | Частично | Pickups/hazards есть; после ревизии shape contact source-driven, но остальной lifecycle ещё ручной |
| Destructible decorations | `DestrObj`, `GameBase` | life flags, source fragments/debris и collision meshes | Частично | Все map destructibles обязаны иметь serialized `destrList` и source collider; OBB–triangle contact заменил proximity sphere, `explosion2.dds` fallback удалён; полный PhysX body/death lifecycle ещё отсутствует |
| Achievements | `AchievmentModel.cpp` | definitions + ручные counters | Частично | Часть условий поддержана; исходный model/event coverage не перенесён полностью |
| HUD | `HudMenu.cpp` | `OriginalRaceHud.cpp` с исходными images/strings | Частично | Основные indicators, notifications и mini-map есть; исходный widget/animation object graph и все состояния не компилируются |
| Mini-map | `HudMenu`, `TraceGfx` | trace-derived bgfx geometry | Частично | Работает по source trace; exact clipping/transforms/all markers требуют дальнейшего сопоставления |
| Camera | `CameraManager.cpp`, `View.cpp` | source-derived formulas в renderer | Частично | Два режима есть; исходный manager, collision/culling transitions и все modes не перенесены |
| Scene graph/render queues | `GraphManager`, `Actor`, `SceneManager` | custom queues в `OriginalRaceRenderer` | Частично | Основные order buckets есть; generic actor/proxy/octree graph не перенесён |
| Materials | `MaterialLibrary`, `MappingShaders`, `DataBase` | parsed records + ручные mappings | Частично | Opaque/alpha/additive/bump/reflection реализованы; direct-name heuristics/fallback mappings остаются |
| Lighting/shadows/HDR | D3D9 graph effects | bgfx/Metal passes | Частично | Реализованы выбранные passes; bit-for-bit и полное graph state parity не доказаны |
| Particles/effects/trails | `FxManager`, effect records | portable emitter/trail renderer | Частично | Значимая часть serialized graph читается; не все node/emitter/action types и lifetime semantics перенесены |
| Weather/water/magma/sky | `Environment.cpp`, graph effects | source records + bgfx passes | Частично | Все world variants загружаются; exact D3D shader/fixed-pipeline result не доказан |
| Commentator | race/HUD sound events | `OriginalRaceCommentator.cpp` | Частично | Оригинальные clips используются; очередь и trigger selection написаны заново |
| Intro/video | `VideoPlayer.cpp`, DirectShow playback | выключено | Не перенесено | Видеозаставки и video UI отсутствуют |
| LAN/network | `NetGame`, `NetRace`, `NetPlayer`, NetLib | выключено | Не перенесено | Offline acceptance не требует сеть, но это часть Windows-продукта |
| Steam | `SteamService`, auth | выключено | Не перенесено | Не относится к offline race, но не должно называться перенесённым |
| Editor | `source/edit`, MapEditor | не входит в `.app` | Не переносился | Редактор не является обязательной частью пользовательской игры |

## Активные суррогаты и заглушки

### 1. Меню

`main_bgfx_original_menu.cpp` вручную объявляет `MenuScreen` и создаёт
универсальные страницы через `createPage`. Это главный источник визуального и
поведенческого несоответствия Windows-версии. `OriginalMainMenu.cpp` загружает
ресурсы и строки, но не переносит классы `MainMenu2`, `MenuSystem`,
`GameMode`, `RaceMenu2`, `OptionsMenu`, `FinishMenu` и `FinalMenu`.

### 2. Игровая логика

`OriginalRaceSession.cpp` объединяет обязанности исходных `Race`, `Player`,
`AIPlayer`, `AICar`, `Weapon`, `Logic`, `GameObject` и achievements в один
ручной state machine. Он уже существенно source-driven, но любое реализованное
им поведение должно считаться частичным до сопоставления каждой ветви с
Windows-кодом.

Оставшиеся явные приближения:

- type-specific projectile contact groups/callbacks ещё не полностью заменяют
  исходный PhysX dispatch;
- trace-following AI вместо `AICar`/`AIPlayer`;
- часть фиксированных timing/force/visual branches для сложных weapons;
- ручные achievement counters.

### 3. Render/audio mapping

Эвристики `weaponEffectTexture` и `weaponSoundPath` удалены. Workshop
`mapObj` теперь ведёт к исходной записи `ctWeapon`; из behavior type `10`
переносятся effect record, local position, ignore-rotation, nested lifetime и
sound refs. Если source behavior или visual отсутствует (`drobilka`, support
`droid`/`reflector`), порт больше не создаёт fallback-вспышку, луч, сферу или
звук.

Оставшиеся fallback/direct material mappings всё ещё требуют записи о
происхождении для каждого исключения.

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
7. Smoke проверяет serialized records `sonar`/`rezonator`/`rocketLauncher`,
   source border reflection, relative speed, time-based lifetime после
   прохождения `maxDist`, actor rotation и TrackPlane clearance.

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

## Очередь дальнейшего переноса

### P0 — offline game parity

1. Перенести исходный menu/widget state machine: `Menu`, `MenuSystem`,
   `MainMenu2`, `GameMode`, `DialogMenu2`, `RaceMenu2`, `OptionsMenu`,
   `FinishMenu`, `FinalMenu`, сохраняя bgfx/Metal только как backend.
2. Завершить исходные type-specific projectile contact groups, forces,
   callbacks и lifetime transitions поверх уже перенесённых shapes/raycasts.
3. Разделить `OriginalRaceSession` по исходным обязанностям и последовательно
   перенести `GameObject`, `Logic`, `Player`, `Race`, `Weapon`.
4. Перенести `AICar`/`AIPlayer`: trace planning, avoidance, tactics,
   difficulty и weapon decisions.
5. Завершить tournament/profile/garage/finish flow и исходные UI transitions.

### P1 — visual/audio parity

1. Закрыть все material mapping fallbacks provenance-тестами.
2. Сопоставить все graph node, particle и effect behavior types.
3. Перенести оставшиеся HUD states, camera transitions и culling semantics.
4. Сопоставить все source sound behaviors и event triggers.
5. Провести покадровое сравнение каждой world/weather/car комбинации после
   переноса логики, а не использовать сравнение как замену переносу.

### P2 — полная продуктовая функциональность

1. Video/intros.
2. LAN/network.
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
```
