# Milestone 8: original Motor Rock audio through SDL3/CoreAudio

## Acceptance path

The corrected `macos-arm64-m8` preset extends the original-resource M7 path:

```text
original MainMenu2 + SDL-independent actions + bgfx/Metal
  -> original Motor Rock Ogg/Vorbis resources
  -> platform-independent AudioBackend
  -> SDL3 default playback stream
  -> CoreAudio
```

`RRR3D_BUILD_ORIGINAL_MENU` remains enabled and
`RRR3D_BUILD_PORTABLE_MENU` remains disabled. No replacement menu, generated
music or synthetic game sound is used.

## Behaviour taken from the legacy game

`OriginalAudioSpec.h` records the menu playlist, sound paths and automatic
volume defaults extracted from `GameMode.cpp`, `Menu.cpp`, `DataBase.cpp` and
`Logic.cpp`. The main-button click path is consumed by both the legacy
`Menu.cpp` and the macOS adapter.

The original `MainMenu2` creates its five buttons with `Menu::ssButton1`.
That scheme contains only `Sounds\\UI\\click.ogg` on button press. It does not
contain a hover or keyboard-navigation sound. M8 therefore plays `click.ogg`
only after a valid `MenuConfirm`; it deliberately does not restore the old
portable slice's invented `navedenie.ogg`/`acception.ogg` behaviour.

The follow-up ports the platform-independent part of legacy
`GameMode::MusicCat`: it builds the same grouped playlist, shuffles all three
group-0 menu entries, consumes the queue from the back and prevents the last
track of one cycle from immediately repeating at the start of the next. A
finished voice automatically selects `Next`; an explicit `Next` uses the same
queue. No menu-only replacement playback policy is used.

All three shipped menu entries are active and retain their title/band/group
metadata:

- `Music\\Track1.ogg` — Jet, Cold Hard Bitch;
- `Music\\Track14.ogg` — Social Distortion, Angel's wings (acoustic);
- `Music\\Track15.ogg` — Stereoside, On our Way.

The source `DialogMenu2::MusicDialog` is also active. It uses the shipped
334×99 `GUI/dlgFrame2.png`, white 32 px band text, gray 24 px track text and
the original offsets. `ShowMusicInfo` runs for the initial menu selection,
automatic/manual `Next`, race start and later game-track changes. Its exact
timeline remains one second offscreen, one second sliding in, three seconds
shown and one second sliding out; changing metadata while it is visible does
not restart that timeline. The same popup covers all three menu records and
all 11 entries in the original game playlist.

`Sounds\\fireGun.ogg`, used by the original fireGun `ShotEffect`, is decoded
and mixed by the M8 test but is not triggered inside the main menu. Its real
gameplay consumer belongs to the race port.

The original automatic category gains are retained:

- Music: 1.2
- Effects: 0.8
- Voice: 1.2
- supported range: 0..2, matching the original options UI

## Backend

`Rock3dEngine/header/audio/AudioBackend.h` has no SDL or Windows types. It
defines sound/voice handles, Music/Effects/Voice buses, one-shot and loop
playback, global and per-voice pause, stop/unload, category and master gain,
device events, and runtime statistics.

`SdlAudioBackend` owns the platform implementation:

```text
original .ogg
  -> pinned libvorbisfile float decode
  -> SDL_AudioStream conversion/resample
  -> 48 kHz stereo F32 voice mixer
  -> SDL default playback stream
  -> CoreAudio output
```

Decoded PCM is checked for finite values and records peak/RMS amplitude, so a
valid header with empty or silent decode cannot satisfy the smoke test. The
mixer combines voices under a mutex, applies master/category/per-voice gain,
limits final samples to `[-1, 1]`, removes completed one-shots, preserves loop
and pause cursors, and releases stream, voices and decoded sounds before
`SDL_Quit`.

libogg 1.3.5 and libvorbis/libvorbisfile 1.3.7 are fetched from the official
Xiph release archives with pinned SHA-256 values and linked statically. No
Homebrew audio dylib or Windows XAudio2/X3DAudio library enters the M8 link
graph.

