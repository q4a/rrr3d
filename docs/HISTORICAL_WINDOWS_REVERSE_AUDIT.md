# Полный обратный аудит macOS-порта относительно Windows-исходников

Дата аудита: 2026-08-24

Проверенная ветка: `macos-arm64`

Проверенный HEAD: `be0170d0de7a03f2147d404ef8dfca4416c8461e`

## Краткий вывод

Аудит подтвердил главную проблему происхождения порта: ветка macOS была
создана от `dxvk-glm`, а не непосредственно от неизменённого Windows-кода.
Однако вывод «в `dxvk-glm` была отключена вся физика» неверен. В этой ветке
сохраняются `NxWheelShape`, `GameCar`, PhysX scene/contact code и исходная
vehicle physics. Явно отключён только `FxPhysicsEmitter`. Основной риск
`dxvk-glm` — массовая замена D3DX math на GLM с последующими ручными
исправлениями порядка умножения матриц и кватернионов.

Более существенное расхождение появилось в самом macOS-пути. Финальная arm64
сборка не компилирует ни один из 34 исходных файлов
`Rock3dGame/source/game/*.cpp`, исходный D3D9 graph, PhysX runtime,
`snd/Audio.cpp` или DirectShow video runtime. Вместо этого она запускает
отдельную крупную `Original*`/bgfx/Jolt/SDL state machine. Она читает реальные
ресурсы и переносит много правил из исходника, но это повторная реализация, а
не прямой перенос исходного object graph.

Точная граница в Git — `59fc40f1a8e8c5050579048e0d66d49aac9895e4`
(`macos: checkpoint corrected milestone 5 renderer slice`). Его parent —
непосредственно `bfbde0d`; одним checkpoint в историю вошли non-Windows
`source/stub`, portable renderer/physics/menu и условный CMake build graph.
Именно к parent этого commit следует возвращаться для архитектурного
перезапуска, сохраняя последующие adapters как справочный материал.

Поэтому текущий результат корректно называть работающим native arm64
source-derived портом с большим покрытием оригинальных данных, но нельзя
называть доказанным полным портом Windows-игры. В текущем HEAD также
подтверждены пять конкретных расхождений игровой логики и одно расхождение
диагностического API.

## 1. Какая версия является эталоном

Репозиторий: `https://github.com/q4a/rrr3d.git`.

| Точка | Commit | Назначение |
| --- | --- | --- |
| Базовый опубликованный source | `c9278732788bf5731ea66cf4337d5a3386e79cfc` | `GAME: version 1.3 base project source`; содержит исходную игровую реализацию |
| Полный исторический reference | `eff933868c1fbdfd266738a403fac80084f2b51e` | текущий remote `master`; добавлены старые зависимости и exporter, игровая реализация практически не менялась |
| Конец модернизации | `ec50208d6ed1d8435eb97e98086b8d8bf23bc7c5` | VS2019/CMake/move/format до DXVK/GLM-работ |
| Исходная точка macOS | `bfbde0d245dcdb201640be9219f7bad58b2b0ed2` | tip `dxvk-glm`; именно от него происходит `macos-arm64` |

`git merge-base eff9338 bfbde0d` возвращает `eff9338`. Remote-проверка на
дату аудита возвращает `master = eff9338` и `dxvk-glm = bfbde0d`.

Для семантики игры использовались файлы из `eff9338:prog/...`. В диапазоне
`c927873..eff9338` исходники `Rock3dGame` и `Rock3dEngine` не менялись;
изменения затронули главным образом добавление зависимостей, exporter,
project files и один utility-файл LexStd. Поэтому `eff9338` удобнее как
воспроизводимый reference, не теряя исходную game logic.

### Историческая среда сборки

Локальные файлы самого reference подтверждают:

- Visual C++ project format `9.00`, то есть Visual Studio 2008;
- только `Win32` configurations;
- PhysX `2.8.4`, build `4824`;
- DirectX SDK version `9.29.1962.0`;
- Boost `1.53`, библиотеки `vc90`;
- target Windows Vista API (`WINVER/_WIN32_WINNT = 0x0600`).

