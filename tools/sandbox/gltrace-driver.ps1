<#
.SYNOPSIS
Traces one scenario of the IWD2EE build with apitrace in Windows Sandbox.

.DESCRIPTION
tools/gltrace.py writes config.json into a read-only tools folder and
starts the sandbox with this script as its logon command. The script
mutes the sandbox and stops its audio services (as driver.ps1 does),
disables the rgnupdt* logon tasks (driver.ps1), replaces the sandbox's
DirectPlay stubs with the host's files (replay-driver.ps1; IWD2EE.exe
loads its libraries dynamically, so this is insurance against the
"Windows Features" prompt), and runs

  apitrace.exe trace --api gl -m -o <results>\<scenario>.trace IWD2EE.exe

in the game folder. -m hooks with mhook instead of IAT patching:
IWD2EE.exe imports only kernel32, user32 and version and loads
OpenGL32.dll dynamically, so there is no IAT entry to patch. The game is
apitrace's child; the script finds its window among the visible
top-level windows of apitrace's descendants and drives the scenario's
input actions with SendInput, as driver.ps1 does. Waits are wall-clock
only: the original has none of iwd2r's control channel, so frames
cannot be polled, and shot/checkpoint ops (gltrace.py drops them) are
logged and skipped.

After the last action the script closes the game (WM_CLOSE, its
confirmation dialog with Return, then a force kill); apitrace flushes
the trace when the game exits, and the script waits for it and checks
the trace is there and non-empty. When it is not, an antivirus inside
the sandbox may have blocked the injection: the script adds a Microsoft
Defender exclusion for the mapped folders and runs the whole trace once
more (attempt1-* keeps the first try's files), as replay-driver.ps1
does.

Foreground losses are logged, not fatal: this is a capture tool, and
the trace of what did run is still worth keeping.

The results folder receives driver.log, trace-stdout.txt (the game's
stdout passes through apitrace), trace-stderr.txt, <scenario>.trace,
exitcode.txt (apitrace's exit code), error.txt (only when the driver
failed) and finally done.txt; then the sandbox shuts down.

-DryRun reads the config and prints what it would do, without running
anything or writing files; it is safe on the host desktop.
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
$stdoutPath = Join-Path $cfg.results 'trace-stdout.txt'
$stderrPath = Join-Path $cfg.results 'trace-stderr.txt'

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
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

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
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

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

    // The window's pixel area, or 0.
    public static int Area(IntPtr hWnd) {
        RECT r;
        if (!GetWindowRect(hWnd, out r)) return 0;
        return Math.Max(0, r.Right - r.Left) * Math.Max(0, r.Bottom - r.Top);
    }

    // The process owning a window.
    public static uint PidOf(IntPtr hWnd) {
        uint pid;
        GetWindowThreadProcessId(hWnd, out pid);
        return pid;
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
    // The callback is a static method behind a static delegate field: an
    // inline anonymous delegate passed straight to EnumWindows was observed
    // to silently enumerate nothing (a reversed delegate must stay rooted
    // for the whole native call; a static field guarantees that).
    static IntPtr[] windowsArg = new IntPtr[0];
    static List<IntPtr> windowsFound;
    static int windowsPid;
    static string windowsClass;
    static bool WindowsCallback(IntPtr hWnd, IntPtr lParam) {
        uint owner;
        GetWindowThreadProcessId(hWnd, out owner);
        if (owner != (uint)windowsPid || !IsWindowVisible(hWnd))
            return true;
        if (!string.IsNullOrEmpty(windowsClass)) {
            StringBuilder name = new StringBuilder(256);
            GetClassName(hWnd, name, name.Capacity);
            if (name.ToString() != windowsClass)
                return true;
        }
        windowsFound.Add(hWnd);
        return true;
    }
    static EnumWindowsProc windowsProc = new EnumWindowsProc(WindowsCallback);
    public static IntPtr[] Windows(int pid, string className) {
        windowsFound = new List<IntPtr>();
        windowsPid = pid;
        windowsClass = className;
        EnumWindows(windowsProc, IntPtr.Zero);
        return windowsFound.ToArray();
    }
}
'@
Add-Type -TypeDefinition $source -Language CSharp
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

# Mutes the sandbox's default playback device through Core Audio (the same
# code as driver.ps1).
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
    public static string MuteDefaultOutput() {
        var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumerator();
        IMMDevice device;
        int hr = enumerator.GetDefaultAudioEndpoint(0, 0, out device);
        if (hr != 0) return string.Format("no playback device (0x{0:X8})", hr);
        Guid iid = typeof(IAudioEndpointVolume).GUID;
        object result;
        hr = device.Activate(ref iid, 23, IntPtr.Zero, out result);
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

$script:tracer = $null
$script:game = $null
$script:gamePid = [uint32]0
$script:window = [IntPtr]::Zero
# The scenario line being executed, and whether foreground changes count:
# from the first action until the last one.
$script:line = 0
$script:watching = $false
$script:focusLosses = New-Object System.Collections.Generic.List[string]

# Logs the foreground changes the watcher saw since the last call. While the
# scenario runs, a window of another process taking the foreground from the
# game window is a focus loss: input sent meanwhile may not have reached the
# game. Logged, not fatal (see the header comment).
function Watch-Foreground {
    foreach ($c in [IwdInput]::ForegroundChanges()) {
        Log "  foreground at $($c.Time): $($c.Window)"
        if ($script:watching -and $c.GameShown -and $c.Pid -ne 0 -and $c.Pid -ne $script:gamePid) {
            $script:focusLosses.Add("line $($script:line): $($c.Window) at $($c.Time)")
        }
    }
}

# Windows Sandbox's scheduled task \Microsoft\Windows\Registry\rgnupdt-run
# has a logon trigger and runs rgnupdt.exe about 30 s after logon. Its
# console window takes the foreground for some 30 ms; a click then is lost.
# The task and its rgnupdt-cleanup twin are disabled before the game starts,
# and an instance already running is waited for. (driver.ps1)
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

function Defender-State {
    try {
        $s = Get-MpComputerStatus -ErrorAction Stop
        return "service $($s.AMServiceEnabled), real-time protection $($s.RealTimeProtectionEnabled)"
    } catch {
        return "unavailable ($_)"
    }
}

function Add-Exclusions {
    foreach ($path in $cfg.exclusions) {
        try {
            Add-MpPreference -ExclusionPath $path -ErrorAction Stop
            Log "added a Defender exclusion for $path"
        } catch {
            Log "could not add a Defender exclusion for ${path}: $_"
        }
    }
    try {
        $now = (Get-MpPreference -ErrorAction Stop).ExclusionPath -join ', '
        Log "Defender exclusions now: $now"
    } catch {
        Log "could not read the Defender exclusions: $_"
    }
}

# Installs DirectPlay, which iwd2.exe imports (DPLAYX.dll) and IWD2EE.exe
# may probe for. The sandbox's SysWOW64 DirectPlay files are 10 KB Features
# on Demand stubs, and loading them makes Windows suspend the game behind a
# "Windows Features" prompt; the host copies its own 32-bit DirectPlay files
# into the tools folder (config "directplay") and this replaces the stubs
# with them. (replay-driver.ps1)
function Install-DirectPlay {
    $target = Join-Path $env:windir 'SysWOW64'
    foreach ($file in @(Get-ChildItem -LiteralPath $cfg.directplay -File)) {
        $dest = Join-Path $target $file.Name
        $want = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        if ([System.IO.File]::Exists($dest)) {
            $have = (Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash
            if ($have -eq $want) { continue }
            $size = (Get-Item -LiteralPath $dest).Length
            & takeown.exe /f $dest 2>&1 | Out-Null
            & icacls.exe $dest /grant '*S-1-5-32-544:F' 2>&1 | Out-Null
        } else {
            $size = 'absent'
        }
        Copy-Item -LiteralPath $file.FullName -Destination $dest -Force -ErrorAction Stop
        Log "DirectPlay: replaced $($file.Name) in SysWOW64 ($size -> $($file.Length) bytes)"
    }
}

# Windows' "Windows Features" prompt (Fondue.exe) means the game needs an
# optional feature the sandbox lacks; it suspends the game until answered
# and any answer kills it. (replay-driver.ps1)
function Check-Feature-Prompt {
    $prompts = @(Get-Process -Name Fondue -ErrorAction SilentlyContinue)
    if ($prompts.Count -eq 0) { return }
    $line = (Get-CimInstance Win32_Process -Filter "ProcessId = $($prompts[0].Id)" -ErrorAction SilentlyContinue).CommandLine
    Log "Windows asks to install a feature: $line"
    Screen-Shot 'stuck'
    $prompts | ForEach-Object { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue }
    throw "Windows Features prompt ($line)"
}

function Check {
    if ($DryRun) { return }
    if ($script:tracer -ne $null -and $script:tracer.HasExited) {
        throw "apitrace exited early (code $($script:tracer.ExitCode))"
    }
    if ($script:game -ne $null -and $script:game.HasExited) {
        throw [System.OperationCanceledException]::new('game exited')
    }
    Watch-Foreground
    Check-Feature-Prompt
    if ((Get-Date) -gt $deadline) { throw "scenario timeout ($($cfg.timeout) s)" }
}

# Wall-clock only: the original has none of iwd2r's control channel, so
# frames cannot be polled; a frame is 1/$frameRate seconds.
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

# The pids of apitrace's descendants, three levels deep (apitrace.exe ->
# the game -> any child it spawns).
function Get-DescendantIds([uint32]$root) {
    $all = New-Object System.Collections.Generic.List[uint32]
    $frontier = New-Object System.Collections.Generic.List[uint32]
    $frontier.Add($root)
    for ($depth = 0; $depth -lt 3 -and $frontier.Count -gt 0; $depth++) {
        $next = New-Object System.Collections.Generic.List[uint32]
        foreach ($parent in $frontier) {
            $children = @(Get-CimInstance -Query "SELECT ProcessId FROM Win32_Process WHERE ParentProcessId = $parent" -ErrorAction SilentlyContinue)
            foreach ($c in $children) {
                $id = [uint32]$c.ProcessId
                if (-not $all.Contains($id)) {
                    $all.Add($id)
                    $next.Add($id)
                }
            }
        }
        $frontier = $next
    }
    return ,$all
}

# The game window: the largest visible top-level window of apitrace's
# descendants, stable over five 200 ms polls, within 180 s. (There is no
# known window class: the game is not iwd2r's SDL_app.)
function Find-GameWindow {
    $end = (Get-Date).AddSeconds(180)
    $last = [IntPtr]::Zero
    $stable = 0
    while ((Get-Date) -lt $end) {
        Check
        $best = [IntPtr]::Zero
        $bestArea = 0
        foreach ($procId in (Get-DescendantIds $script:tracer.Id)) {
            foreach ($h in [IwdInput]::Windows([int]$procId, $null)) {
                $a = [IwdInput]::Area($h)
                if ($a -gt $bestArea) { $bestArea = $a; $best = $h }
            }
        }
        if ($best -ne [IntPtr]::Zero -and $best -eq $last) {
            $stable++
            if ($stable -ge 5) {
                $script:window = $best
                [IwdInput]::GameWindow = $best
                $script:gamePid = [IwdInput]::PidOf($best)
                $script:game = Get-Process -Id $script:gamePid -ErrorAction Stop
                Log ('game window 0x{0:x}, {1} px area, pid {2}' -f $best.ToInt64(), $bestArea, $script:gamePid)
                return
            }
        } else {
            $stable = 0
        }
        $last = $best
        Start-Sleep -Milliseconds 200
    }
    throw 'no game window after 180 s'
}

# Follows the game window if the process recreated it, and brings it to the
# front (SendInput goes to the foreground window).
function Refresh-Window {
    if ($script:game -eq $null -or $script:game.HasExited) { return }
    $best = [IntPtr]::Zero
    $bestArea = 0
    foreach ($h in [IwdInput]::Windows([int]$script:gamePid, $null)) {
        $a = [IwdInput]::Area($h)
        if ($a -gt $bestArea) { $bestArea = $a; $best = $h }
    }
    if ($best -ne [IntPtr]::Zero -and $best -ne $script:window) {
        $script:window = $best
        [IwdInput]::GameWindow = $best
        Log ('  game window is now 0x{0:x}' -f $best.ToInt64())
    }
    if ($script:window -ne [IntPtr]::Zero -and -not [IwdInput]::SetForegroundWindow($script:window)) {
        Log ('  SetForegroundWindow refused; the foreground is ' + [IwdInput]::Describe([IwdInput]::GetForegroundWindow()))
    }
}

# A click or key phase is N frames of wall-clock time.
function Press-Key($action) {
    $vk = [uint16]$action.vk
    $extended = [bool]$action.extended
    if ($action.shift) { [IwdInput]::Key(0x10, $true, $false); Sleep-Frames 1 }
    [IwdInput]::Key($vk, $true, $extended)
    Sleep-Frames 2
    [IwdInput]::Key($vk, $false, $extended)
    if ($action.shift) { Sleep-Frames 1; [IwdInput]::Key(0x10, $false, $false) }
    Sleep-Frames 2
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

function Describe($a) {
    switch ($a.op) {
        'wait' { "wait $($a.frames)" }
        'key' { if ($a.shift) { "key SHIFT+$($a.key)" } else { "key $($a.key)" } }
        'shot' { "shot $($a.name)" }
        'checkpoint' { "checkpoint $($a.name)" }
        'quit' { 'quit' }
        default { "$($a.op) $($a.x) $($a.y)" }
    }
}

# Returns $true when the run should stop (the quit op).
function Do-Action($a) {
    if ($DryRun) {
        switch ($a.op) {
            'wait' { Log ('  would wait {0:0.###} s (wall clock, {1} frames)' -f ($a.frames / $frameRate), $a.frames) }
            { $_ -in 'move', 'click', 'rclick' } {
                $what = if ($a.op -eq 'move') { '' } elseif ($a.op -eq 'click') { ', left button down/up' } else { ', right button down/up' }
                Log "  would SetCursorPos(ClientToScreen(game, $($a.x), $($a.y)))$what"
            }
            'key' { Log ('  would SendInput VK 0x{0:X2} (extended {1}, shift {2})' -f [int]$a.vk, $a.extended, $a.shift) }
            'shot' { Log "  would capture the screen to $($a.name)-screen.png" }
            'checkpoint' { Log '  is a milestone; logged and skipped (no control channel)' }
            'quit' { Log '  would close the game' }
            default { throw "unknown action $($a.op)" }
        }
        return $false
    }
    switch ($a.op) {
        'wait' { Sleep-Frames ([int]$a.frames) }
        { $_ -in 'move', 'click', 'rclick' } {
            Refresh-Window
            $p = [IwdInput]::ToScreen($script:window, [int]$a.x, [int]$a.y)
            [void][IwdInput]::SetCursorPos($p.X, $p.Y)
            Sleep-Frames 1
            if ($a.op -ne 'move') {
                $right = $a.op -eq 'rclick'
                [IwdInput]::Button($right, $true)
                Sleep-Frames 2
                [IwdInput]::Button($right, $false)
                Sleep-Frames 2
            }
        }
        'key' { Refresh-Window; Press-Key $a }
        'shot' { Screen-Shot $a.name }
        'checkpoint' { Log "  checkpoint $($a.name)" }
        'quit' { return $true }
        default { throw "unknown action $($a.op)" }
    }
    return $false
}

# One trace attempt: apitrace trace, find the game window, run the actions,
# close the game and wait for the trace. Throws when there is no trace.
function Run-Trace {
    $arguments = @('trace', '--api', 'gl', '-m', '-o',
        "`"$($cfg.trace)`"", "`"$($cfg.exe)`"")
    Log "running $($cfg.apitrace) $($arguments -join ' ')"
    Log "  in $($cfg.data) (the game inherits it as its working directory)"
    $script:tracer = Start-Process -FilePath $cfg.apitrace -ArgumentList $arguments `
        -WorkingDirectory $cfg.data -PassThru -NoNewWindow `
        -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    $null = $script:tracer.Handle # keeps ExitCode readable after exit
    Log "apitrace pid $($script:tracer.Id)"
    Find-GameWindow
    Screen-Shot 'ready'
    try {
        $script:watching = -not $DryRun
        foreach ($a in $actions) {
            $script:line = $a.line
            Log "line $($a.line): $(Describe $a)"
            if (Do-Action $a) { break }
        }
        Log 'scenario finished'
    } catch [System.OperationCanceledException] {
        Log 'game exited'
    } finally {
        if (-not $DryRun) { Watch-Foreground }
        $script:watching = $false
    }
    End-Run
}

# Closes the game (the trace is flushed when it exits), waits for apitrace
# and verifies the trace.
function End-Run {
    Screen-Shot 'end'
    if ($script:game -ne $null -and -not $script:game.HasExited) {
        Refresh-Window
        if ($script:window -ne [IntPtr]::Zero) {
            [void][IwdInput]::PostMessage($script:window, [IwdInput]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
            Log ('  posted WM_CLOSE to 0x{0:x}' -f $script:window.ToInt64())
        }
        $end = (Get-Date).AddSeconds(15)
        while ((Get-Date) -lt $end -and -not $script:game.HasExited) {
            $dialogs = @([IwdInput]::Windows([int]$script:gamePid, '#32770'))
            if ($dialogs.Count -gt 0) {
                Log ('  confirmation window 0x{0:x}: Return' -f $dialogs[0].ToInt64())
                [void][IwdInput]::SetForegroundWindow($dialogs[0])
                Start-Sleep -Milliseconds 200
                [IwdInput]::Key(0x0D, $true, $false)
                Start-Sleep -Milliseconds 50
                [IwdInput]::Key(0x0D, $false, $false)
            }
            Start-Sleep -Milliseconds 200
        }
        if (-not $script:game.HasExited) {
            Log '  the game is still running after WM_CLOSE; killing it'
            Stop-Process -Id $script:gamePid -Force -ErrorAction SilentlyContinue
            $script:game.WaitForExit(10000) | Out-Null
        }
    }
    Log "waiting for apitrace to flush the trace ($($cfg.trace_wait) s)"
    $end = (Get-Date).AddSeconds([double]$cfg.trace_wait)
    while (-not $script:tracer.WaitForExit(1000)) {
        if ((Get-Date) -gt $end) {
            Log '  apitrace did not exit; killing it'
            Stop-Process -Id $script:tracer.Id -Force -ErrorAction SilentlyContinue
            $script:tracer.WaitForExit(10000) | Out-Null
            break
        }
    }
    if (-not [System.IO.File]::Exists($cfg.trace)) { throw "no trace at $($cfg.trace)" }
    $size = (Get-Item -LiteralPath $cfg.trace).Length
    if ($size -le 0) { throw "the trace at $($cfg.trace) is empty" }
    Log "trace: $size bytes"
}

# Keeps the first attempt's files as attempt1-*.
function Set-Aside {
    foreach ($path in $stdoutPath, $stderrPath) {
        if (Test-Path -LiteralPath $path) {
            Move-Item -LiteralPath $path -Destination (Join-Path $cfg.results ('attempt1-' + (Split-Path -Leaf $path))) -Force
        }
    }
    if (Test-Path -LiteralPath $cfg.trace) {
        Move-Item -LiteralPath $cfg.trace -Destination (Join-Path $cfg.results ('attempt1-' + (Split-Path -Leaf $cfg.trace))) -Force
    }
}

$errorText = $null
$exitCode = $null
# The real run changes the machine it runs on (audio, services, scheduled
# tasks, a shutdown at the end): only inside Windows Sandbox, whose user is
# WDAGUtilityAccount. Elsewhere only -DryRun runs.
$inSandbox = [Environment]::UserName -eq 'WDAGUtilityAccount'
try {
    Log "scenario $($cfg.scenario): $($actions.Count) actions, timeout $($cfg.timeout) s"
    $gpu = (Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Select-Object -First 1).Name
    Log "video adapter: $gpu"
    Log "Defender: $(Defender-State)"
    if ($DryRun) {
        Log "would run $($cfg.apitrace) trace --api gl -m -o `"$($cfg.trace)`" `"$($cfg.exe)`" in $($cfg.data)"
        Log "would add Defender exclusions for $($cfg.exclusions -join ', ') if there is no trace, and retry once"
        foreach ($a in $actions) {
            Log "line $($a.line): $(Describe $a)"
            $null = Do-Action $a
        }
    } else {
        if (-not $inSandbox) {
            throw "gltrace-driver.ps1 runs only inside Windows Sandbox (user WDAGUtilityAccount, not '$([Environment]::UserName)'); use -DryRun elsewhere"
        }
        Log "sandbox user $([Environment]::UserName)"
        # The sandbox's sound reaches the host's speakers. Mute its whole
        # output before anything else runs, and stop the audio service too
        # (a muted device can still be heard on the host on some systems).
        $muteState = 'failed'
        try { $muteState = [IwdAudio]::MuteDefaultOutput() } catch { $muteState = "failed: $_" }
        Log "sandbox audio: $muteState"
        foreach ($service in 'Audiosrv', 'AudioEndpointBuilder') {
            try {
                Stop-Service -Name $service -Force -ErrorAction Stop
                Log "stopped $service"
            } catch {
                Log "could not stop ${service}: $_"
            }
        }
        Install-DirectPlay
        Quiet-Logon
        [IwdInput]::WatchForeground()
        try {
            Run-Trace
        } catch {
            if ("$_" -notmatch 'no trace|is empty') { throw }
            Log "attempt 1 looks blocked: $_"
            Log "Defender: $(Defender-State)"
            Add-Exclusions
            Set-Aside
            Start-Sleep -Seconds 2
            $script:game = $null
            $script:gamePid = [uint32]0
            $script:window = [IntPtr]::Zero
            [IwdInput]::GameWindow = [IntPtr]::Zero
            Run-Trace
            Log 'attempt 2 ran'
        }
        $exitCode = $script:tracer.ExitCode
        if ($script:focusLosses.Count -gt 0) {
            Log ("the game lost the foreground {0} time(s) while the scenario ran, so input may have missed it; first {1}" -f $script:focusLosses.Count, $script:focusLosses[0])
        }
    }
} catch {
    $errorText = "$_"
    Log "driver error: $errorText"
    Log "  exception $($_.Exception.GetType().FullName); inner $($_.Exception.InnerException)"
    Log "  at $($_.InvocationInfo.ScriptLineNumber): $($_.InvocationInfo.Line.Trim())"
    Log "  stack $($_.ScriptStackTrace -replace '\r?\n', ' <- ')"
    if (-not $DryRun) {
        try { Screen-Shot 'stuck' } catch { Log "  no screen shot: $_" }
    }
} finally {
    if ($script:tracer -ne $null -and -not $script:tracer.HasExited) {
        Stop-Process -Id $script:tracer.Id -Force -ErrorAction SilentlyContinue
    }
    if ($script:game -ne $null -and -not $script:game.HasExited) {
        Stop-Process -Id $script:gamePid -Force -ErrorAction SilentlyContinue
    }
    if ($script:tracer -ne $null -and $exitCode -eq $null -and $script:tracer.HasExited) {
        $exitCode = $script:tracer.ExitCode
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
