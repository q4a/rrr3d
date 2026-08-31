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
называть доказанным полным портом Windows-игры. Первичный аудит выделил пять
предполагаемых расхождений игровой логики и одно расхождение
диагностического API. При implementation-перепроверке RA-C03 был снят:
вложенность скобок Windows-кода изначально была прочитана неверно. Остальные
четыре игровых расхождения и capability API исправлены следующим P0-этапом
и закрыты regression-проверками.

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
прямую подачу source torque на driven wheels, пропуск low-speed `restTorque`
и Jolt-specific solver order. Anisotropic кузовной contact из PhysX теперь
реализован узким расширением закреплённого Jolt. Остальные adaptations требуют trace/A-B
валидации и не могут считаться автоматическим физическим паритетом.

## 6. Конкретные расхождения audit HEAD и результат P0

### RA-C01 — campaign reward начисляется только human runtime

**Подтверждено; исправлено.**

Windows `Race::CompleteRace(const Results*)` проходит по всем `_results` и
вызывает `AddMoney`/`AddPoints` для каждого найденного `Player`. В
`OriginalRaceSession::processCheckpoint` деньги и points добавляются только
при `racer == 0`; `completeRemainingRacers` рассчитывает AI reward, но не
добавляет его в AI runtime state.

Влияние: cumulative state компьютерного участника расходится с Windows.
Насколько это видно между конкретными campaign races, требует runtime trace,
но кодовая семантика различается однозначно.

P0 переносит начисление в эквивалент завершения
`Race::CompleteRace(const Results*)`: после окончательного ранжирования
награды один раз применяются ко всем finished/non-disconnected runtime, а не
только к human в момент пересечения линии. Regression завершает всех
участников, проверяет money/points каждого и повторным update исключает
двойное начисление.

### RA-C02 — oil/Maslo прибавляет angular velocity вместо замены momentum

**Подтверждено; исправлено.**

Windows `GameCar::StabilizeForce` переводит текущий angular momentum в
локальную систему, заменяет локальный Z на `_clutchStrength * mass`, затем
пишет полный momentum обратно. Portable session создаёт
`AngularVelocityRequest{0,0,strength}`, а Jolt backend вызывает
`AddLinearAndAngularVelocity`. Отличаются physical quantity, inertia/mass
семантика и операция replace/add.

P0 добавляет отдельную backend-операцию `setAngularMomentum`. Oil/Maslo
переводит сохранённый momentum в локальную систему машины, сохраняет X/Y,
заменяет локальный Z на `strength * mass` и устанавливает полный мировой
momentum. Jolt regression проверяет установку физической величины, а race
regression — сохранение X/Y и замену Z вместо additive velocity.

### RA-C03 — предполагаемая ошибка speed gate border damage

**Не подтвердилось при implementation-перепроверке; изменение не требуется.**

В историческом `GameCar.cpp` условие
`borderContact && vel.magnitude() > 16.0f` открывается на строке 1048, а
`Damage(..., dtTouch)` на строках 1103–1104 находится до закрывающей его
скобки на строке 1105. Следовательно, скорость ограничивает и redirect, и
damage. Portable `speed <= 16.0F -> continue` семантически совпадает с этим
фрагментом. Первичный вывод возник из неверного чтения уровня вложенности в
длинной ветви spring-border correction и настоящим defect не является.

### RA-C04 — введён отсутствующий в Windows cooldown contact damage

**Подтверждено; исправлено.**

Portable session создаёт матрицу `touchCooldown_` и после car-to-car damage
блокирует повторный damage на `0.25` секунды. Windows contact callback такого
cooldown не имеет. `GameCar` включает `NX_NOTIFY_ALL`, поэтому PhysX
доставляет не только начало, но и продолжающийся touch; portable Jolt listener
аналогично записывает `OnContactAdded` и `OnContactPersisted`. `dtTouch` — тип
урона/attribution, а не имя таймера.

P0 удаляет матрицу `touchCooldown_` и 0.25-second gate. Regression подаёт
один и тот же подтверждённый manifold в двух соседних updates и проверяет,
что оба source contact callbacks наносят урон.

### RA-C05 — car collision energy игнорирует вращение

**Подтверждено после проверки старого PhysX SDK header; исправлено.**

Windows сравнивает `NxActor::computeKineticEnergy()`. Заголовок PhysX 2.8.4
из `eff9338` прямо определяет его как total rotational and translational
energy. Portable code сравнивает только `0.5 * mass * speed^2`. При заметном
вращении может быть выбран другой attacker/target для touch damage.

P0 сохраняет вычисленную Jolt rigid-body energy в `VehicleState` как
`0.5*m*v² + 0.5*omega·L`; session использует её для attribution. Для
синтетических backend-независимых states оставлен эквивалентный fallback по
box inertia. Regression задаёт медленной машине большую rotational energy и
проверяет выбор именно её как attacker.

### RA-C06 — capability API сообщает устаревшее состояние

**Подтверждено, низкий приоритет для active UI; исправлено.**

`PortableGame::game_capabilities()` возвращает `video=false` и `audio=false`
даже в активной M10-конфигурации. `log_game_capabilities()` всегда пишет,
что video disabled и network race models pending. `PortableEngine` считает
physics включённой только при `RRR3D_PHYSICS_MINIMAL`, поэтому активный
`RRR3D_PHYSICS_JOLT` отражается как `physics=false`. Финальный main эти
диагностические функции обычно не вызывает, но публичные данные неверны.

P0 передаёт audio/video flags библиотеке, распознаёт Jolt как активную
physics, исправляет устаревший network log и добавляет отдельный capability
smoke, сопоставляющий API с compile definitions.

## 7. Перепроверка прежних находок

| Прежний пункт | Новый результат | Основание |
| --- | --- | --- |
| F-004: finished AI car не освобождается | Не подтвердился как дефект | Windows `AIPlayer::FreeCar()` удаляет AI controller и выставляет `mcNone`, но не удаляет физический `Player::GameCar`; portable zero input + delayed brake соответствует фактической семантике |
| F-005: AI reward не накапливается | Подтверждён как RA-C01; исправлен в P0 | Прямое сравнение обоих finish paths и regression всех runtime |
| F-006: потерян 0.3 s finish block/brake | Уже исправлен | Portable сохраняет 0.3-second transition и затем full brake; есть regression |
| F-007: неверная oil angular semantics | Подтверждён как RA-C02; исправлен в P0 | Momentum replace против velocity add |
| F-008: border damage пропускается при низкой скорости | Не подтвердился; RA-C03 отозван | Windows damage находится внутри `speed > 16` branch |
| F-009: искусственный touch cooldown | Подтверждён как RA-C04; исправлен в P0 | В Windows таймера нет |
| F-010: `armor4` не действует | Уже исправлен | `OriginalRace` и `OriginalGarage` добавляют +10 до difficulty scale, tests присутствуют |
| U-001: неясна семантика `computeKineticEnergy` | Подтверждён как RA-C05; исправлен в P0 | Исторический `NxActor.h` явно включает rotational + translational energy |

## 8. Что тесты подтверждают и чего не подтверждают

На этом HEAD выполнены:

- `ctest --test-dir build/macos-arm64-m10 --output-on-failure`: 6/6 passed,
  включая `rrr3d_portable_capabilities_smoke` и отдельный
  `rrr3d_original_tournament_smoke`;
- arm64 Debug `--physics-smoke-test`: passed;
- resource audit во время smoke: 1196 original assets, map1 и vehicle data
  успешно прочитаны.

Это подтверждает сборку, загрузку ресурсов, основные state transitions,
Jolt vehicle operation и network regression set. P0 также добавил прямые
counterexample checks для RA-C01, RA-C02, RA-C04 и RA-C05. Даже эти проверки
не доказывают полный Windows parity: они закрывают конкретные установленные
расхождения, но не заменяют одинаковый runtime trace двух исполняемых версий.

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

### P0 — исправить доказанные расхождения — выполнено

1. RA-C01: начислять campaign result каждому participant runtime/profile.
2. RA-C02: добавить backend operation «установить angular momentum в local
   Z» вместо additive angular velocity для oil.
3. RA-C03: повторная проверка доказала совпадение текущего speed gate;
   ошибочный пункт отозван без изменения game logic.
4. RA-C04: удалить отсутствующий в Windows 0.25-second cooldown.
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

### P2.1 — первый исполняемый перенос source classes — выполнено

Первым блоком введены независимые от Windows части исходных `Planet::Track`,
`Planet` и `Tournament` из
`eff9338:prog/Rock3dGame/header/game/Race.h` и соответствующих реализаций в
`source/game/Race.cpp`. Они собираются отдельными файлами
`OriginalTournament.h/.cpp`, а не растворены в объединённом
`OriginalRaceSession`.

Перенесены и покрыты прямыми counterexample-тестами:

- `Planet::AddTrack`, `ClearTracks`, обе формы `GetTracks`, `NextTrack` и
  оригинальная формула `abs(pass - 1) % maxPass + 1`;
- состояния `Unlock`, `Open`, `Complete`, `SetState`, `NextPass`, `Reset`;
- `GetRequestPoints`, включая fallback на последний pass и исходное
  округление/множители для нескольких Human/Opponent;
- `HasRequestPoints`, `GetPrice`;
- campaign `Tournament::NextTrack` и `CompleteTrack`;
- skirmish track queue с отдельным перемешиванием списка каждого pass через
  тот же process `rand()` stream, который использовал Windows
  `random_shuffle`.

Это уже active code path: `resolveOriginalTournamentTrack`, завершение
заезда и текст требуемых очков в RaceMenu делегируют новым source classes.
Прежняя дублирующая реализация переходов удалена. D3D9/bgfx и PhysX/Jolt
границы этим блоком не менялись.

Этот этап ещё не означает перенос всего object graph `Tournament`. В
следующий блок остаются `StartPass/CompletePass` с выдачей машин и слотов,
`PlayerData`/gamer lookup, weather side effects `SetCurTrack`, сериализация
полного объекта и связанные методы `Race`. Они должны подключаться к уже
введённым классам, а не воспроизводиться новыми helper-функциями.

### P2.2 — pass inventory, PlayerData и ChangePlanet — выполнено

Следующий блок переносит структуры `Planet::SlotData`, `CarData`,
`PlayerData` и исходные методы управления ими: `Insert/Clear/SetSlots`,
`Insert/Clear/SetCars`, `Insert/ClearPlayers`, обе формы `GetPlayer`,
`GetBoss` и `GetId`. `Tournament` дополнен `NextPlanet`, `PrevPlanet`,
`ChangePlanet`, `GetNextPlanet`, gamer collection и приоритетным
`GetPlayerData`.

`Planet::SetPass` теперь исполняет исходную цепочку: при реальном изменении
сначала вызывается `CompletePass` для старого pass, затем меняется номер.
Благодаря этому `Unlock → Open` действительно завершает pass 0 и выдаёт
начальные записи планеты, а `NextPass` выдаёт записи только что пройденного
pass. `TournamentAdvance` сохраняет точные списки открытых car/slot records.

AI roster больше не строится отдельным упрощённым XML-фильтром. Все
`PlayerData`, машины и слоты сначала читаются из `tournamet.xml`, после чего
активный loader вызывает перенесённый `Planet::StartPass`. Сохранены clamp по
`maxPass`, очистка предыдущей конфигурации и Windows-перенумерация
Computer6+ отдельно для campaign/skirmish.

Hangar также больше не меняет `PlanetProgress` вручную: active menu вызывает
source `Tournament::ChangePlanet`, включая правильную последовательность
`Unlock/Open`, выбор первой трассы pass и обратную запись состояния в
профиль. Regression проверяет закрытую/недоступную планету, pass-0 rewards,
AI pass loadout, завершение финального pass и перенос наград через active
`completeOriginalTournamentTrack`.

После P2.2 внутри этого класса оставались weighted weather state при
`SetCurTrack`, полная XML serialization самого library object и вызов
`StartPass` для уже созданных runtime Player при сетевой смене состояния.

### P2.3 — Planet weather и gameplay object graph Trace — выполнено

Перенесены `Planet::Wheater`, `Wheaters`, `SetWheaters` и точная двухпроходная
реализация `GenerateWheater`: фильтр night, most-probable, weighted interval
с включёнными границами и исходный fallback. `Tournament` теперь хранит
`wheater`/`wheaterNightPass`, сбрасывает night-pass при построении нового
списка трасс и не допускает второй night внутри серии. Активный
`selectOriginalWeather` больше не содержит копию этого алгоритма, а строит
source `Planet` из уже разобранного `tournamet.xml` и делегирует ему выбор.

Отдельными компилируемыми файлами `OriginalTrace.h/.cpp` перенесён gameplay
object graph из `eff9338:prog/Rock3dGame/header/game/Trace.h` и
`source/game/Trace.cpp`:

- `WayPoint`: id/position/size/offset, membership, node links, closest-node и
  sphere raycast;
- `WayPath`: linked insertion/deletion, open/enclosed path, source order
  поиска tile, endpoint sphere fallback и длина;
- `WayNode::Tile`: direction/normal/mid-miter geometry, node radius, turn
  angle, interpolated width/height/Z, track lane index/offset, containment,
  source finish distance и raycast;
- `Trace`: point/path ownership, lookup, preferred-path search, alternate
  paths и stable path/node references.

`OriginalRaceSession` теперь один раз строит этот graph из оригинальных
`map/trace/points` и `map/trace/pathes`. Удалена дублирующая session-формула
miter planes. Через source-классы проходят текущий tile, branch transition,
wrong-way distance, lap position, map projection и reset/respawn anchor.
AI и Human используют один и тот же graph. Адаптер оставляет числовой
`TraceNodeRef` только для существующей сериализации runtime state.

Прямой `OriginalTraceSmoke` проверяет поворотный miter, переменную ширину,
terminal half-width в `GetFinishDist`, closest node, alternate path и links.
Расширенный `OriginalTournamentSmoke` проверяет night filtering, weighted
выбор и night-pass. Полный arm64 Debug прошёл 7/7 CTest, resource audit,
автономную bundle verification и `--physics-smoke-test`, включая
checkpoint/lap/finish, AI catch-up и trace reset.

Пока остаются на boundary, а не внутри source classes: TinyXML
`Trace::Load/Save`, editor-only triangle-strip buffer, D3D ray helper types и
полная Component/Object serialization `Tournament`. Gameplay-методы этих
классов уже находятся в active code path; перечисленные boundary части не
заменены заглушками и не участвуют в гонке.

### P2.4 — первый исполняемый блок source Player — выполнено

Из анонимного `RacerRuntime` и `OriginalRaceSession` выделен отдельный
компилируемый `source::Player` в `OriginalPlayer.h/.cpp`. Это не новое
portable-поведение: класс переносит gameplay-owned часть
`eff9338:Player` и сохраняет исходные константы Human easing, Computer cheat,
`cHumanArmorK`, `cTimeRestoreCar=2.0` и finish block `0.3`.

В active code path перенесены:

- `Player::ReloadWeapons` с порядком `stHyper, stMine, stWeapon1..4`;
- выбор текущего установленного weapon slot и синхронизация charge/HUD;
- `Player::TakeBonus` для money, medpack, immortal и ammunition, включая
  исходный rounded-random index и формулу minimum-one charge;
- `SetFinished`, финишная блокировка управления и reward/place state;
- `AddMoney`, `AddPoints`, campaign result и picked money;
- destroy/restore lifecycle `OnDestroy -> 2 s -> CreateCar/ResetCar`, включая
  отдельный кадр между Jolt respawn request и повторной активацией машины;
- `NetPlayer` disconnect cleanup игрового Player state;
- `ApplyMobility` теперь читает `cHumanArmorK` из того же source-класса.

`OriginalRaceSession` остаётся координатором физики, событий и сетевой
авторитетности, но больше не содержит собственные реализации перечисленных
методов. Прямой `OriginalPlayerSmoke` проверяет slot order, reload, bonus
rounding, healing/shield, finish brake, rewards, restore и disconnect; полный
arm64 Debug прошёл 8/8 CTest, physics smoke и 240-frame race-render smoke.

Граница этого этапа намеренная: `Player::CarState` geometry/progress пока
исполняется адаптером поверх уже перенесённого `source::Trace`, renderer-owned
headlights/color material остаются в bgfx renderer, а RockCar/PhysX вызовы —
на Jolt boundary. Следующий Player-блок должен перенести `CarState` как класс
и передать ему source Trace references, после чего можно отделять `AICar`.

### P2.5 — Player::CarState и прямой Trace object graph — выполнено

Следующий блок переносит вложенный `Player::CarState` из
`eff9338:Player.cpp` непосредственно в `source::Player`. Семь параллельных
массивов `OriginalRaceSession` удалены: current/last node, last coordinate,
wrong-way start, map position и maximum-speed window теперь принадлежат одному
объекту `Player::CarState`, как в Windows.

Перенесены исходные методы и порядок `CarState::Update`:

- `Trace::IsTileContains` с preferred current WayPath;
- отдельные `curTile`, `curNode`, `lastNode` и source link acceptance через
  общие `WayPoint`;
- `GetPathIndex`, `IsMainPath`, `GetPathLength`, `GetDist`, `GetLap` и
  `GetMapPos`;
- переход круга только при возврате на первый node главного WayPath;
- `moveInverseStart - GetDist() > 20` и очистка wrong-way на forward tile;
- one-second maximum-speed window и исходный порог падения скорости 80;
- lane index через `WayNode::Tile::ComputeTrackInd` и `curNode` sphere switch.

Active HUD/debug/minimap, place sorting, commentator distance, AI lane
occupancy/path fallback, AI weapon progress, Hyper turn distance и respawn
теперь читают `Player::CarState`; прежние adapter-проекции
`findTraceTile/projectTraceTile/traceDistance` удалены. Числовой `NodeRef`
остаётся только стабильным идентификатором между source graph и существующим
AI/serialization boundary.

`OriginalPlayerSmoke` расширен независимой геометрией: main/alternate path,
map projection/retention, 20-metre inverse threshold и lost-control. Полный
встроенный race regression продолжает проверять checkpoint/lap/finish,
branch lap и trace reset на реальных resource data.

После P2.5 следующий крупный source-class блок — `AICar::PathState` и
`ControlState`. Renderer-owned Player lights/materials и Jolt vehicle feedback
остаются осознанными backend boundaries, а не заглушками.

### P2.6 — AICar::PathState и ControlState — выполнено

Из монолитного `OriginalRaceSession::aiInput` выделен компилируемый
`source::AICar` в `OriginalAICar.h/.cpp`. Девять session-local массивов
lane/control/reset state удалены; каждый участник теперь имеет собственные
`PathState` и `ControlState`, как в `eff9338:AICar`.

Прямо перенесены:

- `FindFirst/LastUnlockTrack` и `FindFirst/LastSiblingUnlock`;
- source `curTile/nextTile/curNode` lifecycle, включая удержание `lastNode`
  вне trace с допуском 5 и выбор связанного WayPoint path;
- `dirArea = 5 + abs(speed) * kSteerControl * 10`, внутренняя полоса перед
  поворотом и `ComputeTrackNormOff`;
- hysteresis торможения `1.5 * kBreak`/`1.0 * kBreak`;
- `cSteerAngleBias = pi/128`, one-second blocking и исходное чередование
  reverse/forward с инверсией steering при заднем ходе;
- отдельный трёхсекундный `UpdateResetCar` timer, который также работает при
  потере live `curTile`;
- ленивый вызов source RNG только в реальной terminal-node ветке
  `GetRandomNode`, поэтому обычный AI tick не сдвигает weapon/mine RNG.

`AISystem::ComputeTracks` продолжает формировать collision chains в session,
но теперь пишет `freeTracks/lockTracks` прямо в `AICar::PathState`.
Jolt boundary получает только source-команду `Accelerate/Brake/Reverse/None`
и исходный steering angle; backend не владеет AI state.

Добавлен отдельный `OriginalAICarSmoke` для lane search, corner braking,
off-trace fallback, blocking recovery и reset. Resource-based physics smoke
дополнительно прогоняет существующие four-track, Hyper/brake, target-plane,
reverse/reset и AI finish regressions на оригинальной карте.

Следующий decomposition-блок — `AICar::AttackState`: front/back target,
weapon readiness/range, Hyper и Mine state пока source-derived, но всё ещё
координируются массивами и ветвями `OriginalRaceSession`.

### P2.7 — AICar::AttackState — выполнено

Третий вложенный state оригинального `AICar` перенесён в тот же active
`source::AICar`. Из `OriginalRaceSession` удалены retained front/back target,
`placeMineRandom` и отдельный неиспользуемый mine cooldown; сессия теперь
строит только per-frame snapshot машин/установленного оружия и исполняет
backend-neutral решения `AttackState` через существующие Weapon/Jolt методы.

Перенесены source branches:

- поиск ближайшей цели в переднем/заднем конусе `pi/4` по расстоянию до
  плоскости машины и точный `WayNode::Tile::IsZLevelContains`;
- удержание прежней цели, пока новый кандидат не дальше её полного car size;
- lateral и Z gates `ShotByEnemy`, общий abort при неготовом weapon slot,
  сортировка по `maxDist`, 25-процентный случайный выбор и source
  ammo-by-road-progress policy;
- исключение Hyper/Mine из ordinary slots и `ptTorpeda` как единственное
  обычное оружие, способное стрелять назад;
- `RunHyper` с distance-to-next-turn, `pi/6`, brake gate и charge policy;
- `PlaceMine` с исходным диапазоном 5–95%, бонусом 30% за близкую заднюю
  цель, `RandomRange(-0.5, 0)`, лимитом трёх charges (двух для maslo) и
  повторной попыткой до фактической readiness Weapon;
- немедленная очистка retained targets при `PlayerDispose`/network
  disconnect.

`OriginalAICarSmoke` теперь отдельно проверяет front retention, readiness
abort, back torpedo, Hyper turn gate, Mine RNG/charge и target disposal.
Resource smoke продолжает проверять те же решения через реальные weapon
records и projectile effects.

После P2.7 исходные `PathState`, `AttackState` и `ControlState` снова собраны
одним классом. Следующий логичный decomposition-блок — `AIPlayer/AISystem`
lane-chain ownership либо `Weapon/Logic`; PhysX/D3D9 вызовы остаются
осознанными Jolt/bgfx boundaries.

### P2.8 — AISystem::ComputeTracks — выполнено

Формирование цепочек машин и занятости полос вынесено из
`OriginalRaceSession` в отдельный active `source::AISystem`. Удалена
session-local аппроксимация по `TracePoint`, включая повторный расчёт ширины,
высоты и направления плитки и отдельный BFS по машинам.

Перенесены правила `eff9338:AIPlayer.cpp::AISystem::ComputeTracks`:

- в список входят только живые AI с настоящим `CarState::curTile` и
  `curNode`; off-trace fallback `PathState` полосу не занимает;
- боковая сортировка вычисляется исходной clockwise-normal прямой текущей
  плитки;
- каждая пара проверяется один раз в source insertion order, причём
  асимметрично через `curNode->Tile::IsContains(target, false)` первой машины;
- продольное пересечение использует `trackNormLine`-эквивалент и сумму
  source car radii;
- связанные компоненты сливаются в chain, стабильно сортируются и блокируют
  выбранную полосу у всех остальных машин;
- сохранены `lsl::ClampValue` semantics, в том числе исторический случай
  `lower > upper` для chain длиннее числа полос, и дополнительная блокировка
  при одинаковой исходной полосе двух соседних машин;
- `freeTracks/lockTracks` очищаются у каждого включённого AI даже при
  отсутствии соседа, как в Windows.

Сессия теперь только формирует переиспользуемый non-owning список из Jolt
позиций и radii; вся trace/lane логика исполняется на прямом объектном графе
`WayNode`. `OriginalAICarSmoke` проверяет точную трёхмашинную цепочку и очистку
одиночного участника. Следующий крупный gameplay block — source `AIPlayer`
lifecycle либо `Weapon/Logic` ownership.

### P2.9 — AIPlayer lifecycle — выполнено

Перенесён owner-класс `source::AIPlayer`, и прямой массив `AICar` удалён из
`OriginalRaceSession`. Как и в Windows, каждый AI owner теперь связывает один
`Player` с опционально созданным `AICar` и является точкой входа для
`OnProgress`, attack, reset и dispose-target.

Сохранены исходные особенности:

- `CreateCar` идемпотентен, `FreeCar` очищает всё вложенное состояние;
- computer owner получает `cCheatEnableFaster | cCheatEnableSlower`, human
  owner — `cCheatDisable`;
- AI enable flag запрещает применение control/weapon решений, не подменяя
  source path/control вычисления;
- отдельный human AI owner оставлен dormant для перенесённого debug F7 mode;
- `AISystem`, Jolt command adapter, attack execution, blocked-car respawn и
  network `PlayerDispose` обращаются к `AICar` только через `AIPlayer`;
- catch-up/easing ветка сессии читает source cheat mask вместо проверки
  индекса участника; отдельный network HumanPlayer сохраняет faster-only
  mask.

`OriginalAICarSmoke` проверяет computer/human masks, Create/Free lifecycle и
enable/disable command gate. Следующий decomposition block — `Weapon/Logic`
runtime ownership и удаление cooldown/charge coordination из сессии.

### P2.10 — Weapon runtime и Logic::Shot readiness — выполнено

Перенесён active `source::Weapon` с исходными `Desc`, `_shotTime`,
`OnProgress`, `IsReadyShot(delay)`, `IsReadyShot()`, `IsMaslo` и успешным
reset после `CreateShot`. Три несвязанных session-массива — cooldown четырёх
обычных слотов, mine age и Hyper cooldown — заменены одним `WeaponRack` на
участника.

Исправлены обнаруженные расхождения с `eff9338:Weapon.cpp` и
`Logic.cpp::Shot`:

- readiness использует строгое `_shotTime > shotDelay`, а не countdown
  `<= 0`;
- weapon time растёт уже во время стартового countdown, поскольку Windows
  `Weapon` остаётся зарегистрированным `GameObject` до начала управления;
- timer и charge сбрасываются только после реально созданного projectile;
- обычное оружие, Hyper и Mine используют один source lifecycle, включая
  network-replicated shot commit;
- аналоговая мина читает тот же `_shotTime` с `(1-alpha)*0.6`, а `maslo`
  определяется по типу первого projectile (`ptMaslo`), не по имени record;
- удалён отсутствующий в Windows minimum cooldown 0.03 для игрока;
- повторная прямая проверка `AICar.cpp::ShotByEnemy` установила, что AI
  обязан использовать `max(shotDelay, 0.25)`. Ошибочно удалённый на первом
  проходе 0.25-second floor восстановлен в active session;
- AI readiness теперь напрямую спрашивает установленный source `Weapon`.

Добавлен отдельный десятый CTest `OriginalWeaponSmoke`: strict boundary,
failed-prepare retention, successful reset, `ptMaslo` и полный rack progress.
Физическое создание projectile и эффекты остаются Jolt/bgfx boundary внутри
сессии.

### P2.11 — WeaponItem charge ownership и Logic::Shot execution — выполнено

Перенесён active `source::WeaponItem`, связанный с уже существующим
profile-backed charge storage `source::Player`. Он сохраняет исходные
`maxCharge/cntCharge/curCharge/chargeStep/damage/chargeCost`, readiness,
`Reload` и точную транзакцию `Shot` без второй копии боезапаса.

Session primary/Hyper/Mine paths больше не списывают заряды и не сбрасывают
таймеры вручную. Backend сначала сообщает результат `PrepareProj`, после чего
`WeaponItem::Shot` одновременно фиксирует charge и successful-shot reset.
Сохранены две нетривиальные ветки Windows:

- `maxCharge == 0` означает бесконечный боезапас и допускает выстрел при
  `curCharge == 0`;
- явный `newCharge` из `NetPlayer::DoShot` применяется даже при неуспешной
  подготовке projectile, включая empty desc, failed mine ray и spring без
  контакта колёс.

Перенесён `source::Logic::Shot`/`ShotAll` с Windows-порядком битов
Hyper/Mine/Weapon1..4, readiness selection и `cHumanShot`: обычная попытка
человека регистрируется даже без созданного projectile, Hyper исключён.
`source::HumanPlayer::SelectWeapon` ищет следующий primary с
`curCharge > 0`, после последнего заряда переключает слот и не вызывает
`Logic::Shot`, если заряженных слотов нет. Lap reload теперь вызывает
`WeaponItem::Reload`. `OriginalWeaponSmoke` и resource race smoke проверяют
failed prepare, replicated charge, infinite ammo, dry human event,
auto-selection и multi-slot shot.

### P2.12 — HumanPlayer owner/control — выполнено

Перенесён отдельный active `source::HumanPlayer` вместо продолжения
session-local selection helpers. Как Windows `_curWeapon`, owner хранит
текущий физический primary slot и реализует `GetWeaponByIndex`,
`GetWeaponCount`, `SetCurWeapon`, ограниченные `gaWeaponDown/gaWeaponUp` и
рекурсивный `SelectWeapon` после расходования последнего заряда.

`HumanPlayer::Control::OnInputProgress` перенесён как backend-neutral command:
acceleration имеет приоритет над reverse, значения газа/реверса остаются
бинарными и left имеет приоритет над right при одновременном удержании.
SDL по-прежнему владеет сырым состоянием устройств, а результат source
command передаётся в Jolt `VehicleInput`.

Из `OriginalRaceSession` удалены heap-allocated usable-slot vector и ручной
ordinal search. Direct Shot1..4, current shot, next/previous, auto-select и
HUD-selected slot проходят через один `HumanPlayer` owner. Добавлен
одиннадцатый CTest `OriginalHumanPlayerSmoke` для input priority, bounded
selection, sparse direct ordinal и exhausted-ammo reset.

### P2.13 — DroidItem/ReflectorItem и physical slot ownership — выполнено

Перенесены active `source::DroidItem` и `source::ReflectorItem` как
наследники уже перенесённого `WeaponItem`. `workshop.xml` теперь сохраняет
исходный `Slot::Type` (`5..9`) в `WeaponItemType`, поэтому runtime отличает
`stDroid` и `stReflector` по сериализованному классу, а не по косвенному
признаку `repairPeriod > 0` или `reflectValue > 0`.

Сохранены точные особенности `eff9338:Player.cpp`:

- `DroidItem::OnCreateCar` обнуляет `_time` и регистрирует progress, а
  `OnDestroyCar` только снимает регистрацию; новый car снова начинает с
  нулевого таймера;
- сравнение периода строгое: лечение происходит при
  `(_time += deltaTime) > repairPeriod`;
- поле `repairValue` загружается и доступно, но оригинальный active branch
  вызывает `Healt(5.0f)` буквально; это историческое расхождение поля и
  исполнения сохранено;
- каждый установленный Droid имеет независимый таймер и несколько Droid
  последовательно лечат машину, как отдельные зарегистрированные объекты;
- `ReflectorItem::Reflect` использует
  `damage * ClampValue(1-reflectValue, 0, 1)`;
- `Logic::Damage` пропускает touch damage и выбирает первый физический слот
  типа `stReflector`, не суммируя отражатели и не проверяя ненулевой
  коэффициент.

Эти классы теперь являются настоящими полиморфными предметами четырёх
физических `Slot` (`stWeapon1..4`) каждого гонщика. Инициализация профиля,
death/release, двухсекундный respawn/create и network disconnect теперь
проходят через `Player::_slot[]` owner. Из сессии удалены `repairSeconds_`, поиск первого
repair description и ручной расчёт отражения по `WeaponDefinition`.

Расширенный `OriginalWeaponSmoke` проверяет strict period, literal heal,
progress registration, несколько Droid и first-reflector rule. Resource race
smoke дополнительно загружает реальные `droid`/`reflector` из упакованного
`workshop.xml`, устанавливает их через `PlayerProfile`, проверяет 0.4
reflector и фактическое лечение через active session update.

### P2.14 — Player::CreateCar/FreeCar state lifecycle — выполнено

Перенесён оставшийся жизненный цикл `eff9338:Player.cpp::CreateCar/FreeCar`
в active `source::Player` и его вложенный `CarState`. Подтвердилось, что
portable session при восстановлении вызывал только `Resc()` и поэтому
оставлял от уничтоженной машины `moveInverse`, таймер неправильного
направления, накопленную максимальную скорость и таймер потери управления.
После respawn это могло немедленно породить ложную реплику wrong-way либо
повторный `LostControl`.

Теперь каждый `CreateCar`, включая двухсекундный restore, сбрасывает эти
transient-поля. Ветка `newRace` дополнительно ставит `curTile/curNode/lastNode`
на первый узел main path, очищает bonus-projectile registry, возвращает
`nextBonusProjId` к 1 и обнуляет restore timer. `FreeCar(true)` очищает
узлы и круги, как Windows. Удалён придуманный mini-map fallback на первую
точку: без live tile и last node `GetMapPos()` возвращает NullVector.

Прямой regression проверяет create/free/respawn state, начальную привязку к
trace и mine-id lifecycle; 13 non-network CTest, resource verifier, map1
physics и 240-frame Metal/Jolt smoke проходят.

### P2.15 — HumanPlayer::Control gates и ResetCar owner — выполнено

Перенесены оставшиеся ветви `eff9338:HumanPlayer.cpp::Control` и
`HumanPlayer::ResetCar`. Подтвердилось, что session применял SDL-команды
непосредственно и не отбрасывал weapon/mine/hyper/reset при `Player::IsBlock`;
непрерывный `mineHeld` также обходил chat gate. Контактная проверка reset была
верной по смыслу, но оставалась session-local.

Active `source::HumanPlayer` теперь выдаёт точный трёхчастный gate:

- отсутствие car или block запрещают движение и все действия;
- chat запрещает event actions и continuous Hyper/Mine, но сохраняет уже
  удерживаемый gas/steering, потому что Windows проверяет chat после
  `SetMoveCar/SetSteerWheel`;
- AIDebug human AI подавляет continuous driving/Hyper/Mine, но не меняет
  независимый `OnHandleInput` event gate;
- reset принимается только при существующей машине и контакте хотя бы одного
  колеса либо кузова.

Main передаёт реальное состояние `UserChat::inputVisible`, session применяет
только source-filtered control. Прямой HumanPlayer regression покрывает все
ветви; 13 CTest, map1 physics и 240-frame Metal/Jolt smoke проходят.

### P2.16 — Race::StartRace/GoRace runtime state — выполнено

Перенесён active `source::RaceRunState`, владеющий `_startRace`, `_goRace` и
Player block-переходами `Race::StartRace/GoRace/ExitRace`. Аудит подтвердил
существенное расхождение: portable countdown обнулял throttle, но не выполнял
`human->ResetBlock(true)`. Поэтому Jolt-машина четыре секунды оставалась без
полного тормоза и могла смещаться на стартовой решётке под уклоном, контактом
или residual velocity.

Теперь `StartRace` снимает старые block/finished состояния у всего состава и
ставит Human в block. `Player::OnProgress` на каждой offline/network
countdown-стадии выдаёт исходный `mcBrake`; `GoRace` снимает block ровно на
зелёном сигнале. `DEBUG_PX` сохраняет немедленный StartRace→GoRace, а
network stage 4 использует тот же owner. `ExitRace` сбрасывает source run
flags, оставляя Jolt/bgfx teardown backend-слою.

Unit regression покрывает повторный start, go и exit; integrated smoke
проверяет полный brake и block на offline/network countdown и снятие на
cGoRace. 13 CTest, map1 physics и 240-frame Metal/Jolt smoke проходят.

### P2.17 — Player identity/role owner — выполнено

Перенесены поля и методы исходного `Player`: `_id`, `_gamerId`, `_netSlot`,
`_netName`, color, `GetName`, `IsHuman`, `IsComputer` и `IsOpponent`.
Подтвердилось, что active session продолжала заново выводить роль из индекса
гонщика и `Race::Racer::human`; это неэквивалентно Windows, где ID является
битовым значением (`cHuman=0`, computers в младшем байте, opponents в
старшем байте).

Tournament loader теперь присваивает исходные `Race::AddPlayer` IDs, а
network roster выполняет ветку `NetPlayer`: локальный владелец получает
`cHuman`, удалённый human — `netSlot << cOpponentBit`, AI сохраняет descriptor
ID. Gameplay damage authority, AI selection, catch-up reference set,
lap/finish completion и countdown находят роли через active `Player`, а не
через definition/index surrogate. `GetName` сохраняет исходный приоритет
сетевого имени над tournament name.

Прямой regression проверяет все три роли, identity, цвет и name override;
resource audit проверяет IDs полного campaign roster, а network session
regression использует настоящий opponent ID. 13 non-network CTest, resource
verifier и map1 physics smoke проходят.

### P2.18 — Race::ExitRace source teardown — выполнено

Перенесён оставшийся gameplay lifecycle `Race::ExitRace`. Подтвердилось, что
portable automatic finish переходил прямо в FinishMenu после трёхсекундного
таймера и не вызывал source teardown; `AIPlayer`, `Player::CarState`, active
projectiles/mines/effects, pending physics requests и achievement race-state
оставались живыми за меню. Ручной выход вызывал только completion/ranking и
также не выполнял `Player::FreeCar(true)`.

`RaceRunState::ExitRace` теперь освобождает CarState всех Player, а session
однократно освобождает AI cars и item lifecycle, очищает source Logic object
graph, pending network/physics queues, сбрасывает contact effect и выключает
map bonus/decoration instances. Jolt actor/bgfx scene остаются backend
границей и перестают обновляться через `inRace`.

`showFinishMenu` теперь повторяет Windows-цепочку
`cRaceFinishTimeEnd -> Menu::ExitRace -> Race::ExitRace -> FinishMenu`;
ранний HudMenu exit остаётся идемпотентным. Regression проверяет освобождённые
car nodes, AI и object graph после первого/повторного exit. 13 CTest, map1
physics и 300-frame FinishMenu smoke проходят.

### P2.19 — HumanPlayer runtime owner — выполнено

Устранено оставшееся portable-предположение, что локальный `HumanPlayer`
всегда находится в `racers[0]`. В оригинальном Windows-коде владельцем служит
указатель `Race::HumanPlayer`, а в сетевой гонке canonical roster может
поместить локальный descriptor не в первый slot. Session теперь один раз
находит владельца по source `Player::IsHuman()`/`cHuman` ID и использует его
для input, reset/respawn, оружия, мин, hyper, achievement state, записи
профиля и debug AI control.

HUD, commentator, камера, listener/spatial audio, race telemetry, finish и
network publication теперь получают тот же runtime owner. AI-циклы и smoke
метрики проходят весь roster и фильтруют `Player::IsComputer()`, поэтому
remote opponents больше не принимаются ни за локального игрока, ни за AI.
Debug overlay также показывает физику, ввод и vehicle definition именно
локальной машины.

Regression переставляет локального Human в slot 1, оставляет remote opponent
в slot 0 и проверяет маршрутизацию управления и сохранение money/points.
13 non-network CTest, resource verifier, map1 physics и 240-frame Metal/Jolt
render smoke проходят.

### P2.20 — Player visual state и полный Race::StartRace owner — выполнено

Перенесены оставшиеся независимые от D3D9 поля и переходы исходного
`Player`: `HeadLightMode`, `Get/SetHeadlight`, `Get/SetReflScene`. Ранее bgfx
сам угадывал две фары и исключение машины из cube reflection по
`Race::Racer::human`; это обходило active Player и давало неверный результат
при изменённом сетевом порядке/владельце.

`RaceRunState::StartRace` теперь выполняет исходные `ReloadWeapons` и
назначение `hlmTwo/hlmOne/hlmNone` по погоде и `Player::IsHuman()`.
Локальный `HumanPlayer` устанавливает `reflScene=false`, тогда как remote
opponents сохраняют исходное значение `true`; `ExitRace` снимает фары до
`FreeCar(true)`. Renderer только отображает это состояние через bgfx/Metal.

Тот же runtime Human теперь задаёт цель isometric cull-opacity и центр
теневых каскадов, а не `vehicles.front()`. Regression проверяет режимы фар,
reflection flag, стартовую перезарядку и exit teardown. 13 non-network CTest,
resource verifier, map1 physics и 240-frame Metal/Jolt render smoke проходят.

### P2.21 — GameMode race clock owner — выполнено

Перенесён независимый от Win32/D3D9 временной автомат исходного
`GameMode`: `cGoRaceWait`, `cGoRace1..3`, `cGoRace`, `_goRaceTime`,
`_finishTime`, pause и внешний network-stage. `OriginalRaceSession` больше
не владеет параллельными countdown/finish числами и получает только переходы
от active `GameModeRaceState`.

Offline-путь повторяет `Menu::StartRace -> GoRaceTimer` и исходный
`cGoRaceLag=1`; network-путь останавливает локальные часы и применяет стадии
хоста. Finish теперь посылает эквивалент `cRaceFinishTimeEnd` именно при
`(_finishTime += dt) > 3.0f`, а pause не расходует ни один таймер. Unit
regression покрывает offline, external network, pause, DEBUG_PX и граничные
три секунды; 13 non-network CTest проходят.

### P2.22 — Player car/light attachment lifecycle — выполнено

Подтвердилось, что перенос сохранил `HeadLightMode`, но потерял вторую
половину исходного `Player::ReleaseCar/SetLightsParent`: при смерти spot
lights продолжали освещать сцену из старой позиции Jolt-body. Active
`Player` теперь владеет наличием car и attachment света; `Destroy/FreeCar`
отсоединяют свет, а `CreateCar` после restore присоединяет его снова без
сброса выбранного режима фар.

Также подтверждён лишний render branch: Windows `_nightFlare::GraphDesc` не
содержит `gpReflScene`/`gpReflWater`, но порт повторно рисовал flare каждого
автомобиля в cube и water reflection. bgfx adapter теперь показывает flare
только для attached source Player и только в основном scene pass. Unit
regression покрывает death/restore attachment lifecycle.

### P2.23 — GameMode pause/effects coupling — выполнено

Подтвердилось, что portable HudMenu pause останавливал Jolt/session, но не
выполнял вторую половину оригинального `GameMode::Pause`:
`Logic::Mute(Logic::scEffects, pause)`. Поэтому loop-голоса мотора, шин и
эффектов продолжали микшироваться за modal exit dialog.

Все race pause/resume/exit/failure переходы теперь проходят через одну
границу: source `GameModeRaceState` останавливает clocks, а SDL adapter
устанавливает только Effects bus в `0` и возвращает сохранённый options
volume при resume. Music и Voice не приглушаются, точно как в Windows.
Session и integrated renderer smoke проверяют frozen countdown/world,
effects mute и восстановление громкости.

### P2.24 — Player runtime presentation owner — выполнено

Подтвердилось, что после переноса `Player::SetColor`, `SetGamerId` и
сетевого имени часть portable consumers продолжала читать исходный
`Race::Racer` descriptor. Из-за этого изменение цвета через
`NetPlayer::OnSetColor` не доходило до кузова, гусениц, подушек и точки
мини-карты, а kill/opponent/finish HUD мог показывать tournament token вместо
активного network name.

Renderer и HUD теперь, как Windows-код, читают presentation state из active
`Player`. Network snapshot сначала применяет gamer id и цвет ко всем
соответствующим Player, включая локального владельца, и только затем
синхронизирует физику remote машин. Неизменяемый descriptor остаётся только
fallback для отсутствующего runtime slot и владельцем выбранных при загрузке
ресурсных assets. Session regression проверяет перенос gamer id/цвета и
границу индекса; 13 non-network CTest и integrated Metal/Jolt smoke проходят.

### P2.25 — Player cheat/AIPlayer ownership lifecycle — выполнено

Подтвердилось, что флаги исходного `Player::_cheatEnable` были ошибочно
размещены в portable `AIPlayer::cheat_`. Session читал этот параллельный
state для компьютеров и дополнительно выдавал сетевому Human выдуманный
`cheatEnableFaster`, которого нет ни в `Player`, ни в `NetPlayer` Windows.
Это меняло torque/steering физики не по исходной роли.

`Player::GetCheat/SetCheat` и само поле теперь перенесены в active Player.
Как в оригинальном `AIPlayer`, constructor/reset включает Faster+Slower у
не-Human Player, а destructor/rebind снимает их. Session освобождает AI owner
до замены массива Player, создаёт AI только для Computer и dormant debug
Human, но не для authoritative remote Opponent, и `Player::OnProgress` всегда
читает собственную маску. Unit regressions проверяют assign/release owner;
network regression — отсутствие AI/cheat у Human и Opponent.

### P2.26 — Player::ComputeCarBBSize visual bounds — выполнено

Подтвердился ещё один physics/AI surrogate: portable session вычисляла
`CarState::size/radius` из диагонали Jolt/PhysX collision half-extents в трёх
разных местах. Windows `Player::ComputeCarBBSize` вместо этого берёт
`GrActor::GetLocalAABB(false)`, применяет scale визуального actor и сохраняет
диагональ/половину диагонали в `Player::CarState` при создании машины.

Vehicle loader теперь вычисляет backend-neutral эквивалент из полного набора
body `VisualNode` mesh bounds и их source transforms. Размер и radius
передаются active `CarState` один раз при `CreateCar`; `AISystem` lane chains,
`AICar::VehicleState` и attack target retention читают этот owner state.
Три повторных collision-box формулы удалены. Resource regression проверяет
валидный visual AABB, Player regression — size/radius relation, session
regression — соответствие каждой configured vehicle.

### P2.27 — Player::GetName/GetPhoto tournament lookup — выполнено

Подтвердилось, что portable `Player` хранил имя из первоначального
`Race::Racer`, а HUD и отдельный FinishMenu заранее загружали портрет по тому
же descriptor. После исходных `NetPlayer::OnSetGamerId` или замены
дублирующегося gamer id активный `Player` уже имел новый id, но интерфейс мог
продолжить показывать старого персонажа.

В `Race` перенесён backend-neutral каталог исходных `Planet::PlayerData`.
Lookup повторяет точный порядок `Tournament::GetPlayerData`: все global
`GetGamers()` по XML-порядку, затем только игроки текущей планеты. Замена
дубликата по `Race::StartRace` ограничена глобальными gamer entries. HUD,
kill notification и оба finish UI теперь разрешают имя/портрет по runtime
`Player::GetGamerId`, сохраняя `NetPlayer::_netName` как высший приоритет.
Resource regression отдельно проверяет global Snake и различающиеся id=1 на
Intaria/Patagonis.

### P2.28 — Player::ApplyColorMat / CarFrame color scope — выполнено

Подтвердилась визуальная реимплементация: Metal adapter передавал цвет
игрока как tint всем root nodes кузова, а также отдельным include-actors
`GusenizaAnim` и `PodushkaAnim`. Windows `Player::ApplyColorMat` клонирует
материал только первого `IVBMeshNode` корневого car actor (если
`RockCar::disableColor` выключен) и меняет sampler 0 этого клона. Гусеницы,
подушки, колёса и последующие body nodes не перекрашиваются.

Vehicle loader теперь сохраняет сериализованный `motor/disableColor`.
Race renderer применяет runtime `Player::GetColor` только к node 0 корневого
кузова; animated include actors снова используют исходные материалы. В
меню `CarFrame` цвет записывается в active presentation Player, поэтому
выбор палитры сразу виден после переноса runtime owner. Сетевые маленькие
car viewports получают `NetPlayer` color, а их cache invalidation теперь
учитывает изменение цвета. Resource regression проверяет color-material gate
для всех 17 поставляемых машин.

### P2.29 — Player::Shot transaction owner — выполнено

Подтвердилось, что `Weapon` и `WeaponItem` были перенесены, но active session
всё ещё вызывала `WeaponItem::Shot` напрямую в трёх независимых ветках:
primary, Hyper и Mine. Из-за этого исходный `Player::Shot` не был владельцем
атомарной операции charge/projectile-id, а регистрация `_bonusProjs` для мины
дублировалась отдельным session-кодом.

В `source::Player` перенесена backend-neutral транзакция `Shot`. Она принимает
результат platform-подготовки снаряда, всегда передаёт `newCharge` в
`WeaponItem` (включая неуспешный replicated shot, как `NetPlayer::DoShot`) и
регистрирует projectile id только для успешно созданного `stMine`. Все
primary/Hyper/Mine пути, включая AI и network replay, теперь проходят через
этого владельца; прямой `WeaponItem::Shot` из session удалён. Player regression
проверяет local primary, успешную mine-регистрацию и неуспешный replicated
mine shot без ложного live projectile.

### P2.30 — Player result/economy state owner — выполнено

Подтвердился ещё один архитектурный обход: `money`, `points`, `_pickMoney`,
`place` и `finished` оставались публичными portable-полями. Session, HUD,
FinishMenu, network result adapter и smoke fixtures меняли их напрямую,
хотя в `eff9338:Player.h` эти поля закрыты и доступны только через
`Get/SetMoney`, `Get/SetPoints`, `GetPickMoney/ResetPickMoney`,
`Get/SetPlace` и `Get/SetFinished`.

Полный API перенесён в active `source::Player`, поля закрыты. Race start и
profile write, place sorting, AI gates, finish completion, campaign rewards,
HUD и сетевой result snapshot теперь читают одного Player-owner. Особенно
важный finish transition больше не обнуляет собранные деньги присваиванием:
он вызывает исходный `ResetPickMoney` после копирования значения в
`RaceLifecycle::Result`; fixture также проходит через `SetFinished`, сохраняя
побочный immortal flag. Player regression покрывает Set/Get/Add и reset.

### P2.31 — Player::SetCar active record owner — выполнено

Подтвердился ещё один потерянный исходный owner: portable `Player` вообще не
хранил эквивалент `Player::CarState::record`. После создания гонки session,
AI, collision/weapon logic и bgfx renderer десятки раз возвращались к
стартовому `Race::Racer`, поэтому последующая замена машины не могла атомарно
сменить её physics, visual/effect definitions и runtime lifecycle.

Перенесены `Player::GetCarRecord/SetCar`. Как в Windows, смена record сначала
вызывает `FreeCar(true)`, полностью отсоединяя прежнюю машину и очищая её
`CarState`, и только затем записывает новый record. Race reset связывает
каждого active Player с уже настроенной персональной `Vehicle`; gameplay,
AI, weapon contacts, death/energy/shield effects, основной и shadow passes,
анимация гусениц и следы шин читают эту запись. Стартовый descriptor остаётся
только fallback до создания runtime и источником предварительной загрузки GPU
assets. Player regression проверяет identity record и обязательный teardown
при смене машины.

### P2.32 — Player physical item-slot owner — выполнено

Подтвердилось, что уже перенесённые `DroidItem`/`ReflectorItem` оставались в
параллельном `OriginalRaceSession::playerItemRacks_`. Это расходилось с
`Player::_slot[]` Windows и вынуждало session вручную вызывать
`OnCreateCar`, `OnDestroyCar`, progress и reflector lookup при каждом
destroy/restore/disconnect/exit переходе.

На промежуточном этапе `PlayerItemRack` был частью active `source::Player`;
в P2.36 этот временный контейнер удалён, а классы помещены непосредственно в
физические `Slot`.
`CreateCar/FreeCar` сами подключают и отключают slot item lifecycle,
`ProgressBehaviors` исполняет Droid, а damage path получает первый Reflector
из целевого Player. Параллельный session-массив и пять ручных синхронизаций
удалены; racer setup связывает slots до `CreateCar`, как требует исходный
callback. Player regression проверяет регистрацию Droid, исходное лечение на
5 единиц и обязательное отключение progress при `FreeCar`.

### P2.33 — Player weapon-object owner — выполнено

Подтвердилась оставшаяся половина того же обхода: четыре primary `Weapon`,
Hyper и Mine хранились в `OriginalRaceSession::weaponRacks_`. Хотя charge уже
принадлежал Player, readiness timer, `IsMaslo`, `ShotEffect::OnShot` и ссылки
Droid/Reflector смотрели в отдельный массив session.

`WeaponRack` перенесён внутрь active `source::Player`. Все primary/hyper/mine
shot transactions, AI readiness, analog oil trigger и per-projectile
ShotEffect теперь получают Weapon из Player-owner; visible-countdown progress
обходит Players и обновляет их собственные racks. Session-массив удалён.
Player regression проверяет cooldown одного встроенного Weapon и связывает
Droid с тем же объектом, исключая отдельный тестовый surrogate.

### P2.34 — Player::_slot[] physical layout and mobility owner — выполнено

Подтвердилась структурная ошибка в уже перенесённом `PlayerSlotRack`: массив
индексировался по сериализованному `Slot::Type`. В оригинале же
`Player::_slot[]` индексируется отдельным `Player::SlotType`; поэтому
`stWeapon1..stWeapon4` могут одновременно содержать обычный Weapon, Droid или
Reflector. Portable rack схлопывал все такие записи в одну позицию и не мог
повторить `GetSlotInst(Slot::Type)`.

Добавлен точный physical `PlayerSlotType`, `Bind` разбирает все десять имён
`stWheel..stWeapon4`, а class lookup возвращает первый совпавший Slot в
физическом порядке. Rack встроен в active `Player`; перенесены Player-owned
`BindSlots`, `GetSlot`, обе формы `GetSlotInst` и `ApplyMobility`. `Race`
удерживает стабильный оригинальный workshop catalog, поэтому active slots не
ссылаются на временные XML-объекты. Racer setup связывает loadout до
`CreateCar`; предрасчёт configured vehicle теперь также проходит через
`Player::ApplyMobility`, а не напрямую через rack-helper. Regression
проверяет одновременно Droid в Weapon1, пустой Weapon2 и Reflector в Weapon3.

### P2.35 — persistent Player WeaponItem owner — выполнено

Подтвердилась следующая потеря исходного ownership: несмотря на перенос
`WeaponRack`, `OriginalRaceSession::updateGameplay` всё ещё конструировал
новый временный `WeaponItem` для каждой readiness-проверки и каждого
primary/hyper/mine выстрела. `Player::ReloadWeapons` создавал ещё один набор
charge-only wrappers. В Windows один `WeaponItem` постоянно живёт внутри
каждого физического `Slot`, а `HumanPlayer`, `Logic`, network shot и lap reload
обращаются именно к этому объекту.

Active `Player` теперь постоянно хранит четыре primary `WeaponItem`, Hyper и
Mine непосредственно в физических `Slot`, связывает их с собственными `WeaponRack` и charge storage после
формирования loadout и одновременно настраивает производные
Droid/Reflector. `ReloadWeapons`, Human selection/ShotAll, AI, mine, hyper и
сетевые shot transactions используют эти же экземпляры; три фабричные
лямбды, создававшие временные предметы в горячем пути сессии, удалены.
Player regression проверяет устойчивую identity объектов и то, что изменения
заряда через bonus/reload видны тому же экземпляру; lifecycle regression
теперь также связывает реальные WeaponItem до `Race::StartRace`.

### P2.36 — Slot-owned polymorphic weapon items — выполнено

Продолжение аудита подтвердило, что P2.35 ещё оставлял два объекта на один
слот: `PlayerSlotRack::Slot` содержал безликий `SlotItem`, а рабочий
`WeaponItem` находился рядом в отдельном Player-массиве. Это всё ещё не
соответствовало `eff9338:Player.h`, где `Slot::CreateItem` создаёт один
полиморфный `HyperItem`, `MineItem`, `WeaponItem`, `DroidItem` или
`ReflectorItem`.

`WeaponItem` теперь наследует portable `SlotItem`, а `Slot::CreateItem`
создаёт все пять исходных weapon-классов по serialized `Slot::Type`.
`Player::BindWeaponItems`, reload, Human/Logic shot selection, Droid
create/destroy/progress и первый Reflector работают с объектом из
`PlayerSlotRack`; промежуточные `primaryWeaponItems_`, Hyper/Mine items и
`PlayerItemRack` полностью удалены. `Logic::ResolveDamage` принимает target
Player и выполняет исходный `GetSlotInst(stReflector)` lookup.

Также исправлен профильный owner: при `applyPlayerProfile` физические
`slot0..slot9` человека теперь пересобираются из профиля с точными именами
`stWheel..stWeapon4`. Ранее менялись индексы оружия и charge, но физический
Slot мог остаться от стартового race loadout, из-за чего Droid/Reflector и
визуальная запись расходились. Regression проверяет несколько Slot-owned
Droid с независимым lifecycle, два Reflector с first-slot rule и устойчивую
identity WeaponItem непосредственно внутри Slot.

### P2.37 — Garage::InstalSlot placement and live replacement — выполнено

Сверка с `eff9338:Race.cpp::Garage::InstalSlot` подтвердила потерю ещё одной
части исходного состояния. Portable `OriginalGaragePlacement` сохранял лишь
флаги и строки допустимых предметов, выбрасывая `PlaceSlot::pos` и
индивидуальные `PlaceItem::offset/rot`. Одновременно `SlotItem` получал
preview-позу из `workshop.xml`, хотя Windows после выбора машины обязательно
заменяет её на `place.pos + placeItem.offset` и `placeItem.rot`. Рендерер
оружия отдельно повторно читал эти значения, поэтому графическое и игровое
представления одного физического слота расходились.

Каталог гаража теперь сохраняет полные car-specific placement records, а
`Player::BindSlots` применяет точное крепление активного `Vehicle` к тому же
Slot-owned предмету, который используется оружием и эффектами. Перенесён
`Player::SetSlot`: он удаляет прежний полиморфный предмет, задаёт запись и
трансформацию нового, а при существующей машине отключает старый Droid и
сразу подключает новый. `SlotItem::SetPos/SetRot` и копирование `Slot`
сохраняют уже установленную car-specific трансформацию. Регрессии проверяют
реальные `garage.xml` position/rotation, mount offset и замену живого Droid
на Reflector без параллельного объекта.

### P2.38 — ten-slot mount owner and weapon world transform — выполнено

Следующая проверка активного пути показала, что P2.37 ещё не устранил
параллельное состояние полностью. `Vehicle` загружал из `Garage::Car` только
`stWeapon1..4`, а `stHyper` и `stMine` отбрасывались. Session и bgfx renderer
затем повторно искали primary placement в `Vehicle`, не читая трансформацию
у фактически установленного `SlotItem`. В результате Hyper/Mine создавали
снаряд или эффект от центра машины; видимый HyperDrive не рисовался, а
видимая primary-модель и точка выстрела могли разойтись с Player owner.

`Vehicle` теперь хранит исходный десятиэлементный массив `slotMounts` в том
же порядке, что `Player::SlotType`. Загрузчик читает `active/show/pos` и все
record-specific `offset/rot` для Wheel–Weapon4. Добавлены обе исходные формы
`Player::GetSlotInst`: lookup по физической позиции и по полиморфному
`Slot::Type`. `Player::BindSlots` применяет car-specific transform ко всем
десяти слотам.

Gameplay transform primary/Hyper/Mine теперь строится из Slot-owned item;
vehicle placement остаётся только fallback для старой неполной записи.
Renderer обходит `stHyper..stWeapon4`, поэтому видимый HyperDrive и четыре
primary-предмета используют тот же owner и ту же позу, что выстрел/эффект.
Регрессии проверяют ненулевой Hyper offset, его точную world-space позицию в
attached projectile и полный mount из реального `manticora` в `garage.xml`.

### P2.39 — complete SlotItem car lifecycle — выполнено

Сверка `Player::CreateCar/ReleaseCar` обнаружила ещё один остаток временной
архитектуры: callbacks подключения/отключения автомобиля исполнялись только
для `DroidItem` в четырёх primary-позициях. В Windows цикл проходит по всем
десяти физическим `Slot`; именно `WeaponItem::OnCreateCar` создаёт дочерний
weapon actor (`_inst`), а `OnDestroyCar` освобождает его. Поэтому после
`FreeCar` порт продолжал считать primary/Hyper/Mine установленными и готовыми
к выстрелу, хотя машины уже не существовало.

В backend-neutral `SlotItem` перенесены виртуальные car lifecycle callbacks.
`Player::CreateCar`, `FreeCar`, `BindSlots` и `SetSlot` теперь симметрично
подключают/отключают все десять предметов. `WeaponItem` отдельно хранит
состояние live child actor: `IsInstalled`, `IsReadyShot`, `GetWeapon` и
успешность локального `Shot` требуют активной машины; replicated `newCharge`
по-прежнему применяется даже к неуспешному shot, как в Windows. `DroidItem`
вызывает базовый weapon lifecycle вместе с регистрацией progress callback.
Повторный `CreateCar` больше не создаёт callbacks второй раз для уже живой
машины. Regression проверяет detached/attached/detached/respawn-переходы для
обычного оружия и Droid, включая live `Garage::InstalSlot` replacement.

### P2.40 — WeaponItem charge API in active consumers — выполнено

После lifecycle-аудита подтвердился следующий обход исходного владельца:
HUD, AI attack context, ammunition bonus, selected-weapon state и запись
профиля читали публичные staging-массивы `RacerRuntime`, хотя Windows всегда
обращается к установленному `WeaponItem::GetCurCharge/GetCntCharge`. Это
оставляло два пути доступа к одному заряду и позволяло UI/AI увидеть не тот
предмет после live replacement физического слота.

Восстановлен полный изменяемый API исходного `WeaponItem`:
`SetMaxCharge`, `SetCntCharge`, `SetCurCharge`, `SetChargeStep`, `SetDamage` и
`SetChargeCost`. Active HUD, profile writer, AI primary/Hyper/Mine context,
выбор оружия и `Player::TakeAmmunition` теперь читают и меняют Slot-owned
`WeaponItem`; бонус выбирает только реально неполный установленный предмет и
записывает заряд через `SetCurCharge`. Старые arrays пока остаются только как
инициализационное хранилище до `BindWeaponItems` и compatibility surface для
session regression; удаление этого staging требует отдельной перестановки
порядка profile/loadout binding. Regression проверяет весь mutable API и
общую storage identity с текущим зарядом.

### P2.41 — literal WeaponItem current-charge ownership — выполнено

Прямое сравнение с `source/game/Player.cpp` подтвердило, что предыдущая
«общая storage identity» всё ещё была неверной: Windows `WeaponItem` содержит
собственное поле `_curCharge`, сохраняет и загружает его сам. Указатель на
`RacerRuntime::weaponCharges/hyperCharge/mines` теперь используется только
один раз при `BindWeaponItems` как источник уже разобранного profile/loadout;
выстрел, network `newCharge`, reload и ammunition bonus меняют только
Slot-owned `WeaponItem`.

Убраны последние active чтения staging-полей из AI, lap reload, Hyper/Spring,
Mine, ShotAll, network-shot, selected-weapon и regression paths. Публичные
массивы пока сохранены исключительно как временный результат resource/profile
разбора до создания предметов — после binding они намеренно не синхронизируются
обратно. Regression отдельно доказывает эту границу и повторяет исходные
`Get/SetCurCharge` semantics у подключённого и отключённого автомобиля.

### P2.42 — WeaponItem `_wpnDesc` и projectile-derived stats — выполнено

Следующее прямое сравнение выявило ещё один упрощённый owner: portable
`Weapon::Desc` сохранял только типы снарядов и delay, а `WeaponItem::GetDamage`
возвращал отдельное serialized `damage`. В оригинальном `Player.cpp`
`WeaponItem` владеет полным `_wpnDesc`, применяет его к дочернему `Weapon` в
`OnCreateCar`, а `GetDamage` суммирует `damage` каждого `Proj::Desc`; поле
`_damage` прямо помечено исходником как invalid.

Portable descriptor теперь содержит source-relevant `type`, `speed`,
`maxDist` и `damage` каждого непосредственного projectile. Восстановлены
`GetWpnDesc/SetWpnDesc`, detached fallback `GetDesc`, применение descriptor
при car lifecycle и точная сумма projectile damage (параметр `statDmg` в
исходнике также ничего не меняет — соответствующая ветка закомментирована).
Death-effect projectiles не попадают в item descriptor, поскольку Windows
создаёт их из поведения модели, а не из workshop `projList`.

Одновременно восстановлена загрузка `chargeCost` из `workshop.xml`. AI
primary/Hyper/Mine context теперь получает type/range/speed/oil и readiness
из установленного `WeaponItem/Weapon`, а не из параллельного
`WeaponDefinition`. Resource и integrated regressions проверяют реальные
`bulletGun` (`6`, `2750`) и `rifleWeapon` (`6+6`, `9000`), descriptor apply и
detached/attached поведение.

### P2.43 — полный `Proj::Desc` как источник выстрела — выполнено

P2.42 намеренно начинался с минимального набора полей, достаточного для
статистики и AI. Следующая проверка показала, что оставлять создание снаряда
на `Race::WeaponDefinition` означало бы сохранить второго владельца. Теперь
`Weapon::Desc::projectiles` содержит полную portable-транскрипцию исходного
`Proj::Desc`: transforms, три visual/death-effect описания, collision boxes,
model-size placement, relative speed, angular speed, distance, lifetime,
mass, damage и type-specific nested projectile data.

Primary, Hyper и Mine preparation берут descriptor непосредственно из live
`WeaponItem::GetWeapon()->GetDesc()`. Resource `WeaponDefinition` остаётся
стабильным каталогом GPU/effect assets и индексным мостом для bgfx/Jolt, но
не определяет параметры создаваемого gameplay-снаряда. Для generated
death-effect projectiles сохранено отдельное source-index mapping: они не
входят в workshop `_wpnDesc`, но их renderer/effect assets остаются доступны.

Integrated regression после binding изменяет только `_wpnDesc` установленного
предмета и подтверждает, что реально созданный projectile получает новые
speed `77`, maxDist `321` и damage `9.25`; resource-копия при этом не меняется.
Это закрывает прежний формальный перенос descriptor, при котором getters
были source-подобными, а сам выстрел продолжал обходить owner.

### P2.44 — lifetime копии `Proj::Desc` — выполнено

В Windows `Weapon::CreateShot/PrepareProj` передаёт конкретный `Proj::Desc`
создаваемому объекту: уже летящий снаряд не должен начать читать новый
descriptor после замены предмета или повторного `SetWpnDesc`. После P2.43
portable runtime корректно создавал снаряд из slot-owned данных, но update,
death/MineRip и renderer ещё возвращались к static resource-каталогу по
индексу — то есть lifetime исходной копии оставался неперенесённым.

`WeaponItem` и live `Weapon` теперь разделяют immutable descriptor handle,
который фиксируется в `ProjectileRuntime/MineRuntime` при подготовке. Все
gameplay и visual branches читают этот снимок до уничтожения объекта. Для
generated death projectile сохранён static source-index fallback, поскольку
он не входит в workshop `_wpnDesc`. Общий handle намеренно исключает глубокую
копию object/particle/effect graph для каждого быстрого projectile и тем самым
не создаёт новый источник просадки FPS.

Integrated regression после первого выстрела устанавливает в предмет другое
описание (`5/6/1`) и проверяет, что старый runtime сохраняет отдельный handle
и первоначальные `77/321/9.25`. Полная Debug-сборка, 13 offline tests,
resource audit, map1 Jolt physics и 240-frame Metal race smoke проходят.

### P2.45 — полная схема profile/config и `CompletePlanet` load — выполнено

Прямая сверка `GameMode::SaveGameOpt/LoadGameOpt`, `Race::SaveGame/LoadGame`,
`SnProfile` и `SkProfile` подтвердила полный набор реально сохраняемых полей.
Закомментированные в Windows `SaveWorkshop/SaveGarage/SaveAIPlayers` не были
ошибочно объявлены отсутствующей функциональностью: source profile сохраняет
assortment через tournament progress, а установленную комплектацию — через
десять `human/slotN` references и charge attributes.

Обнаружился один настоящий разрыв. Windows `Race::LoadGame` применяет каждый
индекс `planetsCompleted` через `CompletePlanet`, а portable loader напрямую
копировал числа. Поэтому загрузка записи `4` не открывала скрытую планету `5`,
а дубликаты оставались в состоянии. Добавлен единый
`completeOriginalPlanet`, используемый и tournament finish, и disk loader;
финальная турнирная планета расширяет hidden range `5..N`, как
`cTournamentPlanetCount == 5`. `lastProfile/lastNetProfile` также теперь
проходят общий `FindProfile`-эквивалент вместо принятия несуществующего имени.

Profile flow regression выполняет disk round-trip всех quality/resolution/
volume/gameplay/control/music полей, обеих profile libraries, tutorial и
completed planets, полного human/car/color/economy/tournament/slot state, а
также achievement items, records, conditions и iterations. Сохранены
source-ключ `dfficulty`, absent `prefCamera` first-run semantics и временность
`SkProfile`.

### P2.46 — точный lifecycle и persistence `MusicCat` — выполнено

Сверка с `GameMode::MusicCat` и `GameMode::DoStartRace/ExitRace/SaveConfig`
подтвердила два активных расхождения. Portable game MusicCat извлекал первый
элемент очереди уже при запуске приложения и при выходе из гонки выполнял
`Pause+Next`; Windows загружает очередь без извлечения, вызывает `Play` только
в `DoStartRace`, а в `ExitRace` делает только `Stop`. Кроме того, portable
profile сохранял первоначальную строку playlist вместо фактически оставшейся
очереди `_playList`.

Инициализация и начало playback теперь разделены. Первый game track заранее
декодируется в фоне без изменения очереди, извлекается ровно при старте гонки
и начинается с PCM-кадра 0. Выход останавливает voice, но не продвигает
playlist. Перед атомарной записью `user.xml` обе текущие очереди экспортируются
обратно в `menuMusic/playList` и `gameMusic/playList`, как исходный `SaveUser`.
Позиция аудио между запусками не сохраняется: runtime `.state` отключён и
остаётся только изолированным M8 smoke-механизмом; Pause/Resume внутри одного
процесса по-прежнему продолжает тот же трек.

M8 audio smoke проверяет все три menu Ogg, shuffle, pause/resume, automatic и
manual Next; M9 Metal regression дополнительно требует отсутствия выбранного
game track до `StartRace` и нулевую позицию сразу после source `Play`.

### P2.47 — serialized `MusicCat::LoadGame` catalog — выполнено

Следующий проход обнаружил, что static menu catalog был взят не из обычного
Windows path, а из `GameMode::ResetGameData`. Эта функция вызывается только
если `game.xml` невозможно открыть. В поставляемой игре конструктор сначала
успешно выполняет `LoadGameData`, а `MusicCat::LoadGame` заменяет fallback
содержимым `game.xml`. Поэтому порт показывал для menu Ogg неверные названия и
исполнителей (`Jet`, `Social Distortion`) вместо сериализованных `Frantick` и
`The Ventures`.

Добавлен platform-independent loader обеих `menuMusic/tracks` и
`gameMusic/tracks` таблиц с точным чтением item reference, name, band и group,
проверкой существования каждого Ogg и строгими XML errors. Runtime MusicCat,
MusicDialog и shuffle grouping теперь используют единый загруженный каталог:
три menu и одиннадцать game tracks. Hard-coded arrays остаются только
историческим missing-file/smoke fallback, как в Windows.

Resource regression требует точные поставляемые counts, пути и menu metadata;
M8 вывод теперь подтверждает `Frantick - Peter Gunn Theme`,
`Frantick - Bad to the Bone` и `The Ventures - Peter Gunn Theme`. Полные M8
audio и M9 Metal прогоны проходят с сериализованным каталогом.

### P2.48 — serialized languages/commentators и `StringLibrary::Load` — выполнено

Продолжение аудита `GameMode::LoadGameData` подтвердило ещё один разрыв того
же типа. Portable UI вручную повторял шесть имён языков и два стиля диктора,
но `OriginalMainMenu` разрешал загружать только English/Russian. Windows
читает порядок, `file`, `locale`, `charset` и `primId` каждого языка, а также
порядок commentator styles непосредственно из поставляемого `game.xml`;
оба stepper используют индексы этих векторов.

Добавлен общий platform-independent каталог этих записей с проверкой XML и
всех шести language files. `StartOptionsMenu` и обычный Sound/Network tab
теперь получают значения из каталога; прежнее бинарное переключение диктора
и hard-coded language array удалены. MainMenu2 загружает именно сериализованный
`Language::file`, поэтому Portuguese, French, Spain и German являются такими
же рабочими путями, как English/Russian.

При прогоне всех локализаций обнаружилась важная особенность данных: во
French `scMaslo` отсутствует закрывающая кавычка. Windows
`StringLibrary::Load` не отвергает файл, а читает token stream до следующей
кавычки. Portable parser теперь буквально повторяет эту семантику, включая
последнее значение duplicate id и `\\n` replacement. Resource regression
проверяет точные 6/2 records, metadata и успешную загрузку каждого языка.

### P2.49 — единый `Commentator::LoadGame` и source player id — выполнено

Portable audio раньше повторно и независимо разбирал
`game.xml/commentator/comments`, используя собственные fallback-значения.
Это создавало второй источник истины рядом с уже перенесённым
`GameMode::LoadGameData`. Кроме того, `Commentator::Generate` получал индекс
машины в portable-векторе. Windows передаёт битовый `Player::GetId`, а события
без `EventData` используют `cUndefPlayerId`; именно этот id управляет
`forHuman`, `repeatPlayer` и `lastPlayer`.

Полная таблица `delay`, 37 comments, busy/repeat flags и все voice records
теперь строго читается единым `OriginalGameData` loader. SDL-компонент только
связывает эти descriptors с выбранным style и доступными Ogg, сохраняя
исходную фильтрацию отсутствующих переводов. Runtime event adapter переводит
индекс racer в настоящий source `playerId`; `raceStartTime2` и `raceFinish`
передаются без игрока. Resource audit проверяет counts и ключевые queue/repeat/
voice параметры. Audio, Jolt, 13 offline tests и 240-frame Metal race smoke
проходят с общей таблицей.

### P2.50 — общий `StringLibrary::Load` и локализация HUD — выполнено

Аудит HUD обнаружил не визуальное приближение, а полностью неработающий
language path. `OriginalRaceHud` передавал UTF-16LE файл в
`ResourceFileSystem::readText`; тот корректно отвергает NUL bytes, после чего
HUD без сообщения проглатывал exception и оставлял английские `Lap`,
`Reward`, places и player names. Поэтому смена языка работала в меню, но не в
HUD/finish overlay.

Точная token-stream транскрипция Windows `StringLibrary::Load` вынесена из
MainMenu2 в общий `OriginalGameData` component. И MainMenu2, и HUD теперь
читают один `Language::file` через binary UTF-16LE decoder и одну таблицу с
last-duplicate-wins/escaped-newline/malformed-French semantics. HUD больше не
скрывает ошибку и не подменяет выбранный язык английскими строками; invalid
serialized language останавливает инициализацию с точной ошибкой ресурса.
Русский 240-frame Metal regression подтверждает `Круг` и `Награда`, а также
локализованные place/name/reward labels и gamer tokens через тот же каталог.

### P2.51 — source identity пунктов RaceMenu/Angar — выполнено

После подключения общей библиотеки строк в журнале русского smoke остались
английские `Start race` и `Planets`. Аудит `RaceMenu2.cpp` показал, что семь
кнопок `RaceMainFrame` в Windows вообще не имеют текста: они создаются с
`svNull` и показывают только исходные иконки. Portable labels нужны лишь как
идентификаторы навигации и диагностического вывода, но всё равно не должны
быть придуманными английскими литералами.

Идентификатор запуска теперь берётся из реально используемого исходным
`LobbyFrame` строкового ключа `svStartMatch`, а переход в ангар — из
`svCasePlanet`. Временный заголовок `planetsPage` использует тот же ключ;
после `AngarFrame::UpdatePlanets` он, как и раньше, заменяется исходными
`svOpen`/`svRequestPoints`/`svClosed`/`svCompleted`. Геометрия иконок и
отсутствие подписей `RaceMainFrame` не менялись.

### P2.52 — полный владелец `ResourceManager::StringLibrary` — выполнено

Общий parser P2.50 переносил только `StringLibrary::Load`, оставляя результат
обычным `unordered_map`. MainMenu, HUD и network dialogs затем независимо
реализовывали lookup/fallback. Это было реальным source-разрывом: HUD
сохранял придуманные английские defaults при пустом значении, сетевой диалог
подставлял собственную английскую фразу, а MainMenu возвращал пустую строку
для `svNull`. Windows `StringLibrary::Get` во всех трёх случаях возвращает
сам id, если запись отсутствует или пуста; `Has` считает пустую запись
отсутствующей.

Введён единый platform-independent `StringLibrary` с исходными `Get`/`Set`/
`Has` semantics и `std::map` ownership. `OriginalMainMenu::Model`, весь
активный `localized()` path и `OriginalRaceHud` используют только его.
Удалены английские fallback `Player` и `Players who left will be removed`:
последнего текста нет ни в одном из шести поставляемых language files, поэтому
оригинальная Windows-сборка показывает id `svHintLeaversWillBeRemoved`.
Resource regression отдельно фиксирует missing key, explicit empty `svNull`
и доступный `svStartMatch`.

### P2.53 — единый `GameMode::LoadGameData` catalog — выполнено

Обратный аудит полного `GameMode::LoadGameData` показал, что Windows одним
reader-ом последовательно загружает `languages`, `commentators`, `menuMusic`,
`gameMusic` и `commentator`. Portable runtime после предыдущих переносов
разбирал тот же `game.xml` независимыми `OriginalGameData` и
`OriginalAudioSpec` loaders, а `OriginalMainMenu` внутри выполнял ещё одно
скрытое чтение language catalog. Значения совпадали, но ownership/call order
не соответствовали исходному классу и позволяли loaders расходиться.

Оба serialized `MusicCat::LoadGame` списка теперь являются частью единого
`OriginalGameData::Catalog`. Основной runtime выполняет один полный parse и
передаёт тот же catalog MainMenu, MusicDialog, menu/game MusicCat и
commentator. Дублирующий TinyXML music parser удалён; старый audio entry point
оставлен лишь совместимым делегатом для изолированного конструктора. Проверка
3 menu + 11 game tracks теперь проверяет данные того же объекта, что и 6
languages, 2 styles и 37 comments.

### P2.54 — `AutodetectLanguage`/`AutodetectCommentatorStyle` — выполнено

После объединения catalog подтвердился first-run разрыв в `LoadGameOpt`.
Portable `UserConfig` заранее содержал строки `english`/`english` и не
запоминал, присутствовали ли соответствующие узлы в `user.xml`. Поэтому
отсутствующий или новый config никогда не вызывал исходные autodetect-ветви:
русская система запускалась с English, а русский commentator автоматически
не выбирался.

`ProfileState` теперь отдельно хранит presence обоих XML-полей, как уже
делалось для `prefCamera` и `discreteVideoCard`. Перенесены обе исходные
policy-функции: language ищется по Windows `PRIMARYLANGID` и при miss берёт
первую serialized запись; commentator выбирает Russian только для
`lcRussian`, иначе English, затем первый доступный style. CoreFoundation
boundary отображает macOS preferred locale на все шесть shipped primary ids
(German/English/Spanish/French/Portuguese/Russian), а не только ru/en.
`SaveConfig`-эквивалент после любого сохранения помечает оба значения как
serialized. Profile и resource regressions проверяют absent/present state,
locale fallback и Russian/French commentator branches.

### P2.55 — `ControlManager` VirtualKey/config ownership — выполнено

Обратная сверка `ControlManager.cpp`, `GameMode::LoadGameOpt/SaveGameOpt` и
`ControlsFrame` выявила, что portable path вручную повторял имена клавиш в
трёх местах. Кроме риска расхождения это уже давало реальные ошибки:
частичный `ctKeyboard`/`ctGamepad` в `user.xml` очищал все constructor
defaults, неизвестные XML children ошибочно сохранялись как actions,
клавиатурный `Back` был придуманным alias для Backspace, а захват стиков и
триггеров использовал пороги 20000/15000 вместо XInput 7849/8689 и 30/255.

Введён единый platform-independent owner точных таблиц
`cVirtualKeyInfo[2][29]`, `cGameActionStr[25]` и constructor bindings.
Профиль теперь накладывает только 25 известных сериализованных actions на
defaults, пропуски сохраняют исходные значения, а unknown/empty key проходит
буквальную семантику `GetVirtualKeyFromName` (первый символ/`None`). Save path
снова пишет все actions в порядке `cGameActionStr`. SDL boundary получает из
того же контракта точные Windows display names, special keyboard mapping и
XInput thresholds; `ControlsFrame` больше не содержит собственной таблицы.
`None` подтверждён как буквальное имя `cVirtualKeyEnd`, поэтому удаление
binding не локализуется.

### P2.56 — полный `GameMode::LoadGameOpt/SaveGameOpt` — выполнено

После ControlManager целиком сопоставлены соседние quality, resolution,
volume, gameplay, language/commentator, camera и discrete-video ветви.
Подтвердились четыре переносных отклонения. Portable loader ограничивал три
volume значениями 0..2 и cameraDistance 0.6..2.5, хотя Windows передаёт
сериализованные float без clamp. Любая строка `frameRateMode` сохранялась,
несмотря на двухэлементный исходный enum (`sfrNone`, `sfrFixed`), а любой
неизвестный `prefCamera` ошибочно считался валидным `pcIsometric`.

Также восстановлено важное различие целого отсутствующего узла и частичного:
без `<quality>` вызывается `Environment::AutodetectQuality` (на Metal это
middle shadow, high environment/light/post, anisotropic 8x, no MSAA,
`sfrFixed`), но существующий частичный узел оставляет пропуски на constructor
low/linear/no-MSAA. Аналогично отсутствие `<volume>` вызывает значения
1.2/0.8/1.2, тогда как пропуски внутри существующего узла сохраняют исходную
громкость XAudio voice 1.0. `SReadEnum` теперь отклоняет invented tokens,
no-clamp float round-trip и обе partial-node ветви закреплены profile smoke.
Удалён последний тестовый токен `sfrVSync`, которого в Windows enum никогда
не существовало.

### P2.57 — `GameMode::ResetConfig` и первый `SaveConfig` — выполнено

Продолжение сверки startup path выявило различие жизненного цикла, которое не
видно при обычном XML round-trip. Windows при недоступном `user.xml` вызывает
`ResetConfig`, после autodetect языка/диктора немедленно выполняет
`SaveConfig`, и только затем `CheckStartupMenu` открывает обязательный выбор
камеры. Portable runtime до этого сохранял файл лишь после нажатия Apply,
поэтому выход из первого StartOptions повторял его на следующем запуске и не
повторял исходную политику восстановления.

`ProfileState` теперь отдельно хранит presence самого `user.xml`, а
`OriginalProfileStore::saveConfig` является точной узкой границей
`GameMode::SaveConfig`: атомарно пишет только `user.xml`, не создавая
`race.xml`, `Profile/*` или `achievment.xml`. Active startup вызывает её после
загрузки `game.xml` и platform locale autodetect, но сохраняет текущие
first-run camera/discrete flags до завершения `CheckStartupMenu`. Regression
проверяет absent/present переход, все serialized presence fields и отсутствие
побочных записей игрового прогресса.

Заодно исправлена обнаруженная этим regression инфраструктурная утечка:
physics/startup/audio/render smoke завершались через общий shutdown и могли
вызвать `saveRaceProfile` в настоящем Application Support; ProfileFrame
fixtures также могли сохранить состояние раньше shutdown. Теперь весь
`OriginalProfileStore` automated-запуска направлен в очищаемый уникальный
временный каталог. Только обычный интерактивный запуск видит настоящий save
directory; fixtures остаются наблюдательными и не создают `user.xml` на
чистом профиле.

Integrated race-render fixture теперь сам создаёт один default championship
profile во временном store. Это сохраняет покрытие Tournament Load,
ProfileFrame и delete dialog при поставляемом пустом `race.xml`, не используя
профили, случайно оставшиеся от ручных запусков.

### P2.58 — `GameMode::OnFinishFrameClose` audio lifecycle — выполнено

Сверка FinishMenu выявила реальное расхождение порядка. Windows в
`ExitRace` останавливает game MusicCat, но оставляет menu MusicCat на паузе
всё время таблицы результатов. Лишь `Menu::OnFinishClose` вызывает
`Commentator::Stop`, `FadeOutMusic(0)` (source gain 0 с возвратом к 1 за одну
секунду) и `_menuMusic->Pause(false)`. Portable path раньше возобновлял menu
music уже при показе результатов и не очищал незавершённую очередь диктора
при закрытии.

`stopRaceAudio` теперь различает переход в FinishMenu и обычное возвращение в
меню. Finish удерживает menu MusicCat на сохранённой позиции; close очищает
voice/queue, запускает music с нулевым gain и повторяет исходную формулу
`gain += (1-gain)*dt/1s` поверх SDL Music bus. Finish regression теперь сам
закрывает frame и требует три состояния: музыка удержана, commentator
остановлен/музыка возобновлена с нуля, fade реально растёт до меню.

### P2.59 — узкая граница `GameMode::Terminate` — выполнено

Следующий соседний метод подтвердил опасное persistence-расхождение. Windows
при MainMenu Exit вызывает `GameMode::Terminate`: сначала только
`SaveConfig(user.xml)`, затем `World::Terminate`; destructor выполняет
`ExitRace(false)` и не сохраняет race/profile/achievement. Portable shutdown
вместо этого вызывал общий `saveRaceProfile`, поэтому закрытие окна посреди
или сразу после гонки могло записать runtime player state, achievements и
выполнить tournament advance, которого исходный Terminate не делает.

Подготовка serializable state отделена от двух операций. Explicit source
границы продолжают вызывать полный `Race::SaveGame`-эквивалент, а process exit
вызывает только `OriginalProfileStore::saveConfig` до остановки MusicCat —
так оставшиеся playlist queues попадают в `user.xml`, но игровой прогресс не
меняется. Automated shutdown выполняет тот же код во временном profile store;
profile regression уже требует, что config-only write не создаёт `race.xml`,
`Profile/*` и `achievment.xml`.

### P2.60 — точная граница `GameMode::ChangePlanet` — выполнено

Сверка перехода из `AngarFrame` выявила преждевременный unlock в portable
adapter. Windows вызывает `Planet::Unlock` только если завершена текущая
планета и выбран ровно `Tournament::GetNextPlanet`; затем
`Tournament::ChangePlanet` отдельно выполняет `Open`. Portable helper до
этого безусловно вызывал `Unlock` для любого `psClosed`/`psUnavailable`,
поэтому недоступная неследующая планета могла стать открытой и попасть в
profile XML с неверным состоянием.

Helper теперь сначала восстанавливает текущие planet/pass/track в перенесённом
`source::Tournament`, принимает реальный `Race::GetPlanetChampion` state и
разрешает `Unlock` только следующему миру. Обычная закрытая планета открывается
самим `Tournament::ChangePlanet`; недоступная неследующая остаётся
`psUnavailable` с исходным recovery-pass 1; повторный выбор текущей планеты не
меняет её state. Regression фиксирует все три ветви и champion-переход.

### P2.61 — `Planet::StartPass` skirmish loadout — выполнено

Прямое сравнение `GameMode::StartMatch`, `Race::CreatePlayers` и
`Planet::StartPass` подтвердило, что portable roster создавал нужное число
соперников, но оставлял им campaign-комплектацию текущего прохода. В Windows
каждый компьютер skirmish после установки исходной машины/слотов проходит
`Garage::MaxUpgradeCar`: четыре mobility-слота заменяются уровнем
`upgradeMaxLevel`, весь установленный боезапас заполняется до максимума, а
primary weapon mounts выше `weaponMaxLevel` очищаются.

Перенесён тот же порядок на записи `garage.xml`/`workshop.xml`, после чего
configured vehicle AI пересчитывается через уже перенесённый
`Player::ApplyMobility`. Human и сетевые opponent-id не затрагиваются.
Resource regression проверяет уровень всех четырёх апгрейдов, удаление
Weapon2..4 при лимите 1, полный боезапас Hyper/Mine/weapon и неизменность
человеческой комплектации.

### P2.62 — lifecycle `Race::_minDifficulty` и общий `ExitRace` — выполнено

Обратная сверка `Race::EnterProfile`, `SnProfile::LoadGame/SaveGame`,
`Race::ExitRace` и `Menu::DoPlayFinal` выявила две связанные потери. Новый
portable-профиль начинал с `minDifficulty=gdEasy`, хотя Windows использует
sentinel `cDifficultyEnd`; кроме того, естественный финиш не выполнял общий
profile-lifecycle `ExitRace`, поэтому tutorial stage изменялся только при
досрочном выходе, а минимальная реально использованная сложность не
обновлялась вообще. Это делало итоговые Easy/Normal/Hard champion conditions
недостоверными.

Новый профиль теперь начинает с `cDifficultyEnd`, а единая идемпотентная
граница естественного и досрочного завершения один раз повышает tutorial
stage и понижает `minDifficulty` до сложности активного профиля перед
сохранением. Временный SkProfile выполняет тот же runtime-переход, но
существующая source-граница persistence возвращает championship player;
изолированный FinishMenu smoke состояние не меняет. Profile regression
проверяет последовательность Hard -> Normal -> Hard, неизвестный token,
новый профиль и отсутствие утечки skirmish-состояния в кампанию.

### P2.63 — `Garage::_carChanged` и сетевой `Race::GetTotalPoints` — выполнено

Сверка `Garage::BuyCar`, `Race::GetTotalPoints` и
`Planet::GetRequestPoints` выявила два связанных расхождения состояния
кампании. Portable garage перезаписывал `carChanged` при каждой смене машины
и считал достаточным непустое имя прежней записи. Windows только поднимает
флаг, только для Human в campaign и только если прежняя машина действительно
найдена; сброс принадлежит `Race::EnterProfile`. Это поведение восстановлено,
включая сохранение ранее поднятого флага и отсутствие ложного изменения для
невалидной старой ссылки.

Кроме того, tournament advance передавал только очки локального XML-профиля.
Исходный `Race::GetTotalPoints` суммирует очки всех активных Human и сетевых
Opponent, а `Planet::GetRequestPoints` масштабирует порог по их числу.
Active `OriginalRaceSession` теперь предоставляет обе величины из
Player-owned state; уничтоженный `NetPlayer` исключается, даже если его
portable storage ещё удерживается до смены сцены. Finish path использует эти
значения для `Tournament::CompleteTrack`. Regression проверяет двух игроков,
точный повышенный порог 300 очков, вклад удалённого Opponent и исключение
отключившегося игрока.

### P2.64 — порядок сетевого `NetRace::ExitRace` — выполнено

Прямая сверка `NetRace::ExitRace`, `NetRace::OnExitRace` и
`Race::CompleteRace` выявила, что portable host отправлял RPC до выполнения
`GameMode::ExitRace`: в пакет попадали старый track и ещё не сброшенные очки
завершённого прохода. Поле track дополнительно ошибочно бралось из прежнего
network snapshot как глобальный catalog index, хотя Windows сериализует
planet-local `Tournament::GetCurTrackIndex` уже после
`Tournament::CompleteTrack`.

Host теперь сначала завершает и ранжирует гонку, применяет награды и переход
турнира, сбрасывает очки всех оставшихся в `Race::_playerList` игроков на
границе прохода и только затем публикует новый planet-local track, текущую
погоду и результаты. Disconnected tombstones не сбрасываются, поскольку
исходный `NetPlayer` к этому моменту уже удалён из списка. Receiving client,
как и Windows `OnExitRace`, выполняет тот же tournament transition в
транзиентном network profile, но не сохраняет host-owned состояние в свой
offline профиль. Regression фиксирует planet-local mapping, исключение
удалённого игрока и точный сброс очков активных Human/Opponent.

### P2.65 — исходный восьмиместный состав и эффективные круги — выполнено

Сверка `Race::CreatePlayers`, `Planet::StartPass`, `GameMode::StartMatch`,
`NetRace::StartRace`, `NetRace::OnConnected` и `NetRace::WriteMatch`
подтвердила системное ограничение portable-слоя шестью участниками. В
Windows общая константа `Race::cMaxPlayers` равна 8, скirmish допускает
семь компьютеров, а отдельные campaign-границы равны шести участникам и
трём Human. Планета сериализует только `cComputer1..cComputer5`: слоты 6 и
7 циклически используют эти исходные записи, но сохраняют собственные
player/gamer ID, глобальные имена, цвета и последовательные MapObj ID.

Portable Race теперь хранит пять неизменяемых компьютерных шаблонов отдельно
от активного списка и точно воспроизводит увеличение, уменьшение и повторное
увеличение `Race::_players` до восьми машин. UI и network match принимают
диапазоны 2..8 игроков и 0..7 компьютеров; сетевой roster располагается по
общему `NetModel::modelId`, а не по локальному owner-флагу, и получает
канонические динамические MapObj ID. Campaign по-прежнему применяет свои
границы 4..5 AI и до трёх Human на `StartRace`.

Также разделены две ранее смешанные величины: `Tournament::_lapsCount`
всегда остаётся сериализуемой настройкой матча, тогда как эффективное число
кругов в campaign берётся из `Planet::Track::numLaps`; в skirmish используется
настройка Tournament. Resource regression проверяет обе ветви, точное
переиспользование шаблонов для ID 6/7 и восстановление семи AI после shrink;
network regression поднимает полный восьмиместный матч.

### P2.66 — `NetRace` roster и synchronisation gates — выполнено

Продолжение сверки `NetRace::StartRace`, `NetRace::CheckGoWait`,
`NetRace::CheckFinish`, `NetGame::RegPlayer` и `NetGame::UnregPlayer`
обнаружило зависимость portable-кода от порядка `unordered_map`. Windows
хранит `_aiPlayers` в insertion-ordered `List`, создаёт компьютеров по
возрастанию `cComputer1+i` и при уменьшении состава удаляет именно
`_aiPlayers.back()`. Portable собирал AI из hash-map и удалял последний
элемент её нестабильной итерации, поэтому после изменения опций мог оставить
непрерывно не тот набор ID, например `cComputer2` без `cComputer1`.

Перед удалением AI теперь восстанавливается исходный порядок по player ID и
model ID; shrink всегда удаляет наибольший активный `cComputer`, а regrow
создаёт точный непрерывный диапазон. Regression проверяет переход 2 → 1 AI и
повторный полный матч с двумя Human и `cComputer1..6` при лимите восьми мест.

Кроме того, из монолитного main перенесены собственно правила двух source
методов. `CheckGoWait` на host ждёт только удалённых `netOpponents` — локальный
Human вызывает проверку своим `cRaceStartWait`, но не входит в её цикл.
`CheckFinish`, напротив, требует `RaceFinish` от всех Human/Opponent, включая
host. Runtime теперь спрашивает эти условия у `OriginalNetworkModels`, где
находится исходный NetPlayer graph; disconnect автоматически меняет результат
за счёт удаления модели. Loopback regression отдельно фиксирует обе разные
границы и запрещает client-side принятие host-решения.

### P2.67 — `NetPlayer::ResponseStream` graph authority — выполнено

Повторная сверка активного receive path выявила, что исходный код вычисляет
rotation delta от `GameCar::GetGrActor().GetRot()`, а не от физического actor.
Portable `GameObjectFrameSync` делал это правильно, но результат терялся:
Jolt backend самостоятельно повторял проверку `pi/24` уже относительно body.
Во время незавершённого визуального сглаживания две позы различаются, поэтому
backend мог пропустить требуемый source snap и позднее показать резкий доворот.

Решение игрового graph-слоя теперь явно проходит через physics boundary.
Также application adapter проверяет фактический `Player::GetFinished()` перед
применением новой revision, как исходный `ResponseStream`, а не полагается
только на более поздний сетевой флаг. Physics regression фиксирует малый body
delta при одновременно превышенном graph delta и требует точную установку
сетевого quaternion.

### P2.68 — `SoundMotor`/`PxWheelSlipEffect` Source3d lifecycle — выполнено

Прямая сверка `Source3d::Play`, `Source3d::ApplyX3dEffect`,
`EventEffect::OnProgress` и `PxWheelSlipEffect::OnProgress` выявила два
расхождения application adapter. Engine voice при создании считался уже
запущенным, хотя Windows только выделяет `Proxy` и запускает его строго внутри
30 м; поэтому источник, впервые появившийся в зоне 30..45 м, обходил исходную
гистерезисную границу. Начальное состояние proxy теперь stopped, после чего
общая source-формула 30/45 м решает start/stop.

Звук скольжения также ошибочно следовал за `NxWheelContactData::contactPoint`.
Источник использует эту координату только для визуального следа/дыма, а
`EventEffect` каждый кадр ставит `Source3d` в world position объекта
`CarWheel`. Portable emitter теперь получает вычисленную Jolt-позу колеса;
визуальный эффект по-прежнему остаётся в точке контакта. Это устраняет
дрожание и прерывание tyre loop от нестабильного contact sample, не меняя
исходные slip thresholds 0.4/0.7 и множитель громкости 4.

### P2.69 — исполняемый `ControlManager` action path — выполнено

Сверка всего `ControlManager.cpp` обнаружила, что SDL adapter после загрузки
source bindings продолжал добавлять собственные действия: W/S перемещали
меню, Space подтверждал пункт, Backspace возвращал назад, левая кнопка мыши
стреляла, колесо мыши меняло оружие, а левый stick всегда рулил и ходил по
меню. Ни одного из этих alias нет в `OnKeyEvent`, `OnMouseClickEvent` или
`UpdateControllerState` Windows-игры.

Adapter теперь начинает с тех же constructor defaults и создаёт игровые
actions исключительно по двум таблицам `_gameKeys`. Raw Up/Down/Enter и
mouse hit остаются platform-представлением навигации `Menu`, но не дают
побочных игровых команд. D-pad, кнопки, triggers и sticks работают только
через назначенный `VirtualKey`; B снова является `gaBreak`, а не глобальным
Back. Порог trigger точно равен 30/255; stick использует исходные 7849/8689,
включая намеренно различающиеся activation/normalization thresholds правых
directional keys.

XInput-особенность сохранена явно: `GetGameActionState` в Windows опрашивает
первый доступный slot, но `XInputGetKeystroke(XUSER_INDEX_ANY)` принимает
кнопочные события любого controller. Поэтому portable hot-plug/event layer не
фильтрует вторичные устройства как якобы «неисходные»; окончательное
разделение polled state и ANY keystrokes остаётся отдельной backend-границей.

### P2.70 — `MusicCat` clean-start/queue/pause lifecycle — выполнено

Повторная сверка `GameMode::MusicCat`, `ResetConfig`, `LoadUser` и
`SaveUser` подтвердила, что чистый Windows-профиль начинает с двух пустых
очередей. Прежние portable defaults `2` и `6,10,4,7,1,9,8,3,5,0` были
придуманным snapshot и принудительно выбирали не исходный первый трек. Теперь
первый `user.xml` содержит пустые `playList`, а первый menu `Play` создаёт
случайную перестановку тем же алгоритмом `MusicCat`.

`LoadUser` в источнике не валидирует и не дедуплицирует числа. Portable parser
теперь сохраняет этот порядок буквально, а `Play` отбрасывает некорректные
номера только при извлечении. Фоновый decoder проверяет границы независимо и
не может индексировать повреждённую очередь.

Наконец, исходный `Pause(true)` запоминает PCM position и вызывает
`StopMusic`, а не удерживает paused XAudio voice. SDL-голос теперь также
удаляется и при `Pause(false)` создаётся заново с сохранённого кадра; smoke
фиксирует отсутствие активного menu/game voice на FinishMenu.

### P2.71 — `MapObj/MapObjects` active ownership — выполнено

Прямая сверка `MapObj.cpp`, `MapObj.h` и активных decoration/bonus путей
показала, что session хранил три независимых массива: `DestrObj`, базовый
`GameObject` и `AutoProj`. В Windows это один типизированный `MapObj`, а
`MapObjects` задаёт owner/parent, глобальный ID, record/category, special-list
и момент удаления после `OnProgress` уже умершего объекта.

Backend-neutral `source::MapObj/MapObjects` теперь воспроизводит порядок всех
шести `GameObjType` и семи category, переносит общий `GameObject` state при
смене типа, реально создаёт `DestrObj` и объединяет projectile lifetime с
`AutoProj`. Активная гонка создаёт в нём все decoration и bonus placements с
их исходными global `mapObjectId`; отдельные `decorationObjects_`,
`bonusObjects_` и `bonusProjectiles_` больше не являются параллельными
владельцами.

Удаление также возвращено в исходную фазу: bonus получает `Death` при
подборе/контакте и удаляется после следующего progress callback, разрушаемая
декорация сначала выпускает свой destruction list, затем освобождает slot, а
выход из гонки вызывает Destroy для всего оставшегося графа. Stable slot
сохраняет индекс placement для Jolt/bgfx/network adapters после удаления.
Отдельный regression проверяет type-state Assign, unique names, source
`Misc/Crush` special filter, callback-before-delete и полную Death/Clear
семантику.

### P2.72 — runtime `Map` category/ID registry — выполнено

Следующая сверка `Map.cpp` показала, что `mapObjectId` оставался лишь числом в
парсерных структурах. Сетевые lookup-функции линейно сканировали разные
массивы и не видели source-фазы `lsDeath`: Windows хранит семь
`MapObjList` и одну ordered table `_objects`, где ID 0 означает null, мёртвый
объект скрывается обычным lookup, но доступен с `includeDead` до следующего
container progress.

Добавлен backend-neutral `source::Map`. Он владеет всеми семью category
lists, назначает или принимает исходный global ID, запрещает ноль/дубликаты,
удаляет registry entry через callback контейнера, реализует `DelMapObj`,
`GetMapObjCount`, `GetSemaphore`, `Clear` и сброс `_lastId`. Race session
теперь регистрирует в нём все 234 decoration, 52 track, 7 bonus и стартовые
car objects первой карты; network decoration/car/bonus resolution идёт через
единый реестр, а не через три `find_if`.

Regression отдельно фиксирует source-окно dead/includeDead/remove, category
ownership, exact semaphore lookup, explicit и автоматически следующий ID,
duplicate rejection и reset ID до единицы. Session regression проверяет
реальные IDs map1 и исчезновение разрушенной decoration из реестра. Остался
отдельный следующий шаг: `Player::FreeCar/CreateCar` должен удалить car
`MapObj` и выдать новый ID при respawn, как в Windows; текущий первый ID уже
точен, но пока остаётся статичным в течение заезда.

### P2.73 — динамический car `MapObj` death/respawn lifecycle — выполнено

Сверка `Player::OnDestroy`, `FreeCar`, `OnProgress` и `CreateCar` закрыла
оговорку P2.72. В Windows взорванная машина немедленно удаляет свой `MapObj`
из `Map`, две секунды не имеет ID, затем после вычисления reset pose создаёт
новый car object с очередным `_lastId`. Прежний descriptor `Racer::mapObjectId`
ошибочно оставался неизменным весь заезд и мог направить сетевой Shot в уже
удалённую машину.

Session теперь хранит конкретный car `MapObj` каждого Player. `destroyRacer`
и network disconnect удаляют его из registry; `QueueRespawn` работает без
car ID, а `ActivateCar` создаёт новый `gotRockCar` и выдаёт следующий ID.
Обычный ручной `ResetCar` не пересоздаёт actor и сохраняет ID. Исходящая
сетевая цель Shot берётся из текущего session registry; входящий ID уже
разрешается тем же `Map::GetMapObj`.

Physics regression фиксирует весь переход: initial nonzero ID, отсутствие
старого ID сразу после death, ноль во время restore, строго больший ID после
`CreateCar(false)`, недоступность старого ID и неизменность ID при ручном
reset. Network regression отдельно проверяет удаление car ID при disconnect.

### P2.74 — полное runtime-владение `Map::Trace/ground` — выполнено

После P2.72 реестр уже принадлежал `source::Map`, но два объекта исходного
класса оставались отдельными полями `OriginalRaceSession`: четырёхполосный
`Trace` и `TouchDeath` плоскости Z=0. Это сохраняло поведение, но не
оригинальную границу владения `Map::Map(World*)` и позволяло этим состояниям
расходиться при дальнейших изменениях мира.

`source::Map` теперь постоянно владеет ground `MapObj`, его `TouchDeath` и
`Trace(4)`. AI system, `Player::CarState`, поиск длины пути, lap progression
и Jolt contact adapter получают один `Map::GetTrace`; пересечение физической
плоскости вызывает `Map::GetGroundTouchDeath`. Платформенные части остаются
адаптерами: Jolt представляет бесконечную plane shape, а gameplay death type
и owner соответствуют Windows.

Сохранена важная семантика оригинала: `Map::Clear` сбрасывает семь списков и
ID namespace, но не уничтожает постоянные ground и Trace. `buildSourceTrace`
очищает и загружает Trace отдельно, как source `Map::Load`. Regression
проверяет owner/ID ground, четыре полосы, сохранение Trace через `Clear` и
настоящий `dtDeathPlane` contact.

### P2.75 — `GameObject/MapObj/Player` association graph — выполнено

Оригинальные damage, weapon и contact пути определяют участника цепочкой
`GameObject::GetMapObj()->GetPlayer()`. Portable `MapObj` вместо неё хранил
придуманный `playerId_` с индексом вектора, а `GameObject` вообще не знал
свой `MapObj`. Даже после динамических ID это оставляло два несвязанных
графа и вынуждало network lookup доверять служебному `sourceIndex`.

Возвращены обе исходные стороны связи: каждый созданный или заменённый по
`GameObjType` объект получает `GameObject::_mapObj`, car object получает
`MapObj::_player`, а удаление разрывает ссылки до уничтожения. Portable
`ReplaceRef` не воспроизводит legacy refcount — временем жизни Player владеет
race collection, но identity и callback path теперь совпадают с Windows.

`racerForMapObjectId` разрешает ID через `Map`, проверяет category и находит
участника по реальному `MapObj::GetPlayer`, а не по адаптерному индексу.
Regression меняет `gotGameObj` на `gotDestrObj` и подтверждает, что player и
обратная ссылка нового concrete object не потерялись.

### P2.76 — `LogicBehaviors` contact owner — выполнено

Активная `PairPxContactEffect` уже повторяла source cursor/release rules, но
ошибочно принадлежала непосредственно `OriginalRaceSession`. Четыре
глобальных диапазона `touchBorderDamage`, `touchBorderDamageForce`,
`touchCarDamage`, `touchCarDamageForce` также читались прямо из parser
структуры `Race`. В Windows ими и коллекцией behaviors владеет `Logic`.

`source::Logic` теперь является реальным session instance: он хранит все
четыре диапазона, владеет единственной `PairPxContactEffect` и сбрасывает её
sound catalog при старте/выходе гонки. Jolt contact adapter вызывает behavior
через `Logic::GetPairPxContactEffect`, а border/car damage interpolation
читает `Logic` getters, соответствующие исходному API.

Weapon/Logic regression проверяет persistence всех четырёх диапазонов и весь
двухточечный contact lifecycle уже через владельца `Logic`; physics smoke
покрывает фактический border/car путь.

### P2.77 — `MapObjList::InsertItem` назначает `GameObject::Logic` — выполнено

В Windows `Map::MapObjList::InsertItem` до выдачи ID вызывает
`value->GetGameObj().SetLogic(world->GetLogic())`. Portable registry выдавал
ID и owner, но не создавал эту связь. Поэтому восстановленные
`GameObject::_mapObj` и `MapObj::_player` ещё не замыкались на глобальный
damage/contact owner.

`source::GameObject` снова имеет `GetLogic/SetLogic`; source `Assign`
сохраняет эту ссылку при замене concrete type. `source::Map` принимает общий
`Logic` owner и назначает его каждому объекту при Add, до регистрации ID.
`OriginalRaceSession` конструирует `Map(&logic_)`, поэтому decorations,
track, bonuses и все динамические cars используют один Logic instance.

Как и в исходнике, постоянный ground `MapObj`, созданный вне семи
`MapObjList`, Logic-ссылку не получает. Map regression проверяет этот случай,
обычный inserted object и сохранение Logic после `gotGameObj → gotDestrObj`.

### P2.78 — `GameObject` parent/children/include graph — выполнено

Из оригинальных `GameObject.cpp` и `MapObj.cpp` перенесены `_parent`,
`_children` и принадлежащий объекту `IncludeList`. `InsertChild`,
`RemoveChild`, `ClearChildren` и `SetParent` теперь образуют настоящий
не-владеющий граф, а `SetLogic` рекурсивно передаёт исходного владельца всем
дочерним объектам. Вложенные `MapObj` снова принадлежат
`GameObject::GetIncludeList()` и удаляются до уничтожения родителя.

Parent-связь больше не хранится вторично в `MapObj`: она принадлежит только
concrete `GameObject`. Это возвращает единый источник истины оригинала.
`OriginalGameObjectSmoke` проверяет двухуровневое наследование `Logic`,
detach и очистку include list; `OriginalMapObjSmoke` проверяет граф при
удалении. Точная семантика type replacement отдельно восстановлена в P2.82.

### P2.79 — `GameObject::LogicInited/LogicReleased` и `AutoProj` — выполнено

Оригинальный `AutoProj` является наследником `Proj/GameObject`, а не
параллельным объектом рядом с ним. Portable-модель теперь повторяет эту
иерархию: `MapObj` с `gotProj` владеет конкретным `AutoProj` через общий
`GameObject`, а `GetAutoProj` выполняет только typed view.

`GameObject::SetLogic` восстановил source-порядок: освобождает старое
concrete-состояние, назначает владельца дочерним объектам и затем вызывает
`LogicInited`. `AutoProj` готовит map-projectile только при реальном Logic,
освобождается при detach и повторно подготавливается после загрузки нового
type. Ручной вызов из `OriginalRaceSession` удалён. Weapon и MapObj
regressions проверяют ожидание Logic, автоматический init/release и oil
arming/scale через активный object graph.

### P2.80 — `Logic::_gameObjList` transient owner — выполнено

Из `Logic.cpp` перенесён отдельный владелец временных `GameObject`, который
Windows использует для каждого успешно подготовленного быстрого projectile.
`RegGameObj` принимает владение, не допускает повторной регистрации и
назначает общий `Logic`; `ProgressGameObjs` сначала выполняет исходный
`OnProgress`, затем удаляет `lsDeath`; `CleanGameObjs` разрывает Logic-связи
и уничтожает всё при завершении гонки.

Это намеренно не смешано с семью `MapObjList`: map-projectile `AutoProj`
остаётся объектом карты, а transient list имеет отдельную семантику
оригинального `Weapon::CreateShot`. Weapon regression проверяет duplicate
registration, строгую границу `_timeLife > _maxTimeLife`, удаление и clean.

### P2.81 — `MapObjRec/MapObjLib` stable proxy identity — выполнено

Строковое поле `MapObj::record` больше не является единственным runtime
представлением базы. Для каждой из семи source-категорий `Map` владеет
отдельным `MapObjRecordLibrary`; запись фиксирует path, parent, category и
serialized `GameObjType`. Повторный `GetOrCreateRecord` возвращает тот же
адрес, а попытка назначить одному path другой concrete type отклоняется как
повреждённая база.

Активный `Map::AddMapObj` теперь создаёт объект через record proxy и хранит
обратную identity в `MapObj`. Смена live concrete type не теряет исходную
запись, а `Map::Clear` уничтожает экземпляры и ID namespace, но сохраняет
каталог, как Windows `DataBase`. Map regression проверяет category/type,
FindRecord/shared identity, mismatch и persistence каталога через Clear.
XML `SerialNode/RecordLib` writer остаётся parser boundary, а не дублируется
в gameplay owner.

### P2.82 — `MapObj::CreateGameObj/GameObject::Assign` без ложного state copy — выполнено

Повторная сверка выявила прежнюю portable-ошибку: при смене `GameObjType`
использовался полный C++ `operator=`, сохранявший life, death, name и parent.
Windows `GameObject::Assign` переносит только Logic/serialization flags, а
`MapObj::CreateGameObj` затем явно ставит пустые name, parent и component
owner. Поэтому type replacement в рабочем коде выполняет новый узкий
`AssignSource`; C++ value copy оставлен отдельно только для backend-контейнеров
`RaceEffect`.

Имя также возвращено исходному владельцу `GameObject`; `MapObj::GetName` и
`SetName` теперь только делегируют. Отдельное поле `MapObj::name_` удалено.
Regressions подтверждают, что AssignSource сохраняет Logic, но не копирует
life/name, а concrete replacement сбрасывает name/life/parent, сохраняя
связи самого MapObj с Player, record proxy и ID.

### P2.83 — `.r3dMap` instance name и proxy lifetime state — выполнено

Windows `MapObjects::LoadItem` загружает proxy-поля concrete `GameObject`, а
затем назначает имя из XML element (`semaphore0`, `track20`, `money0`).
Portable parser сохранял только record/transform и подставлял полный record
path как имя. Также терялись `life`, `maxTimeLife` и `timeLife` placement-а.

`ObjectInstance` и `BonusInstance` теперь хранят эти поля и признак реальной
proxy-загрузки. Оба map loading paths читают element name и три source
lifetime value. `OriginalRaceSession` сначала применяет record maximum life,
затем proxy override и имя — в порядке `MapObjRec::Load`/`LoadProxy`/
`LoadItem`. Synthetic regression fixtures без proxy state сохраняют прежнюю
явную инициализацию. Parser regression проверяет реальные `semaphore0`,
`track20`, `money0` и bonus `maxTimeLife=0` из `map1.r3dMap`.

### P2.84 — source record name и глобальный `MakeUniqueName` — выполнено

Windows `MapObjects::Add(MapObjRec*)` использует не полный путь записи, а
leaf `MapObjRec::GetName()`. Затем общий `lsl::Component`-корень карты
выполняет `MakeUniqueName`: всегда добавляет числовой суффикс начиная с нуля
и проверяет все семь категорий, а не только текущий `MapObjects`. Portable
runtime прежде создавал имя из полного record path, оставлял первый base без
суффикса и допускал совпадения между категориями.

`MapObjRecord` теперь отдельно хранит source leaf name. Оба record-based
пути создания используют его; явный record overload извлекает тот же leaf.
Observer карты проверяет глобальный live registry, а owner-based временные
контейнеры проверяют общий `GameObject` child graph. Реализован точный цикл
`base0`, `base1`, ... из `lslComponent.cpp`. Regressions покрывают
`semaphore0`, `track10`, `money0`, одинаковый `smoke` в двух категориях и
пару динамических машин `marauder0/marauder1`; map-file instance names затем
по-прежнему заменяются сериализованными XML-именами на этапе P2.83.

### P2.85 — `RecordLib/RecordNode` runtime hierarchy — выполнено

Исходный `RecordLib::GetOrCreateRecord` делит путь по `\\`, создаёт общие
`RecordNode` и сохраняет в каждой записи стабильные `_lib` и `_parent`.
Portable `MapObjRecordLibrary` прежде индексировал только полный path, а
parent сводил к строке последнего сегмента. Из-за этого относительный source
lookup (`Misc\\semaphore`), parent identity и правила общего namespace
record/node отсутствовали.

Для семи map libraries теперь существует собственный корневой узел с именем
категории (`ctEffects` ... `ctBonus`). Полный portable path нормализуется от
category segment, промежуточные узлы создаются один раз, а `MapObjRecord`
содержит стабильные ссылки на library и parent node. `FindRecord` и
`GetOrCreateRecord` принимают как canonical path, так и исходный относительный
путь и возвращают один объект. Как в `RecordLib::ValidateName`, создание
record поверх node и node поверх record отклоняется. Map regression проверяет
root/Misc hierarchy, library/parent identity, canonical/relative equality и
обе коллизии namespace. XML `SerialNode` ownership и writer всё ещё остаются
за parser boundary.

### P2.86 — `GameObject` proxy transform и `Map::AddMapObj(ref)` — выполнено

`GameObject::SaveProxy/LoadProxy` Windows хранит `pos`, `scale`, `rot`,
`life`, `maxTimeLife`, `timeLife` и include list. Portable `GameObject` до
этого этапа вообще не имел координат: `.r3dMap` transforms существовали
только в параллельных renderer/physics массивах. Поэтому runtime Map нельзя
было считать полноценным владельцем proxy state, а исходный clone path
`Map::AddMapObj(MapObj* ref)` отсутствовал.

В `GameObject` добавлены backend-neutral position/scale/quaternion и узкий
`CopyProxyStateFrom`, не смешанный с C++ value copy или `AssignSource`.
`OriginalRaceSession` назначает transform каждому decoration, track и bonus
MapObj в порядке source proxy load; integrated smoke сверяет реальные первые
placements `map1`. Затем перенесён `Map::AddMapObj(ref)`: он требует record
proxy, создаёт новый объект через общий catalog, получает новый глобальный ID
и source unique name, копирует record `maxLife`, шесть proxy-полей и
рекурсивный serialized include graph. Player association, sourceIndex,
listeners, Logic identity, death/touch runtime и имя исходного instance не
копируются. Map regression покрывает `crate0 -> crate1`, общий record,
новый ID, transform/lifetime и вложенный `sparkChild` с parent/Logic.

### P2.87 — `Map::InsertMapObj` ownership transfer — выполнено

Это не editor-only API: Windows `GameCar::OnProgress` отделяет элементы
`_destrList` и вставляет их в глобальную Map, а `ResurrectObj::OnDeath`
аналогично возвращает вложенный объект из include owner. Исходный
`Map::InsertMapObj` выбирает категорию record-а (`Decoration` без record),
после чего `MapObjList::InsertItem` назначает Logic и новый глобальный ID.
Base collection одновременно даёт перенесённому объекту имя `item0`,
`item1`, ... . Portable unique ownership не имел эквивалента этого raw
pointer transfer.

`MapObjects::Extract` теперь вынимает `unique_ptr` из прежнего owner без
`DestroyObject`, уведомляет старый map registry и разрывает parent graph.
`MapObjects::Insert` принимает только уже detached ownership, назначает
source `itemN` через общий name root и новый parent. `Map::InsertMapObj`
определяет category, выдаёт следующий ID, назначает общий Logic и
регистрирует тот же объект. Regression переносит `DestrObj` fragment из
`GameObject` include list в Map и проверяет pointer identity, сохранённые
record/life/transform, новый owner/name/ID; отдельно покрыт `Decoration`
fallback для объекта без record.

### P2.88 — active `DestrObj::_destrList` separation — выполнено

До этого Jolt/renderer создавали исходные meshes и bodies частей по
параллельному `DecorationDebrisDefinition`, но runtime `DestrObj` содержал
только флаг `_checkDestruction`: его собственный `_destrList` оставался
пустым. Это воспроизводило видимый распад, но не исходный object lifecycle из
`GameCar.cpp::DestrObj::OnProgress`.

`DestrObj` теперь владеет отдельным `MapObjects` destruction list, прогрессирует
его перед обработкой смерти и при первом separation передаёт каждую часть в
`Map::InsertMapObj`. Map observer выполняет transfer до удаления умершего
родителя: pointer/serialized scale и lifetime сохраняются, parent/owner
снимаются, world position/rotation принимаются от родителя, а Map назначает
`Decoration`, новый ID, Logic и глобальное `itemN`. `OriginalRaceSession`
инстанцирует список из каждого `ObjectDefinition::destructionPieces` уже при
source reset. Unit regression покрывает два recordless fragment; integrated
`map1` regression проверяет все 14 частей `crush1`, удаление parent ID и
одноразовую регистрацию новых live MapObj. Jolt остаётся только физическим
backend этих же частей.

### P2.89 — `ResurrectObj` world detach и particle-end death — выполнено

Portable `ResurrectObj::OnDeath` прежде выполнял только `GameObject::Resc()`.
В Windows `Resurrect()` дополнительно получает world position/rotation,
вынимает собственный MapObj из include owner и вставляет его в глобальную
Map; благодаря этому `FxSystemWaitingEnd` переживает уничтожение parent до
исчезновения последней частицы.

В `GameObject` восстановлены backend-neutral `GetWorldPos/GetWorldRot`,
`SetWorldPos/SetWorldRot` и иерархический world scale с parent
scale/rotation/translation. Map-aware overload `ResurrectObj::OnDeath`
теперь после одноразового revive извлекает тот же `unique_ptr`, сохраняет
world pose и передаёт его `Map::InsertMapObj`; повторная смерть остаётся
финальной. Соответствующий overload `FxSystemWaitingEnd` начинает fading, а
существующий particle-end progress завершает объект. Regression использует
повёрнутого и масштабированного parent, проверяет переход include →
`ctEffects`, pointer identity, новый ID/`item0`, сохранённую world pose и
окончательное удаление после нулевого particle count.

### P2.90 — централизованный `Logic::OnProgress` для Map — выполнено

Windows `Logic::OnProgress` не обновляет подряд все семь категорий Map. Он
проходит их в строго заданном порядке: специальный список `ctDecoration`
(только `Misc/Crush`), затем `Effects`, `Car`, `Bonus` и в последнюю очередь
отдельный `_gameObjList` временных объектов. Portable race path ранее
прогрессировал только `Bonus`; timed effects/cars и пассивные специальные
декорации оставались вне общего жизненного цикла.

`Logic` теперь хранит non-owning связь с активной `Map`, которую Map
устанавливает и безопасно снимает при разрушении, и предоставляет единый
source-order `OnProgress`. Он собирает число обновлённых/удалённых объектов
по каждой группе, делегирует работу соответствующим `MapObjects`, затем
обновляет transient owner. `OriginalRaceSession` вызывает этот единый путь
до чтения масштаба бонусов. Regression создаёт timed-объекты во всех семи
категориях и подтверждает, что удаляются только special-decoration,
Effects, Car, Bonus и transient; Architecture, Track, Weapon и Waypoint
остаются нетронутыми.

### P2.91 — `_specialList` и container-lock `MapObjects` — выполнено

Прямая сверка с `eff9338:MapObj.cpp` подтвердила ещё один обход исходной
структуры: portable `OnProgressSpecial` каждый кадр сканировал весь список
`ctDecoration`, тогда как Windows хранит отдельный `_specialList` и меняет
его только из `InsertItem/RemoveItem`. На насыщенных картах это создавало
лишнюю работу, растущую с общим количеством декораций, и оставляло transfer
lifecycle неявным.

`MapObjects` теперь ведёт устойчивый список указателей только на record-parent
`Misc/Crush`. Все три Add-пути и `Insert` регистрируют объект после полной
настройки proxy/category, а `Remove`, `Extract` и `Clear` снимают его до
observer/destruction. `OnProgressSpecial` проходит только этот список и
сохраняет исходное правило callback-before-delete. Удаление и extraction во
время callback запрещены тем же container lock; удаление умершего объекта
выполняется после unlock. Regression проверяет наблюдаемый lock из death
listener, отклонённое reentrant removal и одноразовое удаление после callback.

### P2.92 — `Behavior/Behaviors` owner и recursive include progress — выполнено

Сверка `GameObject.cpp::OnProgress` выявила функциональный разрыв после
восстановления глобального Logic pass: Windows внутри каждого GameObject
сначала обновляет собственный `includeList`, а после lifetime/touch checks —
полиморфный контейнер `Behaviors`. Portable GameObject не вызывал ни один из
этих проходов, поэтому вложенный timed effect мог не завершиться вообще.

Перенесены точный 15-элементный `BehaviorType`, базовый `Behavior` с owner,
Logic lookup и deferred `Remove`, а также owning `Behaviors`: Add/Find/Delete,
автоматическая listener registration, progress-before-next-pass removal и
dispatch `OnShot`, `OnMotor`, `OnImmortalStatus`. GameObject создаёт и
разрушает этот owner в исходном порядке; immortality start/end проходят через
него. `OnProgress` теперь рекурсивно прогрессирует include MapObjects до
touch/lifetime и Behaviors после них, возвращая отдельную диагностику обеих
групп. Regression покрывает serialized type names, порядок damage/shot/motor/
immortality callbacks, delayed self-removal и реальное удаление вложенного
timed MapObj.

### P2.93 — concrete Player behavior graph — выполнено

После появления общего `Behaviors` owner оставался активный обход: Player
напрямую вызывал `ImmortalEffect`/`DamageEffect` из виртуального
`OnDamageEvent`, а `ProgressBehaviors` вручную прогрессировал shield, energy
и low-life state machines в другом порядке. В Windows эти объекты добавлены
в GameCar Behaviors и получают damage/immortality/progress через listener
container.

Player теперь устанавливает три concrete adapter behavior с исходными type
identity и относительным порядком: `LowLifePoints`, `ImmortalEffect`,
`DamageEffect`. Они владеют вызовами уже перенесённых state machines,
записывают одноразовые low-life/energy transitions и передают originating
Behavior в `GameObject::LowLife`. Ручные `OnDamageEvent` и
`OnImmortalStatusEvent` удалены; `ProgressBehaviors` задаёт только входной
low-life threshold, затем читает результат общего GameObject pass. Reset
пересобирает owner, а race roster создаётся без copy-шаблона, чтобы adapter
ссылки каждого Player оставались локальными. Regression проверяет три
зарегистрированных типа/listener и прежние shield/energy transitions.

### P2.94 — dynamic Frost Ray `SlowEffect` behavior — выполнено

Portable Player постоянно хранил и вручную прогрессировал `SlowEffect`, а
Frost Ray напрямую менял это поле. В Windows `Proj::FrostRayUpdate` сначала
ищет `btSlowEffect` в owner, при отсутствии динамически добавляет behavior и
связывает его с `model3`; по окончании вложенного эффекта behavior вызывает
`Remove()` и удаляется следующим проходом контейнера.

Теперь Frost Ray вызывает `Player::AttachSlowEffect`: повторный контакт не
перезапускает lifetime, новый `SlowBehavior` появляется с точным serialized
type, получает скорость через общий `GameObject::OnProgress`, ограничивает её
исходными 20 м/с и помечает себя для deferred removal после освобождения
effect/model ownership. Destroy/Disconnect немедленно очищают динамический
behavior. Regression проверяет создание, запрет повторного attach, ограничение
скорости, release, удаление на следующем проходе и cleanup при уничтожении.

### P2.95 — `GameCar` owner и concrete `SoundMotor` behavior — выполнено

Формулы `SoundMotor` уже совпадали с Windows, но state machine находилась в
SDL application loop и не была частью source object graph. Из-за этого
`GameCar::GetBehaviors().OnMotor`, type identity, progress и destruction
lifecycle оставались формальными, а backend самостоятельно сбрасывал RPM.

Portable `GameCar` теперь является `GameObject`. При `Player::CreateCar` он
создаёт concrete `btSoundMotor` с serialized volume/frequency ranges; motor
callback проходит через общий `Behaviors::OnMotor`, а обычный progress — через
`GameObject::OnProgress`. `FreeCar` уничтожает behavior и source RPM state,
после чего SDL освобождает только две platform voices. Main loop больше не
владеет дублирующим `SoundMotor`; он получает готовый mix через session
boundary. Copy/move car state пересоздаёт локальный listener graph без ссылок
на другой объект. Regression проверяет type/listener identity, callback,
progress, copy rebinding и destruction cleanup.

### P2.96 — `CarWheel` object graph и `PxWheelSlipEffect` owner — выполнено

До этого Jolt contact slip дважды интерпретировался вне source graph: renderer
сам вычислял активность следа/дыма, а SDL держал отдельную state machine для
звука шин. В Windows каждое колесо является `CarWheel : GameObject`, владеет
собственным `btPxWheelSlipEffect`, а оба backend-результата исходят из одного
`OnProgress` этого behavior.

`GameCar` теперь создаёт все serialized колёса как child GameObjects. Только
колёса с source type 9 получают concrete behavior; первое из них сохраняет
процедурно назначенный `SkidAsphalt`, остальные остаются visual-only. Session
передаёт Jolt `contact/longitudinalSlip/lateralSlip`, затем один раз выполняет
car/wheel behavior graph. Metal и SDL читают общий `WheelSlipProgress`, не
повторяя thresholds или lifetime. Motor callback также перенесён из audio loop
в session pass, поэтому source state больше не зависит от наличия SDL audio.
Copy/move пересобирает parent/listener связи; `FreeCar` удаляет wheels до car
behavior. Regression проверяет hierarchy, exact type, silent wheel, единый
transition, copy rebinding и полный cleanup.

### P2.97 — car include graph для `GusenizaAnim`/`PodushkaAnim` — выполнено

Гусеница и подушки визуально использовали исходные формулы, но state хранился
в renderer и зависел от количества renderFrame-вызовов. Их serialized child
MapObj, parent GameCar и concrete types 13/14 отсутствовали, поэтому это была
верная картинка без исходного object lifecycle.

`GameCar` теперь создаёт included animation children из загруженного ctCar:
один `btGusenizaAnim` для гусеничного actor и два `btPodushkaAnim` для tags
1/2 одного cushion actor. Session вычисляет первый lead-wheel axle speed с
радиусом и source dead-zone 0.1 м/с. Child behaviors прогрессируют из car pass
перед `CarWheel`; Metal читает готовые UV offset и независимые tag angles и
больше не владеет animation clocks. Copy/move восстанавливает parent/listener
graph, а `FreeCar` удаляет include actors. Regression проверяет exact types,
1+2 layout, порядок/результат progress, dead-zone, copy rebinding и cleanup.

### P2.98 — concrete `FxSystemWaitingEnd`/`LifeEffect` graph — выполнено

Алгоритмы двух effect behaviors уже были перенесены, но `RaceEffect` держал
их как пассивные поля: session вручную вызывал `OnDeath`/`OnProgress`, а
`GameObject` не владел listener lifecycle. Это сохраняло результат нескольких
сценариев, но не исходную модель объекта и легко расходилось при новых типах
эффектов.

Теперь каждый `RaceEffect` имеет стабильно размещённый source `GameObject` и
concrete behaviors типов 2/7. `GameObject::Death` сам рассылает первую смерть
`FxSystemWaitingEndBehavior`, behavior включает fading и выполняет `Resc`, а
его обычный progress посылает финальную смерть после нулевого particle count.
Serialized `LifeEffectBehavior` опрашивает доступность SDL Source3d и выдаёт
один Play transition. Effective emission/visible lifetime задаются через
`GameObject::maximumTimeLife`; contact effects по-прежнему получают внешний
release от source `PairPxContactEffect`. Backend передаёт только число живых
частиц и доступность звука. Regression проверяет type/listener identity,
автоматическую двухступенчатую смерть и одноразовый delayed Play; полный
physics/render/audio smoke подтверждает lifetime и teardown в гонке.

### P2.99 — concrete `FxSystemSrcSpeed` и единый particle input — выполнено

До этого renderer на каждом draw каждого emitter создавал временный
`FxSystemSrcSpeed`, повторно исполнял source `WorldToLocalNorm`, а затем тут же
преобразовывал результат обратно в world space. Помимо отсутствующего
behavior owner это умножало одинаковую работу на scene/reflection passes.

Добавлен concrete behavior serialized типа 3. При создании `RaceEffect`
наличие behavior берётся из parsed emitter provenance (`fire2` в поставляемом
каталоге), Jolt velocity задаётся перед общим `GameObject::OnProgress`, а
behavior сохраняет и локальное source значение, и готовую world-space границу
для bgfx. Attached effect получает скорость автомобиля каждый tick;
`ResurrectObj` фиксирует последнюю скорость при detach, а death effect получает
скорость уничтоженного Jolt body. Renderer теперь только читает этот результат
и не создаёт игровые state machines. Regression проверяет exact type/listener,
parent rotation/inverse-scale и сохранение последнего значения при отсутствии
physics actor; полный physics/Metal smoke прошёл.

### P2.100 — `Weapon : GameObject` и concrete `ShotEffect` — выполнено

Portable `Weapon` хранил правильные `_shotTime`, immutable `Desc` и счётчик
успешно подготовленных projectiles, но оставался отдельным value object.
`OnProjectilePrepared` напрямую увеличивал счётчик, поэтому исходные
`Weapon::GetBehaviors().OnShot(iter->pos)`, type 10 и listener ownership не
существовали.

Теперь каждый из шести slot-owned Weapon наследует source `GameObject` и
создаёт concrete `ShotEffectBehavior` типа 10. `OnProgress` сначала выполняет
общий object/include/behavior pass, затем увеличивает strict readiness timer.
После каждого успешного `PrepareProj` session передаёт serialized `Proj::pos`
через `Behaviors::OnShot`; behavior сохраняет позицию и собственный shot
count. Reset сохраняет description, но очищает timer/object/effect state, как
нужно resident slot object. Copy/move пересобирают локальные owner/listener
связи. Regression проверяет type 10, listener identity, multi-projectile
dispatch, точную позицию, copy rebinding и прежние charge/readiness правила;
offline/network/physics/Metal проверки прошли.

### P2.101 — `Proj : GameObject` и concrete `DeathEffect` — выполнено

Fast/attached projectile runtime хранил backend-neutral `DeathEffect` как
обычное поле и непосредственно вызывал `OnDeath(...)` перед созданием impact.
Хотя флаги `targetChild`/`effectPxIgnoreSenderCar` учитывались, отсутствовали
исходные `Proj` owner, serialized type 6, listener dispatch и базовый progress.

`source::Proj` теперь наследует `GameObject` (его уже перенесённые type rules
остались теми же static helpers) и создаёт concrete `DeathEffectBehavior` для
projectile records, где такой эффект действительно сериализован. Каждый live
`ProjectileRuntime` владеет стабильным Proj object, прогрессирует его один раз
за session tick и завершает через `GameObject::Death(target)`. Behavior получает
реальный target car GameObject, формирует единственный spawn-plan в listener
callback и сохраняет ignore-sender/target-child семантику; прежний прямой
session call удалён. Подготовка всех free/attached/hyper projectile путей
создаёт этот graph до регистрации runtime. Regression проверяет type/listener,
target identity, single-spawn и flags; nested death projectile, disconnect,
network, physics и Metal smoke прошли.

### P2.102 — source death graph для mine/AutoProj — выполнено

После переноса обычных projectiles ветви `Mine`, `ptCrater` и `MineRip`
по-прежнему обходили `Proj`: session напрямую читала `DeathEffectDefinition`
и создавала `RaceEffect`. Особенно опасным было копирование родительского
`MineRuntime` при распаде `MineRip`: один listener-state нельзя разделять
между автономными `model2`/`model3` объектами.

Каждая поставленная мина, mortar crater, отдельное ядро и каждый из пяти
осколков теперь владеют самостоятельным source `Proj`. Для вложенных объектов
behavior типа 6 конфигурируется из их собственного secondary/tertiary
`DeathEffect`, получает общий `OnProgress` и завершается только через
`GameObject::Death`. Контакт передаёт реальный target car GameObject, поэтому
`targetChild`, local transform и LifeEffect attachment больше не являются
session-заглушкой; скорость уничтожаемого объекта поступает в concrete
`FxSystemSrcSpeed`. Regression проверяет шесть различных owner-адресов,
наличие type-6 behavior на каждом и независимое появление secondary/tertiary
death effects. Полные offline/network/physics и 360-frame Metal smoke прошли.

### P2.103 — concrete `TouchDeath` и ground contact graph — выполнено

Death plane уже использовала правильную геометрию Z=0 и сохраняла touch kill
attribution, но `Map` держала отдельный helper `TouchDeath`, а session вызывала
его напрямую. В Windows `Map::_ground` владеет обычным `GameObject`, behavior
типа 0 добавлен в его `Behaviors`, и PhysX contact проходит через общий
`GameObject::OnContact` listener dispatch.

`TouchDeath` теперь является concrete `Behavior`; ground регистрирует его в
собственном object graph, а Jolt adapter передаёт разрешённый target
`GameObject` через общий contact callback. Session только определяет
пересечение физической плоскости и после listener dispatch выполняет backend
teardown уже уничтоженной машины. Прямой helper-call удалён. Regression
проверяет type-0 identity, ровно одного listener, null/repeated contact и
сохранение touch attacker при `DeathPlane`; полные offline/network/physics и
360-frame Metal smoke прошли.

### P2.104 — concrete `ResurrectObj` и source-наследование type 2 — выполнено

Отдельных shipped object records с behavior type 1 не найдено: в активных
ресурсах это базовый класс многочисленных type-2 `FxSystemWaitingEnd`.
Portable код, однако, держал `ResurrectObj` и `FxSystemWaitingEnd` как
автономные state helpers, а concrete type 2 содержал их композицией. Это
повторяло результат, но не Windows class/listener graph.

`ResurrectObj` теперь является concrete type-1 `Behavior`; первый death
выполняет `Resc`, находит `Logic::Map`, извлекает включённый `MapObj`, очищает
имя, сохраняет world pose и передаёт ownership в `Map::InsertMapObj`.
`FxSystemWaitingEndBehavior` наследует его, как Windows-класс, и добавляет
только fading и финальный `Death` после исчезновения частиц. Параллельные
state helpers и ручная передача `Map` удалены. Regression исполняет полный
listener-driven include detach/insert, проверяет type-1 самостоятельную
двухступенчатую смерть, world transform, ownership и type-2 particle end;
offline/network/physics и 360-frame Metal smoke прошли.

### P2.105 — `GameObject` владеет network frame-sync — выполнено

Математика Windows `SetPosSync/SetRotSync` и второго correction channel уже
была перенесена точно, но runtime-владелец оставался платформенным:
`main_bgfx_original_menu.cpp` держал отдельный `networkVehicleFrameSync`
vector, вручную resize/reset его при reload и сопоставлял индекс с `Player`.
В Windows эти поля принадлежат самому `GameObject`; внешний массив мог
расходиться с source lifetime при reset/disconnect.

Каждый `GameObject` теперь владеет своим `GameObjectFrameSync`, копирует его
как object runtime state и очищает в `ResetGameObject`. `OriginalRaceSession`
маршрутизирует `NetPlayer::ResponseStream` pose к конкретному `Player` и
выполняет source `OnFrame` correction; SDL/bgfx adapter только передаёт
network/Jolt pose и применяет вычисленный graph transform к render body и
колёсам. Внешний vector и три ручных reset/resize пути удалены. Regression
проверяет owner identity, оба correction channel и очистку на object reset;
network/offline/physics и 360-frame Metal smoke прошли.

### P2.106 — `OnWake/OnSleep/OnPxSync` body lifecycle — выполнено

После переноса correction owner порт всё ещё подавал в него только последний
Jolt pose. Windows `GameObject` получает `OnWake/OnSleep`, регистрирует body
late/frame progress, хранит previous/current PhysX pose и velocity, выполняет
`OnPxSync(alpha)`, и лишь затем применяет network correction. Порт не имел
physics activity на границе и поэтому не мог корректно воспроизвести sleep.

`VehicleState` теперь несёт реальный `JPH::Body::IsActive`; disabled body
явно считается sleeping. `GameObjectFrameSync::OnPhysicsState` реализует
wake/sleep transitions, previous/current pose и velocity, точные linear/slerp
ветви `OnPxSync`, freeze последней graph pose на sleep и продолжение после
wake. После каждого Jolt step все `Player` получают это состояние, а общий
offline/network render path сначала читает source frame pose и затем применяет
network channels. Regression проверяет half-step position/quaternion/velocity,
sleep freeze и wake; offline/network/physics и 360-frame Metal smoke прошли.

### P2.107 — `GameCar::OnPxSync` и `CarWheel::PxSyncWheel` — выполнено

В `eff9338:prog/Rock3dGame/source/game/GameCar.cpp` кадровая синхронизация
кузова принадлежит `GameCar::OnPxSync`, который затем вызывает
`CarWheel::PxSyncWheel` для каждого дочернего колеса. В порте вычисление
`graphFromPhysical`, перенос мировых позиций колёс и quaternion composition
находились прямо в `main_bgfx_original_menu.cpp`; кроме того, активный
`GameObjectFrameSync` ошибочно принадлежал `Player`, а не его `GameCar`.

Теперь `GameCar` владеет полным source PxSync-проходом, каждый `CarWheel`
хранит вычисленную graph pose, а `OriginalRaceSession` только преобразует
между source pose и backend-neutral `VehicleState`. Jolt по-прежнему
поставляет физические world transforms вместо PhysX `NxWheelShape`, но
иерархия и порядок source-вызовов восстановлены. Network/physics sync также
маршрутизируются в `Player::gameCar`; `GameCar::Reset` очищает старую
коррекцию при смерти/respawn. Ручная wheel-математика удалена из SDL/bgfx
adapter. Regression покрывает body-relative position/rotation колеса,
целый `GameCar::OnPxSync` и reset lifetime; offline/network/physics и
360-frame Metal smoke прошли.

### P2.108 — source `MotorProgress`/`TransmissionProgress` fixed-step — выполнено

Формулы Windows `CarMotorDesc`, `GameCar::MotorProgress` и
`GameCar::TransmissionProgress` присутствовали в Jolt adapter, но активным
владельцем gear/RPM/torque оставался physics backend. Поэтому перенос был
численно близким, однако `GameCar::OnFixedStep` как исходная точка вызова
отсутствовал, а `SoundMotor` получал запоздалый RPM один раз за внешний кадр.

`GameCar` теперь хранит исходные `MoveCarState`, motor description и текущую
передачу, выполняет neutral/brake/back/accel, brake-to-reverse,
reverse-to-forward, source torque/RPM, automatic shift и maximum-speed branch
в правильном Windows-порядке. Jolt вызывает source-контроллер на исходном
fixed-step 1/60 с живыми wheel-contact/axle-speed/body-speed данными, затем
применяет готовые motor/brake команды на двух backend substeps 1/120.
`SoundMotor::OnMotor` снова
диспетчеризуется из этого fixed-step. Backend-local копия оставлена только
как fallback автономного physics-smoke, где Rock3dGame намеренно не участвует.

Regression закрепляет первую/автоматическую/reverse/neutral передачи,
торможение перед сменой направления, source torque, ограничение скорости,
reset и fixed-step RPM audio. Полная arm64 Debug сборка, 15 offline-тестов,
2 network-теста, physics smoke и 360-frame bgfx/Metal smoke прошли.

### P2.109 — source `WheelsProgress`/`JumpProgress`/`StabilizeForce` commands — выполнено

После P2.108 Jolt всё ещё самостоятельно накапливал угол руля, выбирал
contact gate, вычислял rear-wheel pivot/yaw, решал добавлять ли вторую gravity
и airborne pitch, а также выбирал angular damping/clamp. Это оставляло три
крупных части Windows `GameCar::OnFixedStep` владельцами backend-а.

`GameCar` теперь загружает из оригинального `ctCar` dynamics description и
флаги/позиции каждого дочернего `CarWheel`, хранит steering angle, повторяет
`swOnLeft/swOnRight/smManual`, вычисляет source contact gate, yaw и заднюю
точку поворота. Он же выдаёт команды `JumpProgress` и `StabilizeForce`,
включая extra gravity, spring lock, `flyYTorque`, исходные `angDamping` (с
сохранением sentinel `-1`) и roll/pitch clamp. Каждый steering `CarWheel`
получает собственный угол. Jolt применяет эти готовые команды в своей системе
координат и больше не принимает игровых решений в активном runtime; старые
формулы остаются лишь fallback автономного engine smoke.

Regression проверяет digital steering ramp, manual/contact gates, wheel
owner, rear pivot, airborne/spring branches, clutch yaw suppression и damping.
Полная arm64 Debug сборка, offline/network/physics и 360-frame Metal smoke
прошли с теми же контрольными скоростями всех шести машин.

### P2.110 — fixed-step lifetime `clutch/mine/spring` — выполнено

Порт уменьшал `_clutchTime`, `_mineTime` и `_springTime` в
`GameCar::OnProgress`, то есть один раз за UI/render frame. В Windows первые
два таймера находятся в `GameCar::OnFixedStep`, а spring уменьшается внутри
`JumpProgress`; при плавающем FPS прежний путь менял реальную длительность
oil/mine/spring состояний и их physics gates.

Таймеры удалены из `OnProgress` и перенесены в source fixed-step в исходном
порядке: clutch/mine до `MotorProgress`, spring перед airborne pitch branch.
Native runtime помечает сессию как имеющую внешний 1/60 source callback;
session-only regressions без physics world выполняют один эквивалентный
source fixed-step сами. Это исключает и пропуск, и двойное уменьшение.
Regression доказывает, что frame progress не меняет locks, а fixed-step
освобождает их. Полные offline/network/physics тесты (включая MineRip
contact/DeathEffect) и 360-frame Metal smoke прошли.

### P2.111 — source `CarWheel` spin/steer graph transform — выполнено

После возврата `GameCar::OnPxSync` порт всё ещё копировал готовую мировую
ориентацию колеса из `JPH::VehicleConstraint::GetWheelWorldTransform`. В
Windows PhysX сообщает колесу suspension/contact position и axle speed, но
его graph rotation строит сам `CarWheel`: `_summAngle` накапливается в
`OnProgress`, затем `PxSyncWheel` вычисляет `steer(Z) * spin(Y)` и добавляет
сериализованный `invertWheel`. Поэтому Jolt оставался скрытым владельцем
исходной визуальной анимации и мог вносить другую систему осей/знаков.

Каждый source `CarWheel` теперь получает собственный Jolt axle speed,
накапливает исходный `_summAngle`, хранит steering/invert state и строит
точную локальную quaternion-композицию. Jolt pose используется только для
мирового центра колеса по подвеске; его rotation больше не попадает в bgfx.
`invertWheel` загружается напрямую из `ctCar`, а reset/copy сохраняют source
lifetime. Regression проверяет независимость от backend quaternion,
накопление spin, зеркальный поворот и очистку motion state.

### P2.112 — `MyContactModify` normal-force contract — выполнено

Jolt adapter уже вычислял исходный `tireSpring` cutoff и предел реакции
`1.5g`, но применял результат только как максимум tire-friction impulse.
Windows `CarWheel::MyContactModify` меняет сам `normalForce`: при clutch/oil
lock он равен нулю, при превышении `tireSpring` опора отпускается полностью,
иначе подвеска ограничена полутора статическими реакциями. Поэтому прежний
порт сохранял неограниченную опору подвески и мог давать неверные удары,
отскоки и устойчивость кузова.

К закреплённой версии Jolt применяется узкий воспроизводимый CMake patch:
после вычисления unconstrained suspension lambda constraint вызывает adapter
и ограничивает накопленные spring/hard-point lambda прямо внутри solver.
Это важно для корректного warm-start; послешаговая компенсация создавала бы
ложную обратную связь. Поскольку Jolt вызывает constraint callback несколько
раз за solver-step, `tireSpring` release защёлкивается на весь текущий шаг и
потребляется один раз за непрерывный contact episode; иначе одна PhysX-ветвь
ошибочно повторялась бы на каждой внутренней итерации и навсегда оставляла
машину на днище. Clutch release остаётся непрерывным. Ray contact намеренно
остаётся активным: так `CarWheel::OnProgress` по-прежнему видит contact/slip
для следов и звука, как в PhysX callback. В
`WheelContactState` экспортируются source `_nReac` и уже разрешённый normal
impulse, поэтому debug/parity проверки больше не вынуждены угадывать их по
движению кузова.

### P2.113 — `GameCar` wheel/body contact ownership — выполнено

Backend уже передавал `hasContact` и slip в source `CarWheel`, а кузовные
manifold использовались session-логикой для урона и эффектов. Однако сами
Windows-поля `GameCar::_anyWheelContact`, `_wheelsContact`, `_bodyContact` и
`CarWheel::_nReac` отсутствовали: разные потребители повторно выводили часть
состояния из Jolt-массивов, а raw normal reaction из P2.112 останавливалась на
physics boundary.

`GameCar` теперь снова владеет всеми тремя контактными флагами, обновляет их
в source fixed-step и повторно подтверждает по завершённому solver frame.
`CarWheel` хранит контакт, `_nReac`-эквивалент и разрешённый normal impulse;
copy/reset сохраняют исходный lifetime. `OriginalRaceSession` больше не
обрывает эти данные между Jolt и source-объектом. Regression проверяет any/all
wheel transitions, отдельный body contact и raw/clamped wheel telemetry.
Следующим отдельным блоком стала адаптация anisotropic body friction из
`GameCar::OnContactModify`, не маскируемая этими флагами.

### P2.114 — `GameCar::OnContactModify` anisotropic body friction — выполнено

Подтвердилось указанное обратным аудитом отличие: два коэффициента PhysX car
material (`dynamicFriction` и `dynamicFrictionV`) были сведены к одному
`Jolt::ContactSettings::mCombinedFriction`. При контакте с треугольником
трассы Windows `GameCar::OnContactModify` разворачивает friction basis по
нормали треугольника, обнуляет первую касательную и оставляет вторую с
коэффициентом трассы `0.1` либо бордюра `4.0`. Скалярный путь тормозил кузов
по неверной оси и мог заставлять его цепляться за пол или стену.

К закреплённому Jolt добавлен второй узкий воспроизводимый patch: contact
settings принимают world-space первую касательную и второй коэффициент, а
solver ограничивает пару импульсов эллиптическим Coulomb cone. При равных
коэффициентах он точно вырождается в прежний круговой Jolt constraint, так
что car-car и decoration contacts не изменены. Adapter повторяет исходный
`carUp x triangleNormal` basis и fallback на локальную поперечную ось,
назначает `0/0.1` для полотна и `0/4.0` для бордюра. Числовая regression
закрепляет обе ветви, а clean pinned-Jolt extraction принимает оба CMake
patch последовательно.

### P2.115 — source `Player::SetCheatK` / `GameCar` runtime coefficients — выполнено

После переноса fixed-step формул коэффициенты catch-up всё ещё принадлежали
`physics::VehicleInput`: session напрямую передавала `motorTorqueScale` и
`lateralGripScale` в Jolt. В Windows `Player::CheatUpdate` вызывает
`SetCheatK`, а тот меняет живые `GameCar::_motorTorqueK` и
`GameCar::_wheelSteerK`; только затем `MotorProgress` и
`ApplyWheelSteerK` формируют физическое состояние машины.

Portable `GameCar` снова владеет обоими коэффициентами, сохраняет их при
копировании и возвращает к единице при уничтожении/повторном создании
машины. Forward torque умножается на source `_motorTorqueK` (reverse, как и
в Windows, не умножается), а абсолютный `_wheelSteerK` передаётся Jolt вместе
с готовой `DriveCommand`. Активный Jolt callback больше не читает игровые
коэффициенты непосредственно из input; поля input оставлены диагностическим
отражением для debug/network regression и для автономного physics smoke без
game-layer controller. Умножитель `CarMotorDesc::cGameK = 1.15` не добавлялся
повторно: он уже корректно свёрнут в загружаемое `SEM * 1.15`.

Числовая regression проверяет исходный трёхкратный forward torque,
независимый wheel-grip, ограничитель скорости, reset и то, что AI catch-up
меняет именно живой `GameCar`, а обычные Computer не становятся ложными
opponent-reference.

### P2.116 — explicit source `SteerWheelState` — выполнено

Активный порт определял `swOnLeft/swOnRight/smManual` по величине
нормализованного steering: значение около `±1` считалось digital и проходило
через `_steerSpeed`, любое промежуточное — manual. В Windows режим задаётся
отдельно от угла. Особенно существенно, что `AICar::ControlState` всегда
вызывает `SetSteerWheel(smManual)`, даже при полном угле: прежняя эвристика
замедляла реакцию AI именно на резких поворотах.

В `GameCar` возвращены явные `SteerWheelState`, сохранение/copy/reset и
source-геттеры состояния/угла. `VehicleInput` переносит отдельный флаг manual:
AI назначает его безусловно, Human различает keyboard/gamepad-button
(`alphaMax == 0`, digital ramp) и gamepad-axis (`alphaMax != 0`, прямой угол),
а сетевой `ResponseStream` снова передаёт исходное `steerState == 3` вместе с
`steerWheelsAngle`. SDL input owner получил source-specific held lookup, чтобы
одновременно назначенные keyboard и gamepad bindings сохраняли приоритет
клавиатуры из `HumanPlayer::Control`.

Regression покрывает digital accumulation, мгновенный manual full-lock,
состояние живого `GameCar`, Human analog/digital классификацию, SDL source
lookup и сохранение `smManual` при AI reverse/forward blocking cycle. Debug
overlay теперь также показывает фактический manual/digital режим.

### P2.117 — source `GameCar` speed/RPM/wheel telemetry — выполнено

В Windows `GameCar` является единым gameplay-facing владельцем четырёх
значений: `GetSpeed`, `GetLeadWheelSpeed`, исторически неверно названного
`GetDrivenWheelSpeed` и `GetRPM`. Последний из wheel-speed методов намеренно
читает первое колесо вне `GetLeadGroup`, то есть первое свободное колесо;
именно его использует `CameraManager` для подавления обратного дрожания.
Порт обходил этот контракт: session отдельно кэшировала скорость ведущего
колеса, а camera/debug напрямую читали повторные вычисления из Jolt state.

Радиус теперь входит в живое состояние каждого source `CarWheel`, рядом с
уже перенесёнными axle speed, lead/steer и animation fields. Восстановленные
методы `GameCar` выбирают первое ведущее/свободное колесо в исходном порядке,
умножают axle speed на его радиус и применяют source dead zones 0.1 м/с и
1 м/с. `GetRPM` снова использует axle первого ведущего колеса, текущую
source-передачу, gear ratio, differential и ограничение max RPM.

Jolt синхронизирует в `GameCar` только завершённое физическое состояние.
Удалены отдельный `leadWheelSpeed` cache и его повторный цикл по vehicle
description. Camera получает source `GetDrivenWheelSpeed`, а game-debug —
source `GetSpeed/GetRPM/GetCurGear`; одноимённые поля `VehicleState` остаются
backend telemetry для автономных physics tests, но больше не подменяют
игровой объект в активном runtime. Regression проверяет оба порядка выбора
колеса, радиусы, знаки, dead zones, RPM-формулу, copy/reset и track animation.

### P2.118 — live source `GameCar` descriptor contract — выполнено

Обратное сравнение публичного Windows `GameCar` выявило следующий системный
обход исходного владельца. Serialized параметры машины попадали в Jolt spawn
и частично в собранную `DriveCommand`, но у portable `GameCar` отсутствовали
исходные `Get/SetMotorDesc`, `GearUp/GearDown`, setters move/steer/gear и весь
набор живых параметров: `kSteerControl`, steer speed/rotation, angular
damping, airborne pitch, roll/pitch clamps, gravity steering, clutch
immunity, max speed, tire spring и disable-color.

Контракт восстановлен в source object. `ConfigureMotor/ConfigureDynamics`
теперь являются только adapter-входами и заполняют те же живые поля, которые
менял Windows-код. Fixed-step пользуется source setters и gear methods,
ограничитель скорости читает `GetMaxSpeed`, а steering/stabilization уже
формируются из изменяемого descriptor state. `LockClutch` снова имеет
исходную сигнатуру с одним strength и сам проверяет `IsClutchImmunity` вместо
того, чтобы получать этот флаг от session.

Оба AI пути берут `kSteerControl` из своей живой `GameCar`, oil/Maslo contact
берёт там же clutch immunity, а game-debug показывает source max speed,
tire spring и steer runtime values. Vehicle definition/Jolt spawn остаются
backend-конфигурацией геометрии и solver, но больше не выступают параллельным
gameplay owner. Regression покрывает setters, границы передач/угла, motor
descriptor, copy/reset и immunity; профильные mobility-значения поступают в
`GameCar` при `Player::CreateCar` после применения исходного loadout.

### P2.119 — source `RockCar` ownership — выполнено

Оригинальный Windows `RockCar` не является пустым названием поверх
`GameCar`: он владеет вложенной коллекцией `Weapons` и в своём
`OnProgress` обновляет её сразу после автомобиля. В portable-коде этот класс
отсутствовал, `WeaponRack` находился напрямую в `Player`, а session отдельно
продвигала его таймеры в начале каждого кадра. Это оставляло два независимых
владельца lifecycle и делало возможным двойное обновление cooldown при
дальнейшем переносе object graph.

Добавлен backend-neutral `RockCar : GameCar`, владеющий полным набором из
четырёх primary, Hyper и Mine weapon objects. `Player::gameCar` теперь имеет
исходный тип, `BindWeaponItems` и совместимый `GetWeaponRack` обращаются к
`RockCar::GetWeapons`, а отдельное поле из `Player` удалено. Во время гонки
единый `RockCar::OnProgress` обновляет физико-игровое состояние и оружие один
раз после синхронизации колёс. Countdown и завершённое состояние также
продвигают тот же source owner, сохраняя исходную готовность оружия до
`GoRace`; session больше не владеет weapon timers.

Сериализация `RockCar::Weapons` и MapObj include-list остаются границей
следующего этапа object-graph migration: Jolt и bgfx по-прежнему получают
готовые команды, но gameplay-владение уже не дублируется в adapter.

### P2.120 — concrete `MapObj::ClassList` types — выполнено

Следующий обратный аудит подтвердил ещё одну заглушку object graph. Windows
`MapObj::InitClassList` регистрирует шесть конкретных классов — `GameObject`,
`GameCar`, `RockCar`, `AutoProj`, `Weapon`, `DestrObj`. Portable factory
создавала настоящими только `AutoProj` и `DestrObj`; сериализованные
`gotGameCar`, `gotRockCar` и `gotWeapon` молча превращались в общий
`GameObject`. Из-за этого их собственные wheel/weapon/child progress hooks
не могли выполняться в `MapObjects`.

Фабрика теперь создаёт все шесть исходных типов и предоставляет typed
accessors для car, rock-car и weapon. Поскольку переносимые методы имеют
разные result-типы и не могут образовать один виртуальный C++ override,
`MapObjects::ProgressSlot` выполняет эквивалентную source-dispatch явно:
`DestrObj`, `RockCar`, `GameCar`, `Weapon`, затем общий `GameObject`; для
`AutoProj` сохранена отдельная projectile-фаза после base progress.

Regression создаёт каждый ранее заглушенный тип через `MapObj`, проверяет
его фактический класс и подтверждает, что weapon cooldown и вложенное оружие
`RockCar` продвигаются именно контейнером. Это восстанавливает concrete
runtime object graph без переноса D3D9/PhysX actor ownership из адаптеров.

### P2.121 — single live `Player::CarState` / `MapObj` RockCar — выполнено

В Windows `Player::CreateCar` добавляет car record в `Map`, после чего
`CarState::mapObj` и `CarState::gameObj` удерживают ссылки на один созданный
`MapObj/RockCar`. Порт нарушал это устройство: `Player` содержал один
`RockCar` для Jolt/gameplay, а `createRacerMapObject` создавал второй для
Map/Logic, жизни и projectile target identity. После восстановления concrete
factory оба объекта даже корректно обновлялись, но оставались параллельными.

`MapObj` теперь умеет привязать стабильный live `Player::gameCar`, передавая
ему record proxy state, Logic, имя и MapObj identity и освобождая временный
record instance. Car category, projectile target, player slots, camera/debug
telemetry и Jolt adapter видят один и тот же `RockCar`. При удалении карты
внешний объект полностью отсоединяется, но не удаляется; reset сначала
очищает MapObj-ссылки и только затем перестраивает `Player` storage, поэтому
висячих указателей нет.

Прямой session-вызов `gameCar.OnProgress` удалён. Завершённое Jolt-состояние
speed/contact/axle-speed синхронизируется до `Logic::OnProgress`, а исходный
Car category pass один раз обновляет `RockCar`, колёса, children и оружие во
всех фазах, включая countdown. Рост smoke-счётчика `FxTrail` с 12 до 24 не
является удвоением update: это максимум по четырём колёсам всех шести машин
после того, как AI wheels также получают contact state до source slip pass.
Regression отдельно проверяет bind identity и безопасное отсоединение.
На этом этапе сам pointer/lifecycle всё ещё хранился в session; это
оставшееся расхождение окончательно устранено в P2.268.

### P2.122 — source `CarWheel` descriptor and visual offset — выполнено

Windows `CarWheel` хранит не только текущий поворот и скорость оси: каждый
wheel object владеет признаками `lead`/`steer`, `invertWheel`, исходными
longitudinal/lateral slip и локальным `_offset`. Его `PxSyncWheel` применяет
этот offset к графическому actor после синхронизации подвески. В порте offset
оставался в `OriginalRaceRenderer`, поэтому gameplay object и рендер имели
разные wheel poses, а garage и race реализовывали одно правило отдельно.

Полный backend-neutral descriptor возвращён в `CarWheel`; `GameCar`
предоставляет живые lead/steer-группы, которые отражают source setters без
параллельного кэша. `Player::CreateCar` переносит offset из оригинального car
record, а `PxSyncWheel` применяет его в системе координат graph body. Race
renderer теперь рисует уже готовую source pose; garage, где нет racing
object graph, один раз формирует эквивалентную готовую pose на своей adapter
границе. Copy, slip accessors, динамические группы и повёрнутый visual offset
закреплены regression-проверками.

### P2.123 — car-owned life and `Player` listener graph — выполнено

Обратное сравнение подтвердило архитектурную ошибку portable-класса:
Windows `Player` наследуется от `GameObjListener`, а не от `GameObject`.
Жизнь, immortality, damage/death, low-life и car behaviors принадлежат
`CarState::gameObj`, то есть конкретному `RockCar`. В порте те же состояния
жили во втором `GameObject` внутри `Player`, параллельно уже существовавшей
машине.

`Player` теперь снова является listener исходного `RockCar`. Все damage и
authoritative network life переходы направлены в одну car-owned модель;
medpack, shield, droid repair, slow/energy/low-life behaviors, HUD/debug и
respawn читают тот же объект. `RockCar` передаёт source damage/kill dispatch
своему player owner, сохраняя порядок `Damage`, `Kill`, special death,
`Death`. Session подготавливает входы behaviors до единственного
`Logic::OnProgress` и после него только забирает результаты, поэтому второй
скрытый progress не появился.

При переносе выявлены две связанные lifecycle-ошибки. `ReleaseSoundMotor`
раньше очищал всю коллекцию car behaviors вместо одного `SoundMotor`; теперь
он удаляет только свой элемент. Кроме того, listener мог удалить следующий
behavior во время destroy/death dispatch, а snapshot продолжал вызывать уже
освобождённый адрес. Dispatch теперь проверяет, что snapshot-entry всё ещё
зарегистрирован. Regression закрепляет отсутствие наследования `Player` от
`GameObject`, единую life identity, исходный event order и безопасное
удаление listener во время callback.

### P2.124 — dynamic `RockCar::Weapons` MapObj ownership — выполнено

Оригинальный Windows `RockCar` не содержит фиксированный массив из шести
`Weapon`. Его вложенный `RockCar::Weapons` наследует `MapObjects`, а каждый
установленный `WeaponItem::OnCreateCar` добавляет туда конкретный `MapObj`
из оружейной записи. `OnDestroyCar` освобождает эту ссылку, удаление машины
уничтожает collection, а `RockCar::OnProgress` обновляет только реально
установленные объекты. Прежний portable `WeaponRack` создавал шесть оружий
всегда, в том числе для пустых слотов, и не имел ни MapObj identity, ни
исходного parent transform.

Возвращён динамический `RockCar::Weapons : MapObjects`. Он создаёт
`gotWeapon`, назначает car parent, сохраняет record/transform слота,
прогрессирует оружие через общий source container и повторяет специальные
кэши `GetHyperDrive`/`GetMines` по типу первого projectile. Вставка и
удаление теперь проходят через virtual source hooks `MapObjects`, включая
безопасное удаление конкретного `MapObj`.

`Player::CreateCar`, `FreeCar`, `BindSlots`, live `SetSlot` и повторное
применение weapon definitions теперь создают и освобождают те же объекты,
которые видят `WeaponItem`, session shots и shot effects. Фиксированный
`WeaponRack` удалён; session больше не конфигурирует параллельные таймеры.
Regression проверяет parent/owner identity, динамическое число объектов,
progress через `RockCar`, Hyper/Mine cache invalidation и удаление weapon
MapObj вместе с car lifecycle.

### P2.125 — live `Proj` descriptor and source listener links — выполнено

Portable `ProjectileRuntime` уже содержал concrete `Proj` для death-effect
behavior, но сам объект оставался почти пустым: descriptor, weapon owner,
target, player attribution, transform и linked lifetime продолжали жить
только в session-структуре. Поэтому уничтожение weapon/target обрабатывалось
повторно вычисленной проверкой индексов вместо исходного listener graph.

`Proj` теперь принимает полный `ProjectileDefinition` при подготовке,
сохраняет source player id и maximum lifetime, подписывается на конкретные
`Weapon` и target `RockCar`, а linked ray/contact projectiles становятся
детьми оружия. Уничтожение target очищает только homing-ссылку; уничтожение
weapon снимает listener и убивает только projectile, реально связанный с
ним через parent — мины и свободные снаряды остаются в мире. Это соответствует
`Proj::SetWeapon`, `LinkToWeapon`, `SetShot` и `OnDestroy` оригинала.

Fast, attached и Hyper spawn paths теперь передают live weapon/target в этот
объект; Mine передаёт weapon без parent link, а impulse chain обновляет
source target при выборе следующего противника. Runtime position/rotation
синхронизируются в concrete `Proj` до source progress, тогда как Jolt collision
и bgfx visuals остаются backend-данными. Regression проверяет descriptor,
world transform, listeners, target release и различие linked/unlinked death.
На терминальном кадре `Race` сначала устанавливает оригинальный contact context
`DeathEffect` и только затем переводит `Proj` в death-state; это сохраняет
вложенные осколки `MineRip`, которые ранее терялись при раннем auto-expire.

### P2.126 — live `Proj` scratch state ownership — выполнено

Следующее прямое сравнение с `Weapon.cpp` показало, что несколько полей,
которые выглядели как разные session-timers, в Windows являются одним живым
состоянием конкретного `Proj`: `_time1` используется для mine arming,
Torpeda/Impulse homing delay и Thunder reflection cooldown; `_vec1` хранит
Torpeda velocity либо минимальный Rocket clearance; `_tick1` считает
переходы Impulse, а `_state1` и `_ignoreContactProj` также принадлежат объекту.

Эти поля восстановлены в concrete `Proj` и сбрасываются при каждом
`PrepareSource`. `ProjectileRuntime` и `MineRuntime` больше не содержат
параллельные `armingTime`, `homingDelay`, `reflectionCooldown`,
`trackClearance` и `hitCount`. Mine/contact, homing, Rocket TrackPlane,
Thunder reflection и цепной Impulse теперь читают и записывают один source
object; Jolt runtime сохраняет только готовые position/velocity результаты.
Типовая подготовка также повторяет исходные значения: mine начинает с нуля,
MinePiece — с `-1`, Torpeda/Impulse — с `0.4`, а Rocket/Laser отмечают
`ignoreContactProj`. Regression проверяет сброс всех scratch-полей, а
интеграционный заезд — clearance, homing hand-off, Impulse chain и MineRip.

### P2.127 — projectile `InitModel/InitModel2` object graph — выполнено

В Windows `Proj::InitModel` и `InitModel2` добавляют записи `model/model2`
непосредственно в `Proj::GetIncludeList`, удерживают конкретные `MapObj` и
слушают их уничтожение. Portable renderer использовал те же mesh/texture
ресурсы, но соответствующих source-объектов вообще не создавал: projectile
оставался без сериализованных детей, а laser/frost secondary model и
контактный model Drobilka существовали лишь как вычисленные draw/effect rows.

`Proj` теперь создаёт primary/secondary `MapObj` из исходных record refs,
применяет их source life/time-life, сохраняет parent ownership и очищает
указатели через listener callback либо `FreeSourceModel`. Все успешно
подготавливаемые типы повторяют вызовы `InitModel`; Spring остаётся без
модели, Drobilka создаёт её только при реальном контакте, переносит в точку
контакта, сбрасывает исходный `_time1 = 0.5` и освобождает после таймера.
Laser и FrostRay получают оба source child objects. bgfx по-прежнему рисует
ресурсные определения на backend-границе, но lifecycle и include graph больше
не являются renderer-заглушкой. Regression проверяет parent/listener graph,
удаление model/model2 и lazy Drobilka path.

### P2.128 — `Logic::RegGameObj` owns live projectiles — выполнено

После переноса descriptor, scratch-state и child models оставался последний
двойной владелец projectile graph. Windows `Weapon::CreateShot` выделяет
`Proj`, вызывает `PrepareProj` и при успехе сразу передаёт объект в
`Logic::RegGameObj`; `Logic::OnProgress` обновляет и удаляет его. Portable
session вместо этого держала каждый projectile/mine в `shared_ptr` и отдельно
вызывала `Proj::OnProgress`, поэтому уже перенесённый transient registry не
участвовал в реальной гонке.

Все успешные projectile/mine spawn paths теперь выделяют concrete `Proj` и
передают его единственному `Logic` owner. Runtime-массивы хранят только
оригинальный non-owning указатель, проверяемый через `HasGameObj`; ручной
progress удалён. `Logic::CleanGameObjs` выполняется до разрушения car/weapon
MapObjs при reset и exit, сохраняя listener teardown order оригинала.

Поскольку Jolt collision adapter пока принимает терминальное решение после
source `Logic` pass, `GameObject::OnProgress` получил узкую возможность
отложить только автоматический lifetime death для таких `Proj`: time-life,
include-list и behaviors прогрессируют в `Logic` ровно один раз, а session в
тот же кадр устанавливает contact context `DeathEffect` и вызывает `Death`.
Обычные зарегистрированные `GameObject` по-прежнему автоматически умирают на
строгой границе `timeLife > maxTimeLife`. Regression проверяет оба режима,
единоличное владение, удаление после death и безопасный lookup старого адреса.

### P2.129 — `Weapon::CreateShot` и `Proj::ShotDesc` — выполнено

После восстановления владения `Logic` сама транзакция выстрела всё ещё была
разорвана: session-helper выделял и регистрировал `Proj`, `WeaponItem` отдельно
сбрасывал shot timer, а `pushShotEffect` вручную вызывал source behavior.
В оригинальном `Weapon.cpp` всё это делает один `Weapon::CreateShot` после
успешного `PrepareProj`, причём `Behaviors::OnShot` вызывается отдельно для
каждого projectile descriptor.

Portable `Proj` теперь снова хранит полный `ShotDesc`: listener-ссылку на
конкретный target `GameObject` и независимую world target position для
варианта `Weapon::Shot(vec3)`. Backend-neutral `ShotContext` переносит только
значения, которые раньше получались от PhysX actor preparation: transform,
maximum lifetime и link flag. Новый `Weapon::CreateShot` создаёт concrete
`Proj`, применяет descriptor/context, передаёт единственное владение в
`Logic`, сбрасывает source shot timer и отправляет точную локальную позицию
descriptor в `Behaviors::OnShot`.

Primary, attached, Hyper и установленная Mine теперь проходят через эту
транзакцию; автономные crater/MineRip children остаются прямым source spawn,
поскольку в Windows они создаются из `MapObj`, а не оружием. Мгновенные ray
типы по-прежнему разрешаются Jolt-адаптером синхронно, но получают тот же один
успешный source callback. Regression проверяет target object/vector, Logic
ownership, transform/player attribution, timer reset, локальную shot-effect
позицию и отказ фабрики без `Logic`.

### P2.130 — восстановлено наследование `AutoProj : Proj` — выполнено

Обратное сравнение `Weapon.h` выявило ещё одну раннюю структурную заглушку:
оригинальный `AutoProj` является прямым наследником `Proj` и при появлении
`Logic` вызывает базовый `PrepareProj(NULL, ctx)`. Portable-класс был отдельным
`GameObject` с четырьмя дублирующими полями, поэтому map bonuses, oil и
автономные mines не получали descriptor, source model/include graph, scratch
state и общую projectile identity.

`AutoProj` снова наследуется от concrete `Proj`. Его `Reset` принимает полный
`ProjectileDefinition`, а `LogicInited` выполняет общую source preparation с
сериализованным world transform. Поскольку объект принадлежит map category, а
не transient registry `Logic`, после подготовки восстанавливаются proxy
max-life/life/time-life и обычная lifetime policy. Oil arming теперь использует
базовый `_time1` (`sourceTimer`) и меняет scale реального дочернего source
model, а не только отдельный float для renderer adapter.

Создание bonus map objects передаёт все уже распарсенные исходные поля:
visual/death descriptor, collision size/offset, speed, damage и modelSize.
Regression проверяет полиморфизм `AutoProj -> Proj`, полный descriptor,
include/model parent graph, сохранение transform/lifetime, общий scratch timer
и синхронное увеличение model scale.

### P2.131 — concrete `Proj::ComputeAABB` и отказ `PrepareProj` — выполнено

Формула исходного `ComputeAABB(false)` уже применялась загрузчиком ресурсов,
но результат сохранялся только как готовый contact box. Живой `Proj` не имел
ни функции `ComputeAABB`, ни отдельного local AABB модели, поэтому ветвь
`ComputeAABB(true)` для размещения mines оставалась заранее вычисленным float,
а не частью concrete source object.

`ProjectileDefinition` и map `BonusInstance` теперь сохраняют точный model
AABB отдельно от contact collision. Обе исходные ветви перенесены в
`Proj::ComputeAABB`: serialized `size/offset` объединяется с моделью только
при `modelSize`, model-only расчёт начинается с origin, а отсутствие модели
даёт оригинальный fallback cube `0.1`. Loader строит weapon/bonus collision и
surface placement через эту функцию; `AutoProj` получает те же model bounds.

Также `ShotContext` переносит результат backend-подготовки. Если mine raycast,
Spring wheel-contact либо создание Jolt actor не удалось, `Weapon::CreateShot`
теперь атомарно возвращает отказ до allocation/registration: shot timer,
charge-side callback и `Behaviors::OnShot` не изменяются. Regression проверяет
объединение смещённого serialized box с model AABB, model-only/fallback ветви
и отсутствие побочных эффектов неуспешного PrepareProj.

### P2.132 — `LocateProj` / `LinkToWeapon` transform ownership — выполнено

В исходном `Weapon.cpp` свободный снаряд получает world transform через
`LocateProj`, а Hyper, Laser, Spring и FrostRay после этого становятся детьми
оружия и снова получают сериализованные локальные `_desc.pos/_desc.rot`.
Portable preparation раньше сразу назначала parent, а затем записывала world
transform; последующая покадровая синхронизация каждый раз меняла local pose.
Из-за этого concrete source graph не соответствовал mount graph Windows и
дополнительно отставал от runtime-позиции на один кадр.

`Proj` теперь разделяет свободную world-позу и linked local-позу. Для linked
типов Jolt world transform используется, чтобы восстановить world transform
живого `Weapon`, после чего projectile неизменно хранит исходные local
position/rotation и следует за parent. Session синхронизирует concrete объект
после вычисления текущего attached/free transform, а не до него. Regression
проверяет local pose, точный результирующий world pose, повторное движение
mount и независимое размещение unlinked projectile.

### P2.133 — единый `Proj::PrepareProj` context и type state — выполнено

Подготовка concrete projectile оставалась распределена между `Proj` и двумя
helper-функциями session. Поэтому результат зависел от spawn path: Torpeda и
Impulse получали `_time1/_vec1` только при обычном выстреле, mine arming
sentinel назначался снаружи, а FrostRay и Drobilka вообще не получали исходный
`_ignoreContactProj`.

`PrepareSource` теперь, как Windows `PrepareProj`, принимает единый
`ShotContext`, сам назначает shot/target/player/transform и выполняет
type-specific initialization. Rocket/Laser/FrostRay/Drobilka владеют своим
collision-ignore state; Torpeda/Impulse сохраняют 0.4-секундную задержку и
подготовленную Jolt launch velocity; Mine/MineRip/Crater начинают arming с 0,
MinePiece — с `-1`, а Maslo обнуляет scale живой source model. Session больше
не исправляет эти поля после создания, поэтому weapon, autonomous и map spawn
проходят одну concrete source-транзакцию.

### P2.134 — concrete `Proj` progress/contact state dispatch — выполнено

Статические транскрипции projectile-формул уже совпадали с Windows, но
session вручную передавала в них `_time1/_vec1/_tick1`, затем отдельно писала
результат обратно. Это оставляло фактическое владение source-state у Jolt
adapter и позволяло различным call site пропустить часть перехода.

Concrete `Proj` теперь сам выполняет stateful Rocket, Torpeda/Impulse homing,
Mine arming, Thunder reflection и Impulse chain-contact dispatch. Методы
атомарно читают и изменяют исходные scratch-поля, обновляют oil source-model
scale и сбрасывают homing delay при смене цели. Race-session поставляет только
результаты ray/contact queries и применяет возвращённые velocity/transform к
Jolt runtime; прямые `Get/SetSource*` пары из этих игровых переходов удалены.

### P2.135 — Fire/Drobilka/Laser live visual transforms — выполнено

Исходные `FireUpdate` и `DrobilkaUpdate` используют `_desc.pos`, но после
первого кадра назначают projectile именно world rotation оружия. Session
вместо этого каждый кадр повторно умножала serialized projectile rotation,
изменяя направление контактного объёма и огня. Кроме того, impact model Laser
и временная contact model Drobilka управлялись внешним helper-кодом.

Attached runtime теперь отдельно вычисляет weapon transform: position
получается через serialized offset, а Fire/Drobilka rotation берётся напрямую
из оружия, как в Windows. Живой source `Weapon` синхронизируется с mount.
Concrete `Proj` перемещает Laser model2 в hit/local endpoint, создаёт и
удаляет Drobilka contact model по исходному 0.5-секундному таймеру и вращает
source weapon через `_desc.angleSpeed`; session оставляет за собой только
bgfx/Jolt presentation state.

### P2.136 — полная `Weapon::Shot` batch transaction — выполнено

Portable `Weapon::CreateShot` принимал один `ProjectileDefinition`, тогда как
Windows-фабрика принимает полный `Weapon::Desc`, проходит весь `projList`,
сохраняет каждый успешный `Proj` в optional `ProjList` и возвращает успех,
если подготовился хотя бы один descriptor. Также отсутствовали overload для
пустого shot, world target и target object и исходный фильтр типов, которым
запрещено автономное создание без weapon.

Восстановлены все `Weapon::Shot` overload, построение source contexts из
живого weapon transform и batch-фабрика с частичным успехом, per-projectile
timer/effect commit и точным no-weapon type filter из `Weapon.cpp`. Jolt
session продолжает вызывать тот же leaf commit после индивидуальной backend
подготовки, а source/API callers снова получают оригинальную полную
транзакцию и список созданных concrete objects.

### P2.137 — concrete `Proj::FindNextTaget` для цепного Impulse — выполнено

После `ImpulseContact` сессия самостоятельно выбирала следующую машину двумя
вызовами `Player::FindClosestEnemy`. Это воспроизводило формулу, но оставляло
исходный переход `Proj::FindNextTaget` разобранным на adapter-код и допускало
расхождение между concrete target-listener и индексом runtime-снаряда.

В `Proj` восстановлен цельный выбор следующей цели: поиск начинается от
игрока, соответствующего текущему `ShotDesc` target; первый результат,
совпавший со стрелком, пропускается повторным поиском уже от стрелка; цикл
обратно к только что поражённой машине прекращает цепочку. Session теперь
только разрешает `MapObj`/индекс в `Player*` и обратно, после чего прежний
`RetargetImpulse` атомарно меняет listener и сбрасывает homing delay.
Существующий lethal-contact regression проверяет важный исходный случай:
убитая в callback цель всё ещё служит центром поиска, а следующая машина
выбирается по Windows plane-distance правилу.

### P2.138 — восстановлен `Proj::Type` и центральный contact dispatch — выполнено

Типы снарядов в portable-ветке оставались без исходного enum и повторялись
числами в session. Из-за этого уже возникло реальное расхождение: при
подготовке минного состояния значение `20` (`ptCrater`) было пропущено, а
`24` ошибочно подписано как Crater вместо `ptMineProton`. Кроме того, выбор
`RocketContact`/`MineContact`/`ImpulseContact` и `DamageType` выполнялся
несколькими независимыми switch/if цепочками за пределами `Proj`.

Восстановлен полный сериализованный порядок `Proj::Type` 0..24 и единый
`ContactRouteFor`, повторяющий исходный live-state guard и `OnContact` switch.
Маршрут возвращает конкретный handler, Simple/Energy/Mine attribution,
RocketContact response, mine-lock policy и факт прямого урона. Attached,
свободные и минные Jolt-contact пути теперь запрашивают этот маршрут у живого
concrete `Proj`; локальный session switch `sourceProjectileDamageType` удалён.
Crater и MineProton оба получают исходный mine-arming timer, но сохраняют
разные contact semantics: Crater наносит непрерывный `dtMine`-урон, а
MineProton использует обычный незаблокированный `MineContact`. Smoke-test
проверяет enum routes, death guards, damage attribution и оба arming state.

### P2.139 — восстановлен центральный `Proj::OnProgress` dispatch — выполнено

После переноса отдельных формул выбор покадрового поведения всё ещё находился
в `OriginalRaceSession`: числовые проверки отдельно решали, когда запускать
Torpeda/Impulse homing, Laser/Frost ray, Fire/Drobilka mount update,
Rocket/Thunder/Resonanse track-height и Mine/MineRip/MineProton arming. Это
дублировало исходный `Proj::OnProgress` switch и особенно легко смешивало
типы, которые используют общий prepare/contact, но не общий progress — так,
`Crater` намеренно отсутствует в исходном update-switch.

Concrete `Proj` теперь возвращает единый `ProgressRoute` с точным handler и
признаками attached, ray, homing, rocket-height и mine-arming. Оба runtime
контейнера — летящие/attached projectiles и stationary mines — используют
этот маршрут для вызова уже перенесённых stateful методов. Из игровых циклов
удалены все числовые проверки типа в progress-части; числовые значения
остались лишь в definition/import и regression assertions. Тест фиксирует
полный важный набор, включая отсутствие progress у Sonar и Crater и различие
Torpeda homing от Rocket height correction.

Новый source-dispatch также проявил скрытую ошибку фабрики `MineRip`: runtime
ядра/осколка получал дочерний `type`, но concrete `Proj` создавался из
родительского descriptor и потому снова исполнял `MineRipUpdate`. Теперь из
импортированного `model2/model3` record строится собственный concrete
descriptor с его type, visual, collision, lifetime, damage и DeathEffect —
точно как отдельный `MapObj` в Windows. Это прекращает рекурсивное дробление и
возвращает исходный lifecycle дочерних мин.

### P2.140 — восстановлен полный `Proj::PrepareProj` dispatch — выполнено

Оставшийся `GetTypeRules` описывал лишь несколько общих признаков и не был
полной картой исходного `PrepareProj`: отдельно подразумевались создание
model/model2, точный prepare-handler, mine placement/owner lock, разрешение
автономного создания и actor-pair ignore. Поэтому session и фабрика могли
принять разные решения для одного serialized type.

Добавлен единый `PreparationRouteFor` для всех 25 значений `Proj::Type`.
Маршрут фиксирует конкретный prepare-handler, model/model2 initialization,
linked/attached/ray, Rocket/Torpeda/Mortira flags, mine placement и две разные
mine-lock политики, collision ignore и исходный no-weapon filter.
`PrepareSource`, `Weapon::MakeShotContexts`, обе `CreateShot` фабрики,
освобождение linked-снарядов и session spawn теперь используют эту карту;
старый `GetTypeRules` оставлен только как совместимый derived view без второго
switch. Unknown type больше не регистрируется как успешно подготовленный
concrete объект. Regression проходит весь диапазон 0..24 и отдельно проверяет
Laser, Mine, Drobilka, Spring, Crater и MineProton, где различия наиболее
существенны.

### P2.141 — восстановлены `Proj::DamageTarget` и source identity — выполнено

Даже после переноса contact/progress/prepare dispatch живые снаряды передавали
урон через session-поля `damageOwner` и заново собирали тип урона возле каждого
Jolt-contact. В Windows эту команду формирует сам `Proj::DamageTarget`: он
берёт машину-отправителя из parent живого `Weapon`, сохраняет `_playerId`,
цель, величину и `DamageType`, после чего передаёт их `Logic::Damage`.
Одновременно generic `GameObject` предоставляет виртуальный `IsProj`, которым
исходные contact, bonus и listener пути определяют конкретный тип объекта.

В concrete `Proj` восстановлена единая backend-neutral `DamageCommand` и
виртуальная identity `IsProj`. Attached Laser/Frost, Fire/Drobilka, свободные
контактные снаряды и все живые mine/crater объекты теперь формируют атрибуцию
через `Proj::DamageTarget`; session применяет готовую команду и оставляет у
себя только сетевую авторитетность, отражатель, события и Jolt/renderer
presentation. Regression отдельно проверяет generic dispatch, sender-car,
player id, цель, значение и тип урона, а также отказ команды без Logic/цели.

### P2.142 — восстановлены `GameCar::IsCar` и точный kill dispatch — выполнено

Portable `GameObject::Damage` выставлял `killCredit` для любого смертельно
повреждённого `GameObject`, если был известен sender и тип не был Mine. Это
расходилось с прямым условием Windows: событие `cPlayerKill` создаётся только
когда виртуальный `IsCar()` подтверждает, что целью является `GameCar`, и
никогда для `dtMine`. Сам polymorphic `GameCar::IsCar` в порте отсутствовал,
поэтому исходное условие ранее нельзя было выразить без проверки контейнера.

В `GameObject` и `GameCar` восстановлен virtual identity dispatch, а lethal
damage теперь выдаёт kill-credit только машине. Обычные игровые объекты и
разрушаемые декорации сохраняют Damage/Death lifecycle, но больше не могут
создать ложное убийство игрока; `RockCar` по наследованию снова получает
точный исходный `OnKillDispatchEvent`. Regression проверяет generic/object/car
identity, обычное убийство машины, отсутствие kill-credit для Mine и для
обычного `GameObject` как в локальном, так и authoritative damage overload.

### P2.143 — car-target ownership для Mine/Maslo/Spring — выполнено

Session всё ещё заранее превращал Jolt vehicle index в набор булевых
`hasCar`, `mineLocked`, `clutchLocked`, `clutchImmune` и `wheelsContact`, а
затем вызывал статические формулы снаряда. В Windows эти решения принадлежат
конкретному `Proj`: Mine принимает только `target->IsCar`, сравнивает его с
parent оружия и собственным arming timer; Maslo умеет подняться от wheel-child
к parent `GameCar`; Spring берёт машину через parent живого `Weapon`, проверяет
все колёса и сам устанавливает spring lock.

В `Proj` восстановлены concrete `ContactMine`, `ContactMaslo`, разрешение
wheel/car contact target и weapon-based `SpringPrepare`. Session теперь
передаёт generic `GameObject` и физические векторы, а source object сам читает
lock state и меняет `GameCar`. Map `AutoProj` масла идёт по тому же пути.
Одновременно physics-smoke fixture приведена к реальному Jolt contract:
помимо сводного `contactCount` она задаёт per-wheel `wheelContacts`, которыми
в действительности владеет `GameCar::IsWheelsContact`; airborne fixture явно
снимает каждый контакт. Regression проверяет direct-car Mine, non-car reject,
mine-bug lock, wheel-parent Maslo и Spring impulse/lock.

### P2.144 — concrete `FrostRayUpdate` и SlowEffect ownership — выполнено

FrostRay использовал общий `ProgressLaser`, после чего session отдельно
проверял enum типа, вычислял lifetime model3 и напрямую вызывал
`Player::AttachSlowEffect`. В Windows отдельный `Proj::FrostRayUpdate` сначала
выполняет недеформированный LaserUpdate, затем проверяет generic target через
`IsCar`, ищет уже существующий `btSlowEffect` и только один раз добавляет
эффект из `_desc.model3`.

В concrete `Proj` восстановлены отдельный `ProgressFrostRay` и
`AttachFrostSlow`: тип цели, соответствие target живому `Player::gameCar`,
duplicate-behavior gate, fallback lifetime и weapon/projectile attribution
теперь принадлежат снаряду. Session оставляет у себя raycast и применение
Jolt velocity limit, но больше не воспроизводит source-условия FrostRay.
Regression проверяет hit/damage ray, 2.5-секундный model3 lifetime, identity
оружия/снаряда, запрет повторного эффекта и reject обычного GameObject.

### P2.145 — concrete bonus-contact ownership — выполнено

Session всё ещё передавал в бонус независимый `BonusKind` и сводил исходный
контакт к булевому `hasTarget`. Это допускало расхождение между реально
созданным `Proj`, сетевым пакетом и типом награды, тогда как Windows
`Proj::OnContact` получает тип непосредственно из `_desc`, находит игрока
через `GameObject -> MapObj` и только затем вызывает `Logic::TakeBonus`.

В concrete `Proj` восстановлен `ContactBonus`: он проверяет связь generic
цели с `MapObj`, `Player` и именно `Player::gameCar`, выбирает Money/Charge/
Medpack/Immortal из собственного source-description и для нулевой величины
аптечки использует максимальную жизнь цели. Session сохраняет сетевую
авторитетность, RNG и мутацию `Player`, но больше не дублирует source dispatch.
Regression проверяет medpack fallback, тип, правильного владельца и reject
чужого игрока/обычного объекта. Дополнительно исправлена неполная shield
physics-fixture: ей задан исходный `ptImmortal`, без которого новый concrete
dispatch корректно отказывался создавать эффект.

### P2.146 — concrete `SpeedArrowContact` и `LushaContact` — выполнено

Обе трассовые модификации скорости оставались статическими формулами:
session сама вычисляла направление стрелки из placement-transform, передавала
отдельный `bonus.value` и без проверки concrete `Proj` создавала событие.
В Windows `SpeedArrowContact` читает `GetGrActor().GetWorldDir()` и
`_desc.damage` самого снаряда, а `LushaContact` ограничивает скорость тем же
source-параметром только после успешного разрешения контактного объекта.

В `Proj` восстановлены concrete contact-методы и type route gate.
`SpeedArrow` теперь получает направление из мирового quaternion живого
`AutoProj`, формирует точную actor-velocity и подтверждает
`cPlayerSpeedArrow`; `Lusha` читает текущую Jolt-скорость цели, но предел берёт
из собственного description. Session лишь применяет возвращённую velocity
delta и переводит подтверждённое source-событие в renderer/audio queue.
Regression проверяет поворот стрелки на 90 градусов, source damage, порог
лужи, null-target и отбрасывание неверного contact route.

### P2.147 — concrete continuous contacts Fire/Drobilka/Sonar — выполнено

Session продолжала вызывать статические `FireContact`, `DrobilkaContact` и
`SonarContact`, передавая копии damage/mass. Для Drobilka результат считался
дважды: статическая формула давала урон, а concrete вызов отдельно сбрасывал
`_time1`, создавал `_model` и перемещал его в точку контакта. Контакты с
декорациями вообще получали готовое число урона до разрешения source-объекта.

В `Proj` восстановлены concrete `ContactFire`, `ContactDrobilka` и
`ContactSonar`: они проверяют реальный contact route/target, читают
`_desc.damage` и `_desc.mass`, а Drobilka одним вызовом также владеет timer и
контактной моделью. Поиск пересечения с разрушаемой декорацией отделён от
применения урона, поэтому и машина, и `DestrObj` сначала передаются живому
снаряду, после чего Jolt/session исполняет возвращённые damage/impulse
команды. Все статические вызовы этой тройки из race session удалены.
Regression теперь проверяет source descriptor каждой разновидности,
Drobilka model/timer, Sonar impulse и null-target rejection.

### P2.148 — concrete `SpringPrepare` и source lifetime — выполнено

Spring оставался особым исключением: session вызывала статический
`SpringPrepare(weapon, speed)` до транзакции `Player::Shot`, применяла Jolt
velocity request, но настоящий `Proj`, который Windows создаёт, связывает с
оружием и регистрирует в `Logic`, вообще не существовал. Из-за этого source
object graph, `PrepareProj` failure и per-projectile shot behavior формально
обходились.

В concrete `Proj` восстановлен `PrepareSpring`, использующий собственные
`_weapon` и `_desc.speed` после обычного `PrepareSource`. Hyper-slot adapter
теперь создаёт source-снаряд с исходным world/local transform и lifetime,
выполняет wheel-contact gate и `GameCar::LockSpring` внутри него, регистрирует
его в `Logic` только после успешной charge transaction и отправляет
`OnProjectilePrepared`. Jolt получает только готовую local velocity command и
синхронизирует backend spring-lock. Не имеющий отдельного runtime-представления
source-снаряд переведён на собственный `GameObject` lifetime, поэтому он не
остаётся навсегда в Logic. Regression проверяет concrete descriptor speed,
weapon/car ownership, wheel gate, lock и отказ объекта другого типа.

### P2.149 — concrete `MineRipUpdate` time/arming ownership — выполнено

Разрывная мина вызывала `ProgressMine` на source object, но момент деления
session определяла второй статической формулой по своему `mine.seconds` и
копии `projectile.angularSpeed`. В Windows один `Proj::MineRipUpdate` сначала
выполняет `MineUpdate`, затем сравнивает собственный `GetTimeLife()` с
`_desc.angleSpeed` и проверяет live state. Поэтому split мог расходиться с
source lifetime и arming state.

В `Proj` восстановлен concrete `ProgressMineRip`: он продвигает собственный
external lifetime, обновляет `_time1`/arming и формирует split-команду из
своего descriptor и live state. Session использует единый результат для
визуальной фазы и создания model2/model3 дочерних мин, сохраняя у себя только
Jolt-траектории и регистрацию дочерних объектов. Статический `MineRipUpdate`
из race session удалён. Regression проверяет строгую границу `>` после 2
секунд, armed timer, source `GetTimeLife` и отказ projectile другого типа.

### P2.150 — concrete launch speed и maximum lifetime ownership — выполнено

Обычный weapon spawn заранее вызывал статические `CalcSpeed` и
`PrepareMaximumLife` по копиям полей `ProjectileDefinition`, а concrete
`Proj` получал уже готовые `launchVelocity`/`maximumLife`. Это инвертировало
Windows-порядок: там `PrepareProj` сначала владеет `_desc`, затем
`RocketPrepare::CalcSpeed` читает speed-relative правила, сохраняет скорость
Torpeda/Impulse в `_vec1` и устанавливает `_maxTimeLife`.

В `Proj` восстановлены concrete `PrepareLaunch` и однопараметрический
`PrepareMaximumLife`. Primary, Hyper и Spring spawn теперь сначала создают
source object, после чего он читает собственные speed/maxDist/min-time поля,
выравнивает пологий launch-вектор, учитывает скорость машины, обновляет
homing `_vec1` и source lifetime. Session/Jolt применяют получившиеся
direction/speed/velocity и только хранят backend lifetime mirror. Прямые
вызовы обеих статических формул из race session удалены. Regression проверяет
relative-speed, горизонтализацию, Torpeda `_vec1`, max-distance lifetime и
отказ non-RocketPrepare типа.

### P2.151 — concrete Resonanse rotation и Rocket contact torque — выполнено

Обновление Resonanse и угловая реакция Rocket-семейства оставались двумя
разрозненными статическими вычислениями: session передавала в них копии
`angularSpeed` и `mass` из runtime definition. В Windows обе операции являются
методами конкретного `Proj` и читают собственный `_desc`; `RocketContact`
дополнительно подчиняется live-state/type dispatch объекта.

В concrete `Proj` восстановлены `ProgressResonanse` и `ContactRocket`.
Resonanse теперь сам выбирает свой progress handler, читает собственную
угловую скорость и сохраняет новый source world rotation. Rocket contact сам
проверяет concrete route/target state и вычисляет local velocity-change из
собственной массы. Race session только передаёт backend rotation, мировую
PhysX/Jolt contact point и linear velocity, затем применяет возвращённую
команду. Прямые статические вызовы из session удалены. Regression проверяет
source transform, descriptor ownership, null-target rejection и общий
RocketContact путь Resonanse.

### P2.152 — concrete `Proj::OnDestroy` listener lifecycle — выполнено

При уничтожении машины session повторно вычисляла судьбу всех снарядов через
статическую таблицу `Proj::OnDestroy(senderIsWeapon, parentIsWeapon,
senderIsTarget)`. Это обходило уже существующий concrete listener graph:
`Player::FreeCar` уничтожает Weapon MapObj и car MapObj, а зарегистрированный
`Proj::OnDestroy` синхронно освобождает model/model2, weapon и target,
уничтожая только реального ребёнка linked weapon.

Статическая lifecycle-заглушка и её отдельный synthetic test удалены.
`releaseRacerProjectileReferences` теперь только зеркалирует результат
concrete callbacks в Jolt runtime: очищает target после source target,
снимает damage ownership после source weapon, удаляет умерший linked
projectile, отсоединяет живой Fire/Drobilka и сохраняет world mine/Maslo.
Порядок подтверждён существующими concrete weapon/target/model listener
regressions, disconnect lifecycle, network tests и Metal race smoke.

### P2.153 — concrete mine/contact route и ray damage type — выполнено

Session всё ещё выбирала `testMineLock` статически по скопированному type как
для runtime mines, так и для map `AutoProj`, а damage type живых Laser/Frost
объектов повторно вычисляла из runtime definition. Это могло разойтись с
реальным `_desc` после profile/weapon snapshot и обходило различие исходных
`MasloContact`, `MineContact(true)` и `MineContact(false)`.

`Proj::ContactMine` теперь сам получает concrete contact route и различает
Maslo, Mine/MineRip, MinePiece и MineProton. Maslo сохраняет безусловную
проверку `IsMineLocked`; Mine/MineRip применяют её только вместе с исходным
`EnableMineBug`; arming и owner-car правила по-прежнему читаются из source
state. Runtime mines и обе фазы map hazard/network contact вызывают concrete
`AutoProj`, а live ray damage получает `DamageType` из concrete route.
Прямых `ContactRouteFor` вызовов в race session больше нет. Regression
проверяет различие Maslo и Mine при mine-lock и выключенном mine bug.

### P2.154 — единая concrete primary-shot transaction — выполнено

Primary fire сначала списывал charge по одному лишь признаку непустого списка
descriptor, затем session выбирала `PreparationRouteFor(type)` и только внутри
отдельных attached/free ветвей создавала настоящий `Proj`. Это инвертировало
Windows-порядок `Weapon::CreateShot -> PrepareProj -> WeaponItem::Shot` и
оставляло отдельную ray-ветвь со вторым ручным `OnProjectilePrepared`; для
Laser/Frost внутренний ShotEffect получал два callback на один снаряд.

Теперь каждый primary descriptor сначала проходит единый concrete
`Weapon::CreateShot` и регистрацию в `Logic`. Только первый успешно
подготовленный `Proj` коммитит charge, а каждый backend runtime читает
attached/ray/rocket/homing/ballistic route из собственного `_desc` через
`RoutePreparation`. Общая подготовка position, rotation, lifetime, target,
death behavior и source ownership выполняется до Jolt-разветвления. Удалены
последние статические `PreparationRouteFor`/`DamageTypeFor` обращения primary
session и повторный ray callback; гипотетический immediate ray также получает
source damage attribution и корректно умирает через `Logic`.

### P2.155 — concrete Mine и Hyper charge transaction — выполнено

Mine и Hyper оставались двумя отдельными исключениями из восстановленного
порядка shot transaction. Mine сначала списывал charge/регистрировал bonus id
и блокировал машину, а concrete `Proj` создавал после этого. Обычный Hyper
также списывал charge до создания linked source projectile и вычисления его
lifetime. Отказные ветви дополнительно вызывали synthetic
`Player::Shot(false)`, хотя Windows просто возвращается после неуспешного
`PrepareProj`.

Mine теперь сначала выполняет track placement и concrete
`Weapon::CreateShot`, после чего единожды коммитит charge/bonus id и только
затем применяет `LockMine`. Hyper создаёт linked source projectile и получает
maximum lifetime до charge commit; при неожиданном отказе транзакции source
object переводится в Death. Spring сохраняет требуемую двухфазную схему:
concrete `PrepareSource`/wheel gate/`PrepareSpring`, затем charge commit,
регистрация и один `OnProjectilePrepared`. Все synthetic failed-shot вызовы
из Mine/Hyper/Spring удалены. Существующие regressions подтвердили placement,
network mine ids, attached Hyper, Spring impulse/lock и отсутствие расхода
заряда у airborne Spring.

### P2.156 — concrete live projectile descriptor ownership — выполнено

Даже после concrete preparation live projectile loop заново находил
`ProjectileDefinition` через session-owned `weaponDescription` snapshot или
глобальный `race.weapons` fallback. Из него повторно читались transform,
collision, ray offset/distance, damage, speed, relative-speed, angular speed,
death visuals и death-projectile index. При любом различии snapshot и
скопированного Windows `_desc` backend исполнял бы уже не состояние реального
`Proj`.

Live update/contact и `spawnProjectileImpact` теперь получают descriptor
исключительно из `projectile.sourceObject->GetDesc()`. На concrete ownership
переведены attached transforms, Fire/Drobilka boxes, Laser/Frost rays,
Torpeda progression, Rocket clearance, Resonanse/Drobilka rotation, обычный и
decoration damage, death effect и crater spawn selection. Перегрузка
`runtimeProjectileDefinition(ProjectileRuntime)` удалена; snapshot оставался
стабильным backend bookkeeping и источником для автономных `MineRuntime`
fragment records на границе этого блока (оба остатка удалены в P2.157).

### P2.157 — concrete MineRip fragment и renderer descriptor ownership — выполнено

`MineRuntime` всё ещё возвращался к сохранённому `Weapon::DescHandle` либо к
глобальному каталогу оружия для arming/split/contact/death, а renderer делал
такой же lookup и для mines, и для обычных projectiles. Особенно опасным был
MineRip: каждый model2/model3 fragment уже являлся отдельным `Proj` со своим
скопированным `_desc`, но death effect и visual повторно выбирались из
родительского MineRip по `visualVariant`.

Mine, MineRip core и все шесть автономных fragments теперь используют только
`sourceObject->GetDesc()`. На concrete descriptor переведены damage, crater
damage rate, impulse speed, collision/посадка на трассу, split definitions и
death effect. Renderer также читает concrete descriptor; `visualVariant`
оставлен лишь индексом заранее загруженного GPU asset, тогда как само visual
description принадлежит дочернему `Proj`. Удалены обе session snapshot-ссылки
и renderer fallback lookup для projectile/mine. Regression дополнительно
проверяет type/damage/speed в собственном descriptor каждого MineRip child и
сохранение descriptor обычного projectile после замены `WeaponItem::WpnDesc`.

### P2.158 — единый `GameObject::_timeLife` и сохранение lifetime при DeathEffect — выполнено

`Logic::OnProgress` уже вызывал базовый `GameObject::OnProgress` для каждого
transient `Proj` и увеличивал `_timeLife`, но `ProgressMineRip` затем прибавлял
тот же `deltaTime` повторно. MineRip поэтому раскалывался примерно вдвое раньше
исходного `GetTimeLife() > angleSpeed`. Одновременно `ConfigureDeathEffect`
вызывал `ResetGameObject(-1)` после `PrepareSource` и стирал рассчитанный
`_maxTimeLife` у любого projectile/mine с death effect. Session-owned
`ageSeconds/maximumLife` скрывали эту потерю, оставляя source object
бессрочным и создавая риск накопления живых объектов и падения FPS.

Теперь единственный clock продвигается базовым `GameObject::OnProgress`, а
session лишь зеркалирует concrete `_timeLife/_maxTimeLife` для renderer и
backend. MineRip читает уже продвинутый clock; strict `>` expiration всех
projectile/mine проверяется по concrete object. Добавление `DeathEffect`
больше не сбрасывает live-state и lifetime. Заодно stateful Torpeda,
Laser/FrostRay, Impulse, Maslo и bonus contact перестали принимать копии
descriptor-полей и читают speed/relative-speed/angle/damage/lifetime прямо из
собственного `_desc`. Regression запрещает split до точного двухсекундного
порога, проверяет 4–4.5-секундную жизнь MineRip children и сохранение lifetime
после подключения DeathEffect.

### P2.159 — удаление session-owned копий projectile/mine descriptor и clock — выполнено

После P2.158 единственный рабочий lifetime уже принадлежал concrete
`GameObject`, однако `ProjectileRuntime` и `MineRuntime` продолжали хранить
зеркала `age/life/maxLife`, а также копии `damage`, `type`, `collision`,
`angularSpeed`, `maximumDistance` и impulse speed. Renderer читал эти зеркала,
а создание crater и MineRip children сначала заполняло их вручную. Такой
двойной контракт позволял backend и исходному `Proj` снова разойтись после
замены live `WeaponItem::WpnDesc` или изменения source lifecycle.

Runtime теперь хранит только действительно backend-owned состояние: transform,
velocity/distance, attachment, beam scale, network identity и renderer asset
variant. Начальный sampled lifetime передаётся прямо в `Proj::ShotContext`, а
дальнейшие lifetime/descriptor значения читаются только через
`sourceObject->GetTimeLife()`, `GetMaxTimeLife()` и `GetDesc()`. На этот путь
переведены renderer animation time и beam length fallback, crater, MineProton,
Drobilka, Thunder/Frost lifetime и MineRip regressions. Повторная проверка
подтвердила exact-threshold lifetime, автономные дочерние `Proj`, полный Metal
race render и отсутствие session-owned descriptor/clock полей.

### P2.160 — source-owned primary `Player/WeaponItem/Weapon::CreateShot` batch — выполнено

Несмотря на concrete `Proj`, primary fire всё ещё создавал каждый объект из
race session, после чего передавал в `Player::Shot` только булево значение
успеха. Это сохраняло обратный ownership относительно Windows
`Player::Shot -> WeaponItem::Shot -> Weapon::CreateShot`: charge commit,
создание полного descriptor batch и возвращаемый `ProjList` жили в разных
частях portable кода. Кроме того, session-helper вручную добавлял
`DeathEffect`, поэтому новый source batch первоначально выявил отсутствие
death behavior у созданного им projectile.

`WeaponItem::Shot` теперь принимает массив backend-neutral `ShotContext`, сам
вызывает единый `Weapon::CreateShot` batch, списывает один заряд при наличии
хотя бы одного подготовленного `Proj` и возвращает исходный `ProjList`.
`Player::Shot` снова является верхним владельцем этой транзакции. Race session
только строит Jolt transform/query contexts и материализует backend runtime из
уже созданных concrete objects. Один homing target выбирается на весь batch,
как в `HumanPlayer::Shot`. `Proj::PrepareSource` теперь самостоятельно
создаёт descriptor-owned `DeathEffect`, поэтому lifecycle больше не зависит
от session spawn helper. Regression проверяет двухснарядный batch, одно
списание charge, два `ShotEffect` callback и live race damage/death graph.

### P2.161 — source-owned Mine/Hyper/Spring shot transaction — выполнено

После primary batch ещё три live пути использовали переходную схему:
session самостоятельно создавал Mine или Hyper `Proj`, а затем вызывал
`Player::Shot(item, true, ...)`; Spring дополнительно создавался через
`unique_ptr`, проверял колёса вне `Proj` и регистрировался вручную после
списания charge. Булевый overload повторно сбрасывал weapon timer и не мог
вернуть оригинальный `ProjList`/mine identity.

Mine и Hyper/Spring теперь строят только Jolt `ShotContext` и входят в тот же
`Player -> WeaponItem -> Weapon::CreateShot` путь, что primary weapons. Mine
получает bonus-projectile id из первого concrete `Proj`, Hyper runtime
привязывается к объекту, возвращённому source batch. `SpringPrepare` перенесён
в `Proj::PrepareSource`: он проверяет родительский `GameCar`, полный wheel
contact, применяет `LockSpring` и сохраняет velocity command до регистрации.
`Weapon::CreateShot` теперь отвергает реально неподготовленный `Proj`, поэтому
airborne Spring не регистрируется, не расходует заряд и не создаёт
`ShotEffect`. Session factory остался только для автономных crater/MineRip
map objects, у которых в Windows нет `WeaponItem` владельца. Все live race
shot paths больше не используют булевую transaction-заглушку.

### P2.162 — удаление булевого shot transaction bypass — выполнено

После перевода всех live fire путей на concrete batch в `WeaponItem` и
`Player` ещё сохранялись старые overload-ы `Shot(bool projectileCreated)`.
Они позволяли списать charge, сбросить timer и зарегистрировать mine id без
создания единого source `Proj`; gameplay их уже не вызывал, но unit-сценарии
продолжали закреплять этот переходный контракт и оставляли простой путь для
его случайного возврата.

Булевые overload-ы полностью удалены из публичного API и реализации.
Проверки installed/uninstalled, отказа backend preparation, бесконечного
боезапаса и replicated charge теперь проходят через настоящий `ShotContext`,
`Logic`, `Weapon::CreateShot` и возвращаемый `ProjList`. Player regression
также создаёт реальные mine objects, подтверждает добавление bonus id только
после успешной подготовки и затем завершает их через исходный lifecycle.
Таким образом, любой новый вызов `Player/WeaponItem::Shot` обязан создать
concrete projectile transaction; одного внешнего boolean больше недостаточно.

### P2.163 — единый source owner для projectile damage — выполнено

`Proj::DamageTarget` уже владел исходным `_playerId` и очищал его в
`SetWeapon(0)` после уничтожения оружия, но оба session runtime record всё ещё
хранили параллельный `damageOwner`. Его вручную копировали при выстреле,
обнуляли в `releaseRacerProjectileReferences` и даже переносили в создаваемый
death-effect crater, хотя автономный crater в Windows не имеет родительского
Weapon и получает undefined player id. В результате attribution, owner
collision filter и урон декорациям могли читать разные владельцы одного
снаряда.

Поля `ProjectileRuntime::damageOwner` и `MineRuntime::damageOwner` удалены.
Фильтрация собственного автомобиля, Impulse chain owner, урон ray/Fire/
Drobilka/Sonar по декорациям и sender-policy DeathEffect теперь каждый раз
читают `Proj::GetSourcePlayerId()` либо живую ссылку `GetSourceWeapon()`.
Уничтожение Weapon автоматически меняет все эти решения через исходный
listener graph; crater и MineRip children остаются автономными и больше не
наследуют session-only боевого владельца. Backend-поле `owner` сохранено
только для размещения, visual/audio routing и network identity, которым
реально нужен индекс racer вне source `Proj`.

### P2.164 — concrete `ProjList` как единственный результат shot batch — выполнено

После `Weapon::CreateShot` primary session повторно обходила входной
`itemProjectiles`, самостоятельно пропускала invalid prepare routes и по
отдельному `BackendShotContext` сопоставляла очередной `Proj` с transform,
lifetime и renderer asset. При частичном отказе подготовки список созданных
объектов сжимается, поэтому такая индексная догадка могла привязать следующий
живой `Proj` к descriptor/asset предыдущего отклонённого элемента. Это также
оставляло единственный production-вызов статического
`PreparationRouteFor(type)` вне concrete object.

Каждый загруженный `ProjectileDefinition` теперь сохраняет исходную позицию
в `Weapon::Desc::projList`; identity переживает фильтрацию death-projectile
records и копирование в `WeaponItem`. После shot transaction session итерирует
только фактически возвращённый `ProjList`: descriptor, world transform,
rotation, sampled lifetime и preparation route читаются из самого `Proj`, а
стабильный list index выбирает уже загруженный bgfx asset. Параллельные
`BackendShotContext`, повторный input loop и session-вызов
`PreparationRouteFor` удалены. Parser regression проверяет identity всех
загруженных projectiles, а source batch test — её сохранение у каждого
созданного concrete object.

### P2.165 — autonomous crater/MineRip снова являются `AutoProj` — выполнено

DeathEffect crater и оба вида MineRip fragments создавались session-helper-ом
через `new Proj`, прямой `PrepareSource` и `Logic::RegGameObj`. В Windows эти
records добавляются как `gotProj` MapObj, а зарегистрированный concrete class
для них — `AutoProj : Proj`: подготовка запускается из `LogicInited`, повторная
инициализация идемпотентна, а `LogicReleased` снимает auto-state. Обычный
`Proj` обходил этот lifecycle и делал автономные объекты ещё одним особым
runtime видом.

Фабрика теперь создаёт `AutoProj`, передаёт ему полный дочерний descriptor,
world transform и source lifetime, затем инициирует подготовку только через
`SetLogic/LogicInited` и передаёт объект в transient ownership `Logic` лишь
после успешного concrete `Proj::PrepareSource`. Для detached Jolt records
сохраняется внешний lifetime boundary: у них нет MapObj-list observer,
который в Windows создаёт DeathEffect во время удаления, поэтому session
успевает материализовать model2/model3/death visuals до release. Crater и все
шесть MineRip children regressions теперь дополнительно требуют dynamic
`AutoProj` identity и сохраняют прежние arming, split, lifetime и death-effect
пороги.

### P2.166 — `CameraManager` возвращён как source owner — выполнено

Race-ветви оригинального `CameraManager::Control::OnInputFrame` были
переписаны прямо внутри `OriginalRaceRenderer::makeCamera`. Renderer владел
состоянием ThirdPerson/Isometric, фильтрацией обратной скорости по driven
wheel, quaternion interpolation, orthographic lead, компенсацией teleport/
respawn, debug-перемещением и переключением стилей. Хотя формулы уже были
source-derived, это оставляло игровую camera policy частью bgfx backend и не
давало сопоставлять `CameraManager.cpp` как самостоятельный исходный класс.

Добавлен backend-neutral `source::CameraManager` с собственными
`CameraTarget`, `CameraFrame`, style/projection enums и состоянием всех пяти
race/debug camera modes. Активный Metal renderer теперь передаёт только
vehicle snapshot/aspect/profile distance и переводит готовый source frame в
bgfx view/projection matrices. Из renderer удалена inline state machine и её
параллельные поля; debug move/rotate/reset также направлены к source owner.
Отдельный smoke закрепляет stopped/reverse velocity filtering, точные
ThirdPerson offset/FOV, Isometric width/near/far, respawn compensation и
debug retained pose. `FlyTo`, `AutoObserver`, screen-to-ray и editor light
ветви остаются явно открытой частью CameraManager, а не считаются
перенесёнными данным блоком.

### P2.167 — `ControlManager` возвращён как source owner — выполнено

Предыдущая сверка уже восстановила точные `cVirtualKeyInfo`,
`cGameActionStr` и profile bindings, но исполняемым владельцем оставался
`SdlInputManager`: он строил параллельные keyboard/button/axis maps, вручную
вычислял dead zones и отдельно хранил held action state. Поэтому исходный
`GetGameAction`, `GetGameActionState(..., withAlpha)` и ordered
`ControlEvent` graph не существовали как класс.

Добавлен backend-neutral `originalcontrol::ControlManager`. Он владеет всеми
25 source actions в исходном enum-порядке, canonical bindings, raw
VirtualKey state, точной signed normalization для trigger/thumb ranges,
двухконтроллерным polling, ordered input/progress/frame dispatch и очисткой
при focus loss/hot-unplug. SDL теперь оставляет у себя только device handles,
scancode/button/axis translation и rumble; значения осей передаются в
XInput-совместимых диапазонах, включая различие activation/normalization
threshold у правого стика. Исторические keyboard A/B/X collisions также
сохранены, а не заменены удобными алиасами.

Новый deterministic smoke проверяет shared-key action order, прекращение
цепочки обработчиком, repeat, digital `withAlpha`, signed steering,
trigger threshold, progress/frame ordering и device/focus reset. Существующий
SDL virtual-gamepad regression продолжает проверять живой adapter path.
Mouse screen/ray events и регистрация исходных menu widgets относятся к
последующим View/Menu ownership blocks и здесь не объявлены готовыми.

### P2.168 — ядро `World`/`GameMode` возвращено исходным владельцам — выполнено

Главный SDL/bgfx цикл вручную владел двухкадровой задержкой запуска гонки,
паузой и частью start/exit/finish переходов, а fixed/progress/late/frame
списки оригинального `World` вообще отсутствовали как исполняемый объект.
Countdown и finish clocks при этом были временно размещены в общем
`OriginalRaceLifecycle`, хотя в Windows ими владеет `GameMode`.

Добавлены backend-neutral `source::WorldEventPump` и
`source::GameModeState`. Первый воспроизводит точный порядок исходного
`World::Progress`, `FixedStep`, `LateProgress` и `FrameStep`, включая
отложенное снятие progress users, pause gates, reset control при паузе и
порядок environment/network/control/GameMode. Второй владеет admission
match/race, исходным loading-frame gate, ordered GameMode users, pause effect
commands, exit/save sequence и finish-close music transition. Race clocks
перемещены к GameMode owner без параллельной реализации в lifecycle.

Активный Metal path теперь запускает гонку только после команды
`DoStartRace`, полученной через `WorldEventPump`, а loading frame отмечает
сам `GameModeState`. Отдельный regression закрепляет порядок событий,
удаление во время progress, поведение паузы, два представленных loading
кадра, порядок pause/exit/finish команд и GameMode user dispatch. Полный
startup/movie/config/audio-command executor и регистрация всех race-local
объектов в world lists остаются следующими B3b/B4 границами.

### P2.169 — `Logic` progress и базовый `Proj::OnContact` возвращены в source graph — выполнено

`Logic::OnProgress` вызывался непосредственно из `OriginalRaceSession`, а
`PairPxContactEffect::OnProgress` — вручную значительно позже внутри
`updateGameplay`. Это не воспроизводило `LogicBehavior::RegProgressEvent`:
в Windows PhysX сначала отправляет contact callbacks, затем `World` вызывает
`Logic`, затем ordered progress users. Кроме того, projectile/mine/bonus
ветви порта переходили прямо к конкретным `Contact*`, пропуская начальный
`GameObject::OnContact` из `Proj::OnContact`.

`Logic` теперь является host-ом собственного race `WorldEventPump`, а
`PairPxContactEffect` зарегистрирован настоящим `ProgressEvent`. Jolt
manifolds импортируются до source progress; release-команды исполняются
после него. Это устранило повторное использование уже освобождённого contact
effect и вернуло исходный двухточечный/0.1-секундный lifetime order.

Новый `Proj::BeginContact` сначала рассылает базовый listener callback и лишь
затем выбирает concrete route с учётом изменившегося live-state. Активные
car/decor projectile contacts, mines, hazards и bonuses используют этот
путь. Regression дополнительно ставит `TouchDeath` на projectile и проверяет,
что target погибает до switch, поэтому rocket handler подавляется. Полное
сворачивание Jolt movement/raycast views из session остаётся B4b.

### P2.170 — `MapObjRec` снова загружает source до proxy — выполнено

Хотя portable `MapObjRecordLibrary` уже восстанавливал стабильную identity и
иерархию `RecordNode`, запись не содержала сериализованную source-часть.
`MapObj::SetRecordProxy` менял concrete type и path, а
`OriginalRaceSession::reset` затем вручную собирал `DestrObj`, fragments,
base life и bonus `AutoProj::Desc`. Это нарушало главную транзакцию Windows
`MapObj::SetRecord -> MapObjRec::Load -> MapObj::LoadSource`, после которой
map placement загружает только proxy transform/lifetime/name.

`MapObjRecord` теперь владеет source-loader, `DefineRecord` связывает его со
стабильной записью, а `SetRecordProxy` синхронно применяет source до
placement state. Session заранее регистрирует definitions трассы,
декораций, бонусов и машин, после чего активные `Map::AddMapObj` получают
concrete gameplay object только через каталог. Ручная сборка тех же полей
удалена. Map regression проверяет, что source life/lifetime загружены
автоматически и record identity не меняется. Graph и audio cache identity
остаются отдельными B5b/B5c.

### P2.171 — graph-ресурсы снова имеют общую source identity — выполнено

Активные race, garage и angar renderers, а затем workshop и HUD независимо
читали одни и те же `.r3d`/image-файлы и каждый создавал собственные bgfx
buffers/textures. Это расходилось с Windows `ComplexMeshLib` и
`ComplexImageLib`: `GetOrCreateMesh/GetOrCreateIVBMesh` и
`GetOrCreateTex2d/GetOrCreateCubeTex` возвращали ресурс из единой коллекции,
а `ReleaseAll` завершал его общий lifetime. Повторная инициализация frame или
renderer поэтому могла раздувать Metal resources и оставляла риск разных
identity/lifetime для одного исходного имени.

Добавлен `OriginalResourceManager`: канонический физический путь является
ключом, decoded `R3DMeshAsset`, GPU mesh и texture создаются один раз, а
пользователи держат ссылки/handles без локального destroy. Все 3D race/menu
renderers, workshop и HUD переведены на этот owner; локальными остались только
реально производные meshes (HUD-scaled weapon и procedural planes/debug).
Различие исходных `Tex2D`/`TexCube` сохранено: cube DDS передаётся в bgfx как
контейнер, а не отвергается 2D image decoder. Общий shutdown выполняется после
отключения всех потребителей.

Полный 360-frame Metal regression загрузил 116 уникальных meshes и 219
textures и подтвердил 847 cache hits на 1182 запроса, включая workshop,
garage/angar resize round-trip и race reload. Офлайн 18/18, network 2/2 и
physics smoke также прошли. Audio/font/material descriptor collections были
сохранены как следующие части B5, а не объявлены выполненными этим блоком.

### P2.172 — `SoundLib::Find` снова определяет identity OGG — выполнено

В Windows каждый `ResourceManager::LoadSound` сначала вызывает
`_soundLib->Find(pathRoot + name)` и только при отсутствии создаёт `Sound`.
Порт нарушал это владение тремя локальными каталогами: Menu SoundSheme,
commentator и большой `engineSounds` в entry point отдельно декодировали и
выгружали OGG. Даже когда один локальный map убирал повторы внутри системы,
identity и lifetime не распространялись между системами, а повторная
инициализация могла остановить voice через `unloadSound` чужого владельца.

`OriginalResourceManager` теперь содержит SoundLib с canonical physical-path
ключом, исходными name/volume, decoded `SoundInfo` и одним backend handle.
Menu, commentator и все engine/wheel/contact/weapon/effect loaders получают
заимствованный handle; их shutdown останавливает только voices и очищает
ссылки. Единственный `ShutdownSounds` выгружает каталог до завершения SDL
audio backend. Старый `engineSounds` и все локальные `unloadSound` удалены.

В полном Metal smoke 105 запросов дали 76 уникальных OGG и 29 cache hits.
Source Menu SoundSheme сохранил 9 cues, commentator — 38 доступных voice
files, motor/wheel loop teardown прошёл, а после выхода не осталось звуков
шин в меню. Офлайн 18/18, network 2/2 и physics smoke также прошли.
Потоковый `MusicCat` не помещён в PCM SoundLib: его background current/next
decode и eviction являются необходимой portable backend-границей. Следующим
B5d остаются font и material descriptor libraries.

### P2.173 — `Menu` снова владеет root frame state и modal routing — выполнено

В Windows `Menu::SetState` сбрасывает ввод и передаёт состояние в
`ApplyState`, который одновременно управляет GUI mode, cursor/invert-Y и
видимостью Main/Race/Hud/Finish/Info/Final/Options frames. Каждый
`MenuFrame` отдельно владеет visible/modal/topmost, делает invalidate до
layout и ограничивает позицию 15-пиксельным отступом от viewport. В порте
эти обязанности были рассыпаны по локальному `std::vector<MenuScreen>`,
`menuSelection`, domain-specific bool dialogs и pointer gates в 22k-line
entry point.

Добавлен `originalmenu::MenuSystem`: source state machine, `ScreenStack`,
frame registry и modal ordering теперь не зависят от D3D9 или legacy Widget.
Живой SDL/bgfx path использует их для экранных переходов, сбрасывает
`originalcontrol::ControlManager`, синхронизирует Accept/Message/Loading/
UserChat/Options frames и выбирает верхний modal owner перед обработкой
мыши/клавиатуры. `Show`, `ShowModal`, hidden layout gate, invalidate/layout
order и `SetPos` clamp закреплены отдельным regression. Сборка arm64, 19/19
офлайн tests, 2/2 network tests, physics smoke и 360-frame Metal race smoke
прошли.

Этот блок сам по себе не объявлял concrete GUI завершённым. Common
`DialogMenu2` закрыты следующим P2.174, а Main/Profile, Options,
Planet/Garage/Workshop/Race и Finish/Final всё ещё формируют часть
widgets/draw data в host и составляют B6c+; CoreText/bgfx являются допустимой
backend-границей.

### P2.174 — common `DialogMenu2` больше не являются host-заглушками — выполнено

Accept/Info/Weapon/Music внешне рисовались исходными изображениями, но их
state и геометрией владели четыре локальные visual-структуры и отдельные
`musicDialogTime/Offset/Visible`. Это оставляло формальную копию source
значений в renderer и расходилось с `Menu::ShowWeaponDialog`,
`ShowMessage`, `ShowMusicInfo` и `OnProgress`: delayed visibility,
`_weaponTime == -2`, modal result/lifetime и повторный Music popup не имели
общего владельца с `MenuFrame`.

`originalmenu::DialogSystem` теперь содержит backend-neutral перенос
`DialogMenu2.cpp`: normal/max Accept scaling и focus/result, фиксированные
Info/Weapon label layouts, source delay sentinels и точную 1+3+1-секундную
Music animation/placement. Активные dialogs и оба menu/race music catalogs
используют этот state; renderer хранит только производные текстовые texture
handles. Новый regression закрепляет layout, clamp, modal/result, delays и
popup expiry. Сборка arm64, 20/20 offline, 2/2 network, physics и повторный
360-frame Metal smoke прошли; последний также подтвердил race music popup.
UserChat не дублировался — он уже имеет отдельного source owner
`OriginalUserChat`.

Следующая граница B6c — concrete Main/Profile/Options/Race frames. Этот блок
не объявляет весь Widget tree перенесённым.

### P2.175 — `MainMenu2` и `ProfileFrame` снова владеют navigation/layout — выполнено

Ручной host-код хранил `ProfileFocus`, `profileFocusIndex` и
`profileGridScroll`, сам воспроизводил весь граф Up/Down/Left/Right и
самостоятельно определял доступность `GameMode/Tournament`. При этом
Tournament Continue ошибочно включался при любом профиле, хотя оригинальный
`TournamentFrame::OnShow` проверяет отдельный `GetLastProfile(netGame)`.

Добавлены backend-neutral `mainmenu2::FrameController` и
`ProfileFrameState`, прямо сопоставленные с `MainMenu2.cpp` из
`eff9338`: `SetItems/AdjustMenuItems`, tutorial gate, раздельные
last-profile/has-profile gates, circular disabled-skip navigation,
четырёхстрочный grid, scroll arrows, item/close пары, Back и фиксированные
координаты. Активный SDL path направляет keyboard/gamepad/pointer в эти
owners и исполняет только возвращённые select/delete/back команды; bgfx
оставлен draw executor.

Новый `rrr3d_original_main_menu_frames_smoke` проверяет source availability,
layout, disabled wrap, полный Profile focus graph, scroll/select/delete и
clamp после удаления. Сборка arm64, 21/21 offline, 2/2 network, physics и
360-frame Metal smoke прошли. Следующая граница B6d — `OptionsMenu` frames;
полный concrete Widget tree этим блоком ещё не объявлен завершённым.

### P2.176 — `OptionsMenu` и first-run `StartOptionsMenu` снова имеют source owner — выполнено

Четыре option pages уже выглядели близко к Windows и использовали исходные
ресурсы, но их draft, steppers, control column и scrolling всё ещё были
несколькими сотнями строк switch/локальных переменных в renderer entry point.
Это дало наблюдаемое семантическое расхождение: camera distance и laps на
краях зажимались, хотя Windows `StepperBox` циклически переходит к первому или
последнему значению. First-run frame также отдельно хранил camera sentinel,
focus, четыре индекса и Apply gate в host.

Добавлены backend-neutral `originaloptions::OptionsMenuState` и
`StartOptionsMenuState`, прямо сопоставленные с `GameFrame`, `MediaFrame`,
`NetworkTab`, `ControlFrame`, `OptionsMenu` и `StartOptionsMenu` из
`eff9338:prog/Rock3dGame/source/game/OptionsMenu.cpp`. Они владеют 12/8/5/18
rows, source availability, draft commit/cancel, всеми option mutations,
bindings и columns, grid geometry, first-launch `cPrefCameraEnd` sentinel,
camera-only Apply enable и navigation ring. SDL/Cocoa, live volume,
commentator reload, XML persistence и bgfx/CoreText остались backend-командами.

Новый `rrr3d_original_options_menu_smoke` проверяет endpoint wrap, gates,
display/language/volume steppers, bindings, обе сетки и полный StartOptions
Apply lifecycle. Сборка arm64, 22/22 offline, 2/2 network, physics и
360-frame Metal smoke прошли. Следующая граница B6e — concrete
Planet/Garage/Workshop/Race и Finish/Final owners.

### P2.177 — `RaceMenu2` Main/Gamers state и команды возвращены исходным владельцам — выполнено

Предзаездные экраны уже рисовали исходные ресурсы и данные, но
`RaceMenu::ApplyState`, seven-button `RaceMainFrame` и весь `GamersFrame`
navigation graph оставались локальными bool/index/switch в renderer. В
частности общий `MenuSystem` ошибочно считал Gamers состоянием Main, хотя
Windows создаёт его дочерним frame `RaceMenu` и показывает в `Menu::msRace`.

Новый backend-neutral `originalracemenu` переносит `RaceMenuState` с exact
car/spaceship/frame visibility и last-state, `RaceMainFrameState` с семью
source commands, circular horizontal graph, ready-client lock и layout, а
также `GamersFrameState`. Последний владеет ordered gamer availability,
current-id fallback, non-wrapping previous/next, Next/Left/Right graph,
shoulder virtual keys, select/confirm commands и layout. Active SDL path
теперь только строит availability snapshot, исполняет команды и рисует
bgfx/Metal.

Исправлено состояние Gamers → Race. Новый
`rrr3d_original_race_menu_smoke` проверяет `ApplyState`, client-ready gate,
все семь команд/layout, gamer availability/navigation/shoulders и layout.
Сборка arm64, 23/23 offline, 2/2 network, physics и 360-frame Metal smoke
прошли. Следующий B6e.2 — Garage/Workshop/Angar/Achievment owners.

### P2.178 — `RaceMenu2::GarageFrame` получил исходного владельца — выполнено

Garage уже использовал оригинальные панели, каталог, 3D CarFrame и
транзакции, но порядок машин, выбранный индекс, прокрутка верхнего car grid и
полный Back/Buy/arrows/colors graph оставались ручной реализацией внутри
renderer entry point. Там же сохранялись два более старых обработчика Garage,
которые были недостижимы после раннего `continue` и описывали другое
циклическое поведение.

Добавлен backend-neutral `originalracemenu::GarageFrameState`, прямо
сопоставленный с `GarageFrame::UpdateCarList`, `AdjustCarList`, `OnShow`,
`OnInvalidate` и `OnClick` из `eff9338:RaceMenu2.cpp`. Он владеет
available → secret → locked order, campaign secret filter, current-car
selection, non-wrapping Prev/Next, сохраняющим окно алгоритмом car grid,
14 color availability, 18-node graph, shoulder commands и clear-on-hide
lifetime. Сетевые цвета поступают как snapshot; purchase/profile/network и
bgfx/CoreText остаются внешними исполняемыми командами.

Из host удалены `GarageCarView`, `garageCarOrder`, два индекса, ручной
`garageNeighbor` и оба мёртвых fallback-пути. Regression проверяет порядок,
campaign filter, locked state, границы, скрытый сетевой цвет, repaint,
shoulders, car-grid window и layout. Следующая граница B6e.3 —
Workshop/Angar/Achievment owners. Сборка автономного arm64 app, 23/23
offline, 2/2 network, physics smoke и 360-frame bgfx/Metal race smoke прошли.

### P2.179 — `RaceMenu2::WorkshopFrame` получил исходного владельца — выполнено

Workshop уже выполнял исходные профильные транзакции, но renderer всё ещё
владел отдельными `WorkshopDrag`, `WorkshopConfirmation`, goods vector,
scroll и вручную повторял Back+10-slot graph и всю раскладку. Это оставляло
несколько параллельных состояний между mouse hover, keyboard focus, modal и
отрисовкой.

Новый `originalracemenu::WorkshopFrameState` переносит
`UpdateGoods/AdjustGood/ScrollGood`, `OnShow/OnInvalidate`, nav elements,
drag origin/payload и confirmation lifetime из `eff9338:RaceMenu2.cpp`.
Owner фильтрует rewards, выполняет stable cost sort, формирует 12 visible
cells, владеет scroll, различает mouse-only goods и keyboard slot controls,
пропускает hidden/disabled widgets и возвращает Back/Good/Slot commands.
Source layout теперь выдаёт координаты goods, десяти slots, scroll arrows и
Back; `OriginalGarage` остаётся владельцем денежных и install/sell/recharge/
upgrade транзакций.

Удалены все четыре host mirrors. Интеграционная проверка обнаружила отдельный
дефект: Pause/Escape с hovered good исполнял путь Good вместо Back и показывал
`svHintWeaponNotSupport`. Back command теперь исполняется по типу команды, а
не по сохранённому hover index. Regression покрывает sort/filter/scroll,
mouse-only goods, точный slot graph, disabled traversal, slot-plane click,
drag, confirmation и layout. Следующая граница B6e.4 — Angar/Achievment.
Сборка автономного arm64 app, 23/23 offline, 2/2 network, physics smoke и
360-frame bgfx/Metal race smoke прошли.

### P2.180 — `RaceMenu2::AngarFrame` и `SpaceshipFrame` получили исходных владельцев — выполнено

Видимый Angar уже использовал оригинальные панели, планеты, boss cars и 3D
сцену, но renderer продолжал владеть восемью отдельными переменными selection,
previous selection, door timer, scene/red-lamp clocks и travel dialog. Ручная
навигация также расходилась с `RaceMenu2.cpp`: Left/Right на Back переходили к
планете, хотя source graph оставляет обе связи пустыми.

Новые `originalracemenu::AngarFrameState` и `SpaceshipFrameState` переносят
`OnShow/OnInvalidate/OnFocusChanged/OnClick/OnProgress`: champion selection,
кольцо planet viewports/slots, Back edges, campaign/skirmish/network gates,
Stay/Fly modal, 0.25-секундные двери, geometry anchors и непрерывный
трёхсекундный red-lamp clock. Host теперь только применяет ChangePlanet/Back,
показывает source dialog и передаёт готовые координаты и alpha в bgfx.
Regression покрывает champion/skirmish commands, modal Yes/No, точный Back
graph, midpoint дверей, lamp phases и layout. Автономная arm64 сборка, 23/23
offline, 2/2 network, physics и 360-frame bgfx/Metal race smoke прошли.
Следующая граница B6e.4b — `AchievmentFrame`.

### P2.181 — `RaceMenu2::AchievmentFrame` получил исходного владельца — выполнено

Карточки и profile transaction были source-derived, но definitions,
selection, purchase modal и navigation оставались в renderer. Упрощённый
алгоритм шёл только по запрошенному направлению и не повторял рекурсивный
`Menu::NavElementFind`; кроме того, порт сразу назначал focus карточке 0,
тогда как `SetNavElements` снимает focus со всех widgets, и первый direction
лишь устанавливает зарегистрированный key Back.

`originalracemenu::AchievementFrameState` теперь владеет точными девятью
definitions/order/positions, состояниями Missing/Locked/Unlocked/Opened,
serialized price, Back+9 graph, исходным initial no-focus, recursive disabled
traversal, reverse mouse overlap, Buy confirmation и layout. Host оставляет
points/profile write и специальное применение armor4, затем обновляет source
snapshot; bgfx/CoreText только исполняют draw. Regression проверяет definitions,
первый direction → Back, Back → Phaser, обход locked Tankchetti к MusicTrack,
Yes/No, opened disable, locked pointer и координаты. Автономная arm64 сборка,
23/23 offline, 2/2 network, physics и 360-frame bgfx/Metal race smoke прошли.
Следующая граница B6e.5 — Finish/Final owners.

### P2.182 — `FinishMenu` получил исходного владельца — выполнено

Прямая сверка с `eff9338:FinishMenu.cpp` подтвердила два функциональных
расхождения. Renderer заменял сериализованный `Race::Result::voiceNameDur`
константой 1.5 секунды и обрезал список результатов до трёх строк до поиска
последнего участника. Последняя реплика поэтому выбиралась эвристикой по
максимальному `Player::place`, а не исходным `results.back().playerId`.

Новый `originalracemenu::FinishMenuFrameState` владеет полным порядком
результатов, тремя box states, индивидуальными voice durations, исходными
0.15 s delay/0.5 s reveal и alternating slide offsets. Он выдаёт события
First/Second/Third при первом появлении строки и ровно одно Last для последней
записи, независимо от portable place heuristic. Host лишь ставит возвращённые
commentator events в очередь и рисует pose. Close action/escape/mouse command
и layout также перенесены; Money/Points labels и значения восстановлены как
двухстрочные блоки на исходной координате 154 вместо разнесённых 136/172.

Regression использует разные 0.4/0.8/1.2 s durations и отдельного последнего
player/racer, проверяя порядок событий, направления offsets, lifecycle и
геометрию. Автономная arm64 сборка, 23/23 offline, 2/2 network, physics,
отдельный 360-frame FinishMenu и 360-frame bgfx/Metal race smoke прошли.
Следующая граница B6e.5b — `FinalMenu`.

### P2.183 — `FinalMenu` получил исходного владельца — выполнено

Видимый финальный экран уже использовал исходные изображения и музыку, но
большой renderer entry point сам разбирал `svCredits`, вёл 107-секундный
clock, вычислял slide alpha, обрабатывал Back и задавал layout. Кроме того,
все девять слайдов масштабировались по aspect первого DDS, поэтому изображения
с иными пропорциями искажались.

Новый `mainmenu2::FinalMenuFrameState` буквально переносит
`eff9338:FinalMenu.cpp`: разделяет credits по `\n\n` и первой строке,
сбрасывает clock в `OnShow`, выдаёт source scroll/layout, девять временных
интервалов/alpha, Back command и автоматическое закрытие при 107 секундах.
CoreText создаёт только caption/body line textures, bgfx рисует payload и
использует собственный aspect каждого slide, SDL music adapter запускает
`TrackFinal.ogg` с нулевого кадра и возвращает menu MusicCat после закрытия.

Regression закрепляет section parsing, pointer/keyboard input, первый fade,
переход ко второму slide, половину scroll/layout и точный auto-close. Metal
fixture дополнительно ждёт реального фонового Ogg decode, после чего наблюдает
все девять slides, credits, Back, музыку и возврат в MainMenu2. Автономная
arm64 сборка и 360-frame FinalMenu smoke прошли; полный набор offline/network,
physics и race renderer также прошёл. Следующая граница B7 —
`Environment/TraceGfx` и renderer policy.

### P2.184 — `Environment` получил исходного владельца — выполнено

Прямая сверка `eff9338:prog/Rock3dGame/source/game/Environment.cpp` показала,
что значения погоды и world profiles уже присутствовали в порте, но были
скопированы четырежды: в race loader, CLI weather helper, presentation
loaders и smoke expectation. Quality graph и rain lifetime дополнительно
оставались частью большого Metal renderer, поэтому фактического владельца
исходного класса не существовало.

Новый `source::Environment` переносит `ApplyWheater`, `ApplyWorldType`,
`ApplyQuality`, `GetPerspectiveCameraFar`, `StartScene`, `ProcessScene` и
`ReleaseScene`. Он задаёт weather/fog/ambient/sky/far, World1–World6
surface/HDR, Garage/Angar lamps, shadow/light/post/environment quality gates,
isometric fog/sun-shaft/rain exclusions и следование rain за камерой.
`OriginalRace` использует owner при загрузке мира и presentation scenes,
host — для CLI и smoke policy, renderer — для активного pass graph и rain.

Отдельный regression покрывает token mapping, magma/snow/Garage profiles,
Middle/High/night/isometric graph и rain lifecycle. Прошли arm64 build,
24/24 offline, 2/2 network, physics, 360-frame race, Finish и Final Metal
smoke. Следующая граница P2.185/B7b — `TraceGfx`: source selection/reference
lifetime и debug draw records при сохранении bgfx submission.

### P2.185 — `TraceGfx` получил исходного владельца — выполнено

Активный AIDebug/F6 path был подтверждён как суррогат: renderer заранее
строил одну зелёную ribbon фиксированной полуширины 0.08, добавлял высоту
0.35 и всегда соединял последний point с первым. В Windows `TraceGfx`
показывает все waypoint boxes, каждый `WayPath::GetTriStripVBuf` с его
фактической шириной и отдельным серым оттенком, а зелёным выделяет только
selected point/path/tile/link.

Новый `source::TraceGfx` владеет selection/link state, material policy
(transparency, alpha 0.5, no lighting/Z-write/Z-test/fog/cull) и точным
backend-neutral draw list. Он работает непосредственно с уже перенесённым
`source::Map::Trace`. Renderer больше не строит игровую trace geometry:
Metal adapter лишь преобразует records в transient triangles, включая
camera-facing point-link вместо D3D9 `Sprite`.

Regression закрепляет boxes, full-width tile quads, grayscale path range,
все четыре selection/link ветви и очистку удалённой ссылки. Прошли arm64
build, 25/25 offline, 2/2 network, physics и 360-frame Metal race smoke с
`--game-debug`, где F6 включает новый путь. B7 закрыт; следующая граница B8 —
оставшиеся partial `Race/Player/AI/GameCar/Weapon` owners.

### P2.186 — восстановлена атомарная последовательность `AICar::UpdateAI` — выполнено

Метод-к-методу сравнение `eff9338:AICar.cpp` выявило не отсутствие формулы,
а ошибочное разнесение уже перенесённых частей по кадру. Windows
`AICar::UpdateAI` неизменно вызывает `_path.Update`, `_attack.Update`, затем
`_control.Update`, а `AICar::OnProgress` после этого выполняет reset-control.
Порт вызывал Path/Control из `aiInput`, а Attack — позже внутри большого
`OriginalRaceSession::updateGameplay`. Поэтому выбор hyper/mine/weapon мог
видеть другой frame state, а target/RNG менялись после control.

Новый combined `AICar::Update` и `AIPlayer::OnProgress` возвращают один
`ProgressResult` с move/reset и attack-командами, сохраняя исходный порядок.
Session собирает только Jolt pose/speed и живой `WeaponItem` snapshot,
вызывает source transaction один раз и затем переводит команды в
`VehicleInput`/`Weapon::Shot`. Отдельный публичный `AIPlayer::UpdateAttack` и
повторная session dispatch удалены; network authority и debug-human gate
остались адаптерной границей.

`OriginalAICarSmoke` теперь закрепляет одновременную acceleration и front
weapon decision из одного текущего path state. Прошли arm64 build, 25/25
offline, 2/2 network, physics и 360-frame bgfx/Metal race smoke. Следующий
блок B8b — полная сверка `GameCar` callback/order с заменой PhysX queries на
Jolt snapshots.

### P2.187 — `GameCar::OnContact` возвращён исходному владельцу — выполнено

Сверка с `eff9338:prog/Rock3dGame/source/game/GameCar.cpp` подтвердила не
просто backend-замену, а оставшийся суррогат. Border-contact и car-contact
ветви одного Windows callback были разнесены по разным участкам
`OriginalRaceSession::updateGameplay`; session сама интерполировала damage,
выбирала жертву по kinetic energy, снимала clutch и строила spring velocity.
Кроме того, при нулевом `sumFrictionForce` прежняя копия подставляла
геометрический binormal, чего в оригинале нет.

Добавлен backend-neutral `source::GameCar::OnContact`. Он сохраняет точный
порядок Windows: `_bodyContact = true`; track damage threshold и
`!springBorders && alpha == 0` early return; нормализация/inversion normal;
проверка `cdgShotTransparency`; сброс clutch; порог модуля скорости `>16`;
spring-border redirect по normal/friction/forward; touch damage; car victim
по `computeKineticEnergy`; нулевой `target->Damage` для прочего объекта.
Метод возвращает только команды velocity/damage/touch, потому что Jolt body
и конкретный MapObj остаются backend-адаптером.

`OriginalRaceSession` теперь лишь классифицирует Jolt actor, передаёт
normal/friction force, velocity, forward и kinetic-energy snapshot, затем
исполняет source result. Отдельные session-owned damage/rebound формулы
удалены. `OriginalGameCarSmoke` закрепляет early return без сброса clutch,
border damage, spring redirect, обе energy-ветви и decoration touch.
Следующий B8c — аудит оставшихся partial `Player/Race/Weapon` методов.

### P2.188 — presentation graph `Player` возвращён исходному владельцу — выполнено

Прямая сверка блока `Player.cpp:InitLight` — `ApplyColor` подтвердила, что
порт сохранил видимый результат, но не владельца. `Player` содержал лишь
mode/boolean/color, тогда как `OriginalRaceRenderer` заново определял число
фар, их `0.3/±1/3.190` transforms, quaternion, range/cones, список flare из
garage record, reflection exclusion и color-material gate. Поэтому это была
вторая реализация исходных Player-методов внутри Metal backend.

Добавлен `source::Player::PresentationState`. `SetHeadlight` теперь точно
выполняет `InitLight/FreeLight`, создаёт/удаляет night-flare actor state и
сохраняет локальные spot-light параметры. `CreateCar` вызывает
`SetLightsParent`, `ApplyReflScene`, slot creation и `ApplyColorMaterial`;
`FreeCar` сначала разрушает slot actors, затем отсоединяет lights/flare и
color material от удаляемого actor, сохраняя созданный clone до следующего
`ApplyColorMat`, как Windows `ReleaseCar`. `SetColor` и `SetReflScene`
немедленно меняют активный presentation owner.

Renderer теперь только composes source local transforms с Jolt body,
передаёт готовые light/material records в bgfx, исключает car из cube pass
по source `reflectionScene` и рисует только source night-flare nodes.
Regression проверяет detached/attached lifetime, One/Two transforms,
white/red flare records, release/re-attach, reflection и material color.
Прошли arm64 build, 25/25 offline, 2/2 network, physics и 360-frame Metal
race smoke.
Следующий блок B8d — `Race/Weapon` method audit.

### P2.189 — завершён исходный `CameraManager`: AutoObserver, FlyTo и screen/ray — выполнено

Аудит B8d сначала исключил ложные пробелы. `Proj::EnableFilter/DisableFilter`
уже имеют Jolt-эквивалент: луч Laser/FrostRay исключает машину-владельца,
не меняя фильтр остальных тел. `Race::ResetCarPos` также перенесён буквально:
четыре машины в ряду, интервал 7, visual-AABB width, `Vec2NormCW`, высота +2
и обнуление всего физического состояния через `resetVehicle`.

Реальный разрыв найден в `CameraManager`. Активные Garage/Angar
`csAutoObserver` формулы находились локальной state machine в SDL entry point,
а `FlyTo/StopFly/InFly` и `ScreenToWorld/WorldToScreen/ScreenToRay/`
`ScreenPixelRayCastWithPlaneXY` в source owner отсутствовали. Добавлен
backend-neutral `source::AutoObserver`: порог drag 15 px, mouse angular
scale `pi*0.001`, трёхсекундное auto-restore/rotation, asymmetric yaw clamps,
pitch bounds, direction reversal и quaternion interpolation теперь живут в
`Rock3dGame`. SDL-функция только переводит pointer down/up/move, а Garage и
Angar продолжают использовать собственные независимые observer instances.

`source::CameraManager` получил исходную сглаженную FlyTo transaction и
чистые camera-space projection/ray операции. bgfx по-прежнему отвечает лишь
за построение и отправку matrices. Regression закрепляет fly completion,
perspective screen/world round trip, center ray, пересечение плоскости XY,
observer idle turn/clamp/reversal и drag threshold. Arm64 build и отдельный
CameraManager smoke прошли; также прошли 25/25 offline, 2/2 network, Jolt
physics и 360-frame SDL/bgfx/Metal race regression.

### P2.190 — возвращены `Achievment`, `AchievmentMapObj` и `AchievmentGamer` — выполнено

Прямая сверка `AchievmentModel.cpp` обнаружила, что девять condition-классов
уже принадлежали active `source::AchievmentModel`, но верхний reward layer
оставался распределённой повторной реализацией. Entry point вручную разбирал
`asLocked/asUnlocked/asOpened`, вычитал points и открывал карточку, а Garage
отдельно обходил records и gamerId. Это было функциональным surrogate, а не
переносом исходных владельцев.

`source::AchievmentModel` теперь хранит полный профиль items без потери
неизвестных полей, выполняет исходные `Unlock`, `Open`, `Buy`,
`ConsumePoints`, `CheckAchievment`, `CheckMapObj` и `CheckGamerId` и сохраняет
результат обратно через profile adapter. `OriginalRaceSession` публикует этот
owner активным Race/Menu-клиентам; AchievmentFrame больше не меняет XML-map
самостоятельно, а Garage/Gamers делегируют тому же source rule set.

Regression закрепляет неизвестный achievement=true, запрет покупки locked,
переход Locked→Unlocked→Opened, атомарное списание points, недостаточный
баланс, map/gamer gates и сохранение records/custom fields.

### P2.191 — `DataBase` снова владеет семью `RecordLib` — выполнено

Повторная сверка `DataBase.cpp`, `RecordLib.cpp`, `MapObj.cpp` и active
session path выявила ошибку ownership. Иерархия typed records уже была
перенесена, но семь библиотек хранились внутри `source::Map`, а функция
`OriginalRaceSession::registerSourceDataBase` вручную создавала source-loader
для каждой карты. В Windows `DataBase` живёт дольше Map и является единым
владельцем библиотек и concrete source-load transaction.

Добавлен active backend-neutral `source::DataBase`. Он владеет всеми семью
категориями, выполняет единый `Configure/Clear`, предоставляет исходные
`GetMapObjLib/GetRecord`, загружает records ctDecoration/ctTrack/ctBonus/ctCar
и сохраняет source-before-proxy порядок. `source::Map` теперь получает
DataBase извне; только isolated unit map создаёт локальный owner. Из session
удалена вся 116-строчная повторная регистрация record loaders.

Map regression проверяет external owner identity, четыре категории,
destructible child source transform, car life, точный record lookup и
безопасный reload/fix-up после уничтожения live MapObj.

### P2.192 — восстановлен исходный владелец `HudMenu` — выполнено

Сверка `HudMenu.h/.cpp` подтвердила, что активный `OriginalRaceHud` уже
рисовал большинство правильных ресурсов и повторял анимации уведомлений, но
самого source-владельца не существовало. Позиции `PlayerStateFrame` и
`MiniMapFrame`, одно состояние `msMain`, Escape/pause transaction и
`tablo0..tablo4` оставались числами и условиями внутри bgfx/SDL слоя.

Добавлен backend-neutral `source::HudMenu`. Он владеет исходной видимостью
Main, всеми layout-точками и minimap 320x320, фильтром Escape down/non-repeat,
а также event-driven countdown: изображения 0..3 сохраняются до следующего
события, `tablo4` за 1.5 секунды теряет alpha и увеличивается на 200 единиц в
секунду. Активный HUD больше не выводит countdown из phase/elapsed повторно,
а SDL применяет source-команду к существующим cursor/dialog/pause adapters.
bgfx оставлен только загрузчиком ресурсов и исполнителем draw calls.

Regression фиксирует все координаты, state visibility, обе Escape-ветви,
каждую countdown-стадию, середину/завершение анимации и Reset. Следующий
HUD-блок — перенос runtime collections `PlayerStateFrame`/`MiniMapFrame`,
которые пока ещё хранятся в renderer adapter.

### P2.193 — `MiniMapFrame::BuildPath/UpdateMap` возвращены в source — выполнено

Продолжение HUD-аудита нашло крупный алгоритмический surrogate: 371 строка
`MiniMapFrame` была встроена прямо в `OriginalRaceHud.cpp`. Хотя формулы уже
были близки к Windows, владельцем road graph, align/smoothing и world-to-map
преобразования ошибочно являлся bgfx renderer.

Добавлен `source::MiniMapFrame`. Он повторяет `ComputeNode`, `AlignNode`,
`AlignMidNodes` и `BuildPath`: допуск 20 градусов, size error 2, smoothing
radius 10 с двумя slices, исходные half-width/radius формулы и alternating UV.
Там же теперь находятся 320x320 fit по диагонали bounds, right-top anchor,
start direction/size и стабильное преобразование `CarState::GetMapPos` в HUD.

Из `OriginalRaceHud` удалены локальные bounds/scale/origin/start поля и весь
повторный алгоритм. Adapter конвертирует только backend-neutral вершины в
bgfx `Vertex`, загружает mesh и передаёт color/draw calls. Regression строит
замкнутую карту с поворотами, проверяет topology, UV-compatible pairs,
start orientation/size, оси world-to-map и Clear/rebuild lifetime.

### P2.194 — очереди `PlayerStateFrame` возвращены source owner — выполнено

После переноса внешнего HudMenu и MiniMap оставались renderer-owned
`PickItems` и `AchievmentItems`. Это было не только неверное ownership:
achievement popup не получал исходный `lastIndex = current size`, а fly
считался линейно от сохранённой стартовой точки. Windows каждый кадр делает
lerp от текущей widget position, поэтому траектория и перестроение stack
различались.

Добавлен active `source::PlayerStateFrame` с устойчивыми `HudItemId`.
`NewPickItem/ProccessPickItems` теперь владеют insert-front order, 5 s
lifetime, fade 0.3/4.7, шагами 90/120 и spacing 85. Source
`NewAchievment/ProccessAchievments` выбирает одну из восьми исходных стартовых
позиций, сохраняет slot/image dimensions, правильный initial lastIndex,
current-position fly 0.3 s, double-size ping 0.2–0.4, points fade после 0.8,
stack reindex 0.15 и общий fade/remove в 4.7–5.0 s.

`OriginalRaceHud` хранит только GPU images, kill labels и payload по id;
позиция, alpha, scale, ordering и lifetime читаются из source owner. При
удалении source item adapter освобождает соответствующий CoreText texture.
Regression проверяет pick slide/fade/removal, точную achievement trajectory,
ping/points alpha, двухэлементный reindex, lifetime и Reset.

### P2.195 — `PlayerStateFrame::CarLife` возвращён source owner — выполнено

Car-life overlays оставались последним крупным runtime state внутри bgfx HUD.
Сверка показала два поведенческих расхождения: порт всегда сбрасывал общий
alpha при повторной атаке и плавно гасил overlay уничтоженной машины. Windows
раздельно ведёт alpha background/bar, повторно обнуляет только полностью
видимый bar и немедленно освобождает target при исчезновении GameObject.

`source::PlayerStateFrame` теперь владеет двумя исходными слотами CarLife,
target identity, timer/timeMax, progress, background/bar alpha и visibility.
Восстановлены `ShowCarLifeBar`, `StepLerp` и `ProccessCarLifeBar`: 0.3 s fade,
1.5/4.0 s durations, edge fade, повторный bar flash и immediate destroyed
release. Также возвращена точная исходная экранная раскладка: projected point
clamp в `[0, vp-back]`/`[backHeight, vp]`, затем half-size offset.

Renderer передаёт только результат camera projection, life fraction и размеры
backend image; draw отдельно применяет source background/bar alpha.
Opponent suppression теперь использует source `GetCarLife` identity до
полного release. Regression покрывает first fade-in, placement, repeated hit,
edge fade, separate alphas и destroyed cleanup.

### P2.196 — `PlayerStateFrame::Opponent` возвращён source owner — выполнено

Прямая сверка `UpdateOpponents`, `UpdateState`, `RemoveOpponent` и
`OnDisconnectedPlayer` подтвердила, что активный HUD всё ещё держал состояние
opponent labels внутри bgfx adapter. Одновременно существовали поведенческие
расхождения: текст располагался приблизительным сдвигом `-20`, collision
center совпадал с точкой вместо label world position, car-life менял только
alpha, но не исходный list order, а overlap пропускал скрытые предыдущие
элементы.

В `source::PlayerStateFrame` добавлены исходные `Opponent` collection и полный
кадровый transaction. Source выполняет stable sort по убыванию места, затем
по порядку двух car-life slots переносит targets в начало; сохраняет состояние
скрытого уничтоженного объекта, удаляет отключившегося участника, повторяет
edge/car-life fade `alpha - 4*dt` и считает перекрытие со всеми предыдущими
элементами списка. Восстановлена widget-геометрия dummy/point/centered label:
исходные clamps через text AABB minimum, отдельные point/label positions и
радиус по text AABB либо `carLifeBack`.

`OriginalRaceHud` теперь хранит только CoreText texture по racer id, получает
camera projection и передаёт размеры backend ресурсов. Source state определяет
draw order, visibility, позиции, radius и alpha; отключённые GPU labels
освобождаются сразу. Regression фиксирует place order, обе экранные позиции,
car-life move-to-front/radius/fade, edge fade, collision suppression, hidden
lifetime, disconnect removal и Reset.

### P2.197 — `UpdateSlots/UpdateState` и lap HUD возвращены source owner — выполнено

Следующая сверка `PlayerStateFrame::InsertSlot`, `ClearSlot`, `UpdateSlots`,
`UpdateState`, `OnAdjustLayout` и `MiniMapFrame::OnProgress` подтвердила ещё
один renderer surrogate. bgfx HUD напрямую выбирал weapon definitions,
считал charge text, selected slot, place/life и lap. При этом четыре primary
слота располагались по физическим индексам с пустыми промежутками, а Windows
компактно размещает только установленные boxes. Hyper/mine всегда рисовались
с отдельными `hyperSlot.png`/`mineSlot.png`, хотя исходный HUD эти RaceMenu
icons вообще не создаёт; его два 3D viewport также компактно занимают первые
доступные Hyper/Mine layout-точки.

Добавлены `HudRaceStateInput`, шесть исходных `HudWeaponSlot` и
`HudPlayerRaceState` в active `source::PlayerStateFrame`. Source теперь
владеет Hyper/Mine/Weapon1..4 order, compact indices, selected primary,
absolute box/view/label positions, visual identity, charge pairs, place и
life progress. Сохраняется исходное поведение жизни: при отсутствии car
GameObject bar удерживает последнее значение вместо принудительного нуля.
`MiniMapFrame` получил исходный `min(numLaps + 1, lapsCount)` и lifetime
lap state.

Renderer только собирает portable Slot/WeaponItem snapshot, локализует
готовые place/lap/charge values и исполняет source draw positions. Удалены
придуманные HUD-загрузки и draw calls `mineSlot.png`/`hyperSlot.png`;
оригинальные primary `slot.png`/`slotSel.png` остаются. Regression покрывает
lap clamp/reset, compact primary gaps, physical selected-slot mapping,
Hyper/Mine compression, charge/visual identity, life hold и Reset.

### P2.198 — `MiniMapFrame::CreatePlayers/UpdatePlayers` возвращены source owner — выполнено

Завершающий MiniMap-аудит подтвердил, что после переноса `BuildPath/UpdateMap`
renderer всё ещё каждый кадр заново создавал временный `mapMarkers_`. Это
теряло исходный lifetime `CreatePlayers -> DelPlayer/OnDisconnectedPlayer`,
а количество markers дополнительно ограничивалось размером physics vehicle
массива, чего Windows `Race::PlayerList` не делает.

`source::MiniMapFrame` теперь владеет стабильной player collection по racer
identity. `UpdatePlayers` создаёт отсутствующие записи в roster order,
обновляет source `CarState::GetMapPos` и car color, удаляет только исчезнувших
или отключённых участников и переводит world map coordinates через тот же
source map transform. `Clear` освобождает geometry, lap и players единым
lifetime transaction.

`OriginalRaceHud` больше не содержит `MiniMapMarker` и не реконструирует
коллекцию renderer-owned объектов. Он передаёт source position/color snapshots
и рисует готовые markers; bgfx texture/20x20 Plane3d-compatible quad остаются
backend boundary. Regression проверяет roster order, map transform, position
и color update, disconnect removal, lookup и Clear.

### P2.199 — `PlayerStateFrame::OnProcessEvent` возвращён source owner — выполнено

Прямая сверка последнего крупного метода PlayerStateFrame подтвердила, что
bgfx adapter самостоятельно интерпретировал Bonus/Achievement/Damage/Kill и
countdown events. Нашлась не только ownership-проблема, но и реальная ошибка:
portable `RaceEvent` хранит victim в `racer`, attacker в `target`, а исходный
`cPlayerDamage` — attacker в `playerId`, victim в `targetPlayerId`. Adapter
частично развернул смысл, но назначил повреждение игрока в slot 0 и повреждение
соперника в slot 1. В Windows enum обратный: `clOpponent=0`, `clHuman=1`.
Это меняло порядок car-life targets в opponent list и могло перекрывать не ту
подпись.

Добавлен source `ProcessEvent` transaction с `HudPlayerEventInput/Result`.
Он восстанавливает human-only pick/kill filters, kill-credit gate, mapping
Medpack/Ammunition(Mine/Hyper/Primary)/Money/Immortal, achievement creation,
countdown 0..4 и точные damage branches/durations: human slot 1 на 1.5 s,
opponent slot 0 на 4 s. Item queues и car-life mutируются внутри source owner;
result сообщает adapter только какой GPU payload создать.

`PickNotification` больше не хранит повторные gameplay `BonusKind/PickSlot`,
а использует готовый source `HudPickVisual`. Renderer отвечает только за
image dimensions, CoreText kill name/photo и `HudMenu` countdown texture.
Regression закрепляет visual mapping, фильтр чужого pick, achievement item,
обе damage field/slot ветви, kill gate/target и countdown command.

### P2.200 — удалён второй, выдуманный FinishMenu внутри race HUD — выполнено

Аудит перехода `Race -> FinishMenu` подтвердил крупное архитектурное и
видимое расхождение. В Windows `HudMenu.cpp` не содержит результатов финиша:
после `cRaceFinishTimeEnd` `Menu::ExitRaceGoFinish` создаёт отдельный
`FinishMenu`, который использует `Race::Results`, последовательные
`voiceNameDur`, три place events и отдельный last-player event. В порте эта
source `FinishMenuFrameState` уже была активна в host, но параллельно
`OriginalRaceHud` при одном только `RacePhase::Finished` строил собственную
трёхстрочную таблицу с единым alpha и сортировкой runtime places.

Именно этот surrogate мог показывать первую таблицу сразу на трассе, а затем
вторую правильную таблицу после `finishPresentationReady`; это соответствует
наблюдавшемуся двойному финишному экрану. Из race HUD удалены 226 строк второго
result owner: runtime sorting/reward fallback, global reveal timer, ранний
draw return, дублированные frames/cups/textures и CoreText rows.

Единственным владельцем результата остался уже перенесённый
`originalracemenu::FinishMenuFrameState`, вызываемый только source transition
`showFinishMenu`: он получает `raceSession.results()` в исходном порядке,
учитывает индивидуальные voice durations, выдаёт First/Second/Third/Last и
обрабатывает закрытие. Отдельный `--finish-menu-smoke-test` проверяет полный
Metal frame/audio/return path; обычный HUD до transition продолжает рисовать
трассу без придуманной промежуточной таблицы.

### P2.201 — `HumanPlayer::Control` input transaction возвращён source owner — выполнено

Прямая сверка `HumanPlayer.cpp` подтвердила, что SDL adapter схлопывал все
сообщения одного кадра в независимые bool-флаги, а `OriginalRaceSession`
исполнял их в собственном порядке: reset до weapon actions, Mine/Hyper до
Shot/ShotAll и только один direct-slot request. Windows передаёт каждое
`InputMessage` немедленно и сохраняет порядок доставки, затем отдельно
вызывает `OnInputProgress`: сначала движение, потом Hyper и analog Mine.

`source::HumanPlayer::OnHandleInput` теперь принимает ordered source
`InputMessage` list, применяет исходные block/car/chat, down/repeat и
controller-alpha gates и возвращает команды `ShotAll`, `ResetCar`, digital
Mine, `ShotCurrent`, WeaponDown/Up и direct Weapon1..4 в том же порядке.
Активный SDL путь больше не хранит девять параллельных gameplay flags:
он передаёт source-сообщения и непрерывные polled состояния Hyper/Mine.
Session исполняет event transaction до progress weapons и до mine/contact
прохода текущего кадра, как исходный `ControlManager -> World::Progress`.

Дополнительно устранено подтверждённое поведенческое расхождение: цифровой
`gaMine` больше не запускает Maslo. Исходник исключает `IsMaslo()` из edge
ветви и обрабатывает масло только continuous polling с readiness delay.
Исправлен и точный zero-weapon результат WeaponUp (`_curWeapon == -1`).
Regression закрепляет порядок, repeat/analog filters, gates и empty inventory.

### P2.202 — подготовка полного `Weapon/Proj` shot batch возвращена source owner — выполнено

После переноса `Weapon::CreateShot` session всё ещё заранее вычислял для
каждого projectile собственные world position/rotation, lifetime и стартовую
скорость. Последняя была просто `direction * desc.speed`: исходные ветви
`speedRelative` и `speedRelativeMin`, учитывающие скорость машины, до создания
объекта терялись. Это особенно расходилось для Torpeda/Impulse, которые в
Windows сохраняют результат `Proj::CalcSpeed` в `_vec1` уже при PrepareProj.

`source::Weapon::BuildShotContexts` теперь выполняет backend-neutral часть
исходной транзакции `Weapon::Shot -> CreateShot -> Proj::PrepareProj`: один
target передаётся всему descriptor batch, live transform оружия сочетается с
локальными position/rotation/scale каждого projectile, minimum lifetime
семплируется adapter-ом из исходного FloatRange, а окончательный max lifetime
и launch velocity вычисляются точными `Proj::PrepareMaximumLife/CalcSpeed`.
Jolt сообщает только transform/linear velocity mounted actor и затем создаёт
либо обновляет физическое тело; gameplay-параметры больше не формируются в
session.

Удалён ставший ненужным session `projectileWorldTransform`. Regression
проверяет двухснарядный batch, общий Logic/target/playerId, повёрнутый и
масштабированный mount, sampled lifetime и обе исходные relative-speed ветви.

### P2.203 — attached `Proj::OnProgress` и Drobilka rotation возвращены source owner — выполнено

Session самостоятельно компоновал transform для Hyper/Laser/Fire/Drobilka/
FrostRay, отдельно исправлял rotation Fire/Drobilka, копировал car velocity в
Fire и только затем вызывал часть source update. Вдобавок вращение Drobilka
дублировалось в `Player::weaponSpinRadians`: session накапливал scalar, а
renderer ещё раз строил quaternion. Это не является полем Windows Player и
могло расходиться с live Weapon после destroy/respawn или смены mount.

Добавлен единый `Proj::ProgressAttached`: он читает concrete descriptor,
применяет source `LocalToWorldCoord(_desc.pos)`, различает linked rotation и
точное присваивание weapon world rotation для Fire/Drobilka, возвращает
Fire linear velocity и внутри вызывает Drobilka update. Jolt/session теперь
передают только actor transform/scale/velocity и исполняют collision queries.

Drobilka rotation хранится в concrete `Weapon`, откуда её читают и contact
transform, и bgfx renderer. `Player::weaponSpinRadians` и session accumulator
удалены. Одновременно исправлен подтверждённый порядок quaternion multiply:
Windows выполняет `weapon->GetRot() * deltaX`, тогда как порт делал
`deltaX * localRotation`; ошибка была видна на непустом mount rotation.
Regression проверяет scale/rotation/velocity Fire, persistent Weapon spin,
неединичный mount и правое умножение, contact actor и исчезновение эффекта.

### P2.204 — free-projectile `Proj::OnProgress` dispatcher возвращён source owner — выполнено

После attached-блока session всё ещё самостоятельно выбирал RocketUpdate,
ResonanseUpdate и ThunderUpdate/Contact, мутировал position/rotation/velocity
между отдельными вызовами и вручную синхронизировал source transform. Это
оставляло type switch вне `Proj::OnProgress`. Также обнаружена функциональная
ошибка: `ProgressTorpeda` вызывался только при валидном runtime target, поэтому
исходный `_time1 = max(_time1-dt, 0)` не продвигался без цели. Получив цель
позже, Torpeda/Impulse снова ждали полные 0.4 s вместо уже прошедшего времени.

`Proj::ProgressFree` теперь единым concrete dispatch выполняет Rocket height,
Resonanse rotation, Thunder cooldown/reflection и окончательный graph sync.
Session интегрирует Jolt velocity/gravity и передаёт только результаты track
ray и shot-transparency contact. Homing update вызывается каждый кадр даже без
target; наличие цели влияет только на ветвь поворота, как в Windows.

Regression закрепляет no-target timer, последовательный Rocket clearance,
Thunder cooldown/reflection и запись итоговых position/velocity. В session
больше нет прямых вызовов `ProgressRocket`, `ProgressResonanse` или
`ProgressThunder`.

### P2.205 — placed mine progress dispatcher возвращён source owner — выполнено

Maslo/Mine/MineRip/MineProton оставались последним runtime path, где session
сам выбирал между `ProgressMine` и `ProgressMineRip`, отдельно переносил
arming alpha и синхронизировал transform до backend ballistic integration.
Теперь `Proj::ProgressPlacedMine` выполняет concrete type dispatch, source
arming/model scale, MineRip split decision и окончательный graph transform.

Session отвечает только за Jolt-подобное движение автономных fragments,
контакт с track plane, создание nested descriptor children и collision/network
authority. Source transform записывается после integration, поэтому concrete
Proj/DeathEffect больше не остаётся на позиции предыдущего кадра. Прямые
`ProgressMine/ProgressMineRip` вызовы из session удалены. Regression проверяет
arming completion, model scale и итоговую position/rotation placed Maslo.

### P2.206 — moving `Proj::OnContact` transaction возвращена source owner — выполнено

После progress-блоков collision adapter всё ещё вручную исполнял switch для
движущихся Rocket/Torpeda/Mortira/Thunder/Resonanse, Sonar и Impulse. Он
повторно вызывал `DamageTarget`, отдельно мутировал Impulse hit count и после
урона заново спрашивал `ContactRocket`. Последнее давало два расхождения с
`Weapon.cpp`: ракетный `Death(dtSimple, target)` происходил после damage, а
torque исчезал, если damage успевал уничтожить target до повторного route.

`Proj::ContactDynamic` теперь выполняет `GameObject::OnContact`, live-state
guard и concrete handler одним source transaction. Он заранее формирует
`DamageCommand`, Sonar impulse, Rocket torque и Impulse `_tick1`/retarget
решение. Adapter только применяет Jolt linear/angular deltas, damage command
и эффект уничтожения. Для rocket-family восстановлен исходный порядок
`Death -> Damage -> torque`; Impulse уничтожается с `dtEnergy`, а не с
прежним безусловным `dtSimple` внутри `DestroyWithEffect`.

Regression проверяет owner/player attribution, rocket death-before-damage
команду и torque, три последовательных Impulse damage step и сохранение
energy death type. Mine/bonus contact authority остаётся отдельной следующей
границей, поскольку её Windows-порядок проходит через `Logic::MineContact` и
сетевой RPC.

### P2.207 — `Logic::MineContact`/`TakeBonus` и RPC ordering возвращены source owner — выполнено

Прямая сверка `Logic.cpp`, `Weapon.cpp` и `NetPlayer.cpp` подтвердила два
расхождения. Для мины порт вызывал базовый `GameObject::OnContact` только при
получении сетевого replay, хотя Windows исполняет его на физическом контакте
до обращения к `Logic::MineContact`. Конкретный `Proj::MineContact` также был
переставлен: session наносил damage и impulse до смерти мины, тогда как source
выполняет `Death -> DamageTarget -> AddContactForce`.

`Logic::MineContact` теперь формирует исходный local/RPC dispatch, а
`Proj::ResolveMineContact` — concrete death/damage/force transaction. Session
оставляет за собой только overlap, target-owner gate, сетевой transport и
применение Jolt impulse. RPC replay не повторяет базовый listener pass.

Вторая подтверждённая ошибка находилась в бонусах: network client сразу
вызывал `Player::TakeBonus`, удалял MapObj и лишь затем отправлял событие в
transport. Windows `NetPlayer::TakeBonus` до RPC ничего не меняет; реальное
применение происходит только в `OnTakeBonus` на всех узлах. `Logic::TakeBonus`
теперь явно различает RPC request и player application. Pending request не
повторяется на следующих кадрах, не показывает HUD и не засчитывает
achievement; replay использует переданные type/value и только тогда удаляет
бонус.

Regression покрывает local/network dispatch, недоступный NetPlayer, точный
mine death-before-damage command, отсутствие ранней мутации life/MapObj при
bonus request и последующее replicated применение. Прошли 29/29 CTest,
physics smoke и 360-frame bgfx/Metal race smoke.

### P2.208 — атомарный `Player::OnProgress` restore возвращён source owner — выполнено

Сверка `Player::OnProgress`, `CreateCar(false)` и `ResetCar` обнаружила
критический двухкадровый surrogate. После смерти portable Player по истечении
двух секунд выставлял полную life и выдавал respawn, но оставлял
`carPresent == false` и не создавал новый car MapObj. Только следующий кадр
отдельно выполнял `CreateCar`. Session при этом передавал в Player придуманный
флаг `!IsDestroyed()` вместо проверки принадлежащего Player `_car.mapObj`.

Windows выполняет всё атомарно внутри одного fixed-step: пока MapObj отсутствует
увеличивается `_timeRestoreCar`; только при строгом `> 2.0f` вызываются
`CreateCar(false)` и затем `ResetCar`. `Player::OnProgress` теперь сам читает
`HasCar()`, а `ProgressRestore` сохраняет этот строгий timer и создаёт concrete
car до возврата единственной команды `QueueRespawn`. Adapter в том же callback
материализует новый source MapObj, выполняет ResetCar ray query и отдаёт Jolt
готовую pose. Промежуточный `ActivateCar` state удалён.

Regression проверяет отсутствие восстановления ровно на 2.0 s, новый MapObj
ID, live Player/attached lights и единственный respawn в одном кадре, а также
стабильность следующего кадра. Это устраняет состояние невидимой машины без
MapObj, способное приводить к телепортации после взрыва.

### P2.209 — ownership/listener lifecycle bonus `Proj` возвращён из Windows — выполнено

Прямая сверка `Player::InsertBonusProj`, `RemoveBonusProj`, `GetBonusProj`,
`GetBonusProjId` и `OnDestroy` подтвердила, что порт хранил только числовые ID
сетевых мин. Реальный `Proj*` не удерживался и не слушался, а session удалял ID
вручную сразу при `Death`, до исходной границы уничтожения объекта. Это
разрывало Windows lifetime и позволяло RPC lookup расходиться с concrete
снарядом.

`Player` теперь хранит пару `Proj*/id`, подписывается при успешном mine shot,
отклоняет уже мёртвый объект в обоих lookup и окончательно удаляет запись из
`OnDestroy`. Ранняя session-cleanup удалена. Одновременно восстановлен важный
порядок деструкторов: `Proj`, `RockCar` и `GameCar` вызывают idempotent
`DestroyObject` ещё с действующим derived type, как их Windows `Destroy()`;
иначе callback из базового деструктора уже не видел `IsProj()`/`IsCar()`.

Выявленный этим переносом crash при завершении также исправлен исходным явным
`Player::~Player`: bonus listeners, headlights, car listener и color material
освобождаются до начала разрушения членов класса. Regression проверяет
pointer/id lookup, немедленное исключение death-state и удаление записи после
deferred object cleanup.

### P2.210 — `MapObj` больше не уничтожает context до `GameObject::Destroy` — выполнено

Сверка `MapObj::~MapObj` и `MapObj::CreateGameObj` выявила ещё одно concrete
lifecycle-расхождение. Windows вызывает `delete _gameObj`, пока старый объект
всё ещё содержит `_mapObj`, `_logic` и `_parent`; его `Destroy()` уведомляет
listeners именно с этим контекстом. Порт сначала вручную выполнял
`SetParent(nullptr)`, `SetLogic(nullptr)` и `SetMapObj(nullptr)`, и только
затем разрушал объект. Поэтому destruction effects, Player/network listeners
и attribution получали уже обезличенный sender.

Owned `GameObject` теперь разрушается в исходном порядке и только затем
публикуется concrete replacement. Это применено к обычному деструктору,
смене `GameObjType` и временному record instance при `BindGameObj`. Отдельная
portable stable-address ветвь для внешнего `Player::gameCar` остаётся
не-владеющей и лишь безопасно отцепляет объект.

Regression фиксирует, что callback при type replacement ещё видит старые
`MapObj`, `Logic`, parent и обратную ссылку `MapObj::GetGameObj`; прямое
удаление projectile MapObj дополнительно проверяет действующий `IsProj()`.

### P2.211 — `MineRipUpdate` nested spawn plan перенесён в concrete `Proj` — выполнено

После placed-mine dispatch в session оставался крупный кусок исходного
`Proj::MineRipUpdate`: ручная реконструкция независимых `model2/model3`
descriptor, sampling их lifetime, создание одного core и пяти fragments, а
также копия дискретного `Vec3Range((-3,-3,3),(3,3,1),vdVolume)` и импульса.
Это означало, что типы, damage, death effect и RNG order дочерних снарядов
по-прежнему определял adapter.

Новый `Proj::BuildMineRipSplitPlan` читает concrete parent descriptor и
возвращает полный source child batch. Он владеет независимыми child
`ProjectileDefinition`, исходным порядком RNG (core lifetime, затем для
каждого fragment lifetime/direction), 100×100×100 grid distribution,
нормализацией `dir * 10`, visual variant/arming scale и parent-death command.
`OriginalRaceSession` теперь только материализует эти записи как автономные
Jolt/runtime mine views и применяет уже подготовленную скорость.

Regression проверяет один core и пять fragments, типы 11/13, отдельные
visual records, damage/lifetime, ровно 11 RNG выборок, модуль fragment
velocity 10 и положительный Z. Существующий integration regression продолжает
проверять полную split/death-effect гонку.

### P2.212 — Mortira `deathProjectile`/Crater spawn plan перенесён в `Proj` — выполнено

Второй nested-object surrogate находился в общей session-функции смерти
снаряда. Adapter сам проверял `deathProjectile`, выбирал запись из weapon
catalog, допускал только type 20, вычислял position offset/lifetime и
переносил `effectPxIgnoreSenderCar` в создаваемый crater.

`Proj::BuildDeathProjectileSpawnPlan` теперь владеет всем этим source
решением. Он не потребляет RNG при отсутствующем DeathEffect или неверной
записи, проверяет concrete `ptCrater`, возвращает исходный projectile index,
descriptor, local position, sampled lifetime и owner ignore-pair. Session
лишь складывает offset с Jolt world contact и материализует готовый plan.

Regression закрепляет valid Mortira→Crater путь, lifetime sampling, offset и
ignoreSenderCar, а также два отрицательных gate: отсутствующий effect и
неверный child type. Полный mortar integration по-прежнему проверяет crater
continuous damage и owner filtering.

### P2.213 — `GameObject` event registration и Jolt fixed-step bridge восстановлены — выполнено

Сверка `GameObject::Reg/UnregFrameEvent`, `Reg/UnregProgressEvent`,
`Reg/UnregLateProgressEvent`, `Reg/UnregFixedStepEvent`, `SetLogic`,
`SetSyncFrameEvent`, `SetBodyProgressEvent` и конструктора/деструктора
`GameCar` подтвердила, что portable object graph полностью утратил четыре
source-счётчика. `GameCar` вызывался из Jolt напрямую независимо от
регистрации, а awake/sleep и network correction не управляли списками
frame/late callbacks.

Четыре счётчика и точный `SetLogic` unregister/re-register order возвращены.
Как и в Windows, progress counter сохраняется, но его World-вызовы остаются
закомментированной source-ветвью. `GameCar` регистрирует fixed-step в
конструкторе и снимает его до derived destruction. Jolt callback теперь
делает адресный `WorldEventPump::DispatchFixedStepEvent` для одной машины:
общий `World::FixedStep` внутри per-vehicle callback не вызывается и N×N либо
двойного шага физики не возникает.

После Jolt solver state `SynchronizePhysicsState` включает/выключает исходную
пару late+frame по awake/sleep, а network correction удерживает второй frame
reference. `DispatchPxSync` вызывает только зарегистрированный source frame;
спящее тело использует последний готовый graph state. Regression проверяет
reference counts, перенос одного объекта между двумя Logic/World, отсутствие
portable progress registration, единственный torque/gear dispatch, awake,
sleep и correction gate. Полные physics и 360-frame Metal smokes подтвердили
шесть движущихся машин без повторного source step.

### P2.214 — `Race::OnLateProgress` перенесён за завершённый Jolt step — выполнено

Сверка Windows `Race::OnLateProgress` с активным main loop обнаружила
ошибочную временную границу. `OriginalRaceSession::updatePlaces` выполнялся
до `physicsWorld->step`, поэтому сортировка `_playerPlaceList`, place в HUD и
события LeadChanged/ThirdChanged/LastFar/Domination/ThirdFar использовали
положение предыдущего solver frame. Это было особенно заметно как скачок
точек/мест при быстром изменении траектории.

`RacePlaceModel` теперь реализует `LateProgressEvent`, хранит prepared roster
и вычисляет результат только из World late-progress. Active macOS loop
сначала завершает Jolt step, синхронизирует все vehicle states и только затем
вызывает `OriginalRaceSession::lateProgress`. Один pending pass создаётся во
время countdown, racing и finish-wait; early exit делает final synchronous
pass до разрушения Player graph.

Session-only regressions, где нет отдельного physics backend, используют тот
же event path немедленно в конце `update`, а не отдельную формулу. Unit test
проверяет deferred boundary и одноразовый take результата. Полные physics и
360-frame bgfx/Metal smokes сохранили шесть машин, колёсные контакты и AI
progress.

### P2.215 — `Race::OnFixedStep` перенесён на source fixed clock — выполнено

Прямая сверка `World::OnFrame` и `Race::OnFixedStep` подтвердила зависимость
порта от render FPS. Windows при каждом `maxTimeStep` сначала вызывает
`FixedStep`, где Race последовательно выполняет всех `Player::OnProgress` и
затем `AISystem::OnProgress`, и только после этого запускает
`_pxScene->Compute`. Порт выполнял этот блок один раз в
`OriginalRaceSession::update`, после чего Jolt мог сделать несколько шагов
по 1/120 секунды с одним устаревшим AI решением.

В backend-neutral physics API добавлен единый `WorldFixedStepController`.
Jolt обновляет snapshot завершённого предыдущего интервала, вызывает Race
один раз на весь roster, применяет итоговые inputs/reset команды и затем
вызывает отдельный зарегистрированный `GameCar` callback для каждой машины.
Это сохраняет исходный порядок и не создаёт N×N dispatch.
`Player::ResetCar`, возникший внутри callback, применяется до solver step.

Поскольку main уже обработал события Progress перед входом в physics,
события checkpoint/lap/finish/respawn из fixed-step хранятся в отдельной
очереди до следующего adapter pass; обработанные frame events при этом не
повторяются. Physics regression требует один source callback и reset на один
интервал 1/60. Прошли 29/29 CTest, полный map1 physics smoke,
360-frame Metal race smoke и обычный запуск `.app` с видимым главным меню.

### P2.216 — source 60 Hz отделён от Jolt 120 Hz — выполнено

Дополнительная сверка константы обнаружила принципиальную деталь:
`World::cMaxSimStep` в оригинале равен `1/60.0f`, а 1/120 является только
внутренним шагом replacement backend. Первичная интеграция B8m ошибочно
связала source callbacks с каждым Jolt substep и тем самым удвоила бы
Player/AI timers, RNG, `SoundMotor::OnMotor` и GameCar steering/stabilize.

Jolt теперь хранит отдельный persistent аккумулятор 60 Гц. Race и все
зарегистрированные GameCar вызываются только при полном source interval;
готовая torque/brake/gear/grip команда кэшируется и применяется на двух
solver substeps. Direct steering yaw и momentum stabilization выполняются
один раз, силы/torque интегрируются backend с delta 1/120. На дисплее 120 Гц
первый frame не продвигает source clock, второй даёт ровно один callback.
Regression проверяет также единственный reset. 720-frame Metal smoke дал
скорости игрока 39.36 и AI 37–42 без накопительной деградации.

### P2.217 — AI fixed-step actions больше не теряются — выполнено

После переноса `Race::OnFixedStep` обнаружился отдельный adapter-дефект:
`progressAi` сохранял только последний `AICar::ProgressResult` каждого
гонщика. Если один render frame содержал несколько source-интервалов 1/60,
ранний Shot/Hyper/Mine стирался более поздним решением. Кроме того,
`AICar::TakeResetCar` читался только в последующем `updateGameplay`, поэтому
off-trace reset не попадал в текущий Jolt reset batch.

Теперь все attack-решения сохраняются в порядке вызовов source fixed-step и
исполняются последовательно с повторной проверкой live/finished/death и
готовности concrete Weapon. Reset edge потребляется сразу после
`AISystem::OnProgress` и материализуется до того же solver update. Очередь
очищается при disconnect, finish, reset и ExitRace. Regression выполняет два
fixed-step без промежуточного render pass: первый создаёт торпедный выстрел,
второй теряет цель; выстрел не теряется. Отдельный тест требует AI reset в
том же переданном Jolt batch.

### P2.218 — `Logic::Shot` ИИ возвращён в исходный fixed-step — выполнено

Прямая сверка цепочки Windows `AICar::AttackState::Update -> Logic::Shot ->
Player::Shot -> WeaponItem::Shot -> Weapon::CreateShot` показала, что
предыдущий bridge сохранял только команду атаки. Конкретный `Proj`, списание
заряда и `Weapon::OnShot` выполнялись позже в render `updateGameplay`. При
двух source-интервалах 1/60 в одном кадре второй AI step поэтому видел
устаревшие charge/cooldown и мог принять лишнее решение о выстреле.

Теперь обычное оружие ИИ подготавливает полный source `ShotContext`, создаёт
принадлежащие `Logic` concrete `Proj`, списывает charge и сбрасывает cooldown
непосредственно внутри того же `Race::OnFixedStep`. Упорядоченная очередь
хранит только временный adapter-view этих объектов и на следующем frame без
повторного `Shot` создаёт Jolt/bgfx runtime-представление. Stale/finish/
disconnect ветви вызывают `Proj::Death` до разрушения владельца оружия.

Regression требует, чтобы первый fixed-step списал ровно один заряд и
обнулил shot time, второй step без render pass не списал заряд повторно, а
последующая backend materialization только продвинула исходный cooldown.
Прошли 29/29 CTest, полный map1 physics smoke и 360-frame bgfx/Metal smoke.

### P2.219 — Hyper и Mine ИИ возвращены в исходный fixed-step — выполнено

Сверка `AICar::AttackState::RunHyper` и `PlaceMine` показала, что обе функции
в Windows вызывают `Logic::Shot` непосредственно между обычной атакой и
`ControlState::Update`. Portable очередь сохраняла флаги `useHyper/useMine`,
но вызывала `Player::Shot` только в следующем render `updateGameplay`.

Теперь fixed-step вычисляет source transform установленного `stHyper` или
`stMine`, выполняет исходную projectile preparation, lifetime RNG и
`WeaponItem::Shot`. Hyper сразу изменяет charge/cooldown и сохраняет
конкретный `Proj`, рассчитанные source duration и local impulse. Mine в том
же шаге выполняет track placement, регистрирует bonus-projectile id, ставит
`GameCar::LockMine` и передаёт concrete mine `Proj` во владение `Logic`.

Frame adapter материализует только Jolt velocity/runtime view, события и
эффекты, не вызывая `Shot` повторно. Regression проводит AI последовательно
по исходному WayPath: на подходящем участке Hyper и Mine обязаны списаться до
adapter pass, второй fixed-step не может списать их повторно, после чего
backend получает ровно по одному runtime object. Прошли arm64 build, 29/29
CTest, physics smoke и 360-frame bgfx/Metal smoke.

### P2.220 — backend-команды Hyper и Mine применяются до того же solver step — выполнено

После переноса source-транзакций оставалась ещё одна временная граница:
готовые `Proj`, charge и cooldown уже появлялись внутри
`Race::OnFixedStep`, но `ProjectileRuntime`/`MineRuntime` и импульс Hyper
создавались следующим render adapter pass. Поэтому Jolt видел специальную
атаку на один кадр позже оригинального PhysX `Compute`.

Hyper и Mine теперь создают единственное backend-neutral runtime-view сразу
после успешного `WeaponItem::Shot` в том же source fixed-step. Расширенный
`WorldFixedStepController` возвращает адресные linear-velocity commands;
Jolt применяет их после reset-команд и до текущего `system_.Update`.
Spring lock также переносится в итоговый input этого интервала. Поздний
adapter оставлен владельцем только событий, звуковых/визуальных эффектов и
не повторяет runtime или импульс.

Regression требует один Hyper runtime и одну velocity-команду до adapter,
один Mine runtime до adapter, отсутствие повторной команды после adapter и
неизменный charge на втором fixed-step. Physics smoke дополнительно проверяет,
что переданная callback-ом скорость уже видна после того же Jolt solve.

### P2.221 — runtime обычного AI-снаряда создаётся в firing fixed-step — выполнено

После B8r обычная ветка `AICar::AttackState::ShotByEnemy` оставалась
асимметричной специальным атакам: concrete source `Proj` уже создавался в
`Race::OnFixedStep`, но автономный `ProjectileRuntime` появлялся только при
последующем render adapter pass. Это откладывало backend-тело торпеды и
прочих обычных снарядов относительно исходного `Weapon::CreateShot`.

Общий builder теперь формирует runtime из фактически подготовленного `Proj`:
использует его world transform, `weaponListIndex`, preparation route,
relative launch velocity, ballistic/homing/attached признаки и выбранную AI
цель. Fixed-step сразу регистрирует все persistent projectile views. Adapter
находит их по concrete `Proj*`, не вызывает launch preparation второй раз и
оставляет за собой только ray resolution, событие и presentation effects.

Для встроенного session fallback сохранён исходный порядок
`World::Progress -> Race::OnFixedStep -> physics`: новый projectile не
получает лишний `Proj::OnProgress` внутри того же кадра. Regression требует
один runtime до adapter, один после него и прежний 3D target. Прошли 29/29
CTest, physics smoke и 360-frame bgfx/Metal smoke.

### P2.222 — свободные `Proj` получили Jolt actor lifecycle — выполнено

В оригинале после `Weapon::CreateShot` свободный `Proj` существует как
динамический PhysX actor: начальная скорость задаётся до `World::Compute`, а
завершённая поза читается игровым объектом после solve. Portable session до
B8t вручную прибавляла `velocity * renderDelta`, хотя сами машина и мир уже
вычислялись Jolt; это оставляло движение снарядов зависимым от FPS и не
сохраняло исходную границу физического шага.

Теперь каждый persistent non-ray/non-attached projectile получает стабильный
body id и dynamic Jolt sensor box из исходных `ProjDesc::collision`, `mass` и
gravity route. Команда Create, возникшая в `Race::OnFixedStep`, применяется
до текущего solver update; после solve pose и linear velocity возвращаются в
тот же `ProjectileRuntime`. Source `Proj::OnProgress`/`ProgressFree` остаются
владельцами lifetime, homing, Thunder и rocket-height правил и могут
синхронизировать изменённое состояние в тело перед следующим solve.

Ручная интеграция сохранена только для headless regression без подключённого
physics backend. Тесты требуют одно создание тела, обратную синхронизацию
Jolt-позы и отсутствие повторного Create. Physics smoke отдельно создаёт
sensor projectile из world fixed callback и проверяет его перемещение уже в
том же 60-Hz интервале. Оставшаяся граница B8u — заменить session snapshot
box/ray contacts событиями Jolt contact/raycast query.

### P2.223 — `Proj::OnContact` получает Jolt sensor manifold — выполнено

Contact listener раньше отбрасывал сторону projectile даже после появления
его Jolt actor: он заполнял только `VehicleState::bodyContacts`, а session
повторно искала цель пересечением render-snapshot OBB. На высокой скорости
это теряло промежуточное столкновение, а при рассинхронизации кадра могло
повредить объект, которого физический actor не касался.

Listener теперь распознаёт projectile user-data и сохраняет для каждого тела
тип поверхности, точный vehicle/decor instance, actor id, normal, relative
normal speed и точки manifold. Готовый contact stream возвращается вместе с
pose/velocity после solve. `OriginalRaceSession` использует его для
`ContactDynamic`, owner re-arm, Sonar, destructible decoration и Thunder
reflection; прежний OBB путь вызывается только без external physics.

Projectile sensor больше не попадает машине как ложный `TrackPlane`
`BodyContact`. Physics regression создаёт второй projectile внутри машины и
требует `Vehicle/otherVehicle=0` с реальной точкой контакта. Session
regression проверяет полный round-trip contact identity. Оставшаяся B8v
граница — ray-only Laser/FrostRay и rocket-height ground query, которые пока
используют CPU-копию геометрии вместо Jolt narrow-phase query.

### P2.224 — projectile-group raycasts перенесены в Jolt narrow phase — выполнено

Оригинальные `LaserUpdate`/`FrostRayUpdate`, `MinePrepare` и
`Proj::RocketUpdate` вызывают PhysX scene query по текущим actors. Portable
ветка продолжала трассировать загруженные треугольники и snapshot OBB машин,
поэтому не видела завершённый solver pose и могла пересечь уже удалённый или
динамически перемещённый actor.

`OriginalVehicleWorld` теперь предоставляет closest raycast с фильтрами
полной projectile group или только TrackPlane. Jolt collector читает живые
vehicle/decoration/surface bodies, исключает actor стрелка и projectile
sensors, сохраняет actor identity, hit point и world normal, принимает обе
стороны исходных mesh triangles. Удалённые decoration bodies автоматически
исчезают из query вместе с physics actor.

Session получает этот query через backend callback. Laser/FrostRay,
обычный/AI MinePrepare, падение отделившейся мины и rocket-height используют
его в активной гонке; прежние CPU функции остались только deterministic
fallback и эталоном regression без world. Physics smoke проверяет closest
vehicle hit, ignored shooter и отдельный TrackPlane hit, session smoke — что
установка MineRip действительно прошла через backend callback.

### P2.225 — размещённые Mine/MineRip возвращены в physics actor graph — выполнено

Windows хранит поставленную мину и каждый отделившийся MineRip fragment как
самостоятельный `Proj` с PhysX actor. Portable runtime после переноса source
объектов всё ещё вручную интегрировал fragment velocity/gravity и определял
контакт машины OBB-пересечением render snapshots.

`MineRuntime` теперь использует тот же stable-id Create/Synchronize/Destroy
bridge, что свободные projectiles. Обычная и AI Mine создают Jolt sensor body
из concrete `ProjDesc` collision/mass в момент source Shot; AI-команда
попадает до того же solve. Autonomous crater и каждый MineRip child получают
собственный id, source descriptor, velocity и gravity, а копирование parent
runtime явно сбрасывает backend identity.

После solve session принимает pose/velocity/manifold, выполняет исходные
arming/Maslo/Crater/MineContact и network RPC branches по точному vehicle id и
contact point. Ground clamp останавливает fragment и одновременно отключает
его gravity factor. Death, lifetime, split и stale Logic owner выдают Destroy
до удаления runtime. Headless smoke сохраняет ручную интеграцию. Regression
требует один same-step Jolt Create для AI Mine и отсутствие повторного actor.

### P2.226 — map-owned `AutoProj` получили статические Jolt actors — выполнено

Serialized Bonus MapObjs (`Money`, `Medpack`, `Charge`, `Immortal`,
`SpeedArrow`, `Lusha`, `Maslo`, `Mine`) в Windows создают projectile-group
PhysX shapes ещё при загрузке карты. Порт хранил их source `AutoProj`, но
контакты всех машин вычислял отдельным OBB loop по неизменному transform.

При включении external physics session теперь bootstrap-ит ровно один static
Jolt sensor на каждый живой Bonus MapObj, используя concrete AutoProj
collision и map transform. Static motion type исключает gravity/solver drift
и не поддерживает bodies активными без причины. Completed manifold
сохраняется по stable body id и используется всеми исходными contact branches:
SpeedArrow, Lusha, Maslo, map Mine и обычный TakeBonus.

Source death/pickup и map-mine destruction выдают Destroy, очищают contact
cache и исключают actor из следующих ray/contact queries. Reset заново
создаёт body roster, а initial bind отделяет bootstrap commands от последующей
AI attack transaction. Regression требует один Create для единственного oil
AutoProj и проверяет полный bootstrap count в fixed-step fixtures.

### P2.227 — attached Fire/Drobilka actors перенесены в Jolt — выполнено

Оригинальные `Proj::FireUpdate` и `Proj::DrobilkaUpdate` каждый source tick
переставляют отдельный PhysX actor в transform установленного оружия и
копируют линейную скорость машины. Portable session переносил сами handlers,
но продолжал искать машины и destructible decorations реконструированным OBB
по render snapshot; реального moving contact actor в backend не было.

Persistent attached projectile теперь создаёт kinematic Jolt sensor с
collision center/half-extents конкретного `ProjDesc`. `ProgressAttached`
выдаёт его pose и velocity до очередного solve, а завершённый manifold
возвращает точный vehicle/decor id и point в `FireContact`/`DrobilkaContact`.
Headless source regression сохраняет OBB fallback только при выключенном
external physics.

Когда `Weapon::OnDestroy` очищает ссылку, actor не исчезает: bridge сначала
удаляет kinematic body и в том же source update создаёт dynamic body с
последней скоростью и gravity, повторяя исходное продолжение жизни `Proj`.
Jolt physics smoke теперь использует именно kinematic projectile для
обязательного vehicle manifold. Оставшаяся B8z граница — death-plane и
ResetCar scene queries, которые ещё вычисляются session snapshot helper-ами.

### P2.228 — `cdgPlaneDeath` и `Player::ResetCar` scene queries перенесены — выполнено

`Map::Map` в Windows всегда создаёт +Z `NxPlaneShape` на Z=0 в отдельной
группе `cdgPlaneDeath`. Порт проверял нижнюю точку реконструированного OBB
машины после gameplay update, а `Player::ResetCar` искал ближайший объект
CPU-raycast по копии mesh/vehicle transforms. Оба пути обходили живую physics
scene и могли расходиться с завершённым solver pose или уже удалённым actor.

Jolt world теперь содержит отдельный static PlaneShape sensor с identity
`DeathPlane`. Его manifold проходит через обычный vehicle contact stream в
исходный `Map::GetGround().TouchDeath`, при этом исключён из generic
PairPxContactEffect, чтобы смерть не создавала зацикленный звук трения.
Обычные projectile rays фильтруют plane; только ResetCar query включает его,
повторяя Windows mask `TrackPlane | PlaneDeath | Default`.

Source `Player::ResetCar` по-прежнему выбирает tile, три offsets и до пяти
предыдущих узлов. Заменён только callback: closest Jolt hit различает
TrackPlane, собственную машину, blocked vehicle/decoration и DeathPlane.
CPU mesh/OBB/plane helper оставлен исключительно для source-only regression
без backend. Physics smoke проверяет ray filter и реальный car↔plane sensor
contact; session smoke требует три backend reset rays с death-plane mask.

### P2.229 — исходный `Behavior::PxNotify` восстановлен — выполнено

Concrete behavior classes были перенесены, но их базовый owner потерял
`PxNotifies`. Из-за этого Jolt death-plane contact попадал в общий listener
dispatch прямым вызовом session и не зависел от того, зарегистрирован ли у
ground живой `TouchDeath`, как это делал PhysX actor source-версии.

Portable `Behavior` теперь хранит backend-neutral `Contact` и
`ContactModify` subscriptions; `Behaviors` агрегирует их для своего
`GameObject`. `TouchDeath` включает `Contact` при создании, а удаление
behavior автоматически снимает owner requirement. Session проверяет эту
регистрацию перед dispatch завершённого death-plane manifold. Smoke-тесты
покрывают установку, удаление, map ground и неактивный modify flag.

### P2.230 — car `DeathEffect` возвращён в `GameObject` graph — выполнено

Windows `DataBase::LoadCar` добавляет после `LowLifePoints` две отдельные
записи `DeathEffect`: `death2` и fragment actor автомобиля. Порт сохранял их
definitions, но `OriginalRaceSession::destroyRacer` создавал оба эффекта
безусловно; у самого `RockCar` type-6 listeners отсутствовали.

Теперь `Player::CreateCar` материализует все serialized vehicle death
behaviors в исходном порядке и пересоздаёт их при каждом respawn. Наличие
`Logic` проверяется самим owning behavior в `OnDeath`, поэтому прямой
`Logic::Damage(GameObject&)`, death-plane и Player wrappers сходятся в одном
пути. Session потребляет ordered spawn-plans и больше не решает, должен ли
эффект существовать. Регрессии фиксируют duplicate behavior entries,
target-child flag, два shipped death visuals/sounds и сброс one-live state
после восстановления машины.

### P2.231 — bonus/map mine `DeathEffect` возвращён owning `AutoProj` — выполнено

Подтвердился ещё один synthetic dispatch: records из `DataBase::LoadBonus`
корректно становились `AutoProj`, а portable `DataBase::Configure` уже
передавал type/model/collision/value и serialized model `DeathEffect`. Но
`OriginalRaceSession` не потреблял его concrete type-6 listener и создавал
impact visual/sound напрямую из definition.

Для mine hazard source `Proj::MineContact` теперь приводит к
`AutoProj::DestroyWithEffect(target, Mine)`. Для обычного pickup source
`Player::TakeBonus(GameObject&)` по-прежнему первым вызывает `bonus.Death()`;
до него session только устанавливает context listener, а после получает
one-live spawn result. Renderer, SDL audio и Jolt получают уже решённый
source plan. Регрессии проверяют behavior на `AutoProj`, mine damage/removal,
pickup reward, visual и отложенный `LifeEffect` sound.

### P2.232 — global `LogicBehaviors` graph восстановлен — выполнено

Windows `Logic` не владеет `PairPxContactEffect` прямым полем. Он создаёт
`LogicBehaviors`, а concrete `LogicBehavior` регистрирует себя в World и
получает Map/Logic через owner. PhysX `PxSceneUser::OnContact` также сначала
dispatch-ит container, а не вызывает pair effect напрямую.

Portable runtime теперь повторяет этот graph: отдельные `LogicBehaviorType`,
`LogicBehavior`, `LogicBehaviors` и concrete pair instance имеют стабильные
owner links; attach/detach World использует source registration methods.
Session преобразует Jolt manifold и вызывает только `Logic::OnContact`.
Unit regression закрепляет единственный shipped catalog entry, его type,
owner/Logic pointers и ordered contact progress/release.

### P2.233 — `LogicEventEffect` и identity `spark2` восстановлены — выполнено

В original graph `PairPxContactEffect` наследует `LogicEventEffect`, который
владеет созданными MapObj effects и удаляет точный объект по его destroy
listener. Порт хранил в contact только `bool` и заставлял session искать
живой `RaceEffect` по actor-pair/slot, что особенно опасно при перекрытии
старого fading и нового active поколения.

Portable `LogicEventEffect` теперь сохраняет effect record/position и выдаёт
stable handle каждому create. Contact update, release и окончательный
renderer destroy проходят с одним handle; `NotifyEffectDestroyed` очищает и
owner list, и оставшуюся contact reference как source `OnDestroyEffect`.
`DataBase::Init` provenance `ctEffects/spark2` передаётся owner-у вместе с
пятью sound variants. Unit и resource regressions проходят полный lifecycle.

### P2.234 — `DataBase::Init` снова создаёт global behavior — выполнено

Обратная проверка `eff9338:prog/Rock3dGame/source/game/DataBase.cpp:4347-4357`
подтвердила ошибку ownership: portable `LogicBehaviors` заранее создавал
pair effect, а session конфигурировал его числом звуков. Оригинал делает это
только после `InitMapObjLib`: добавляет behavior, привязывает record
`ctEffects/spark2` и пять конкретных объектов SoundLib.

Portable `DataBase::Configure(Race, Logic)` теперь выполняет ту же
транзакцию. Behavior хранит канонические sound paths и сам выбирает запись;
SDL adapter получает выбранный путь из owner. Повторная конфигурация
переиспользует один behavior и очищает его live contacts. В этот же typed
record graph возвращены все извлечённые active effect/projectile records и
полный `Race::weapons` catalog как concrete `AutoProj`/`Weapon` factories.
Unit regression требует нулевой catalog до DataBase, один type после него и
проверяет фактическую загрузку projectile/weapon descriptors.

### P2.235 — source `ShotEffect` владеет spawn transaction — выполнено

Windows `GameBase.cpp:827-847` показывает, что type-10 behavior на каждый
`Behaviors::OnShot(pos)` сам создаёт child effect и запускает выбранный
`Source3d`. В портированном runtime `ShotEffectBehavior` лишь увеличивал
counter, а большой `OriginalRaceSession::pushShotEffect` повторно читал
record data и исполнял всю логику вручную.

Перенесён полный owner state: effect definition, sound catalog, base
position, local impulse, `ignoreRot` и ordered pending spawns. Callback
возникает только после успешного `PrepareProj`, как в source, поэтому rejected
и dry shots не создают payload. `WeaponItem` сохраняет record behavior рядом
с descriptor и устанавливает его на live mount при `CreateCar`; DataBase
делает то же для прямого record proxy. Adapter теперь потребляет один plan на
один prepared projectile. Regression подтверждает multi-projectile order и
то, что копия Weapon не теряет serialized behavior state.

### P2.236 — car event-effect records возвращены behavior owners — выполнено

Сверка `DataBase::LoadCar` (`GameBase.cpp::LowLifePoints`, `DamageEffect`,
`ImmortalEffect`) подтвердила ещё один смешанный путь. Состояния behavior уже
были перенесены, но `smoke6` рисовался непосредственно по флагу Player,
energy-hit после owner boolean заново выбирался из `Race::Vehicle`, а shield
record и `scaleK` оставались только данными renderer-а. Тем самым source
`EventEffect::_effect/_pos` не владели созданным child actor.

`Player::SetCar` теперь связывает все три concrete behavior с точными
`ObjectDefinition` выбранного автомобиля. `LowLifePoints` создаёт один
постоянный child-effect на первом переходе ниже 35%, прогрессирует его
отдельный возраст и немедленно удаляет через `FreeEffect(false)` при лечении.
Energy `DamageEffect` выдаёт owner spawn-plan только для `dtEnergy` и только
пока прежний 0.5-секундный actor не жив. `ImmortalEffect` хранит исходные
`shield1` и `(1.3, 1.7, 1.7)`; bgfx берёт record и коэффициент у behavior.
Session больше не выбирает эти записи повторно. Regression проверяет identity,
child attachment, lifetime, release и отсутствие повторного создания.

### P2.237 — Frost `SlowEffect` снова владеет model3 child — выполнено

Сверка `Weapon.cpp::Proj::FrostRayUpdate` и
`GameBase.cpp::SlowEffect::OnProgress/OnDestroyEffect` выявила оставшийся
двойной путь: dynamic behavior ограничивал скорость и время жизни, однако
bgfx самостоятельно выбирал `projectiles[weapon][projectile].tertiaryVisual`
по сохранённым индексам. В Windows именно
`SlowEffect::SetEffect(_desc.GetModel3())` сохраняет точную запись и создаёт
car-child actor.

Portable `SlowEffect` теперь хранит точный `ObjectDefinition*` и выдаёт
одноразовый `EventEffect` spawn-plan. `Proj::AttachFrostSlow` передаёт owner-у
стабильную запись model3, а session только материализует `VehicleSlowEffect`
с parent-racer и source lifetime. Renderer больше не смотрит на состояние
`Player::slowEffect` и не выбирает record повторно. Regression проверяет
identity, one-shot consumption, child attachment, отсутствие refresh при
повторном луче и совместное завершение behavior/model.

### P2.238 — wheel `PxWheelSlipEffect` records/lifecycle восстановлены — выполнено

Прямая сверка `DataBase::LoadCar` и `GameBase.cpp::PxWheelSlipEffect`
подтвердила, что portable graph схлопывал до одного boolean/state две
отдельные type-9 записи каждого колеса: `trail` с local Z=0.01 и `smoke7`.
Кроме того, SDL создавал SkidAsphalt loop для любого активного колеса, хотя
Windows назначает этот Source3d только serialized trail behavior первого
колеса и некоторые машины (например `devildriver`) вообще имеют лишь
беззвучный smoke behavior.

Parser теперь сохраняет ordered `WheelSlipEffectDefinition` прямо из
`db.xml`; каждый `CarWheel` создаёт столько concrete behaviors/listeners,
сколько записано в source. Каждый owner отдельно хранит EventEffect record,
position, impulse, ignoreRot, sound catalog и Make/Free state. Renderer
различает trail/smoke по owner record, применяет исходный +0.01 world-Z и
оставляет fading smoke в последней точке контакта. Audio выделяет voice
только behavior с реальным sound reference. Regression проверяет два owner,
identity, порядок, звук только trail, независимый release и реальные
одно-/двух-behavior варианты всего car catalog.

### P2.239 — `EventEffect/LifeEffect` sound catalog возвращён owner-у — выполнено

Сверка `EventEffect::AddSound/GiveSource3d` и `LifeEffect::OnProgress`
подтвердила ещё одно раздвоение состояния: portable `RaceEffect` хранил
`lifeSoundPaths` рядом с concrete type-7 behavior и сам выбирал случайный
звук. В исходнике каталог `_sounds`, ленивый выбор Source3d и one-shot
`_play` принадлежат одному behavior graph.

`EventEffect` теперь владеет serialized sound catalog и исходным
равномерным выбором. `LifeEffectBehavior` хранит каталог, выбранный sound
reference и одноразовый Play request; session только сообщает backend
availability/position/lifetime и переводит request в SDL. Параллельный
`RaceEffect::lifeSoundPaths` удалён. Тем же общим владельцем теперь
пользуются `ShotEffect` и `PxWheelSlipEffect`, а копирование колеса
перепривязывает sound reference к каталогу новой копии. В `db.xml`
подтверждены все 6 type-7 записей: каждая имеет ровно один sound.

### P2.240 — `EventEffect::GameObjEvent::OnDestroy` возвращён — выполнено

Portable `EventEffect::_makeEffect` не получал обратного уведомления, когда
его материализованный `RaceEffect::effectOwner` умирал или принудительно
удалялся при reset/finish/respawn/disconnect. Это расходилось с Windows
`GameObjEvent::OnDestroy`, который сначала вызывает virtual
`OnDestroyEffect`, затем удаляет MapObj из `_effObjList` и очищает
`_makeEffect`.

Каждый one-live spawn-plan теперь несёт точную ссылку на создавший его
`EventEffect`. `RaceEffect` сохраняет её, а единый
`notifyEffectDestroyed` доставляет callback для natural lifetime и всех
явных erase/clear paths. Callback выполняется до перестройки Player/Logic
owners. Для dynamic Frost отдельно восстановлена ветка
`SlowEffect::OnDestroyEffect`: состояние сбрасывается, concrete behavior
помечается на deferred removal. Regression проверяет повторное создание
DamageEffect после callback, SlowEffect removal и live owner identity в
полном session smoke.

### P2.241 — `EventEffect::_effObjList` и transient `ShotEffect` identities возвращены — выполнено

Windows `EventEffect::CreateEffect` добавляет каждый созданный `MapObj` в
`_effObjList`, даже если он не является единственным `_makeEffect`.
Portable `ShotEffect` до этого выдавал несколько backend visuals, но base
owner не знал ни одного из них: уничтожение нельзя было сопоставить с
конкретным выстрелом, а копирование pending spawn переносило указатели на
чужой behavior.

`EventEffect` теперь выдаёт монотонный `EffectId`, хранит полный список
живых identities и отдельно отмечает distinguished make-effect. Каждый
visual `ShotEffect::OnShot` регистрирует собственный handle; `RaceEffect`
возвращает владельцу именно этот handle при natural или forced teardown.
Тот же объект передаёт renderer точную live visual-definition, поэтому
повторный выбор из глобального `Race::weapons` удалён.
Копия `Weapon` сохраняет serialized ShotEffect и shot counter, но начинает
с пустыми live/pending identities. Regression проверяет два одновременных
выстрела, независимое удаление, copy boundary и равенство source-list с
живыми backend objects в полном session smoke.

### P2.242 — `DeathEffect` owner identity и lifetime boundary возвращены — выполнено

После восстановления общего `_effObjList` выяснилось, что type-6
`DeathEffect` всё ещё терял identity при передаче в backend. Особенно опасен
был portable порядок: погибший `Proj` удалялся из `Logic` до окончания
созданного им взрыва, а будущий callback неизбежно ссылался бы на уже
уничтоженный behavior. У автомобиля, наоборот, behavior graph заменяется
при `CreateCar(false)` во время respawn.

`DeathEffect::SpawnResult` теперь несёт exact owner/handle. Vehicle,
projectile, mine и bonus death visuals сохраняют эту пару вместе с точной
live visual-definition. `Logic::ProgressGameObjs` удерживает погибший
transient `Proj`, пока его `DeathEffect` имеет live handles, и освобождает
после callback. При car respawn старые callback identities отсоединяются в
тот же переход, где Windows destructor снимает listener; detached particle
visual может закончить жизнь без ссылки на уничтоженный behavior. Renderer
использует переданную owner-definition вместо повторного descriptor lookup.
Regression покрывает type-6 car graph, targetChild projectile, mine/mortar,
owner retention/release и отсутствие stale owner после respawn.

### P2.243 — полная source-конфигурация `DeathEffect` возвращена owner-у — выполнено

Прямая сверка `EventEffect::LoadSource/CreateEffect` и
`DeathEffect::OnDeath` выявила оставшееся расхождение: type-6 behavior уже
выдавал точный owner/handle, но visual record, local position/impulse и
`ignoreRot` сессия повторно брала из `Race` descriptor. В Windows все эти
поля загружаются в унаследованный `EventEffect` и создаваемый `MapObj`
получает их от behavior-а.

`DeathEffect` и concrete `DeathEffectBehavior` теперь сохраняют полный
source record, position, impulse, rotation flag и sound catalog.
`SpawnResult` переносит один согласованный snapshot для vehicle,
projectile, mine и bonus paths. Session больше не реконструирует эти поля
из параллельного descriptor-а; Jolt debris использует тот же local impulse
ровно один раз. Regression проверяет owner metadata, сохранение после
respawn/PrepareSource и соответствие active vehicle-death actors исходным
записям.

### P2.244 — lifecycle вложенных `Proj::_model/_model2` возвращён — выполнено

Сверка `Proj::InitProj`, `Proj::ProgressLaser` и `Proj::OnDeath` выявила
ложную реконструкцию: portable session создавал `model2` и `model3` при
каждом impact/expiry по глобальному projectile descriptor. В Windows
include-объектами `Proj` являются только реально инициализированные
`_model/_model2`; `FxSystemWaitingEnd` отсоединяется в точном последнем
world transform. Поле `model3` для Frost устанавливается на машине только
как `SlowEffect`, а MineRip создаёт свои model2/model3 явно как новые
автономные `MapObj`.

`Proj` теперь формирует release plan только из существующих include models
с waiting-end emitters и сохраняет их точные position/rotation/scale.
Session копирует конкретный `MapObjRec` в shared owner, поэтому record живёт
до окончания backend effect даже после удаления `Proj`. Универсальное
создание descriptor `model2/model3` удалено: хвосты laser/frost остаются в
точке попадания, Frost model3 больше не возникает в мире при expiry.
Unit/session regression проверяет endpoint transform, lifetime ownership и
исключение никогда не инициализированного model3.

### P2.245 — source `View` lifecycle и единый Retina hit testing возвращены — выполнено

Сверка `View::ScreenToView`, `ViewToProj`, `ProjToView`,
`OnMouseClickEvent` и `OnMouseMoveEvent` выявила активный разрыв: SDL-host
повторял пересчёт logical coordinates вручную в двенадцати menu branches,
а состояние click/move из Windows вообще не имело владельца. После
fullscreen или смены Retina drawable разные branches могли наблюдать
разные пары window/backbuffer размеров.

`originalview::ViewState` теперь владеет исходным округлением, D3D Y-flip,
projection conversion, click snapshot, move delta и offset от последнего
клика. SDL adapter обновляет размеры до pointer dispatch и при coalesced
resize; все menu/dialog/garage/workshop/options hit tests и cursor используют
этот owner. Отдельный regression фиксирует формулы на 2× Retina viewport и
жизненный цикл Reset. Win32 window styles, D3D reset и screen-ray остаются
платформенными adapter responsibilities.

### P2.246 — source `World::MainProgress` timing возвращён — выполнено

Прямая сверка выявила, что host вручную усреднял 15 кадров, но терял
остальную семантику `World`: double accumulator, fixed-step schedule и
render interpolation. В `WorldEventPump::FrameStep` постоянно передавался
`physicsAlpha=0`, поэтому frame listeners никогда не видели фактическую
долю незавершённого физического шага.

Новый `source::WorldFrameClock` переносит `MainProgress` constants и state:
`cMaxSimStep=1/60`, maximum delta `7/60`, 15 double samples, double
accumulator, fixed count и `startRace ? alpha : -1`. Active SDL loop теперь
использует этот owner вместо массива host-а. Jolt остаётся backend-владельцем
solver execution и не шагается второй раз; source schedule определяет frame
timing/alpha, а существующий `WorldFixedStepController` обслуживает Jolt
substeps. Unit regression фиксирует averaging, carry, clamp и race gate.

### P2.247 — `GameMode` и race graph снова используют один `World` — выполнено

После восстановления clock обнаружилось структурное расхождение: active
host создавал `WorldEventPump` для `GameModeState`, тогда как
`OriginalRaceSession` скрыто владел вторым World для `Logic`,
`RacePlaceModel` и всех GameObject listeners. В Windows `GameMode::Reg*` и
`Logic::Reg*` делегируют одному `_world`; два pause/frame/lifetime домена в
исходнике невозможны.

`OriginalRaceSession` теперь принимает внешний World с безопасным owned
fallback только для headless tests. Active app создаёт общий owner раньше
session и подключает к нему GameMode/Logic/Race/GameCar graph. Destructor
session успевает снять все registrations до уничтожения World. Адресные
Jolt fixed/frame dispatch сохранены, поэтому общего N×N fixed callback нет.
Кроме того, вычисленный `WorldFrameClock::physicsAlpha` передаётся в
`GameCar::OnPxSync`; прежний default 1.0 больше не обходит source
interpolation. Regression проверяет shared identity и три active event list.

### P2.248 — `GameMode::_startUpTime` возвращён source owner — выполнено

Сверка `GameMode::Run`, `OnFrame` и `OnHandleInput` показала, что startup
оставался реконструированным непосредственно в executable: boolean active,
float seconds, продублированные alpha-формулы и произвольный порог 12.25 s.
Такой код совпадал с картинкой приблизительно, но не имел исходных стадий
`-2/-3` и мог менять число loading/menu кадров при другом frame delta.

`source::GameModeStartupState` теперь владеет буквальным автоматом: два
1/3/1-second logo участка с вторым delay 7 s, пустой inter-logo кадр,
`-2` loading, `-3` StartGame и Escape→`-2`. bgfx получает только готовые
alpha/load/start результаты. Regression проверяет середину первого fade,
пустой интервал, hold второго logo, skip, ровно один loading frame и
одноразовый StartGame. Movie `_movieTime` остаётся следующим отдельным
GameMode блоком, поскольку требует video backend commands.

### P2.249 — `GameMode::_movieTime` возвращён source owner — выполнено

Следующая прямая сверка подтвердила аналогичный разрыв в роликах: portable
menu callback сразу останавливал menu music и запускал AVFoundation, а любое
завершение немедленно делало Stop/resume/completion. Windows выполняет это
через 13 состояний `_movieTime`, намеренно оставляя кадры для fullscreen
window, video mode, DirectShow open и безопасного unload.

`GameModeMovieState` теперь выдаёт backend-neutral команды ровно на source
кадрах 0/1/6/9/12. Active host сохраняет выбранный movie/completion, но
Pause/Open/ResetInput/Unload/Resume и VideoStopped происходят только по этим
командам. AVFoundation completion, error и пользовательский skip вызывают
эквивалент `OnGraphEvent` и переводят state в 8; polling прекращается до
отложенного Unload, исключая повторный reset tail. Regression проходит весь
автомат и доказывает четыре wait frame до Play и после completion.

### P2.250 — `World::_env->ProcessScene` возвращён World owner — выполнено

Сверка `World.cpp::FrameStep` и `Environment.cpp::ProcessScene` выявила
следующее ownership-расхождение: portable `OriginalRaceRenderer` напрямую
вызывал Environment во время render preparation. В оригинале World делает
это после ordered frame listeners, до network/control/GameMode, и полностью
пропускает при pause.

Общий active `WorldEventPump` теперь хранит `source::Environment` и вызывает
его в точном source boundary. Renderer передаёт только последнюю
backend-neutral camera/description context и рисует готовое rain state.
Однокадровая позиция камеры соответствует оригиналу: camera
`OnInputFrame` вызывается ControlManager уже после Environment. ReleaseScene
очищает borrowed context до reload. Regression проверяет отсутствие
renderer-side advance, World advance и pause gate; Garage/Angar остаются
presentation-only owners вне active race World.

### P2.251 — `ResourceManager` GUI ImageLib/lifetime возвращены — выполнено

Сверка оригинальных constructor/destructor и `LoadSound` выявила, что
portable GUI оставался вне уже введённого ResourceManager: 111 загрузок
исходных `mainmenu2::Image` создавали отдельные backend texture и экранный
cleanup считал себя их владельцем. Теперь canonical physical path объединяет
GUI, HUD и 3D textures в одном ImageLib-owner; локальный Release игнорирует
заимствованные handles, а общий shutdown выполняется после UI/renderers и до
уничтожения GraphicsDevice.

Деструктор manager идемпотентно выполняет source-порядок
SoundLib→ImageLib→MeshLib. Cache hit `LoadSound` снова применяет последний
volume, а AudioBackend rebind выгружает старую SoundLib до замены указателя.
Новый regression с fake graphics/audio backends доказывает единственную
texture identity, чужой/backend release, смену audio owner и отсутствие
double destruction. TextFontLib закрыт следующим блоком; ComplexMatLib открыт.

### P2.252 — `ResourceManager::TextFontLib` возвращён — выполнено

Прямая сверка `LoadGUI`, `LoadFont`, `ComplexTextFontLibrary::Get` и
`SetFontCharset` показала, что active CoreText path всё ещё получал только
произвольные face/pointSize/bold из executable. Перенесён точный каталог:
Header=44 regular, Item=32 regular, Small=24 regular, VerySmall=18 bold,
VerySmallThink=18 regular, Verdana. Descriptor содержит source charset;
смена языка обновляет все пять существующих записей, как Windows цикл.

Menu и динамический HUD теперь разрешают CoreText параметры через общий
TextFontLib owner. Исправлены явные reconstructed HUD размеры: place 30/bold
стал Header, lap 25/bold и ammo 18/bold стали Small, opponent 15 стал
VerySmall. Options/StartOptions снова используют Small 24 вместо общего
VerySmall 18, из-за которого строки были смещены. Regression проверяет пять
имён, weight, lookup и Russian charset propagation. ComplexMatLib закрыт
следующим resource-owner блоком.

### P2.253 — `ResourceManager::ComplexMatLib` возвращён — выполнено

Сверка оригинальных `ComplexMatLib::LoadLibMat/Get` и
`ResourceManager::AddSampler2dTo` подтвердила не отсутствие material mapping,
а неправильное владение: portable race renderer и HUD копировали уже
перенесённые descriptors в каждый локальный asset. Поэтому одно исходное
имя не имело общей library identity и sampler paths читались из копий.

`OriginalResourceManager` теперь владеет canonical `LibMaterial` по source
record name и сохраняет первый загруженный descriptor. Все активные mesh,
shadow, transparency, particle и HUD weapon-preview paths используют ссылки
на него; diffuse и normal/reflection textures загружаются через тот же
ImageLib owner из canonical fields. MatLib очищается между TextFontLib и
ImageLib/MeshLib. Regression проверяет стабильный адрес, first-record
semantics, material fields и shutdown; bgfx pipeline state остаётся только
backend-представлением исходного материала.

### P2.254 — world-tag lifetime `ComplexMeshLib/ComplexImageLib` возвращён — выполнено

Прямая сверка `ComplexMesh::Load/Unload`, `ComplexImage::Load/Unload` и
`ResourceManager::LoadWorld` подтвердила, что общий portable cache никогда
не освобождал ресурсы прошлой планеты. После нескольких campaign переходов
он удерживал decoded meshes и Metal textures всех посещённых миров, хотя
Windows освобождает payload старого tag и оставляет только library record.

Mesh/Image records теперь имеют индекс World1..World6, вычисленный из того
же `Data\\WorldN` имени. Переключение происходит после teardown активных HUD
и race assets, уничтожает только handles прежнего мира и сохраняет record;
повторный `Get` до первого кадра заново материализует его с тем же адресом.
Global tag -1 не выгружается. Startup и tournament reload вызывают
`LoadWorld` из serialized `TrackCatalogEntry::worldType`. Regression с двумя
валидными `.r3d` и двумя images проверяет World1→World2→World1, destruction,
stable identity, handle recreation и idempotent shutdown.

Одновременно проверен следующий предполагаемый пробел — `ShaderLib`. В
оригинальном constructor обе регистрации shader classes закомментированы,
по всему `prog` нет вызовов `GetShaderLib`; библиотека shipped runtime пуста.
Поэтому переносить туда invented bgfx entries было бы ошибкой. Реальные graph
shader/effect objects остаются в renderer backend, как D3D9-объекты остаются
у `GraphManager` в Windows.

### P2.255 — полный `ResourceManager::Load()` MatLib catalog возвращён — выполнено

Следующая сверка выявила, что canonical MatLib всё ещё наполнялась по факту
посещения активного race/menu renderer. Оригинальный `ResourceManager::Load`
до игры вызывает world1..6/effects/crush/cars/bonus/weapons/upgrades/GUI и
тем самым делает доступными все именованные материалы независимо от карты.

В source подтверждены 251 уникальное имя из активных `Load*LibMat` и
`LoadCarLibMat` вызовов и ещё шесть вручную созданных записей, итого 257.
Публичный backend-neutral catalog теперь строит все descriptors из того же
mapping кода, а `OriginalResourceManager::Load` регистрирует их до создания
экранов. gravBall, mortiraBall и Car blend корректно остаются без diffuse
texture; два World2/track2 вызова остаются закомментированными, как в Windows.
Полный physics resource regression проверяет 257 уникальных имён и existence
всех declared texture paths; GPU payload по-прежнему загружается лениво.

### P2.256 — полный `ComplexMeshLib/ComplexImageLib` catalog возвращён — выполнено

После MatLib manager всё ещё не воспроизводил второй основной результат
`ResourceManager::Load`: Windows заранее добавляет все `ComplexMesh` и
`ComplexImage`, тогда как portable records появлялись только при первом draw.
Так терялись не только имена, но и `buildTBN`, immediate `Load/Init`, mip/GUI,
world tag и его отдельное действие `cTagLoadData/cTagInitIVB/
cTagInitTex2d`.

Из точного файла `eff9338:prog/Rock3dGame/source/game/ResourceManager.cpp`
механически сформирован проверяемый catalog: 324 `LoadMesh` и 490
`LoadImage`. Он намеренно сохраняет повторные вызовы `World6/Track/most`,
`Car/monstertruckBossWheel` и `World5/Texture/piece`; canonical owner, как и
lookup по имени, объединяет их в 322 mesh/489 image identities. Все flags и
порядок записаны в backend-neutral descriptors, а `OriginalResourceManager`
регистрирует их до GUI/race. Decoded R3D и Metal textures по-прежнему
создаются только при первом потреблении, что является заменой D3D9 payload,
а не заменой игровой библиотеки.

Catalog не получен сканированием и поэтому не засасывает лишние assets,
включая многочисленные файлы `* 2.*`. Regression проверяет call/identity
counts, exact duplicates, mip/GUI/TBN и все шесть world distributions.
Shipped data подтверждает единственное расхождение самого оригинала:
неиспользуемый `GUI/wndLight6.png` объявлен, но отсутствует; рабочий
`RaceMenu2` выбирает `wndLight4.png`. Logical record key повторяет
case-insensitive Windows identity, тогда как реальное открытие продолжает
проходить через защищённый canonical data path и backend filesystem.

### P2.257 — `ResourceManager::LoadSounds` возвращён — выполнено

После Mesh/Image/Mat/TextFont оставалась одна подтверждённая потеря внутри
самого `ResourceManager::Load`: последний вызов `LoadSounds()` отсутствовал.
Из-за этого source SoundLib не имел заранее созданной библиотеки, а host
лениво добавлял только UI, motor и эффекты, встретившиеся в active race.

Из того же `eff9338` generated catalog перенёс ровно 46 активных вызовов
`LoadSound("Sounds\\...")`; три закомментированных bullet sounds не добавлены.
Все пути уникальны и присутствуют в shipped data. Сохранены eager load,
нулевой distance scaler, 39 volume=1 и семь volume=2 resources. Descriptors
создаются в исходной `Load` стадии; разрешённая platform-адаптация откладывает
только Ogg decode до `AttachAudio`, потому что SDL/CoreAudio поднимается после
Metal resources. До создания Menu SoundSheme загружены все 46 handles.

`GetSound` больше не возвращает пустой handle для заранее существующего
record: он materialize-ит payload, затем применяет последнюю volume, как
Windows `SoundLib::Find` + `SetVolume`. Обязательная ошибка decode завершает
startup, optional commentator voices по-прежнему проверяются до загрузки.
`MusicCat` остаётся потоковой адаптацией отдельного `GameMode::LoadMusic`,
поэтому возвращать eager decode шестнадцати больших треков и прежнее
зависание меню было бы неверно. Regression проверяет весь SFX catalog;
dummy/Metal race smoke показывает 126 loaded SoundLib records после
добавления доступных голосов.

### P2.258 — `ResourceManager::StringLib` возвращён в общий owner — выполнено

Предыдущие P2.50/P2.52 точно перенесли parser и lookup semantics, но
разместили persistent результат не там: `MainMenu2::Model` владел одной
копией, HUD при каждом initialize загружал вторую. В `eff9338`
`ResourceManager` создаёт ровно один `_stringLib`, а
`GameMode::ApplyLanguage` вызывает на нём `LoadFromFile` сразу после
`SetFontCharset`.

`OriginalResourceManager` теперь владеет StringLib вместе с остальными
source libraries и предоставляет один lookup active menu и HUD.
`ApplyLanguage` разбирает новый UTF-16LE файл до публикации, затем меняет
TextFontLib charset, StringLib content и language name. Options не применяет
половину новой локали к старым text textures: как Windows, он сохраняет
config и выводит need-reload. Shutdown освобождает SoundLib, StringLib,
TextFontLib, MatLib, ImageLib и MeshLib в исходном порядке.

Fake-manager regression проверяет два последовательных языка, стабильную
identity, replacement старых keys, пустое значение, fallback id, escaped
newline, charset и idempotent cleanup. Shipped verification и Metal race
smoke проходят на общей русской StringLib.

### P2.259 — общий `snd::Source3d` lifecycle возвращён постоянным emitters — выполнено

Повторная сверка `snd::Source::{Init,Free,Play,Stop}`, `Source3d::{Play,Stop,
ApplyX3dEffect,MyReport::OnStreamEnd}`, `SoundMotor` и `PxWheelSlipEffect`
подтвердила оставшуюся архитектурную потерю. Формулы громкости и 30/45 м уже
совпадали, но active race держал raw SDL voice handles и отдельные boolean
proxy states. Поэтому game-side play intent, natural EOF и backend voice не
имели одного владельца, как исходный `Source3d`.

`OriginalSource3d` теперь является movable RAII owner над SDL/CoreAudio:
sound/resource volume, source volume, frequency, position, loop/once, `_play`
intent и proxy voice живут вместе. На расстоянии больше 45 м backend voice
именно останавливается, внутри 30 м создаётся заново, а полоса 30..45 м не
меняет предыдущее состояние. Natural EOF одноразового source очищает intent;
повторный `Play` активного source остаётся idempotent.

Два motor source каждой машины и wheel-slip source каждого звучащего колеса
переведены на этот owner. Race exit/reload/disconnect больше не поддерживают
второй глобальный registry handles. Fake backend закрепляет gain/pitch,
stop-lag, restart, move без double-stop и `pmOnce` completion. XAudio2 proxy,
priority allocator и streaming заменены разрешённой SDL/CoreAudio backend
границей; transient Shot/Life/Contact records переводятся следующим блоком.

### P2.260 — transient `Source3d` и PCM stop/resume возвращены — выполнено

Сверка `Proxy::Streaming::{Play,Stop,GetPos,SetPos}` уточнила важную деталь
P2.259: дальний `Source3d::ApplyX3dEffect` вызывает Stop, который не перематывает
stream. Raw SDL implementation сохраняла voice в paused-состоянии, а первая
версия общего owner уничтожала handle без cursor. Второй вариант неверно
начинал loop заново и мог проявляться как повторяющееся заикание шин.

Owner теперь снимает `voicePositionFrames` перед stop-lag/explicit Stop и
возобновляет новый backend voice через `startFrame`. `SetPos(0)` останавливает
текущий handle без сброса play intent и создаёт его с нуля на следующем
Update. Это повторяет `ShotEffect::OnShot` и условный rewind
`PxWheelSlipEffect`, тогда как обычный выход за 45 м продолжает прежний cursor.

Все три оставшихся spatial host records переведены на этот класс. Shot
Source3d сохраняется на owner/slot/sound после EOF и при каждом выстреле
получает `SetPos(0); Play()`. Life source живёт до effect lifetime. Pair
contact source после `pmOnce` EOF остаётся молчащим в node до release
последнего point через 0.1 s; повторный collision того же живого pair его не
пересоздаёт. Start/exit/disconnect/reload уничтожают owners через RAII.
Regression закрепляет resume с кадров 123 и 222, explicit seek 0, EOF и
точное число backend Stop.

### P2.261 — обычный `snd::Source` возвращён Menu и Commentator — выполнено

Прямая сверка `Source::{SetSound,SetPos,Play,Stop,IsPlaying}`,
`Menu::{PlaySound,StopSound}` и `GameMode::Commentator::{Play,Next,
OnStreamEnd}` подтвердила два последних raw voice owner вне MusicCat.
Menu вручную держал один SDL handle, а Commentator — другой handle и очередь;
общего source lifecycle между ними не существовало.

Backend-neutral `OriginalSource` теперь владеет category, sound/resource
volume, source gain/frequency, mode, pause, cursor и Proxy handle. Stop
сохраняет frame, SetPos освобождает текущий voice и задаёт точный cursor,
SetSound применяет resource volume, Play idempotent для active proxy и RAII
не допускает double-stop. Regression проверяет Effects/Voice-independent
fields, gain 2×0.25, pitch 1.5 и cursor 33→77→0.

Menu SoundSheme использует один Effects owner для всех девяти cues с точной
цепочкой Stop/SetSound/SetPos(0)/Play. Commentator использует один Voice owner
для replace, queued natural Next и pause; все 37 comment descriptors и 80
доступных файлов остаются прежними. Последняя direct `audio.play` ветка в
active game host удалена после доказательства, что все три создаваемых
`EffectSound` имеют Shot, Life либо PairContact identity. MusicCat не
схлопывается в Source: его отдельная очередь и background decode соответствуют
исходному вложенному классу и остаются специализированным adapter owner.

### P2.262 — `DamageEffect`/`ImmortalEffect` Source3d callbacks — выполнено

Сверка `DamageEffect::OnDamage`, `ImmortalEffect::OnImmortalStatus` и
`EventEffect::{GiveSource3d,OnProgress}` выявила оставшийся разрыв после общей
Source3d migration. Visual state machines уже были source-owned, но их
behavior-каталоги и независимые audio callbacks не существовали: звук мог
быть ошибочно отнесён к `LifeEffect` порождённого visual actor.

`Vehicle` теперь отдельно хранит type-5/type-11 `sounds` самой car behavior,
а `DamageEffect` и `ImmortalEffect` выдают play request независимо от
`MakeEffect`. Damage при каждом подходящем `dtEnergy` повторяет
Stop→once→SetPos(0)→Play, даже если прежний 0.5-секундный actor ещё жив;
Immortal делает once→SetPos(0)→Play только на реальном переходе shield off→on.
Host держит один `OriginalSource3d` на racer/behavior/path, каждый кадр
следует за позицией автомобиля и уничтожает owner вместе с car.

В исходных сгенерированных `DataBase::LoadCar` и поставляемом `db.xml` оба
каталога намеренно пусты (три bullet-hit AddSound закомментированы, shield
activation cue приходит из DeathEffect бонуса `Snd\\shieldOn`). Поэтому
активные ресурсы не получают выдуманный новый звук; synthetic regressions
закрепляют полностью рабочий callback для данных, где каталог задан.

### P2.263 — `Logic::SndCategory` снова владеет gain и mute — выполнено

После возврата concrete Source/Source3d оставался более высокий утраченный
owner: Windows `Logic` создаёт три submix voice, хранит `_volume/_mute` для
Music, Effects и Voice, а `GameMode::Pause` вызывает только
`Mute(scEffects)`. Portable host вместо этого читал profile config напрямую
и вычислял zero/restore из race pause-флага.

В `source::Logic` возвращены `GetVolume`, `SetVolume`, `AutodetectVolume` и
`Mute` с исходной семантикой: stored volume меняется даже под mute, applied
volume остаётся нулём и восстанавливается при unmute. RaceSession больше не
выдаёт pause как суррогат audio-state и маршрутизирует pause в Effects owner.
Config load, Options preview/cancel/apply, race-start и finish music fade
передают в SDL/CoreAudio уже готовый source gain. Backend только реализует
три submix bus; source default/autodetect/muted-update и active pause/resume
закреплены regressions.

### P2.264 — `GameMode` снова владеет music fade — выполнено

Прямая сверка `FadeInMusic`, `FadeOutMusic`, `OnFinishFrameClose` и
`OnFrame` выявила отдельный host-суррогат поверх восстановленного Logic
submix. `sourceMenuMusicGain` вручную реализовывал только переход 0→1 и при
каждом старте гонки сбрасывался в 1, хотя Windows `PlayMusic` сохраняет
volume единственного `_music` source.

Новый `GameModeMusicFadeState` владеет `_fadeMusic`, `_fadeSpeedMusic` и
текущим source-volume. Сохранены даже исторически обратные названия:
`FadeInMusic` выбирает target 0, `FadeOutMusic` — target 1. Frame выполняет
точную формулу `(target-current)*deltaTime/(speed>0?speed:1)` и clamp 0..1;
pause/movie gate не продвигает переход. `OnFinishFrameClose` запускает
source-owned 0→1, SDL/CoreAudio применяет его произведение с уже
source-owned `Logic::scMusic`. Regression проверяет числовые шаги, обе цели,
inactive frame и композицию внутри `GameModeState`.

### P2.265 — единый `GameMode::_music` source возвращён — выполнено

Прямая сверка конструктора/деструктора `GameMode`, `MusicCat::{Play,Stop,
Pause,OnStreamEnd}` и `GameMode::{PlayMusic,StopMusic}` выявила более
глубокое расхождение: Windows создаёт ровно один `snd::Source _music`, а
`_menuMusic` и `_gameMusic` являются только двумя владельцами playlist/report
состояния над ним. Portable adapter ошибочно создавал по независимому SDL
voice на каждый каталог, поэтому при неидеальном переходе оба трека могли
жить одновременно, а cursor/stream-end принадлежали не тому MusicCat.

`OriginalGameModeMusicSource` теперь владеет единственным backend voice и
текущим report-owner. Любой `PlayMusic` сначала выполняет глобальный
`StopMusic`, затем публикует нового владельца; natural EOF доставляется
только активному каталогу, а заменённый report не запускает свой Next.
`Pause(true)` сохраняет PCM cursor и освобождает общий voice, `Pause(false)`
возобновляет тот же трек. Menu и game MusicCat подключены к одному owner;
FinalMenu намеренно оставлен на отдельном source. Детерминированный fake
backend regression проверяет identity, owner A→B, cursor, natural EOF и
global Stop; integrated smoke закрепляет невозможность двух voices.

### P2.266 — `Run(false)` и `CheckStartupMenu` исправлены — выполнено

Повторная сверка `GameMode::{Run,PrepareGame,StartGame,FreeIntro,
CheckStartupMenu}` обнаружила ошибку в ранее закрытом P2.248: portable
`Run(false)` делал startup owner сразу inactive. Windows при отсутствии двух
логотипов всё равно проводит `_startUpTime=0` blank, затем `-2` с
`startLogo.dds`, затем `-3`, где последовательно исполняются PrepareGame и
StartGame/FreeIntro. Повторный Run после StartGame игнорируется.

`GameModeStartupState` теперь хранит prepared/started guards и выдаёт обе
ветви `Run(true/false)` с точными blank/loading/prepare/free/start edges.
Отдельный `GameModeStartupMenuState` переносит one-shot приоритет
`_prefCameraAutodetect` перед `_discreteVideoChanged`: закрытие обязательного
StartOptions вызывает Check повторно и только тогда потребляет GPU action.

Связанный integrated smoke выявил ещё один active дефект: SDL release-event
обрабатывался host-меню как второй Up/Down/Left/Right, хотя Windows
`OnHandleInput` принимает навигацию только при `ksDown`. Release теперь не
двигает selection и не меняет option второй раз; audio smoke выбирает
Multiplayer одним нажатием. Unit regression проверяет обе Run-ветви,
idempotency, Prepare/Free и оба результата integrated/discrete GPU.

### P2.267 — глобальные `MapObj` ID возвращены live `MapObjList` — выполнено

Сверка `Map::{Load,AddMapObj,InsertMapObj}` и
`Map::MapObjList::InsertItem` обнаружила, что portable importer подменял
исходного владельца identity. Он заранее пересчитывал XML-категории,
записывал ID в descriptors декораций, бонусов и racers, а session затем
насильно повторял вычисленные значения и резервировал диапазон. В Windows
ID отсутствует в этих descriptors: `_lastId` увеличивается единственным
живым `MapObjList` непосредственно при успешной вставке.

Удалены `mapObjectId`, `firstDynamicMapObjectId`, обе assign-функции,
`ReserveIdsThrough` и публичный overload `AddMapObj(..., sourceId, ...)`.
Автоматические Add/Insert сначала разрешают record и создают объект, затем
регистрируют следующий свободный ID; неудачная операция не меняет
`_lastId`. Network damage/bonus transport получает ID из live `MapObj`, а
не из parser mirror.

Проверка 190 bundled карт показала, что `ctEffects`, `ctWeapon`, `ctCar` и
`ctWaypoint` во всех них пусты; active load order полностью материализуется
как Decoration→Track→Bonus→Car. Integrated map1 regression закрепляет
1..234/235..286/287..293/294+, а Map regression — последовательность после
ошибки DataBase, clone/transfer и глобальный registry lookup.

### P2.268 — `Player::CreateCar/FreeCar` владеют car MapObj — выполнено

Прямая сверка `Player::{CreateCar,FreeCar,OnDestroy,OnProgress}` показала,
что portable session всё ещё обходил `CarState::mapObj`: отдельный vector
создавал car MapObj до `Player::CreateCar`, вручную удалял его после
`Destroy/Disconnect` и заново материализовал перед `ResetCar`. Это оставляло
два признака существования машины и две независимые lifecycle-ветви.

Live `MapObj*` перенесён в `source::Player`. `CreateCar` вызывает общий
`Map::AddMapObj`, связывает единственный embedded `gameCar` и Player;
`FreeCar` снимает graph/audio/presentation state и удаляет этот объект из
Map. Restore, disconnect и ExitRace уже вызывают эти исходные методы, поэтому
session-side create/free и весь `racerMapObjects_` удалены. Все target paths
получают GameObject от `Player::GetCarMapObj`.

Lifetime полей исправлен: DataBase/Map уничтожаются после Player. Regression
закрепляет pointer identity и reverse links, IDs 1→удалён→2 на respawn и
очистку при Destroy. Полный active race smoke подтверждает сохранение шести
живых машин, AI targets, Jolt contacts и Metal submission.

### P2.269 — live MapObj владеет состоянием декораций — выполнено

Сверка `MapObjList`, `GameObject::{Damage,OnDeath,GetLiveState}` и
`DestrObj::GetLife` выявила следующий остаток adapter-архитектуры:
`OriginalRaceSession` хранил `decorationActive_` и `decorationLife_` как
второй gameplay owner. Контакты, world ray, reset-car ray и projectile paths
могли принимать решение по этому зеркалу независимо от живого source
объекта.

Теперь active/death проверяются непосредственно через category
`MapObjects::Get(sourceIndex)` и `GameObject::LiveState`, а life берётся из
конкретного `DestrObj`. Source death с `ProgressOne` удаляет объект из Map и
тем самым атомарно выключает collision/ray participation. Отдельное
обнуление массивов при ExitRace удалено: `Map::Clear` уже является исходным
владельцем этого перехода.

Старый vector API оставлен только на границе renderer/smoke и строится по
требованию из живого graph. Ray helpers переведены на live predicate, чтобы
не выполнять полный O(N) rebuild при каждом Laser/FrostRay либо reset ray.
Регрессии разрушения и сетевого урона проверяют удаление actor, отсутствие
повторного события и authoritative life/death через этот единый путь.

### P2.270 — live AutoProj владеет bonus lifetime и scale — выполнено

Сверка Windows `Logic::TakeBonus`, `Player::TakeBonus`, `Proj::OnContact` и
`AutoProj::OnProgress` подтвердила, что bonus-category объект не имеет
отдельного active-флага: pickup вызывает `GameObject::Death`, mine hazard
проходит через `DestroyWithEffect`, а model arming-scale хранит сам
`AutoProj`. В portable session эти значения дублировали
`bonusActive_`/`bonusScales_`; более того, Jolt мог деактивировать source
bonus своим `ProjectileBodyState::active=false`.

Все contact/pickup/network decisions теперь читают live MapObj и
`GameObject::LiveState`; renderer scale берётся из `GetModelScale` по
требованию. Source death единолично инициирует destroy sensor command. В
обратном направлении backend больше не меняет gameplay state: потерянный
sensor живого AutoProj получает новый ID и create command.

Integrated regression искусственно возвращает inactive Jolt sensor,
проверяет, что bonus остаётся source-live, и требует ровно один новый sensor.
Оригинальные countdown arming 0→0.4→1, mine death effect, pickup и
NetPlayer request/replay regressions продолжают проверять общую цепочку.

### P2.271 — Logic владеет transient Proj/Mine lifetime — выполнено

Сверка `Logic::{RegGameObj,ProgressGameObjs,CleanGameObjs}` и projectile
death/contact paths показала, что `ProjectileRuntime::active` и
`MineRuntime::active` всё ещё выступали вторым владельцем. В частности,
неактивный Jolt body мог выключить runtime, хотя зарегистрированный
`source::Proj` оставался жив, а несколько adapter-ветвей завершали только
boolean без `GameObject::Death`.

Все gameplay gates и удаление runtime теперь основаны на наличии объекта в
Logic registry и его `LiveState`. Impact, timeout, attached-owner loss и mine
contact сначала завершают source object; actor destroy и erase следуют за
этим. Renderer-compatible `active` обновляется в публичных getters и не
участвует в решениях.

Jolt остаётся pose/contact/actor boundary. Если backend сообщает inactive
body для живого transient Proj либо Mine, старый ID отбрасывается и выдаётся
одна create-команда с новым ID; source object не меняется. Integrated smoke
закрепляет оба recovery пути вместе с fixed-step attack/mine ownership.

### P2.272 — Race владеет завершением турнирного прохода — выполнено

Повторная сверка `Race::CompleteRace(const Results*)`, `Race::GetTotalPoints`
и `Tournament::CompleteTrack` выявила оставшийся host-owned хвост. Session
ранжировал участников и начислял награды, но 22k-line SDL entry point отдельно
решал, когда продвинуть турнир, сам собирал aggregate points/count, очищал
очки завершённого прохода и удерживал one-shot флаг. Это позволяло сохранению
профиля обходить исходную атомарную транзакцию Race.

Новый `OriginalRaceSession::commitProfileAndTournament` сначала публикует
live Player/achievement state, затем один раз выполняет source tournament
advance на сумме Human/Opponent и очищает points только при `passComplete`.
Disconnected tombstones, как и удалённые Windows `NetPlayer`, исключены.
Host получает готовый `TournamentAdvance` и отвечает только за persistence и
UI transition. Regression проверяет запрет раннего advance, окончательные
money/points, pass reset и невозможность повторного продвижения.

### P2.273 — Proj владеет исходными scene-query — выполнено

Повторная сверка `Proj::{LaserUpdate,RocketUpdate,MinePrepare}` выявила, что
session всё ещё вручную выбирал origin, distance и collision group. При этом
`MinePrepare` был фактически урезан с `cdgTrackPlane | cdgShotTrack` до одной
TrackPlane: уже размещённая мина не могла отклонить установку второй.

Concrete `source::Proj` теперь выдаёт backend-neutral `SceneRayQuery` для
Laser/FrostRay, Rocket и Mine, включая точные offsets и group policy. Jolt
сохраняет `ShotTrack` bit только у mine actor-ов и возвращает их отдельную
identity из closest-ray. Session лишь преобразует векторы и применяет
исходный `AcceptSceneRayHit`; CPU fallback проверяет те же live mine OBB.

Regression покрывает построение всех трёх запросов, Jolt distinction между
TrackPlane и ShotTrack и запрет stacking без расхода второй charge.

### P2.274 — RaceLifecycle владеет Player/result/reward settlement — выполнено

После возврата tournament advance аудит `Race::CompleteRace(Player*)` и
`Race::CompleteRace(const Results*)` обнаружил оставшийся split: session
сама меняла `Player`, сбрасывала pickMoney, ставила finish block, начисляла
campaign rewards и удерживала второй one-shot-флаг. В `Player` при этом был
неоригинальный `ApplyRaceReward`, который брал текущее pickMoney уже после
reset и не входил в Windows API.

`source::RaceLifecycle` теперь выполняет обе source-транзакции: публикует
`RaceResult` в Player и после materialization всех результатов один раз
начисляет деньги из сохранённого `result.pickedMoney` и points. Session
занимается только pending Jolt/AI/input cleanup. Из Player удалён придуманный
метод, а one-shot reward state перенесён к владельцу `_results`.

Lifecycle regression закрепляет порядок AI/Human результатов, exact reward
для мест 1–3, pickMoney reset, 0.3-second block и отсутствие double award.

### P2.275 — удалена вторая модель trace/checkpoint прогресса — выполнено

Метод-к-методу сверка `Player::CarState::Update`, `GetDist`, `GetLap` и
`GetMapPos` показала, что concrete source owner уже содержит оригинальную
логику полностью. Однако session поверх него поддерживала `nextPathNode`,
сама переводила NodeRef обратно в parser `TracePoint` и выпускала событие
`Checkpoint`, которого нет в Windows GameEvent/Race/Player.

Эта параллельная модель использовалась только debug и smoke, но могла
расходиться на branch path и после reset. Она удалена вместе с мёртвыми
trace lookup helpers. Debug читает live NodeRef, AI progress validation —
точный `CarState::GetLap`, а MiniMap как и раньше использует исходный
`CarState::GetMapPos`.

## Итоговое решение

Текущий порт не следует выбрасывать: в нём уже есть native platform
backends, оригинальные ресурсы и значительный объём перенесённых правил.
Но продолжать считать `Original*` названия доказательством исходности нельзя.
Эталоном для следующей работы должен быть `eff9338:prog/...`, а
`dxvk-glm` — только вспомогательной модернизированной веткой. Закрывать
подсистему как «портированную» следует лишь после прямого source mapping и
Windows/macOS trace comparison.
### P2.276 — `HumanPlayer::_curWeapon` снова единственный owner выбора — выполнено

Прямая сверка `HumanPlayer::{ChangeWeapon,SelectWeapon,Shot,ShotAll}` и
`Player::WeaponItem` обнаружила ещё один session-суррогат. Хотя source
`HumanPlayer` уже хранил `_curWeapon`, portable `Player` параллельно держал
`selectedWeaponSlot`, `selectedWeapon` и `ammunition`; session постоянно
вызывал `SyncSelectedWeapon` перед human, AI и network shots. В результате
команда прямого Shot1..4 или ShotAll могла временно либо постоянно менять
выбранное оружие HUD, чего исходный код не делает.

`fireWeapon` теперь получает физический primary slot явно и разрешает
weapon/index/item из source loadout только для этой команды. Human current
shot использует результат `HumanPlayer::SelectWeapon`; direct, all, AI и
network paths не касаются human selection. HUD получает `_curWeapon` через
read-only session boundary, а новый race/reset возвращает исходное начальное
значение 0. Удалены также мёртвые поля `ammunition` и `speedBoostSeconds`:
первое дублировало `WeaponItem::GetCurCharge`, второе нигде не включалось и
создавало отсутствующую в Windows throttle-ветвь. Полные 32 CTest и active
Jolt/Metal smoke подтверждают owner/order.
### P2.277 — live weapon charge возвращён только в WeaponItem — выполнено

После P2.276 прямое сравнение `WeaponItem::{Load,Shot,Reload,GetCurCharge,
GetCntCharge}` подтвердило второй слой зеркал. Portable `WeaponItem` уже
хранил source `_curCharge/_cntCharge`, но `Player` публично сохранял ещё
шесть staging-полей; после bind они становились stale.

Введён одноразовый `Player::WeaponLoadout` для profile/roster import.
`BindWeaponItems` копирует его значения в concrete primary/hyper/mine items
и сохраняет только record indices, нужные renderer/network adapters. Все
живые изменения charge принадлежат WeaponItem. Player, Weapon, lifecycle и
integrated race regressions переведены на этот owner; 32 CTest, resources,
Jolt и Metal smoke прошли.

### P2.278 — world pose снарядов возвращён concrete Proj — выполнено

Сверка исходных `Proj::{FireUpdate,ProgressAttached,ProgressFree,
ProgressPlacedMine}` показала, что их `GameObject` является владельцем
мирового transform. В portable path source-progress соблюдал это правило,
но `synchronizeProjectilePhysics` записывал результат Jolt исключительно в
`ProjectileRuntime`/`MineRuntime`. После backend tick исходные death effects,
scene queries и восстановление потерянного actor-а могли видеть старую позу.

Теперь Jolt state проходит через source `SetWorldPos/SetWorldRot`, а публичные
render views получают position/rotation из `GetWorldPos/GetWorldRot`.
Session-поля остаются только transport snapshot для Jolt/Metal и swept
contact. Integrated regression отдельно проверяет projectile и mine pose.

### P2.279 — homing target принадлежит только Proj::_shot — выполнено

`ProjectileRuntime::target` удалён как опасная параллельная модель. Source
`Proj` уже слушает target `GameObject` и в `OnDestroy` очищает
`_shot.targetMapObject`; session index такого lifetime не имел. После
respawn он мог адресовать новый car object прежнего racer index, хотя
оригинальный снаряд цель уже потерял.

Homing update, Impulse contact filtering и запрет decoration-contact во
время цепочки теперь каждый раз сопоставляют живой
`Proj::GetSourceTarget()` с текущим `Player::GetCarMapObj()`. Подготовка
backend runtime больше не принимает отдельный `homingTarget`; retarget
меняет только source ShotDesc/listener graph. Smoke assertions проверяют
source identity для AI 3D selection, sphereGun и Impulse handoff.

### P2.280 — Maslo visual scale возвращён source model — выполнено

Прямая сверка `MasloPrepare/MasloUpdate` подтвердила, что fade масла не
является полем projectile actor-а: исходник ставит scale 0 и затем 0..1 на
включённом `_model->GetGameObj()`. Portable source уже выполнял это точно,
но renderer обходил его через дублирующий `MineRuntime::armingAlpha`.

Поле удалено; bgfx/Metal получает scale из concrete source model. Заодно
удалены мёртвый distance accumulator session и неисполняемый
`MineRipChildSpawn::armingScale`, отсутствующие в Windows call graph.
Regression проверяет, что активное масло завершает arming именно в source
model, а проверка max-distance использует разность реальных world poses.

### P2.281 — projectile speed принадлежит одному velocity vector — выполнено

В Windows `CalcSpeed` возвращает `D3DXVECTOR3`, который становится
`NxBodyDesc::linearVelocity`; дополнительного scalar live-state нет.
`ProjectileRuntime::speed` был портовым зеркалом, которое приходилось
обновлять после Jolt sync, Torpeda, Fire и launch.

Поле удалено. Единственный backend owner — velocity, как единственным PhysX
owner был actor velocity. Все потребители используют его magnitude. Вместе
с зеркалом удалён отсутствующий в оригинале fallback `max(speed, 1)`,
заставлявший projectile с нулевой скоростью ползти вперёд.

### P2.282 — projectile route и weapon link возвращены concrete Proj — выполнено

Сверка `eff9338:prog/Rock3dGame/source/game/Weapon.cpp` подтвердила два
параллельных owner-а в session. `MortiraPrepare` включает обычную PhysX
гравитацию параметром `disableGravity=false` у `RocketPrepare`; отдельного
live-поля ballistic в projectile нет. Аналогично, mine/MineRip lifetime
связан только с `Proj::_weapon`: `Proj::OnDestroy` вызывает `SetWeapon(0)`.

`ProjectileRuntime::ballistic` и `MineRuntime::linkedToOwner` удалены.
Jolt gravity и portable fallback выводятся из живого
`Proj::RoutePreparation()`, а regression автономных model2/model3 объектов
проверяет `GetSourceWeapon()==nullptr`. `ProjectileRuntime::attached` не
является новым gameplay owner-ом и сохранён как конечное состояние backend
body: при потере weapon он обеспечивает ровно один переход
kinematic-to-dynamic, которого у source `Proj` после listener callback уже
нельзя восстановить.