Строка меню исходника — `v. 1.2.0`, то есть она совпадает с оставленной в
Parallels Windows-копией. Это подтверждает совместимость reference по
видимой версии, но не является доказательством побитового соответствия
опубликованного source конкретному `MR.exe`.

## 2. Что изменилось до начала macOS-порта

### 2.1 Модернизация `eff9338..ec50208`

Диапазон содержит девять commits. Основные действия:

- минимальные VS2019 compatibility fixes;
- создание VS2019 project files;
- удаление из Git старых third-party headers/libraries;
- перемещение `prog` в `src`;
- замена solution на CMake;
- выделение MathLib;
- форматирование.

Raw diff показывает 715 файлов и около 184 тысяч удалённых строк, но почти
всё это удалённые SDK/dependency trees, а не вырезанная игра. Рассматривать
этот raw diff как потерю 184 тысяч строк игровой логики нельзя.

### 2.2 Изменения `ec50208..bfbde0d` (`dxvk-glm`)

Диапазон содержит 53 commits, 288 изменённых файлов, 9835 добавлений и 14697
удалений. В `Rock3dEngine`/`Rock3dGame` затронуто 186 файлов, включая все 34
исходных game `.cpp`.

Основные классы изменений:

1. добавление DXVK как external project и Linux/XPlatform shims;
2. последовательная замена `D3DXVECTOR2/3/4`, `D3DXQUATERNION`,
   `D3DXCOLOR`, `D3DXPLANE`, части `D3DXMATRIX` и D3DX functions на GLM;
3. ручные исправления после механических замен:
   - `math: fix multiplication order`;
   - `math: fix items rotation in main menu`;
   - `math: fix car rotation, replace glm::mix with glm::slerp`;
   - `math: fix black 3d scene after replace D3DXCOLOR`;
4. Linux-заглушки D3D interfaces;
5. комментирование `FxPhysicsEmitter` в `bfbde0d`.

Это не нейтральная смена build system. Порядок матричного/кватернионного
умножения менялся в camera, car, game objects, weapons, GUI и scene graph.
Даже если исправления были правильными для GLM conventions, `dxvk-glm`
нельзя использовать как единственный oracle оригинального Windows-поведения.

Одновременно проверка отвергла предположение об отключении основной PhysX
vehicle physics: в `bfbde0d` остаются `NxWheelShape`, contact modify/report,
PhysX scene и весь `GameCar.cpp`. Отключённый `FxPhysicsEmitter` относится к
particle/effect emitter, а не к приводу, колёсам или chassis автомобиля.

### 2.3 Точка начала неправильного macOS-направления

Первый macOS commit после `dxvk-glm` — `59fc40f`; промежуточных commits между
ним и `bfbde0d` нет. Он меняет 1318 файлов, добавляет ресурсы и одновременно
создаёт новый non-Windows runtime:

- `Rock3dGame` на non-Windows начинает собирать `source/stub/Portable*`;
- `Rock3dEngine` выбирает `PortableEngine`, bgfx и
  `MinimalVehiclePhysics` вместо legacy graph/PhysX;
- executable выбирает `main_bgfx*`/`main_sdl`, а не исходный `RRR3d.cpp`;
- публичный legacy `CreateWorld/IWorld` становится недоступным.

Таким образом, ошибка не началась в одном позднем AI, renderer или physics
fix. Она зафиксирована в самом первом macOS checkpoint: платформенные
backends были введены вместе с параллельной game implementation. Последующие
milestones сильно улучшили эту implementation и заменили minimal physics на
Jolt, но не вернули legacy classes в active target.

## 3. Что реально компилируется в arm64 Debug

Проверен фактический `build/macos-arm64-m10/build.ninja` и CMake cache, а не
только наличие файлов в дереве. Конфигурация: `Debug`, `arm64`, bgfx, Jolt,
SDL input/audio, AVFoundation video и NetLib включены; Steam и MapEditor
выключены.

Активные верхнеуровневые реализации:

- engine: `ProgressTimer`, `R3DMeshAsset`, `ResourceFileSystem`,
  `PortableEngine`, `BgfxGraphicsDevice`, `JoltVehiclePhysics`;
