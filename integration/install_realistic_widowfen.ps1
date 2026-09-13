param([string]$Delivery='H:\DarkRelicRealisticDelivery-20260913')
$ErrorActionPreference='Stop'
$Root='H:\DarkRelicRealistic-20260913'
$Source='H:\DarkRelicPolish-20260912'
$Job="$Delivery\install-job.json"
if(Test-Path $Job) { throw 'Install receipt exists; inspect it before any retry' }
$State=@{complete=$false;phase='preflight';started=[DateTime]::UtcNow.ToString('o');pid=$PID}
function Save-State { $State | ConvertTo-Json -Depth 8 | Set-Content $Job }
Save-State
try {
    if((Test-Path $Root) -or (Test-Path 'H:\DarkRelicRealisticPackage')) { throw 'Candidate destination exists' }
    if(Get-Process UnrealEditor,UnrealEditor-Cmd,DarkRelicSmoke,UnrealBuildTool,AutomationTool -ErrorAction SilentlyContinue) { throw 'Existing runtime or build must finish first' }
    $Prior=Get-Content "$Source\IntegrationEvidence\polish-release.json" -Raw | ConvertFrom-Json
    if(!$Prior.complete -or !$Prior.passed) { throw 'Polish baseline is not verified' }
    if((Get-PSDrive H).Free -lt 25GB) { throw 'Need 25 GB free before copying' }
    $Protected=Get-Content 'H:\DarkRelicPolishDelivery-20260912\protected-before.json' -Raw | ConvertFrom-Json
    $Protected+=@($Prior.packageHashes)
    foreach($File in $Protected) { if((Get-FileHash $File.path -Algorithm SHA256).Hash -ne $File.sha256) { throw "Protected baseline differs: $($File.path)" } }
    $Protected | ConvertTo-Json -Depth 5 | Set-Content "$Delivery\protected-before.json"
    $State.phase='copy'; Save-State
    & robocopy $Source $Root /E /XD Intermediate Saved IntegrationEvidence .git /R:1 /W:1 /NFL /NDL /NJH /NJS *> "$Delivery\copy.log"
    if($LASTEXITCODE -gt 7) { throw 'Project copy failed' }
    New-Item -ItemType Directory "$Root\IntegrationEvidence" | Out-Null
    Copy-Item "$Delivery\integration\*" "$Root\IntegrationEvidence" -Force
    $State.phase='build'; Save-State
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$Root\IntegrationEvidence\run_realistic_build.ps1" *> "$Delivery\build-launch.log"
    $State.buildExit=$LASTEXITCODE
    if($LASTEXITCODE -ne 0) { throw 'Scene build failed; inspect realistic-build.json' }
    $State.phase='ready-for-visual-review'
} catch { $State.phase='failed'; $State.error=$_.Exception.Message }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); Save-State }
if($State.phase -eq 'failed') { exit 1 }
