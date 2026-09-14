param([Parameter(Mandatory=$true)][string]$SourceCommit,[Parameter(Mandatory=$true)][string]$ManifestSha256)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$Root='H:\DarkRelicBodyGlow-20260913'
$Delivery='H:\DarkRelicBodyGlowDelivery-20260913'
$Evidence="$Root\IntegrationEvidence"
$Output='H:\DarkRelicBodyGlowPackage\Windows'
$Receipt="$Evidence\body-glow-release.json"
if(Test-Path $Receipt) { throw 'Release receipt exists; inspect before retry' }
$State=@{complete=$false;passed=$false;phase='preflight';pid=$PID;sourceCommit=$SourceCommit;started=[DateTime]::UtcNow.ToString('o')}
function Save-State { $State | ConvertTo-Json -Depth 8 | Set-Content $Receipt }
Save-State
try {
    $Package=Get-Content "$Delivery\package-job.json" -Raw | ConvertFrom-Json
    if(!$Package.complete -or $Package.phase -ne 'ready-for-packaged-review' -or $Package.checks -lt 102) { throw 'Packaged runtime must pass' }
    if(Get-Process UnrealEditor,UnrealEditor-Cmd,DarkRelicSmoke,UnrealBuildTool,AutomationTool -ErrorAction SilentlyContinue) { throw 'Another runtime is active' }
    $Review=Get-Content "$Delivery\packaged-visual-review.json" -Raw | ConvertFrom-Json
    if(!$Review.passed -or $Review.frames.Count -ne 4 -or @($Review.frames.name | Select-Object -Unique).Count -ne 4) { throw 'Four reviewed Fury states required' }
    foreach($Frame in $Review.frames) {
        if($Frame.name -notin 'charge','peak','fade','off') { throw 'Unexpected capture state' }
        if((Get-FileHash "$Evidence\packaged-aura-$($Frame.name).png").Hash -ne $Frame.sha256) { throw 'Reviewed capture changed' }
    }
    if((Get-FileHash "$Delivery\release-source-manifest.json").Hash -ne $ManifestSha256) { throw 'Source manifest digest differs' }
    $Manifest=Get-Content "$Delivery\release-source-manifest.json" -Raw | ConvertFrom-Json
    if($Manifest.Count -lt 14 -or @($Manifest.path | Select-Object -Unique).Count -ne $Manifest.Count) { throw 'Incomplete or duplicate source manifest' }
    foreach($File in $Manifest) {
        $Base=if($File.location -eq 'project'){$Root}elseif($File.location -eq 'delivery'){$Delivery}else{throw 'Invalid source location'}
        $Path=[IO.Path]::GetFullPath((Join-Path $Base $File.path))
        if(!$Path.StartsWith($Base+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Source path escapes candidate' }
        if((Get-FileHash $Path).Hash -ne $File.sha256) { throw "Source mismatch: $($File.path)" }
    }
    $State.sourceFiles=$Manifest.Count
    $State.phase='ordinary-play'; Save-State
    $Log="$Evidence\body-glow-ordinary.log"
    $Game=Start-Process "$Output\DarkRelicSmoke\Binaries\Win64\DarkRelicSmoke.exe" -ArgumentList @('-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Log") -PassThru
    $null=$Game.Handle
    $State.childPid=$Game.Id; Save-State
    $Deadline=[DateTime]::UtcNow.AddSeconds(90)
    $Ready=$false
    while([DateTime]::UtcNow -lt $Deadline -and !$Game.HasExited) {
        if(Test-Path $Log) { $Ready=[bool](Select-String $Log -Pattern 'DARK_RELIC_PLAYABLE_READY' -Quiet) }
        if($Ready) { break }
        Start-Sleep -Milliseconds 500
        $Game.Refresh()
    }
    if(!$Ready -or $Game.HasExited) { throw 'Ordinary game did not become ready' }
    $Shell=New-Object -ComObject WScript.Shell
    if(!$Shell.AppActivate($Game.Id)) { throw 'Could not focus candidate' }
    Start-Sleep -Milliseconds 500
    $Shell.SendKeys('r')
    Start-Sleep -Seconds 8
    if(!(Select-String $Log -Pattern 'DARK_RELIC_VOICE event=6 ' -Quiet)) { throw 'Normal R input did not activate Fury' }
    if(!$Shell.AppActivate($Game.Id)) { throw 'Could not focus candidate for Escape' }
    Start-Sleep -Milliseconds 300
    $Shell.SendKeys('{ESC}')
    if(!$Game.WaitForExit(30000)) { throw 'Escape exit timed out; inspect child' }
    $Game.WaitForExit()
    if($Game.ExitCode -ne 0) { throw 'Ordinary game exit failed' }
    $State.ordinaryExit=$Game.ExitCode
    if(Select-String "$Evidence\body-glow-packaged-runtime.log","$Evidence\body-glow-packaged-capture.log",$Log -Pattern 'Failed to compile Material|Missing ComponentMask input|Error \[SM5\]|Fatal error' -Quiet) { throw 'Packaged rendering error' }
    $Protected=Get-Content "$Delivery\protected-before.json" -Raw | ConvertFrom-Json
    foreach($File in $Protected) {
        if((Get-FileHash $File.path).Hash -ne $File.sha256) { throw "Protected release changed: $($File.path)" }
    }
    $State.protectedFiles=$Protected.Count
    $State.packageHashes=@(Get-ChildItem $Output -Recurse -File | Where-Object {$_.Extension -in '.exe','.dll','.pak','.ucas','.utoc'} | ForEach-Object {@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})
    $Shortcut=Join-Path ([Environment]::GetFolderPath('Desktop')) 'Dark Relic Warden Body Glow.lnk'
    if(Test-Path $Shortcut) { throw 'Shortcut exists; preserve and inspect' }
    $Link=$Shell.CreateShortcut($Shortcut)
    $Link.TargetPath="$Output\DarkRelicSmoke.exe"
    $Link.Arguments='-dx11 -windowed -ResX=1920 -ResY=1080'
    $Link.WorkingDirectory=$Output
    $Link.Save()
    $Check=$Shell.CreateShortcut($Shortcut)
    if($Check.TargetPath -ne $Link.TargetPath -or $Check.Arguments -ne $Link.Arguments -or !(Test-Path $Check.TargetPath)) { throw 'Shortcut readback mismatch' }
    $State.shortcut=$Shortcut; $State.packagedChecks=$Package.checks; $State.reviewedFrames=$Review.frames.Count
    $State.passed=$true; $State.phase='passed'
} catch { $State.phase='failed'; $State.error=$_.Exception.Message }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); Save-State }
if(!$State.passed) { exit 1 }
