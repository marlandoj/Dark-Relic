$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$Root='H:\DarkRelicPolish-20260912'
$Evidence="$Root\IntegrationEvidence"
$Package='H:\DarkRelicPolishPackage\Windows\DarkRelicSmoke'
$Delivery='H:\DarkRelicPolishDelivery-20260912'
$State=@{complete=$false;passed=$false;phase='preflight';started=[DateTime]::UtcNow.ToString('o');pid=$PID}
$Receipt="$Evidence\polish-release.json"
function Save-State { $State | ConvertTo-Json -Depth 6 | Set-Content $Receipt }
Save-State
try {
    $Build=Get-Content "$Evidence\polish-build.json" -Raw | ConvertFrom-Json
    if(!$Build.complete -or $Build.phase -ne 'passed') { throw 'Aura build is not verified' }
    if(Get-Process UnrealEditor,DarkRelicSmoke,UnrealBuildTool -ErrorAction SilentlyContinue) { throw 'Another runtime remains active' }
    foreach($Height in @(720,1080)) {
        $Width=if($Height -eq 720){1280}else{1920}
        $State.phase="presentation-$Height"; Save-State
        $CaptureStarted=[DateTime]::Now
        $P=Start-Process "$Package\Binaries\Win64\DarkRelicSmoke.exe" -ArgumentList @('-DarkRelicPolishCapture','-unattended','-dx11','-windowed',"-ResX=$Width","-ResY=$Height","-abslog=$Evidence\polish-capture-$Height.log") -PassThru
        $State.childPid=$P.Id; Save-State
        if(!$P.WaitForExit(120000) -or $P.ExitCode -ne 0) { throw 'Presentation capture failed; inspect child' }
        $State["capture${Height}Exit"]=$P.ExitCode
        foreach($Name in @('neutral','warnings','ward-locked','ward-ready','extracting','victory','upgrade','death')) {
            $Frame="$Package\IntegrationEvidence\polish-$Name.png"
            if(!(Test-Path $Frame) -or (Get-Item $Frame).Length -lt 1000 -or (Get-Item $Frame).LastWriteTime -lt $CaptureStarted) { throw "Missing or stale $Name capture" }
            Copy-Item $Frame "$Evidence\polish-$Height-$Name.png"
        }
    }
    $State.phase='ordinary-play'; Save-State
    $P=Start-Process "$Package\Binaries\Win64\DarkRelicSmoke.exe" -ArgumentList @('-dx11','-windowed','-ResX=1920','-ResY=1080',"-abslog=$Evidence\aura-ordinary.log") -PassThru
    $State.childPid=$P.Id; Save-State
    $Deadline=[DateTime]::UtcNow.AddSeconds(60)
    $Ready=$false
    while([DateTime]::UtcNow -lt $Deadline -and !$P.HasExited) {
        if(Test-Path "$Evidence\aura-ordinary.log") { $Ready=[bool](Select-String "$Evidence\aura-ordinary.log" -Pattern 'DARK_RELIC_PLAYABLE_READY' -Quiet) }
        if($Ready) { break }
        Start-Sleep -Milliseconds 500
        $P.Refresh()
    }
    if(!$Ready -or $P.HasExited) { throw 'Ordinary game did not become ready' }
    $Shell=New-Object -ComObject WScript.Shell
    if(!$Shell.AppActivate($P.Id)) { throw 'Could not focus candidate for Fury input' }
    Start-Sleep -Milliseconds 400
    $Shell.SendKeys('r')
    Start-Sleep -Seconds 8
    if(!$Shell.AppActivate($P.Id)) { throw 'Could not focus candidate for Escape' }
    Start-Sleep -Milliseconds 300
    $Shell.SendKeys('{ESC}')
    if(!$P.WaitForExit(20000) -or $P.ExitCode -ne 0) { throw 'Ordinary Escape exit failed; inspect child' }
    $State.ordinaryExit=$P.ExitCode
    $State.phase='verify-hashes'; Save-State
    $Protected=Get-Content "$Delivery\protected-before.json" -Raw | ConvertFrom-Json
    foreach($File in $Protected) {
        if((Get-FileHash $File.path -Algorithm SHA256).Hash -ne $File.sha256) { throw "Protected package changed: $($File.path)" }
    }
    $State.protectedFiles=$Protected.Count
    $Expected=Get-Content "$Delivery\source-hashes.json" -Raw | ConvertFrom-Json
    foreach($File in $Expected) {
        if((Get-FileHash (Join-Path $Root $File.path) -Algorithm SHA256).Hash -ne $File.sha256) { throw "Source mismatch: $($File.path)" }
    }
    $State.sourceFiles=$Expected.Count
    $State.packageHashes=@(Get-ChildItem 'H:\DarkRelicPolishPackage' -Recurse -File | Where-Object {$_.Extension -in '.exe','.pak','.ucas','.utoc'} | ForEach-Object {@{path=$_.FullName;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
    $Shortcut=Join-Path ([Environment]::GetFolderPath('Desktop')) 'Dark Relic Polish Candidate.lnk'
    $Link=$Shell.CreateShortcut($Shortcut)
    if(Test-Path $Shortcut) {
        if($Link.TargetPath -ne 'H:\DarkRelicPolishPackage\Windows\DarkRelicSmoke.exe' -or $Link.Arguments -ne '-dx11 -windowed -ResX=1920 -ResY=1080') { throw 'Existing candidate shortcut differs; preserve it for inspection' }
    }
    $Link.TargetPath='H:\DarkRelicPolishPackage\Windows\DarkRelicSmoke.exe'
    $Link.Arguments='-dx11 -windowed -ResX=1920 -ResY=1080'
    $Link.WorkingDirectory='H:\DarkRelicPolishPackage\Windows'
    $Link.Save()
    $Check=$Shell.CreateShortcut($Shortcut)
    if($Check.TargetPath -ne $Link.TargetPath -or $Check.Arguments -ne $Link.Arguments) { throw 'Shortcut readback mismatch' }
    $State.shortcut=$Shortcut
    $State.passed=$true; $State.phase='passed'
} catch { $State.phase='failed'; $State.error=$_.Exception.Message }
finally { $State.complete=$true; $State.finished=[DateTime]::UtcNow.ToString('o'); Save-State }
if(!$State.passed) { exit 1 }
