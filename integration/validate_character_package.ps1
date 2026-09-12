param([switch]$ProfileOnly)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$Root='H:\DarkRelicCharacterPackage\Windows\DarkRelicSmoke'
$Evidence='H:\DarkRelicCharacters-20260912\IntegrationEvidence'
$Receipt="$Evidence\character-stability.json"
$Build=Get-Content "$Evidence\character-build.json" -Raw | ConvertFrom-Json
if(!$Build.complete -or $Build.phase -ne 'passed') { throw 'Character package is not verified yet' }
$State=@{complete=$false;passed=$false;phase='relaunch';runs=@();started=[DateTime]::UtcNow.ToString('o')}
if($ProfileOnly) {
    $Previous=Get-Content $Receipt -Raw | ConvertFrom-Json
    if(!$Previous.complete -or $Previous.runs.Count -ne 4 -or @($Previous.runs | Where-Object {!$_.passed -or $_.exitCode -ne 0 -or $_.checks -lt 27}).Count) { throw 'Profile-only retry requires four verified relaunches' }
    $State.runs=@($Previous.runs)
    Copy-Item $Receipt "$Evidence\character-stability-$([DateTime]::UtcNow.ToString('yyyyMMddTHHmmss')).json"
}
$State | ConvertTo-Json -Depth 5 | Set-Content $Receipt
try {
    for($Index=2;$Index -le 5 -and !$ProfileOnly;$Index++) {
        $RunStarted=[DateTime]::Now
        $p=Start-Process "$Root\Binaries\Win64\DarkRelicSmoke.exe" -ArgumentList @('-DarkRelicSmoke','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\character-relaunch-$Index.log") -PassThru
        $State.childPid=$p.Id; $State | ConvertTo-Json -Depth 5 | Set-Content $Receipt
        $p.WaitForExit()
        $Result=Get-Content "$Root\IntegrationEvidence\runtime-smoke.json" -Raw | ConvertFrom-Json
        if((Get-Item "$Root\IntegrationEvidence\runtime-smoke.json").LastWriteTime -lt $RunStarted) { throw 'Runtime receipt was not refreshed by this process' }
        $State.runs+=@{run=$Index;exitCode=$p.ExitCode;checks=$Result.checks;passed=$Result.passed}
        $State | ConvertTo-Json -Depth 5 | Set-Content $Receipt
        if($p.ExitCode -ne 0 -or !$Result.passed -or $Result.checks -lt 27) { throw "Packaged relaunch $Index failed" }
    }
    $State.phase='profile'; $State | ConvertTo-Json -Depth 5 | Set-Content $Receipt
    $Start=[DateTime]::Now
    $p=Start-Process "$Root\Binaries\Win64\DarkRelicSmoke.exe" -ArgumentList @('-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080','-csvCaptureFrames=1800','-ExecCmds="t.maxFPS 0"',"-abslog=$Evidence\character-profile.log") -PassThru
    $State.childPid=$p.Id; $State | ConvertTo-Json -Depth 5 | Set-Content $Receipt
    $Profile=$null
    $Deadline=[DateTime]::Now.AddSeconds(100)
    while([DateTime]::Now -lt $Deadline -and !$p.HasExited) {
        $Profile=Get-ChildItem "$Root\Saved\Profiling\CSV" -Filter '*.csv' -ErrorAction SilentlyContinue | Where-Object {$_.LastWriteTime -ge $Start -and $_.Length -gt 10000} | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if($Profile) { Start-Sleep -Seconds 2; break }
        Start-Sleep -Milliseconds 500
        $p.Refresh()
    }
    $p.Refresh()
    if(!$p.HasExited) {
        $Shell=New-Object -ComObject WScript.Shell
        if(!$Shell.AppActivate($p.Id)) { throw 'Could not focus the profiling game for its Escape control' }
        Start-Sleep -Milliseconds 300
        $Shell.SendKeys('{ESC}')
        $State.closeMethod='Escape game control'
        if(!$p.WaitForExit(20000)) { throw 'Profiling game did not exit within 20 seconds' }
    }
    $State.profileExit=$p.ExitCode
    if(!$Profile -or $p.ExitCode -ne 0) { throw 'No successful completed frame profile' }
    $Reader=[IO.File]::OpenText($Profile.FullName)
    try {
        $Header=$Reader.ReadLine().Split(',')
        $Column=[Array]::IndexOf($Header,'FrameTime')
        if($Column -lt 0) { throw 'FrameTime column missing' }
        $Times=New-Object 'System.Collections.Generic.List[double]'
        $Row=0
        while(!$Reader.EndOfStream) {
            $Cells=$Reader.ReadLine().Split(','); $Value=0.0
            if($Cells.Count -gt $Column -and [double]::TryParse($Cells[$Column],[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$Value) -and $Value -gt 0) {
                $Row++
                if($Row -gt 300) { $Times.Add($Value) }
            }
        }
    } finally { $Reader.Dispose() }
    if($Times.Count -lt 900) { throw 'Too few post-warmup frame samples' }
    $Sorted=@($Times | Sort-Object); $Mean=($Times | Measure-Object -Average).Average
    $State.profile=@{source=$Profile.FullName;samples=$Times.Count;warmupFramesExcluded=300;averageFps=[Math]::Round(1000/$Mean,2);p95FrameMs=$Sorted[[Math]::Ceiling($Sorted.Count*.95)-1];p99FrameMs=$Sorted[[Math]::Ceiling($Sorted.Count*.99)-1];scope='1080p DX11 stationary player with active enemies; no traversal benchmark';runEndedDuringSample=[bool](Select-String -Path "$Evidence\character-profile.log" -Pattern 'DARK_RELIC_RUN_ENDED' -Quiet)}
    $State.passed=$true; $State.phase='passed'
} catch { $State.error=$_.Exception.Message; $State.phase='failed' }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); $State | ConvertTo-Json -Depth 6 | Set-Content $Receipt }
