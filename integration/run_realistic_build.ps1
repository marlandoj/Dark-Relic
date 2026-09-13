param([string]$Root='H:\DarkRelicRealistic-20260913',[string]$Output='H:\DarkRelicRealisticPackage')
$ErrorActionPreference='Stop'
$Evidence="$Root\IntegrationEvidence"
$Receipt="$Evidence\realistic-build.json"
$Engine='H:\Epic Games\UE_5.8\UE_5.8\Engine'
$Map='/Game/WidowfenPrep/LVL_DarkRelicRealistic'
if(Test-Path $Receipt) { throw 'Build receipt exists; inspect it before any retry' }
$State=@{complete=$false;phase='scene';started=[DateTime]::UtcNow.ToString('o');pid=$PID;project=$Root;map=$Map}
function Save-State { $State | ConvertTo-Json -Depth 8 | Set-Content $Receipt }
function Run-Child([string]$Exe,[string[]]$Arguments,[string]$Stage,[int]$Timeout=240000) {
    $State.phase=$Stage; Save-State
    $P=Start-Process $Exe -ArgumentList $Arguments -PassThru -RedirectStandardOutput "$Evidence\$Stage-stdout.log" -RedirectStandardError "$Evidence\$Stage-stderr.log"
    $null=$P.Handle
    $State.childPid=$P.Id; Save-State
    if(!$P.WaitForExit($Timeout)) { throw "$Stage timed out; inspect child $($P.Id) before retry" }
    $P.WaitForExit()
    $State["${Stage}Exit"]=$P.ExitCode; Save-State
    if($P.ExitCode -ne 0) { throw "$Stage failed with exit $($P.ExitCode)" }
}
Save-State
try {
    if(Test-Path $Output) { throw 'Package output exists; preserve it' }
    if(Get-Process UnrealEditor,UnrealEditor-Cmd,DarkRelicSmoke,UnrealBuildTool,AutomationTool -ErrorAction SilentlyContinue) { throw 'Existing runtime or build must finish first' }
    Run-Child "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" @("$Root\DarkRelicSmoke.uproject",'-run=pythonscript',"-script=$Evidence/realistic_widowfen.py",'-unattended','-NullRHI',"-abslog=$Evidence\realistic-scene.log") 'scene' 600000
    $Scene=Get-Content "$Evidence\realistic-scene.json" -Raw | ConvertFrom-Json
    if(!$Scene.complete -or !$Scene.passed) { throw 'Scene assembly failed' }
    Run-Child "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" @("$Root\DarkRelicSmoke.uproject",'-run=pythonscript',"-script=$Evidence/validate_realistic_scene.py",'-unattended','-NullRHI',"-abslog=$Evidence\realistic-parity.log") 'parity'
    $Parity=Get-Content "$Evidence\realistic-parity.json" -Raw | ConvertFrom-Json
    if(!$Parity.complete -or !$Parity.passed) { throw 'Scene gameplay parity failed' }
    Run-Child "$Engine\Binaries\Win64\UnrealEditor.exe" @("$Root\DarkRelicSmoke.uproject",$Map,'-game','-DarkRelicSmoke','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\realistic-runtime.log") 'runtime'
    $Runtime=Get-Content "$Evidence\runtime-smoke.json" -Raw | ConvertFrom-Json
    if(!$Runtime.passed -or $Runtime.checks -lt 102) { throw 'Runtime regression checks failed' }
    Copy-Item "$Evidence\runtime-smoke.json" "$Evidence\realistic-editor-runtime.json"
    Run-Child "$Engine\Binaries\Win64\UnrealEditor.exe" @("$Root\DarkRelicSmoke.uproject",$Map,'-game','-DarkRelicPolishCapture','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\realistic-capture.log") 'capture'
    if(Select-String "$Evidence\realistic-capture.log" -Pattern 'Failed to compile Material|Missing ComponentMask input' -Quiet) { throw 'Material compilation failed in rendered DX11' }
    foreach($Name in @('neutral','warnings','ward-locked','ward-ready','extracting','victory','upgrade','death')) {
        if(!(Test-Path "$Evidence\polish-$Name.png")) { throw "Missing gameplay capture: $Name" }
    }
    $State.phase='ready-for-visual-review'
} catch { $State.phase='failed'; $State.error=$_.Exception.Message }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); Save-State }
if($State.phase -eq 'failed') { exit 1 }
