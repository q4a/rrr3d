using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;

internal static class MotorRockReferenceControl
{
    private const string DefaultGamePath = @"\\Mac\Home\Downloads\Motor Rock\MR.exe";
    private const int SwRestore = 9;
    private const uint SwpNoMove = 0x0002;
    private const uint SwpNoSize = 0x0001;
    private const uint SwpShowWindow = 0x0040;

    private const uint InputMouse = 0;
    private const uint InputKeyboard = 1;
    private const uint MouseMove = 0x0001;
    private const uint MouseLeftDown = 0x0002;
    private const uint MouseLeftUp = 0x0004;
    private const uint MouseAbsolute = 0x8000;
    private const uint MouseVirtualDesktop = 0x4000;
    private const uint KeyExtended = 0x0001;
    private const uint KeyUp = 0x0002;
    private const uint KeyScanCode = 0x0008;
    private const uint MapVkToScanCode = 0;
    private const uint AsfwAny = 0xFFFFFFFF;
    private const uint LsfwUnlock = 2;

    private static readonly IntPtr HwndTop = IntPtr.Zero;
    private static readonly IntPtr HwndTopMost = new IntPtr(-1);
    private static readonly IntPtr HwndNotTopMost = new IntPtr(-2);

    private static readonly Dictionary<string, ushort> Keys =
        new Dictionary<string, ushort>(StringComparer.OrdinalIgnoreCase)
        {
            { "backspace", 0x08 },
            { "tab", 0x09 },
            { "enter", 0x0D },
            { "return", 0x0D },
            { "shift", 0x10 },
            { "control", 0x11 },
            { "ctrl", 0x11 },
            { "alt", 0x12 },
            { "escape", 0x1B },
            { "esc", 0x1B },
            { "space", 0x20 },
            { "pageup", 0x21 },
            { "pagedown", 0x22 },
            { "end", 0x23 },
            { "home", 0x24 },
            { "left", 0x25 },
            { "up", 0x26 },
            { "right", 0x27 },
            { "down", 0x28 },
            { "insert", 0x2D },
            { "delete", 0x2E },
            { "f1", 0x70 },
            { "f2", 0x71 },
            { "f3", 0x72 },
            { "f4", 0x73 },
            { "f5", 0x74 },
            { "f6", 0x75 },
            { "f7", 0x76 },
            { "f8", 0x77 },
            { "f9", 0x78 },
            { "f10", 0x79 },
            { "f11", 0x7A },
            { "f12", 0x7B }
        };

    [StructLayout(LayoutKind.Sequential)]
    private struct Rect
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Input
    {
        public uint Type;
        public InputUnion Data;
    }

    [StructLayout(LayoutKind.Explicit)]
    private struct InputUnion
    {
        [FieldOffset(0)] public MouseInput Mouse;
        [FieldOffset(0)] public KeyboardInput Keyboard;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MouseInput
    {
        public int Dx;
        public int Dy;
        public uint MouseData;
        public uint Flags;
        public uint Time;
        public IntPtr ExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KeyboardInput
    {
        public ushort VirtualKey;
        public ushort ScanCode;
        public uint Flags;
        public uint Time;
        public IntPtr ExtraInfo;
    }

    [DllImport("user32.dll")]
    private static extern bool SetProcessDPIAware();

    [DllImport("kernel32.dll")]
    private static extern IntPtr GetConsoleWindow();

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr window, int command);

    [DllImport("user32.dll")]
    private static extern bool ShowWindowAsync(IntPtr window, int command);

    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    private static extern IntPtr SetActiveWindow(IntPtr window);

    [DllImport("user32.dll")]
    private static extern bool AllowSetForegroundWindow(uint processId);

    [DllImport("user32.dll")]
    private static extern bool LockSetForegroundWindow(uint lockCode);

    [DllImport("user32.dll")]
    private static extern bool BringWindowToTop(IntPtr window);

    [DllImport("user32.dll")]
    private static extern IntPtr SetFocus(IntPtr window);

    [DllImport("user32.dll")]
    private static extern void SwitchToThisWindow(IntPtr window, bool altTab);

    [DllImport("user32.dll")]
    private static extern bool SetWindowPos(
        IntPtr window,
        IntPtr insertAfter,
        int x,
        int y,
        int width,
        int height,
        uint flags);

