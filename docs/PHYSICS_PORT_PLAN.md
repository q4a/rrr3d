# Milestone 9.1: перенос оригинальной гонки

> Продолжение renderer/particle backend выполнено в
> `docs/MILESTONE_9_2.md`. Ограничения ниже фиксируют границу именно M9.1.

## Результат

Milestone 9.1 продолжает исправленный путь M5–M9: это перенос исходной
логики и данных Motor Rock на portable C++/bgfx/Metal/Jolt, а не новая игра
по мотивам оригинала. Старые `PortableRace` и procedural-трасса не входят в
preset `macos-arm64-m9`.

`Single Player` загружает турнир, профиль, трассу, автомобили, workshop,
окружение, HUD, звук и эффекты из оригинальных XML, `.r3d`, DDS/PNG и Ogg.
Параметрами запуска можно выбрать любую из 88 турнирных трасс, 17 гаражных
машин и штатные варианты погоды.

## Источники и профиль

Portable loader использует оригинальные:

- `tournamet.xml`, `garage.xml`, `workshop.xml`, `db.xml` и карты
  `Data/Map/*/*.r3dMap`;
- visual/collision records, составные и разрушаемые decorations, material
  records, источники света и вложенные `includeList`;
- garage/workshop характеристики, четыре weapon slots, projectile variants,
  mines, hyper, droid и reflector;
- `UserInfo.xml`, `config.xml` и `achievment.xml`.

Профиль сохраняет выбранные машину, персонажа, планету и pass, деньги,
garage/workshop upgrades, музыку, язык, управление, сетевой профиль и полное
состояние achievements. При первом запуске определения achievements берутся
из штатного XML, а пользовательский файл накладывает накопленные значения.
Запись выполняется в Application Support, ресурсы игры не изменяются.

Windows-пути с несовпадающим регистром разрешаются только при единственном
однозначном case-insensitive совпадении. `ResourceFileSystem` по-прежнему
запрещает абсолютные пути, выход из game-data, неоднозначные совпадения и
слишком большие файлы.

## Физика и состояние гонки

Windows остаётся на PhysX 2.8.4. Apple Silicon использует Jolt Physics 5.5.0
через SDL/renderer-независимый `OriginalVehiclePhysics`:

- Z-up игровая система преобразуется в Y-up Jolt;
- static triangle collision строится из штатных `.r3d`;
- параметры body, center of mass, колёс, suspension, differential, brakes,
  RPM и torque берутся из `db.xml`;
- игрок и все соперники имеют отдельные Jolt vehicles;
- simulation выполняется фиксированными шагами 1/120 s;
- стартовая решётка и reset/respawn повторяют исходные trace и формулы
  `Race::ResetCarPos`.

`OriginalRaceSession` переносит countdown, pause/resume, ordered checkpoints,
laps, place, wrong-way, finish, reset и respawn. В нём работают исходные
roster/loadout соперников, trace-based AI, collision/contact damage,
разрушение decorations, bonuses, shield, death/kill flow и achievements.

Это поведенческий перенос, но не численная эмуляция PhysX. Contact solver,
tire model и порядок разрешения ограничений Jolt отличаются, поэтому
побитовая идентичность траекторий Windows/PhysX не заявляется.

## Workshop, оружие и поддержка

Загружается полный workshop catalog и четыре оригинальных слота. Portable
session учитывает ammunition, cooldown, цены, upgrades, damage/radius,
скорость и lifetime projectile, multi-projectile, ray/attached weapons,
homing torpedo/impulse, frost, laser, fire и дробилку.

Перенесены:

- projectile models и их primary/secondary/tertiary visuals;
- oil, MineRip и остальные mine descriptors, включая grow/owner-lock;
- hyper/acceleration, spring velocity, droid repair и reflector;
- shield, medpack, ammunition, money и speed bonuses;
- AI fire, damage, kills/deaths и source-derived loadouts.

`enableMineBug` из оригинальной конфигурации сохраняет legacy owner-lock для
соответствующих типов мин. `springBorders` в исходнике только передаётся
внутрь старого PhysX wrapper; отдельная выдуманная механика для него не
добавлялась.

## Камера, HUD и mini-map

Камера перенесена по исходным режимам и константам `CameraManager`:
`pcThirdPerson` (названный `Rear view` в UI) и `pcIsometric`, target offset,
speed pull-back, FOV, near/far planes и переключение действий ввода.

HUD использует оригинальные GUI textures, bitmap font и layout-данные. Он
отображает:

