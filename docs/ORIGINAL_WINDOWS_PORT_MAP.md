# Карта переноса оригинального Windows-кода

Дата среза: 2026-08-27

Ветка: `macos-arm64`

Reference: `eff933868c1fbdfd266738a403fac80084f2b51e:prog`

## Как читать карту

Это карта владельцев кода, а не перечень похожих имён. Для каждого из 34
исходных `Rock3dGame/source/game/*.cpp` проверено, какой класс реально входит
в arm64 target и кто сейчас владеет состоянием исходного объекта.

Статусы:

- **Source owner** — в `Rock3dGame` уже существует отдельный backend-neutral
  класс с исходным состоянием и значительной частью исходного call graph.
- **Distributed** — правила встречаются в порте, но разбросаны между
  `OriginalRaceSession`, renderer и 22k-line entry point; исходного владельца
  пока нет.
- **Backend boundary** — исходная platform policy отделена не полностью от
  SDL/bgfx/Jolt/AVFoundation adapter.
- **Absent** — пользовательская функция или исходный публичный объект не
  выполняется в arm64 target.

Наличие статуса **Source owner** не означает побитовый или полный паритет.
Оно означает, что дальнейшее сравнение можно вести метод к методу внутри
правильного класса, а не искать поведение в renderer/session.

## Проверяемая граница сборки

- В reference находятся 34 game `.cpp`, 51 751 строка и примерно 3 928
  scoped definitions.
- В фактической arm64-сборке не компилируется ни один из этих 34 `.cpp`.
- Активный non-Windows `Rock3dGame` собирает 25 `Original*.cpp` и несколько
  portable adapters; UI/renderer/audio добавляют ещё восемь `Original*.cpp`.
- Поэтому оставшаяся работа — не «дописать несколько TODO», а постепенно
  вернуть исходных владельцев состояния, сохранив четыре разрешённые
  платформенные границы: D3D9→bgfx/Metal, PhysX→Jolt, XAudio→SDL/CoreAudio и
  DirectShow→AVFoundation.

## Translation-unit map

| Исходный `.cpp` | Активный macOS-владелец | Статус | Оставшаяся граница |
| --- | --- | --- | --- |
| `AICar` | `source::AICar::{PathState,ControlState,AttackState}` | Source owner, partial | Свести session adapter к входному physics snapshot и выходной команде |
| `AIPlayer` | `source::AIPlayer`, `source::AISystem` | Source owner, partial | Проверить полный порядок `OnProgress`, сетевые ветви и debug ownership |
| `AchievmentModel` | `source::AchievmentModel` | Source owner, partial | Закрыть все event/condition subclasses и persistence order |
| `CameraManager` | Формулы внутри `OriginalRaceRenderer` | Distributed | Вернуть отдельный `CameraManager`; renderer должен только строить matrices |
| `ControlManager` | `OriginalControlBindings` + `SdlInputManager` + main event switch | Backend boundary | Вернуть source action dispatcher/event list поверх SDL device state |
| `DataBase` | `OriginalRace`, `OriginalGarage`, `OriginalGameData` loaders | Distributed | Вернуть record libraries/fix-up ownership и единый object factory |
| `DialogMenu2` | Ручные frames в `main_bgfx_original_menu` | Distributed | Перенести widget state, focus/invalidate и dialog transitions |
| `Environment` | `Race::environment` + `OriginalRaceRenderer` | Distributed | Перенести environment owner, weather/lamp progress и graph commands |
| `FinalMenu` | Ручной `MenuScreen::Credits` | Distributed | Вернуть source frame lifecycle и command routing |
| `FinishMenu` | `OriginalRaceHud` + ручной Finish screen | Distributed | Вернуть последовательность result/final frames и закрытие |
| `GameBase` | `OriginalGameObject`, behaviors, effects, motor sound state | Source owner, partial | Закрыть отсутствующие behavior subclasses и единый progress dispatch |
| `GameCar` | `source::GameCar` + Jolt vehicle adapter | Source owner, partial | Сравнить каждый PhysX callback/order и убрать session-owned car branches |
| `GameMode` | main loop + MusicCat/commentator + session states | Distributed | Вернуть source game-mode event pump, timers и transition ownership |
| `GameObject` | `source::GameObject`, frame sync, listener graph | Source owner, partial | Перенести virtual progress/fixed/frame dispatch без session обходов |
| `HudMenu` | `OriginalRaceHud` | Distributed | Данные в отдельном владельце, но source Widget/Menu graph отсутствует |
| `HumanPlayer` | `source::HumanPlayer` | Source owner, partial | Подключить полный source input message path и event order |
| `Logic` | `source::Logic` | Source owner, partial | Перенести central object/contact/network dispatch, убрать parallel loops |
| `MainMenu2` | `OriginalMainMenu::Controller` + main screen stack | Distributed | Вернуть source frame tree, profile/network callbacks и invalidation |
| `Map` | `source::Map` | Source owner, partial | Завершить load/fix-up ownership и backend create/destroy commands |
| `MapObj` | `source::MapObj*` record/list hierarchy | Source owner, partial | Убрать importer/runtime mirrors и проверить все concrete object types |
| `Menu` | Ручной screen stack/render code | Distributed | Самый крупный GUI block: source widgets, layouts, focus, animation |
| `MenuSystem` | Ручные input/layout helpers | Distributed | Вернуть root event routing и frame ownership |
| `OptionsMenu` | Ручные option pages + `OriginalProfile` | Distributed | Вернуть source controls, apply/reset/autodetect transitions |
| `Player` | `source::Player`, `CarState`, behavior classes | Source owner, partial | Убрать оставшиеся session mirrors, проверить full event/listener order |
| `Race` | `OriginalRace`, `OriginalRaceSession`, lifecycle/place/tournament | Source owner, partial | Разложить 7 971-строчный источник по исходным владельцам вместо session |
| `RaceMenu2` | Ручные garage/workshop/race frames | Distributed | Вернуть source car/weapon frame graph и command transitions |
| `RecordLib` | Набор XML/R3D import helpers | Distributed | Вернуть typed record library, proxy/source load и fix-up pass |
| `ResourceManager` | `ResourceFileSystem`, `R3DMeshAsset`, per-system caches | Backend boundary | Вернуть source resource identity/cache/factory поверх native loaders |
| `RockCar` | `source::RockCar`, event sink | Source owner, partial | Проверить attachment/listener lifetime вместе с Player/GameCar |
| `Trace` | `source::{Trace,WayPath,WayNode,WayPoint}` | Source owner, strong | Осталась graph/debug visualization boundary |
| `TraceGfx` | `OriginalRaceRenderer` debug draw | Backend boundary | Перенести source trace visual state, оставить bgfx submission |
| `View` | SDL window/input + bgfx device | Backend boundary | Перенести source view policy: reset/display/input coordinate lifecycle |
| `Weapon` | `source::{Weapon,Proj,AutoProj,WeaponItem...}` | Source owner, partial | Вернуть центральные virtual contact/progress callbacks; убрать session dispatch |
| `World` | `main_bgfx_original_menu` + `PortableGame::CreateWorld` exception | Absent/Distributed | Вернуть публичный world/game event graph поверх native backends |