- game: `MusicCat`, `PortableGame`, `OriginalMainMenu`, `OriginalProfile`,
  `OriginalGarage`, `OriginalRace`, `OriginalRaceSession`,
  `OriginalNetwork*`, `OriginalUserChat`;
- executable: `main_bgfx_original_menu`, `OriginalRaceRenderer`,
  `OriginalWorkshopRenderer`, `OriginalRaceHud`, audio/commentator/spatial
  adapters, SDL input/audio, CoreText and `MacVideoPlayer`;
- напрямую переиспользованы utility/transport layers LexStd, MathLib, NetLib
  и XPlatform.

Не входят в arm64 executable/library graph:

- все 34 `src/Rock3dGame/source/game/*.cpp`;
- все 30 legacy `Rock3dEngine/source/graph/*.cpp`;
- исходные `Rock3dEngine/source/px/*.cpp`;
- `Rock3dGame/source/snd/Audio.cpp`;
- `Rock3dGame/source/video/{VideoPlayer,playback,video}.cpp`;
- `Rock3dGame/source/net/{NetGame,NetRace,NetPlayer,SteamService}.cpp`;
- MapEditor.

После ответвления от `dxvk-glm` legacy game tree изменён только в
`MainMenu2.cpp` и `Menu.cpp` (55 добавлений, 43 удаления). Эти правки не
влияют на активную macOS-сборку, потому что оба файла в неё не входят.

Non-Windows CMake по-прежнему определяет
`RRR3D_PORTABLE_GAME_STUBS=1` и `RRR3D_PORTABLE_ENGINE_STUBS=1`. Это не
означает, что вся игра сейчас пустая, но точно отражает факт обхода legacy
`IWorld`/engine object graph.

## 4. Ресурсы игры

Тезис «текущий порт не использует ресурсы игры» для HEAD `be0170d` не
подтвердился.

`legacy-assets.catalog` содержит 1196 импортированных файлов Motor Rock
общим размером 553107397 bytes. Ещё четыре portable-файла — сам catalog,
`manifest.cfg`, `menu/menu.cfg` и `ui/font5x7.txt`. Всего в разрешённом bundle
находится 1200 файлов, включая:

- 330 `.r3d` meshes;
- 95 `.r3dMap` maps;
- 309 DDS и 213 PNG;
- 23 исходных `.fx`;
- 182 Ogg;
- 14 AVI;
- исходные XML и локализации.

Текущий smoke действительно загрузил map1, 52 placed track objects, 1175
collision triangles и исходную Marauder description. Следовательно,
проблема не в отсутствии данных. Проблема в том, что data-driven поведение
интерпретируется новой state machine и новыми backend-ами.

## 5. Матрица обратного паритета

Обозначения:

- **Direct** — тот же или переносимый исходный код входит в target;
- **Platform** — сменён только платформенный backend, policy должна быть
  проверена отдельно;
- **Reimplemented** — поведение вручную воспроизведено в новых классах;
- **Absent** — исходная система отсутствует в продукте.

