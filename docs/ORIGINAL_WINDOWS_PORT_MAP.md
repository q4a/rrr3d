# Карта переноса оригинального Windows-кода

Дата среза: 2026-08-31

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
| `GameBase` | `OriginalGameObject`, all serialized behaviors, effects, motor sound state | Source owner, active behavior graph | Все shipped type 0–14 имеют concrete owner/runtime; bgfx/Jolt/SDL исполняют только graph/physics/audio commands |
| `GameCar` | `source::GameCar::{OnFixedStepDrive,OnContact,OnPxSync}` + Jolt vehicle adapter | Source owner, active drive/contact frame | Оставшиеся PhysX solver queries и actor operations являются Jolt boundary; продолжить аудит public serialization/editor-only методов |
| `GameMode` | source startup/startup-menu/movie/race/music-fade states + one shared menu/game music source on shared World | Source owner, active startup/menu/race/audio-policy path | Config serialization и backend dispatch остаются adapters; bgfx/SDL/CoreAudio исполняют готовые source commands |
| `GameObject` | `source::GameObject`, event counters, frame sync, listener/contact graph | Source owner, active core | Jolt/bgfx snapshots остаются backend boundary; source fixed/frame registration подключена |
| `HudMenu` | `source::{HudMenu,MiniMapFrame,PlayerStateFrame}` + `OriginalRaceHud` | Source owner, active HUD/minimap/player-state policy | State/Escape/layout/countdown, MiniMap, weapon/place/life, OnProcessEvent, notifications, car-life и opponents source-owned; FinishMenu не дублируется, bgfx/CoreText payload и camera projection остаются backend boundary |
| `HumanPlayer` | `source::HumanPlayer` | Source owner, active selection owner | Полный input message order, driving/progress gates и единственное состояние текущего primary weapon перенесены; SDL только переводит source-сообщения |
| `Logic` | `source::Logic` + shared active `WorldEventPump` registration | Source owner, active object/contact/audio-category/transient lifetime core | Proj/Mine lifetime source-owned; остались network authority и backend pose/contact views, SDL/CoreAudio остаётся submix backend |
| `MainMenu2` | `mainmenu2::{Controller,FrameController,ProfileFrameState,FinalMenuFrameState}` + `originalmenu::ScreenStack` | Source owner, active Main/Profile/Final path | Остались concrete network callbacks и backend draw submission |
| `Map` | `source::Map` + shared `source::DataBase` | Source owner, active registry path | XML parsing и backend actor create/destroy остаются adapters |
| `MapObj` | `source::MapObj*` record/list hierarchy | Source owner, active live-ID/runtime hierarchy | Global ID и decoration/bonus lifetime выдаёт только live `Map::MapObjList`/`GameObject`; AutoProj владеет arming scale, `sourceIndex` остаётся backend mapping |
| `Menu` | `originalmenu::MenuSystem` + source Main/Profile/Dialog/Options/Race/Finish/Final owners | Source owner, frame core | Завершить concrete network callbacks; bgfx/CoreText/SDL остаются backend boundary |
| `MenuSystem` | `originalmenu::{MenuSystem,ScreenStack,FrameState}` | Source owner, active menu path | Подключить concrete navigation graphs; bgfx остаётся draw executor |
| `OptionsMenu` | `originaloptions::{OptionsMenuState,StartOptionsMenuState}` + backend visuals | Source owner, active options path | Legacy Widget events заменены SDL input, CoreText и bgfx draw submission |
| `Player` | `source::Player`, `CarState`, behaviors, `PresentationState` | Source owner, active gameplay/presentation/live-car/trace path | `CarState` владеет trace state, concrete `WeaponItem` — live charge, `HumanPlayer` — primary selection; profile/import передаёт одноразовый `WeaponLoadout`, bgfx/Jolt исполняют graph/actor commands |
| `Race` | `OriginalRace`, `source::{RaceRunState,RaceLifecycle,RacePlaceModel}`, session adapter, tournament on shared World | Source owner, active fixed/late/finish/settlement lifecycle | `CompleteRace` Player publication, picked-money reset, block, campaign reward, tournament/pass-reset source-owned; продолжить вынос physics/contact materialization из session |
| `RaceMenu2` | `originalracemenu::{RaceMenuState,RaceMainFrameState,GamersFrameState,GarageFrameState,WorkshopFrameState,SpaceshipFrameState,AngarFrameState,AchievementFrameState}` | Source owner, all six offline frame paths | Проверить оставшиеся network callbacks и оставить bgfx/CoreText backend boundary |
| `RecordLib` | `source::{MapObjRecordLibrary,MapObjRecordNode,MapObjRecord}` | Source owner, active hierarchy | Editor-only mutation/serialization API не входит в пользовательский runtime |
| `ResourceManager` | `OriginalResourceManager` + native readers/uploaders | Source owner, active GUI/graph/sound/font/material/string path | Полные 322 Mesh/489 Image/257 Mat/46 eager Sound, 5 TextFont и выбранная String library имеют общие identity, descriptors, language/charset, world tags и lifetime; bgfx/SDL payload остаётся backend boundary |
| `RockCar` | `source::RockCar`, dynamic `Weapons`, Player listener/contact sink | Source owner, active gameplay graph | PhysX actor/solver calls заменены Jolt; editor/legacy serializer остаётся parser boundary |
| `Trace` | `source::{Trace,WayPath,WayNode,WayPoint}` | Source owner, active race/debug path | Editor serialization остаётся вне пользовательской игры; gameplay geometry и TraceGfx используют один owner |
| `TraceGfx` | `source::TraceGfx` + transient bgfx submission | Source owner, active F6 path | Selection/link/geometry/material policy source-owned; D3D9 Box/Sprite/DrawPrimitiveUP заменены backend triangles |
| `View` | `originalview::ViewState` + SDL/bgfx adapters | Source owner, active input/display path | `ScreenToView`, projection conversion и mouse click/move snapshots source-owned; SDL сообщает logical client/drawable sizes, bgfx исполняет resize |
| `Weapon` | `source::{Weapon,Proj,AutoProj,WeaponItem...}` | Source owner, active shot/contact/progress/query core | `Proj` владеет Laser/FrostRay/Rocket/Mine scene-query masks/offsets/range и shot-track rejection; Jolt исполняет raycast/actor writes, network authority остаётся adapter boundary |
| `World` | one active `source::WorldFrameClock`/`WorldEventPump`/`Environment` + native `WorldHost` | Source owner, active timing/event/environment core | GameMode, Logic, Race/place, GameObject registrations и `Environment::ProcessScene` используют один owner; SDL clock, Jolt substeps и bgfx submission остаются backend boundaries |

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

Результат B4a/B8ad: `Logic` снова подключён к `WorldEventPump` как исходный
`WorldHost`, а отдельный `LogicBehaviors` владеет единственным shipped
`PairPxContactEffect`. `LogicBehavior::RegProgressEvent` ставит его ordered
`ProgressEvent` после `Logic::OnProgress`; attach/detach World больше не
управляет конкретным эффектом напрямую. Jolt contact manifolds входят через
`Logic::OnContact → LogicBehaviors::OnContact`, как PhysX `PxSceneUser` в
Windows; команды освобождения эффектов забираются после progress. Ручной
вызов contact timer из session удалён.

Результат B8ae: между global behavior и pair effect снова находится
`LogicEventEffect`. В D3D9 owner хранил `MapObj*`; bgfx adapter получает
backend-neutral stable handle, но create → Death → particle-end destroy и
очистка contact reference принадлежат исходному owner. Actor/slot остаются
renderer metadata и больше не используются для выбора поколения эффекта.

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

