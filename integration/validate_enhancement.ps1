$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$Root='H:\DarkRelicEnhancedPackage\Windows\DarkRelicSmoke'
$Evidence='H:\DarkRelicEnhanced-20260912\IntegrationEvidence'
$Receipt="$Evidence\enhancement-stability.json"
$Exe="$Root\Binaries\Win64\DarkRelicSmoke.exe"
$Build=Get-Content "$Evidence\enhancement-build.json" -Raw | ConvertFrom-Json
if(!$Build.complete -or $Build.phase -ne 'passed') { throw 'Candidate build has not passed' }
if(Get-Process UnrealEditor,UnrealEditor-Cmd,DarkRelicSmoke -ErrorAction SilentlyContinue) { throw 'Game or editor already active' }
if(Test-Path $Receipt) { throw 'Validation already registered; inspect receipt before retry' }
$State=@{complete=$false;passed=$false;started=[DateTime]::UtcNow.ToString('o');runs=@()}
function Save-State { $State | ConvertTo-Json -Depth 8 | Set-Content $Receipt }
Save-State
try {
    foreach($Mode in @('normal','csv-audio','csv-noaudio')) {
        $Start=[DateTime]::Now
        $Arguments=@('-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080', '-ExecCmds="t.maxFPS 120"',"-abslog=$Evidence\enhancement-$Mode.log")
        if($Mode -ne 'normal') { $Arguments+='-csvCaptureFrames=1800' }
        if($Mode -eq 'csv-noaudio') { $Arguments+='-nosound' }
        $Process=Start-Process $Exe -ArgumentList $Arguments -PassThru
        $State.phase=$Mode; $State.childPid=$Process.Id; Save-State
        $Profile=$null
        $Deadline=[DateTime]::Now.AddSeconds(75)
        while([DateTime]::Now -lt $Deadline -and !$Process.HasExited) {
            if($Mode -eq 'normal') {
                if(([DateTime]::Now-$Start).TotalSeconds -gt 12) { break }
            } else {
                $Profile=Get-ChildItem "$Root\Saved\Profiling\CSV" -Filter '*.csv' -ErrorAction SilentlyContinue | Where-Object {$_.LastWriteTime -ge $Start -and $_.Length -gt 10000} | Sort-Object LastWriteTime -Descending | Select-Object -First 1
                if($Profile) { Start-Sleep -Seconds 2; break }
            }
            Start-Sleep -Milliseconds 500
            $Process.Refresh()
        }
        if(!$Process.HasExited) {
            $Shell=New-Object -ComObject WScript.Shell
            if(!$Shell.AppActivate($Process.Id)) { throw 'Could not focus candidate for Escape' }
            Start-Sleep -Milliseconds 300
            $Shell.SendKeys('{ESC}')
            if(!$Process.WaitForExit(20000)) { throw 'Candidate did not exit after Escape; inspect child PID' }
        }
        $Process.WaitForExit()
        $Run=@{mode=$Mode;exitCode=$Process.ExitCode;normalExit=($Process.ExitCode -eq 0);closeMethod='Escape';profile=$null}
        if($Profile) {
            $Reader=[IO.File]::OpenText($Profile.FullName)
            $Times=New-Object 'System.Collections.Generic.List[double]'
            try {
                $Header=$Reader.ReadLine().Split(',')
                $Column=[Array]::IndexOf($Header,'FrameTime')
                $Rows=0
                while(!$Reader.EndOfStream -and $Column -ge 0) {
                    $Cells=$Reader.ReadLine().Split(','); $Value=0.0
                    if($Cells.Count -gt $Column -and [double]::TryParse($Cells[$Column],[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$Value) -and $Value -gt 0) {
                        $Rows++
                        if($Rows -gt 300) { $Times.Add($Value) }
                    }
                }
            } finally { $Reader.Dispose() }
            if($Times.Count -gt 0) {
                $Sorted=@($Times | Sort-Object)
                $Mean=($Times | Measure-Object -Average).Average
                $Run.profile=@{path=$Profile.FullName;samples=$Times.Count;warmupExcluded=300;averageFps=[Math]::Round(1000/$Mean,2);p95Ms=$Sorted[[Math]::Ceiling($Sorted.Count*.95)-1];p99Ms=$Sorted[[Math]::Ceiling($Sorted.Count*.99)-1];scope='stationary player, active enemies, 1080p DX11; not traversal or human playtest';runEndedDuringSample=[bool](Select-String "$Evidence\enhancement-$Mode.log" -Pattern 'DARK_RELIC_RUN_ENDED' -Quiet)}
            }
        }
        $State.runs+=@($Run); Save-State
    }
    $State.passed=(@($State.runs | Where-Object {!$_.normalExit}).Count -eq 0 -and @($State.runs | Where-Object {$_.mode -ne 'normal' -and (!$_.profile -or $_.profile.samples -lt 900)}).Count -eq 0)
} catch { $State.error=$_.Exception.Message }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); Save-State }
