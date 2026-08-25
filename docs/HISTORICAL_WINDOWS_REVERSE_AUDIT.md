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
прямую подачу source torque на driven wheels, пропуск low-speed `restTorque`,
scalar friction вместо PhysX anisotropic material и Jolt-specific solver
order. Это допустимые engineering adaptations, но они требуют trace/A-B
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

## Итоговое решение

Текущий порт не следует выбрасывать: в нём уже есть native platform
backends, оригинальные ресурсы и значительный объём перенесённых правил.
Но продолжать считать `Original*` названия доказательством исходности нельзя.
Эталоном для следующей работы должен быть `eff9338:prog/...`, а
`dxvk-glm` — только вспомогательной модернизированной веткой. Закрывать
подсистему как «портированную» следует лишь после прямого source mapping и
Windows/macOS trace comparison.
