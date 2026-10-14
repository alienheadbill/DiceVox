<#
  AHE plugin updater (Windows).

  Downloads the newest build of an AHE plugin from its GitHub "Latest build"
  release and installs it into the VST3 folder.

  First run: copies itself to %LOCALAPPDATA%\AHE\Updater and puts an
  "Update DiceVox" shortcut on the desktop. After that, just double-click the shortcut.

  Requires the GitHub CLI, signed in once (the repo is private).
  If either step is missing, the script prints exactly what to run:
      winget install --id GitHub.cli
      gh auth login

  Compatible with Windows PowerShell 5.1 (built into Windows).
#>
[CmdletBinding()]
param(
    [string] $Plugin     = "DiceVox",
    [string] $Repo       = "alienheadbill/DiceVox",
    [string] $Release    = "latest-build",
    [string] $InstallDir = "$env:CommonProgramFiles\VST3",
    [string] $StateDir   = "$env:LOCALAPPDATA\AHE\Updater",
    [switch] $Force,        # reinstall even if already up to date
    [switch] $NoShortcut,   # don't self-install / create the desktop shortcut
    [switch] $NoElevate,    # copy without an admin prompt (testing / custom InstallDir)
    [switch] $NoPause       # don't wait for a key at the end
)

$ErrorActionPreference = "Stop"
$isWin = ($env:OS -eq "Windows_NT")

function Say  ($msg) { Write-Host $msg }
function Good ($msg) { Write-Host $msg -ForegroundColor Green }
function Warn ($msg) { Write-Host $msg -ForegroundColor Yellow }

function Finish ([int] $code) {
    if (-not $NoPause) {
        Write-Host ""
        Read-Host "Press Enter to close" | Out-Null
    }
    exit $code
}

# Builds the admin-side install step as an encoded command. Paths go in as
# literal strings, so spaces or apostrophes in a Windows user name can't break it.
function Get-EncodedInstallCommand ([string] $pluginPath, [string] $installDir, [string] $newPlugin) {
    function Lit ([string] $s) { "'" + ($s -replace "'", "''") + "'" }
    $cmd = "`$ErrorActionPreference='Stop'; " +
           "if (Test-Path -LiteralPath $(Lit $pluginPath)) { Remove-Item -LiteralPath $(Lit $pluginPath) -Recurse -Force }; " +
           "New-Item -ItemType Directory -Force -Path $(Lit $installDir) | Out-Null; " +
           "Copy-Item -LiteralPath $(Lit $newPlugin) -Destination $(Lit $installDir) -Recurse -Force"
    [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($cmd))
}

function Fail ($msg) {
    Write-Host ""
    Write-Host $msg -ForegroundColor Red
    Finish 1
}

# Runs gh and returns its text output + exit code. Windows PowerShell 5.1
# turns any stderr from a program into a crash when ErrorActionPreference is
# "Stop", so gh runs with "Continue" here and errors come back as plain text.
function Invoke-Gh ([string[]] $ghArgs) {
    $ErrorActionPreference = "Continue"
    $out = & $script:ghExe @ghArgs 2>&1 | ForEach-Object { "$_" }
    return @{ Out = (($out -join "`n").Trim()); Code = $LASTEXITCODE }
}

# Safety net: any unexpected error is shown and the window stays open.
trap {
    Write-Host ""
    Write-Host "Something went wrong: $_" -ForegroundColor Red
    Write-Host "Take a screenshot of this window and send it over." -ForegroundColor Red
    Finish 1
}

Write-Host ""
Write-Host "  AHE  |  $Plugin updater" -ForegroundColor Magenta
Write-Host "  ----------------------------------------"
Write-Host ""

