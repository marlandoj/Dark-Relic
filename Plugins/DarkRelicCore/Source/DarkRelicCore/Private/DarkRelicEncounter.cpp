#include "DarkRelicEncounter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Engine/SkeletalMesh.h"
#include "UnrealClient.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"

static bool ValidVisuals(const FDarkRelicCharacterVisuals& V)
{
    if (!V.Mesh || !V.Idle || !V.Move || !V.LightAttack || !V.Death) return false;
    for (auto* Sequence : {V.Idle.Get(), V.Move.Get(), V.LightAttack.Get(), V.HeavyAttack.Get(), V.Dodge.Get(), V.Death.Get()})
        if (Sequence && Sequence->GetSkeleton() != V.Mesh->GetSkeleton()) return false;
    return !V.Locomotion || V.Locomotion->GetSkeleton() == V.Mesh->GetSkeleton();
}

static void ApplyVisuals(ACharacter* Character, const FDarkRelicCharacterVisuals& V)
{
    auto* Mesh = Character->GetMesh();
    Mesh->EmptyOverrideMaterials();
    Mesh->SetSkeletalMeshAsset(V.Mesh);
    Mesh->SetRelativeScale3D(FVector(V.MeshScale));
    Mesh->SetRelativeLocation(FVector(0,0,-Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
    Mesh->SetRelativeRotation(FRotator(0,-90,0));
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mesh->PlayAnimation(V.Idle, true);
}

static bool PlayCharacterAction(ACharacter* Character, UAnimSequence* Sequence, float Duration)
{
    if (!Character || !Sequence || !Character->GetMesh()->GetSkeletalMeshAsset() ||
        Sequence->GetSkeleton() != Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()) return false;
    Character->GetMesh()->PlayAnimation(Sequence, false);
    Character->GetMesh()->SetPlayRate(Sequence->GetPlayLength()/FMath::Max(Duration,0.05f));
    return true;
}

static void TickCharacterVisuals(ACharacter* Character, const FDarkRelicCharacterVisuals& V, float& Remaining, float Dt)
{
    Remaining = FMath::Max(0.f, Remaining-Dt);
    if (Remaining > 0 || !V.Mesh) return;
    float Speed = Character->GetVelocity().Size2D();
    UAnimationAsset* Asset = V.Locomotion && Speed > 10 ? static_cast<UAnimationAsset*>(V.Locomotion.Get()) :
        static_cast<UAnimationAsset*>(Speed > 10 ? V.Move.Get() : V.Idle.Get());
    auto* Mesh = Character->GetMesh();
    auto* Instance = Mesh->GetSingleNodeInstance();
    if (!Instance || Instance->GetCurrentAsset() != Asset) Mesh->PlayAnimation(Asset, true);
    Mesh->SetPlayRate(V.Locomotion && Asset == V.Locomotion.Get() ? FMath::Max(1.f,Speed/FMath::Max(1.f,V.LocomotionMaxSpeed)) : 1.f);
    if (V.Locomotion && Asset == V.Locomotion.Get())
        if (auto* Node = Mesh->GetSingleNodeInstance()) Node->SetBlendSpacePosition(V.LocomotionAxes*Speed);
}

void ADarkRelicEncounter::PlayCue(float Frequency, float Duration, float Gain, int32 Texture, const FVector* Position)
{
    if (ActiveSounds.Num() >= 16 || FParse::Param(FCommandLine::Get(),TEXT("nosound"))) return;
    auto* Wave=NewObject<USoundWaveProcedural>();
    Wave->SetSampleRate(22050);
    Wave->NumChannels=1;
    Wave->Duration=Duration;
    TArray<int16> Samples;
    int32 Count=FMath::RoundToInt(22050*Duration);
    Samples.SetNumUninitialized(Count);
    FRandomStream Noise(117 + Texture * 31);
    float Filtered = 0;
    for(int32 I=0;I<Count;++I)
    {
        float T=I/22050.f;
        float Envelope=FMath::Min(1.f,T*200)*FMath::Exp(-T*7/Duration);
        float Signal=FMath::Sin(2*PI*Frequency*T)+0.35f*FMath::Sin(2*PI*Frequency*2.71f*T);
        Filtered = 0.92f*Filtered + 0.08f*Noise.FRandRange(-1.f,1.f);
        if (Texture == 1) Signal = Signal*0.5f + Noise.FRandRange(-1.f,1.f)*0.8f;
        if (Texture == 2) Signal = Filtered*4 + FMath::Sin(2*PI*65*T)*0.3f;
        if (Texture == 3)
        {
            Envelope = FMath::Sin(PI*T/Duration)*0.65f;
            Signal = Filtered*2 + 0.07f*FMath::Sin(2*PI*(1650+120*FMath::Sin(T*9))*T)*FMath::Pow(FMath::Max(0.f,FMath::Sin(T*4)),8.f);
        }
        Samples[I]=static_cast<int16>(FMath::Clamp(Signal*Envelope*Gain,-1.f,1.f)*32767);
    }
    Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()),Samples.Num()*sizeof(int16));
    float Volume = Position && Player ? FMath::Clamp(1.f-FVector::Dist(*Position,Player->GetActorLocation())/1600.f,0.05f,1.f) : 1.f;
    auto* Component = UGameplayStatics::SpawnSound2D(this,Wave,Volume,1,0,nullptr,false,false);
    if (Component) { FDarkRelicTimedSound Sound; Sound.Component=Component; Sound.Remaining=Duration+0.05f; ActiveSounds.Add(Sound); }
}

void ADarkRelicEncounter::EndPlay(const EEndPlayReason::Type Reason)
{
    for (auto& Sound : ActiveSounds) if (IsValid(Sound.Component)) { Sound.Component->Stop(); Sound.Component->DestroyComponent(); }
    ActiveSounds.Empty();
    if (Player) if (auto* Camera=Player->FindComponentByClass<UCameraComponent>()) Camera->ClearAdditiveOffset();
    Super::EndPlay(Reason);
}

