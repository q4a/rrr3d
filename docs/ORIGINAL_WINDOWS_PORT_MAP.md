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
| `DialogMenu2` | `originalmenu::DialogSystem` + GPU text caches | Source owner, active dialogs | Остался уже перенесённый отдельно UserChat и backend draw submission |
| `Environment` | `Race::environment` + `OriginalRaceRenderer` | Distributed | Перенести environment owner, weather/lamp progress и graph commands |
| `FinalMenu` | `mainmenu2::FinalMenuFrameState` + bgfx/CoreText view | Source owner, active frame | Legacy Widget API заменён backend draw/input/audio; credits, 107 s clock, slide alpha, Back и layout принадлежат source owner |
| `FinishMenu` | `originalracemenu::FinishMenuFrameState` + bgfx/CoreText view | Source owner, active frame | Legacy Widget API заменён backend draw/input; result order, timing, events и layout принадлежат source owner |
| `GameBase` | `OriginalGameObject`, behaviors, effects, motor sound state | Source owner, partial | Закрыть отсутствующие behavior subclasses и единый progress dispatch |
| `GameCar` | `source::GameCar` + Jolt vehicle adapter | Source owner, partial | Сравнить каждый PhysX callback/order и убрать session-owned car branches |
| `GameMode` | `source::GameModeState` + `GameModeRaceState` | Source owner, active race path | Startup/movie/config и часть menu/audio backend-команд ещё находятся в host |
| `GameObject` | `source::GameObject`, frame sync, listener/contact graph | Source owner, active core | Подключить оставшиеся fixed/frame callbacks и убрать backend-view ветви session |
| `HudMenu` | `OriginalRaceHud` | Distributed | Данные в отдельном владельце, но source Widget/Menu graph отсутствует |
| `HumanPlayer` | `source::HumanPlayer` | Source owner, partial | Подключить полный source input message path и event order |
| `Logic` | `source::Logic` + `WorldEventPump` progress registration | Source owner, active object/contact core | Убрать оставшиеся network authority и projectile backend-view loops из session |
| `MainMenu2` | `mainmenu2::{Controller,FrameController,ProfileFrameState,FinalMenuFrameState}` + `originalmenu::ScreenStack` | Source owner, active Main/Profile/Final path | Остались concrete network callbacks и backend draw submission |
| `Map` | `source::Map` | Source owner, partial | Завершить load/fix-up ownership и backend create/destroy commands |
| `MapObj` | `source::MapObj*` record/list hierarchy | Source owner, partial | Убрать importer/runtime mirrors и проверить все concrete object types |
| `Menu` | `originalmenu::MenuSystem` + source Main/Profile/Dialog/Options/Race/Finish/Final owners | Source owner, frame core | Завершить concrete network callbacks; bgfx/CoreText/SDL остаются backend boundary |
| `MenuSystem` | `originalmenu::{MenuSystem,ScreenStack,FrameState}` | Source owner, active menu path | Подключить concrete navigation graphs; bgfx остаётся draw executor |
| `OptionsMenu` | `originaloptions::{OptionsMenuState,StartOptionsMenuState}` + backend visuals | Source owner, active options path | Legacy Widget events заменены SDL input, CoreText и bgfx draw submission |
| `Player` | `source::Player`, `CarState`, behavior classes | Source owner, partial | Убрать оставшиеся session mirrors, проверить full event/listener order |
| `Race` | `OriginalRace`, `OriginalRaceSession`, lifecycle/place/tournament | Source owner, partial | Разложить 7 971-строчный источник по исходным владельцам вместо session |
| `RaceMenu2` | `originalracemenu::{RaceMenuState,RaceMainFrameState,GamersFrameState,GarageFrameState,WorkshopFrameState,SpaceshipFrameState,AngarFrameState,AchievementFrameState}` | Source owner, all six offline frame paths | Проверить оставшиеся network callbacks и оставить bgfx/CoreText backend boundary |
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

### B6 — Menu/MenuSystem и исходные frames (B6a–B6e.5 выполнены)

Перенести widget tree крупными экранами: common dialog/frame primitives,
MainMenu/Profile, Options, Planet/Garage/Workshop/Race и Finish/Final. Metal
renderer получает готовый source draw list и не решает focus/layout/state.

