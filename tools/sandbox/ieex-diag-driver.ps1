# IWD2EE loader bisect driver: runs IWD2EE.exe (the IEex loader) UNTRACED
# with its stdout+stderr redirected to results\loader-console.log, waits,
# then captures the screen, the window/process state and a game-directory
# diff. Purpose: decide whether the injection failure seen under apitrace
# (runs/20261018-144712) is apitrace's child injection or the sandbox.
#
# Console mode: the mrfearless loader prints its per-step diagnostics to
# stdout only when GetConsoleProcessList returns exactly 2 for it, which
# needs its own console holding just cmd.exe and the loader; the hidden
# cmd /c wrapper below provides exactly that. In GUI mode the failing
# branch (remote LoadLibrary returning FALSE) is silent, which is what
# run 1 showed: one generic "An error occurred whilst attempting to
# inject IEex.dll." dialog.
#
# Output files in config "results" (C:\iwd2r\results): diag.log,
# loader-console.log, loader-console.err, processes.txt, windows.txt,
# game-before.txt, game-after.txt, game-diff.txt, final-screen.png,
# exitcode.txt (always 0; the diagnosis is in the other files) and
# done.txt.

param([string]$Config, [switch]$DryRun)
$ErrorActionPreference = 'Stop'

$cfg = Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json

function Log([string]$message) {
    $line = '{0:yyyy-MM-dd HH:mm:ss} {1}' -f (Get-Date), $message
    Write-Output $line
    Add-Content -LiteralPath (Join-Path $cfg.results 'diag.log') -Value $line
}

$cs = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
namespace IwdDiag {
    public struct Win { public IntPtr Hwnd; public uint Pid; public string Class; public string Title; }
    public static class Wins {
        public delegate bool EWP(IntPtr hWnd, IntPtr lParam);
        [DllImport("user32.dll")] static extern bool EnumWindows(EWP proc, IntPtr lParam);
        [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr hWnd, StringBuilder name, int size);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr hWnd, StringBuilder name, int size);
        static List<Win> found;
        static bool Callback(IntPtr hWnd, IntPtr lParam) {
            uint pid;
            GetWindowThreadProcessId(hWnd, out pid);
            if (!IsWindowVisible(hWnd)) return true;
            StringBuilder cls = new StringBuilder(256);
            GetClassName(hWnd, cls, cls.Capacity);
            StringBuilder txt = new StringBuilder(256);
            GetWindowText(hWnd, txt, txt.Capacity);
            found.Add(new Win { Hwnd = hWnd, Pid = pid, Class = cls.ToString(), Title = txt.ToString() });
            return true;
        }
        static EWP proc = new EWP(Callback);
        public static List<Win> Visible() {
            found = new List<Win>();
            EnumWindows(proc, IntPtr.Zero);
            return found;
        }
    }
}
'@
Add-Type -TypeDefinition $cs -Language CSharp

function Add-Exclusions {
    foreach ($path in $cfg.exclusions) {
        try {
            Add-MpPreference -ExclusionPath $path -ErrorAction Stop
            Log "added a Defender exclusion for $path"
        } catch {
            Log "could not add a Defender exclusion for ${path}: $_"
        }
    }
}

# Same rationale as gltrace-driver.ps1: iwd2.exe imports DPLAYX.dll and the
# sandbox's SysWOW64 copies are 10 KB stubs that raise a Windows Features
# prompt which suspends the game.
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

function Snapshot([string]$tag) {
    $rows = Get-ChildItem -LiteralPath $cfg.data -File |
        Sort-Object Name |
        ForEach-Object { '{0} | {1} | {2}' -f $_.Name, $_.Length, $_.LastWriteTimeUtc.ToString('yyyy-MM-dd HH:mm:ss') }
    Set-Content -LiteralPath (Join-Path $cfg.results "game-$tag.txt") -Value $rows -Encoding UTF8
}

function Dump-Windows([string]$tag) {
    $lines = foreach ($w in [IwdDiag.Wins]::Visible()) {
        'hwnd={0} pid={1} class={2} title={3}' -f $w.Hwnd, $w.Pid, $w.Class, $w.Title
    }
    Set-Content -LiteralPath (Join-Path $cfg.results $tag) -Value $lines -Encoding UTF8
    $lines | ForEach-Object { Log "  window $_" }
}

