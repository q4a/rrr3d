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
| `AICar` | `source::AICar::{PathState,AttackState,ControlState,ProgressResult}` | Source owner, active frame | Jolt/weapon snapshot и исполнение готовых move/shot команд остаются backend adapter |
| `AIPlayer` | `source::AIPlayer`, `source::AISystem` | Source owner, active frame | Сетевой authority filter и AIDebug Metal/text submission остаются host boundary |
| `AchievmentModel` | `source::AchievmentModel` | Source owner, active condition/reward path | XML чтение/запись остаётся profile adapter; source owner владеет всеми 9 conditions, reward states, покупкой и garage/gamer gates |
| `CameraManager` | `source::{CameraManager,AutoObserver}` | Source owner, active race/presentation path | FlyTo, AutoObserver и screen/ray policy source-owned; bgfx строит только matrices, SDL переводит pointer events |
| `ControlManager` | `originalcontrol::ControlManager` + `SdlInputManager` | Source owner, active input path | Mouse screen/ray messages и menu/widget listeners остаются в блоках View/Menu |
| `DataBase` | `source::DataBase` + `OriginalRace` reader | Source owner, active record path | Полный graph/material/physics catalog остаётся разделённым по разрешённым bgfx/Jolt adapters |
| `DialogMenu2` | `originalmenu::DialogSystem` + GPU text caches | Source owner, active dialogs | Остался уже перенесённый отдельно UserChat и backend draw submission |
| `Environment` | `source::Environment` + `OriginalRaceRenderer` | Source owner, active race/presentation path | Weather/world/quality/rain lifetime принадлежат source owner; bgfx/Metal исполняет pass/material commands и lamp shadow submission |
| `FinalMenu` | `mainmenu2::FinalMenuFrameState` + bgfx/CoreText view | Source owner, active frame | Legacy Widget API заменён backend draw/input/audio; credits, 107 s clock, slide alpha, Back и layout принадлежат source owner |
| `FinishMenu` | `originalracemenu::FinishMenuFrameState` + bgfx/CoreText view | Source owner, active frame | Legacy Widget API заменён backend draw/input; result order, timing, events и layout принадлежат source owner |
| `GameBase` | `OriginalGameObject`, behaviors, effects, motor sound state | Source owner, partial | Закрыть отсутствующие behavior subclasses и единый progress dispatch |
| `GameCar` | `source::GameCar::{OnFixedStepDrive,OnContact,OnPxSync}` + Jolt vehicle adapter | Source owner, active drive/contact frame | Оставшиеся PhysX solver queries и actor operations являются Jolt boundary; продолжить аудит public serialization/editor-only методов |
| `GameMode` | `source::GameModeState` + `GameModeRaceState` | Source owner, active race path | Startup/movie/config и часть menu/audio backend-команд ещё находятся в host |
| `GameObject` | `source::GameObject`, frame sync, listener/contact graph | Source owner, active core | Подключить оставшиеся fixed/frame callbacks и убрать backend-view ветви session |
| `HudMenu` | `source::{HudMenu,MiniMapFrame,PlayerStateFrame}` + `OriginalRaceHud` | Source owner, active HUD/minimap/player-state policy | State/Escape/layout/countdown, MiniMap, weapon/place/life, OnProcessEvent, notifications, car-life и opponents source-owned; FinishMenu не дублируется, bgfx/CoreText payload и camera projection остаются backend boundary |
| `HumanPlayer` | `source::HumanPlayer` | Source owner | Полный input message order, driving/progress gates и weapon selection перенесены; SDL только переводит source-сообщения |
| `Logic` | `source::Logic` + `WorldEventPump` progress registration | Source owner, active object/contact core | Убрать оставшиеся network authority и projectile backend-view loops из session |
| `MainMenu2` | `mainmenu2::{Controller,FrameController,ProfileFrameState,FinalMenuFrameState}` + `originalmenu::ScreenStack` | Source owner, active Main/Profile/Final path | Остались concrete network callbacks и backend draw submission |
| `Map` | `source::Map` + shared `source::DataBase` | Source owner, active registry path | XML parsing и backend actor create/destroy остаются adapters |
| `MapObj` | `source::MapObj*` record/list hierarchy | Source owner, partial | Убрать importer/runtime mirrors и проверить все concrete object types |
| `Menu` | `originalmenu::MenuSystem` + source Main/Profile/Dialog/Options/Race/Finish/Final owners | Source owner, frame core | Завершить concrete network callbacks; bgfx/CoreText/SDL остаются backend boundary |
| `MenuSystem` | `originalmenu::{MenuSystem,ScreenStack,FrameState}` | Source owner, active menu path | Подключить concrete navigation graphs; bgfx остаётся draw executor |
| `OptionsMenu` | `originaloptions::{OptionsMenuState,StartOptionsMenuState}` + backend visuals | Source owner, active options path | Legacy Widget events заменены SDL input, CoreText и bgfx draw submission |
| `Player` | `source::Player`, `CarState`, behaviors, `PresentationState` | Source owner, active gameplay/presentation state | bgfx/Jolt исполняют graph/actor commands; продолжить аудит remaining event/listener and profile bridges |
| `Race` | `OriginalRace`, `OriginalRaceSession`, lifecycle/place/tournament | Source owner, partial | Разложить 7 971-строчный источник по исходным владельцам вместо session |
| `RaceMenu2` | `originalracemenu::{RaceMenuState,RaceMainFrameState,GamersFrameState,GarageFrameState,WorkshopFrameState,SpaceshipFrameState,AngarFrameState,AchievementFrameState}` | Source owner, all six offline frame paths | Проверить оставшиеся network callbacks и оставить bgfx/CoreText backend boundary |
| `RecordLib` | `source::{MapObjRecordLibrary,MapObjRecordNode,MapObjRecord}` | Source owner, active hierarchy | Editor-only mutation/serialization API не входит в пользовательский runtime |
| `ResourceManager` | `OriginalResourceManager` + native readers/uploaders | Source owner, graph/sound path | Mesh/image/sound identity и lifetime общие; font/material-library ownership ещё нужно завершить |
| `RockCar` | `source::RockCar`, event sink | Source owner, partial | Проверить attachment/listener lifetime вместе с Player/GameCar |
| `Trace` | `source::{Trace,WayPath,WayNode,WayPoint}` | Source owner, active race/debug path | Editor serialization остаётся вне пользовательской игры; gameplay geometry и TraceGfx используют один owner |
| `TraceGfx` | `source::TraceGfx` + transient bgfx submission | Source owner, active F6 path | Selection/link/geometry/material policy source-owned; D3D9 Box/Sprite/DrawPrimitiveUP заменены backend triangles |
| `View` | SDL window/input + bgfx device | Backend boundary | Перенести source view policy: reset/display/input coordinate lifecycle |
| `Weapon` | `source::{Weapon,Proj,AutoProj,WeaponItem...}` | Source owner, active shot/contact/progress core | Shot preparation и attached progress выполнены; полностью свернуть оставшиеся free-projectile type-specific backend-view loops в Proj adapter |
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
исходного файла закрыта follow-up-блоком B8d/P2.189: `FlyTo/StopFly/InFly`,
`AutoObserver` для Garage/Angar и `ScreenToWorld/WorldToScreen/ScreenToRay/`
`ScreenPixelRayCastWithPlaneXY` теперь принадлежат source owner. SDL оставляет
только перевод pointer events, bgfx — построение view/projection matrices.

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
только proxy transform/life/name. Definitions `ctDecoration`, `ctTrack`,
`ctBonus` и `ctCar` загружаются как source records; ручная повторная сборка
destructible fragments, базовой жизни и `AutoProj` bonus description из
instance-load path удалена. Таким образом parser остаётся reader, а
record-каталог является фабрикой concrete gameplay object.

