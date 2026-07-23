# Перенос гонки и результат Milestone 9

## Что является результатом

Milestone 9 запускает из перенесённого `MainMenu2` гонку, собранную из
оригинальных данных Motor Rock. Старый procedural `PortableRace` с придуманной
овальной трассой в preset `macos-arm64-m9` не компилируется и результатом
milestone не является.

По умолчанию `Single Player` открывает
`Data/Map/World1/map1.r3dMap`, ставит Marauder и пять соперников в исходную
стартовую решётку и начинает четыре круга после трёхсекундного countdown.
Параметрами запуска можно выбрать любую из 88 турнирных трасс, любую из 17
машин гаража и один из четырёх перенесённых режимов погоды.

## Происхождение сцены

Для первой карты загрузчик использует:

- 52 размещения `ctTrack`, 234 размещения `ctDecoration`, 7 `ctBonus` и
  исходную trace из `map1.r3dMap`;
- visual/collision records, составные destructible-объекты и material records
  из `db.xml`;
- body/wheel meshes, DDS и список 17 машин из `garage.xml`;
- четыре круга, состав AI и варианты машин для pass 1/2 из `tournamet.xml`;
- 1175 collision triangles после размещения track и имеющих collision
  decorations.

Material mapping map1 повторяет записи `ResourceManager` для World1,
World2 semaphore, World3 exceptions, Crush и Bonus. Alpha-test применяется к
растительности и прозрачным bonus materials. Для остальных миров loader
сначала ищет соответствующую штатную texture, затем исходный track atlas.
Все 88 карт проходят разрешение mesh/material/collision ресурсов.

В исходной базе встречаются Windows-пути с другим регистром, например
`pxMost.r3d` при файле `PXmost.r3d`. Перенесённый loader исправляет такой путь
только при единственном однозначном case-insensitive совпадении. Строгий
`ResourceFileSystem` и защита от неоднозначных/выходящих за game-data путей
остаются включены.

## Физика и старт

PhysX 2.8.4 остаётся неизменным для Windows. На Apple Silicon используется
Jolt Physics 5.5.0, зафиксированный commit
`23dadd0e603f1b321142d4c74df07fce85064989`.

Adapter:

- переводит игровую Z-up систему в Jolt Y-up;
- строит static triangle collision из штатных `.r3d`;
- использует legacy gravity `-20`, friction/restitution `0.5`;
- создаёт отдельный Jolt vehicle для игрока и каждого AI;
- загружает mass, shape pose, center of mass, wheel positions/radius,
  suspension spring/damper/travel, driven/steering flags, differential,
  brake torque, max RPM и torque из `db.xml`;
- выполняет simulation фиксированными шагами 1/120 s;
- воспроизводит формулу `Race::ResetCarPos`: по четыре машины в ряду,
  `rowSpace = 7`, ширина ряда из body meshes, первая trace point плюс 2 по Z.

Это перенос данных и поведения, но не численная эмуляция PhysX 2. Jolt имеет
другой contact solver и tire model, поэтому окончательная калибровка требует
одинакового записанного Windows replay.

## Race state и AI

`OriginalRaceSession` реализует:

- countdown, pause/resume, упорядоченные checkpoints, laps, place,
  wrong-way, finish и остановку управления после финиша;
- ручной reset (`R`/gamepad North) и respawn на последнем пройденном trace
  point при падении или застревании;
- пять AI-соперников из исходного planet/pass roster, steering к следующей
  trace point, throttle и braking перед поворотом;
- life, collision damage, shield и respawn после уничтожения;
- pickup денег, medpack, ammunition, mine, shield и speed boost;
- базовый направленный gun, ammunition/cooldown, AI fire и временные
  weapon/damage effects;
- отключение подобранных bonuses и разрушенных `gotDestrObj`.

