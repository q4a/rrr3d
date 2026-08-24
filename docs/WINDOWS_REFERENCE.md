# Windows-эталон Motor Rock

Для визуального и поведенческого сравнения macOS-порта используется именно
эта копия, а не CrossOver:

- виртуальная машина Parallels: `Windows 11`;
- исполняемый файл: `\\Mac\Home\Downloads\Motor Rock\MR.exe`;
- соответствующий путь macOS: `/Users/dmitrijcerednicenko/Downloads/Motor Rock/MR.exe`;
- проверенный полноэкранный режим игры: `1920x1080`;
- версия в главном меню: `v. 1.2.0`.

## Почему не RDP

RDP создаёт отдельный Windows-сеанс и может заменить графический адаптер,
режим вывода, DPI и аудиоустройство. Такой кадр не является точным эталоном
локального DirectX-запуска в Parallels. Кроме того, `MR.exe` не предоставляет
accessibility tree: Parallels/Codex видит окно Проводника, а не exclusive
DirectX surface.

Поэтому применяется гибридный путь:

1. `MotorRockReferenceControl.exe` работает в текущем интерактивном Windows-
   сеансе, находит окно `MR.exe`, восстанавливает фокус и отправляет абсолютное
   движение мыши, клики и клавиши через Win32 `SendInput`. Его `CopyFromScreen`
   захватывает фактический DirectX-кадр после программного ввода.
2. `prlctl capture` независимо снимает framebuffer самой VM. Он удобен для
   контроля запуска, но после возврата exclusive DirectX focus иногда выдаёт
   чёрный кадр; поэтому не является основным источником menu-reference.

## Управление

Из корня репозитория:

```sh
tools/windows_reference/motorrock-reference.sh build
tools/windows_reference/motorrock-reference.sh start
tools/windows_reference/motorrock-reference.sh status
tools/windows_reference/motorrock-reference.sh focus
tools/windows_reference/motorrock-reference.sh click 960 570
tools/windows_reference/motorrock-reference.sh key escape
tools/windows_reference/motorrock-reference.sh game-capture "$PWD/build/windows-reference.png"
tools/windows_reference/motorrock-reference.sh capture /tmp/windows-vm-framebuffer.png
```

Координаты `click` задаются в пикселях текущего Windows desktop/game mode.
Для эталонного полноэкранного режима это пространство `1920x1080`.

Канонические сравнительные кадры меню следует получать командой
`game-capture`. Путь результата должен находиться внутри домашней папки macOS,
чтобы Windows могла записать PNG через `\\Mac\Home`. Команда `capture` нужна
как независимая проверка framebuffer Parallels; `guest-capture` принимает
готовый Windows/UNC-путь и оставлена для низкоуровневой диагностики.

## Первая сверка интерфейса

Эталонные главный экран и `Settings` сняты в `1920x1080`. Они подтвердили,
что исходная GUI-система считает координаты в пикселях D3D9 backbuffer, а не
в логических desktop points. В macOS-порте Retina-окно `960x540` points имеет
Metal drawable `1920x1080`; использование размера `960x540` как GUI viewport
увеличивало и обрезало оригинальные ресурсы, растягивало текстовые текстуры и
смещало области кликов.

Правило для последующих экранов:

- проекция и `MainMenu2Spec::virtualWidth/virtualHeight` используют drawable
  pixels;
- координаты SDL-мыши остаются в window points и переводятся в GUI-space
  пропорцией `drawable / window`;
- при смене разрешения вместе с bgfx backbuffer пересоздаётся ортографическая
  GUI-камера.
