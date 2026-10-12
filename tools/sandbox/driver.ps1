# iwd2r. Distributed under LICENSE.md.
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
(NAME.png) through the IWD2R_CONTROL_DIR channel. `wait N` lasts N/30 s and
until the engine has presented N frames (the same channel). `quit` posts
WM_CLOSE to the game window, what its title bar's close button does, and
confirms the engine's message box with Return.

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

    [DllImport("user32.dll", SetLastError = true)]
    static extern uint SendInput(uint count, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
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

$script:game = $null
$script:window = [IntPtr]::Zero
$script:baseline = @()

function Check {
    if ($DryRun) { return }
    if ($script:game.HasExited) { throw [System.OperationCanceledException]::new('game exited') }
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

# Creates control\<name> and waits for the engine to delete it (it does on
# its next presented frame).
function Request([string]$name, [double]$seconds) {
    $path = Join-Path $cfg.control $name
    Set-Content -LiteralPath $path -Value '' -Encoding ASCII
    $end = (Get-Date).AddSeconds($seconds)
    while (Test-Path -LiteralPath $path) {
        Check
        if ((Get-Date) -gt $end) { throw "engine did not answer $name in $seconds s" }
        Start-Sleep -Milliseconds 10
    }
}

function Engine-Frames {
    Request 'frames.req' ($deadline - (Get-Date)).TotalSeconds
    return [int](Get-Content -Raw -LiteralPath (Join-Path $cfg.control 'frames.txt')).Trim()
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
        Log ('  game window is now 0x{0:x}' -f $script:window.ToInt64())
    }
    [void][IwdInput]::SetForegroundWindow($script:window)
}

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

function Engine-Shot([string]$name) {
    Request "shot-$name.req" 60
    if (-not (Test-Path -LiteralPath (Join-Path $cfg.results "$name.png"))) {
        throw "engine wrote no $name.png"
    }
    Log "  wrote $name.png"
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
                Press-Key ([pscustomobject]@{ vk = 0x0D; extended = $false; shift = $false })
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
        'key' { if ($a.shift) { "key SHIFT+$($a.key)" } else { "key $($a.key)" } }
        'shot' { "shot $($a.name)" }
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
            'key' { Log ('  would SendInput VK 0x{0:X2} (extended {1}, shift {2})' -f [int]$a.vk, $a.extended, $a.shift) }
            'shot' { Log "  would capture the screen to $($a.name)-screen.png and request $($a.name).png" }
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
        'shot' { Screen-Shot $a.name; Engine-Shot $a.name }
        'quit' { Quit-Game }
        default { throw "unknown action $($a.op)" }
    }
}

$errorText = $null
$exitCode = $null
try {
    Log "scenario $($cfg.scenario): $($actions.Count) actions"
    $gpu = (Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Select-Object -First 1).Name
    Log "video adapter: $gpu"
    if ($DryRun) {
        Log "would run $($cfg.exe) with IWD2_DATA=$($cfg.data) IWD2R_INI_OVERRIDE=$($cfg.ini_override)"
    } else {
        New-Item -ItemType Directory -Force -Path $cfg.control | Out-Null
        $env:IWD2_DATA = $cfg.data
        $env:IWD2R_INI_OVERRIDE = $cfg.ini_override
        $env:IWD2R_SHOT_DIR = $cfg.results
        $env:IWD2R_CONTROL_DIR = $cfg.control
        Remove-Item Env:IWD2R_SCRIPT -ErrorAction SilentlyContinue
        $script:game = Start-Process -FilePath $cfg.exe -WorkingDirectory $cfg.data -PassThru -NoNewWindow `
            -RedirectStandardOutput (Join-Path $cfg.results 'log.txt') `
            -RedirectStandardError (Join-Path $cfg.results 'stderr.txt')
        $null = $script:game.Handle # keeps ExitCode readable after exit
        Log "game pid $($script:game.Id)"
        Find-Window
    }
    try {
        foreach ($a in $actions) {
            Log "line $($a.line): $(Describe $a)"
            Do-Action $a
        }
        Log 'scenario finished'
    } catch [System.OperationCanceledException] {
        Log 'game exited'
    }
    if (-not $DryRun) {
        $left = [Math]::Max(1, ($deadline - (Get-Date)).TotalMilliseconds)
        if (-not $script:game.WaitForExit([int]$left)) { throw "scenario timeout ($($cfg.timeout) s)" }
        $exitCode = $script:game.ExitCode
    }
} catch {
    $errorText = "$_"
    if ($script:game -ne $null -and $script:game.HasExited) { $exitCode = $script:game.ExitCode }
    Log "driver error: $errorText"
} finally {
    if ($script:game -ne $null -and -not $script:game.HasExited) {
        Stop-Process -Id $script:game.Id -Force -ErrorAction SilentlyContinue
    }
    Log "exit code $exitCode"
    if (-not $DryRun) {
        Set-Content -LiteralPath (Join-Path $cfg.results 'exitcode.txt') -Value "$exitCode" -Encoding ASCII
        if ($errorText) {
            Set-Content -LiteralPath (Join-Path $cfg.results 'error.txt') -Value $errorText -Encoding UTF8
        }
        Set-Content -LiteralPath (Join-Path $cfg.results 'done.txt') -Value 'done' -Encoding ASCII
        if ($cfg.shutdown) { shutdown.exe /s /t 0 }
    }
}
if ($errorText) { exit 1 }
