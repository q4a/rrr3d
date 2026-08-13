# Аудит порта RRR3D / Motor Rock на macOS arm64

Аудит выполнен для исходной ветки `dxvk-glm`, commit
`bfbde0d245dcdb201640be9219f7bad58b2b0ed2`. Поиск выполнялся по отслеживаемым
файлам Git, до изменения платформенного слоя. Количества ниже нужны как
ориентир для декомпозиции, а не как оценка трудоёмкости: одно употребление
`HWND` может протянуть Windows-тип через несколько публичных интерфейсов.

Обновление после Milestone 3: `Rock3dEngine`, `Rock3dGame` и `RRR3d` теперь
собираются как arm64 portable targets с отключёнными renderer, physics,
network, video и Steam. Это не отменяет найденные ниже blockers: legacy
engine/game source sets изолированы явными CMake manifests, а недоступные API
не возвращают фиктивные runtime-объекты.

## 1. Build system

Исходный CMake фактически поддерживал Windows и незавершённый Linux-путь:

- платформа выбиралась преимущественно через `MSVC`, а не через `WIN32`,
  `APPLE` и Linux;
- на любом не-MSVC хосте безусловно добавлялся `ExternalProject` для
  `dxvk-native` с плавающим `GIT_TAG origin/master` и Linux-расширением `.so`;
- `XPlatform` не был добавлен из `src/CMakeLists.txt`, поэтому исходная
  конфигурация завершалась, но целей `XPlatform`, `LexStd` и `MathLib` в
  ожидаемом базовом графе не было;
- include-директории задавались глобально; зависимости между библиотеками
  выражались не полностью;
- игра и редактор добавлялись без платформенного разделения, а Windows
  executable создавался через `add_executable(... WIN32 ...)`;
- `MapEditor` содержит MFC и не может быть целью macOS;
- Windows-библиотеки передавались по логическим именам без расширения:
  `d3d9`, `d3dx9`, `dxguid`, `dxerr`, `strmiids`, `winmm`, `X3daudio`,
  `xinput`, `Iphlpapi`, `PhysXLoader`, `PhysXCooking`, `libogg_static`,
  `libvorbis_static`, `libvorbisfile_static` и конфигурационные варианты
  `tinyxml_STL`;
- Steam API включён исходниками, но явная CMake-зависимость отсутствует;
- CI (`.github/workflows/ci.yml`) работает только на `windows-2019`, только
  для x86, загружает `extern.7z` из release `1.3.1-dev3` и использует старые
  поколения GitHub Actions.

В Git нет подмодулей, каталога `extern` и игровых данных. Значит, чистого
клонирования недостаточно даже для исторической Windows-сборки. Проверенный
архив `extern.7z` содержит `boost`, `directx`, `glm`, `ogg`, `physx`, `steam`,
`tinyxml` и `vorbis`; это отдельный бинарный набор, а не часть репозитория.

После Milestone 3 CMake разделяет `WIN32`, `APPLE` и Linux, имеет отдельные
опции подсистем и preset `macos-arm64-debug`. В preset входят базовые
библиотеки, portable `Rock3dEngine`/`Rock3dGame` и SDL executable. Полный
legacy gameplay при этом ещё не переносим и намеренно не входит в macOS
source manifests.

## 2. Platform APIs

Прямые включения `windows.h` в исходном состоянии (8 файлов):

- `src/LexStd/header/lslUtility.h`;
- `src/RRR3d/stdafx.h`;
- `src/Rock3dEngine/header/graph/Driver/DriverTypes.h`;
- `src/Rock3dEngine/header/stdafx.h`;
- `src/Rock3dGame/header/stdafx.h`;
- `src/Rock3dGame/header/video/video.h`;
- `src/Rock3dGame/source/video/playback.cpp`;
- `src/XPlatform/header/d3dx9math.h`.

`HWND` встречается 69 раз в 23 файлах:

- entry point: `src/RRR3d/RRR3d.cpp`;
- engine headers: `GraphManager.h`, `D3D9RenderDriver.h`, `DriverTypes.h`,
  `RenderDriver.h`, `Engine.h`;
- engine sources: `GraphManager.cpp`, `D3D9RenderDriver.cpp`, `Engine.cpp`;
- game headers/interfaces: `GameMode.h`, `View.h`, `World.h`,
  `VideoPlayer.h`, `playback.h`, `video.h`, `IView.h`, `IWorld.h`;
- game sources: `GameMode.cpp`, `View.cpp`, `World.cpp`, `VideoPlayer.cpp`,
  `playback.cpp`, `video.cpp`.