AI и combat здесь являются первым переносимым runtime-слоем над исходными
данными. Полный legacy `AIPlayer`, весь workshop weapon/projectile catalog,
физические мины, индивидуальные upgrade/weapon slots и оригинальный effect
graph ещё не перенесены.

## Renderer, HUD, камера и погода

`OriginalRaceRenderer` через bgfx/Metal рисует все track/decor/bonus/car/wheel
visual nodes с исходными `.r3d`, material groups и DDS. Сцена получила
направленное освещение от map sun, ambient, fog, sky texture, alpha-test,
исчезновение разрушенных объектов и анимацию доступных bonuses.

Камера использует исходные константы третьего лица из `CameraManager`:
`cCamTargetOff = (-4.6, 0, 2.4)`, дополнительное смещение назад, speed
pull-back, FOV 75°, near 1 и far 120. HUD использует штатные
`placeMineHyper.png`, `lifeBarBack.png`, `lifeBar.png`, `lap.png` и выводит
place, lap, speed, RPM, ammunition, mines, money, countdown, wrong-way,
pause и finish.

Опции `fair`, `night`, `cloudy` и `rainy` переносят исходные environment
colors/fog; rainy добавляет локальные rain streaks. Полный legacy particle
manager, shadows/reflections, Sahara/Hell/Snow и все специальные world effects
пока отсутствуют.

## Audio и управление

SDL actions подключают keyboard/mouse/gamepad:

- `W/S/A/D` или triggers/stick — accelerate, brake и steering;
- Space/left mouse/right shoulder — базовое оружие;
- `R`/gamepad North — reset;
- `P`, Escape или gamepad Start — pause/return;
- Tab, mouse wheel и gamepad West уже дают portable `ChangeWeapon`, но полный
  legacy selector пока не подключён к workshop slots.

Для каждой из шести машин загружаются её `sndIdle` и `sndRPM` из `db.xml`.
Jolt engine RPM управляет cross-fade и pitch. У соперников есть distance
attenuation и stereo pan относительно ориентации машины игрока. Это рабочий
portable spatial baseline, а не полная X3DAudio emitter/listener parity с
legacy cones, doppler и obstruction.

## Проверка

```bash
cmake --preset macos-arm64-m9
cmake --build --preset macos-arm64-m9 --target RRR3d -j 8

build/macos-arm64-m9/Debug/RRR3d --physics-smoke-test
SDL_AUDIODRIVER=dummy \
  build/macos-arm64-m9/Debug/RRR3d --audio-smoke-test
build/macos-arm64-m9/Debug/RRR3d --race-render-smoke-test

build/macos-arm64-m9/Debug/RRR3d \
  --track=1 --car=dirtdevil --weather=rainy \
  --race-render-smoke-test
```

Полный catalog audit проверяет `--verify-resources --track=N` для
`N=0..87`; отдельный audit выбирает все 17 garage records. Physics/session
smoke проверяет acceleration, brake, steering, suspension contact, countdown,
checkpoint/lap/finish, weapon/damage, bonus и respawn. Metal smoke проходит
240 кадров, требует реального menu dispatch, движения и wheel contacts.

## Что ещё нельзя назвать parity

Автоматические проверки подтверждают происхождение ресурсов и целостность
runtime, но не заменяют визуальный A/B. Попытка запустить Windows reference и
macOS build через локальный GUI была остановлена заблокированным macOS
сеансом. После разблокировки нужно записать одинаковый старт и контрольный
заезд в Windows/PhysX и macOS/Jolt, затем сравнить:

- framing/FOV и положение всех машин на стартовой решётке;
- acceleration, braking distance, steering radius и suspension response;
- collision, ramp/jump/fall и reset;
- lap/checkpoint timing, AI lines и combat outcomes;
- light/fog/alpha materials, particles, shadows и audio falloff.

До этой ручной проверки и переноса полного legacy AI/weapon/effect stack
Milestone 9 является расширенным playable race slice, а не полной копией
Windows race mode.