Follow-up B5a.1 вернул и самого владельца `DataBase.cpp`. Семь
`MapObjRecordLibrary` больше не принадлежат `Map`, а живут в отдельном
`source::DataBase`; `Map` получает стабильные record identities по ссылке.
`DataBase::Configure` выполняет единый clear/load/fix-up transaction при
смене карты, а `OriginalRaceSession` больше не содержит
`registerSourceDataBase` и lambdas загрузки записей. Standalone Map-smoke
получает локальный DataBase, active Race — общий owner с правильным порядком
уничтожения `Map -> RecordLib`.

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

Результат B7a: добавлен самостоятельный `source::Environment`, напрямую
сопоставленный с `Environment::{ApplyWheater,ApplyWorldType,ApplyQuality,
GetPerspectiveCameraFar,StartScene,ProcessScene,ReleaseScene}`. Он владеет
семью weather-ветвями, шестью world surface/HDR profiles, точными Garage и
Angar profiles, quality graph для shadow/light/post/environment, isometric
исключениями и rain lifecycle/follow-camera. Загрузчик гонки, CLI weather,
Garage/Angar presentation и Metal renderer используют один owner; четыре
разрозненные копии таблиц удалены.

`OriginalRaceRenderer` теперь получает готовый `EnvironmentRenderPolicy` и
оставляет у себя только bgfx pass/material submission. Regression закрепляет
weather tokens, World4 magma, World5 planar reflection, Garage lamps,
Middle/High/night/isometric quality graph и пересоздание rain при смене типа
камеры. Автономная arm64 сборка, 24/24 offline, 2/2 network, physics,
360-frame race, Finish и Final Metal smoke прошли.

Результат B7b: добавлен `source::TraceGfx` с исходными `SetSelPoint`,
`SetSelPath`, `SetSelNode`, `SetPointLink`, материалом transparency/alpha 0.5,
отключёнными lighting/Z-write/Z-test/fog/cull и backend-neutral draw list.
Он выдаёт красные waypoint boxes, серый диапазон отдельных paths, зелёные
selection path/point/tile и направленный point-link. Прежняя придуманная
зелёная лента шириной 0.16 и принудительно замкнутый последний сегмент удалены.