### B5 — DataBase/RecordLib/ResourceManager (B5a–B5g выполнены)

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

Результат B5a.2/B8af возвращает хвост исходного `DataBase::Init` после
`InitMapObjLib`. `LogicBehaviors` больше не конструирует pair effect заранее:
DataBase добавляет его, связывает с typed `ctEffects/spark2` record и хранит
в owner все пять `light_impact` sound paths. Session только преобразует
выбранный sound/effect в SDL/bgfx payload. `ctEffects` projectile records и
полный active `ctWeapon` catalog также создают concrete `AutoProj`/`Weapon`
с parsed descriptors; shipped пустой `ctWaypoint` остаётся пустым.

Результат B5a.3/B8ag переносит record-компонент type-10 вместе с concrete
`Weapon`. `ShotEffect::OnShot` после каждого успешного `PrepareProj` сам
формирует child effect/sound request с serialized pos/impulse/ignoreRot.
`WeaponItem` переносит компонент record-а на mounted weapon; session больше
не реконструирует его из `Race::weapons`, а лишь исполняет bgfx/SDL payload.

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

Результат B5c.1/B8aw закрывает оставшийся обход ImageLib в активном GUI.
Все 111 source-image uploads в `main_bgfx_original_menu` получают texture у
того же canonical owner, что HUD/garage/race; экранные release paths больше
не уничтожают shared handle. Manager имеет RAII shutdown и исходный порядок
SoundLib→ImageLib→MeshLib. Повторный `LoadSound` после `Find` обновляет volume,
а rebind backend сначала выгружает прежний SoundLib. Fake-backend regression
фиксирует cache/borrow/rebind/idempotence без зависимости от Metal window.

`MusicCat` намеренно остаётся отдельным потоковым владельцем: он асинхронно
декодирует только текущий/следующий track и выгружает старый, тогда как
Windows-каталог `LoadMusic` был декларацией имён. Принудительное помещение
всех tracks в PCM SoundLib вернуло бы задержки и расход памяти, устранённые в
Milestone 8.

Результат B5d/B8ax: `TextFontLib` владеет точными пятью descriptor records
из `LoadGUI`, а `SetFontCharset` обновляет их вместе при смене языка. Menu и
HUD больше не выбирают Verdana/size/weight напрямую: CoreText растеризует
полученный source descriptor. Исправлены source font roles для HUD и все
Options/StartOptions labels возвращены к Small 24. Regression проверяет
имена, размеры, bold и charset propagation.

Результат B5e/B8ay: `ComplexMatLib` снова является общим owner именованных
`LibMaterial`. Race mesh nodes, particle emitters и HUD weapon previews
хранят ссылки на один canonical descriptor, а ImageLib samplers загружаются
из него. Первый descriptor с данным source-именем сохраняет identity, как в
Windows library; bgfx pipeline/texture payload остаётся backend boundary.

Результат B5f/B8az: `ComplexMeshLib` и `ComplexImageLib` сохраняют records
после выгрузки их decoded/GPU payload. `ResourceManager::LoadWorld` снова
использует теги `wtWorld1..wtWorld6`: после teardown текущего race renderer
освобождает mesh/image payload прежней планеты, а новый мир лениво
материализуется до первого кадра с прежней identity. Глобальные GUI/car/
weapon resources не затрагиваются. Проверка `ShaderLib` отдельно доказала,
что в shipped source она пуста: обе регистрации закомментированы, поэтому
bgfx shader catalog туда намеренно не выдуман.

Результат B5g/B8ba: `ResourceManager::Load()` заранее регистрирует полный
source MatLib catalog: 251 уникальное имя из активных `Load*LibMat`/
`LoadCarLibMat` вызовов и шесть вручную созданных `LibMaterial`, всего 257.
Parser больше не определяет доступность материала фактом посещения трассы.
Descriptor records создаются сразу, но ImageLib/bgfx payload остаётся ленивым.

Результат B5h/B8bb: перенесён декларативный каталог `ComplexMeshLib` и
`ComplexImageLib` из всех оригинальных `Load*`-методов. Сохранены 324
`LoadMesh`/490 `LoadImage` вызовов в исходном порядке, включая три повторные
записи; manager объединяет их в 322 mesh и 489 image identity. Для каждой
записи сохранены TBN, immediate load/init, mip count, GUI, world index и
отдельные tag actions `LoadData/InitIVB/InitTex2d`. Это не filesystem scan:
лишние копии ресурсов с суффиксом ` 2` в библиотеку не попадают. Записи
создаются при `ResourceManager::Load`, а decoded/bgfx payload остаётся
ленивой backend-границей. Единственный объявленный, но отсутствующий в
оригинальном game-data `GUI/wndLight6.png` остаётся допустимой неиспользуемой
записью, как в Windows; активный `RaceMenu2` использует `wndLight4.png`.

Результат B5i/B8bc: восстановлен вызываемый в конце `ResourceManager::Load`
`LoadSounds`. Все 46 именованных gameplay/UI SFX records существуют до
создания menu SoundSheme; сохранены семь source volume=2, остальные volume=1,
нулевой distanceScaler и eager-load. Так как SDL audio backend создаётся
позже графических библиотек, descriptors регистрируются при `Load`, а Ogg
payload материализуется одной стадией `AttachAudio` до menu initialization.
Music остаётся в уже перенесённом потоковом `MusicCat`, а commentator
добавляет style-dependent optional records при выборе языка, как отдельные
исходные `LoadMusic/LoadCommentator` lifetimes.

Результат B5j/B8bd: `StringLibrary` больше не хранится отдельной копией в
`MainMenu2::Model` и не перечитывается HUD-ом. Единственный persistent owner
находится в `OriginalResourceManager`, а `GameMode::ApplyLanguage` перенесён
как атомарная загрузка UTF-16LE StringLib и смена charset всего TextFontLib.
Все active menu lookup и HUD lap/place/racer/gamer strings читают этот owner.
Выбор другого языка в Options сохраняется вместе с исходным need-reload
dialog, но не смешивает новый charset со старым уже построенным widget tree.
Shutdown соблюдает Windows-порядок Sound→String→TextFont→Mat→Image→Mesh;
regression проверяет Get/Has, empty/missing fallback, escaped newline,
повторную загрузку с устойчивой identity и очистку.

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

Результат B8d: moving `Proj::OnContact` для Rocket/Torpeda/Mortira/Thunder/
Resonanse, Sonar и Impulse перенесён в единый `source::Proj::ContactDynamic`.
Source снова владеет listener/live-state guard, damage attribution, Rocket
torque, Sonar impulse и Impulse `_tick1`; session применяет только Jolt
commands и effect spawn. Исправлены подтверждённые отличия: Rocket death
снова предшествует damage, lethal damage не подавляет последующий torque, а
Impulse death сохраняет `dtEnergy` вместо adapter `dtSimple`.

Следующий B8e — перенести оставшуюся `Logic::MineContact`/bonus contact
authority, включая сетевой RPC gate, не смешивая её с Jolt overlap query.

Результат B8e: source `Logic::MineContact` снова выбирает immediate
`Proj::MineContact` либо RPC contacted `NetPlayer`, а concrete Proj выдаёт
исходную транзакцию `Death -> DamageTarget(dtMine) -> AddContactForce`.
`GameObject::OnContact` выполняется на первичном физическом контакте до
network gate; RPC replay его не дублирует. Jolt boundary ограничен overlap,
геометрией contact point и применением готового impulse.

