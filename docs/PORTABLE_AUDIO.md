# Portable audio Milestone 8

> **Superseded historical report.** This file describes audio attached to the
> cancelled standalone portable vertical slice and is not the corrected M8
> acceptance status. See `docs/ORIGINAL_AUDIO_M8.md`.

## Результат

Milestone 8 добавляет настоящий macOS audio path поверх SDL3 Audio/CoreAudio.
Главное меню декодирует оригинальный `Data/Music/Track1.ogg`, запускает его в
цикле и воспроизводит оригинальные UI effects. Gameplay action `UseWeapon`
маршрутизирован в `Data/Sounds/fireGun.ogg`, поэтому тот же effects bus готов
для подключения race scene в Milestone 9.

В imported game data находятся 62 Ogg/Vorbis-файла: 16 музыкальных треков и
46 UI/gameplay effects. Файлы не транскодируются и не заменяются.

## Архитектура

Legacy `snd::Engine` объединяет XAudio2 voices, X3DAudio, streaming buffers и
game resources в Windows-specific API. На macOS он не включается. Вместо
переписывания всех игровых вызовов добавлен SDL-независимый интерфейс
`Rock3dEngine/header/audio/AudioBackend.h`:

- opaque `SoundHandle` и `VoiceHandle`;
- music/effects buses;
- load/unload, play/stop и one-shot/loop;
- global и per-voice pause/resume;
- master, music, effects и per-voice volume;
- playback-device notifications и runtime statistics.

`SdlAudioBackend` находится в platform executable и является единственным
местом, где видны SDL audio types. Поток данных:

```text
Ogg/Vorbis resource
  -> libvorbisfile float PCM decode
  -> SDL_AudioStream channel conversion/resample
  -> 48 kHz stereo F32 portable voice mixer
  -> SDL default playback stream
  -> CoreAudio output device
```

Mixer работает в SDL audio callback, защищает sound/voice lifecycle mutex'ом,
суммирует music и effects buses, ограничивает итоговый сигнал диапазоном
`[-1, 1]` и автоматически удаляет завершившиеся one-shot voices. Shutdown
сначала останавливает callback/device, затем освобождает voices и decoded
sounds, поэтому SDL objects не переживают `SDL_Quit`.

## Ogg/Vorbis и зависимости

Из официальных архивов статически собираются:

| Библиотека | Версия | SHA-256 |
| --- | --- | --- |
| libogg | 1.3.5 | `c4d91be36fc8e54deae7575241e03f4211eb102afb3fc0775fbbc1b740016705` |
| libvorbis/libvorbisfile | 1.3.7 | `b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b` |

Homebrew dylib не используется. Decoder читает Vorbis float blocks и отдаёт
SDL converter'у исходную частоту/число каналов. Текущие ресурсы имеют 44.1 kHz
stereo; backend также принимает mono и multichannel single-stream Vorbis до
восьми каналов. На один decoded sound действует safety limit 512 MiB.

На этом milestone sound загружается целиком в PCM. Это даёт простой и
детерминированный lifecycle, но `Track1.ogg` занимает примерно 92 MiB после
преобразования. Streaming/ring buffer для длинной музыки — оптимизация после
функционального порта; интерфейс backend менять для неё не требуется.

## Меню, эффекты и pause

При обычном запуске:

- `Track1.ogg` играет в loop на music bus;
- hover, MenuUp/MenuDown и ChangeWeapon воспроизводят `navedenie.ogg`;
- MenuConfirm воспроизводит `acception.ogg`;
- UseWeapon воспроизводит gameplay effect `fireGun.ogg`;
- action `Pause` переключает global device pause/resume.

Master volume установлен в 0.85, music bus в 0.45, effects bus в 0.85.
Значения на API ограничиваются диапазоном 0…1.

## Смена аудиоустройства

Backend открывает `SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK`, а не сохраняет ID или
имя конкретных динамиков. SDL3/CoreAudio переносит такой logical stream на
новое системное default device. `SDL_EVENT_AUDIO_DEVICE_ADDED`,
`REMOVED` и `FORMAT_CHANGED` поступают в переносимую границу, учитываются в
statistics и логируются. Это покрывает подключение/отключение наушников и
смену формата без пересоздания игровых sound/voice handles.

Автоматический event path проверен тестовыми notifications; физическое
переключение Bluetooth/USB-устройства на проверочной машине не выполнялось и
остаётся release hardware check.

## Сборка и проверка

```bash
cmake --preset macos-arm64-m8
cmake --build --preset macos-arm64-m8 --clean-first -j 8

SDL_AUDIODRIVER=dummy \
  build/macos-arm64-m8/Debug/RRR3d --audio-smoke-test
build/macos-arm64-m8/Debug/RRR3d --smoke-test-frames=90
build/macos-arm64-m8/Debug/RRR3d
```

`--audio-smoke-test` использует реальные imported Ogg files и проверяет:

- metadata и decode музыки, UI и gameplay effects;
- SDL conversion и работу playback callback;
- one-shot completion и looping voice;
- master/music/effects volume и clamp;
- global и per-voice pause/resume;
- added/format-changed/removed device notifications;
- stop, unload и отсутствие leaked sounds/voices;
- 180 кадров объединённого input/menu/audio runtime.

Dummy driver нужен только для бесшумной автоматической проверки. Отдельный
запуск подтвердил runtime driver `coreaudio`, output `MacBook Pro Speakers`,
оригинальную музыку и 90 кадров bgfx/Metal menu. Запуск из `/tmp` подтвердил,
что audio resources находятся рядом с executable, а не через current working
directory.

## Подключение к Milestone 9

Исправленный M9 подключил `AudioBackend` к original-data race state. На старте
MusicCat приостанавливает текущий сохранённый menu track, а sound references
Marauder из `db.xml` запускают `engine_player_heavy_tom.ogg` и
`Motor_high02.ogg` как loops. Возврат в меню останавливает оба engine voice и
продолжает тот же MusicCat track. Collision/weapon/finish sound graph в M9 ещё
не перенесён и искусственно не воспроизводится. X3DAudio spatial
emitter/listener model также пока отсутствует; engine идёт через effects bus
без 3D attenuation.