# --- 1. First run: install the updater itself + desktop shortcut ------------
if ($isWin -and -not $NoShortcut) {
    $here = Split-Path -Parent $MyInvocation.MyCommand.Path
    New-Item -ItemType Directory -Force -Path $StateDir | Out-Null

    if ((Resolve-Path $here).Path.TrimEnd('\') -ne (Resolve-Path $StateDir).Path.TrimEnd('\')) {
        Copy-Item -Force (Join-Path $here "Update-DiceVox.ps1") $StateDir
        Copy-Item -Force (Join-Path $here "Update-DiceVox.cmd") $StateDir
    }

    $desktop  = [Environment]::GetFolderPath("Desktop")
    $lnkPath  = Join-Path $desktop "Update $Plugin.lnk"
    if (-not (Test-Path $lnkPath)) {
        $shell = New-Object -ComObject WScript.Shell
        $lnk = $shell.CreateShortcut($lnkPath)
        $lnk.TargetPath       = Join-Path $StateDir "Update-DiceVox.cmd"
        $lnk.WorkingDirectory = $StateDir
        $lnk.Description      = "Update $Plugin to the newest AHE build"
        $lnk.IconLocation     = "$env:SystemRoot\System32\shell32.dll,238"
        $lnk.Save()
        Good "Added an 'Update $Plugin' shortcut to your desktop."
        Write-Host ""
    }
}

# --- 2. GitHub CLI present and signed in? ------------------------------------
$ghCmd = Get-Command gh -ErrorAction SilentlyContinue
$script:ghExe = if ($ghCmd) { $ghCmd.Source } else { $null }
if (-not $script:ghExe -and $isWin) {
    # Freshly installed via winget but this window started before PATH was updated.
    $fallback = Join-Path $env:ProgramFiles "GitHub CLI\gh.exe"
    if (Test-Path $fallback) { $script:ghExe = $fallback }
}
if (-not $script:ghExe) {
    Fail ("GitHub CLI is not installed. Open PowerShell and run:`n" +
          "    winget install --id GitHub.cli`n" +
          "then close and reopen PowerShell and run:`n" +
          "    gh auth login")
}

# --- 3. What's the newest build? ---------------------------------------------
$assetName = "$Plugin-Windows.zip"
Say "Checking for a new build..."

$res = Invoke-Gh @("api", "repos/$Repo/releases/tags/$Release")
$json = $res.Out
if ($res.Code -ne 0) {
    $err = $json
    if ($err -match "auth login|401|Bad credentials|Not Found|404") {
        # A private repo looks "Not Found" when you're not signed in.
        Fail ("Couldn't get the release. Most likely the GitHub CLI isn't signed in.`n" +
              "Open PowerShell and run:`n" +
              "    gh auth login`n" +
              "(choose GitHub.com, HTTPS, and 'Login with a web browser').`n`n" +
              "GitHub said: $err")
    }
    Fail "Couldn't reach GitHub (check your internet connection):`n$err"
}

$rel   = ($json -join "`n") | ConvertFrom-Json
$asset = $rel.assets | Where-Object { $_.name -eq $assetName } | Select-Object -First 1
if (-not $asset) { Fail "The '$Release' release has no $assetName yet (a build may still be running)." }

$buildLabel = ($rel.body -split "`n")[0].Trim()   # "Built from commit abc1234."
$stateFile  = Join-Path $StateDir "$Plugin.installed"
$installed  = if (Test-Path $stateFile) { (Get-Content $stateFile -Raw).Trim() } else { "" }
$pluginPath = Join-Path $InstallDir "$Plugin.vst3"

if (-not $Force -and $installed -eq "$($asset.id)" -and (Test-Path $pluginPath)) {
    Good "You already have the newest $Plugin. ($buildLabel)"
    Finish 0
}

# --- 4. FL Studio must be closed (Windows locks loaded plugin files) ---------
if ($isWin) {
    $blocking = @("FL64", "FL", $Plugin)
    while (Get-Process -Name $blocking -ErrorAction SilentlyContinue) {
        Warn "Close FL Studio first (it keeps the plugin file locked), then press Enter."
        Read-Host | Out-Null
    }
}

# --- 5. Download + unzip -----------------------------------------------------
$work = Join-Path ([IO.Path]::GetTempPath()) ("ahe-update-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $work | Out-Null

try {
    Say "Downloading $assetName..."
    # Download straight to a file with Invoke-WebRequest, using gh's sign-in token.
    # (Passing a zip through a program's text output would corrupt it.)
    # GitHub redirects to a signed storage URL; the token is dropped on that hop.
    $zipPath = Join-Path $work $assetName
    $tok     = Invoke-Gh @("auth", "token")
    $token   = if ($tok.Code -eq 0) { $tok.Out } else { $env:GH_TOKEN }
    $headers = @{ Accept = "application/octet-stream"; "User-Agent" = "AHE-Updater" }
    if ($token) { $headers.Authorization = "Bearer $token" }

    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $oldProgress = $ProgressPreference; $ProgressPreference = "SilentlyContinue"  # PS 5.1 is very slow with the progress bar

    $ok = $false
    for ($attempt = 1; $attempt -le 3 -and -not $ok; $attempt++) {
        if ($attempt -gt 1) { Warn "Download hiccup, retrying ($attempt/3)..."; Start-Sleep -Seconds 3 }
        try {
            Invoke-WebRequest -UseBasicParsing -Uri "https://api.github.com/repos/$Repo/releases/assets/$($asset.id)" `
                              -Headers $headers -OutFile $zipPath
            $ok = (Test-Path $zipPath) -and ((Get-Item $zipPath).Length -eq $asset.size)
        } catch {
            $ok = $false
        }
    }
    $ProgressPreference = $oldProgress
    if (-not $ok) { Fail "Download failed after 3 tries. Check your internet connection and run it again." }

    Expand-Archive -Path $zipPath -DestinationPath (Join-Path $work "unzipped") -Force
    $newPlugin = Join-Path $work "unzipped\$Plugin.vst3"
    if (-not (Test-Path $newPlugin)) { Fail "The download didn't contain $Plugin.vst3." }

    # --- 6. Install (replace the old folder) ---------------------------------
    Say "Installing to $InstallDir..."

    if ($isWin -and -not $NoElevate) {
        # Only this step needs admin rights (Program Files).
        $encoded = Get-EncodedInstallCommand $pluginPath $InstallDir $newPlugin
        try {
            $p = Start-Process powershell -Verb RunAs -Wait -PassThru -WindowStyle Hidden `
                     -ArgumentList "-NoProfile", "-ExecutionPolicy", "Bypass", "-EncodedCommand", $encoded
        } catch {
            Fail "Install cancelled (the admin prompt was declined)."
        }
        if ($p.ExitCode -ne 0) { Fail "Install failed (exit code $($p.ExitCode))." }
    }
    else {
        if (Test-Path $pluginPath) { Remove-Item -Recurse -Force $pluginPath }
        New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
        Copy-Item -Recurse -Force $newPlugin $InstallDir
    }

    if (-not (Test-Path $pluginPath)) { Fail "Install failed: $pluginPath is missing afterwards." }

    New-Item -ItemType Directory -Force -Path $StateDir | Out-Null
    Set-Content -Path $stateFile -Value "$($asset.id)" -NoNewline

    Write-Host ""
    Good "$Plugin updated. ($buildLabel)"
    Say  "Open FL Studio and your projects will use the new version."
}
finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}

Finish 0