Используются также `HINSTANCE`, `HRESULT`, `DWORD`, `WORD`, `BYTE`, `BOOL`,
`UINT`, `LONG`, `RECT`, `POINT`, `MSG`, `WPARAM`, `LPARAM`, `LRESULT`,
`WNDCLASSEX`, `GUID`, `SOCKET`, COM-интерфейсы и Windows-строковые типы.
Нельзя переносить эти typedef глобально: графические типы должны жить у
renderer backend, оконные — у window abstraction, COM — только у Windows video.

Инвентаризация вызываемых Win32-функций выявила, в частности:

| API | Число строк | Назначение |
| --- | ---: | --- |
| `AdjustWindowRect` | 2 | размер окна |
| `RegisterClassEx`, `CreateWindowEx` | 1 + 1 | создание окна |
| `ShowWindow`, `UpdateWindow`, `SetWindowPos`, `SetWindowLong` | 1 + 4 + 13 + 2 | управление окном |
| `PeekMessage`, `TranslateMessage`, `DispatchMessage`, `DefWindowProc` | 1 каждое | event loop |
| `PostQuitMessage` | 1 | завершение |
| `GetClientRect`, `GetSystemMetrics` | 12 + 8 | геометрия |
| `GetCursorPos`, `ScreenToClient`, `ShowCursor`, `SetFocus` | 1 + 5 + 9 + 10 | мышь/focus |
| `GetAsyncKeyState` | 10 | клавиатура |
| `QueryPerformanceCounter`, `QueryPerformanceFrequency` | 18 + 4 | время |
| `GetTickCount`, `Sleep` | 1 + 3 | время/ожидание |
| `GetModuleFileNameW`, `SetCurrentDirectoryW` | 1 + 1 | пути/рабочий каталог |
| `MessageBox` | 7 | ошибки/UI |
| `CoInitializeEx`, `CoCreateInstance` | 1 + 2 | COM/DirectShow |

`__declspec` встречается 4 раза в `src/NetLib/header/NetCommon.h` и
`src/Rock3dGame/include/Rock3dGame.h`. Исходных вызовов `LoadLibrary`,
`GetProcAddress` или `FreeLibrary` не найдено: явных точек загрузки DLL нет,
динамические библиотеки приходили через обычную линковку. Новый `XPlatform`
локализует загрузку shared libraries через `dlopen`/`dlsym` на Unix и
Win32 API на Windows.

Историческая Windows точка входа, оконный класс, `CreateWindowEx`, message loop
и обработка `WM_KEYDOWN`, `WM_MOUSEMOVE` остаются в `src/RRR3d/RRR3d.cpp`.
На macOS эта граница уже заменена отдельным SDL3/Cocoa entry point; Windows
файл в target не попадает.

## 3. Renderer

Широкий поиск D3D9-токенов (`IDirect3D*`, `Direct3DCreate9`, `D3D*`) даёт
1104 строки в 79 файлах; `D3DX*` — 129 строк в 17 файлах. Поэтому renderer
не изолирован одним классом.

Создание D3D9-устройства сосредоточено в
`src/Rock3dEngine/source/graph/Driver/D3D9RenderDriver.cpp`; управление идёт
через `GraphManager` и `graph/Engine`. Публичные интерфейсы передают `HWND` и
D3D-типы, поэтому граница backend пока протекает в game/engine code.

Используются D3DX effects/shaders, матрицы и векторы, загрузка изображений,
font/sprite helpers. Ресурсы включают `.fx` и `.dds`. Одного отображения D3D9
на Vulkan недостаточно, пока не решён D3DX-слой.

Цепочка `D3D9 -> DXVK Native -> Vulkan -> MoltenVK -> Metal` на этом этапе
не проверялась: базовый проект до renderer ещё не компилировался на macOS.
`RRR3D_ENABLE_DXVK_NATIVE=OFF` отделяет эксперимент. Оценивать цепочку следует
отдельным коротким spike; при несовместимости не расширять его до бесконечного
форка DXVK, а сравнить стоимость `bgfx` и собственного Metal backend.

Добавленные для `MathLib` минимальные структуры `D3DMATRIX`, `D3DVECTOR`,
`D3DCOLORVALUE` и `D3DCOLOR` — только совместимый математический ABI. Они не
являются renderer implementation.

## 4. Input

Windows message input находится в `src/RRR3d/RRR3d.cpp`. Polling через
`GetAsyncKeyState` используется в game code. XInput находится в одном файле —
`src/Rock3dGame/source/ControlManager.cpp` (`XInputGetState`,
`XInputGetKeystroke`, `XINPUT_STATE`).

SDL3 event loop и перевод keyboard/mouse events реализованы в Milestone 2,
включая key repeat, UTF-8 text и focus. События пока заканчиваются в diagnostic
adapter: подключение к существующим gameplay actions и SDL Gamepad остаётся
будущей работой. Прямая замена числовых Virtual-Key codes по-прежнему опасна.

