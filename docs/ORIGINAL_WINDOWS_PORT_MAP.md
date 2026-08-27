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
| `CameraManager` | `source::CameraManager` | Source owner, race path | Ещё не перенесены FlyTo, AutoObserver и screen/ray utility; bgfx строит matrices |
| `ControlManager` | `originalcontrol::ControlManager` + `SdlInputManager` | Source owner, active input path | Mouse screen/ray messages и menu/widget listeners остаются в блоках View/Menu |
| `DataBase` | `OriginalRace`, `OriginalGarage`, `OriginalGameData` loaders | Distributed | Вернуть record libraries/fix-up ownership и единый object factory |
| `DialogMenu2` | Ручные frames в `main_bgfx_original_menu` | Distributed | Перенести widget state, focus/invalidate и dialog transitions |
| `Environment` | `Race::environment` + `OriginalRaceRenderer` | Distributed | Перенести environment owner, weather/lamp progress и graph commands |
| `FinalMenu` | Ручной `MenuScreen::Credits` | Distributed | Вернуть source frame lifecycle и command routing |
| `FinishMenu` | `OriginalRaceHud` + ручной Finish screen | Distributed | Вернуть последовательность result/final frames и закрытие |
| `GameBase` | `OriginalGameObject`, behaviors, effects, motor sound state | Source owner, partial | Закрыть отсутствующие behavior subclasses и единый progress dispatch |
| `GameCar` | `source::GameCar` + Jolt vehicle adapter | Source owner, partial | Сравнить каждый PhysX callback/order и убрать session-owned car branches |
| `GameMode` | `source::GameModeState` + `GameModeRaceState` | Source owner, active race path | Startup/movie/config и часть menu/audio backend-команд ещё находятся в host |
| `GameObject` | `source::GameObject`, frame sync, listener/contact graph | Source owner, active core | Подключить оставшиеся fixed/frame callbacks и убрать backend-view ветви session |
| `HudMenu` | `OriginalRaceHud` | Distributed | Данные в отдельном владельце, но source Widget/Menu graph отсутствует |
| `HumanPlayer` | `source::HumanPlayer` | Source owner, partial | Подключить полный source input message path и event order |
| `Logic` | `source::Logic` + `WorldEventPump` progress registration | Source owner, active object/contact core | Убрать оставшиеся network authority и projectile backend-view loops из session |
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
| `ResourceManager` | `OriginalResourceManager` + native readers/uploaders | Source owner, graph/sound path | Mesh/image/sound identity и lifetime общие; font/material-library ownership ещё нужно завершить |
| `RockCar` | `source::RockCar`, event sink | Source owner, partial | Проверить attachment/listener lifetime вместе с Player/GameCar |
| `Trace` | `source::{Trace,WayPath,WayNode,WayPoint}` | Source owner, strong | Осталась graph/debug visualization boundary |
| `TraceGfx` | `OriginalRaceRenderer` debug draw | Backend boundary | Перенести source trace visual state, оставить bgfx submission |
| `View` | SDL window/input + bgfx device | Backend boundary | Перенести source view policy: reset/display/input coordinate lifecycle |
| `Weapon` | `source::{Weapon,Proj,AutoProj,WeaponItem...}` | Source owner, active contact core | Полностью свернуть type-specific progress backend-view loops в Proj adapter |
| `World` | `source::WorldEventPump` + native `WorldHost` | Source owner, event core | Подключить к спискам все race objects/environment/network adapters вместо оставшихся session loops |

## Очередь крупных блоков

### B1 — CameraManager ownership (выполнено)

Перенести backend-neutral состояние и race-ветви
`CameraManager::Control::OnInputFrame`: ThirdPerson, Isometric, debug styles,
velocity filtering, style transitions, lead smoothing и respawn/teleport
compensation. `OriginalRaceRenderer` оставляет только bgfx view/projection.

Результат: отдельный `source::CameraManager` входит в `Rock3dGame`, имеет
deterministic regression и используется активным Metal race path. Из
renderer удалена inline race-camera state machine. Оставшаяся часть
исходного файла (FlyTo, AutoObserver и screen/ray utility) сохранена в строке
карты как отдельный следующий camera block, а не объявлена готовой.

### B2 — ControlManager action dispatcher (выполнено)

Объединить уже перенесённые VirtualKey tables и SDL device snapshot с
исходными `GetGameAction*`, normalization, ordered event list и focus reset.
Меню и HumanPlayer должны получать source input messages, а не отдельные
ручные switch-блоки.

Результат: добавлен самостоятельный `originalcontrol::ControlManager` с
точным порядком 25 `GameAction`, `GetGameAction*`, исходной signed
dead-zone/trigger normalization, polled raw/action state, ordered
`ControlEvent` dispatch и focus/device reset. `SdlInputManager` больше не
хранит собственные action maps и не вычисляет нормализованные значения:
он переводит SDL scancode/button/axis в source `VirtualKey` и передаёт raw
диапазоны новому владельцу. Активный `HumanPlayer` polling path получает
состояние от него; portable menu/debug actions сохранены как расширение вне
исходной таблицы. Отдельные mouse screen/ray messages и подключение исходных
widget listeners остаются явно открыты в блоках View/Menu, а не считаются
частью выполненного device/action блока.

### B3 — World/GameMode event pump (ядро выполнено)

Вернуть владельцев fixed/progress/late/frame lists, pause/input reset,
start/exit match, start/exit race, loading/countdown/finish timers и movie/
music transitions. SDL loop остаётся platform host, но перестаёт владеть
правилами игры.