function Screen-Shot([string]$name) {
    Add-Type -AssemblyName System.Drawing
    Add-Type -AssemblyName System.Windows.Forms
    $bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
    $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bitmap.Size)
        $bitmap.Save((Join-Path $cfg.results "$name-screen.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
    Log "  wrote $name-screen.png"
}

function Dump-Processes([string]$tag) {
    $names = 'IWD2EE', 'iwd2', 'cmd', 'apitrace', 'Fondue'
    $lines = foreach ($p in (Get-Process -Name $names -ErrorAction SilentlyContinue)) {
        '{0} | pid={1} | title=[{2}] | responding={3} | exited={4} | start={5}' -f $p.Name, $p.Id, $p.MainWindowTitle, $p.Responding, $p.HasExited, $p.StartTime.ToString('HH:mm:ss')
    }
    Set-Content -LiteralPath (Join-Path $cfg.results $tag) -Value $lines -Encoding UTF8
    $lines | ForEach-Object { Log "  process $_" }
}

Log "ieex-diag driver on $env:COMPUTERNAME, config $Config"
if (-not $DryRun) { Add-Exclusions }
if (-not $DryRun) { Install-DirectPlay }
Snapshot 'before'

$loader = $null
$deadline = (Get-Date).AddSeconds($cfg.timeout)
if (-not $DryRun) {
    # Hidden own console: the loader then sees GetConsoleProcessList == 2
    # (cmd + itself) and runs in console mode with full diagnostics.
    $inner = '""{0}" > "{1}\loader-console.log" 2>&1"' -f $cfg.exe, $cfg.results
    Log "launching: $env:ComSpec /c $inner (working dir $($cfg.data))"
    $loader = Start-Process -FilePath $env:ComSpec -ArgumentList "/c $inner" `
        -WorkingDirectory $cfg.data -WindowStyle Hidden -PassThru
    $null = $loader.Handle
    Log "loader wrapper pid $($loader.Id)"
} else {
    Log "dry-run: would launch $($cfg.exe) via a hidden cmd /c wrapper"
}

# Poll until the game window shows (success), the loader wrapper exits
# (console mode ends the loader either way; grace 20 s for iwd2.exe), or
# the deadline. Every Fondue.exe sighting is killed and logged: the
# Windows Features prompt suspends the game silently.
while ($true) {
    Start-Sleep -Seconds 5
    if ($DryRun) { Log 'dry-run: poll tick'; break }
    $iwd2 = @(Get-Process -Name iwd2 -ErrorAction SilentlyContinue)
    $ldr = if ($loader -ne $null) { Get-Process -Id $loader.Id -ErrorAction SilentlyContinue } else { $null }
    $visible = @([IwdDiag.Wins]::Visible())
    $gamePid = if ($iwd2.Count -gt 0) { [int]$iwd2[0].Id } else { -1 }
    $gameWin = @($visible | Where-Object { [int]$_.Pid -eq $gamePid })
    Log ("poll: iwd2={0} loader-cmd={1} visible-windows={2}" -f `
        (($iwd2 | ForEach-Object Id) -join ','), (($ldr | ForEach-Object Id) -join ','), $visible.Count)
    foreach ($fondue in @(Get-Process -Name Fondue -ErrorAction SilentlyContinue)) {
        Log 'Windows Features prompt appeared; killing it'
        Screen-Shot 'fondue'
        Stop-Process -Id $fondue.Id -Force -ErrorAction SilentlyContinue
    }
    if ($iwd2.Count -gt 0 -and $gameWin.Count -gt 0) {
        Log 'iwd2.exe has a visible window: the game booted untraced'
        break
    }
    if ($ldr -eq $null) {
        Log 'loader wrapper exited'
        $grace = (Get-Date).AddSeconds(20)
        while ((Get-Date) -lt $grace) {
            Start-Sleep -Seconds 5
            $iwd2 = @(Get-Process -Name iwd2 -ErrorAction SilentlyContinue)
            if ($iwd2.Count -gt 0) { break }
        }
        if ($iwd2.Count -gt 0) { Log "iwd2.exe still alive ($($iwd2[0].Id)) without a window" }
        else { Log 'iwd2.exe gone after loader exit' }
        break
    }
    if ((Get-Date) -gt $deadline) { Log "deadline ($($cfg.timeout) s) with the loader wrapper alive"; break }
}

Dump-Windows 'windows.txt'
Dump-Processes 'processes.txt'
if (-not $DryRun) { Screen-Shot 'final' }
Snapshot 'after'

# Game-directory diff (names, sizes, mtimes).
$before = @(Get-Content -LiteralPath (Join-Path $cfg.results 'game-before.txt') -ErrorAction SilentlyContinue)
$after = @(Get-Content -LiteralPath (Join-Path $cfg.results 'game-after.txt') -ErrorAction SilentlyContinue)
$diff = Compare-Object -ReferenceObject $before -DifferenceObject $after -IncludeEqual:$false |
    ForEach-Object { '{0} {1}' -f $_.SideIndicator, $_.InputObject }
Set-Content -LiteralPath (Join-Path $cfg.results 'game-diff.txt') -Value $diff -Encoding UTF8
if (@($diff).Count -eq 0) { Log 'game directory: no file changes' }
else { $diff | ForEach-Object { Log "  game-dir $_" } }

# The loader's own stdout/stderr copy (console diagnostics; the loader also
# redirects iwd2.exe's stdout into the same handle when in console mode).
foreach ($name in 'loader-console.log', 'loader-console.err') {
    $path = Join-Path $cfg.results $name
    if ([System.IO.File]::Exists($path)) {
        Log "--- $name ---"
        Get-Content -LiteralPath $path | ForEach-Object { Log "  | $_" }
    } else {
        Log "$name was not created"
    }
}

Set-Content -LiteralPath (Join-Path $cfg.results 'exitcode.txt') -Value '0' -Encoding ASCII
Set-Content -LiteralPath (Join-Path $cfg.results 'done.txt') -Value 'done' -Encoding ASCII
Log 'done'