## 5. Audio

WinMM/`PlaySound` и связанные упоминания дают 16 строк в 5 файлах; собственно
`PlaySound` встречается 13 раз в трёх игровых файлах. Внешний архив содержит
Ogg/Vorbis, а ресурсы используют `.ogg`.

На macOS Windows `winmm` и `X3daudio` недоступны. Плановая замена — SDL3 Audio
с сохранением существующего декодирования Ogg/Vorbis либо с отдельным
проверенным декодером. На первом этапе `RRR3D_ENABLE_VIDEO=OFF` не заменяет
аудио игры; полноценная audio option/target boundary потребуется позже.

## 6. Video

DirectShow/COM-видеопуть распределён по:

- `src/Rock3dGame/header/video/VideoPlayer.h`;
- `src/Rock3dGame/header/video/playback.h`;
- `src/Rock3dGame/header/video/video.h`;
- `src/Rock3dGame/source/video/VideoPlayer.cpp`;
- `src/Rock3dGame/source/video/playback.cpp`;
- `src/Rock3dGame/source/video/video.cpp`.

Он использует `IGraphBuilder`, `IMediaControl`, `IMediaEvent`, `IVideoWindow`,
`IMediaSeeking`, COM и `strmiids`. Подсистема отделена опцией
`RRR3D_ENABLE_VIDEO=OFF`; portable game capability явно сообщает, что video
недоступно. Семантика пропуска заставок/роликов появится вместе с настоящим
portable world — текущая API-заглушка мир не создаёт.

## 7. Network

Низкоуровневый `NetLib` оказался уже построен поверх Boost.Asio; Windows-only
частью была прежде всего `GetAdaptersAddresses`/`Iphlpapi`. В follow-up он
собирается arm64 как static library с pinned Boost 1.85.0/GLM 1.0.1,
использует `getifaddrs` для IPv4 adapters и современный Asio executor/restart
contract. Исправлены исходные дефекты dynamic `BitStream`, LP64 `long`,
command size, reconnect и инициализации wire headers.

Preset `macos-arm64-network` проверяет настоящий TCP connect/command/reconnect,
UDP datagram и Windows-compatible размеры/layout заголовков на loopback.
M9/M10/Release presets теперь включают transport и source-derived
`OriginalNetworkSession`: перенесены `NetGame` lifecycle, порт/sync rate,
adapter list, `NetworkFrame`, server/client type frames, LAN browser и ручной
IP. `NetRace`/`NetPlayer` модели репликации всё ещё не подключены, поэтому TCP
handshake не выдаётся за готовую сетевую гонку. Steam P2P остаётся отдельным
backend и не смешивается с socket portability.

## 8. Physics

Поиск `Nx*`/`PhysX` даёт 707 строк в 24 файлах. Основные места:

- `src/Rock3dEngine/header/px` и `src/Rock3dEngine/source/px`;
- `Physx`/`Stream` и `FxManager` в engine;
- игровые world/car/object/controller types в `Rock3dGame`.

Архив зависимостей содержит PhysX SDK 2.8.4
(`NX_SDK_VERSION_NUMBER=284`) и готовые Windows `PhysXLoader`/
`PhysXCooking` библиотеки. Нативного arm64 macOS варианта этой версии нет в
проекте. `RRR3D_ENABLE_PHYSICS=OFF` предотвращает подключение этих `.lib`, но
game code всё ещё требует архитектурной границы или совместимой реализации.
Физику нельзя без отдельного решения заменить, не изменив gameplay и форматы.

## 9. Resources

В исходном Windows startup выполняется
`SetCurrentDirectoryW(lsl::GetAppPath().c_str())`. На non-Windows
`GetAppPath()` был пустым. `ResourceManager` добавляет префикс `Data\\`, а по
исходникам найдено более 3115 строковых литералов с обратным слешем в 34
файлах. Загружаются `.r3d`, `.dds`, `.ogg`, XML и D3DX `.fx`.

Абсолютные Windows-пути:

- `src/Rock3dGame/source/game/World.cpp`: `C:\\dump.txt`;
- `src/MapEditor/ViewTree.cpp`: `C:\\1.txt`;
- `src/MapEditor/PropertiesWnd.cpp`: `c:\\`;
- `src/tools/rock3dExprot.ms`: `c:\\*.r3d`;
- manifest strings MapEditor содержат Windows architecture identifiers, но не
  являются файловыми путями runtime.

В отслеживаемом Git нет `Data` и иных игровых assets. Следовательно,
`RRR3d.app` нельзя упаковать из одного репозитория; источник и права на данные
нужно определить до bundle milestone.