| Подсистема Windows | Текущий macOS-путь | Класс | Итог аудита |
| --- | --- | --- | --- |
| LexStd/часть MathLib | текущие `src/LexStd`, `src/MathLib` | Direct после GLM-модификаций | Переиспользуется, но math уже происходит от `dxvk-glm`, а не от pristine D3DX |
| NetLib transport | Boost.Asio NetLib | Direct с platform fixes | TCP/UDP layer реально компилируется и проходит loopback |
| `World`, `GameMode`, `Race`, `Player`, `GameObject` | `OriginalRace*`, `OriginalProfile`, main runtime | Reimplemented | Legacy component/listener ownership, event ordering и общий object graph не перенесены напрямую |
| AI (`AICar`, `AIPlayer`) | блоки в `OriginalRaceSession` | Reimplemented | Много source branches перенесено, но управление и state находятся в другой архитектуре и другом physics feedback loop |
| Weapons/bonuses/damage/achievements | typed records/state в `OriginalRaceSession` | Reimplemented | Использует source enums/XML и имеет большое smoke-покрытие; не тот call graph/callback order |
| Menu/GUI/HUD/mini-map | ручной `MenuScreen`, `OriginalMainMenu`, `OriginalRaceHud`, 20k-line entry point | Reimplemented | Ресурсы/layout частично source-matched; legacy Widget/MenuSystem graph не компилируется |
| Profiles/garage/workshop | `OriginalProfile`, `OriginalGarage`, ручные frames | Reimplemented | Исходные XML/rules используются; persistence и transitions не исполняют исходные classes |
| D3D9 graph/material/effects | bgfx/Metal + 32 shader source files | Platform + Reimplemented | 23 `.fx` присутствуют как данные, но D3D9 effects не компилируются и не выполняются; passes/material mapping перенесены вручную |
| PhysX 2.8.4 vehicle/contact runtime | Jolt 5.5 adapter | Platform + Reimplemented | Source parameters/rules перенесены частично; tire, suspension, collision solver и callback ordering численно другие |
| XAudio2/X3DAudio graph | SDL/CoreAudio mixer и ручная race audio policy | Platform + Reimplemented | Оригинальные Ogg и многие правила используются; точная X3DAudio DSP matrix/object lifecycle не перенесены |
| DirectShow video | AVFoundation/AVPlayer | Platform | Все 14 роликов имеют native path; backend закономерно другой |
| Win32/XInput input | SDL input/actions | Platform + Reimplemented | Работает, но исходный `ControlManager.cpp` не входит в target |
| NetGame/NetRace/NetPlayer model graph | `OriginalNetwork*` поверх прямого NetLib | Reimplemented | LAN и основные RPC перенесены; legacy model/event classes не компилируются |
| Steam/auth/achievements backend | `RRR3D_ENABLE_STEAM=OFF` | Absent | Не портировано |
| MapEditor | `RRR3D_BUILD_MAP_EDITOR=OFF` | Absent | Не входит в пользовательскую игру, но не является перенесённым продуктом |
| Legacy public `CreateWorld/IWorld` | `PortableGame::CreateWorld` бросает exception | Absent | Финальный entry point обходит публичный legacy world API |

### Renderer boundary

Текущий renderer не является заглушкой: он реализует shadow splits,
environment cube, reflection/refraction, HDR/bloom/tone map, water, fog,
grass, sky, sun shafts и source-derived material modes. Однако это вручную
написанные bgfx shaders/passes, а не перенос `GraphManager`, `SceneManager`,
`Actor`, `MaterialLibrary`, `MappingShaders` и D3D9 effect runtime. В
результате остаются недоказанными render-state inheritance, sorting,
lifetimes, occlusion/culling corner cases и точное совпадение shader math.

### Physics boundary

Jolt adapter использует исходные vehicle/XML parameters и переносит много
правил `GameCar`, но исходный PhysX solver не выполняется. Известные
намеренные отличия включают отключённую Jolt transmission propagation,
прямую подачу source torque на driven wheels, пропуск low-speed `restTorque`,
scalar friction вместо PhysX anisotropic material и Jolt-specific solver
order. Это допустимые engineering adaptations, но они требуют trace/A-B
валидации и не могут считаться автоматическим физическим паритетом.

## 6. Подтверждённые конкретные расхождения HEAD

### RA-C01 — campaign reward начисляется только human runtime

**Подтверждено.**

Windows `Race::CompleteRace(const Results*)` проходит по всем `_results` и
вызывает `AddMoney`/`AddPoints` для каждого найденного `Player`. В
`OriginalRaceSession::processCheckpoint` деньги и points добавляются только
при `racer == 0`; `completeRemainingRacers` рассчитывает AI reward, но не
добавляет его в AI runtime state.

Влияние: cumulative state компьютерного участника расходится с Windows.
Насколько это видно между конкретными campaign races, требует runtime trace,
но кодовая семантика различается однозначно.

### RA-C02 — oil/Maslo прибавляет angular velocity вместо замены momentum

**Подтверждено.**

