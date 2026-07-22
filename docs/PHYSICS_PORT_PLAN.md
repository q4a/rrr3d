# Physics port plan and Milestone 9 result

## Результат M9

Milestone 9 добавляет запускаемую однокруговую гонку на macOS Apple Silicon.
Пункт `SINGLE PLAYER` теперь переводит меню в race scene. Автомобиль получает
непрерывные значения `Accelerate`, `Brake`, `TurnLeft` и `TurnRight` из
переносимого action layer, движется с фиксированным шагом 120 Hz, сталкивается
с двумя границами трассы, проходит три checkpoint и фиксирует финиш.

Сцена использует:

- границы и 18 размещений секций из оригинального
  `Data/Map/debugTrack.r3dMap`;
- проверенные исходные visual meshes `track1.r3d`, `track2.r3d`, `buggi.r3d`;
- проверенные collision meshes `pxTrack1.r3d` и `pxTrack2.r3d`;
- bgfx/Metal для road ribbon, ограждений, машины, камеры и маркеров;
- `engine_player_heavy_mot.ogg`, `carcrash05.ogg`, `fireGun.ogg` и UI audio
  через уже перенесённый SDL3/CoreAudio backend.

Текущий renderer строит переносимое представление трассы из карты. Бинарные
`.r3d` meshes валидируются и используются как источник геометрического
контракта, но их полный visual-material parser ещё не подключён. Это отделяет
M9 physics vertical slice от дальнейшего переноса legacy scene graph.

## Аудит legacy PhysX

В Windows-коде используется NVIDIA PhysX 2.8.4
(`NX_SDK_VERSION_NUMBER=284`). Поиск PhysX/Nx API даёт 717 совпадений в 27
файлах `Rock3dEngine` и `Rock3dGame`. macOS target не линкует
`PhysXLoader.lib`, `PhysXCooking.lib` или другие Windows binaries.

### Scene и simulation

`Rock3dEngine/source/px/Physx.cpp` создаёт один `NxScene` через
`NxPhysicsSDK::createScene`:

- Z-up (`upAxis=2`), gravity `(0, 0, -20)`;
- variable timestep;
- default material: static/dynamic friction 0.5, restitution 0.5;
- collision groups для car, wheel, shot, track, transparent track и border;
- `NxUserContactModify`, `NxUserContactReport` и `NxUserNotify` callbacks;
- synchronous `simulate(deltaTime)` + `fetchResults`.

Gameplay также выполняет scene raycasts, меняет actor-pair/group filter flags
и напрямую читает/записывает linear/angular velocity и momentum.

### Shapes и cooking

Legacy wrapper поддерживает plane, box, sphere, capsule, triangle mesh,
convex mesh и wheel shapes. `NxTriangleMeshDesc` заполняется vertex/index
данными из custom `.r3d` mesh, после чего `NxCookTriangleMesh` или
`NxCookConvexMesh` готовит буфер в памяти и SDK создаёт runtime mesh. То есть
`pxTrack*.r3d` — custom collision geometry, а не готовая платформенная PhysX
serialization. В импортированных ресурсах найдено 33 таких `px*.r3d` track
meshes; это позволяет позже готовить их для другого backend без декодирования
старого PhysX binary stream.

### Vehicle API

`GameCar`, `CarWheel` и `RockCar` используют `NxWheelShapeDesc`/
`NxWheelShape`, wheel-contact callback, suspension spring/damper/travel,
longitudinal/lateral tire force functions, motor torque, steer angle,
inverse wheel mass и material index контакта. Car/game object code напрямую
работает с `NxActor`, forces, impulses, torque, damping, mass pose, wheel
contact и triangle-mesh raycast.

### Связность с gameplay

Зависимость не ограничена engine wrapper. `GameBase`, `GameObject`,
`GameCar`, `AIPlayer`, `Player`, `Weapon`, `Logic` и network replication
обращаются к `NxActor`/`NxScene` напрямую. Механическая замена заголовков на
PhysX 5 или Jolt без промежуточной границы потребовала бы переписать
автомобиль, оружие, AI и replication одновременно.

## Рассмотренные стратегии

