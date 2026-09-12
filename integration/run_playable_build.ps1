$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$Root = 'H:\DarkRelicIntegration-97ff125'
$Engine = 'H:\Epic Games\UE_5.8\UE_5.8\Engine'
$Receipt = "$Root\IntegrationEvidence\playable-build.json"
$State = @{ complete = $false; phase = 'compile'; started = [DateTime]::UtcNow.ToString('o') }
$State | ConvertTo-Json | Set-Content $Receipt
try {
    & "$Engine\Build\BatchFiles\Build.bat" DarkRelicSmokeEditor Win64 Development "-Project=$Root\DarkRelicSmoke.uproject" -WaitMutex -NoHotReloadFromIDE *> "$Root\IntegrationEvidence\playable-build.log"
    $State.buildExit = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) { throw 'Editor compilation failed' }
    $State.phase = 'map'
    $State | ConvertTo-Json | Set-Content $Receipt
    & "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Root\DarkRelicSmoke.uproject" -run=pythonscript -script=H:/DarkRelicDelivery-97ff125/build_playable_map.py -unattended -NullRHI "-abslog=$Root\IntegrationEvidence\playable-map.log"
    $State.mapExit = $LASTEXITCODE
    $MapResult = Get-Content "$Root\IntegrationEvidence\playable-map.json" -Raw | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or !$MapResult.passed) { throw 'Map assembly failed' }
    & "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Root\DarkRelicSmoke.uproject" -run=pythonscript -script=H:/DarkRelicDelivery-97ff125/polish_playable_map.py -unattended -NullRHI "-abslog=$Root\IntegrationEvidence\playable-polish.log"
    $Polish = Get-Content "$Root\IntegrationEvidence\playable-polish.json" -Raw | ConvertFrom-Json
    if (!$Polish.passed) { throw 'Visual polish failed' }
    $State.phase = 'runtime-smoke'
    $State | ConvertTo-Json | Set-Content $Receipt
    $RuntimeProcess = Start-Process -FilePath "$Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList @("$Root\DarkRelicSmoke.uproject",'/Game/WidowfenPrep/LVL_DarkRelicPlayable','-game','-DarkRelicSmoke','-unattended','-NullRHI','-nosound',"-abslog=$Root\IntegrationEvidence\runtime-smoke.log") -PassThru
    $RuntimeProcess.WaitForExit()
    $State.runtimeExit = $RuntimeProcess.ExitCode
    $Runtime = Get-Content "$Root\IntegrationEvidence\runtime-smoke.json" -Raw | ConvertFrom-Json
    if ($RuntimeProcess.ExitCode -ne 0 -or !$Runtime.passed) { throw 'Runtime smoke failed' }
    $State.phase = 'capture'
    $State | ConvertTo-Json | Set-Content $Receipt
    $CaptureProcess = Start-Process -FilePath "$Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList @("$Root\DarkRelicSmoke.uproject",'/Game/WidowfenPrep/LVL_DarkRelicPlayable','-game','-DarkRelicCapture','-unattended','-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Root\IntegrationEvidence\capture.log") -PassThru
    $CaptureProcess.WaitForExit()
    $State.captureExit = $CaptureProcess.ExitCode
    if ($CaptureProcess.ExitCode -ne 0) { throw 'Rendered capture failed' }
    $State.phase = 'package'
    $State | ConvertTo-Json | Set-Content $Receipt
    $ConfigPath = "$Root\Config\DefaultEngine.ini"
    $Config = [IO.File]::ReadAllText($ConfigPath)
    $Config = $Config -replace '(?m)^GameDefaultMap=.*$','GameDefaultMap=/Game/WidowfenPrep/LVL_DarkRelicPlayable.LVL_DarkRelicPlayable'
    $Config = $Config -replace 'DefaultGraphicsRHI=DefaultGraphicsRHI_DX12','DefaultGraphicsRHI=DefaultGraphicsRHI_DX11'
    [IO.File]::WriteAllText($ConfigPath,$Config)
    $GameConfig = "$Root\Config\DefaultGame.ini"
    $GameText = [IO.File]::ReadAllText($GameConfig)
    if (!$GameText.Contains('DirectoriesToAlwaysCook=(Path="/Game/WidowfenPrep/Sources/wooden_handle_saber")')) {
        $GameText += "`r`n[/Script/UnrealEd.ProjectPackagingSettings]`r`n+DirectoriesToAlwaysCook=(Path=`"/Game/WidowfenPrep/Sources/wooden_handle_saber`")`r`n+DirectoriesToAlwaysCook=(Path=`"/Game/Characters/Mannequins/Anims/Unarmed`")`r`n"
        [IO.File]::WriteAllText($GameConfig,$GameText)
    }
    & "$Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$Root\DarkRelicSmoke.uproject" -nop4 -platform=Win64 -clientconfig=Development -build -cook -map=/Game/WidowfenPrep/LVL_DarkRelicPlayable -stage -pak -iostore -archive -archivedirectory=H:/DarkRelicPlayablePackage -utf8output *> "$Root\IntegrationEvidence\playable-package.log"
    $State.packageExit = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) { throw 'Packaging failed' }
    $State.phase = 'passed'
} catch { $State.error = $_.Exception.Message; $State.phase = 'failed' }
finally { $State.complete = $true; $State | ConvertTo-Json | Set-Content $Receipt }