Коллизий двух отслеживаемых путей, различающихся только регистром, не найдено.
Однако обнаружено уже проявившееся несовпадение include:
`lslSerialFileXml.h` против фактического `lslSerialFileXML.h`; оно исправлено в
затронутом базовом target. Проверку регистра нужно оставить в CI.

Новый `XPlatform` предоставляет разделители/нормализацию путей, путь к
executable, resources, Application Support и Logs. Это фундамент, а не полная
миграция ресурсов: массовые `Data\\...` ещё не заменены и bundle ещё не создан.

## 10. Third-party dependencies

В Git зависимостей нет. Отдельный исторический `extern.7z` содержит:

| Зависимость | Версия/состояние архива | Применение |
| --- | --- | --- |
| Boost | 1.59.0, VC140 libs | общие utilities/network |
| DirectX SDK | Windows headers/libs | D3D9, D3DX, XInput, XAudio helpers |
| GLM | 0.9.9.8 | математика ветки `dxvk-glm` |
| libogg | 1.3.5 | Ogg container |
| libvorbis | 1.3.7 | audio decode |
| PhysX | 2.8.4, Windows binaries | physics/cooking |
| Steamworks | версия не зафиксирована в CMake | Steam/P2P |
| TinyXML 1 | 2.6.2 | XML serialization |

Для базового Milestone 1 реально требуются только GLM и TinyXML 1.
Milestones 2 и 3 дополнительно собирают SDL3 3.4.12 статически из pinned source,
чтобы сохранить deployment target macOS 13. Конфигурация на проверенной машине
использует Homebrew GLM 1.0.3 и собирает TinyXML 2.6.2 из URL с SHA-256
`15bdfdcec58a7da30adc87ac2b078e4417dbe5392f3afb719f9ba6d062645593`.
TinyXML-2 не совместим с используемым API и не является заменой.

Версии, источники, лицензии, target names и bundle policy вынесены в
`docs/MACOS_DEPENDENCIES.md`.

## 11. Critical blockers

1. Renderer и публичные интерфейсы глубоко зависят от D3D9/D3DX/`HWND`; путь
   DXVK Native + MoltenVK ещё не доказан на arm64.
2. На момент Milestone 3 игровые данные отсутствовали. В Milestone 6 локальный
   набор импортирован и загрузка проверена, но источник и права на
   распространение assets всё ещё нужно согласовать до публикации полного
   `.app`.
3. PhysX 2.8.4 представлен только устаревшими Windows binaries; замена влияет
   на детерминизм и gameplay.
4. DirectShow, WinMM/XAudio, XInput и Win32 event loop требуют отдельных
   portable backend/заглушек.
5. `IView`/`IWorld` больше не протаскивают обязательный `HWND`, но внутренние
   legacy game/engine headers всё ещё передают Win32 и D3D-типы между слоями.
6. Steamworks version/runtime и macOS redistribution policy не зафиксированы.
7. Часть исходников хранится в CP1251. Затронутые базовые файлы переведены в
   UTF-8, но остальной репозиторий требует контролируемой, отдельной миграции.
8. Windows CI зависит от внешнего mutable binary bundle и не проверяет, что
   macOS-изменения сохраняют Windows build.

## 12. Recommended order of work

1. Сохранить зелёным текущий milestone: CMake + `XPlatform`, `LexStd`,
   `MathLib` на macOS arm64.
2. Расширить platform contracts и убрать `HWND`/Win32-типы с границ
   game/engine, не подключая renderer.
3. Добавить SDL3 window/event/input shell и нативную `.app`-цель без gameplay.
4. Отдельным time-boxed spike проверить D3D9 -> DXVK Native -> Vulkan ->
   MoltenVK; документировать D3DX gaps и минимальный triangle/clear path.
5. По результату выбрать DXVK, `bgfx` или Metal backend и только затем
   переносить renderer/resource shaders.
6. Перенести audio/gamepad; реализовать video-disabled stub.
7. Отдельно решить PhysX 2.8.4 compatibility/replacement и провести gameplay
   regression tests.
8. Перенести network и Steam как независимые optional backends.
9. Нормализовать resource paths, определить источник `Data`, собрать и
   подписать `RRR3d.app`.
10. Добавить macOS arm64 CI и Windows regression CI.

Обновление после Milestone 3: пункты 1 и 3 выполнены, а публичная оконная
граница из пункта 2 очищена настолько, чтобы arm64 portable targets
компилировались и линковались. Следующий шаг — строго отдельный time-boxed
renderer spike из пункта 4; он не должен преждевременно включать весь legacy
gameplay или PhysX.
