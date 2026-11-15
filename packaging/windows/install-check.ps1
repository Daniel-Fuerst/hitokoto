# SPDX-License-Identifier: GPL-3.0-or-later
# S037 (decision 8): installs, upgrades, starts and uninstalls the MSI of one version on a fresh
# Windows runner without Qt, in one scope, without dialogs:
#
#   pwsh packaging/windows/install-check.ps1 -Msi <hitokoto-X.Y.Z-x64.msi>
#        -TestMsi <upgrade-test-0.0.0.msi> -Version X.Y.Z -Scope user|machine
#
# 1. Without ACCEPT_LICENSE=yes the MSI installs nothing.
# 2. The 0.0.0 MSI (same files plus upgrade-marker.txt) installs; then the MSI of X.Y.Z over it.
# 3. Exactly one hitokoto is installed, in version X.Y.Z, with exactly the files of files.txt (so
#    the marker is gone), the Start menu entry and the "Open with" entry for .md;
#    hitokoto.exe --version prints "hitokoto X.Y.Z" with Windows' own platform.
# 4. The 0.0.0 MSI over X.Y.Z is refused, X.Y.Z stays.
# 5. After the uninstall no file, Start menu entry, "Open with" entry or "Installed apps" entry of
#    hitokoto is left.
# Every msiexec call writes a verbose log next to the MSI (<name>-<scope>-<step>.log).
param(
    [Parameter(Mandatory)] [string] $Msi,
    [Parameter(Mandatory)] [string] $TestMsi,
    [Parameter(Mandatory)] [string] $Version,
    [Parameter(Mandatory)] [ValidateSet('user', 'machine')] [string] $Scope
)
$ErrorActionPreference = 'Stop'

$recipe = $PSScriptRoot
$root = (Resolve-Path (Join-Path $recipe '..\..')).Path
$script = Join-Path $root 'scripts\packaging\WindowsPackage.cmake'
$Msi = (Resolve-Path $Msi).Path
$TestMsi = (Resolve-Path $TestMsi).Path
$logs = Split-Path -Parent $Msi

if ($Scope -eq 'user') {
    $scopeArgs = @('MSIINSTALLPERUSER=1')
    $folder = Join-Path $env:LOCALAPPDATA 'Programs\hitokoto'
    $startMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\hitokoto.lnk'
    $classes = 'HKCU:\Software\Classes'
} else {
    $scopeArgs = @('ALLUSERS=1')
    $folder = Join-Path $env:ProgramFiles 'hitokoto'
    $startMenu = Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\hitokoto.lnk'
    $classes = 'HKLM:\Software\Classes'
}