`Logic::TakeBonus` также разделяет source RPC request и `Player::TakeBonus`.
В сетевой игре pickup больше не лечит/начисляет деньги, не уничтожает MapObj,
не рисует HUD и не засчитывает achievement до `NetPlayer::OnTakeBonus` replay.
Pending contact подавляет повторные запросы, а replay использует переданные
type/value и сохраняет исходный Charge RNG момент.

Результат B8f: восстановление машины теперь целиком принадлежит concrete
`Player::OnProgress`. Подтверждённый surrogate оживлял `gameCar` и выдавал
respawn за кадр до `CreateCar(false)`/нового MapObj; session затем определял
наличие машины по `!IsDestroyed()`, а не по source `_car.mapObj`.

Возвращён точный строгий timer `> 2.0f` и атомарный порядок
`CreateCar(false) -> MapObj materialization -> ResetCar`. Удалён invented
`ActivateCar` промежуточный state. Jolt получает только итоговую reset pose в
том же fixed-step, а следующий кадр не создаёт второй объект/respawn.

Результат B8g: возвращён полный `Player::_bonusProjs` owner вместо списка
голых ID. Успешный mine shot сохраняет `Proj*/id` и listener, live lookup
работает по concrete объекту, а `Player::OnDestroy` удаляет запись на исходной
границе. Session больше не имитирует эту очистку сразу при `Death`.

Также восстановлен Windows destructor dispatch: `Proj::~Proj`,
`RockCar::~RockCar` и `GameCar::~GameCar` вызывают `DestroyObject` до потери
derived vtable. Явный `Player::~Player` отцепляет projectile/car listeners до
разрушения embedded `GameCar`, устраняя подтверждённый shutdown crash.

Результат B8h: `MapObj::~MapObj`, `CreateGameObj` и временный record object в
`BindGameObj` больше не стирают `MapObj`/`Logic`/parent до уничтожения owned
`GameObject`. Возвращён source порядок `Assign old -> delete old -> publish
replacement`; `OnDestroy` снова получает полный sender context и derived
identity. Не-владеющий stable-address `Player::gameCar` сохранён отдельной
portable backend-ветвью.

Результат B8i: оставшийся session-owned `MineRipUpdate` split перенесён в
`Proj::BuildMineRipSplitPlan`. Concrete Proj теперь формирует model2/model3
child descriptors, source lifetime RNG order, один core/пять fragments,
дискретный Vec3Range impulse и parent-death command. Session ограничен
материализацией готовых child plans в Jolt/runtime views.

`GameObject::SetLogic` registration counters не были подменены пустым
generic callback: Windows `GameCar::OnFixedStep` требует snapshot реального
PhysX шага, поэтому их активное подключение должно выполняться единым B8j
вместе с Jolt fixed-step bridge, без второго физического шага.

Результат B8j: `Proj::BuildDeathProjectileSpawnPlan` забрал из session
Mortira `deathProjectile` lookup, `ptCrater` type gate, child descriptor,
position offset, lifetime RNG и `effectPxIgnoreSenderCar`. Session применяет
только Jolt world contact transform и создаёт готовый autonomous runtime view;
невалидные планы не потребляют RNG.

Результат B8k: восстановлены четыре source reference counter и точный
`GameObject::SetLogic` unregister/re-register order. `GameCar` снова
регистрирует fixed-step в constructor и снимает в derived destructor;
portable progress counter намеренно не добавлен в World, поскольку эти
вызовы закомментированы в оригинале.

Jolt per-vehicle callback адресно dispatch-ит только соответствующий
зарегистрированный `GameCar`, поэтому общий World list не выполняется N раз.
Awake/sleep управляет late+frame reference, network correction — отдельным
frame reference, а renderer получает последний source graph state, когда
тело спит. Regression закрепляет смену Logic/World, ref-count gates, один
torque/gear update и active/sleep frame lifecycle.

Следующий B8l — продолжить method-to-method аудит `Race/Player/Weapon` и
вынести следующий подтверждённый session-owned gameplay transaction в
concrete source owner.

Результат B8l: `RacePlaceModel` стал реальным `LateProgressEvent`. Active
runtime больше не сортирует места и не формирует Lead/Third/Last события до
`physicsWorld->step`; main передаёт завершённые Jolt poses, после чего World
late-progress вызывает source `Race::OnLateProgress` owner.

Countdown, обычная гонка и finish-wait ставят один pending late pass. Для
headless source-smokes сохранён синхронный fallback, а exit выполняет
немедленный final late pass до teardown. Regression закрепляет, что prepared
roster не сортируется до World callback и готовый update выдаётся ровно один
раз.

Следующий B8m — проверить source fixed-step частоту `Race::OnFixedStep`
(Player затем AISystem) относительно Jolt 1/120 substeps и убрать следующий
подтверждённый frame-rate dependent session path.

Результат B8m: добавлена отдельная world-level граница source fixed-step.
На каждом исходном интервале `World::cMaxSimStep == 1/60` она получает
завершённые состояния всех машин и ровно один раз выполняет
`Race::OnFixedStep`: сначала весь список `Player::OnProgress`, затем
`AISystem::OnProgress` при `GoRace`. После неё адресные callbacks вызывают
зарегистрированные `GameCar`, как и порядок Windows
`Race -> level/GameCar`.

Frame update больше не повторяет Player/AI с render delta. Jolt сохраняет
два внутренних solver substep 1/120, но source AI/GameCar получают один шаг
1/60, а готовая drive command кэшируется между backend substeps. Возникший в
`Player::OnProgress` `ResetCar` возвращается backend как готовая команда и
применяется до того же Jolt `Update`. Fixed-step события сохраняются отдельно
до следующего adapter event pass, не теряются при очистке кадра и не
дублируют уже обработанные Progress events. Regression требует ноль source
callbacks после первого render frame 1/120, ровно один после второго и один
reset.

Результат B8n: дополнительная сверка `World::cMaxSimStep` исправила
обнаруженную при B8m неверную привязку source callbacks к backend 1/120.
Аккумулятор Race/GameCar теперь строго 60 Гц, сохраняется между render frames
и не зависит от частоты дисплея. Torque/gear/brake/grip кэшируются на полный
source interval; прямой yaw и momentum stabilization не применяются дважды.
Два вызова physics по 1/120 дают ноль, затем ровно один source callback.

Следующий B8o — проверить применение результата `AISystem::OnProgress`:
portable `updateGameplay` сейчас исполняет attack/weapon command до нового
physics callback и поэтому использует решение предыдущего кадра вместо того
же исходного fixed-step.

Результат B8o: одиночный `aiProgressScratch_` больше не является владельцем
результата `AICar::AttackState::Update`. Каждый source fixed-step сохраняет
свою attack-команду в упорядоченной очереди, поэтому два или более интервала
1/60 внутри медленного render frame не стирают ранний Shot/Hyper/Mine решением
последнего интервала. Перед исполнением повторно проверяются live Player,
finished/disconnected/death и готовность concrete Weapon.

`AICar::ControlState::UpdateResetCar` также возвращён в атомарную
`Race::OnFixedStep` транзакцию: edge потребляется там же и готовый ResetCar
попадает в Jolt reset batch до solver update. Поздний frame-loop удалён.
Regression закрепляет два fixed-step, где первый стреляет, второй теряет цель,
и отдельный off-trace AI reset, появляющийся в том же backend batch.

