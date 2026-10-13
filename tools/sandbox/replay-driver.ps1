<#
.SYNOPSIS
Records one scenario in the original iwd2.exe inside Windows Sandbox.

.DESCRIPTION
tools/oracle.py replay record and replay compare (tools/winsandbox.py's
run_replay) write config.json into a read-only tools folder and start the
sandbox with this script as its logon command. The script mutes the
sandbox and, with config "stop_audio", stops its audio services (as
driver.ps1 does), replaces the sandbox's DirectPlay stubs with the host's
DirectPlay files (Install-DirectPlay), sets the
recorder's environment from the config (IWD2R_SCRIPT, IWD2R_REPLAY_DIR,
IWD2R_DUMP_EVERY, IWD2R_SEED and the test switches), and runs

  injector.exe <exe> <workdir> <recorder.dll> --timeout <timeout>

as the host mode does. The injector kills the game at the timeout itself;
the script kills the injector (which closes its job and so the game) at
the backstop, as oracle.py's run_injector does.

When the injector or the recorder DLL is missing, cannot be started, or
the DLL never loaded into the game (exit code 12 without a recorder.log),
an antivirus inside the sandbox may have blocked it: the script adds a
Microsoft Defender exclusion for the mapped folders (the sandbox user is an
administrator) and tries once more. The first attempt's files are kept as
attempt1-*.

A "Windows Features" prompt during the run (Windows asking for another
optional feature) is logged with a screen shot (stuck-screen.png) and
fails the run; so is a recorder that has not loaded after 20 s (logged
only).

The results folder receives driver.log, injector-stdout.txt,
injector-stderr.txt, exitcode.txt (the injector's exit code, empty when the
backstop killed it), error.txt (only when the driver failed) and finally
done.txt; the recorder writes into results\replay. Then the sandbox shuts
down.

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
$logPath = Join-Path $cfg.results 'driver.log'
$stdoutPath = Join-Path $cfg.results 'injector-stdout.txt'
$stderrPath = Join-Path $cfg.results 'injector-stderr.txt'

function Log([string]$message) {
    $elapsed = ((Get-Date) - $started).TotalSeconds
    $line = '{0} +{1,7:0.000} {2}' -f (Get-Date -Format 'HH:mm:ss.fff'), $elapsed, $message
    Write-Host $line
    if (-not $DryRun) {
        Add-Content -LiteralPath $logPath -Value $line -Encoding UTF8
    }
}

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

function Missing-Files {
    $missing = @()
    foreach ($path in $cfg.injector, $cfg.recorder, $cfg.exe) {
        if (-not [System.IO.File]::Exists($path)) { $missing += $path }
    }
    return $missing
}

# Logs what the game does when the recorder has not loaded 20 s after the
# start: its threads, its 32-bit modules, every visible window, and a screen
# shot (stuck-screen.png).
function Diagnose {
    Log 'diagnostics'
    foreach ($g in @(Get-Process -Name iwd2 -ErrorAction SilentlyContinue)) {
        Log "  iwd2 pid $($g.Id), $($g.Threads.Count) threads"
        foreach ($t in $g.Threads) {
            $why = if ($t.ThreadState -eq 'Wait') { " ($($t.WaitReason))" } else { '' }
            Log "    thread $($t.Id): $($t.ThreadState)$why"
        }
        $modules = & "$env:windir\SysWOW64\tasklist.exe" /m /fi "PID eq $($g.Id)" /fo list 2>&1 | Out-String
        Log "  modules: $(($modules -split '\s+' | Where-Object { $_ -match '\.(dll|exe)$' }) -join ' ')"
    }
    foreach ($w in @(Get-Process | Where-Object { $_.MainWindowTitle })) {
        Log "  window of $($w.ProcessName) ($($w.Id)): $($w.MainWindowTitle)"
    }
    try {
        Add-Type -AssemblyName System.Drawing
        Add-Type -AssemblyName System.Windows.Forms
        $bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
        $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bitmap.Size)
        $bitmap.Save((Join-Path $cfg.results 'stuck-screen.png'), [System.Drawing.Imaging.ImageFormat]::Png)
        $graphics.Dispose()
        $bitmap.Dispose()
        Log '  wrote stuck-screen.png'
    } catch {
        Log "  no screen shot: $_"
    }
}

# Installs DirectPlay, which iwd2.exe imports (DPLAYX.dll). The sandbox has
# it disabled: its SysWOW64 DirectPlay files are 10 KB Features on Demand
# stubs, and loading them makes Windows suspend the game behind a "Windows
# Features: An app on your PC needs DirectPlay" prompt; skipping or closing
# the prompt kills the game with 0xC0000022. Enabling the optional feature
# fails there ("The component store has been corrupted": the sandbox has no
# payload). So the host copies its own 32-bit DirectPlay files into the
# tools folder (config "directplay") and this replaces the stubs with them
# (the sandbox user is an administrator, and the sandbox is thrown away).
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
# and any answer kills it. Logs the prompt, takes a screen shot, closes it
# and returns its description (the injector then reports the dead game).
function Feature-Prompt {
    $prompts = @(Get-Process -Name Fondue -ErrorAction SilentlyContinue)
    if ($prompts.Count -eq 0) { return $null }
    $line = (Get-CimInstance Win32_Process -Filter "ProcessId = $($prompts[0].Id)" -ErrorAction SilentlyContinue).CommandLine
    Log "Windows asks to install a feature: $line"
    Diagnose
    $prompts | ForEach-Object { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue }
    return "Windows Features prompt ($line)"
}

