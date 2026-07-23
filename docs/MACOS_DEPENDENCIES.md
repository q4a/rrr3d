# Зависимости macOS-порта RRR3D

Документ отделяет зависимости, реально используемые текущим arm64 milestone,
от исторического Windows bundle и возможных зависимостей следующих этапов.
Пути Homebrew не зашиты: CMake ищет config package/target, поэтому формулы из
нативного arm64 prefix обнаруживаются через стандартные механизмы CMake.

## Текущий milestone: original-data physics race M9

| Имя | Зафиксированная/проверенная версия | Источник | Лицензия | CMake target | Включать в `.app` |
| --- | --- | --- | --- | --- | --- |
| GLM | требуется >= 1.0; проверена 1.0.3 arm64 | Homebrew `glm` или `glm_DIR`; Windows fallback `extern/glm` | MIT | `glm::glm-header-only` на macOS; `glm::glm` / `rrr3d_glm` на остальных платформах | нет, header-only |
| TinyXML 1 | 2.6.2; SHA-256 `15bdfdcec58a7da30adc87ac2b078e4417dbe5392f3afb719f9ba6d062645593` | [официальный архив SourceForge](https://sourceforge.net/projects/tinyxml/files/tinyxml/2.6.2/) через `FetchContent`; Windows fallback `extern/tinyxml` | zlib | `rrr3d_tinyxml` | статически, notice/license при дистрибуции |
| SDL3 | 3.4.12; SHA-256 `f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7` | [официальный release `libsdl-org/SDL`](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.12), собирается через `FetchContent` | zlib | `SDL3::SDL3-static` | статически; добавить license notice при дистрибуции |
| libogg | 1.3.5; SHA-256 `c4d91be36fc8e54deae7575241e03f4211eb102afb3fc0775fbbc1b740016705` | [официальный Xiph release](https://downloads.xiph.org/releases/ogg/) через `FetchContent` | BSD-3-Clause | `rrr3d_ogg` / `Ogg::ogg` | статически; добавить license notice |
| libvorbis/libvorbisfile | 1.3.7; SHA-256 `b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b` | [официальный Xiph release](https://downloads.xiph.org/releases/vorbis/) через `FetchContent` | BSD-3-Clause | `rrr3d_vorbis`, `rrr3d_vorbisfile` | статически; добавить license notice |
| bgfx | commit `65551a7db19240b4d105f09e665190d196243d92` | `scripts/build_bgfx_macos.sh`, официальный GitHub | BSD-2-Clause | `rrr3d_bgfx` | статически; добавить copyright/license notice |
| bx | commit `5a2b876258ab5843d5e1dfde695b127baf9e354a` | тот же pinned build script | BSD-2-Clause | `rrr3d_bx` | статически; добавить copyright/license notice |
| bimg | commit `da38bface6384cdd7f69733b08fb58e57a63cffa` | тот же pinned build script | BSD-2-Clause | `rrr3d_bimg`, `rrr3d_bimg_decode` | статически; DDS/PNG decode; добавить copyright/license notice |
| Jolt Physics | 5.5.0, commit `23dadd0e603f1b321142d4c74df07fce85064989`, SHA-256 `e4a44a9bfdcdb1e47259b7a66a300d9f31daf1b28ec19617761c3aad7165b979` | официальный GitHub archive через `FetchContent` | MIT | `Jolt` | статически для non-Windows original M9; `JoltPhysics.txt` |
| Apple system libraries | macOS 13 SDK или новее | Xcode Command Line Tools | Apple SDK terms | `${CMAKE_DL_LIBS}` при необходимости | системные, не копировать |

TinyXML-2 не подходит: проект использует API оригинального TinyXML 1. Архив
TinyXML зафиксирован и по версии, и по digest; плавающих branch/tag здесь нет.

Homebrew SDL3 3.4.12 установлен и имеет архитектуру arm64, но его bottle на
проверенной машине собран с minimum macOS 26.0. Он не линкуется в проект:
macOS-ветвь всегда собирает тот же SDL3 release из исходников с deployment
target 13.0. В M3/M5/M6 включены только нужные им SDL video/window/event
возможности. Preset M7 дополнительно включает joystick и HIDAPI без libusb;
проверенная сборка содержит нативные `hidapi`, `iokit`, `mfi` и `virtual`
backends. Haptic subsystem остаётся выключен, а gamepad rumble идёт через SDL
Gamepad API/HIDAPI. Preset M8 включает SDL Audio с `coreaudio`, `disk` и
`dummy` backends; GPU/render и camera SDL subsystems остаются отключены.
Итоговый `RRR3d` и Ogg/Vorbis decoder статически слинкованы и зависят только
от Apple system frameworks.

В отменённом M10 portable bundle те же статические targets входят в
`RRR3d.app/Contents/MacOS/RRR3d`. `Contents/Frameworks` намеренно пуст:
проверка `otool -L` не находит `/opt/homebrew`, `/usr/local`, локальный build
directory или Windows runtime. Bundle получает `@executable_path/../Frameworks`
в rpath для контролируемых будущих dylib, очищается от extended attributes и
ad-hoc подписывается после копирования ресурсов.

Jolt подключается только при `RRR3D_ENABLE_PHYSICS=ON` вместе с
`RRR3D_BUILD_ORIGINAL_MENU=ON`. Windows PhysX targets и старые `.lib` при этом
не меняются; preset M6–M8 Jolt не загружают и не собирают. Будущий исправленный
M10 должен быть заново основан на M9 и включить Jolt notice в bundle.

Homebrew GLM 1.0.3 экспортирует и header-only target, и optional compiled
dylib. Этот проект использует только `glm::glm-header-only`: dylib также собран
для minimum macOS 26.0 и намеренно исключён из Milestone 3 link graph.

bgfx, bx и bimg/bimg_decode не берутся из Homebrew и не используют плавающий branch.
Скрипт собирает их официальным GENie/gmake pipeline для `osx-arm64`,
`--with-macos=13.0`. `shaderc` является build-only host tool: Metal shader
binary встраивается в `RRR3d`, сам executable `shaderc` не нужен runtime.
`Cocoa`, `Metal`, `QuartzCore`, `IOKit`, `CoreAudio`, `AudioToolbox`,
`AVFoundation`, `VideoToolbox`, `GameController`, `ForceFeedback` и
weak-linked `CoreHaptics` — системные frameworks.

Инструменты проверенной сборки: CMake 4.3.1, Ninja 1.13.2, AppleClang 21.0.0.
Минимальные требования проекта остаются CMake 3.21 и macOS 13.0; проверенные
версии инструментов не должны ошибочно становиться минимальными.

## Исторический Windows extern bundle

Этот архив не находится в Git и загружается Windows CI из release
`1.3.1-dev3`. Его нельзя копировать в macOS bundle: большинство библиотек
собрано для Windows/x86/VC140.

| Имя | Версия в архиве | Источник/форма | Лицензия | Старый CMake/link name | macOS policy |
| --- | --- | --- | --- | --- | --- |
| Boost | 1.59.0 | headers + VC140 libs | Boost Software License 1.0 | прямые include/lib paths | при необходимости искать package target; не bundle старые libs |
| GLM | 0.9.9.8 | headers | MIT | глобальный include | заменено `glm::glm` >= 1.0 |
| libogg | 1.3.5 | Windows static libs | BSD-3-Clause | `libogg_static` | заменено pinned source target M8 |
| libvorbis | 1.3.7 | Windows static libs | BSD-3-Clause | `libvorbis_static`, `libvorbisfile_static` | заменено pinned source targets M8 |
| TinyXML 1 | 2.6.2 | headers + Debug/Release libs | zlib | `tinyxml_STL` variants | собирается из pinned source |
| PhysX | 2.8.4 (`NX_SDK_VERSION_NUMBER=284`) | legacy Windows SDK/binaries | proprietary NVIDIA EULA | `PhysXLoader`, `PhysXCooking` | не переносить бинарники; нужен отдельный design decision |
| DirectX SDK | legacy D3D9/D3DX/XInput libraries | Windows SDK bundle | Microsoft proprietary | `d3d9`, `d3dx9`, `dxguid`, `dxerr`, `X3daudio`, `xinput` | запрещено подключать на macOS |
| Steamworks | точная версия не зафиксирована | headers + Windows runtime | Valve Steamworks SDK agreement | CMake-зависимость отсутствует | отключено; версию/redistribution уточнить отдельно |

## Renderer feasibility Milestone 4

| Имя | Зафиксированная/проверенная версия | Использование | Bundle policy | Решение |
| --- | --- | --- | --- | --- |
| DXVK Native | v2.7.1, commit `c3dd74be6baec53786d4e064a572185b70347a17` | отдельная Meson D3D9/SDL3 WSI сборка + Darwin patch | staged `libdxvk_d3d9.0.dylib`, `@rpath`, ad-hoc sign | отклонён как backend: обязательные features отсутствуют |
| MoltenVK | Homebrew 1.4.1 arm64 | только `rrr3d_dxvk_moltenvk_test` | staged `libMoltenVK.dylib`, `@rpath`, ad-hoc sign; Apache-2.0 notice обязателен при дистрибуции | работает как Vulkan portability runtime, но feature set недостаточен DXVK |
| SDL3 shared | 3.4.12, тот же pinned source/digest | SDL3 WSI DXVK test | staged `libSDL3.0.dylib`, `@rpath`, ad-hoc sign | только feasibility target; обычный shell остаётся static |

DXVK build-only prerequisites: Meson 1.11.2, Ninja 1.13.2, glslang 16.4.0 и
Homebrew SDL3 headers/pkg-config. Они не входят в runtime. Vulkan Loader
Homebrew 1.4.341.0 тоже не входит в target: DXVK открывает staged MoltenVK
напрямую.

Подробности решения — `docs/DXVK_MOLTENVK_FEASIBILITY.md`.

## Возможные зависимости следующих этапов

Наличие формулы на машине — только проверка доступности, не решение об
архитектуре и не новая обязательная зависимость.

| Имя | Проверено локально | Лицензия | Плановый target | Bundle policy | Статус |
| --- | --- | --- | --- | --- | --- |
| MoltenVK | Homebrew 1.4.1 arm64 | Apache-2.0 | package-specific Vulkan/MoltenVK target | dylib/framework + licenses либо static | проверен в M4; не выбран для DXVK backend |
| Vulkan Loader | Homebrew 1.4.341.0 arm64 | Apache-2.0 | `Vulkan::Vulkan` | loader policy проверить вместе с MoltenVK | только будущий renderer spike |
| Boost | Homebrew 1.90.0_1 arm64 | Boost Software License 1.0 | `Boost::*` | предпочесть static/минимум компонентов | пока не требуется базовым target |
| libogg | Homebrew 1.3.6 arm64 | BSD-3-Clause | не используется | не bundle | M8 собирает pinned 1.3.5 из source |
| libvorbis | Homebrew 1.3.7 arm64 | BSD-3-Clause | не используется | не bundle | M8 собирает pinned 1.3.7 из source |
| DXVK Native | v2.7.1 / `c3dd74b...` | zlib; vendored header licenses сохранить | отдельный imported target | только feasibility staging | отклонён, renderer integration выключена |
| bgfx | pinned commits из таблицы выше | BSD-2-Clause | `rrr3d_bgfx` | статически | выбран и проверен в Milestone 5 |
| SDL_GPU | API из pinned SDL3 3.4.12 | zlib вместе с SDL3 | будущий отдельный renderer target | уже часть SDL source | альтернативный кандидат Milestone 5 |

Та же pinned SDL3 используется input и audio этапами; отдельные presets
сохраняют отключённые подсистемы выключенными. Архитектура audio backend и
runtime checks описаны в `docs/PORTABLE_AUDIO.md`.

## Правила обновления

- Каждая fetched dependency должна иметь точный tag/commit и checksum, где это
  применимо. `origin/master` запрещён.
- Платформенные binary bundles нельзя выбирать только через `MSVC`; границы —
  `WIN32`, `APPLE` и Linux.
- Новая зависимость должна экспортировать target-based include/link interface.
- Перед добавлением в `.app` фиксируются архитектура arm64, license notices,
  rpath/install name и codesign behaviour.
- Наличие x86_64 Homebrew пакета или Rosetta не считается успешным нативным
  портом.
