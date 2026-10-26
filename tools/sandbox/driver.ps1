<#
.SYNOPSIS
Runs one iwd2r scenario with real input inside Windows Sandbox.

.DESCRIPTION
play.py --vm windows writes config.json and actions.json (the scenario,
already parsed and translated by tools/realinput.py) into a read-only tools
folder and starts the sandbox with this script as its logon command. The
script starts iwd2r.exe from the mapped build folder against the mapped copy
of the install, waits for its window and executes the actions with genuine
Windows input: SetCursorPos and SendInput for the mouse, SendInput with
virtual-key and scan codes for keys. `shot NAME` captures the screen with
System.Drawing (NAME-screen.png) and asks the engine for its framebuffer
(NAME.png) through the IWD2R_CONTROL_DIR channel; `checkpoint NAME` asks it
for a state dump (checkpoint-NAME.json). `wait N` lasts N/30 s and until the
engine has presented N frames (the same channel). `quit` posts WM_CLOSE to
the game window, what its title bar's close button does, and confirms the
engine's message box with Return.

Input reaches the game only while its window is the foreground window: SDL
drops the click that activates a window, and the engine stops polling input
while it is inactive (CChitin::OnAltTab). Before the game starts the driver
disables the sandbox's logon task that took the foreground about 30 s after
logon (Quiet-Logon); while the scenario runs it watches the foreground and
fails the run when another process took it.