Long sounds are decoded fully, but never on the UI/render thread. A single
background job first decodes the selected track and then preloads the next
playlist entries. Completion is polled without waiting from the SDL/bgfx loop;
voice creation and automatic/manual transitions therefore do not decode or
resample synchronously. The implementation remains bounded to 512 MiB per
sound. A future streaming/ring-buffer backend can replace this policy without
changing `MusicCat`.

`PlayOptions::startFrame` and `voicePositionFrames` preserve the mixer cursor.
Pause freezes the active voice without discarding it; resume continues from
that cursor. The current track, remaining shuffled playlist, RNG state,
pause flag and frame position are atomically written to
`~/Library/Application Support/RRR3d/menu-music.state` on transitions,
pause/resume, clean shutdown and periodically during playback. A catalog
fingerprint and strict range/duplicate validation reject stale or corrupt
state.

## Resource coverage

The copied game data contains 182 original Ogg containers:

- 16 in `Data/Music`;
- 46 in `Data/Sounds` including UI and gameplay effects;
- 120 in `Data/Voice`.

The smoke test checks the `OggS` capture pattern of all 182 files, then fully
decodes representative resources from each active M8 role. The integrated
follow-up additionally background-decodes and validates every original menu
track:

- `Music\\Track1.ogg`, `Track14.ogg`, `Track15.ogg` — complete menu playlist;
- `Sounds\\UI\\click.ogg` — actual MainMenu2 `ssButton1` effect;
- `Sounds\\fireGun.ogg` — original gameplay `ShotEffect`.

## Device and integration behaviour

The backend opens `SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK`. SDL/CoreAudio default
device migration therefore does not invalidate game sound/voice handles.
Playback added, removed and format-changed events are forwarded to the
platform-independent boundary and counted/logged.

The integrated smoke injects a real SDL keyboard event sequence and requires
this complete path to succeed:

```text
Down + Return
  -> SdlInputManager
  -> MenuDown + MenuConfirm
  -> original MainMenu2 selection 1
  -> shared Network command
  -> ssButton1 click voice
```

It additionally verifies virtual-gamepad M7 regression, nonzero decoded PCM,
the SDL playback callback, one-shot completion, a loop crossing its sample
boundary, 0..2 gain clamp, all three category gains, global/per-voice
pause/resume, stop/unload and device notifications. The MusicCat follow-up
requires render frames to continue while all three tracks decode, verifies a
stable cursor while paused and progress after resume, seeks near the real Ogg
end to exercise automatic `Next`, performs another explicit `Next`, proves the
three selections are distinct, observes the source `MusicDialog` and
round-trips the persisted state. Its isolated smoke state is removed
afterwards.

## Build and verification

```bash
cmake --preset macos-arm64-m8
cmake --build --preset macos-arm64-m8 -j 8

SDL_AUDIODRIVER=dummy \
  build/macos-arm64-m8/Debug/RRR3d \
  --language=russian --audio-smoke-test

build/macos-arm64-m8/Debug/RRR3d \
  --language=english --smoke-test-frames=180
```

The dummy driver makes the automated test silent. A separate runtime check
opened SDL's `coreaudio` driver and the system `MacBook Pro Speakers` default
device. Physical unplug/replug of a USB or Bluetooth playback device was not
available and remains a release hardware check.

## Honest boundary

M8 satisfies the sound milestone for the running original menu and proves
that original gameplay effects work through the same backend. It does not
claim the Windows `snd::Engine`, X3DAudio spatial emitters, commentator queue
or race sound graph are already ported.

The independent MusicCat policy, its three real menu tracks, background
decoding, shuffle, automatic/manual Next, pause/resume and state restoration
are now present. This still does not port the Windows-only spatial engine,
commentator queue or race sound graph. M9 must attach the backend to the real
race/physics port rather than firing gameplay effects from the menu as a
demonstration.