void ADarkRelicEncounter::Impact(const FVector& Position, bool Heavy)
{
    if (Impacts.Num() >= 12) Impacts.RemoveAt(0);
    FDarkRelicImpact Spark; Spark.Position=Position; Spark.Heavy=Heavy; Impacts.Add(Spark);
    ++ImpactCount;
    PlayCue(Heavy ? 85.f : 145.f,Heavy ? 0.34f : 0.22f,0.24f,1,&Position);
    if (!ReducedMotion) { ShakeRemaining=0.16f; ShakeStrength=Heavy ? 0.55f : 0.28f; }
}

void ADarkRelicEncounter::FeedbackTick(float Dt)
{
    for (int32 I=ActiveSounds.Num()-1;I>=0;--I)
    {
        ActiveSounds[I].Remaining-=Dt;
        if (ActiveSounds[I].Remaining<=0 || !IsValid(ActiveSounds[I].Component))
        {
            if (IsValid(ActiveSounds[I].Component)) { ActiveSounds[I].Component->Stop(); ActiveSounds[I].Component->DestroyComponent(); }
            ActiveSounds.RemoveAt(I);
        }
    }
    for (int32 I=Impacts.Num()-1;I>=0;--I) { Impacts[I].Remaining-=Dt; if (Impacts[I].Remaining<=0) Impacts.RemoveAt(I); }
    WardPulse=FMath::Max(0.f,WardPulse-Dt);
    CelebrationRemaining=FMath::Max(0.f,CelebrationRemaining-Dt);
    ShakeRemaining=FMath::Max(0.f,ShakeRemaining-Dt);
    if (auto* Camera=Player->FindComponentByClass<UCameraComponent>())
    {
        Camera->ClearAdditiveOffset();
        if (!ReducedMotion && ShakeRemaining>0)
        {
            float Offset=FMath::Sin(ShakeRemaining*140)*ShakeRemaining/0.16f*ShakeStrength;
            Camera->AddAdditiveOffset(FTransform(FRotator(Offset,0,Offset*0.4f)),0);
        }
    }
    const auto S=Run->GetSnapshot();
    const bool Live=S.Phase==EDarkRelicPhase::Running || S.Phase==EDarkRelicPhase::Extracting;
    if (!Live) return;
    FootstepRemaining-=Dt;
    if (Player->GetVelocity().Size2D()>80 && Player->GetCharacterMovement()->IsMovingOnGround() && FootstepRemaining<=0)
    { PlayCue(95,0.12f,0.10f,2); FootstepRemaining=0.34f; }
    AmbienceRemaining-=Dt;
    if (AmbienceRemaining<=0) { PlayCue(70,5,0.12f,3); AmbienceRemaining=4.8f; }
    if (S.Phase==EDarkRelicPhase::Extracting)
    {
        BellRemaining-=Dt;
        if (BellRemaining<=0)
        {
            float Progress=1.f-S.ExtractionRemaining/FMath::Max(0.1f,Run->Tuning.ExtractionSeconds);
            PlayCue(220+Progress*110,0.7f,0.14f+Progress*0.07f);
            BellRemaining=FMath::Lerp(1.8f,0.45f,Progress); WardPulse=0.5f; ++BellCount;
        }
    } else BellRemaining=0;
}

ADarkRelicEncounter::ADarkRelicEncounter()
{
    PrimaryActorTick.bCanEverTick = true;
    Run = CreateDefaultSubobject<UDarkRelicRunComponent>(TEXT("Run"));
}