function Invoke-Msiexec([string] $step, [string[]] $arguments) {
    $log = Join-Path $logs "msiexec-$Scope-$step.log"
    $all = $arguments + @('/qn', '/norestart', '/l*v', "`"$log`"")
    $process = Start-Process -FilePath msiexec.exe -ArgumentList $all -Wait -PassThru
    Write-Host "msiexec $($all -join ' ') -> $($process.ExitCode)"
    return $process.ExitCode
}

# The "Installed apps" entries of hitokoto, per user and per machine.
function Get-Installed {
    $keys = @('HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
              'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
              'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*')
    @(Get-ItemProperty -Path $keys -ErrorAction SilentlyContinue |
        Where-Object { $_.DisplayName -eq 'hitokoto' })
}

function Assert([bool] $condition, [string] $what) {
    if (-not $condition) { throw "S037 install check ($Scope): $what" }
    Write-Host "ok: $what"
}

function Get-OpenWith {
    $key = Join-Path $classes '.md\OpenWithProgids'
    if (-not (Test-Path $key)) { return $null }
    (Get-Item $key).GetValue('hitokoto.md', $null)
}

Assert ((Get-Installed).Count -eq 0) 'no hitokoto is installed before the check'

# 1. The licence.
$code = Invoke-Msiexec 'no-licence' (@('/i', "`"$Msi`"") + $scopeArgs)
Assert ($code -ne 0) "without ACCEPT_LICENSE=yes msiexec fails (exit code $code)"
Assert ((Get-Installed).Count -eq 0) 'without ACCEPT_LICENSE=yes nothing is installed'

# 2. 0.0.0, then X.Y.Z over it.
$code = Invoke-Msiexec 'install-0.0.0' (@('/i', "`"$TestMsi`"", 'ACCEPT_LICENSE=yes') + $scopeArgs)
Assert ($code -eq 0) "the 0.0.0 MSI installs (exit code $code)"
Assert (Test-Path (Join-Path $folder 'upgrade-marker.txt')) "0.0.0 is installed in $folder"
$code = Invoke-Msiexec 'upgrade' (@('/i', "`"$Msi`"", 'ACCEPT_LICENSE=yes') + $scopeArgs)
Assert ($code -eq 0) "the MSI of $Version installs over 0.0.0 (exit code $code)"

# 3. What is installed.
$entries = Get-Installed
Assert ($entries.Count -eq 1) "exactly one hitokoto in Installed apps (found $($entries.Count))"
Assert ($entries[0].DisplayVersion -eq $Version) "Installed apps shows $Version ($($entries[0].DisplayVersion))"
$list = Join-Path $logs "installed-$Scope.txt"
Get-ChildItem -Recurse -File $folder | ForEach-Object { $_.FullName.Substring($folder.Length + 1) } |
    Set-Content -Encoding utf8 $list
cmake -DMODE=files "-DEXPECTED_FILE=$recipe\files.txt" "-DACTUAL_FILE=$list" -P $script
Assert ($LASTEXITCODE -eq 0) "the installed files are those of files.txt (the 0.0.0 marker is gone)"
Assert (Test-Path $startMenu) "the Start menu entry $startMenu"
Assert ($null -ne (Get-OpenWith)) "hitokoto is offered under Open with for .md ($classes)"

$exe = Join-Path $folder 'hitokoto.exe'
$stdout = Join-Path $logs "version-$Scope.txt"
$env:QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES = '1'
$process = Start-Process -FilePath $exe -ArgumentList '--version' -Wait -PassThru -NoNewWindow `
    -RedirectStandardOutput $stdout -RedirectStandardError "$stdout.err"
$printed = ((Get-Content -Raw $stdout) ?? '').Trim()
Write-Host "hitokoto --version -> $($process.ExitCode): '$printed'"
Get-Content -Raw "$stdout.err" -ErrorAction SilentlyContinue | Write-Host
Assert ($process.ExitCode -eq 0 -and $printed -eq "hitokoto $Version") "hitokoto --version prints 'hitokoto $Version'"

# 4. An older version over a newer one.
$code = Invoke-Msiexec 'downgrade' (@('/i', "`"$TestMsi`"", 'ACCEPT_LICENSE=yes') + $scopeArgs)
Assert ($code -ne 0) "the 0.0.0 MSI over $Version is refused (exit code $code)"
$entries = Get-Installed
Assert ($entries.Count -eq 1 -and $entries[0].DisplayVersion -eq $Version) "$Version stays installed"

# 5. Uninstall.
$code = Invoke-Msiexec 'uninstall' (@('/x', "`"$Msi`"") + $scopeArgs)
Assert ($code -eq 0) "the uninstall succeeds (exit code $code)"
Assert ((Get-Installed).Count -eq 0) 'no hitokoto is left in Installed apps'
Assert (-not (Test-Path $folder)) "no folder $folder is left"
Assert (-not (Test-Path $startMenu)) 'no Start menu entry is left'
Assert ($null -eq (Get-OpenWith)) 'no Open with entry of the installer is left'
Assert (-not (Test-Path (Join-Path $classes 'hitokoto.md'))) 'no ProgID hitokoto.md is left'