    [DllImport("user32.dll")]
    private static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();

    [DllImport("user32.dll")]
    private static extern bool AttachThreadInput(uint fromThread, uint toThread, bool attach);

    [DllImport("user32.dll")]
    private static extern bool GetWindowRect(IntPtr window, out Rect rect);

    [DllImport("user32.dll")]
    private static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    private static extern int GetSystemMetrics(int index);

    [DllImport("user32.dll")]
    private static extern uint SendInput(uint count, Input[] inputs, int inputSize);

    [DllImport("user32.dll")]
    private static extern uint MapVirtualKey(uint code, uint mapType);

    public static int Main(string[] args)
    {
        SetProcessDPIAware();
        HideOwnConsole();

        if (args.Length == 0)
        {
            return Usage("A command is required.");
        }

        try
        {
            string command = args[0].ToLowerInvariant();
            switch (command)
            {
                case "start":
                    return StartMotorRock(args.Length >= 2 ? args[1] : DefaultGamePath) ? 0 : 8;
                case "status":
                    return PrintStatus();
                case "focus":
                    return FocusMotorRock() ? 0 : 3;
                case "move":
                    RequireArgs(args, 3);
                    return MoveMouse(ParseInt(args[1]), ParseInt(args[2])) ? 0 : 4;
                case "click":
                    RequireArgs(args, 3);
                    return Click(ParseInt(args[1]), ParseInt(args[2])) ? 0 : 5;
                case "key":
                    RequireArgs(args, 2);
                    int count = args.Length >= 3 ? Math.Max(1, ParseInt(args[2])) : 1;
                    return PressKey(args[1], count) ? 0 : 6;
                case "screenshot":
                    RequireArgs(args, 2);
                    return SaveScreenshot(args[1]) ? 0 : 7;
                default:
                    return Usage("Unknown command: " + args[0]);
            }
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine(exception.Message);
            return 2;
        }
    }

    private static void HideOwnConsole()
    {
        IntPtr console = GetConsoleWindow();
        if (console != IntPtr.Zero)
        {
            ShowWindow(console, 0);
        }
    }

    private static Process FindMotorRock()
    {
        Process process = Process.GetProcessesByName("MR")
            .Where(item => item.MainWindowHandle != IntPtr.Zero)
            .OrderByDescending(item => item.WorkingSet64)
            .FirstOrDefault();

        if (process == null)
        {
            throw new InvalidOperationException("MR.exe is not running or has no top-level window.");
        }

        return process;
    }

    private static bool StartMotorRock(string executablePath)
    {
        Process existing = Process.GetProcessesByName("MR")
            .Where(item => item.MainWindowHandle != IntPtr.Zero)
            .FirstOrDefault();
        if (existing == null)
        {
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = executablePath,
                WorkingDirectory = Path.GetDirectoryName(executablePath),
                UseShellExecute = true
            };
            Process.Start(startInfo);

            Stopwatch timeout = Stopwatch.StartNew();
            while (timeout.Elapsed < TimeSpan.FromSeconds(20))
            {
                Thread.Sleep(150);
                existing = Process.GetProcessesByName("MR")
                    .Where(item => item.MainWindowHandle != IntPtr.Zero)
                    .FirstOrDefault();
                if (existing != null)
                {
                    break;
                }
            }
        }

        if (existing == null)
        {
            Console.Error.WriteLine("MR.exe did not expose a top-level window within 20 seconds.");
            return false;
        }

        bool focused = FocusMotorRock();
        Console.WriteLine("start path={0} pid={1} focused={2}", executablePath, existing.Id, focused);
        return true;
    }