The results folder receives driver.log, log.txt (the game's output),
exitcode.txt (the game's exit code), error.txt (only when the driver
failed) and finally done.txt; then the sandbox shuts down.

-DryRun reads the inputs and prints what it would do, without starting the
game, sending input or writing files; it is safe on the host desktop.
#>
param(
    [Parameter(Mandatory = $true)][string]$Config,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$started = Get-Date
$cfg = Get-Content -Raw -LiteralPath $Config | ConvertFrom-Json
$actionsPath = $cfg.actions
if ($DryRun -and -not (Test-Path -LiteralPath $actionsPath)) {
    # On the host the sandbox paths do not exist: actions.json sits next to
    # config.json.
    $actionsPath = Join-Path (Split-Path -Parent $Config) 'actions.json'
}
# Windows PowerShell 5.1 emits a JSON array as one object; the second
# pipeline enumerates it.
$actions = @((Get-Content -Raw -LiteralPath $actionsPath | ConvertFrom-Json) | ForEach-Object { $_ })
$frameRate = [double]$cfg.frame_rate
$deadline = $started.AddSeconds([double]$cfg.timeout)
$logPath = Join-Path $cfg.results 'driver.log'

function Log([string]$message) {
    $elapsed = ((Get-Date) - $started).TotalSeconds
    $line = '{0} +{1,7:0.000} {2}' -f (Get-Date -Format 'HH:mm:ss.fff'), $elapsed, $message
    Write-Host $line
    if (-not $DryRun) {
        Add-Content -LiteralPath $logPath -Value $line -Encoding UTF8
    }
}

$source = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class IwdInput {
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X; public int Y; }

    [StructLayout(LayoutKind.Sequential)]
    struct MOUSEINPUT {
        public int dx; public int dy; public uint mouseData; public uint dwFlags;
        public uint time; public IntPtr dwExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct KEYBDINPUT {
        public ushort wVk; public ushort wScan; public uint dwFlags;
        public uint time; public IntPtr dwExtraInfo;
    }

    [StructLayout(LayoutKind.Explicit)]
    struct INPUTUNION {
        [FieldOffset(0)] public MOUSEINPUT mi;
        [FieldOffset(0)] public KEYBDINPUT ki;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct INPUT { public uint type; public INPUTUNION u; }

    delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    public class ForegroundChange {
        public string Time;
        public uint Pid;
        public string Window;
        // The game window (GameWindow) still existed and was visible: the
        // window took the foreground from it, rather than after it closed.
        public bool GameShown;
    }

    // The game window, for the watcher; the driver sets it.
    public static volatile IntPtr GameWindow = IntPtr.Zero;

    [DllImport("user32.dll", SetLastError = true)]
    static extern uint SendInput(uint count, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr hWnd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int size);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] static extern uint MapVirtualKey(uint code, uint mapType);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindowsProc proc, IntPtr lParam);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    static extern int GetClassName(IntPtr hWnd, StringBuilder name, int size);

    const uint INPUT_MOUSE = 0, INPUT_KEYBOARD = 1;
    const uint MOUSEEVENTF_LEFTDOWN = 0x02, MOUSEEVENTF_LEFTUP = 0x04;
    const uint MOUSEEVENTF_RIGHTDOWN = 0x08, MOUSEEVENTF_RIGHTUP = 0x10;
    const uint KEYEVENTF_EXTENDEDKEY = 0x1, KEYEVENTF_KEYUP = 0x2;
    public const uint WM_CLOSE = 0x10;

    static void Send(INPUT input) {
        INPUT[] inputs = new INPUT[] { input };
        if (SendInput(1, inputs, Marshal.SizeOf(typeof(INPUT))) != 1)
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
    }

    public static void Button(bool right, bool down) {
        INPUT input = new INPUT();
        input.type = INPUT_MOUSE;
        input.u.mi.dwFlags = right ? (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP)
                                   : (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
        Send(input);
    }

    public static void Key(ushort vk, bool down, bool extended) {
        INPUT input = new INPUT();
        input.type = INPUT_KEYBOARD;
        input.u.ki.wVk = vk;
        input.u.ki.wScan = (ushort)MapVirtualKey(vk, 0);
        input.u.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP) | (extended ? KEYEVENTF_EXTENDEDKEY : 0);
        Send(input);
    }

    public static POINT ToScreen(IntPtr hWnd, int x, int y) {
        POINT p = new POINT();
        p.X = x;
        p.Y = y;
        if (!ClientToScreen(hWnd, ref p))
            throw new InvalidOperationException("ClientToScreen failed");
        return p;
    }

    // "0xHWND CLASS 'TITLE' pid N", or "none".
    public static string Describe(IntPtr hWnd) {
        if (hWnd == IntPtr.Zero)
            return "none";
        uint pid;
        GetWindowThreadProcessId(hWnd, out pid);
        StringBuilder name = new StringBuilder(256);
        GetClassName(hWnd, name, name.Capacity);
        StringBuilder title = new StringBuilder(256);
        GetWindowText(hWnd, title, title.Capacity);
        return string.Format("0x{0:x} {1} '{2}' pid {3}", hWnd.ToInt64(), name, title, pid);
    }

    // Every change of the foreground window, polled every 2 ms on a thread
    // of its own, so a window that holds the foreground for a few
    // milliseconds is seen too.
    static System.Collections.Concurrent.ConcurrentQueue<ForegroundChange> changes =
        new System.Collections.Concurrent.ConcurrentQueue<ForegroundChange>();
    static System.Threading.Thread watcher;

    public static void WatchForeground() {
        if (watcher != null)
            return;
        watcher = new System.Threading.Thread(delegate() {
            IntPtr last = IntPtr.Zero;
            while (true) {
                IntPtr now = GetForegroundWindow();
                if (now != last) {
                    last = now;
                    ForegroundChange change = new ForegroundChange();
                    change.Time = DateTime.Now.ToString("HH:mm:ss.fff");
                    uint pid = 0;
                    if (now != IntPtr.Zero)
                        GetWindowThreadProcessId(now, out pid);
                    change.Pid = pid;
                    change.Window = Describe(now);
                    IntPtr game = GameWindow;
                    change.GameShown = game != IntPtr.Zero && IsWindow(game) && IsWindowVisible(game);
                    changes.Enqueue(change);
                }
                System.Threading.Thread.Sleep(2);
            }
        });
        watcher.IsBackground = true;
        watcher.Start();
    }

    public static ForegroundChange[] ForegroundChanges() {
        List<ForegroundChange> list = new List<ForegroundChange>();
        ForegroundChange change;
        while (changes.TryDequeue(out change))
            list.Add(change);
        return list.ToArray();
    }

    // The visible top-level windows of a process, optionally of one class.
    public static IntPtr[] Windows(int pid, string className) {
        List<IntPtr> found = new List<IntPtr>();
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam) {
            uint owner;
            GetWindowThreadProcessId(hWnd, out owner);
            if (owner != (uint)pid || !IsWindowVisible(hWnd))
                return true;
            if (className != null) {
                StringBuilder name = new StringBuilder(256);
                GetClassName(hWnd, name, name.Capacity);
                if (name.ToString() != className)
                    return true;
            }
            found.Add(hWnd);
            return true;
        }, IntPtr.Zero);
        return found.ToArray();
    }
}
'@
Add-Type -TypeDefinition $source -Language CSharp
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