Результат B6a: добавлен backend-neutral `originalmenu::MenuSystem`, который
воспроизводит `Menu::SetState/ApplyState` и общую часть
`MenuFrame::Show/ShowModal/AdjustLayout/Invalidate/SetPos`. Он владеет
исходными уровнями topmost, modal ordering, 15-пиксельным ограничением frame
в viewport, visibility снимком `Main/Race/Hud/Finish/Info/Final`, флагами
loading/options/start-options и ревизиями сброса ввода.

Активный macOS path больше не владеет локальным `std::vector<MenuScreen>` и
отдельным selection: они находятся в source `ScreenStack`. Состояние frame
выводится из живого main/race/hud/finish/final пути, переходы вызывают
`ControlManager::ResetInput`, а Message/Accept/Loading/UserChat/Options
вход направляется через верхний modal frame. CoreText и bgfx сохраняются
только как backend текста и draw-команд. Отдельный smoke закрепляет исходный
visibility/reset порядок, modal precedence, hidden-layout gate, invalidate
и viewport clamp.

Результат B6b: перенесены common frames из `DialogMenu2.cpp` и управляющая
ими часть `Menu::OnProgress`: Accept, Weapon, Info/Message и Music. Новый
`originalmenu::DialogSystem` владеет исходными строками/state, yes/no result
и focus/hover, normal/max layout, button/frame scaling, modal lifetime,
message/weapon delay и пятисекундной Music popup-анимацией. `SetPos` идёт
через B6a `MenuSystem`; music использует исходную допускающую выход за край
экрана позицию без ошибочного clamp.

Активные Profile/garage/workshop/controls/exit dialogs, loading message и
menu/race music popup используют этот owner. Старые `*DialogVisual` теперь
содержат только CoreText/bgfx handles и не принимают игровых решений.
Отдельный regression проверяет обе раскладки Accept, result/hide, loading
Info, delayed Weapon и точные фазы Music popup.

Результат B6c: `mainmenu2::FrameController` перенёс общую часть
`MainMenu2::SetItems/AdjustMenuItems` и правила `MainFrame`, `GameModeFrame`,
`TournamentFrame`, `DifficultyFrame`: source item availability, круговой
Up/Down с пропуском disabled, первая доступная позиция, отдельный Back и
фиксированная раскладка. Исправлено расхождение Continue: оно включается
только при существующем сериализованном `lastProfile/lastNetProfile`, а не
при любом профиле.

`mainmenu2::ProfileFrameState` теперь владеет четырьмя видимыми строками,
scroll clamp, item/close/up/down/back focus graph, pointer focus, командами
select/delete/scroll/back и исходными координатами grid/arrows/back. Большой
host entry point только выполняет команды профиля и рисует полученное
состояние CoreText/bgfx. Отдельный regression закрепляет доступность,
wrap/disabled navigation, всю сетку ProfileFrame и layout.

Результат B6d: `originaloptions::OptionsMenuState` теперь владеет четырьмя
исходными вкладками Game/Media/Network/Controls, draft lifecycle
LoadCfg/ApplyChanges/CancelChanges, availability gates, всеми stepper и
volume mutations, control bindings, keyboard/gamepad column, tab mapping,
grid scroll и координатами. Исправлены два конкретных host-расхождения:
camera distance 1.00–2.00 и laps 1–8 снова циклически переходят через
границы, как `gui::StepperBox`, вместо clamp.

Отдельный `StartOptionsMenuState` владеет исходным `cPrefCameraEnd/Select`
sentinel, единственным camera-driven Apply gate, четырьмя catalog indices,
кольцом focus и ApplyChanges. Активный host выполняет только Cocoa window,
audio reload, persistence и bgfx/CoreText submission. Regression закрепляет
12/8/5/18 rows, network/difficulty gates, layout/scroll, wrap, draft
commit/cancel, bindings и весь first-run camera gate.

Результат B6e.1: `originalracemenu::RaceMenuState` перенёс точный
`RaceMenu::SetState/ApplyState`: last state, CarFrame для Main/Garage/
Workshop, SpaceshipFrame для Angar и взаимно исключающую видимость шести
concrete frames. `RaceMainFrameState` владеет семью командами, source
horizontal ring, client-ready gate и нижней раскладкой. Host больше не
интерпретирует индексы иконок как игровые команды.