    private static bool FocusMotorRock()
    {
        Process process = FindMotorRock();
        IntPtr target = process.MainWindowHandle;
        bool focused = false;

        for (int attempt = 0; attempt < 5 && !focused; ++attempt)
        {
            IntPtr foreground = GetForegroundWindow();
            uint ignored;
            uint currentThread = GetCurrentThreadId();
            uint foregroundThread = foreground == IntPtr.Zero
                ? 0
                : GetWindowThreadProcessId(foreground, out ignored);
            uint targetThread = GetWindowThreadProcessId(target, out ignored);

            AllowSetForegroundWindow(AsfwAny);
            LockSetForegroundWindow(LsfwUnlock);
            TapAlt();
            ShowWindowAsync(target, SwRestore);
            bool attachedForeground = foregroundThread != 0 && foregroundThread != currentThread &&
                                      AttachThreadInput(currentThread, foregroundThread, true);
            bool attachedTarget = targetThread != 0 && targetThread != currentThread &&
                                  AttachThreadInput(currentThread, targetThread, true);

            try
            {
                SetWindowPos(target, HwndTopMost, 0, 0, 0, 0, SwpNoMove | SwpNoSize | SwpShowWindow);
                SetWindowPos(target, HwndNotTopMost, 0, 0, 0, 0, SwpNoMove | SwpNoSize | SwpShowWindow);
                BringWindowToTop(target);
                SetWindowPos(target, HwndTop, 0, 0, 0, 0, SwpNoMove | SwpNoSize | SwpShowWindow);
                SetActiveWindow(target);
                SetForegroundWindow(target);
                SetFocus(target);
                SwitchToThisWindow(target, true);
            }
            finally
            {
                if (attachedTarget)
                {
                    AttachThreadInput(currentThread, targetThread, false);
                }
                if (attachedForeground)
                {
                    AttachThreadInput(currentThread, foregroundThread, false);
                }
            }

            Thread.Sleep(140);
            focused = GetForegroundWindow() == target;
        }

        Console.WriteLine("focus pid={0} hwnd=0x{1:X} focused={2}",
            process.Id,
            target.ToInt64(),
            focused);
        return focused;
    }

    private static void TapAlt()
    {
        Input down = CreateVirtualKeyInput(0x12, 0);
        Input up = CreateVirtualKeyInput(0x12, KeyUp);
        SendInput(2, new[] { down, up }, Marshal.SizeOf(typeof(Input)));
    }

    private static bool MoveMouse(int x, int y)
    {
        FocusMotorRock();
        int virtualLeft = GetSystemMetrics(76);
        int virtualTop = GetSystemMetrics(77);
        int virtualWidth = Math.Max(1, GetSystemMetrics(78));
        int virtualHeight = Math.Max(1, GetSystemMetrics(79));
        int normalizedX = (int)Math.Round((x - virtualLeft) * 65535.0 / Math.Max(1, virtualWidth - 1));
        int normalizedY = (int)Math.Round((y - virtualTop) * 65535.0 / Math.Max(1, virtualHeight - 1));
        normalizedX = Math.Max(0, Math.Min(65535, normalizedX));
        normalizedY = Math.Max(0, Math.Min(65535, normalizedY));

        SetCursorPos(x, y);
        Input input = CreateMouseInput(normalizedX, normalizedY, MouseMove | MouseAbsolute | MouseVirtualDesktop);
        bool sent = SendInput(1, new[] { input }, Marshal.SizeOf(typeof(Input))) == 1;
        Thread.Sleep(60);
        Console.WriteLine("move x={0} y={1} sent={2}", x, y, sent);
        return sent;
    }

    private static bool Click(int x, int y)
    {
        if (!MoveMouse(x, y))
        {
            return false;
        }

        Input down = CreateMouseInput(0, 0, MouseLeftDown);
        Input up = CreateMouseInput(0, 0, MouseLeftUp);
        uint sent = SendInput(2, new[] { down, up }, Marshal.SizeOf(typeof(Input)));
        Thread.Sleep(120);
        Console.WriteLine("click x={0} y={1} sent={2}", x, y, sent == 2);
        return sent == 2;
    }

    private static bool PressKey(string name, int count)
    {
        FocusMotorRock();
        ushort virtualKey = ParseKey(name);
        ushort scanCode = (ushort)MapVirtualKey(virtualKey, MapVkToScanCode);
        bool extended = virtualKey >= 0x21 && virtualKey <= 0x2E;
        uint baseFlags = KeyScanCode | (extended ? KeyExtended : 0);

        for (int index = 0; index < count; ++index)
        {
            Input down = CreateKeyboardInput(scanCode, baseFlags);
            Input up = CreateKeyboardInput(scanCode, baseFlags | KeyUp);
            if (SendInput(2, new[] { down, up }, Marshal.SizeOf(typeof(Input))) != 2)
            {
                return false;
            }
            Thread.Sleep(90);
        }

        Console.WriteLine("key name={0} count={1} sent=true", name, count);
        return true;
    }

