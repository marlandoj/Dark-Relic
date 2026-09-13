param([string]$Delivery='H:\DarkRelicPolishDelivery-20260912')
$ErrorActionPreference='Stop'
$Root='H:\DarkRelicPolish-20260912'
$Source='H:\DarkRelicEnemyFeedback-20260912'
$State=@{complete=$false;phase='preflight';started=[DateTime]::UtcNow.ToString('o');pid=$PID}
$Job="$Delivery\job.json"
if (Test-Path $Job) { throw 'Install receipt already exists; inspect before retry' }
function Save-State { $State | ConvertTo-Json -Depth 8 | Set-Content $Job }
Save-State
try {
    if ((Test-Path $Root) -or (Test-Path 'H:\DarkRelicPolishPackage')) { throw 'Candidate destination already exists' }
    if (Get-Process UnrealEditor,UnrealEditor-Cmd,DarkRelicSmoke,UnrealBuildTool,AutomationTool -ErrorAction SilentlyContinue) { throw 'Existing game or build must finish first' }
    $Prior=Get-Content "$Source\IntegrationEvidence\enemy-build.json" -Raw | ConvertFrom-Json
    if (!$Prior.complete -or $Prior.phase -ne 'passed') { throw 'Aura baseline is not verified' }
    if ((Get-PSDrive H).Free -lt 25GB) { throw 'Need 25 GB free before copying' }
    $Protected=@()
    foreach($Package in @('H:\DarkRelicPlayablePackage','H:\DarkRelicCharacterPackage','H:\DarkRelicEnhancedPackage','H:\DarkRelicWardenPackage','H:\DarkRelicFeedbackPackage','H:\DarkRelicAuraPackage','H:\DarkRelicEnemyFeedbackPackage')) {
        $Protected+=Get-ChildItem $Package -Recurse -File | Where-Object { $_.Extension -in '.exe','.pak','.ucas','.utoc' } | ForEach-Object { @{path=$_.FullName;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash} }
    }
    $Archive='H:\DarkRelicSubmission-20260912\Dark-Relic-Windows.zip'
    if (Test-Path $Archive) { $Protected+=@{path=$Archive;sha256=(Get-FileHash $Archive -Algorithm SHA256).Hash} }
    $Protected | ConvertTo-Json -Depth 4 | Set-Content "$Delivery\protected-before.json"
    $State.phase='copy'; Save-State
    & robocopy $Source $Root /E /XD Intermediate Saved IntegrationEvidence .git /R:1 /W:1 /NFL /NDL /NJH /NJS *> "$Delivery\copy.log"
    if ($LASTEXITCODE -gt 7) { throw 'Project copy failed' }
    $Kit=Get-ChildItem "$Delivery\source" -Directory | Select-Object -First 1 -ExpandProperty FullName
    Copy-Item "$Kit\Plugins\DarkRelicCore\Source\*" "$Root\Plugins\DarkRelicCore\Source" -Recurse -Force
    New-Item -ItemType Directory "$Root\IntegrationEvidence" | Out-Null
    Copy-Item "$Kit\integration\*" "$Root\IntegrationEvidence" -Force
    $State.phase='build'; Save-State
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$Root\IntegrationEvidence\run_review_build.ps1" -Package *> "$Delivery\build-launch.log"
    $State.buildExit=$LASTEXITCODE
    if ($LASTEXITCODE -ne 0) { throw 'Enemy feedback build failed; read polish-build.json' }
    $State.phase='passed'
} catch { $State.phase='failed'; $State.error=$_.Exception.Message }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); Save-State }
if ($State.phase -ne 'passed') { exit 1 }