# Runs the injector once. Returns @{ code; blocked = why the run looks
# blocked by an antivirus, or $null }.
function Run-Injector {
    $missing = @(Missing-Files)
    if ($missing.Count -gt 0) {
        return @{ code = $null; blocked = "missing: $($missing -join ', ')" }
    }
    $arguments = @("`"$($cfg.exe)`"", "`"$($cfg.workdir)`"", "`"$($cfg.recorder)`"",
        '--timeout', "$($cfg.timeout)")
    Log "running $($cfg.injector) $($arguments -join ' ')"
    $runStarted = Get-Date
    try {
        $p = Start-Process -FilePath $cfg.injector -ArgumentList $arguments -WorkingDirectory $cfg.workdir `
            -PassThru -NoNewWindow -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    } catch {
        return @{ code = $null; blocked = "the injector did not start: $_" }
    }
    $null = $p.Handle # keeps ExitCode readable after exit
    Log "injector pid $($p.Id)"
    $end = (Get-Date).AddSeconds([double]$cfg.backstop)
    $diagnosed = $false
    $prompt = $null
    while (-not $p.WaitForExit(1000)) {
        if ((Get-Date) -gt $end) { break }
        if (-not $prompt) { $prompt = Feature-Prompt }
        if (-not $diagnosed -and ((Get-Date) - $runStarted).TotalSeconds -gt 20) {
            $diagnosed = $true
            $out = if (Test-Path -LiteralPath $stdoutPath) { Get-Content -Raw -LiteralPath $stdoutPath } else { '' }
            if ($out -notmatch 'loaded into') {
                Log 'the recorder has not loaded after 20 s'
                Diagnose
            }
        }
    }
    if (-not $p.HasExited) {
        Log "backstop: the injector still runs after $($cfg.backstop) s; killing it (and so its job and the game)"
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        $p.WaitForExit(10000) | Out-Null
        return @{ code = $null; blocked = $null }
    }
    $code = $p.ExitCode
    Log "injector exit code $code"
    $blocked = $null
    $recorderLog = Join-Path $cfg.replay_dir 'recorder.log'
    $stderrText = if (Test-Path -LiteralPath $stderrPath) { Get-Content -Raw -LiteralPath $stderrPath } else { '' }
    # "cannot run LoadLibraryA" is the remote thread failing or hanging, not
    # a refused file: only a load that returned NULL (or "no DLL") counts.
    if ($code -eq 12 -and -not [System.IO.File]::Exists($recorderLog) -and $stderrText -notmatch 'cannot run LoadLibraryA') {
        $blocked = 'exit code 12 and no recorder.log: the DLL never loaded'
    }
    $missing = @(Missing-Files)
    if ($missing.Count -gt 0) {
        $blocked = "vanished during the run: $($missing -join ', ')"
    }
    return @{ code = $code; blocked = $blocked; prompt = $prompt }
}

# Keeps the first attempt's files as attempt1-* and empties the replay
# folder (all but scenario.txt) for the second.
function Set-Aside {
    foreach ($path in $stdoutPath, $stderrPath) {
        if (Test-Path -LiteralPath $path) {
            Move-Item -LiteralPath $path -Destination (Join-Path $cfg.results ('attempt1-' + (Split-Path -Leaf $path))) -Force
        }
    }
    Get-ChildItem -LiteralPath $cfg.replay_dir -File | Where-Object { $_.Name -ne 'scenario.txt' } | ForEach-Object {
        Move-Item -LiteralPath $_.FullName -Destination (Join-Path $cfg.results ('attempt1-replay-' + $_.Name)) -Force
    }
}

$errorText = $null
$exitCode = $null
try {
    Log "replay: timeout $($cfg.timeout) s, backstop $($cfg.backstop) s"
    $gpu = (Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Select-Object -First 1).Name
    Log "video adapter: $gpu"
    Log "Defender: $(Defender-State)"
    foreach ($prop in $cfg.env.PSObject.Properties) {
        Log "  $($prop.Name)=$($prop.Value)"
        if (-not $DryRun) { Set-Item -Path "Env:$($prop.Name)" -Value "$($prop.Value)" }
    }
    if ($DryRun) {
        Log "would run $($cfg.injector) `"$($cfg.exe)`" `"$($cfg.workdir)`" `"$($cfg.recorder)`" --timeout $($cfg.timeout) in $($cfg.workdir)"
        Log "would replace SysWOW64's DirectPlay files with those in $($cfg.directplay)"
        Log "would add Defender exclusions for $($cfg.exclusions -join ', ') if it is blocked, and retry once"
    } else {
        $muteState = 'failed'
        try { $muteState = [IwdAudio]::MuteDefaultOutput() } catch { $muteState = "failed: $_" }
        Log "sandbox audio: $muteState"
        if ($cfg.stop_audio) {
            foreach ($service in 'Audiosrv', 'AudioEndpointBuilder') {
                try {
                    Stop-Service -Name $service -Force -ErrorAction Stop
                    Log "stopped $service"
                } catch {
                    Log "could not stop ${service}: $_"
                }
            }
        } else {
            Log 'audio services left running'
        }
        Install-DirectPlay
        $run = Run-Injector
        if ($run.blocked) {
            Log "attempt 1 looks blocked: $($run.blocked)"
            Log "Defender: $(Defender-State)"
            Add-Exclusions
            Set-Aside
            Start-Sleep -Seconds 2
            $run = Run-Injector
            if ($run.blocked) {
                $exitCode = $run.code
                Log "attempt 2 looks blocked too: $($run.blocked)"
                throw "blocked twice: $($run.blocked)"
            }
            Log 'attempt 2 ran'
        }
        $exitCode = $run.code
        if ($run.prompt) { throw $run.prompt }
    }
} catch {
    $errorText = "$_"
    Log "driver error: $errorText"
    Log "  at $($_.InvocationInfo.ScriptLineNumber): $($_.InvocationInfo.Line.Trim())"
} finally {
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