| Вариант | Плюсы | Риск/стоимость | Решение M9 |
| --- | --- | --- | --- |
| Современный PhysX wrapper | Близкая терминология shapes/actors/cooking | PhysX 2 wheel API удалён; direct Nx coupling всё равно переписывается | Не выбран |
| Jolt Physics | Нативный arm64, активный проект, хорошие rigid bodies/queries | Нужен отдельный vehicle controller и importer; большая новая dependency | Кандидат после vertical slice |
| Минимальная custom vehicle physics | Малый проверяемый объём, нет внешней ABI, быстро даёт runnable race | Не воспроизводит многотельную suspension и weapon dynamics | Выбран для M9 |
| Временный scene-loading stub | Позволяет проверить карты | Нет движения/столкновений, критерий M9 не выполняется | Отклонён |

Выбранная реализация находится за SDL/Metal-независимым
`physics/PhysicsBackend.h`. Gameplay видит только `TrackGeometry`,
`VehicleInput`, `VehicleState`, `TrackSample` и `PhysicsWorld`. Поэтому
минимальный backend можно заменить Jolt или современным PhysX, не меняя
input, menu, audio и renderer race scene.

## Реализованная модель

- stadium-track выводится из min/max координат секций `ctTrack` оригинальной
  debug map;
- nearest-point projection формирует непрерывный collision corridor;
- машина имеет longitudinal acceleration, braking, quadratic drag,
  speed-dependent steering и circular collision footprint;
- при пересечении ограждения позиция возвращается в corridor, normal velocity
  отражается и гасится, событие collision направляется в audio;
- трамплин на нижней прямой проверяет подъём, отрыв, gravity и landing;
- race progress разворачивает cyclic track distance, не засчитывает движение
  назад и требует checkpoints 25/50/75% до финиша;
- simulation делается fixed step 1/120 s с frame accumulator; большой frame
  time ограничен, чтобы после паузы не было physics explosion;
- Tab выполняет безопасный reset на старт, P ставит simulation/audio на
  паузу, Escape возвращает в меню.

## Автоматическая проверка

`--physics-smoke-test` сначала без renderer проверяет:

1. разгон за две секунды;
2. торможение;
3. изменение heading при steering;
4. столкновение с внешним ограждением;
5. take-off и landing на трамплине;
6. reset;
7. одинаковый результат двух fixed-step replays;
8. автономное прохождение checkpoints и однокруговый финиш.

Затем приложение запускает настоящие SDL3/audio/bgfx/Metal subsystems и
рисует 240 кадров race scene. Зафиксированный arm64 результат:

```text
Milestone 9 physics smoke: acceleration, braking, steering, wall collision,
ramp jump/landing, reset, deterministic replay, checkpoints, and one-lap
finish passed; finish 17.8581 s, max 89.7191 km/h, collisions 0
Milestone 9 physics/race/render smoke test completed after 240 frames
```

Команды:

```bash
cmake --preset macos-arm64-m9
cmake --build --preset macos-arm64-m9 --clean-first -j 8

SDL_AUDIO_DRIVER=dummy \
  build/macos-arm64-m9/Debug/RRR3d --physics-smoke-test
build/macos-arm64-m9/Debug/RRR3d --verify-resources
build/macos-arm64-m9/Debug/RRR3d
```

## Windows reference и критерий parity

Задание требует перед сохранением точного поведения снять Windows reference:
position, acceleration, braking, turning, collision, jump, fall, reset и
slope. Рабочая среда M9 — Apple Silicon, Windows executable/совместимый PhysX
runtime и эталонный replay не предоставлены, поэтому численное равенство
legacy PhysX 2.8.4 не заявляется.

M9 закрывает функциональный критерий вертикального среза: на macOS машина
движется по ресурсной трассе, сталкивается с окружением, проходит checkpoint
и завершает гонку. Известные отличия от Windows — single-body car вместо
четырёх `NxWheelShape`, gravity 9.81 вместо 20, процедурная collision corridor
вместо полного triangle mesh и отсутствие weapon rigid bodies. Перед заменой
backend или заявлением release parity нужно снять Windows trace тем же набором
сценариев и сохранить его как versioned test data.