Следующий B8p — убрать оставшуюся задержку materialization attack-команд до
следующего render `updateGameplay`: `Logic::Shot` должен изменить charge,
cooldown и source projectile graph до следующего source fixed-step, даже если
оба интервала 1/60 находятся внутри одного render frame.

Результат B8p: обычная AI-атака теперь завершает исходную цепочку
`Logic::Shot -> Player::Shot -> WeaponItem::Shot -> Weapon::CreateShot`
внутри того же source fixed-step. Transform берётся из установленного
physical slot и текущего Jolt pose, concrete `Proj` сразу передаётся во
владение `Logic`, charge списывается, а `Weapon::OnShot` сбрасывает cooldown
до следующего вызова `AICar::AttackState`.

Очередь сохраняет созданные source projectiles и позже materialize-ит только
backend view — `ProjectileRuntime`, contact/effect/event и bgfx presentation —
без второго вызова `Shot`. При stale owner, finish и disconnect временные
projectiles уничтожаются до teardown weapon/player listeners. Regression
проверяет ровно одно списание и cooldown через два fixed-step без render
между ними; arm64 build, 29/29 CTest, physics и 360-frame Metal smoke прошли.

Следующий B8q — перенести в ту же fixed-step транзакцию исходные
`Logic::Shot(stHyper)` и `Logic::Shot(stMine)`; сейчас их ordered-команды
сохраняются, но concrete source projectile/charge всё ещё создаются frame
adapter-ом.

Результат B8q: `AICar::AttackState::RunHyper` и `PlaceMine` теперь завершают
свои исходные `Logic::Shot` транзакции внутри `Race::OnFixedStep` вслед за
обычной атакой. Для Hyper fixed-step создаёт concrete `Proj`, списывает
charge, сбрасывает cooldown, рассчитывает lifetime и сохраняет точный local
impulse/spring lock. Для Mine он выполняет source ray placement,
`Player::InsertBonusProj`, charge/cooldown и `GameCar::LockMine`.

Frame adapter получает уже готовые source projectiles и создаёт только
`ProjectileRuntime`/`MineRuntime`, Jolt velocity request, event и effect.
Повторного `Shot` нет. Regression проводит AI по WayPath и проверяет source
state до render pass, отсутствие второго списания на следующем fixed-step и
ровно одну backend materialization. Полная arm64 сборка, 29/29 CTest,
physics и 360-frame Metal smoke прошли.

Следующий B8r — передать подготовленные Jolt velocity/runtime команды из
того же world fixed-step до solver update, устранив оставшуюся одно-frame
backend задержку Hyper/Mine при сохранении source 60 Гц и Jolt 120 Гц.

Результат B8r: Hyper создаёт attached `ProjectileRuntime`, Mine создаёт
`MineRuntime`, а spring lock попадает в итоговый vehicle input непосредственно
внутри source fixed-step. Новый список `VehicleLinearVelocityCommand`
возвращается из `WorldFixedStepController`; Jolt применяет его после
same-step reset и до текущего `system_.Update`.

Frame adapter теперь отвечает только за `HyperActivated`/`MinePlaced`, shot
effects и presentation lifetime. Он не создаёт повторный runtime и не
добавляет второй импульс. Regression требует по одному runtime до adapter и
одну Hyper velocity-команду; Jolt smoke проверяет, что эта скорость уже
присутствует после того же solver update.

Следующий B8s — проверить и при необходимости перенести в эту же границу
обычный AI projectile runtime: concrete source `Proj` создаётся fixed-step-ом,
но его автономный Jolt/bgfx view пока материализуется render adapter-ом.

Результат B8s: `prepareAiWeaponAttack` после успешного
`Logic::Shot -> Weapon::CreateShot` сразу строит backend-neutral runtime для
каждого persistent concrete `Proj`. Общий builder переносит точный source
transform, projectile index, relative launch velocity и
ballistic/homing/attached route; выбранная `AttackState` цель становится
runtime homing target в том же firing tick.

`fireWeapon` больше не создаёт второй runtime и не повторяет
`PrepareLaunch`: он находит fixed-step view по `Proj*`, затем формирует только
ray/contact result, network event и visual/audio effects. Headless fallback
отдельно сохраняет порядок World frame progress перед Race fixed-step, не
продвигая новый projectile дважды в кадре его создания.

Следующий B8t — проверить backend-интеграцию свободных projectiles между
source fixed intervals. Их source `Proj::OnProgress` корректно остаётся
frame event, но перенос позиции/контактов replacement physics пока выполняет
session `updateGameplay`, а не Jolt solver boundary.

Результат B8t: persistent non-ray/non-attached `Proj` получает dynamic Jolt
sensor body со стабильным id, исходным collision center/half-extents, mass,
gravity factor и launch velocity. Create из source fixed-step применяется до
solver; после solve session получает фактические pose/velocity и использует
их для source progress и bgfx presentation. Destroy следует lifetime/death
исходного объекта, а Synchronize переносит homing/Thunder/rocket изменения
обратно в backend.

Ручной `velocity * delta` путь оставлен только session regression без
external physics. Интеграционный тест требует один Create и pose round-trip,
Jolt smoke — перемещение нового actor в том же interval. Следующий B8u —
подключить Jolt sensor contact stream и ray queries к исходным
`Proj::OnContact`/`Logic` branches, затем удалить дублирующие snapshot
box/ray проверки активной гонки.

Результат B8u: `OriginalContactListener` теперь маршрутизирует manifold не
только в `GameCar`, но и в projectile actor по его user-data index. В
`ProjectileBodyState` сохраняются точные vehicle/decoration owner, surface,
actor id, normal, relative speed и contact points. Sensor-projectile при этом
исключён из списка chassis contacts автомобиля и не маскируется как track.

Session использует этот поток для owner separation, `ContactDynamic`, Sonar,
decoration damage и Thunder border reflection. OBB reconstruction остаётся
только при выключенном external projectile physics. Jolt regression требует
реальный manifold projectile↔vehicle, adapter regression — contact identity
round-trip. Следующий B8v — backend raycast для Laser/FrostRay и
`Proj::RocketUpdate`, остающийся snapshot query активной гонки.

Результат B8v: replacement physics экспортирует closest ray query поверх
живого Jolt narrow phase. Полная projectile group различает Vehicle,
Decoration и track surface, сохраняет body actor id/point/normal, игнорирует
машину стрелка и projectile sensors. TrackPlane-only вариант соответствует
группе, которую исходные `MinePrepare` и `RocketUpdate` запрашивали отдельно.

`OriginalRaceSession` использует backend callback для Laser/FrostRay,
ручной/AI MinePrepare, ballistic mine ground и rocket-height; CPU mesh/OBB
query остаётся только fallback-ом без world. Regression проверяет оба Jolt
фильтра и факт вызова callback при source MineRip placement. Следующий B8w —
перенести физические actors и contact stream размещённых mine/bonus
projectiles, которые пока остаются session-integrated.

Результат B8w: weapon/AI Mine, crater и каждый MineRip child получают
stable-id Jolt sensor actor из concrete source `ProjDesc`. AI Create выдаётся
из `Race::OnFixedStep` до solve; split children сбрасывают скопированный
backend id и создают собственные bodies. Jolt gravity/velocity заменяют
ручную интеграцию, а ground clamp синхронно ставит velocity/gravity в ноль.

