param([string]$Delivery='H:\DarkRelicAuraDelivery-20260912')
$ErrorActionPreference='Stop'
$Root='H:\DarkRelicAura-20260912'
$Source='H:\DarkRelicFeedback-20260912'
$State=@{complete=$false;phase='preflight';started=[DateTime]::UtcNow.ToString('o');pid=$PID}
$Job="$Delivery\job.json"
function Save-State { $State | ConvertTo-Json -Depth 5 | Set-Content $Job }
Save-State
try {
    if (Test-Path $Root) { throw 'Aura destination already exists; inspect prior attempt' }
    if (Get-Process UnrealEditor,DarkRelicSmoke,UnrealBuildTool -ErrorAction SilentlyContinue) { throw 'Existing game or build process must finish first' }
    $Prior=Get-Content "$Source\IntegrationEvidence\feedback-build.json" -Raw | ConvertFrom-Json
    if (!$Prior.complete -or $Prior.phase -ne 'passed') { throw 'Feedback baseline is not verified' }
    $Protected=@()
    foreach($Package in @('H:\DarkRelicPlayablePackage','H:\DarkRelicCharacterPackage','H:\DarkRelicEnhancedPackage','H:\DarkRelicWardenPackage','H:\DarkRelicFeedbackPackage')) {
        $Protected+=Get-ChildItem $Package -Recurse -File | Where-Object { $_.Extension -in '.exe','.pak','.ucas','.utoc' } | ForEach-Object { @{path=$_.FullName;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash} }
    }
    $Protected | ConvertTo-Json -Depth 4 | Set-Content "$Delivery\protected-before.json"
    $State.phase='copy'; Save-State
    & robocopy $Source $Root /E /XD Intermediate Saved IntegrationEvidence .git /R:1 /W:1 /NFL /NDL /NJH /NJS *> "$Delivery\copy.log"
    if ($LASTEXITCODE -gt 7) { throw 'Project copy failed' }
    Expand-Archive "$Delivery\source.zip" "$Delivery\source"
    Copy-Item "$Delivery\source\Plugins\DarkRelicCore\Source\*" "$Root\Plugins\DarkRelicCore\Source" -Recurse -Force
    New-Item -ItemType Directory "$Root\IntegrationEvidence" | Out-Null
    Copy-Item "$Delivery\source\integration\bind_warden_aura.py" "$Root\IntegrationEvidence\bind_warden_aura.py"
    Copy-Item "$Delivery\source\integration\run_aura_build.ps1" "$Root\IntegrationEvidence\run_aura_build.ps1"
    $State.phase='build'; Save-State
    & "$Root\IntegrationEvidence\run_aura_build.ps1" -Package
    if ($LASTEXITCODE -ne 0) { throw 'Aura build failed; read aura-build.json' }
    $State.phase='passed'
} catch { $State.phase='failed'; $State.error=$_.Exception.Message }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); Save-State }
if ($State.phase -ne 'passed') { exit 1 }