`GamersFrameState` владеет achievement/network availability, выбором
текущего gamer id, previous/next без wrap, Next/Left/Right navigation graph,
shoulder bindings, confirm/select commands и исходной геометрией planet,
arrows и next. Исправлен отдельный state bug: Gamers теперь переводит общий
`MenuSystem` в Race, как дочерний frame `RaceMenu`, а не остаётся Main.

Результат B6e.2: `originalracemenu::GarageFrameState` теперь владеет точной
трёхчастной последовательностью available/secret/locked из
`GarageFrame::UpdateCarList`, campaign-фильтром secret cars, текущей машиной,
границами Prev/Next, source car-grid window и `OnShow(false)` lifetime.
Перенесён полный граф Back/Buy/arrows/14 colors, пропуск hidden/disabled
стрелок и занятых network colors, shoulder bindings и команды
selection/buy/back/repaint. Host оставляет за собой только профильную
транзакцию, сетевой запрос, CoreText и bgfx draw submission; прежние локальные
списки/индексы, ручной граф и два недостижимых альтернативных обработчика
удалены.

Результат B6e.3: `WorkshopFrameState` владеет source-фильтрацией и stable
cost sort товаров, 3×4 visible grid, scroll clamp/arrows, Back+10 slots
navigation graph, hidden/disabled control traversal, pointer-only goods,
drag payload/origin, Buy/Sell confirmation state и исходной геометрией goods,
slots, arrows и Back. Renderer больше не содержит собственных
`WorkshopDrag`, `WorkshopConfirmation`, goods vector или scroll index; он
исполняет возвращённые buy/install/sell/recharge/upgrade команды через уже
перенесённый `OriginalGarage` и рисует bgfx payload.

Исправлен конкретный host defect: Escape/Pause при mouse focus на товаре
раньше мог активировать товар и показать `svHintWeaponNotSupport` вместо
выхода. Source `Back` command теперь не зависит от hover focus.

Результат B6e.4a: `AngarFrameState` владеет списком и состояниями планет,
исходным выбором следующей планеты после победы, cyclic planet graph с
нулевыми Left/Right edges у Back, viewport-to-slot focus, close-info,
campaign/skirmish/network gates, Stay/Fly confirmation и четырьмя командами
Back/RequestTravel/ChangePlanet/CannotTravel. Перенесены 0.25 s door timing и
геометрия bottom panel, planet/slot cells, info/close и dialog anchor.
`SpaceshipFrameState` отдельно владеет непрерывным трёхсекундным clock красной
лампы и scene time. Восемь renderer-owned Angar переменных и ручной input
dispatch удалены; bgfx исполняет source commands и рисует готовое состояние.

Результат B6e.4b: `AchievementFrameState` владеет девятью source definitions,
state/price snapshot, исходным отсутствием focus после `OnShow`, точным
Back+9-card graph и рекурсивным disabled traversal из `Menu::NavElementFind`,
reverse pointer selection, Buy confirmation и layout. `armor4` по-прежнему
намеренно использует `musicTrack` artwork. Renderer-owned purchase flags,
pending index и упрощённый same-direction traversal удалены. Host применяет
только points/profile transaction и special armor4 presentation, а bgfx
рисует source entries/layout.

Результат B6e.5a: `FinishMenuFrameState` владеет полным упорядоченным
`Race::Results`, первыми тремя boxes, индивидуальными `voiceNameDur`,
0.15/0.5-секундной reveal-анимацией, alternating offsets, событиями
First/Second/Third и отдельным Last строго для `results.back()`, close input и
layout. Удалены renderer-owned clocks/voice indices и эвристика поиска
последнего по максимальному `place`; подписи/значения Money/Points снова
являются исходными двухстрочными labels на `y=154`.

Результат B6e.5b: `mainmenu2::FinalMenuFrameState` владеет разбором
`svCredits` на caption/body sections, точным 107-секундным clock, девятью
source slide intervals/alpha, Back input и исходной геометрией slides,
credits root и кнопки. Host больше не хранит собственный final clock и не
разбирает смысловые credit blocks: он создаёт CoreText line payload, запускает
неперсистентный `TrackFinal.ogg` с нуля и исполняет bgfx draw commands.
Исправлено отдельное visual-расхождение: aspect каждого slide вычисляется по
его собственному DDS, а не по первому изображению. Ускоренный renderer smoke
теперь ждёт фактического завершения фонового Ogg decode перед запуском
source timeline и проверяет все девять slides, scroll, Back, музыку и
автоматический возврат.

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