MineContact/Maslo/Crater/network branches используют vehicle identity и
точку sensor manifold. Lifetime, split, source death и stale Logic owner
удаляют body до стирания runtime. Source-only fallback сохраняет прежний OBB
путь. Следующий B8x — зарегистрировать map-owned AutoProj bonus/hazard actors
и убрать оставшийся OBB contact loop статических pickups/mines/oil.

Результат B8x: все живые Bonus MapObj bootstrap-ят static Jolt sensor actor
из своего concrete `AutoProj` descriptor и serialized transform. Stable ids
сопоставляют contact stream с индексом map bonus; SpeedArrow, Lusha, Maslo,
MineHazard и TakeBonus используют реальный vehicle manifold вместо OBB.

Pickup/source death немедленно выдаёт Destroy и очищает cache. Static motion
не создаёт gravity drift и постоянную dynamic integration нагрузку. Reset
строит roster заново; fixed-step regressions сначала отделяют map bootstrap,
затем требуют единственный attack actor. Следующий B8y — проверить death
plane/reset/direct attached weapon contact consumers и перенести оставшиеся
активные snapshot queries по подтверждённому Windows call graph.

Результат B8y: `FireUpdate` и `DrobilkaUpdate` управляют kinematic Jolt
sensor actor в transform установленного оружия. Fire/Drobilka contact handlers
получают vehicle/decor identity и manifold point от завершённого solve вместо
реконструкции OBB по snapshot. Source-only tests сохраняют прежний fallback.

`Proj::OnDestroy`-переход также сохранён: потерявший weapon pointer attached
actor пересоздаётся dynamic, продолжает движение с последней car velocity и
включает gravity. Physics smoke требует kinematic projectile↔vehicle contact.
Следующий B8z — перенести постоянную death plane и ResetCar scene queries в
backend, не меняя исходный `Player::ResetCar` выбор trace node.

Результат B8z: `Map::Map` снова владеет отдельным death-plane physics actor —
в replacement backend это static Jolt PlaneShape sensor на source Z=0.
Vehicle manifold вызывает оригинальный TouchDeath/DeathPlane path; generic
contact particles/audio этот actor не обслуживают.

`Player::ResetCar` сохранил Windows tile/offset/previous-node алгоритм, а его
scene callback использует closest Jolt ray с маской TrackPlane + DeathPlane +
Default actors. Projectile queries death plane фильтруют. CPU reconstruction
осталась только при отсутствии backend. Следующий B8aa — повторный поиск
активных snapshot physics consumers после закрытия projectile/mine/bonus/
attached/death/reset graph.

Результат B8ac: `DataBase::LoadBonus`/`AutoProj::InitProj` ownership доведён
до завершения объекта. Pickup сохраняет исходный порядок
`Player::TakeBonus`: сначала `bonus.Death()` и type-6 listeners, затем reward.
Map mine использует тот же `Proj::DestroyWithEffect` с target и `dtMine`.
Session больше не принимает решение о создании serialized bonus effect, а
только материализует подтверждённый spawn-plan в bgfx/SDL/Jolt adapters.

Результат B8ah: car-owned `LowLifePoints`, `DamageEffect` и
`ImmortalEffect` снова получают свои `MapObjRec`-эквиваленты непосредственно
в `Player::SetCar`. Persistent `smoke6` создаётся и удаляется как child car
actor, `damageEnergy*` выдаётся через one-live owner plan, а shield record и
`scaleK` принадлежат behavior. Session/renderer используют эти ссылки и не
выбирают эффекты повторно из общего Race descriptor.

Результат B8ai: `Proj::FrostRayUpdate` снова добавляет `SlowEffect`, который
сам владеет точной model3-записью и одним car-child actor. Session переводит
owner spawn-plan в backend object, а bgfx рисует этот actor; прямой renderer
lookup по weapon/projectile индексам удалён.

Результат B8aj: serialized wheel graph больше не схлопывается. Каждый
`CarWheel` владеет ordered type-9 `trail`/`smoke7` behaviors с точными
record/pos/sound полями. bgfx/SDL потребляют отдельные owner states; ложный
SkidAsphalt на беззвучных колёсах удалён.

Результат B8ak: `EventEffect::_sounds` восстановлен как общий каталог
concrete behavior. `LifeEffect` сам выбирает свой Source3d reference и
выдаёт один Play request; `RaceEffect` больше не хранит параллельный список.
`ShotEffect` и `PxWheelSlipEffect` используют тот же owner API, SDL остаётся
только backend источника/позиционирования.

Результат B8al: backend effect-object снова уведомляет конкретный
`EventEffect` через эквивалент `GameObjEvent::OnDestroy`. Natural expiry,
reset, finish, respawn и disconnect используют один callback path; ссылки
доставляются до уничтожения Player/Logic owners. Frost model3 дополнительно
запускает source `SlowEffect::OnDestroyEffect` deferred removal.

Результат B8am: общий `EventEffect::_effObjList` больше не схлопнут в один
boolean `_makeEffect`. Каждый transient `ShotEffect::CreateEffect` получает
свой owner handle, backend возвращает тот же handle при уничтожении, а
копирование `Weapon` не переносит live actors и pending owner pointers.
bgfx получает visual-definition от этого же live owner, без повторного
lookup в `Race::weapons`.

Результат B8an: type-6 `DeathEffect` передаёт backend exact owner/handle для
vehicle/projectile/mine/bonus actors. Погибший transient `Proj` остаётся в
`Logic` до callback последнего death-effect; car respawn отсоединяет ссылки
старого behavior graph. Renderer читает live owner-definition.

Результат B8ao: унаследованные поля `EventEffect` для type-6 больше не
остаются session-owned. `DeathEffect` хранит exact visual record, local
position/impulse, `ignoreRot` и sound catalog, а единый spawn snapshot
обслуживает car/projectile/mine/bonus. Jolt dynamic debris получает
локальный импульс из этого snapshot один раз, после поворота body, как
PhysX `addLocalForce(..., NX_IMPULSE)`.

Результат B8ap: `Proj::_model/_model2` снова являются concrete include
objects, а не именами, заново материализуемыми session. При смерти только
реально созданный child с `FxSystemWaitingEnd` отсоединяется в своём точном
world transform и удерживает копию source record до конца частиц. Generic
impact больше не создаёт `model3`: Frost использует его исключительно как
car-owned `SlowEffect`, MineRip — через исходный explicit nested spawn.

Результат B8aq: формулы и pointer-state из `View.cpp` перенесены в
`originalview::ViewState`. Все активные menu/dialog/garage/workshop/options
hit tests и исходный курсор используют единый rounded `ScreenToView`, а
click/move snapshots сохраняют projection, delta и offset от последнего
клика. SDL adapter обновляет logical client и Metal drawable размеры на
каждом resize и перед pointer dispatch, поэтому Retina/fullscreen transition
больше не оставляет разные масштабы у отрисовки и input. Win32 window style,
D3D reset и camera ray construction остаются разрешёнными backend-границами.

Результат B8ar: `World::MainProgress` больше не дублируется локальной
15-элементной таблицей host-а и не передаёт постоянный `physicsAlpha=0`.
`source::WorldFrameClock` хранит исходные double frame samples/accumulator,
15-frame average, clamp `7/60`, шаг `1/60`, число fixed steps и render alpha.
Active loop потребляет его `deltaTime/physicsAlpha`; Jolt выполняет
разрешённый solver substep через уже существующий `WorldFixedStepController`,
без второго физического шага. Regression покрывает half-step carry, alpha,
clamp, race-off `-1` и отключение frame synchronization.