Windows `GameCar::StabilizeForce` переводит текущий angular momentum в
локальную систему, заменяет локальный Z на `_clutchStrength * mass`, затем
пишет полный momentum обратно. Portable session создаёт
`AngularVelocityRequest{0,0,strength}`, а Jolt backend вызывает
`AddLinearAndAngularVelocity`. Отличаются physical quantity, inertia/mass
семантика и операция replace/add.

### RA-C03 — border damage ошибочно зависит от скорости `> 16`

**Подтверждено.**

В Windows проверка скорости ограничивает только spring-border correction;
расчёт `touchDamage` выполняется после этой ветви. В portable code условие
`vehicles[racer].speed <= 16.0F` делает `continue` до применения damage.
Поэтому сильный низкоскоростной удар о border может не нанести исходный урон.

### RA-C04 — введён отсутствующий в Windows cooldown contact damage

**Подтверждено.**

Portable session создаёт матрицу `touchCooldown_` и после car-to-car damage
блокирует повторный damage на `0.25` секунды. Windows contact callback такого
cooldown не имеет. `dtTouch` — тип урона/attribution, а не имя таймера.

### RA-C05 — car collision energy игнорирует вращение

**Подтверждено после проверки старого PhysX SDK header.**

Windows сравнивает `NxActor::computeKineticEnergy()`. Заголовок PhysX 2.8.4
из `eff9338` прямо определяет его как total rotational and translational
energy. Portable code сравнивает только `0.5 * mass * speed^2`. При заметном
вращении может быть выбран другой attacker/target для touch damage.

### RA-C06 — capability API сообщает устаревшее состояние

**Подтверждено, низкий приоритет для active UI.**

`PortableGame::game_capabilities()` возвращает `video=false` и `audio=false`
даже в активной M10-конфигурации. `log_game_capabilities()` всегда пишет,
что video disabled и network race models pending. `PortableEngine` считает
physics включённой только при `RRR3D_PHYSICS_MINIMAL`, поэтому активный
`RRR3D_PHYSICS_JOLT` отражается как `physics=false`. Финальный main эти
диагностические функции обычно не вызывает, но публичные данные неверны.

## 7. Перепроверка прежних находок

| Прежний пункт | Новый результат | Основание |
| --- | --- | --- |
| F-004: finished AI car не освобождается | Не подтвердился как дефект | Windows `AIPlayer::FreeCar()` удаляет AI controller и выставляет `mcNone`, но не удаляет физический `Player::GameCar`; portable zero input + delayed brake соответствует фактической семантике |
| F-005: AI reward не накапливается | Подтверждён как RA-C01 | Прямое сравнение обоих finish paths |
| F-006: потерян 0.3 s finish block/brake | Уже исправлен | Portable сохраняет 0.3-second transition и затем full brake; есть regression |
| F-007: неверная oil angular semantics | Подтверждён как RA-C02 | Momentum replace против velocity add |
| F-008: border damage пропускается при низкой скорости | Подтверждён как RA-C03 | Скоростная проверка стоит на другом уровне ветвления |
| F-009: искусственный touch cooldown | Подтверждён как RA-C04 | В Windows таймера нет |
| F-010: `armor4` не действует | Уже исправлен | `OriginalRace` и `OriginalGarage` добавляют +10 до difficulty scale, tests присутствуют |
| U-001: неясна семантика `computeKineticEnergy` | Теперь подтверждён как RA-C05 | Исторический `NxActor.h` явно включает rotational + translational energy |

## 8. Что тесты подтверждают и чего не подтверждают

На этом HEAD выполнены:

- `ctest --test-dir build/macos-arm64-m10 --output-on-failure`: 4/4 passed;
- arm64 Debug `--physics-smoke-test`: passed;
- resource audit во время smoke: 1196 original assets, map1 и vehicle data
  успешно прочитаны.

Это подтверждает сборку, загрузку ресурсов, основные state transitions,
Jolt vehicle operation и network regression set. Все тесты проходят даже
при наличии RA-C01…RA-C05, потому что соответствующие проверки отсутствуют
либо закрепляют более грубый portable контракт. Smoke tests нельзя
использовать как доказательство полного Windows parity.

