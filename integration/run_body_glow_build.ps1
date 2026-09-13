param([string]$Delivery='H:\DarkRelicBodyGlowDelivery-20260913',[switch]$Package)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$Root='H:\DarkRelicBodyGlow-20260913'
$Source='H:\DarkRelicRealistic-20260913'
$Output='H:\DarkRelicBodyGlowPackage'
$Engine='H:\Epic Games\UE_5.8\UE_5.8\Engine'
$Map='/Game/WidowfenPrep/LVL_DarkRelicRealistic'
$Evidence="$Root\IntegrationEvidence"
$Receipt=if($Package){"$Delivery\package-job.json"}else{"$Delivery\build-job.json"}
if(Test-Path $Receipt) { throw 'Receipt exists; inspect it before retry' }
$State=@{complete=$false;phase='preflight';pid=$PID;started=[DateTime]::UtcNow.ToString('o');project=$Root}
function Save-State { $State | ConvertTo-Json -Depth 8 | Set-Content $Receipt }
function Run-Game([string]$Exe,[string[]]$Arguments,[string]$Stage) {
    $State.phase=$Stage; Save-State
    $P=Start-Process $Exe -ArgumentList $Arguments -PassThru
    $null=$P.Handle
    $State.childPid=$P.Id; Save-State
    if(!$P.WaitForExit(240000)) { throw "$Stage timed out; inspect child before retry" }
    $P.WaitForExit()
    $State["${Stage}Exit"]=$P.ExitCode; Save-State
    if($P.ExitCode -ne 0) { throw "$Stage failed with exit $($P.ExitCode)" }
}
function Check-Materials([string]$Log) {
    if(Select-String $Log -Pattern 'Failed to compile Material|Missing ComponentMask input|Error \[SM5\]' -Quiet) { throw 'Rendered DX11 material compilation failed' }
}
Save-State
try {
    if(Get-Process UnrealEditor,UnrealEditor-Cmd,DarkRelicSmoke,UnrealBuildTool,AutomationTool -ErrorAction SilentlyContinue) { throw 'Existing runtime or build must finish first' }
    if(!$Package) {
        if(Test-Path $Root) { throw 'Candidate already exists; preserve it' }
        if((Get-PSDrive H).Free -lt 25GB) { throw 'Need 25 GB free' }
        $Release=Get-Content "$Source\IntegrationEvidence\realistic-finalize.json" -Raw | ConvertFrom-Json
        if(!$Release.complete -or !$Release.passed) { throw 'Realistic baseline is not verified' }
        $Protected=@(Get-Content 'H:\DarkRelicRealisticDelivery-20260913\protected-before.json' -Raw | ConvertFrom-Json)
        $Protected+=@(Get-ChildItem 'H:\DarkRelicRealisticPackage' -Recurse -File | Where-Object { $_.Extension -in '.exe','.dll','.pak','.ucas','.utoc' } | ForEach-Object { @{path=$_.FullName;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash} })
        foreach($F in $Protected) { if((Get-FileHash $F.path -Algorithm SHA256).Hash -ne $F.sha256) { throw "Protected baseline differs: $($F.path)" } }
        $Protected | ConvertTo-Json -Depth 5 | Set-Content "$Delivery\protected-before.json"
        $State.phase='copy'; Save-State
        & robocopy $Source $Root /E /XD Intermediate Saved IntegrationEvidence .git /R:1 /W:1 /NFL /NDL /NJH /NJS *> "$Delivery\copy.log"
        if($LASTEXITCODE -gt 7) { throw 'Project copy failed' }
        New-Item -ItemType Directory $Evidence | Out-Null
        Copy-Item "$Delivery\integration\bind_warden_body_glow.py" $Evidence
        Copy-Item "$Delivery\Plugins\*" "$Root\Plugins" -Recurse -Force
        $State.phase='compile'; Save-State
        & "$Engine\Build\BatchFiles\Build.bat" DarkRelicSmokeEditor Win64 Development "-Project=$Root\DarkRelicSmoke.uproject" -WaitMutex -NoHotReloadFromIDE -NoLiveCoding *> "$Evidence\body-glow-compile.log"
        $State.compileExit=$LASTEXITCODE; Save-State
        if($LASTEXITCODE -ne 0) { throw 'Editor compilation failed' }
        Run-Game "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" @("$Root\DarkRelicSmoke.uproject",'-run=pythonscript',"-script=$Evidence/bind_warden_body_glow.py",'-unattended','-NullRHI',"-abslog=$Evidence\body-glow-bindings.log") 'bindings'
        $Bindings=Get-Content "$Evidence\body-glow-bindings.json" -Raw | ConvertFrom-Json
        if(!$Bindings.complete -or !$Bindings.passed) { throw 'Body glow bindings failed' }
        Run-Game "$Engine\Binaries\Win64\UnrealEditor.exe" @("$Root\DarkRelicSmoke.uproject",$Map,'-game','-DarkRelicSmoke','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\body-glow-runtime.log") 'runtime'
        $Runtime=Get-Content "$Evidence\runtime-smoke.json" -Raw | ConvertFrom-Json
        if(!$Runtime.passed -or $Runtime.checks -lt 102) { throw 'Runtime regression checks failed' }
        Copy-Item "$Evidence\runtime-smoke.json" "$Evidence\body-glow-editor-runtime.json"
        Run-Game "$Engine\Binaries\Win64\UnrealEditor.exe" @("$Root\DarkRelicSmoke.uproject",$Map,'-game','-DarkRelicAuraCapture','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\body-glow-capture.log") 'capture'
        Check-Materials "$Evidence\body-glow-capture.log"
        foreach($Name in @('charge','peak','fade','off')) { if(!(Test-Path "$Evidence\aura-$Name.png")) { throw "Missing capture $Name" } }
        $State.phase='ready-for-visual-review'
    } else {
        $Prior=Get-Content "$Delivery\build-job.json" -Raw | ConvertFrom-Json
        if(!$Prior.complete -or $Prior.phase -ne 'ready-for-visual-review') { throw 'Editor build must pass first' }
        if(!(Get-Content "$Delivery\visual-review.json" -Raw | ConvertFrom-Json).passed) { throw 'Visual review must pass first' }
        if(Test-Path $Output) { throw 'Package already exists; preserve it' }
        $State.phase='package'; Save-State
        & "$Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$Root\DarkRelicSmoke.uproject" -nop4 -platform=Win64 -clientconfig=Development -nocompileeditor -skipbuildeditor -build -cook "-map=$Map" -stage -pak -iostore -archive "-archivedirectory=$Output" -utf8output *> "$Evidence\body-glow-package.log"
        $State.packageExit=$LASTEXITCODE; Save-State
        if($LASTEXITCODE -ne 0) { throw 'Packaging failed' }
        $PackageEvidence="$Output\Windows\DarkRelicSmoke\IntegrationEvidence"
        New-Item -ItemType Directory $PackageEvidence -Force | Out-Null
        Run-Game "$Output\Windows\DarkRelicSmoke.exe" @('-DarkRelicSmoke','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\body-glow-packaged-runtime.log") 'packaged-runtime'
        $Runtime=Get-Content "$PackageEvidence\runtime-smoke.json" -Raw | ConvertFrom-Json
        if(!$Runtime.passed -or $Runtime.checks -lt 102) { throw 'Packaged regression checks failed' }
        Copy-Item "$PackageEvidence\runtime-smoke.json" "$Evidence\body-glow-packaged-runtime.json"
        Run-Game "$Output\Windows\DarkRelicSmoke.exe" @('-DarkRelicAuraCapture','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\body-glow-packaged-capture.log") 'packaged-capture'
        Check-Materials "$Evidence\body-glow-packaged-capture.log"
        foreach($Name in @('charge','peak','fade','off')) { Copy-Item "$PackageEvidence\aura-$Name.png" "$Evidence\packaged-aura-$Name.png" }
        $Protected=Get-Content "$Delivery\protected-before.json" -Raw | ConvertFrom-Json
        foreach($F in $Protected) { if((Get-FileHash $F.path -Algorithm SHA256).Hash -ne $F.sha256) { throw "Protected release changed: $($F.path)" } }
        $State.checks=$Runtime.checks; $State.protectedFiles=$Protected.Count; $State.phase='ready-for-packaged-review'
    }
} catch { $State.phase='failed'; $State.error=$_.Exception.Message }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); Save-State }
if($State.phase -eq 'failed') { exit 1 }