Результат B8as: устранены два параллельных `WorldEventPump`. Active app
создаёт World до `OriginalRaceSession`; session подключает к нему `Logic`,
`RacePlaceModel`, GameCar и behaviors, а затем тот же объект получает
`GameModeState`. Встроенный session World сохранён только для автономных
headless regressions. Pause/frame/fixed/late registrations теперь имеют
один lifetime owner. `WorldFrameClock::physicsAlpha` дополнительно передаётся
в адресный `GameCar::DispatchPxSync`, поэтому графическая поза использует
исходную незавершённую долю шага, а не default `1.0`. Regression требует
внешний owner и ненулевые progress/fixed/late списки; Jolt остаётся solver.

Результат B8at: host-переменные `sourceStartupActive/sourceStartupSeconds`
и повторные alpha-формулы удалены. `source::GameModeStartupState` исполняет
точный `GameMode::_startUpTime`: delay/fade/hold двух логотипов, пустой
интервал 6–7 s, переход в `-2`, один loading frame, затем `-3/StartGame`.
Escape вызывает тот же `-2`, а не перескакивает прямо в меню. Active renderer
получает только два alpha и команды load/start; StartOptions/discrete-video и
CoreText/bgfx остаются backend/application границами. Regression закрепляет
все стадии и одноразовый StartGame edge.

Результат B8au: `GameMode::_movieTime` перенесён в
`source::GameModeMovieState`. Menu callback теперь только выбирает файл и
completion; owner выдаёт prepare-window frame 0, video-mode/music-pause frame
1, wait 2–5, Open/Play/ResetInput frame 6, ожидание backend на 7, completion
tail 8–11 с Unload на 9 и ExitVideoMode/`cVideoStopped` на 12. SDL/AVFoundation
исполняют команды; skip/complete/error больше не перепрыгивают сразу к меню.
Regression проверяет каждый transition и отсутствие раннего Play/Unload.

Результат B8av: `Environment` больше не является приватным frame owner
`OriginalRaceRenderer`. Active `WorldEventPump` владеет общей source
Environment-ссылкой и вызывает `ProcessScene(deltaTime)` строго после
ordered `FrameEvent`, до network/control/GameMode, как `World.cpp:279–301`.
Renderer оставляет только backend-neutral snapshot текущей description,
camera style и позиции; этот snapshot обрабатывается World на следующем
кадре, что совпадает с Windows-порядком, где `CameraManager::Control::OnInputFrame`
также идёт после Environment. Garage/Angar presentation renderers сохраняют
локальный owner, поскольку не входят в active race World. Regression
проверяет World-owned rain follow и pause gate.

Результат B8aw: общий `snd::Source3d` восстановлен как backend-neutral
`OriginalSource3d`, владеющий play intent и текущим SDL voice. Постоянные
`SoundMotor` idle/rpm и `PxWheelSlipEffect` больше не разделены между raw
handle, boolean proxy state и глобальным teardown registry. Source owner
повторяет `<30` start, 30..45 retain, `>45` Stop/restart, source×resource
gain, RPM pitch, loop/once и natural `pmOnce` completion; CoreAudio/SDL
остаётся decoder/mixer boundary. Regression проверяет lifecycle и move без
double-stop. Transient Shot/Life/Contact emitters остаются следующим
крупным блоком того же owner migration.

Результат B8ax: `OriginalSource3d` дополнен точным
`Proxy::Streaming::Stop/GetPos/SetPos` cursor lifecycle. Stop-lag сохраняет
PCM frame и resume передаёт его SDL backend; source rewind 0 используется
для нового wheel slip, motor и каждого `ShotEffect::OnShot`. Оставшиеся raw
Shot/Life/PairContact voice records заменены общим owner. Shot живёт на
slot/sound, Life — до effect death, Contact — до pair release через 0.1 s,
включая молчащий state после `pmOnce` EOF. RAII заменяет ручные Stop ветви на
race transition/disconnect; CoreAudio остаётся mixer boundary. Regression
проверяет два сохранённых cursor, seek 0, EOF и single-owner teardown.

Результат B8ay: возвращён обычный `snd::Source` owner. `OriginalSource`
хранит category, sound/resource/source gain, frequency, loop/once, pause,
PCM cursor и SDL Proxy handle. Menu SoundSheme больше не управляет raw voice:
все девять cues проходят один Effects owner через
Stop→SetSound→SetPos(0)→Play. Commentator queue/replace/EOF Next использует
один Voice owner и тот же pause/rewind lifecycle. Regression проверяет
category, gain/pitch, 33→77 resume, rewind 0 и teardown. Последняя direct
`audio.play` ветка удалена из active host после классификации всех текущих
EffectSound как Shot/Life/PairContact; MusicCat остаётся отдельным source
playlist/streaming классом, SDL/CoreAudio — mixer boundary.

Результат B8bi: `DamageEffect::OnDamage` и
`ImmortalEffect::OnImmortalStatus` снова владеют отдельными car-lifetime
Source3d callbacks. Loader хранит type-5/type-11 sounds отдельно от
LifeEffect sounds порождённого actor; Damage выполняет Stop/rewind на каждом
подходящем damage callback, Immortal — rewind при status=true. Host owner
следует за car и сохраняется после once EOF до уничтожения поведения.
Shipped каталоги доказанно пусты (`<sounds />`, закомментированные bullet
AddSound), поэтому порт не добавляет искусственный cue; shield bonus по-прежнему
звучит через исходный `Snd\\shieldOn` Death/LifeEffect путь.

Результат B8bj: возвращены `Logic::GetVolume`, `SetVolume`,
`AutodetectVolume` и `Mute` для исходных `scMusic/scEffects/scVoice`.
`source::Logic` снова раздельно хранит persistent gain и mute-флаг каждой
категории: изменение громкости во время mute не включает звук, а unmute
восстанавливает последнее значение. `GameMode::Pause` теперь действительно
вызывает Effects mute у этого owner-а; прежний session pause-суррогат удалён.

Initial config, Options live preview/cancel/apply, race start и finish music
fade получают итоговый gain из `Logic`. SDL/CoreAudio только выставляет
соответствующий bus volume и больше не решает, какое сохранённое значение
нужно восстановить. Regression проверяет исходные autodetect 1.2/0.8/1.2,
SetVolume под mute и активный pause→zero→resume path RaceSession.

Результат B8bk: удалена host-переменная, изображавшая
`GameMode::_fadeMusic`. `GameModeMusicFadeState` воспроизводит буквальные
`FadeInMusic`/`FadeOutMusic` (исторические имена ставят target 0/1), optional
source volume, `_fadeSpeedMusic` и формулу каждого `OnFrame` с clamp 0..1.
`GameModeState::OnFinishFrameClose` теперь сам начинает переход 0→1; host
применяет произведение этого source-volume и `Logic::scMusic` к SDL bus.

Начало гонки больше не принудительно сбрасывает fade в 1 — оригинальный
`PlayMusic` сохраняет текущую громкость общего source. Pause и movie frame
останавливают interpolation, а Options меняет только category gain и не
обходит source gain. Regression закрепляет target/speed, последовательность
0→0.25→0.4375, inactive gate, обратный target и finish-close ownership.

