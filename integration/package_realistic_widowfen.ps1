param([string]$Root='H:\DarkRelicRealistic-20260913',[string]$Output='H:\DarkRelicRealisticPackage')
$ErrorActionPreference='Stop'
$Evidence="$Root\IntegrationEvidence"
$Receipt="$Evidence\realistic-package.json"
$Engine='H:\Epic Games\UE_5.8\UE_5.8\Engine'
$Map='/Game/WidowfenPrep/LVL_DarkRelicRealistic'
if(Test-Path $Receipt) { throw 'Package receipt exists; inspect before retry' }
if(Test-Path $Output) { throw 'Output exists; preserve it' }
$State=@{complete=$false;phase='preflight';pid=$PID;started=[DateTime]::UtcNow.ToString('o');map=$Map}
function Save-State { $State | ConvertTo-Json -Depth 6 | Set-Content $Receipt }
Save-State
try {
    $Prior=Get-Content "$Evidence\realistic-build.json" -Raw | ConvertFrom-Json
    if(!$Prior.complete -or $Prior.phase -ne 'ready-for-visual-review') { throw 'Scene build did not pass' }
    if(!(Test-Path "$Evidence\visual-review.json") -or !(Get-Content "$Evidence\visual-review.json" -Raw | ConvertFrom-Json).passed) { throw 'Rendered scene review is required' }
    $Parity=Get-Content "$Evidence\realistic-parity.json" -Raw | ConvertFrom-Json
    if(!$Parity.complete -or !$Parity.passed) { throw 'Saved gameplay parity is required' }
    if(Get-Process UnrealEditor,UnrealEditor-Cmd,DarkRelicSmoke,UnrealBuildTool,AutomationTool -ErrorAction SilentlyContinue) { throw 'Existing runtime or build must finish first' }
    $ConfigPath="$Root\Config\DefaultEngine.ini"
    $Config=[IO.File]::ReadAllText($ConfigPath)
    if($Config -notmatch '(?m)^GameDefaultMap=') { throw 'Missing default map setting' }
    $Config=$Config -replace '(?m)^GameDefaultMap=.*$',"GameDefaultMap=$Map.LVL_DarkRelicRealistic"
    $Config=$Config -replace 'DefaultGraphicsRHI=DefaultGraphicsRHI_DX12','DefaultGraphicsRHI=DefaultGraphicsRHI_DX11'
    [IO.File]::WriteAllText($ConfigPath,$Config)
    if([IO.File]::ReadAllText($ConfigPath) -notmatch 'GameDefaultMap=/Game/WidowfenPrep/LVL_DarkRelicRealistic.LVL_DarkRelicRealistic') { throw 'Default map readback failed' }
    $State.phase='package'; Save-State
    & "$Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$Root\DarkRelicSmoke.uproject" -nop4 -platform=Win64 -clientconfig=Development -nocompileeditor -skipbuildeditor -build -cook "-map=$Map" -stage -pak -iostore -archive "-archivedirectory=$Output" -utf8output *> "$Evidence\realistic-package.log"
    $State.packageExit=$LASTEXITCODE; Save-State
    if($LASTEXITCODE -ne 0) { throw 'Packaging failed' }
    $PackageEvidence="$Output\Windows\DarkRelicSmoke\IntegrationEvidence"
    New-Item -ItemType Directory $PackageEvidence -Force | Out-Null
    $State.phase='packaged-runtime'; Save-State
    $P=Start-Process "$Output\Windows\DarkRelicSmoke.exe" -ArgumentList @('-DarkRelicSmoke','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\realistic-packaged-runtime.log") -PassThru
    $null=$P.Handle
    $State.childPid=$P.Id; Save-State
    if(!$P.WaitForExit(180000) -or $P.ExitCode -ne 0) { throw 'Packaged runtime failed; inspect child' }
    $Runtime=Get-Content "$PackageEvidence\runtime-smoke.json" -Raw | ConvertFrom-Json
    if(!$Runtime.passed -or $Runtime.checks -lt 102) { throw 'Packaged regression checks failed' }
    Copy-Item "$PackageEvidence\runtime-smoke.json" "$Evidence\realistic-packaged-runtime.json"
    $State.runtimeExit=$P.ExitCode; $State.checks=$Runtime.checks
    if(!(Select-String "$Evidence\realistic-packaged-runtime.log" -Pattern 'LVL_DarkRelicRealistic' -Quiet)) { throw 'Packaged map name not found in runtime log' }
    $State.phase='passed'
} catch { $State.phase='failed'; $State.error=$_.Exception.Message }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); Save-State }
if($State.phase -ne 'passed') { exit 1 }
