<#
  Tests for Update-DiceVox.ps1. Runs on Windows or Linux PowerShell 7 (pwsh).
  Installs into a temporary folder, never into the real VST3 folder.
  Needs the GitHub CLI signed in with access to the repo.

      pwsh -File tools/updater/Test-Updater.ps1
#>
$ErrorActionPreference = "Stop"
$script  = Join-Path $PSScriptRoot "Update-DiceVox.ps1"
$root    = Join-Path ([IO.Path]::GetTempPath()) ("ahe-updater-test-" + [Guid]::NewGuid().ToString("N"))
$vst3    = Join-Path $root "VST3"
$state   = Join-Path $root "state"
$plugin  = Join-Path $vst3 "DiceVox.vst3"
$common  = @("-InstallDir", $vst3, "-StateDir", $state, "-NoElevate", "-NoShortcut", "-NoPause")
$failures = 0

function Check ([bool] $ok, [string] $name) {
    Write-Host ($(if ($ok) { "  PASS  " } else { "  FAIL  " }) + $name)
    if (-not $ok) { $script:failures++ }
}

function Run ([string[]] $extra = @()) {
    $ErrorActionPreference = "Continue"   # PS 5.1 would otherwise throw on any child stderr
    $exe = (Get-Process -Id $PID).Path
    $out = & $exe -NoProfile -File $script @common @extra 2>&1 | Out-String
    return @{ Output = $out; Code = $LASTEXITCODE }
}

Write-Host "Updater tests"
try {
    # 0. Script parses cleanly
    $errs = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile($script, [ref]$null, [ref]$errs)
    Check ($errs.Count -eq 0) "Script has no syntax errors"

    # 1. Fresh install
    $r = Run
    $binary = Get-ChildItem -Recurse -File -Path $plugin -ErrorAction SilentlyContinue |
              Where-Object { $_.FullName -match "x86_64-win" } | Select-Object -First 1
    Check ($r.Code -eq 0 -and $r.Output -match "updated" -and $binary) "Fresh install puts DiceVox.vst3 in the VST3 folder"
    Check (Test-Path (Join-Path $state "DiceVox.installed")) "Remembers which build is installed"

    # 2. Already current -> no download
    $r = Run
    Check ($r.Code -eq 0 -and $r.Output -match "already have the newest") "Second run says it's already up to date"

    # 3. Forced reinstall replaces the folder (no leftover files from old builds)
    $stale = Join-Path $plugin "STALE_FILE"
    Set-Content -Path $stale -Value "old"
    $r = Run @("-Force")
    Check ($r.Code -eq 0 -and -not (Test-Path $stale)) "Reinstall removes files left over from an old build"

    # 4. Plugin deleted by hand -> reinstalls even though the state file says current
    Remove-Item -Recurse -Force $plugin
    $r = Run
    Check ($r.Code -eq 0 -and (Test-Path $plugin)) "Reinstalls if the plugin folder went missing"

    # 5. Bad repo -> clear error, non-zero exit
    $r = Run @("-Repo", "alienheadbill/this-repo-does-not-exist")
    Check ($r.Code -ne 0 -and $r.Output -match "Couldn't") "Bad repo gives a clear error"

    # 6. Admin install step survives a user name with a space and an apostrophe
    $func = $ast.FindAll({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
                                     $n.Name -eq "Get-EncodedInstallCommand" }, $true) | Select-Object -First 1
    Invoke-Expression $func.Extent.Text
    $tricky    = Join-Path $root "Bill O'Neil Temp"
    $src       = Join-Path $tricky "DiceVox.vst3"
    $dstDir    = Join-Path $root "Program Files (x86) 'Test'\VST3"
    New-Item -ItemType Directory -Force -Path (Join-Path $src "Contents") | Out-Null
    Set-Content -Path (Join-Path $src "Contents/marker") -Value "new"
    $enc = Get-EncodedInstallCommand (Join-Path $dstDir "DiceVox.vst3") $dstDir $src
    & (Get-Process -Id $PID).Path -NoProfile -EncodedCommand $enc
    Check ((Get-Content -LiteralPath (Join-Path $dstDir "DiceVox.vst3/Contents/marker")) -eq "new") `
          "Admin install step handles paths with spaces and apostrophes"
}
finally {
    Remove-Item -Recurse -Force $root -ErrorAction SilentlyContinue
}

if ($failures -eq 0) { Write-Host "All updater tests passed." } else { Write-Host "$failures updater test(s) failed." }
exit $failures