# Mutes the sandbox's default playback device through Core Audio. This mutes
# every sound in the sandbox, unlike a mute key press, which only toggles
# and cannot be checked.
Add-Type -Language CSharp -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

[Guid("5CDF2C82-841E-4546-9722-0CF74078229A"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown), ComImport]
interface IAudioEndpointVolume {
    int RegisterControlChangeNotify(IntPtr notify);
    int UnregisterControlChangeNotify(IntPtr notify);
    int GetChannelCount(out uint count);
    int SetMasterVolumeLevel(float level, ref Guid context);
    int SetMasterVolumeLevelScalar(float level, ref Guid context);
    int GetMasterVolumeLevel(out float level);
    int GetMasterVolumeLevelScalar(out float level);
    int SetChannelVolumeLevel(uint channel, float level, ref Guid context);
    int SetChannelVolumeLevelScalar(uint channel, float level, ref Guid context);
    int GetChannelVolumeLevel(uint channel, out float level);
    int GetChannelVolumeLevelScalar(uint channel, out float level);
    int SetMute([MarshalAs(UnmanagedType.Bool)] bool mute, ref Guid context);
    int GetMute([MarshalAs(UnmanagedType.Bool)] out bool mute);
}

[Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown), ComImport]
interface IMMDevice {
    int Activate(ref Guid iid, int context, IntPtr parameters, [MarshalAs(UnmanagedType.IUnknown)] out object result);
}

[Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown), ComImport]
interface IMMDeviceEnumerator {
    int EnumAudioEndpoints(int flow, int mask, out IntPtr devices);
    int GetDefaultAudioEndpoint(int flow, int role, out IMMDevice device);
}

[ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
class MMDeviceEnumerator { }

public static class IwdAudio {
    // Returns "muted", or why the device could not be muted.
    public static string MuteDefaultOutput() {
        var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumerator();
        IMMDevice device;
        // eRender = 0, eConsole = 0
        int hr = enumerator.GetDefaultAudioEndpoint(0, 0, out device);
        if (hr != 0) return string.Format("no playback device (0x{0:X8})", hr);
        Guid iid = typeof(IAudioEndpointVolume).GUID;
        object result;
        hr = device.Activate(ref iid, 23 /* CLSCTX_ALL */, IntPtr.Zero, out result);
        if (hr != 0) return string.Format("no volume control (0x{0:X8})", hr);
        var volume = (IAudioEndpointVolume)result;
        Guid context = Guid.Empty;
        hr = volume.SetMute(true, ref context);
        if (hr != 0) return string.Format("SetMute failed (0x{0:X8})", hr);
        bool muted;
        hr = volume.GetMute(out muted);
        if (hr != 0) return string.Format("GetMute failed (0x{0:X8})", hr);
        return muted ? "muted" : "still unmuted";
    }
}
'@

$script:game = $null
$script:window = [IntPtr]::Zero
$script:baseline = @()
# The scenario line being executed, and whether foreground changes count:
# from the first action until quit.
$script:line = 0
$script:watching = $false
$script:focusLosses = New-Object System.Collections.Generic.List[string]

# Logs the foreground changes the watcher saw since the last call. While the
# scenario runs, a window of another process taking the foreground from the
# game window counts as a focus loss: input sent meanwhile may not have
# reached the game. When the game closes its window (it quits by itself), the
# window that comes to the front is no loss.
function Watch-Foreground {
    foreach ($c in [IwdInput]::ForegroundChanges()) {
        Log "  foreground at $($c.Time): $($c.Window)"
        if ($script:watching -and $c.GameShown -and $c.Pid -ne 0 -and $c.Pid -ne [uint32]$script:game.Id) {
            $script:focusLosses.Add("line $($script:line): $($c.Window) at $($c.Time)")
        }
    }
}

# Windows Sandbox's scheduled task \Microsoft\Windows\Registry\rgnupdt-run
# has a logon trigger and runs rgnupdt.exe about 30 s after logon. Its
# console window takes the foreground for some 30 ms; a click then is lost,
# and it was the Gender click in character generation. The task and its
# rgnupdt-cleanup twin are disabled before the game starts, and an instance
# already running is waited for.
function Quiet-Logon {
    $tasks = @()
    try {
        $tasks = @(Get-ScheduledTask -TaskPath '\Microsoft\Windows\Registry\' -ErrorAction Stop |
            Where-Object { $_.TaskName -like 'rgnupdt*' })
    } catch {
        Log "no logon tasks to disable: $_"
    }
    foreach ($t in $tasks) {
        try {
            $null = Disable-ScheduledTask -InputObject $t -ErrorAction Stop
            Log "disabled scheduled task $($t.TaskPath)$($t.TaskName)"
        } catch {
            Log "could not disable $($t.TaskPath)$($t.TaskName): $_"
        }
    }
    $running = @(Get-Process -Name 'rgnupdt' -ErrorAction SilentlyContinue)
    if ($running.Count -gt 0) {
        Log 'waiting for rgnupdt.exe to finish'
        $running | Wait-Process -Timeout 30 -ErrorAction SilentlyContinue
    }
}

function Check {
    if ($DryRun) { return }
    if ($script:game.HasExited) { throw [System.OperationCanceledException]::new('game exited') }
    Watch-Foreground
    if ((Get-Date) -gt $deadline) { throw "scenario timeout ($($cfg.timeout) s)" }
}

function Sleep-Frames([double]$frames) {
    if ($DryRun) { return }
    $end = (Get-Date).AddSeconds($frames / $frameRate)
    while ($true) {
        Check
        $left = ($end - (Get-Date)).TotalMilliseconds
        if ($left -le 0) { return }
        Start-Sleep -Milliseconds ([Math]::Min(100, [Math]::Ceiling($left)))
    }
}

# Runs a file operation on the control folder again while Windows refuses it
# for a moment: a file the engine just deleted can stay pending deletion
# (longer in the sandbox's mapped folder), and creating or opening that name
# meanwhile fails with "Access is denied".
# Windows PowerShell wraps a cmdlet's error in its own exception type, so the
# whole InnerException chain is checked rather than the outer type.
function Is-TransientIo($record) {
    $e = $record.Exception
    while ($e -ne $null) {
        if ($e -is [System.UnauthorizedAccessException] -or $e -is [System.IO.IOException]) { return $true }
        $e = $e.InnerException
    }
    return $false
}

function Retry-Io([scriptblock]$operation) {
    $end = (Get-Date).AddSeconds(5)
    while ($true) {
        try {
            return & $operation
        } catch {
            if (-not (Is-TransientIo $_) -or (Get-Date) -gt $end) { throw }
            Start-Sleep -Milliseconds 20
        }
    }
}

# Creates control\<name> and waits for the engine to delete it (it does on
# its next presented frame).
function Request([string]$name, [double]$seconds) {
    $path = Join-Path $cfg.control $name
    Retry-Io { Set-Content -LiteralPath $path -Value '' -Encoding ASCII -ErrorAction Stop }
    $end = (Get-Date).AddSeconds($seconds)
    # File.Exists, not Test-Path: Windows PowerShell's Test-Path throws
    # "Access is denied" on a file the engine is still deleting, while
    # File.Exists answers false, which is right since the engine handled it.
    while ([System.IO.File]::Exists($path)) {
        Check
        if ((Get-Date) -gt $end) { throw "engine did not answer $name in $seconds s" }
        Start-Sleep -Milliseconds 10
    }
}

function Engine-Frames {
    Request 'frames.req' ($deadline - (Get-Date)).TotalSeconds
    $answer = Join-Path $cfg.control 'frames.txt'
    return [int](Retry-Io { Get-Content -Raw -LiteralPath $answer -ErrorAction Stop }).Trim()
}

function Wait-Frames([int]$frames) {
    if ($DryRun) { return }
    $first = Engine-Frames
    Sleep-Frames $frames
    $done = (Engine-Frames) - $first
    while ($done -lt $frames) {
        Sleep-Frames ([Math]::Min($frames - $done, $frameRate))
        $done = (Engine-Frames) - $first
    }
}

function Find-Window {
    $end = (Get-Date).AddSeconds(120)
    $last = [IntPtr]::Zero
    $stable = 0
    while ((Get-Date) -lt $end) {
        Check
        $ids = [IwdInput]::Windows($script:game.Id, 'SDL_app')
        $current = if ($ids.Count -gt 0) { $ids[0] } else { [IntPtr]::Zero }
        if ($current -ne [IntPtr]::Zero -and $current -eq $last) {
            $stable++
            if ($stable -ge 5) {
                $script:window = $current
                [IwdInput]::GameWindow = $current
                Log ('game window 0x{0:x}' -f $current.ToInt64())
                return
            }
        } else {
            $stable = 0
        }
        $last = $current
        Start-Sleep -Milliseconds 200
    }
    throw 'no game window after 120 s'
}

# Follows the game window if the engine recreated it, and brings it to the
# front (SendInput goes to the foreground window).
function Refresh-Window {
    $ids = [IwdInput]::Windows($script:game.Id, 'SDL_app')
    if ($ids.Count -gt 0 -and -not ($ids -contains $script:window)) {
        $script:window = $ids[0]
        [IwdInput]::GameWindow = $script:window
        Log ('  game window is now 0x{0:x}' -f $script:window.ToInt64())
    }
    if (-not [IwdInput]::SetForegroundWindow($script:window)) {
        Log ('  SetForegroundWindow refused; the foreground is ' + [IwdInput]::Describe([IwdInput]::GetForegroundWindow()))
    }
}

# Seconds a phase of a click or key waits at most for the engine to read the
# mouse buttons: a screen that does not read them, or a load.
$pollTimeout = 1.0

# platform::window::ButtonPolls through the control channel (input.req), or
# $null when the engine did not answer in $seconds.
function Button-Polls([double]$seconds) {
    $path = Join-Path $cfg.control 'input.req'
    Retry-Io { Set-Content -LiteralPath $path -Value '' -Encoding ASCII -ErrorAction Stop }
    $end = (Get-Date).AddSeconds([Math]::Max($seconds, 0.01))
    while ([System.IO.File]::Exists($path)) {
        Check
        if ((Get-Date) -gt $end) { return $null }
        Start-Sleep -Milliseconds 5
    }
    $answer = Join-Path $cfg.control 'input.txt'
    return [long](Retry-Io { Get-Content -Raw -LiteralPath $answer -ErrorAction Stop }).Trim()
}

# A phase of a click or key press: N frames of wall-clock time, and until the
# engine has read the mouse buttons after the phase began (at most
# $pollTimeout). The engine polls input once per AI update and reads only
# "down" and "pressed since the last poll" (GetAsyncKeyState); in the
# sandbox its updates stalled for up to 165 ms, so a release and the next
# press could fall between two polls and read as one press: the portrait
# double click that centres the view became one click. Waiting for presented
# frames instead put a double click's presses too far apart for the
# engine's double-click time.
function Hold-Frames([int]$frames) {
    if ($DryRun) { return }
    $start = Get-Date
    $end = $start.AddSeconds($pollTimeout)
    $first = Button-Polls $pollTimeout
    $left = ($start.AddSeconds($frames / $frameRate) - (Get-Date)).TotalSeconds
    if ($left -gt 0) { Sleep-Frames ($left * $frameRate) }
    while ($first -ne $null) {
        $now = Button-Polls ($end - (Get-Date)).TotalSeconds
        if ($now -ne $null -and $now -gt $first) { return }
        if ((Get-Date) -ge $end) { break }
    }
    Log "  the engine did not read the mouse buttons in $pollTimeout s"
}

# -Modal: the engine is inside a message box and presents no frames.
function Press-Key($action, [switch]$Modal) {
    $vk = [uint16]$action.vk
    $extended = [bool]$action.extended
    $phase = if ($Modal) { { param($n) Sleep-Frames $n } } else { { param($n) Hold-Frames $n } }
    # The modifier VK codes the scenario named (windows_actions' mods), in
    # the order it wrote them; held down before the key and released after
    # it in reverse. Quit-Game's synthetic action has no mods property.
    $mods = @()
    if ($action.PSObject.Properties['mods']) { $mods = @($action.mods) }
    foreach ($m in $mods) { [IwdInput]::Key($m, $true, $false); & $phase 1 }
    if ($action.shift) { [IwdInput]::Key(0x10, $true, $false); & $phase 1 }
    [IwdInput]::Key($vk, $true, $extended)
    & $phase 2
    [IwdInput]::Key($vk, $false, $extended)
    if ($action.shift) { & $phase 1; [IwdInput]::Key(0x10, $false, $false) }
    for ($i = $mods.Count - 1; $i -ge 0; $i--) { & $phase 1; [IwdInput]::Key($mods[$i], $false, $false) }
    & $phase 2
}

function Screen-Shot([string]$name) {
    $path = Join-Path $cfg.results "$name-screen.png"
    $bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
    $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bitmap.Size)
        $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
    Log "  wrote $name-screen.png"
}