## 9. Границы доказательства

Статически нельзя доказать:

- побитовое соответствие опубликованного source конкретному Windows
  `MR.exe`;
- одинаковые PhysX/Jolt trajectory, contact timing и wheel slip;
- pixel parity D3D9/bgfx при всех material/effect states;
- точное audio timing/spatial matrix XAudio2/X3DAudio против SDL;
- идентичный порядок событий legacy object graph и объединённой session;
- полный parity всех 88 track × 17 car × weather/mode combinations.

Для этого нужен пересобранный Windows reference из `eff9338` и
детерминированный сравнительный trace. Обычный визуальный прогон полезен, но
недостаточен для AI, physics, lap/damage и audio state.

## 10. Состояние рабочего дерева при аудите

До создания этого отчёта tracked modifications отсутствовали. Найдены 1124
untracked файла: 1123 имеют суффикс ` 2`, ещё один — прежний
`docs/MACOS_PORT_SOURCE_PARITY_FINDINGS.md`. Также в `.git/refs` находятся
пять битых копий refs с ` 2`; из-за них некоторые команды с `--all` падают.

Эти пользовательские файлы и refs не изменялись и не включаются в audit
commit. Сравнения выполнялись по явным commit IDs, чтобы битые refs не могли
исказить вывод.

## 11. Рекомендуемый путь восстановления полного порта

### P0 — исправить доказанные расхождения

1. RA-C01: начислять campaign result каждому participant runtime/profile.
2. RA-C02: добавить backend operation «установить angular momentum в local
   Z» вместо additive angular velocity для oil.
3. RA-C03: вынести speed gate внутрь spring-border redirect, оставив damage
   снаружи.
4. RA-C04: удалить 0.25-second cooldown либо доказать его наличие в другом
   исходном слое; текущий Windows source его не содержит.
5. RA-C05: сравнивать translational + rotational energy через inertia tensor.
6. RA-C06: привести capability API к реальным build flags.

Для каждого исправления нужен source-counterexample regression, а не тест на
текущее portable поведение.

### P1 — получить исполняемый oracle

1. Собрать `eff9338` без модернизации в отдельной Windows VM:
   VS2008/Win32, PhysX 2.8.4 build 4824, DXSDK 9.29.1962, Boost 1.53 vc90.
2. Не смешивать эту сборку с `dxvk-glm` или VS2019/CMake.
3. Добавить одинаковый debug trace на Windows и macOS: per fixed tick pose,
   linear/angular momentum, wheel state, control command, AI state/path,
   checkpoint/lap/place, damage/events и audio commands.
4. Сравнивать одинаковые profile, RNG seed, track/car/weather и inputs.

### P2 — менять архитектуру переноса

Дальнейшее ручное расширение единого `OriginalRaceSession` повышает риск
новых расхождений. Для полного порта лучше постепенно компилировать
независимые от Windows исходные classes, выделяя узкие interfaces только на
границах D3D9, PhysX, XAudio2, DirectShow и Win32. `Original*` adapters можно
оставить как переходный слой и источник tests, но не как окончательное
доказательство переноса.

Рекомендуемый порядок object-graph migration:

1. `RecordLib`, `DataBase`, `Trace`, profile/tournament rules;
2. `Player`, `Race`, `Logic`, achievements без renderer/physics types;
3. AI state/controllers с abstract vehicle feedback;
4. weapons/game objects/event listeners;
5. menu/widget state graph;
6. renderer/material/effect graph;
7. physics contacts/wheels и audio graph;
8. network model graph и только затем Steam.

## Итоговое решение

Текущий порт не следует выбрасывать: в нём уже есть native platform
backends, оригинальные ресурсы и значительный объём перенесённых правил.
Но продолжать считать `Original*` названия доказательством исходности нельзя.
Эталоном для следующей работы должен быть `eff9338:prog/...`, а
`dxvk-glm` — только вспомогательной модернизированной веткой. Закрывать
подсистему как «портированную» следует лишь после прямого source mapping и
Windows/macOS trace comparison.