Результат B8bl: `OriginalGameModeMusicSource` возвращает единственный
`GameMode::_music`, который Windows разделяет между `_menuMusic` и
`_gameMusic`. Каталоги сохраняют независимые playlist/current/cursor, но
`PlayMusic` всегда выполняет глобальный Stop, меняет report-owner и создаёт
ровно один backend voice. Natural EOF обрабатывает только текущий MusicCat;
заменённый каталог не может выполнить ложный Next или перезапустить музыку.

Menu pause/resume сохраняет PCM frame на общем source, race Play заменяет
его тем же способом, а FinalMenu остаётся на отдельном owner. Integrated
smoke закрепляет shared identity и mutual exclusion voices, а fake backend
regression — owner A→B, cursor, natural EOF и global Stop. SDL/CoreAudio
остаётся decode/mixer boundary.

Результат B8bm: исправлена пропущенная ветвь `GameMode::Run(false)`. Даже без
Yard/Laboratoria owner выдаёт исходный пустой `_startUpTime=0` кадр, затем
`-2/startLogo` и `-3/PrepareGame→StartGame→FreeIntro`; повторный Run после
start guard не действует. Полная intro-ветвь выдаёт те же prepare/free edges.

`GameModeStartupMenuState` владеет `_prefCameraAutodetect` и
`_discreteVideoChanged`, их строгим else-if приоритетом и повторным Check
после закрытия StartOptions. Host больше не очищает эти flags вручную.
Дополнительно menu dispatch принимает directional actions только при
`InputMessage::ksDown`: SDL key-up не двигает selection/stepper второй раз.
Unit и active startup/StartOptions/audio smokes закрепляют оба Run пути,
one-shot GPU commands и реальный одинарный Multiplayer navigation.

Результат B8bn: устранено параллельное владение глобальными `MapObj` ID.
Импортёр больше не записывает ID в `ObjectInstance`, `BonusInstance`,
`Racer` и `Race`, не подсчитывает невоплощённые категории и не резервирует
диапазон перед созданием машин. Публичный overload с принудительным ID также
удалён: `Map::AddMapObj` и `Map::InsertMapObj` назначают следующий ID только
после успешного разрешения record и вставки, как исходный
`MapObjList::InsertItem`.

Active session создаёт Decoration→Track→Bonus→Car через один live owner.
Сетевые damage/bonus events берут ID прямо из живого `MapObj`, а не из
parser-descriptor. Проверены все 190 bundled `.r3dMap`: неиспользуемые в
этом порядке `ctEffects/ctWeapon/ctCar/ctWaypoint` пусты. Regression
закрепляет реальную последовательность map1 1..234, 235..286, 287..293,
машины с 294, а также отсутствие пропуска ID после отклонённого record.

Результат B8bo: устранён `OriginalRaceSession::racerMapObjects_`. В Windows
`Player::CarState::mapObj` создаётся самим `Player::CreateCar` через
`Map::AddMapObj`, связывается с `Player` и удаляется в `FreeCar`. Portable
`Player` теперь хранит тот же live pointer и выполняет весь этот lifecycle;
session больше не создаёт, не удаляет и не восстанавливает вторую identity.

Смерть, строгая двухсекундная задержка, `CreateCar(false)→ResetCar`, network
disconnect и `Race::ExitRace` идут через одного владельца. Weapon target,
homing/impulse, mine/death target и сетевой поиск читают MapObj прямо у
`Player`. Порядок полей session исправлен так, чтобы Map/DataBase переживали
деструкторы Player. Regression проверяет bind `MapObj↔Player↔gameCar`,
удаление ID, выдачу нового ID при respawn и окончательное удаление при
Destroy; активный 300-frame Jolt/Metal заезд проверяет шесть машин.

Результат B8bp: `decorationActive_` и `decorationLife_` больше не являются
параллельным gameplay owner. Collision contacts, Laser/FrostRay, reset-car
ray, projectile hits и damage path читают `MapObj`,
`GameObject::LiveState` и `DestrObj::GetLife` непосредственно из category
`MapObjects`. Удаление через `ProgressOne` либо `Map::Clear` само исключает
объект из всех игровых запросов.

Совместимые vector getters сохранены для renderer/smoke, но каждый раз
строят snapshot из живого object graph. Внутренние ray helpers принимают
live predicate и не выполняют O(N) snapshot на каждый запрос. Existing
destruction/network regressions закрепляют удаление actor, отсутствие
повторной death emission и authoritative life/death state.

Результат B8bq: bonus category также избавлена от параллельного owner.
Pickup, mine/oil/slow/speed contacts и network replay используют live
`MapObj/AutoProj` и `GameObject::LiveState`; arming scale берётся из
`AutoProj::GetModelScale`. `bonusActive_`/`bonusScales_` существуют только
как snapshots для renderer/smoke.

Jolt sensor следует за source lifetime: source death выдаёт destroy command,
но inactive backend state не убивает бонус. Для ещё живого AutoProj sensor
пересоздаётся с новым backend ID. Regression закрепляет это направление
ownership вместе с исходными pickup/hazard/network/arming переходами.

Результат B8br: `ProjectileRuntime::active` и `MineRuntime::active` больше
не владеют transient lifetime. Единственный owner — зарегистрированный в
`Logic` concrete `source::Proj`; gameplay проверяет registry и
`GameObject::LiveState`, а impact/timeout/contact выполняет source Death до
actor teardown и удаления runtime view.

Публичные runtime getters обновляют `active` только для renderer/smoke.
Потеря Jolt actor живого projectile/mine приводит к одной create-команде с
новым backend ID, а не к смерти source object. Две integrated regression
закрепляют это направление ownership.

Результат B8bs: хвост `Race::CompleteRace(const Results*)` больше не
выполняется вручную в 22k-line SDL host. `OriginalRaceSession` атомарно
публикует live `Player`/achievement state, вычисляет исходные
`Race::GetTotalPoints` и число Human/Opponent, вызывает
`Tournament::CompleteTrack` и при завершении прохода очищает points всех
оставшихся в source PlayerList участников. Одноразовый флаг принадлежит
этому же Race-owner, поэтому повторное сохранение профиля не продвигает
турнир второй раз.

Главный файл теперь получает только готовый `TournamentAdvance`, сохраняет
профиль и выбирает следующий UI frame. Regression запрещает settlement до
финиша Human, проверяет live money/points, pass reset и one-shot вызов.

Результат B8bt: параметры scene-query больше не собираются вручную в
`OriginalRaceSession`. Concrete `source::Proj` формирует исходные запросы
`LaserUpdate`/`FrostRayUpdate` (offset `sizeAddPx`, projectile group и
`maxDist`), `RocketUpdate` (`pos + Z*4`, только TrackPlane) и
`MinePrepare` (`pos + Z*2`, TrackPlane | ShotTrack).

Jolt raycast получил отдельную идентичность `cdgShotTrack`: в эту группу
попадают только actor-ы размещённых mine projectiles, а обычные projectile и
bonus sensors её не загрязняют. `MinePrepare` снова отклоняет ближайшее
попадание по уже существующей мине без расхода charge. Headless OBB fallback,
Jolt smoke и integrated race regression закрепляют одинаковое правило.

Результат B8bu: оставшаяся часть `Race::CompleteRace` перенесена из session
в `source::RaceLifecycle`. Он теперь публикует один `Result` в concrete
`Player` точным порядком `SetFinished/SetPlace -> ResetPickMoney ->
SetBlockTime(0.3)` и после завершения всех участников ровно один раз выдаёт
campaign `money + captured pickMoney` и points.