F6/AIDebug использует живой `source::Map::Trace`; bgfx переводит source
records в transient triangles вместо D3D9 `Box`, `Sprite` и
`DrawPrimitiveUP`, не владея selection или цветами. Regression проверяет
material flags, box/path geometry, grayscale range, все selections/link и
release удалённой ссылки. Прошли arm64 build, 25/25 offline, 2/2 network,
physics и отдельный 360-frame `--game-debug` Metal race smoke с активным F6.

B7 завершён. Следующий крупный этап B8 — повторный метод-к-методу аудит
оставшихся partial `Race/Player/AI/GameCar/Weapon` владельцев.

### B8 — завершение Race/Player/AI/GameCar/Weapon parity

После возврата общих владельцев повторить метод-к-методу аудит оставшихся
partial классов. На этом этапе session должен стать orchestration adapter,
а не второй реализацией игры.

Результат B8a: повторная сверка `AICar::UpdateAI`, `AICar::OnProgress`,
`AIPlayer::OnProgress` и `AISystem::OnProgress` обнаружила реальное нарушение
порядка. Session сначала отдельно выполняла Path/Control и записывала Jolt
input, а `AttackState` вызывала значительно позже внутри projectile/weapon
прохода. В Windows один `UpdateAI` всегда исполняет
`PathState -> AttackState -> ControlState`, причём attack видит текущий brake
и обновляет retained targets/RNG до control/reset.

Добавлен единый `AICar::ProgressResult` и combined `AIPlayer::OnProgress`.
`OriginalRaceSession::progressAi` теперь только собирает physics/weapon
snapshot, вызывает один source frame и сохраняет готовые move/attack commands;
Jolt input и `Weapon::Shot` исполняются позднее как backend commands. Отдельный
session-вызов `UpdateAttack` удалён. Regression проверяет, что один кадр
одновременно выдаёт source acceleration и правильное решение выстрела.
Прошли arm64 build, 25/25 offline, 2/2 network, physics и 360-frame
bgfx/Metal race smoke с шестью машинами.

Результат B8b: прямая сверка `GameCar::OnContact` подтвердила, что исходная
транзакция была вручную разделена между двумя местами
`OriginalRaceSession::updateGameplay`. В session находились собственные
формулы border/car damage, выбор жертвы по kinetic energy, clutch release и
spring-border redirect; при нулевом friction vector она дополнительно
подставляла отсутствующее в Windows направление.

`source::GameCar::OnContact` теперь снова является единым владельцем
исходного порядка: выставляет body contact, вычисляет пороги, определяет
shot-transparent border, снимает clutch lock, возвращает точную скорость
отскока, выбирает source/target damage по kinetic energy и формирует
нулевой touch для прочих объектов. Session только переводит Jolt snapshots
в backend-neutral input и применяет готовые velocity/damage commands.
Regression закрепляет low-force early return, high-speed border damage,
spring redirect, обе ветви car energy и decoration touch.

Следующий B8c — продолжение метода-к-методу аудита оставшихся partial
`Player/Race/Weapon` владельцев и удаление следующей подтверждённой
session-owned gameplay ветви.

Результат B8c: аудит `Player::InitLight`, `FreeLight`, `SetLightParent`,
`CreateNightLights`, `SetLightsParent`, `ApplyReflScene`, `ApplyColorMat` и
`ApplyColor` подтвердил renderer-owned суррогат. Portable `Player` хранил
только `HeadLightMode`, `reflScene` и цвет; renderer самостоятельно создавал
локальные позиции фар, выбирал их число, повторно читал `Vehicle::nightLights`
и решал, применять ли цветовой материал.

В `source::Player::PresentationState` возвращены точные created/enabled
lifetimes двух spot lights, исходные transforms/cones/range, first-light
high-quality shadow request, night-flare create/attach/node list,
`gpReflScene` и clone/attach state первого IVBMesh material. `CreateCar`,
`FreeCar`, `SetHeadlight`, `SetReflScene` и `SetColor` теперь обновляют этот
source owner в исходном порядке. Metal больше не реконструирует правила:
он только преобразует готовые source records в world lights/sprites,
reflection-pass gate и node color command.

Прошли arm64 build, 25/25 offline, 2/2 network, physics и 360-frame
bgfx/Metal race smoke с шестью машинами.

Следующий B8d — продолжить прямую сверку `Race/Weapon` и выбрать следующий
активный session-owned branch, а не отсутствующий editor-only API.

## Правило обновления карты

Каждый крупный block commit обязан:

1. назвать точные методы из `eff9338:prog`, которые стали активными;
2. указать новый source owner и оставшийся backend boundary;
3. добавить regression на source state/order;
4. обновить строку таблицы и статус блока;
5. пройти arm64 build, offline CTest, network CTest, physics smoke и Metal
   race-render smoke, если block касается активной гонки.
