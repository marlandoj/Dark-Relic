param([Parameter(Mandatory=$true)][string]$ManifestPath,[Parameter(Mandatory=$true)][string]$ManifestSha256,[Parameter(Mandatory=$true)][string]$SourceCommit)
$ErrorActionPreference='Stop'
$Root='H:\DarkRelicRealistic-20260913'
$Evidence="$Root\IntegrationEvidence"
$Delivery='H:\DarkRelicRealisticDelivery-20260913'
$Receipt="$Evidence\realistic-finalize.json"
if(Test-Path $Receipt) { throw 'Finalization receipt exists; inspect before retry' }
$State=@{complete=$false;passed=$false;phase='preflight';sourceCommit=$SourceCommit;manifestSha256=$ManifestSha256;driverSha256=(Get-FileHash $PSCommandPath -Algorithm SHA256).Hash;pid=$PID;started=[DateTime]::UtcNow.ToString('o')}
function Save-State { $State | ConvertTo-Json -Depth 7 | Set-Content $Receipt }
Save-State
try {
    $Prior=Get-Content "$Evidence\realistic-release.json" -Raw | ConvertFrom-Json
    $Package=Get-Content "$Evidence\realistic-package.json" -Raw | ConvertFrom-Json
    if(!$Prior.complete -or $Prior.passed -or $Prior.error -ne 'Source mismatch: IntegrationEvidence\realistic_widowfen.py' -or $Prior.ordinaryExit -ne 0 -or $Prior.capture720Exit -ne 0 -or $Prior.capture1080Exit -ne 0) { throw 'Expected completed runtime/capture evidence with only the recorded stale-manifest failure' }
    if(!$Package.complete -or $Package.phase -ne 'passed' -or $Package.checks -lt 102) { throw 'Packaged runtime checks did not pass' }
    if(Get-Process UnrealEditor,UnrealEditor-Cmd,DarkRelicSmoke,UnrealBuildTool,AutomationTool -ErrorAction SilentlyContinue) { throw 'Existing runtime/build remains active' }
    if((Get-FileHash $ManifestPath -Algorithm SHA256).Hash -ne $ManifestSha256) { throw 'Expected source manifest digest differs' }
    $Manifest=Get-Content $ManifestPath -Raw | ConvertFrom-Json
    if($Manifest.Count -ne 18 -or @($Manifest.path | Select-Object -Unique).Count -ne 18) { throw 'Expected 18 unique source files' }
    foreach($File in $Manifest) {
        $Path=[IO.Path]::GetFullPath((Join-Path $Root $File.path))
        if(!$Path.StartsWith($Root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Source path escapes candidate' }
        if((Get-FileHash $Path -Algorithm SHA256).Hash -ne $File.sha256) { throw "Verified repository source differs: $($File.path)" }
    }
    $Review=Get-Content "$Evidence\visual-review-final.json" -Raw | ConvertFrom-Json
    if(!$Review.passed -or $Review.frames.Count -ne 16 -or @($Review.frames.name | Select-Object -Unique).Count -ne 16) { throw 'Sixteen reviewed packaged frames required' }
    foreach($Frame in $Review.frames) {
        if($Frame.name -notmatch '^(720|1080)-(neutral|warnings|ward-locked|ward-ready|extracting|victory|upgrade|death)$') { throw 'Unexpected frame name' }
        if((Get-FileHash "$Evidence\polish-$($Frame.name).png" -Algorithm SHA256).Hash -ne $Frame.sourceSha256) { throw 'Reviewed frame changed' }
    }
    if(Select-String "$Evidence\polish-capture-720.log","$Evidence\polish-capture-1080.log" -Pattern 'Failed to compile Material|Missing ComponentMask input|Fatal error' -Quiet) { throw 'Packaged render error' }
    $Protected=Get-Content "$Delivery\protected-before.json" -Raw | ConvertFrom-Json
    foreach($File in $Protected) {
        if((Get-FileHash $File.path -Algorithm SHA256).Hash -ne $File.sha256) { throw "Protected release changed: $($File.path)" }
    }
    $State.protectedFiles=$Protected.Count
    $State.sourceFiles=$Manifest.Count
    $State.reviewedFrames=$Review.frames.Count
    $State.ordinaryExit=$Prior.ordinaryExit
    $State.packagedChecks=$Package.checks
    $State.packageHashes=@(Get-ChildItem 'H:\DarkRelicRealisticPackage' -Recurse -File | Where-Object {$_.Extension -in '.exe','.pak','.ucas','.utoc'} | ForEach-Object {@{path=$_.FullName;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
    $Shell=New-Object -ComObject WScript.Shell
    $Shortcut=Join-Path ([Environment]::GetFolderPath('Desktop')) 'Dark Relic Realistic Widowfen.lnk'
    $Link=$Shell.CreateShortcut($Shortcut)
    $Target='H:\DarkRelicRealisticPackage\Windows\DarkRelicSmoke.exe'
    $Arguments='-dx11 -windowed -ResX=1920 -ResY=1080'
    if((Test-Path $Shortcut) -and ($Link.TargetPath -ne $Target -or $Link.Arguments -ne $Arguments)) { throw 'Conflicting shortcut must be preserved' }
    $Link.TargetPath=$Target
    $Link.Arguments=$Arguments
    $Link.WorkingDirectory='H:\DarkRelicRealisticPackage\Windows'
    $Link.Save()
    $Check=$Shell.CreateShortcut($Shortcut)
    if($Check.TargetPath -ne $Target -or $Check.Arguments -ne $Arguments) { throw 'Shortcut readback failed' }
    $State.shortcut=$Shortcut
    $State.passed=$true
    $State.phase='passed'
} catch { $State.phase='failed'; $State.error=$_.Exception.Message }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); Save-State }
if(!$State.passed) { exit 1 }