Результат B3a: добавлены самостоятельные `source::WorldEventPump` и
`source::GameModeState`. `WorldEventPump` владеет исходными ordered
fixed/progress/late/frame lists, удалением отложенно снятых progress users,
pause gate и точным порядком environment/network/control/GameMode.
`GameModeState` владеет start/exit match, двухкадровым loading gate,
start/exit race, паузой эффектов, ordered user events и finish-close
командами. Исходные countdown/finish clocks перенесены из общего
`OriginalRaceLifecycle` к этому же владельцу. Активный SDL/bgfx цикл теперь
получает `DoStartRace` через `World → GameMode`, а не через локальные счётчики.

Открытая B3b: startup/shutdown и movie sequence, загрузка/сохранение общего
GameMode config, а также исполнение всех music/commentator команд пока остаются
в platform host. Кроме того, в world lists ещё не зарегистрированы все
race-local concrete objects — это пересекается с центральным dispatch B4.

### B4 — central GameObject/Logic/Proj dispatch (B4a выполнено)

Убрать оставшиеся параллельные projectile/mine/contact loops из
`OriginalRaceSession`. Concrete `GameObject`/`Proj` должны сами исполнять
исходный virtual progress/contact graph, возвращая Jolt/audio/render
commands только на backend boundary.

Результат B4a: `Logic` снова подключён к `WorldEventPump` как исходный
`WorldHost`, а `PairPxContactEffect` — как ordered `ProgressEvent` после
`Logic::OnProgress`. Jolt contact manifolds теперь импортируются до этого
progress-прохода, как PhysX callbacks в Windows; команды освобождения
эффектов забираются после него. Ручной вызов contact timer из середины
`OriginalRaceSession::updateGameplay` удалён.

В `Proj` возвращён первый этап исходного `OnContact`: базовый
`GameObject::OnContact` выполняется до live-state guard и type switch. Этот
путь подключён для car/decor projectile contacts, mines и map bonuses; если
listener (например `TouchDeath`) уничтожил target, конкретный обработчик и
damage подавляются исходным владельцем.

Открытая B4b: `projectiles_`/`mines_` пока остаются необходимыми
Jolt/render views, а часть type-specific ray/movement queries всё ещё
находится в session adapter. Их нужно свести к входным physics snapshots и
выходным командам `Proj`, не удаляя массивы представления до появления
эквивалента PhysX actor ownership.

### B5 — DataBase/RecordLib/ResourceManager (B5a–B5c выполнены)

Вернуть единый typed record graph, source/proxy load, fix-up names, concrete
object factory и resource identity. Текущие проверенные XML/R3D parsers
становятся backend readers, а не владельцами игровых объектов.

Результат B5a: устранён разрыв между уже перенесённой иерархией
`MapObjRecordLibrary` и активной загрузкой гонки. `MapObjRecord` теперь, как
Windows `MapObjRec`, хранит source-loader; `MapObj::SetRecordProxy`
синхронно загружает source-часть записи, после чего `.r3dMap` накладывает
только proxy transform/life/name. `OriginalRaceSession` заранее регистрирует
в семи библиотеках определения `ctDecoration`, `ctTrack`, `ctBonus` и
`ctCar`; ручная повторная сборка destructible fragments, базовой жизни и
`AutoProj` bonus description из session удалена. Таким образом parser
остаётся reader, `DataBase`-каталог снова является фабрикой concrete
gameplay object.

Результат B5b: добавлен единый `OriginalResourceManager`, который возвращает
стабильную identity по каноническому физическому пути и владеет decoded
`R3DMeshAsset`, bgfx mesh и texture до общего shutdown. Это source-аналог
`ComplexMesh::GetOrCreateMesh/GetOrCreateIVBMesh` и
`ComplexImage::GetOrCreateTex2d/GetOrCreateCubeTex`; D3D9 не переносился,
upload остаётся границей bgfx/Metal. Гонка, гараж, ангар, мастерская и HUD
больше не создают отдельные копии одинаковых mesh/image. DDS cube сохраняет
container identity и не подменяется 2D-декодированием. В полном 360-frame
Metal smoke каталог создал 116 уникальных мешей и 219 текстур и повторно
использовал 847 из 1182 запросов; resize и повторная инициализация 3D frames
прошли без двойного освобождения.

Результат B5c: реализована source `SoundLib`-часть того же manager. Как в
Windows `ResourceManager::LoadSound -> SoundLib::Find`, короткий/зацикленный
OGG теперь сначала ищется по стабильному canonical path и декодируется SDL
backend только один раз. Menu SoundSheme, commentator и motor/wheel/contact/
weapon/effect paths используют заимствованные handles и больше не выгружают
общий sound из локального владельца. Общий shutdown освобождает SoundLib до
остановки audio backend. 360-frame Metal smoke получил 76 уникальных sounds
на 105 запросов (29 повторных), сохранил правильное завершение всех loop
voices и прошёл без висячего звука в меню.

`MusicCat` намеренно остаётся отдельным потоковым владельцем: он асинхронно
декодирует только текущий/следующий track и выгружает старый, тогда как
Windows-каталог `LoadMusic` был декларацией имён. Принудительное помещение
всех tracks в PCM SoundLib вернуло бы задержки и расход памяти, устранённые в
Milestone 8.

Открытая B5d: вернуть source identity для font descriptors и material-library
descriptors/samplers. Их backend payload останется CoreText и bgfx/Metal.

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