void ADarkRelicEncounter::BeginPlay()
{
    Super::BeginPlay();
    Smoke = FParse::Param(FCommandLine::Get(), TEXT("DarkRelicSmoke"));
    Capture = FParse::Param(FCommandLine::Get(), TEXT("DarkRelicCapture"));
    if (Smoke)
    {
        SmokeSlot = TEXT("DarkRelicRuntimeSmoke_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Run->SaveSlot = SmokeSlot;
        Run->Tuning.ExtractionSeconds = 1.5f;
    }
    else
    {
        Run->SaveSlot = TEXT("DarkRelicWidowfenBankV1");
        if (UGameplayStatics::DoesSaveGameExist(Run->SaveSlot, 0) && !Run->LoadBank())
            Notify(TEXT("Could not load bank. Existing save preserved."));
    }
    Run->OnRunEnded.AddDynamic(this, &ADarkRelicEncounter::EndRun);
}

bool ADarkRelicEncounter::InitializePlayer()
{
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    Player = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
    if (!Player) return false;
    if (RequireCharacterVisuals && (!ValidVisuals(HeroVisuals) || EnemyVisuals.Num()!=3 ||
        EnemyVisuals.ContainsByPredicate([](const FDarkRelicCharacterVisuals& V){return !ValidVisuals(V);}) ||
        !HeroVisuals.HeavyAttack || !HeroVisuals.Dodge))
    {
        UE_LOG(LogTemp, Error, TEXT("DARK_RELIC_CHARACTER_BINDINGS_INVALID"));
        if (Smoke) { SmokeCheck(TEXT("required character bindings are compatible"),false); FinishSmoke(); }
        return false;
    }
    if (ValidVisuals(HeroVisuals)) ApplyVisuals(Player,HeroVisuals);
    PC->ClientSetHUD(ADarkRelicHUD::StaticClass());
    PC->SetViewTarget(Player);
    PC->SetInputMode(FInputModeGameOnly());
    PC->bShowMouseCursor = false;
    Player->GetCharacterMovement()->MaxWalkSpeed = 440;
    Player->GetCharacterMovement()->bOrientRotationToMovement = true;
    Initialized = true;
    if (!HeroVisuals.Mesh)
    if (auto* Saber=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/WidowfenPrep/Sources/wooden_handle_saber/wooden_handle_saber/StaticMeshes/wooden_handle_saber.wooden_handle_saber")))
    {
        auto* Weapon=NewObject<UStaticMeshComponent>(Player);
        Weapon->SetStaticMesh(Saber);
        Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Weapon->RegisterComponent();
        Weapon->AttachToComponent(Player->GetMesh(),FAttachmentTransformRules::SnapToTargetNotIncludingScale,TEXT("hand_r"));
        Weapon->SetRelativeScale3D(FVector(90.f/FMath::Max(1.f,static_cast<float>(Saber->GetBounds().BoxExtent.GetMax()*2))));
        Weapon->SetRelativeRotation(FRotator(0,90,0));
    }
    Restart();
    UE_LOG(LogTemp, Display, TEXT("DARK_RELIC_PLAYABLE_READY pawn=%s mesh=%s"), *Player->GetName(), *GetNameSafe(Player->GetMesh()->GetSkeletalMeshAsset()));
    return true;
}

void ADarkRelicEncounter::Notify(const FString& Text)
{
    Message = Text;
    MessageRemaining = 3;
}

void ADarkRelicEncounter::ResetEncounter()
{
    for (auto& E : Enemies) if (IsValid(E.Actor)) E.Actor->Destroy();
    for (auto& P : Pickups) if (IsValid(P)) P->Destroy();
    Enemies.Empty();
    Pickups.Empty();
    const FVector Locations[] = {FVector(-450,180,120), FVector(500,450,120), FVector(150,1100,120)};
    for (int32 I = 0; I < 3; ++I)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        auto* Enemy = GetWorld()->SpawnActor<ACharacter>(ACharacter::StaticClass(), Locations[I], FRotator::ZeroRotator, Params);
        if (!Enemy) continue;
        Enemy->GetCapsuleComponent()->InitCapsuleSize(34,88);
        Enemy->GetMesh()->SetSkeletalMeshAsset(Player->GetMesh()->GetSkeletalMeshAsset());
        Enemy->GetMesh()->SetAnimInstanceClass(Player->GetMesh()->GetAnimClass());
        Enemy->GetMesh()->SetRelativeLocation(FVector(0,0,-88));
        Enemy->GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
        Enemy->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (EnemyVisuals.IsValidIndex(I) && ValidVisuals(EnemyVisuals[I])) ApplyVisuals(Enemy,EnemyVisuals[I]);
        Enemy->GetCharacterMovement()->bRunPhysicsWithNoController = true;
        Enemy->GetCharacterMovement()->bOrientRotationToMovement = true;
        Enemy->GetCharacterMovement()->MaxWalkSpeed = I == 1 ? 170 : 230;
        Enemy->GetCharacterMovement()->RotationRate = FRotator(0,400,0);
        if (I == 2) Enemy->SetActorScale3D(FVector(1.25f));
        FDarkRelicEnemy E;
        E.Actor = Enemy; E.Home = Locations[I]; E.Role = I;
        E.Health = E.MaxHealth = I == 2 ? 150 : 60;
        Enemies.Add(E);
    }
    const FVector LootLocations[] = {FVector(-950,50,75),FVector(650,650,75),FVector(-550,1000,75),FVector(-100,800,170)};
    const TCHAR* RelicPath = TEXT("/Game/VisualPrep/Relic/blackbell-reeves-chain-ue5-emissive/StaticMeshes/SM_BlackbellReevesChain.SM_BlackbellReevesChain");
    for (int32 I = 0; I < 4; ++I)
    {
        auto* Pickup = GetWorld()->SpawnActor<AStaticMeshActor>(LootLocations[I], FRotator::ZeroRotator);
        if (!Pickup) continue;
        auto* Mesh = LoadObject<UStaticMesh>(nullptr, I == 3 ? RelicPath : TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        Pickup->SetMobility(EComponentMobility::Movable);
        Pickup->GetStaticMeshComponent()->SetStaticMesh(Mesh);
        Pickup->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        float Scale = I == 3 && Mesh ? 28.f / FMath::Max(1.f,static_cast<float>(Mesh->GetBounds().BoxExtent.GetMax()*2)) : 0.3f;
        Pickup->SetActorScale3D(FVector(Scale));
        Pickups.Add(Pickup);
    }
}

void ADarkRelicEncounter::Restart()
{
    if (!Player || !Run->StartRun()) return;
    AttackRemaining = 0;
    HeroAnimationRemaining = 0;
    CelebrationRemaining=0; BellRemaining=0; WardPulse=0; ShakeRemaining=0;
    Impacts.Empty();
    if (HeroVisuals.Mesh) ApplyVisuals(Player,HeroVisuals);
    Player->TeleportTo(PlayerStart, FRotator(0,90,0), false, true);
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if (auto* PC = Cast<APlayerController>(Player->GetController()))
    {
        PC->SetIgnoreMoveInput(false);
        PC->SetControlRotation(FRotator(-12,90,0));
    }
    ResetEncounter();
    Notify(TEXT("Recover Blackbell. Escape through the northern ward."));
}

void ADarkRelicEncounter::EndRun(bool Escaped)
{
    AttackRemaining = 0;
    if (Escaped)
    {
        CelebrationRemaining=5; WardPulse=1;
        PlayCue(330,1.8f,0.18f); PlayCue(440,2.2f,0.12f); PlayCue(660,2.6f,0.08f);
    }
    if (!Escaped && PlayCharacterAction(Player,HeroVisuals.Death,1.6f)) HeroAnimationRemaining=100000;
    if (Player) Player->GetCharacterMovement()->DisableMovement();
    if (!Run->SaveBank()) Notify(TEXT("Bank save failed. Keep this session open."));
    else Notify(Escaped ? TEXT("Extraction complete. Your spoils are banked.") : TEXT("You fell. Carried loot was lost; your bank is safe."));
    UE_LOG(LogTemp, Display, TEXT("DARK_RELIC_RUN_ENDED escaped=%d credits=%d"), Escaped, Run->GetSnapshot().Credits);
}

void ADarkRelicEncounter::Attack(bool Heavy)
{
    if (!Run->TryAction(Heavy ? EDarkRelicAction::Heavy : EDarkRelicAction::Light)) return;
    AttackRemaining = Heavy ? 0.42f : 0.18f;
    AttackDamage = Heavy ? 55 : 25;
    AttackHeavy = Heavy;
    PlayCue(Heavy ? 130.f : 220.f,0.22f,0.08f);
    if (HeroVisuals.Mesh)
    {
        HeroAnimationRemaining = Heavy ? 0.85f : 0.42f;
        PlayCharacterAction(Player,Heavy ? HeroVisuals.HeavyAttack.Get() : HeroVisuals.LightAttack.Get(),HeroAnimationRemaining);
        return;
    }
    if (Player && Player->GetMesh()->GetAnimInstance())
    {
        auto* Animation=LoadObject<UAnimSequence>(nullptr,Heavy ? TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack") : TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
        if (Animation) Player->GetMesh()->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(Animation,TEXT("DefaultSlot"),0.05f,0.12f);
    }
}

void ADarkRelicEncounter::Heal()
{
    if (!Run->TryAction(EDarkRelicAction::Heal)) Notify(TEXT("Healing unavailable: check charges, health and recovery."));
}

void ADarkRelicEncounter::Dodge()
{
    if (!Run->TryAction(EDarkRelicAction::Dodge) || !Player) return;
    FVector Direction = Player->GetLastMovementInputVector().GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction = Player->GetActorForwardVector();
    Player->LaunchCharacter(Direction * 800 + FVector(0,0,80), true, true);
    if (HeroVisuals.Mesh)
    {
        HeroAnimationRemaining=0.4f;
        PlayCharacterAction(Player,HeroVisuals.Dodge,HeroAnimationRemaining);
        return;
    }
    if (auto* Anim=Player->GetMesh()->GetAnimInstance())
        if (auto* Sequence=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash.MM_Dash")))
            Anim->PlaySlotAnimationAsDynamicMontage(Sequence,TEXT("DefaultSlot"),0.05f,0.1f);
}

void ADarkRelicEncounter::Interact()
{
    if (!Player) return;
    for (int32 I = 0; I < Pickups.Num(); ++I)
    {
        if (!IsValid(Pickups[I]) || FVector::Dist(Player->GetActorLocation(), Pickups[I]->GetActorLocation()) > 210) continue;
        if (I == 3 && Enemies.ContainsByPredicate([](const FDarkRelicEnemy& E){ return E.Role == 2 && E.Health > 0; }))
        { Notify(TEXT("The Bellkeeper binds the relic. Defeat the elite first.")); return; }
        if (Run->CollectLoot(static_cast<EDarkRelicItem>(I), 1, I+1))
        { Pickups[I]->Destroy(); PlayCue(I==3?440.f:880.f,0.45f,0.10f); Notify(I == 3 ? TEXT("Blackbell recovered. Reach the northern ward.") : TEXT("Spoils collected. Extract to keep them.")); }
        return;
    }
    if (Run->GetSnapshot().InZone)
    {
        if (Run->GetSnapshot().Carried.Num() < 4 || Run->GetSnapshot().Carried[3] == 0)
            Notify(TEXT("Recover Blackbell before extracting."));
        else if (Run->BeginExtraction()) Notify(TEXT("Hold the ward. Stay inside and survive the countdown."));
    }
}

void ADarkRelicEncounter::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!Initialized && !InitializePlayer()) return;
    if (!IsValid(Player)) return;
    FeedbackTick(Dt);
    MessageRemaining = FMath::Max(0.f, MessageRemaining-Dt);
    HitFlash = FMath::Max(0.f, HitFlash-Dt);
    auto* PC = Cast<APlayerController>(Player->GetController());
    const auto State = Run->GetSnapshot();
    const bool Live = State.Phase == EDarkRelicPhase::Running || State.Phase == EDarkRelicPhase::Extracting;
    if (HeroVisuals.Mesh) TickCharacterVisuals(Player,HeroVisuals,HeroAnimationRemaining,Dt);
    for (auto& E : Enemies)
        if (IsValid(E.Actor) && E.Health>0 && EnemyVisuals.IsValidIndex(E.Role))
            TickCharacterVisuals(E.Actor,EnemyVisuals[E.Role],E.AnimationRemaining,Dt);
    if(Capture)
    {
        float Previous=CaptureElapsed;
        CaptureElapsed+=Dt;
        if(Previous<5 && CaptureElapsed>=5) FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("IntegrationEvidence/playable-frame.png"),true,false);
        if(CaptureElapsed>8) FPlatformMisc::RequestExit(false);
    }
    if (PC && !Smoke)
    {
        if (PC->WasInputKeyJustPressed(EKeys::Escape)) PC->ConsoleCommand(TEXT("quit"));
        if (PC->WasInputKeyJustPressed(EKeys::M)) { ReducedMotion=!ReducedMotion; Notify(ReducedMotion ? TEXT("Camera shake off") : TEXT("Camera shake on")); }
        if (Live)
        {
            if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton)) Attack(false);
            if (PC->WasInputKeyJustPressed(EKeys::RightMouseButton)) Attack(true);
            if (PC->WasInputKeyJustPressed(EKeys::LeftShift)) Dodge();
            if (PC->WasInputKeyJustPressed(EKeys::Q)) Heal();
            if (PC->WasInputKeyJustPressed(EKeys::E)) Interact();
        }
        else
        {
            if (PC->WasInputKeyJustPressed(EKeys::U))
            {
                if (Run->BuyUpgrade()) { Run->SaveBank(); Notify(TEXT("Warden resolve upgraded. Next run gains maximum health.")); }
                else Notify(TEXT("Upgrade requires banked credits or is already owned."));
            }
            if (PC->WasInputKeyJustPressed(EKeys::R)) Restart();
        }
    }
    if (Live)
    {
        const bool InZone = FVector::Dist2D(Player->GetActorLocation(),ExtractionCenter) < 230;
        if (State.InZone != InZone) Run->SetInExtractionZone(InZone);
        if (Player->GetActorLocation().Z < -600) Run->ReceiveDamage(10000);
        if (AttackRemaining > 0)
        {
            AttackRemaining -= Dt;
            if (AttackRemaining <= 0)
                for (auto& E : Enemies)
                {
                    if (!IsValid(E.Actor) || E.Health <= 0) continue;
                    FVector To = E.Actor->GetActorLocation()-Player->GetActorLocation();
                    if (To.Size() > (AttackHeavy ? 240 : 200) || FVector::DotProduct(To.GetSafeNormal2D(),Player->GetActorForwardVector()) < 0.15f) continue;
                    FHitResult Hit; FCollisionQueryParams Query; Query.AddIgnoredActor(Player);
                    bool Blocked = GetWorld()->LineTraceSingleByChannel(Hit,Player->GetActorLocation(),E.Actor->GetActorLocation(),ECC_Visibility,Query);
                    if (Blocked && Hit.GetActor() != E.Actor) continue;
                    E.Health -= AttackDamage;
                    Impact(E.Actor->GetActorLocation()+FVector(0,0,35),AttackHeavy);
                    if (E.BellAttack.remaining<=0) { E.Windup = 0; E.Cooldown = 0.65f; }
                    if (E.Health <= 0)
                    {
                        E.Actor->GetCharacterMovement()->DisableMovement();
                        E.BellAttack.cancel(); E.Windup=0;
                        E.Actor->SetActorEnableCollision(false);
                        if (!EnemyVisuals.IsValidIndex(E.Role) || !PlayCharacterAction(E.Actor,EnemyVisuals[E.Role].Death,1.3f)) E.Actor->SetActorHiddenInGame(true);
                        Notify(E.Role == 2 ? TEXT("Bellkeeper defeated. Blackbell is unbound.") : TEXT("Enemy defeated."));
                    }
                    else Notify(FString::Printf(TEXT("%s hit: %.0f"), AttackHeavy ? TEXT("Heavy") : TEXT("Light"),AttackDamage));
                }
        }
        for (auto& E : Enemies)
        {
            if (!IsValid(E.Actor) || E.Health <= 0) continue;
            const auto Current=Run->GetSnapshot();
            if (Current.Phase!=EDarkRelicPhase::Running && Current.Phase!=EDarkRelicPhase::Extracting) break;
            E.Cooldown -= Dt;
            FVector To = Player->GetActorLocation()-E.Actor->GetActorLocation();
            float Distance = To.Size2D();
            float Range = E.Role == 1 ? 650.f : 150.f;
            if (E.Role==2)
            {
                if (E.BellAttack.update_health(E.Health,E.MaxHealth))
                {
                    Notify(TEXT("BELLKEEPER ENRAGED. Watch the ground and clear the ring."));
                    PlayCue(70,0.9f,0.23f,1);
                    E.Actor->GetCharacterMovement()->MaxWalkSpeed=290;
                }
                const bool WasArea=E.BellAttack.remaining>0;
                if (E.BellAttack.tick(Dt))
                {
                    FHitResult Hit; FCollisionQueryParams Query; Query.AddIgnoredActor(E.Actor);
                    bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,E.AreaCenter,Player->GetActorLocation(),ECC_Visibility,Query);
                    FVector Delta=Player->GetActorLocation()-E.AreaCenter;
                    if (E.BellAttack.hits(Delta.Size2D(),Delta.Z,!Blocked || Hit.GetActor()==Player) && Run->ReceiveDamage(E.BellAttack.damage())) HitFlash=0.25f;
                    Impact(E.AreaCenter,true); ++AreaAttackCount;
                    E.Cooldown=E.BellAttack.enraged ? 0.9f : 1.4f;
                }
                if (WasArea) continue;
                if (Distance<560 && E.Windup<=0 && E.Cooldown<=0 && E.BellAttack.begin())
                {
                    E.AreaCenter=E.Actor->GetActorLocation();
                    E.Actor->GetCharacterMovement()->StopMovementImmediately();
                    E.Actor->SetActorRotation(To.Rotation());
                    E.AnimationRemaining=static_cast<float>(E.BellAttack.duration)+0.25f;
                    if (EnemyVisuals.IsValidIndex(E.Role)) PlayCharacterAction(E.Actor,EnemyVisuals[E.Role].HeavyAttack ? EnemyVisuals[E.Role].HeavyAttack.Get() : EnemyVisuals[E.Role].LightAttack.Get(),E.AnimationRemaining);
                    PlayCue(110,0.6f,0.18f,0,&E.AreaCenter);
                    Notify(TEXT("BELLKEEPER: AREA STRIKE. Leave the marked ring."));
                    continue;
                }
            }
            if (E.Windup > 0)
            {
                E.Windup -= Dt;
                if (E.Windup <= 0)
                {
                    FHitResult Hit; FCollisionQueryParams Query; Query.AddIgnoredActor(E.Actor);
                    bool Blocked = GetWorld()->LineTraceSingleByChannel(Hit,E.Actor->GetActorLocation(),Player->GetActorLocation(),ECC_Visibility,Query);
                    if (Distance < Range+30 && (!Blocked || Hit.GetActor() == Player) && Run->ReceiveDamage(E.Role == 2 ? 30 : 14))
                    { HitFlash = 0.25f; Impact(Player->GetActorLocation(),E.Role==2); }
                    E.Cooldown = E.Role == 2 ? (E.BellAttack.enraged ? 1.f : 1.4f) : 1.8f;
                }
            }
            else if (Distance < 1000)
            {
                if (Distance > Range) E.Actor->AddMovementInput(To.GetSafeNormal2D(),1,true);
                else if (E.Cooldown <= 0)
                {
                    E.Windup = E.Role == 2 ? (E.BellAttack.enraged ? 0.9f : 1.1f) : 0.8f;
                    E.Actor->SetActorRotation(To.Rotation());
                    E.Actor->GetCharacterMovement()->StopMovementImmediately();
                    FVector CuePosition=E.Actor->GetActorLocation();
                    PlayCue(E.Role==0 ? 180.f : E.Role==1 ? 620.f : 90.f,0.35f,0.12f,E.Role==0 ? 1 : 0,&CuePosition);
                    if (EnemyVisuals.IsValidIndex(E.Role))
                    {
                        E.AnimationRemaining=E.Windup+0.25f;
                        PlayCharacterAction(E.Actor,EnemyVisuals[E.Role].LightAttack,E.AnimationRemaining);
                    }
                }
            }
        }
    }
    if (Smoke) SmokeTick(Dt);
}

