param([switch]$Package,[switch]$ResumePackage)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$Root='H:\DarkRelicAura-20260912'
$Engine='H:\Epic Games\UE_5.8\UE_5.8\Engine'
$Evidence="$Root\IntegrationEvidence"
$Receipt="$Evidence\aura-build.json"
$Map='/Game/WidowfenPrep/LVL_DarkRelicEnhanced'
$State=@{complete=$false;phase='compile';started=[DateTime]::UtcNow.ToString('o');project=$Root}
if(Test-Path $Receipt) {
    $Prior=Get-Content $Receipt -Raw | ConvertFrom-Json
    if(!$Prior.complete) { throw 'Prior character build is incomplete; inspect it before retrying' }
    Copy-Item $Receipt "$Evidence\enhancement-build-$([DateTime]::UtcNow.ToString('yyyyMMddTHHmmss')).json"
}
$State | ConvertTo-Json | Set-Content $Receipt
function Save-State { $State | ConvertTo-Json -Depth 5 | Set-Content $Receipt }
function Run-Game([string]$Exe,[string[]]$Arguments,[string]$Stage) {
    $State.phase=$Stage; Save-State
    $Process=Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru
    $State.childPid=$Process.Id; Save-State
    if(!$Process.WaitForExit(180000)) { throw "$Stage exceeded 180 seconds; inspect registered child before retry" }
    $State["${Stage}Exit"]=$Process.ExitCode; Save-State
    if($Process.ExitCode -ne 0) { throw "$Stage failed with exit $($Process.ExitCode)" }
}
try {
    if (!$ResumePackage) {
    & "$Engine\Build\BatchFiles\Build.bat" DarkRelicSmokeEditor Win64 Development "-Project=$Root\DarkRelicSmoke.uproject" -WaitMutex -NoHotReloadFromIDE -NoLiveCoding *> "$Evidence\character-compile.log"
    $State.compileExit=$LASTEXITCODE; Save-State
    if($LASTEXITCODE -ne 0) { throw 'Character editor compilation failed' }
    $State.phase='bindings'; Save-State
    & "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Root\DarkRelicSmoke.uproject" -run=pythonscript "-script=$Evidence/bind_warden_aura.py" -unattended -NullRHI "-abslog=$Evidence\aura-bindings.log"
    $State.bindingsExit=$LASTEXITCODE; Save-State
    $Bindings=Get-Content "$Evidence\aura-bindings.json" -Raw | ConvertFrom-Json
    if($LASTEXITCODE -ne 0 -or !$Bindings.passed) { throw 'Feedback binding validation failed' }
    Run-Game "$Engine\Binaries\Win64\UnrealEditor.exe" @("$Root\DarkRelicSmoke.uproject",$Map,'-game','-DarkRelicSmoke','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\character-runtime.log") 'runtime'
    $Runtime=Get-Content "$Evidence\runtime-smoke.json" -Raw | ConvertFrom-Json
    if(!$Runtime.passed -or $Runtime.checks -lt 70) { throw 'Character runtime checks did not pass' }
    Copy-Item "$Evidence\runtime-smoke.json" "$Evidence\character-runtime.json"
    Run-Game "$Engine\Binaries\Win64\UnrealEditor.exe" @("$Root\DarkRelicSmoke.uproject",$Map,'-game','-DarkRelicCapture','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\character-capture.log") 'capture'
    if(!(Test-Path "$Evidence\playable-frame.png")) { throw 'Capture file missing' }
    } else {
        $Bindings=Get-Content "$Evidence\aura-bindings.json" -Raw | ConvertFrom-Json
        $Runtime=Get-Content "$Evidence\character-runtime.json" -Raw | ConvertFrom-Json
        if(!$Bindings.passed -or !$Runtime.passed -or $Runtime.checks -lt 70) { throw 'Cannot resume without passing character bindings and runtime evidence' }
        $State.compileExit=$Prior.compileExit
        $State.bindingsExit=$Prior.bindingsExit
        $State.runtimeExit=$Prior.runtimeExit
        $State.captureExit=$Prior.captureExit
    }
    Run-Game "$Engine\Binaries\Win64\UnrealEditor.exe" @("$Root\DarkRelicSmoke.uproject",$Map,'-game','-DarkRelicAuraCapture','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\aura-capture.log") 'aura-capture'
    if($Package) {
        $State.phase='package'; Save-State
        $ConfigPath="$Root\Config\DefaultEngine.ini"
        $Config=[IO.File]::ReadAllText($ConfigPath)
        $Config=$Config -replace '(?m)^GameDefaultMap=.*$',"GameDefaultMap=$Map.LVL_DarkRelicEnhanced"
        $Config=$Config -replace 'DefaultGraphicsRHI=DefaultGraphicsRHI_DX12','DefaultGraphicsRHI=DefaultGraphicsRHI_DX11'
        [IO.File]::WriteAllText($ConfigPath,$Config)
        & "$Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$Root\DarkRelicSmoke.uproject" -nop4 -platform=Win64 -clientconfig=Development -nocompileeditor -skipbuildeditor -build -cook "-map=$Map" -stage -pak -iostore -archive -archivedirectory=H:/DarkRelicAuraPackage -utf8output *> "$Evidence\character-package.log"
        $State.packageExit=$LASTEXITCODE; Save-State
        if($LASTEXITCODE -ne 0) { throw 'Character packaging failed' }
        $PackageEvidence='H:\DarkRelicAuraPackage\Windows\DarkRelicSmoke\IntegrationEvidence'
        New-Item -ItemType Directory $PackageEvidence -Force | Out-Null
        Run-Game 'H:\DarkRelicAuraPackage\Windows\DarkRelicSmoke.exe' @('-DarkRelicSmoke','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\character-packaged-runtime.log") 'packaged-runtime'
        $Packaged=Get-Content "$PackageEvidence\runtime-smoke.json" -Raw | ConvertFrom-Json
        if(!$Packaged.passed -or $Packaged.checks -lt 70) { throw 'Packaged character smoke failed' }
        Copy-Item "$PackageEvidence\runtime-smoke.json" "$Evidence\character-packaged-runtime.json"
        Run-Game 'H:\DarkRelicAuraPackage\Windows\DarkRelicSmoke.exe' @('-DarkRelicCapture','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\character-packaged-capture.log") 'packaged-capture'
        Copy-Item "$PackageEvidence\playable-frame.png" "$Evidence\character-packaged-frame.png"
    }
    $State.phase='passed'
} catch { $State.phase='failed'; $State.error=$_.Exception.Message }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); Save-State }

if($State.phase -ne "passed") { exit 1 }
