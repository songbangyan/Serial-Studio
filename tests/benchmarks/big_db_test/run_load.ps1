# Headless load run for the big_db_test project (Windows / PowerShell).
#
# Starts the UDP frame simulator, runs the app headless loading the big project
# against a UDP source, and lets the app quit itself via --exit-after. Used by
# CI for PGO training (heavy transform / dashboard coverage) and as a non-crash
# verification of the optimized binary.
#
# The app must exit through its own event loop: PGO-instrumented builds flush
# profile data (.profraw/.pgc) only on a normal exit, so killing it here used
# to silently discard the whole training run. A kill is now the failure path,
# taken only when the app misses its exit window.
#
# Usage: run_load.ps1 -App <app.exe> [-Seconds 25]
#
# Exits non-zero if the app crashes before the window elapses or has to be
# killed because it failed to quit gracefully within the grace period.
param(
    [Parameter(Mandatory = $true)][string]$App,
    [int]$Seconds = 25
)

$dir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$port = 8080

# One faulted channel per board type keeps the diagnostics decode path under
# load: healthy boards suppress their diagnostics frame entirely.
$sim = Start-Process -FilePath 'python' `
    -ArgumentList @("$dir/big_db_test.py", '--host', '127.0.0.1', '--port', "$port", '--rate', '50', `
        '--faults', 'TA:2:5,TB:1:3,TC:3:7') `
    -PassThru -NoNewWindow
$appProc = Start-Process -FilePath $App `
    -ArgumentList @('--headless', '--project', "$dir/big_db_test.ssproj", '--udp', "$port", `
        '--exit-after', "$Seconds") `
    -PassThru -NoNewWindow

$grace = 30
$rc = 0
if ($appProc.WaitForExit(($Seconds + $grace) * 1000)) {
    $rc = $appProc.ExitCode
    if ($rc -eq 0) {
        Write-Host "big_db_test: load run completed $Seconds s and exited cleanly"
    }
    else {
        Write-Host "big_db_test: app exited with rc=$rc"
    }
}
else {
    $appProc.Kill()
    Write-Host "big_db_test: app failed to exit gracefully (killed after $Seconds s + $grace s grace)"
    $rc = 1
}

try { $sim.Kill() } catch { }
exit $rc
