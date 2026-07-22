# Portable input and SDL3 gamepad (Milestone 7)

## Результат

Milestone 7 добавляет единый action layer между SDL3 и игровым/menu code.
Preset `macos-arm64-m7` сохраняет M6 menu/resources/bgfx Metal и дополнительно
собирает SDL joystick и HIDAPI. Клавиатура, мышь и gamepad управляют одной
menu model; игровые компоненты не зависят от SDL scancode или XInput.

SDL собран статически с нативными macOS backends `hidapi`, `iokit`, `mfi` и
`virtual`. `libusb` не используется. Подключение и отключение контроллера
обрабатываются во время работы приложения.

## Граница действий

Публичный `Rock3dGame/include/PortableInput.h` определяет действия независимо
от SDL:

- gameplay: `Accelerate`, `Brake`, `TurnLeft`, `TurnRight`, `UseWeapon`,
  `ChangeWeapon`, `Pause`;
- menu: `MenuUp`, `MenuDown`, `MenuConfirm`, `MenuBack`.

Каждое событие содержит normalized value, active/release state, repeat flag,
тип источника и device id. `SdlInputManager` остаётся в executable и только
переводит SDL events в эту модель.

Текущие menu bindings:

| Действие | Клавиатура | Мышь | Gamepad |
| --- | --- | --- | --- |
| MenuUp | Up, W | wheel up | D-pad Up, left stick up |
| MenuDown | Down, S | wheel down | D-pad Down, left stick down |
| MenuConfirm | Enter, keypad Enter, Space | left click | South/A/Cross, Start |
| MenuBack | Escape, Backspace | right click | East/B/Circle, Back |

Mouse motion переводится из logical SDL window coordinates в virtual canvas
1920×1100. Hover меняет selection; left click сначала выбирает пункт под
курсором, затем активирует его.

## Analog input и dead zones

- left stick menu press threshold: 0.55;
- left stick release/dead-zone threshold: 0.25;
- раздельные press/release thresholds дают hysteresis и не допускают дребезг;
- left stick X создаёт normalized `TurnLeft`/`TurnRight` values после dead zone;
- left/right triggers создают `Brake`/`Accelerate` с trigger dead zone 0.12;
- D-pad и face buttons остаются цифровыми действиями.

`rumble(device, low, high, duration)` передаёт normalized amplitudes в
`SDL_RumbleGamepad`. Неподдерживаемое устройство возвращает ошибку без
падения приложения.

При focus loss action layer выдаёт releases и сбрасывает stick hysteresis.
При hot-unplug контроллер закрывается и его действия освобождаются. При
hot-plug SDL mapping открывается автоматически, поэтому Xbox, PlayStation и
обычные Bluetooth gamepads используют стандартную SDL Gamepad раскладку.

## Сборка и проверка

```bash
cmake --preset macos-arm64-m7
cmake --build --preset macos-arm64-m7 -j 8

build/macos-arm64-m7/Debug/RRR3d --verify-resources
build/macos-arm64-m7/Debug/RRR3d --input-smoke-test
build/macos-arm64-m7/Debug/RRR3d
```

`--input-smoke-test` создаёт SDL virtual gamepad, проверяет настоящий
hot-plug/open/detach lifecycle, затем через SDL Gamepad state проверяет:

- keyboard, mouse button/wheel и focus reset;
- отсутствие menu action внутри stick dead zone;
- stick threshold и hysteresis release;
- analog steering и trigger;
- D-pad, confirm и back buttons;
- rumble dispatch;
- hot-unplug и 120 кадров bgfx/Metal menu.

Проверенный вывод:

```text
Input: SDL3 keyboard/mouse/gamepad, 0 gamepad(s), stick dead zone 0.25
Gamepad connected: id=3 name='RRR3D Milestone 7 Virtual Gamepad'
Gamepad disconnected: id=3
Milestone 7 input smoke: keyboard, mouse, focus reset, gamepad hot-plug, dead zones, analog axes, buttons, triggers, and rumble passed
Milestone 7 input/menu smoke test completed after 120 frames
```

Также вручную проверены реальные Cocoa window events: Down/Enter выбрали
`MULTIPLAYER`, mouse hover/click выбрали другой пункт и активировали его.
Физический controller на проверочной машине не подключался; hardware-specific
Bluetooth/USB и rumble следует повторить на Xbox/PlayStation devices перед
release.

## Граница следующего этапа

Gameplay world и race пока не собраны, поэтому gameplay actions уже
формируются, но ещё не имеют car consumer. Milestone 8 должен добавить SDL3
Audio, сохранив M7 input preset зелёным. Подключение action values к машине
будет проверено вместе с запускаемой гонкой в Milestone 9.