void ADarkRelicEncounter::SmokeCheck(const FString& Name, bool Passed)
{
    ++SmokeChecks;
    SmokeFailed |= !Passed;
    UE_LOG(LogTemp, Display, TEXT("DARK_RELIC_RUNTIME_CHECK %s %s"), Passed ? TEXT("PASS") : TEXT("FAIL"), *Name);
}

void ADarkRelicEncounter::FinishSmoke()
{
    SmokeCheck(TEXT("isolated save cleanup"), UGameplayStatics::DeleteGameInSlot(SmokeSlot,0));
    FString Result = FString::Printf(TEXT("{\"complete\":true,\"passed\":%s,\"checks\":%d,\"scope\":\"actual runtime actor, ticking, extraction and non-empty persistence\"}"), SmokeFailed ? TEXT("false") : TEXT("true"),SmokeChecks);
    FFileHelper::SaveStringToFile(Result,*(FPaths::ProjectDir()/TEXT("IntegrationEvidence/runtime-smoke.json")));
    FPlatformMisc::RequestExitWithStatus(false,SmokeFailed ? 1 : 0);
}

void ADarkRelicEncounter::SmokeTick(float Dt)
{
    SmokeElapsed += Dt;
    auto S = Run->GetSnapshot();
    if (SmokeElapsed > 40) { SmokeCheck(TEXT("runtime completed within deadline"),false); FinishSmoke(); return; }
    if (SmokeStage == 0 && SmokeElapsed > 1)
    {
        SmokeCheck(TEXT("real player and HUD"),Player && UGameplayStatics::GetPlayerController(this,0)->GetHUD()->IsA<ADarkRelicHUD>());
        SmokeCheck(TEXT("three live enemies and four pickups"),Enemies.Num()==3 && Pickups.Num()==4);
        if (RequireCharacterVisuals)
        {
            SmokeCheck(TEXT("Greystone mesh and live animation instance"),Player->GetMesh()->GetSkeletalMeshAsset()==HeroVisuals.Mesh && Player->GetMesh()->GetSingleNodeInstance()!=nullptr);
            SmokeCheck(TEXT("stationary hero plays idle instead of clamped jogging blendspace"),Player->GetMesh()->GetSingleNodeInstance()->GetCurrentAsset()==HeroVisuals.Idle);
            SmokeCheck(TEXT("hero heavy and dodge use hero skeleton"),ValidVisuals(HeroVisuals) && HeroVisuals.HeavyAttack && HeroVisuals.Dodge);
            for(int32 I=0;I<Enemies.Num();++I)
                SmokeCheck(FString::Printf(TEXT("enemy role %d owns compatible mesh and animation"),I),EnemyVisuals.IsValidIndex(I) && ValidVisuals(EnemyVisuals[I]) && Enemies[I].Actor->GetMesh()->GetSkeletalMeshAsset()==EnemyVisuals[I].Mesh && EnemyVisuals[I].Mesh!=HeroVisuals.Mesh);
            SmokeCheck(TEXT("three visually distinct enemy roles"),EnemyVisuals.Num()==3 && EnemyVisuals[0].Mesh!=EnemyVisuals[1].Mesh && EnemyVisuals[1].Mesh!=EnemyVisuals[2].Mesh && EnemyVisuals[0].Mesh!=EnemyVisuals[2].Mesh);
        }
        for (auto& E : Enemies) { E.Cooldown = 100; E.Windup=0; E.BellAttack.cooldown=100; }
        Player->TeleportTo(FVector(-950,50,110),FRotator::ZeroRotator,false,true); Interact();
        SmokeCheck(TEXT("spatial pickup reaches production rules"),Run->GetSnapshot().Carried[0]==1);
        Run->ReceiveDamage(30); Heal(); SmokeStage=1;
    }
    else if (SmokeStage == 1 && S.Action == EDarkRelicAction::None)
    {
        SmokeCheck(TEXT("world tick completes healing"),S.Health==S.MaxHealth && S.Heals==1);
        Dodge(); SmokeCheck(TEXT("dodge blocks live damage"),!Run->ReceiveDamage(25));
        SmokeStage=2;
    }
    else if (SmokeStage == 2 && S.Action == EDarkRelicAction::None)
    {
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->TeleportTo(FVector(-300,-200,110),FRotator::ZeroRotator,false,true);
        Enemies[0].Actor->TeleportTo(FVector(-160,-200,110),FRotator(0,180,0),false,true);
        Attack(false); SmokeStage=10;
        if(RequireCharacterVisuals) SmokeCheck(TEXT("light attack plays Greystone sequence"),Player->GetMesh()->GetSingleNodeInstance()->GetCurrentAsset()==HeroVisuals.LightAttack);
    }
    else if(SmokeStage==10 && S.Action==EDarkRelicAction::None)
    {
        SmokeCheck(TEXT("light attack contacts enemy in world"),Enemies[0].Health==35);
        Attack(true); SmokeStage=11;
        if(RequireCharacterVisuals) SmokeCheck(TEXT("heavy attack plays Greystone sequence"),Player->GetMesh()->GetSingleNodeInstance()->GetCurrentAsset()==HeroVisuals.HeavyAttack);
    }
    else if(SmokeStage==11 && S.Action==EDarkRelicAction::None)
    {
        SmokeCheck(TEXT("heavy attack kills and disables enemy"),Enemies[0].Health<=0 && !Enemies[0].Actor->GetActorEnableCollision());
        Enemies[1].Actor->TeleportTo(Player->GetActorLocation()+FVector(100,0,0),FRotator::ZeroRotator,false,true);
        Enemies[1].Cooldown=0; Enemies[1].Windup=0; SmokeStage=13;
    }
    else if(SmokeStage==13 && S.Health<S.MaxHealth)
    {
        SmokeCheck(TEXT("enemy telegraph resolves into real damage"),S.Health==S.MaxHealth-14);
        Enemies[1].Cooldown=100; Enemies[1].Windup=0;
        Player->TeleportTo(FVector(-100,700,120),FRotator::ZeroRotator,false,true); Interact();
        SmokeCheck(TEXT("elite prevents early relic pickup"),Run->GetSnapshot().Carried[3]==0);
        auto& Boss=Enemies[2]; Boss.Health=74; Boss.Cooldown=0; Boss.Windup=0; Boss.BellAttack.cooldown=0;
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->TeleportTo(FVector(-300,-200,110),FRotator::ZeroRotator,false,true);
        Boss.Actor->TeleportTo(FVector(-160,-200,110),FRotator::ZeroRotator,false,true);
        SmokeAreaHealth=Run->GetSnapshot().Health;
        SmokeStage=14;
    }
    else if (SmokeStage==14 && Enemies[2].BellAttack.remaining>0)
    {
        SmokeCheck(TEXT("boss enrage below half health has readable windup"),Enemies[2].BellAttack.enraged && Enemies[2].BellAttack.remaining>1 && S.Health==SmokeAreaHealth);
        Player->TeleportTo(FVector(-700,-200,110),FRotator::ZeroRotator,false,true);
        SmokeStage=15;
    }
    else if (SmokeStage==15 && AreaAttackCount==1)
    {
        SmokeCheck(TEXT("leaving boss area prevents damage"),S.Health==SmokeAreaHealth);
        Player->TeleportTo(FVector(-300,-200,110),FRotator::ZeroRotator,false,true);
        Enemies[2].BellAttack.cooldown=0; Enemies[2].Cooldown=0;
        SmokeStage=16;
    }
    else if (SmokeStage==16 && Enemies[2].BellAttack.remaining>0 && Enemies[2].BellAttack.remaining<0.15)
    {
        Run->TryAction(EDarkRelicAction::Dodge);
        SmokeStage=17;
    }
    else if (SmokeStage==17 && AreaAttackCount==2 && S.Action==EDarkRelicAction::None)
    {
        SmokeCheck(TEXT("dodge invulnerability blocks area damage"),S.Health==SmokeAreaHealth);
        Enemies[2].BellAttack.cooldown=0; Enemies[2].Cooldown=0;
        SmokeStage=18;
    }
    else if (SmokeStage==18 && AreaAttackCount==3)
    {
        SmokeCheck(TEXT("standing in enraged boss area deals one hit"),Run->GetSnapshot().Health==SmokeAreaHealth-34);
        SmokeCheck(TEXT("landed attacks emit impact feedback"),ImpactCount>=5);
        SmokeStage=12;
    }
    else if (SmokeStage == 12)
    {
        for (auto& E : Enemies) { E.Health=0; E.Actor->SetActorHiddenInGame(true); E.Actor->SetActorEnableCollision(false); }
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->TeleportTo(FVector(-100,700,120),FRotator::ZeroRotator,false,true); Interact();
        SmokeCheck(TEXT("relic pickup unlocked after elite"),Run->GetSnapshot().Carried[3]==1);
        Player->TeleportTo(ExtractionCenter,FRotator::ZeroRotator,false,true); SmokeStage=3;
    }
    else if (SmokeStage == 3 && S.InZone) { Interact(); SmokeCheck(TEXT("spatial extraction starts"),Run->GetSnapshot().Phase==EDarkRelicPhase::Extracting); SmokeStage=4; }
    else if (SmokeStage == 4 && S.Phase == EDarkRelicPhase::Escaped)
    {
        SmokeCheck(TEXT("world tick completes extraction"),S.Credits>0 && S.Banked[3]==1);
        SmokeCheck(TEXT("extraction bells and escape celebration triggered"),BellCount>0 && CelebrationRemaining>0);
        auto* Fresh=NewObject<UDarkRelicRunComponent>(this); Fresh->SaveSlot=SmokeSlot;
        SmokeCheck(TEXT("fresh component reloads non-empty bank"),Fresh->LoadBank() && Fresh->GetSnapshot().Credits==S.Credits && Fresh->GetSnapshot().Banked[3]==1);
        SmokeCheck(TEXT("upgrade purchased from earned bank"),Run->BuyUpgrade());
        SmokeCheck(TEXT("upgraded bank saves"),Run->SaveBank());
        Restart(); Run->CollectLoot(EDarkRelicItem::Salt,1,99); Run->ReceiveDamage(10000);
        SmokeCheck(TEXT("death loses carried loot and keeps bank"),Run->GetSnapshot().Phase==EDarkRelicPhase::Dead && Run->GetSnapshot().Carried[2]==0 && Run->GetSnapshot().Banked[3]==1);
        Restart(); SmokeCheck(TEXT("restart restores upgraded health"),Run->GetSnapshot().Health==Run->GetSnapshot().MaxHealth && Run->GetSnapshot().Upgrade==1);
        FinishSmoke();
    }
}

void ADarkRelicHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;
    ADarkRelicEncounter* Game=nullptr;
    for (TActorIterator<ADarkRelicEncounter> It(GetWorld());It;++It) { Game=*It; break; }
    if (!Game || !Game->Player) return;
    const auto S=Game->Run->GetSnapshot();
    const float Scale=FMath::Clamp(Canvas->ClipY/1080.f,0.65f,2.f);
    const FLinearColor Ink(0.025f,0.035f,0.033f,0.88f), Text(0.9f,0.9f,0.82f), Gold(0.83f,0.65f,0.32f), Red(0.7f,0.17f,0.13f), Green(0.35f,0.65f,0.48f);
    auto Label=[&](const FString& T,float X,float Y,float Size,FLinearColor C){ DrawText(T,C,X*Scale,Y*Scale,GEngine->GetMediumFont(),Size*Scale*1.7f,false); };
    auto Bar=[&](float X,float Y,float W,float H,float V,FLinearColor C){ DrawRect(Ink,X*Scale,Y*Scale,W*Scale,H*Scale); DrawRect(C,(X+2)*Scale,(Y+2)*Scale,(W-4)*Scale*FMath::Clamp(V,0.f,1.f),(H-4)*Scale); };
    auto Ring=[&](const FVector& Center,float Radius,FLinearColor Color,float Width)
    {
        for (int32 I=0;I<64;++I)
        {
            float A=I*2*PI/64, B=(I+1)*2*PI/64;
            FVector P=Project(Center+FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,0));
            FVector Q=Project(Center+FVector(FMath::Cos(B)*Radius,FMath::Sin(B)*Radius,0));
            if (P.Z>0 && Q.Z>0) DrawLine(P.X,P.Y,Q.X,Q.Y,Color,Width*Scale);
        }
    };
    float WardGlow=Game->WardPulse*0.7f+(S.Phase==EDarkRelicPhase::Extracting ? 0.65f : 0.25f);
    Ring(Game->ExtractionCenter-FVector(0,0,75),230,FLinearColor(0.9f,0.72f,0.35f,WardGlow),3);
    if (S.Phase==EDarkRelicPhase::Extracting || Game->CelebrationRemaining>0)
    {
        Ring(Game->ExtractionCenter-FVector(0,0,65),205+Game->WardPulse*40,FLinearColor(1,0.82f,0.5f,WardGlow),5);
        for (int32 I=0;I<8;++I)
        {
            float A=I*PI/4;
            FVector Base=Game->ExtractionCenter+FVector(FMath::Cos(A)*210,FMath::Sin(A)*210,-70);
            FVector P=Project(Base), Q=Project(Base+FVector(0,0,100+WardGlow*120));
            if(P.Z>0 && Q.Z>0) DrawLine(P.X,P.Y,Q.X,Q.Y,FLinearColor(0.9f,0.72f,0.35f,WardGlow*0.5f),2*Scale);
        }
    }
    for (const auto& Spark:Game->Impacts)
    {
        FVector P=Project(Spark.Position);
        if (P.Z<=0) continue;
        float Age=1-Spark.Remaining/0.3f;
        for(int32 I=0;I<9;++I)
        {
            float A=I*2*PI/9, R=(Spark.Heavy ? 65.f : 40.f)*Age*Scale;
            DrawLine(P.X+FMath::Cos(A)*R*0.4f,P.Y+FMath::Sin(A)*R*0.4f,P.X+FMath::Cos(A)*R,P.Y+FMath::Sin(A)*R,FLinearColor(1,0.72f,0.3f,1-Age),2*Scale);
        }
    }
    Label(TEXT("DARK RELIC  /  WIDOWFEN"),40,30,1.2f,Text);
    Label(TEXT("Health"),40,76,0.9f,Text); Bar(40,100,260,16,S.Health/S.MaxHealth,Red);
    Label(TEXT("Stamina"),40,123,0.9f,Text); Bar(40,147,260,12,S.Stamina/S.MaxStamina,Green);
    Label(FString::Printf(TEXT("Heals %d   |   Bank %d   |   Resolve %d/1"),S.Heals,S.Credits,S.Upgrade),40,176,0.8f,Text);
    Label(S.Carried.Num()==4 && S.Carried[3]>0 ? TEXT("REACH THE NORTHERN WARD") : TEXT("DEFEAT THE BELLKEEPER. RECOVER BLACKBELL."),40,220,0.9f,Gold);
    if (S.Carried.Num()==4) Label(FString::Printf(TEXT("Iron %d   Tallow %d   Salt %d   Blackbell %d"),S.Carried[0],S.Carried[1],S.Carried[2],S.Carried[3]),40,253,0.8f,Text);
    float Bottom=Canvas->ClipY/Scale;
    DrawRect(Ink,24*Scale,(Bottom-78)*Scale,Canvas->ClipX-48*Scale,54*Scale);
    Label(TEXT("WASD Move  |  Mouse Look  |  LMB Light  |  RMB Heavy  |  Shift Dodge  |  Q Heal  |  E Interact"),40,Bottom-61,0.82f,Text);
    if (Game->MessageRemaining>0) Label(Game->Message,40,Bottom-115,1.f,Gold);
    if (S.Phase==EDarkRelicPhase::Extracting)
    { Label(FString::Printf(TEXT("HOLD THE WARD  %.1fs"),S.ExtractionRemaining),40,302,1.2f,Gold); Bar(40,340,300,14,1-S.ExtractionRemaining/Game->Run->Tuning.ExtractionSeconds,Gold); }
    else if (S.InZone) Label(TEXT("E  Begin extraction"),40,300,1.1f,Gold);
    for (const auto& E : Game->Enemies)
    {
        if (!IsValid(E.Actor)||E.Health<=0) continue;
        bool Area=E.Role==2 && E.BellAttack.remaining>0;
        if (Area)
        {
            FVector Ground=E.AreaCenter-FVector(0,0,75);
            Ring(Ground,static_cast<float>(dark_relic::BellkeeperAttack::Radius),Gold,4);
            Ring(Ground,static_cast<float>(dark_relic::BellkeeperAttack::Radius*(1-E.BellAttack.remaining/E.BellAttack.duration)),Red,3);
        }
        FVector P=Project(E.Actor->GetActorLocation()+FVector(0,0,120));
        if(P.Z<=0) continue;
        float X=P.X/Scale-65,Y=P.Y/Scale;
        Label(Area ? TEXT("!  LEAVE THE RING") : E.Windup>0 ? TEXT("!  DODGE") : E.Role==2 ? (E.BellAttack.enraged ? TEXT("BELLKEEPER: ENRAGED") : TEXT("BELLKEEPER")) : E.Role==1 ? TEXT("HEXBOUND") : TEXT("DREG"),X,Y,0.75f,(Area||E.Windup>0)?Gold:Text);
        Bar(X,Y+22,130,8,E.Health/E.MaxHealth,Red);
        if (Area) Bar(X,Y+33,130,5,static_cast<float>(1-E.BellAttack.remaining/E.BellAttack.duration),Gold);
    }
    if (S.Phase==EDarkRelicPhase::Dead || S.Phase==EDarkRelicPhase::Escaped)
    {
        float X=Canvas->ClipX/Scale*0.5f-270, Y=Bottom*0.40f;
        DrawRect(Ink,X*Scale,(Y-24)*Scale,540*Scale,175*Scale);
        Label(S.Phase==EDarkRelicPhase::Escaped?TEXT("BLACKBELL RECLAIMED"):TEXT("THE FEN CLAIMED YOU"),X+28,Y,1.4f,Gold);
        Label(FString::Printf(TEXT("Banked credits: %d"),S.Credits),X+28,Y+48,1.f,Text);
        Label(TEXT("R  New run     U  Upgrade resolve     Esc  Quit"),X+28,Y+94,0.85f,Text);
    }
    if(Game->HitFlash>0) DrawRect(FLinearColor(0.5f,0.05f,0.02f,Game->HitFlash),0,0,Canvas->ClipX,Canvas->ClipY);
}