function Engine-Shot([string]$name) {
    Request "shot-$name.req" 60
    if (-not [System.IO.File]::Exists((Join-Path $cfg.results "$name.png"))) {
        throw "engine wrote no $name.png"
    }
    Log "  wrote $name.png"
}

function State-Dump([string]$name) {
    Request "state-$name.req" 60
    if (-not [System.IO.File]::Exists((Join-Path $cfg.results "checkpoint-$name.json"))) {
        throw "engine wrote no checkpoint-$name.json"
    }
    Log "  wrote checkpoint-$name.json"
}

function Quit-Game {
    while ($true) {
        Refresh-Window
        [void][IwdInput]::PostMessage($script:window, [IwdInput]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
        Log ('  posted WM_CLOSE to 0x{0:x}' -f $script:window.ToInt64())
        $end = (Get-Date).AddSeconds(10)
        while ((Get-Date) -lt $end) {
            Check
            $dialogs = @([IwdInput]::Windows($script:game.Id, '#32770'))
            if ($dialogs.Count -gt 0) {
                Log ('  confirmation window 0x{0:x}: Return' -f $dialogs[0].ToInt64())
                [void][IwdInput]::SetForegroundWindow($dialogs[0])
                Sleep-Frames 2
                Press-Key ([pscustomobject]@{ vk = 0x0D; extended = $false; shift = $false }) -Modal
                return
            }
            Start-Sleep -Milliseconds 100
        }
        Log '  no confirmation window after 10 s'
    }
}

function Describe($a) {
    switch ($a.op) {
        'wait' { "wait $($a.frames)" }
        'key' {
            $mods = ''
            if ($a.PSObject.Properties['mods'] -and $a.mods) {
                $mods = ($a.mods | ForEach-Object {
                        # The VK code back as its scenario name.
                        switch ([int]$_) {
                            0x11 { 'CTRL+' } 0x10 { 'SHIFT+' } 0x12 { 'ALT+' }
                            default { '' }
                        }
                    }) -join ''
            }
            $shift = if ($a.shift) { 'SHIFT+' } else { '' }
            "key $mods$shift$($a.key)"
        }
        'shot' { "shot $($a.name)" }
        'checkpoint' { "checkpoint $($a.name)" }
        'quit' { 'quit' }
        default { "$($a.op) $($a.x) $($a.y)" }
    }
}

function Do-Action($a) {
    if ($DryRun) {
        switch ($a.op) {
            'wait' { Log ('  would wait {0:0.###} s and {1} engine frames' -f ($a.frames / $frameRate), $a.frames) }
            { $_ -in 'move', 'click', 'rclick' } {
                $what = if ($a.op -eq 'move') { '' } elseif ($a.op -eq 'click') { ', left button down/up' } else { ', right button down/up' }
                Log "  would SetCursorPos(ClientToScreen(game, $($a.x), $($a.y)))$what"
            }
            'key' {
                $mods = @()
                if ($a.PSObject.Properties['mods']) { $mods = @($a.mods) }
                $modsText = if ($mods.Count) { ' mods ' + (($mods | ForEach-Object { '0x{0:X2}' -f [int]$_ }) -join ',') } else { '' }
                Log ('  would SendInput VK 0x{0:X2} (extended {1}, shift {2}{3})' -f [int]$a.vk, $a.extended, $a.shift, $modsText)
            }
            'shot' { Log "  would capture the screen to $($a.name)-screen.png and request $($a.name).png" }
            'checkpoint' { Log "  would request checkpoint-$($a.name).json" }
            'quit' { Log '  would post WM_CLOSE and confirm the dialog with Return' }
            default { throw "unknown action $($a.op)" }
        }
        return
    }
    switch ($a.op) {
        'wait' { Wait-Frames ([int]$a.frames) }
        { $_ -in 'move', 'click', 'rclick' } {
            Refresh-Window
            $p = [IwdInput]::ToScreen($script:window, [int]$a.x, [int]$a.y)
            [void][IwdInput]::SetCursorPos($p.X, $p.Y)
            Hold-Frames 1
            if ($a.op -ne 'move') {
                $right = $a.op -eq 'rclick'
                [IwdInput]::Button($right, $true)
                Hold-Frames 2
                [IwdInput]::Button($right, $false)
                Hold-Frames 2
            }
        }
        'key' { Refresh-Window; Press-Key $a }
        'shot' { Screen-Shot $a.name; Engine-Shot $a.name }
        'checkpoint' { State-Dump $a.name }
        'quit' { Watch-Foreground; $script:watching = $false; Quit-Game }
        default { throw "unknown action $($a.op)" }
    }
}

$errorText = $null
$exitCode = $null
# The real run changes the machine it runs on (audio, services, scheduled
# tasks, a shutdown at the end): only inside Windows Sandbox, whose user is
# WDAGUtilityAccount. Elsewhere only -DryRun runs.
$inSandbox = [Environment]::UserName -eq 'WDAGUtilityAccount'
try {
    Log "scenario $($cfg.scenario): $($actions.Count) actions"
    $gpu = (Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Select-Object -First 1).Name
    Log "video adapter: $gpu"
    if ($DryRun) {
        Log "would run $($cfg.exe) with IWD2_DATA=$($cfg.data) IWD2R_INI_OVERRIDE=$($cfg.ini_override)"
    } else {
        if (-not $inSandbox) {
            throw "driver.ps1 runs only inside Windows Sandbox (user WDAGUtilityAccount, not '$([Environment]::UserName)'); use -DryRun elsewhere"
        }
        Log "sandbox user $([Environment]::UserName)"
        # The sandbox's sound reaches the host's speakers. Mute its whole
        # output before anything else runs.
        $muteState = 'failed'
        try { $muteState = [IwdAudio]::MuteDefaultOutput() } catch { $muteState = "failed: $_" }
        Log "sandbox audio: $muteState"
        # A muted device can still be heard on the host on some systems, so
        # the sandbox's audio service is stopped too. The sandbox user is an
        # administrator; the game then runs without an audio device.
        foreach ($service in 'Audiosrv', 'AudioEndpointBuilder') {
            try {
                Stop-Service -Name $service -Force -ErrorAction Stop
                Log "stopped $service"
            } catch {
                Log "could not stop ${service}: $_"
            }
        }
        New-Item -ItemType Directory -Force -Path $cfg.control | Out-Null
        $env:IWD2_DATA = $cfg.data
        $env:IWD2R_INI_OVERRIDE = $cfg.ini_override
        $env:IWD2R_SHOT_DIR = $cfg.results
        $env:IWD2R_CONTROL_DIR = $cfg.control
        Remove-Item Env:IWD2R_SCRIPT -ErrorAction SilentlyContinue
        Quiet-Logon
        [IwdInput]::WatchForeground()
        $script:game = Start-Process -FilePath $cfg.exe -WorkingDirectory $cfg.data -PassThru -NoNewWindow `
            -RedirectStandardOutput (Join-Path $cfg.results 'log.txt') `
            -RedirectStandardError (Join-Path $cfg.results 'stderr.txt')
        $null = $script:game.Handle # keeps ExitCode readable after exit
        Log "game pid $($script:game.Id)"
        Find-Window
    }
    try {
        $script:watching = -not $DryRun
        foreach ($a in $actions) {
            $script:line = $a.line
            Log "line $($a.line): $(Describe $a)"
            Do-Action $a
        }
        Log 'scenario finished'
    } catch [System.OperationCanceledException] {
        Log 'game exited'
    } finally {
        if (-not $DryRun) { Watch-Foreground }
        $script:watching = $false
    }
    if (-not $DryRun) {
        $left = [Math]::Max(1, ($deadline - (Get-Date)).TotalMilliseconds)
        if (-not $script:game.WaitForExit([int]$left)) { throw "scenario timeout ($($cfg.timeout) s)" }
        $exitCode = $script:game.ExitCode
        if ($script:focusLosses.Count -gt 0) {
            throw ("the game lost the foreground {0} time(s) while the scenario ran, so input may have missed it; first {1}" -f $script:focusLosses.Count, $script:focusLosses[0])
        }
    }
} catch {
    $errorText = "$_"
    if ($script:game -ne $null -and $script:game.HasExited) { $exitCode = $script:game.ExitCode }
    Log "driver error: $errorText"
    Log "  exception $($_.Exception.GetType().FullName); inner $($_.Exception.InnerException)"
    Log "  at $($_.InvocationInfo.ScriptLineNumber): $($_.InvocationInfo.Line.Trim())"
    Log "  stack $($_.ScriptStackTrace -replace '\r?\n', ' <- ')"
} finally {
    if ($script:game -ne $null -and -not $script:game.HasExited) {
        Stop-Process -Id $script:game.Id -Force -ErrorAction SilentlyContinue
    }
    Log "exit code $exitCode"
    if (-not $DryRun -and $inSandbox) {
        Set-Content -LiteralPath (Join-Path $cfg.results 'exitcode.txt') -Value "$exitCode" -Encoding ASCII
        if ($errorText) {
            Set-Content -LiteralPath (Join-Path $cfg.results 'error.txt') -Value $errorText -Encoding UTF8
        }
        Set-Content -LiteralPath (Join-Path $cfg.results 'done.txt') -Value 'done' -Encoding ASCII
        if ($cfg.shutdown) { shutdown.exe /s /t 0 }
    }
}
if ($errorText) { exit 1 }
