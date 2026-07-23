# Original game resources and main menu (Milestone 6)

## Результат

Milestone 6 повторно выполнен с оригинальными ресурсами Motor Rock. В
`resources/game-data` находятся полное дерево `Data`, семь статических XML,
portable manifest/menu model и небольшой bitmap font. Всего импортировано
1196 оригинальных файлов, 553107397 байт.

Основной `RRR3d` до создания SDL window:

1. монтирует `game-data` относительно executable;
2. проверяет manifest версии `rrr3d-game-data-v2`;
3. сверяет точный путь и размер каждого файла с `legacy-assets.catalog`;
4. декодирует оригинальные DDS/PNG menu assets в RGBA8 через pinned `bimg`;
5. проверяет font/menu model и только после этого запускает bgfx/Metal.

Legacy `MainMenu2` по-прежнему нельзя собрать отдельно от D3D9 GUI, audio,
network и `World`, поэтому его state/layout воспроизведены на portable
renderer без фиктивных engine objects.

## Импорт

Исходный каталог не изменяется. Воспроизводимый импорт запускается так:

```bash
scripts/import_game_resources.sh '/path/to/Motor Rock'
```

Скрипт переносит всё дерево `Data` и конфигурации `game.xml`, `db.xml`,
`garage.xml`, `race.xml`, `workshop.xml`, `tournamet.xml`, `achievment.xml`.
После копирования он генерирует отсортированный size-catalog.

Намеренно исключены:

- Windows `.exe`/`.dll` и CUDA/PhysX binaries;
- `appLog.txt`;
- `Profile` и `user.xml` как пользовательские данные;
- `.DS_Store` и `Thumbs.db`.

Скрипт отказывается смешивать импорт с уже существующим `Data`, чтобы старые
файлы из другого выпуска не оставались незамеченными.

## Поиск каталогов

`r3d::resource::ResourceFileSystem` получает корневой каталог явно. Основной
executable выбирает его так:

1. `--data-dir=<path>` — developer/test override;
2. иначе `<resource_directory>/game-data`;
3. для обычного Mach-O это каталог executable;
4. для M10 `.app` — `Contents/Resources`.

CMake копирует весь пакет рядом с обычным M6–M9 executable, а bundle presets
помещают его в `RRR3d.app/Contents/Resources/game-data`. Поэтому запуск не
зависит от current working directory или расположения `.app`.

Writable-каталоги отделены от read-only ресурсов:

- `~/Library/Application Support/RRR3d` — save/config data;
- `~/Library/Logs/RRR3d` — logs.

Статические XML в package являются исходными defaults. Когда их legacy save
paths будут подключены, запись должна направляться в Application Support, а
не обратно в package.

## Правила mount и catalog

Resource filesystem:

- принимает только относительные virtual paths;
- нормализует legacy `\` в `/`;
- отклоняет absolute paths, drive letters и `..`;
- проверяет точный регистр каждого компонента даже на case-insensitive APFS;
- после symlink resolution проверяет containment внутри `game-data`;
- принимает только regular files;
- ограничивает binary resource 256 MiB и text resource 16 MiB;
- выдаёт конкретный `ResourceError` вместо пустого asset.

Каждая строка `legacy-assets.catalog` имеет вид `size<TAB>path`. Loader
проверяет все 1196 entries, отсутствие duplicates, фактический размер и
обязательные menu/language/database files. Это обнаруживает неполную копию,
неверный регистр и случайное смешивание наборов до renderer initialization.

## Menu resources и rendering

Исправленный original-menu path использует те же ресурсы и layout, что legacy
`MainMenu2`:

- `Data/GUI/mainFrame.dds` — DXT1 background 1920×1100;
- `Data/GUI/topPanel5.png` — прозрачный logo panel 1920×220;
- `Data/GUI/bottomPanel5.png` — нижняя панель, загружена для следующих states;
- `Data/GUI/mainItemSel5.png` — selection 350×48;
- строки главного state соответствуют оригинальным English resources:
  `Single Player`, `Multiplayer`, `Settings`, `Authors`, `Exit`.

`bimg_decode` распаковывает DDS/PNG в RGBA8. UI pass использует virtual canvas
1920×1100, orthographic camera, alpha blending, depth ordering и Retina
backbuffer. Текст создаётся CoreText с запросом штатного Verdana и строками из
оригинальной English/Russian localization.

Up/Down меняют выбранный пункт, Enter активирует, Escape закрывает окно.
`EXIT` завершает программу. В исправленном Milestone 9 `SINGLE PLAYER`
запускает первую гонку `Data/Map/World1/map1.r3dMap` на Marauder; map/car/
physics data берутся из `tournamet.xml`, `db.xml` и `garage.xml`.

## Сборка и проверка

```bash
scripts/build_bgfx_macos.sh
cmake --preset macos-arm64-m6
cmake --build --preset macos-arm64-m6 -j 8

build/macos-arm64-m6/Debug/RRR3d --verify-resources
build/macos-arm64-m6/Debug/RRR3d --smoke-test-frames=120
build/macos-arm64-m6/Debug/RRR3d
```

Проверенный вывод:

```text
Resource validation: 5 menu items, 1920x1100 background, 1196 original assets (553107397 bytes)
RRR3D renderer backend: bgfx/Metal
Main menu: original DDS/PNG artwork, portable font, selection state
Milestone 6 menu smoke test completed after 120 frames
```

Визуально проверены DXT1 background, PNG alpha, original logo/selection,
normal/selected text colors и переход selection с `SINGLE PLAYER` на
`MULTIPLAYER`.

## Продолжение в Milestone 7

Milestone 7 выполнен: SDL keyboard/mouse/gamepad переводятся в portable action
map, а `MenuUp`, `MenuDown`, `MenuConfirm`, `MenuBack` подключены к текущей
menu model. Dead zones и hot-plug описаны в `docs/PORTABLE_INPUT.md`.

Наличие всего `Data` теперь позволяет отдельно переносить audio decoders,
`.r3d`/`.r3dMap` parsers и gameplay world без повторного поиска ресурсов.

## Продолжение в Milestone 8

Milestone 8 выполнен: portable filesystem отдаёт original Ogg resources
защищённому exact-case resolver'у, libvorbis декодирует музыку/UI/gameplay
effects, а SDL3/CoreAudio воспроизводит их в текущем меню. Формат package и
importer менять не потребовалось. Архитектура и проверки описаны в
`docs/PORTABLE_AUDIO.md`.

## Подключение в Milestone 9

Race loader выбирает первый track/car через `tournamet.xml`, читает 52
`ctTrack` placement и trace из `Data/Map/World1/map1.r3dMap`, разрешает записи
через `db.xml`/`garage.xml` и загружает исходные visual/collision `.r3d` через
тот же exact-case filesystem. Jolt получает фактические triangle meshes, а
bgfx/Metal — фактические vertex/index/material groups и DDS atlas. Детали и
ограничения описаны в `docs/PHYSICS_PORT_PLAN.md`.

## Упаковка в Milestone 10

Presets `macos-arm64-m10` и `macos-arm64-release` кладут тот же проверенный
пакет в `Contents/Resources/game-data`. `XPlatform::resource_directory()`
распознаёт `Contents/MacOS/RRR3d`, поднимается к `Contents/Resources` и не
использует current working directory. Перемещённый bundle прошёл resource и
physics smoke через LaunchServices. Сборка и подпись описаны в
`docs/BUILDING_MACOS.md`.