- place, lap, speed, RPM, life, ammunition, money и четыре weapon slots;
- mines/hyper/support state, countdown, wrong-way, pause и finish;
- checkpoint/bonus/damage/kill/achievement notifications;
- mini-map по полной trace карты с позициями игрока и соперников.

Координаты mini-map и динамических индикаторов вычисляются из исходной trace,
а не из придуманной овальной схемы.

## Materials, окружение и effect graph

Renderer загружает все track/decor/bonus/car/wheel visual nodes, исходные
material groups и texture atlases. Перенесены ambient/sun/fog, alpha test с
исходным reference, blend/cull/depth flags, specular/shininess, emissive,
`ignoreFog`, UV atlas animation и map/world material mapping.

Environment loader охватывает World1–World5, Crush/Bonus exceptions, sky,
weather, grass, water, ground fog, magma, rain и исходные высоты/скорости
прокрутки. Разрушаемые decorations исчезают и создают исходные эффекты.

Generic parser переносит source `ntSprite`, `ntPlane`, nested `includeList`
и particle emitters: limits, lifetime/start ranges, density, position/scale
ranges, velocity, acceleration/gravity, world coordinate flag, material и
time/distance triggers. Эти данные используются для weapon impacts, trails,
frost/laser endpoints, mines, hyper и environment effects. Portable emitter
детерминированно ограничивает число одновременно рисуемых частиц; точный
D3D9 sorting/billboarding и внутренний scheduler legacy particle manager
не воспроизводятся побитово.

Metal backend пока не имеет отдельных legacy HDR, planar-reflection и
shadow-map passes. Их source-параметры и флаги загружаются, но текущие
отражения/тени являются portable renderer approximations, а не заявленной
графической parity.

## Звук и управление

SDL action layer поддерживает keyboard, mouse и gamepad: непрерывные throttle,
brake/steering, reset, pause, переключение camera modes, выбор каждого weapon
slot, next slot, fire, mine и hyper. Droid/reflector активируются через
штатное действие выбранного workshop slot.

MusicCat сохраняет shuffle, текущий track/cursor и pause, воспроизводит все
три оригинальных menu-трека и декодирует/preload их фоновым worker.

В гонке engine idle/RPM loops следуют физическим оборотам всех машин.
Перенесены исходные weapon, impact, damage, collision, bonus, lap, finish и
commentator Ogg. Для движущихся источников применяются distance attenuation,
stereo pan и pitch/doppler approximation. Legacy X3DAudio cones, obstruction
и его точная DSP-матрица не входят в portable backend.

## Финальная проверка

После завершения всего переноса M9.1 выполнен один финальный цикл:

```bash
cmake --preset macos-arm64-m9
cmake --build --preset macos-arm64-m9 --target RRR3d -j 8

build/macos-arm64-m9/Debug/RRR3d --physics-smoke-test
SDL_AUDIODRIVER=dummy \
  build/macos-arm64-m9/Debug/RRR3d --audio-smoke-test
build/macos-arm64-m9/Debug/RRR3d --race-render-smoke-test
build/macos-arm64-m9/Debug/RRR3d --verify-resources
```

Configure и arm64 Debug build завершились успешно. Physics/session smoke
проверил 1175 исходных collision triangles, Jolt acceleration/braking/
steering/suspension, workshop mobility, countdown, checkpoints/laps/finish,
weapon/damage, bonus и respawn. Audio/MusicCat smoke проверил 182 Ogg,
background decode, все три menu tracks, race sounds и lifecycle mixer.

Resource sweep разрешил все 88 tournament tracks и все 17 garage cars.
Оконный smoke прошёл реальный `MainMenu2 -> Single Player`, 240 кадров
bgfx/Metal race, движение Marauder и контакты всех четырёх колёс.

По прямому требованию для M9.1 Windows/Parallels A/B не выполнялся. Весь
финальный цикл запущен только после завершения кода и документации.

## Честная граница M9.1

Перенесены данные и независимая от Windows логика, найденные в исходной
гонке, профиле, workshop, HUD, camera, material/effect и audio слоях. В
portable target не включаются Windows-only D3D9/PhysX/XAudio2/X3DAudio,
Steam, network и video runtime.

Оставшиеся различия относятся не к подмене ресурсов или выдуманной игровой
механике, а к backend-эквивалентности:

- Jolt не даёт численно идентичный PhysX 2.8 solver/tire model;
- bgfx/Metal ещё требует отдельных HDR/reflection/shadow-map render passes;
- portable particle renderer использует исходные параметры, но не точный
  legacy D3D9 particle scheduler/sorting;
- SDL audio не воспроизводит точную X3DAudio DSP-матрицу.

Эти ограничения не скрываются и не замещаются процедурными аналогами.