    private static bool SaveScreenshot(string outputPath)
    {
        FocusMotorRock();
        Thread.Sleep(120);
        int left = GetSystemMetrics(76);
        int top = GetSystemMetrics(77);
        int width = GetSystemMetrics(78);
        int height = GetSystemMetrics(79);
        string fullPath = Path.GetFullPath(outputPath);
        string outputDirectory = Path.GetDirectoryName(fullPath);
        if (!string.IsNullOrEmpty(outputDirectory))
        {
            Directory.CreateDirectory(outputDirectory);
        }

        using (Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format32bppArgb))
        using (Graphics graphics = Graphics.FromImage(bitmap))
        {
            graphics.CopyFromScreen(left, top, 0, 0, new Size(width, height));
            bitmap.Save(fullPath, ImageFormat.Png);
        }

        Console.WriteLine("screenshot path={0} width={1} height={2}", fullPath, width, height);
        return File.Exists(fullPath);
    }

    private static int PrintStatus()
    {
        Process process = FindMotorRock();
        Rect rect;
        bool hasRect = GetWindowRect(process.MainWindowHandle, out rect);
        Console.WriteLine(
            "pid={0} hwnd=0x{1:X} responding={2} foreground={3} rect={4},{5},{6},{7} screen={8}x{9}",
            process.Id,
            process.MainWindowHandle.ToInt64(),
            process.Responding,
            GetForegroundWindow() == process.MainWindowHandle,
            hasRect ? rect.Left : 0,
            hasRect ? rect.Top : 0,
            hasRect ? rect.Right : 0,
            hasRect ? rect.Bottom : 0,
            GetSystemMetrics(0),
            GetSystemMetrics(1));
        return 0;
    }

    private static Input CreateMouseInput(int x, int y, uint flags)
    {
        return new Input
        {
            Type = InputMouse,
            Data = new InputUnion
            {
                Mouse = new MouseInput
                {
                    Dx = x,
                    Dy = y,
                    Flags = flags,
                    MouseData = 0,
                    Time = 0,
                    ExtraInfo = IntPtr.Zero
                }
            }
        };
    }

    private static Input CreateKeyboardInput(ushort scanCode, uint flags)
    {
        return new Input
        {
            Type = InputKeyboard,
            Data = new InputUnion
            {
                Keyboard = new KeyboardInput
                {
                    VirtualKey = 0,
                    ScanCode = scanCode,
                    Flags = flags,
                    Time = 0,
                    ExtraInfo = IntPtr.Zero
                }
            }
        };
    }

    private static Input CreateVirtualKeyInput(ushort virtualKey, uint flags)
    {
        return new Input
        {
            Type = InputKeyboard,
            Data = new InputUnion
            {
                Keyboard = new KeyboardInput
                {
                    VirtualKey = virtualKey,
                    ScanCode = 0,
                    Flags = flags,
                    Time = 0,
                    ExtraInfo = IntPtr.Zero
                }
            }
        };
    }

    private static ushort ParseKey(string value)
    {
        ushort key;
        if (Keys.TryGetValue(value, out key))
        {
            return key;
        }

        if (value.Length == 1)
        {
            char character = char.ToUpperInvariant(value[0]);
            if ((character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9'))
            {
                return character;
            }
        }

        if (value.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
        {
            return Convert.ToUInt16(value.Substring(2), 16);
        }

        throw new ArgumentException("Unsupported key: " + value);
    }

    private static int ParseInt(string value)
    {
        int parsed;
        if (!int.TryParse(value, out parsed))
        {
            throw new ArgumentException("Expected an integer, got: " + value);
        }
        return parsed;
    }

    private static void RequireArgs(string[] args, int count)
    {
        if (args.Length < count)
        {
            throw new ArgumentException("Not enough arguments for command: " + args[0]);
        }
    }

    private static int Usage(string message)
    {
        Console.Error.WriteLine(message);
        Console.Error.WriteLine("Commands: start [EXE] | status | focus | move X Y | click X Y | key NAME [COUNT] | screenshot PATH");
        return 1;
    }
}
