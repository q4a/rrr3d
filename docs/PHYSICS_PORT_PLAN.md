# Перенос физики и результат Milestone 9

## Статус

Milestone 9 запускает из оригинального `MainMenu2` первую штатную гонку
турнира. Пункт `Single Player` открывает `Data/Map/World1/map1.r3dMap`, ставит
`world\db\root\ctCar\marauder` в стартовую точку исходной trace и включает
vehicle simulation. Старый procedural `PortableRace` с овальной трассой в
этом preset не компилируется и не является результатом M9.

## Источники данных

Новый путь не задаёт трассу, автомобиль или баланс константами приложения:

- `tournamet.xml` выбирает `map1.r3dMap` и четыре круга;
- `map1.r3dMap` задаёт 52 размещения `ctTrack`, их position/rotation/scale и
  trace из пяти точек;
- `db.xml` разрешает visual/collision meshes трассы и параметры Marauder:
  mass, box shape pose, local center of mass, четыре wheel position, radius,
  suspension travel/spring/damper, driven/steering flags, brake torque,
  differential ratio, max RPM и torque;
- `garage.xml` задаёт исходные body/wheel `.r3d` и `marauder.dds`;
- `px*.r3d` material groups дают 591 collision triangle после размещения
  секций карты;
- `Sounds/engine_player_heavy_tom.ogg` и `Sounds/Motor_high02.ogg` берутся из
  sound references автомобиля в `db.xml`.

Старт совпадает с `Race::ResetCarPos`: первая точка первого path плюс 2 по Z,
ориентация направлена на следующую точку path.

## Backend

PhysX 2.8.4 остаётся неизменным в Windows-ветке. Для Apple Silicon используется
Jolt Physics 5.5.0, зафиксированный commit
`23dadd0e603f1b321142d4c74df07fce85064989` и SHA-256 архива. Jolt собирается
статически только для non-Windows original-menu physics preset.

`OriginalVehiclePhysics` не выпускает типы Jolt за границу engine. Adapter:

- переводит игровую Z-up систему в Jolt Y-up;
- строит один static triangle mesh из исходных collision meshes карты;
- сохраняет legacy gravity `-20`, friction/restitution `0.5`;
- создаёт dynamic body с mass/shape pose/center of mass из `db.xml`;
- создаёт четыре настоящих wheel/suspension constraint;
- подаёт torque через driven differential, steering и brake torque;
- выполняет simulation фиксированными шагами 1/120 s;
- использует двусторонние suspension raycasts, поскольку старые PhysX meshes
  содержат winding, рассчитанный на поведение PhysX 2.

Это не численная эмуляция PhysX 2: Jolt contact solver и tire model отличаются.
Однако геометрия, масса, suspension, motor и стартовое состояние происходят из
исходных ресурсов, а не из придуманной игровой механики.

## Renderer, input и audio

`OriginalRaceRenderer` загружает фактические `.r3d` vertices/indices/material
groups и DDS texture atlas, рисует все 52 track placements, кузов и четыре
wheel transform через bgfx/Metal. Камера следует за физическим кузовом.

SDL-независимые actions `Accelerate`, `Brake`, `TurnLeft` и `TurnRight`
управляют Jolt vehicle. `Escape`/`Pause` возвращает в `MainMenu2`. На старте
гонки MusicCat ставится на паузу и включаются два штатных loop автомобиля; при
возврате engine voices останавливаются, MusicCat продолжает сохранённый трек.

## Проверка

```bash
cmake --preset macos-arm64-m9
cmake --build --preset macos-arm64-m9 --target RRR3d -j 8

build/macos-arm64-m9/Debug/RRR3d --physics-smoke-test
build/macos-arm64-m9/Debug/RRR3d --race-render-smoke-test
```

Headless test проверяет provenance ресурсов, контакт suspension, acceleration,
braking, steering и reset. Интеграционный test посылает настоящий
`MenuConfirm` в `Single Player`, рендерит 240 Metal frames и требует контакт
всех колёс и движение автомобиля.

Проверенный результат:

```text
Milestone 9 original race smoke passed: Data/Map/World1/map1.r3dMap,
52 placed track objects, 591 original collision triangles, marauder/Jolt
vehicle acceleration, braking, steering, suspension contacts, and trace reset

Milestone 9 original Single Player/map1/marauder/Jolt/bgfx/Metal smoke test
completed after 240 frames; max speed 11.4956, wheel contacts 4
```

## Граница Milestone 9

Перенесён запускаемый physics race slice, но не полный Windows race mode. Пока
нет AI-соперников, lap/checkpoint state machine, HUD, оружия, damage, bonuses,
decorations/effects, weather, spatial audio и остальных карт/автомобилей.
`ctDecoration` также ещё не рисуется, поэтому M9 визуализирует исходную трассу
и Marauder, но не весь scenery Windows-версии. Эти пункты нельзя считать
готовыми или заменять procedural аналогами.

Перед заявлением physics parity нужен записанный Windows/PhysX reference replay
для acceleration, braking, turning, suspension, collision, slope, jump/fall и
reset. Следующее безопасное расширение — перенести `ctDecoration`/material
mapping первой карты, затем lap/checkpoint/HUD и только после этого AI.