## Очередь крупных блоков

### B1 — CameraManager ownership (в работе)

Перенести backend-neutral состояние и race-ветви
`CameraManager::Control::OnInputFrame`: ThirdPerson, Isometric, debug styles,
velocity filtering, style transitions, lead smoothing и respawn/teleport
compensation. `OriginalRaceRenderer` оставляет только bgfx view/projection.

Критерий закрытия: отдельный `source::CameraManager` входит в
`Rock3dGame`, имеет deterministic regression и используется активным Metal
race path.

### B2 — ControlManager action dispatcher

Объединить уже перенесённые VirtualKey tables и SDL device snapshot с
исходными `GetGameAction*`, normalization, ordered event list и focus reset.
Меню и HumanPlayer должны получать source input messages, а не отдельные
ручные switch-блоки.

### B3 — World/GameMode event pump

Вернуть владельцев fixed/progress/late/frame lists, pause/input reset,
start/exit match, start/exit race, loading/countdown/finish timers и movie/
music transitions. SDL loop остаётся platform host, но перестаёт владеть
правилами игры.

### B4 — central GameObject/Logic/Proj dispatch

Убрать оставшиеся параллельные projectile/mine/contact loops из
`OriginalRaceSession`. Concrete `GameObject`/`Proj` должны сами исполнять
исходный virtual progress/contact graph, возвращая Jolt/audio/render
commands только на backend boundary.

### B5 — DataBase/RecordLib/ResourceManager

Вернуть единый typed record graph, source/proxy load, fix-up names, concrete
object factory и resource identity. Текущие проверенные XML/R3D parsers
становятся backend readers, а не владельцами игровых объектов.

### B6 — Menu/MenuSystem и исходные frames

Перенести widget tree крупными экранами: common dialog/frame primitives,
MainMenu/Profile, Options, Planet/Garage/Workshop/Race и Finish/Final. Metal
renderer получает готовый source draw list и не решает focus/layout/state.

### B7 — Environment/TraceGfx/render policy

Вынести из 6.9k-line renderer исходные environment progress, lamp/weather,
trace visibility, material/pass selection и effect lifetimes. bgfx остаётся
исполнителем draw/pass commands.

### B8 — завершение Race/Player/AI/GameCar/Weapon parity

После возврата общих владельцев повторить метод-к-методу аудит оставшихся
partial классов. На этом этапе session должен стать orchestration adapter,
а не второй реализацией игры.

## Правило обновления карты

Каждый крупный block commit обязан:

1. назвать точные методы из `eff9338:prog`, которые стали активными;
2. указать новый source owner и оставшийся backend boundary;
3. добавить regression на source state/order;
4. обновить строку таблицы и статус блока;
5. пройти arm64 build, offline CTest, network CTest, physics smoke и Metal
   race-render smoke, если block касается активной гонки.