Session оставляет только отмену pending backend attacks, очистку input и
`AIPlayer::FreeCar` adapter. Отдельный `campaignRewardsApplied_` удалён из
session; one-shot принадлежит Race lifecycle. Также удалён придуманный
`Player::ApplyRaceReward`, отсутствующий в Windows API и читавший pickMoney
уже после исходного reset. Regression проверяет разный порядок финиша,
captured pickMoney, block, все три награды и запрет повторного начисления.

Результат B8bv: аудит `Player::CarState::Update` подтвердил точный перенос
tile search, linked-node validation, lap wrap, wrong-way distance,
lost-control window и map position. Ошибка находилась не внутри этого класса,
а в оставленной вокруг него второй модели прогресса: session отдельно вела
`Player::nextPathNode`, создавала отсутствующее в Windows событие
`Checkpoint` и повторно обходила parser trace arrays.

Суррогаты и неиспользуемые `tracePathAt/tracePoint/racerTraceNode` удалены.
Game debug теперь показывает живой `CarState::GetLastNodeRef`, а Metal AI
smoke измеряет исходный `CarState::GetLap()` вместо придуманной дроби
`numLaps + nextPathNode/pathSize`. В session осталась только адаптация
исходного `lapPassed` в `Race::OnLapPass`/публичные race events.

Результат B8bw: удалена вторая модель выбора primary weapon из `Player` и
`OriginalRaceSession`. В Windows `_curWeapon` принадлежит `HumanPlayer`, а
конкретный `WeaponItem` передаётся в Shot напрямую; portable path теперь
соблюдает тот же owner. `HumanPlayer::{ChangeWeapon,SelectWeapon}` единолично
меняют текущий слот, HUD читает его через session boundary, а AI, network,
Shot1..4 и ShotAll передают исполняемый slot в `fireWeapon` без временного
изменения human selection.

Удалены отсутствующие в Windows `Player::selectedWeaponSlot`,
`selectedWeapon`, `ammunition`, `speedBoostSeconds` и
`SyncSelectedWeapon`. Последний был особенно опасен тем, что direct shot и
AI могли менять HUD selection, а dead boost-ветвь могла принудительно
подменять throttle. Reset session восстанавливает constructor `_curWeapon=0`.
Regression закрепляет next/previous, direct-slot selection preservation,
ShotAll, AI/network slot execution и live WeaponItem charge.

Результат B8bx: завершён соседний profile/loadout bridge. Удалены публичные
`Player::{weaponCharges,weaponCapacity,hyperCharge,hyperCapacity,mines,
mineCapacity}` — после первоначального `BindWeaponItems` они не обновлялись и
образовывали ложную вторую копию `_curCharge/_cntCharge`.

Profile, roster и default catalog теперь строят одноразовый
`Player::WeaponLoadout`. `BindWeaponItems` атомарно переносит из него record
indices и начальные charge в concrete `WeaponItem`; дальше bonus, Shot,
Reload, HUD, AI и profile save читают только source item. Regression
проверяет primary/hyper/mine, Droid/Reflector, reload и ammunition order.

Результат B8by: мировой transform projectile/mine возвращён concrete
`source::Proj`. Оригинальные `ProgressAttached`, `ProgressFree` и
`ProgressPlacedMine` уже обновляли source `GameObject`, но обратная
синхронизация Jolt меняла только session view. Теперь каждый принятый Jolt
body state немедленно публикует position/rotation в `Proj`, а renderer
snapshots перед выдачей читают transform обратно из source owner.

`ProjectileRuntime`/`MineRuntime` сохраняют pose только как backend/render
adapter для команд Jolt, swept contact и Metal submit. Regression двигает и
вращает активные projectile и mine через synthetic Jolt state и требует
точного совпадения `Proj::GetWorldPos/GetWorldRot`.

Результат B8bz: удалено второе live-хранилище homing/Impulse target из
`ProjectileRuntime`. В Windows цель принадлежит `Proj::_shot` и очищается
его listener-ом при уничтожении target `MapObj`; индекс session переживал
respawn и мог самопроизвольно начать означать новую машину того же racer-а.

`OriginalRaceSession` теперь разрешает backend racer index только из
`Proj::GetSourceTarget()` и текущей source Map identity. Initial target
создаётся исключительно через `Weapon::ShotContext`, Impulse retarget —
исключительно через `Proj::RetargetImpulse`. Regression AI, torpeda и
Impulse проверяет сам source pointer, а не удалённое зеркало.

Результат B8ca: визуальный arming scale масла снова принадлежит включённой
модели concrete `Proj`. Windows `MasloPrepare/MasloUpdate` меняют
`_model->GameObject::scale`; session больше не копирует результат в
`MineRuntime::armingAlpha`, а Metal renderer читает живой local scale
исходной модели.

Удалены также неиспользуемые портовые `ProjectileRuntime::distance` и
`MineRipChildSpawn::armingScale`. Первое только накапливалось и не
существовало в original gameplay, второе никогда не применялось и ошибочно
приписывало core/piece scale исходному split plan. Distance regression теперь
сравнивает реальные source world positions.

Результат B8cb: удалён второй live scalar скорости projectile. Оригинальный
`Proj::CalcSpeed` создаёт один velocity vector и передаёт его PhysX actor;
отдельного `Proj::speed` в Windows нет. Jolt boundary теперь аналогично
хранит только `ProjectileRuntime::velocity`.

Homing, reflection, attached fire, ballistic gravity, spawn preview и
регрессии вычисляют длину этого вектора по месту. Удалён портовый минимум
скорости 1 для свободного actor-а: нулевой source/Jolt velocity больше не
создаёт выдуманное самодвижение.

Результат B8cc: из session удалены производные `ballistic` и
`linkedToOwner`. В исходном `Proj::MortiraPrepare` гравитация задаётся
единственным вызовом `RocketPrepare(weapon, false)`, а принадлежность оружию
хранится в `Proj::_weapon` и снимается `Proj::OnDestroy -> SetWeapon(0)`.

Jolt create/synchronize и fallback integration теперь каждый раз получают
ballistic route из `Proj::RoutePreparation()`. Проверки автономных MineRip
fragment-ов читают `Proj::GetSourceWeapon()`, поэтому невозможна комбинация,
в которой session считает мину отсоединённой, а source listener graph — ещё
связанной. `attached` оставлен только как backend transition state:
он фиксирует необходимость пересоздать ранее kinematic Jolt body как dynamic
после того, как source listener уже очистил оружие.

Результат B8cd: перенесена недостающая graph-часть
`Proj::LaserUpdate`. В Windows concrete `Proj` меняет позицию и размеры
primary `Sprite`, позицию `_model2` и scale первого sampler-а. Portable
`Proj::ProgressLaser` раньше владел только `_model2`, а distance/width/UV
копировались в `ProjectileRuntime`.

Теперь `Proj::LaserVisualState` представляет отсутствующий portable Graph
Sprite и обновляется в том же source-вызове, что damage и impact model.
Metal читает только этот state; три session-поля удалены. Также восстановлена
ветка `distort`: FrostRay меняет длину геометрии, но, как и в оригинале, не
записывает laser UV scale.

## Правило обновления карты

Каждый крупный block commit обязан:

1. назвать точные методы из `eff9338:prog`, которые стали активными;
2. указать новый source owner и оставшийся backend boundary;
3. добавить regression на source state/order;
4. обновить строку таблицы и статус блока;
5. пройти arm64 build, offline CTest, network CTest, physics smoke и Metal
   race-render smoke, если block касается активной гонки.
