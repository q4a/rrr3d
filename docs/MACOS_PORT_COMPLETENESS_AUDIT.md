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
| Vehicle simulation | PhysX 2.8.4 `NxWheelShape` | Jolt custom vehicle adapter | Частично | Нативная замена работает, но это semantic reimplementation; полная численная эквивалентность PhysX tire/suspension/solver не доказана |
| Car-to-track/car contacts | PhysX filters/reports | Jolt contacts → session | Частично | Основной damage path есть; все group/mask/callback/force branches исходного `Logic`/`GameObject` отсутствуют |
| Bonus/mine/crater contacts | `Proj::ComputeAABB` + `CreatePxBox` | после этой ревизии source AABB + OBB SAT | Перенесено | Удалены прежние сферы `3.5`; primary, `model2`, `model3` и death-projectile получают отдельные source boxes |
| Countdown/checkpoints/laps/place | `Race.cpp`, `Trace.cpp`, `Player.cpp` | `OriginalRaceSession.cpp` | Частично | Основная гонка работает; это ручной state machine, все special race modes/edge cases не сопоставлены |
| Reset/respawn | `Race`/`Player` | trace-based requests | Частично | Базовый путь есть; точное raycast/orientation/penalty поведение не полностью перенесено |
| AI | `AICar.cpp`, `AIPlayer.cpp` | steering/brake path по trace | Суррогат | Исходные AI classes, tactical state, avoidance, weapon selection и difficulty branches не компилируются |
| Weapons/projectiles | `Weapon.cpp`, `Player.cpp`, `Logic.cpp` | type-switch runtime в `OriginalRaceSession` | Частично | Основные типы визуально/функционально представлены, но collision, homing, forces, timing и contacts частично упрощены |
| Projectile effect selection | `DataBase.cpp`, serialized models/behaviors | таблица `weaponEffectTexture(type)` | Суррогат | Таблица и `bullet.dds` fallback не являются общим переносом исходного effect graph |
| Weapon sound selection | исходные sound behaviors/DataBase | `weaponSoundPath` по подстроке имени | Суррогат | Это эвристика, её нужно заменить чтением исходных sound records |
| Damage/support/shield | `GameObject`, `Player`, `Weapon`, behaviors | ручные расчёты session | Частично | Основные transitions есть; полная damage type/force/reflect/immortality матрица не перенесена |
| Bonuses | `Proj` types 4–10 | ручной switch + исходные values | Частично | Pickups/hazards есть; после ревизии shape contact source-driven, но остальной lifecycle ещё ручной |
| Destructible decorations | `DestrObj`, `GameBase` | life flags, fragments/effects | Частично | Visual pieces и часть debris есть; полный PhysX body/contact/death behavior отсутствует |
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

- projectile `impactDistance`/segment contacts вместо общего PhysX shape
  pipeline;
- установка mine позади автомобиля вместо исходного raycast на track shape и
  выравнивания по normal;
- trace-following AI вместо `AICar`/`AIPlayer`;
- часть фиксированных timing/force/visual branches для сложных weapons;
- ручные achievement counters.

### 3. Render/audio mapping

В `OriginalRace.cpp` остаются `weaponEffectTexture` и `weaponSoundPath`.
Первая выбирает текстуру по номеру projectile type, вторая — по подстроке
имени оружия. Это не общий исходный resource/behavior graph. Аналогично
fallback/direct material mappings требуют записи о происхождении для каждого
исключения.

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

Открытым остаётся исходное размещение mine raycast-ом по track shapes и общий
shape/contact pipeline для летящих projectile types.

## Очередь дальнейшего переноса

### P0 — offline game parity

1. Перенести исходный menu/widget state machine: `Menu`, `MenuSystem`,
   `MainMenu2`, `GameMode`, `DialogMenu2`, `RaceMenu2`, `OptionsMenu`,
   `FinishMenu`, `FinalMenu`, сохраняя bgfx/Metal только как backend.
2. Заменить projectile segment approximations и ручное mine placement на
   исходные type-specific shapes, track raycast, contact groups и callbacks.
3. Убрать `weaponEffectTexture`/`weaponSoundPath` heuristics; переносить
   model/effect/sound behaviors из `DataBase.cpp`, `Weapon.cpp` и records.
4. Разделить `OriginalRaceSession` по исходным обязанностям и последовательно
   перенести `GameObject`, `Logic`, `Player`, `Race`, `Weapon`.
5. Перенести `AICar`/`AIPlayer`: trace planning, avoidance, tactics,
   difficulty и weapon decisions.
6. Завершить tournament/profile/garage/finish flow и исходные UI transitions.

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
