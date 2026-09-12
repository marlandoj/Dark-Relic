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
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Portable/FuryAura.h"

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

void ADarkRelicEncounter::ClearCues()
{
    for (auto& Sound : ActiveSounds)
        if (IsValid(Sound.Component)) { Sound.Component->Stop(); Sound.Component->DestroyComponent(); }
    ActiveSounds.Empty();
}

static int VoicePriority(EDarkRelicVoice Event)
{
    if (Event==EDarkRelicVoice::Death || Event==EDarkRelicVoice::Cheer) return 3;
    if (Event==EDarkRelicVoice::Pain || Event==EDarkRelicVoice::HeavyPain) return 2;
    return 0;
}

bool ADarkRelicEncounter::HexTellVisible(const FDarkRelicEnemy& E) const
{
    const auto S=Run->GetSnapshot();
    if (!IsValid(Player) || !IsValid(E.Actor) || E.Role!=1 || E.Health<=0 || E.Windup<=0 ||
        (S.Phase!=EDarkRelicPhase::Running && S.Phase!=EDarkRelicPhase::Extracting)) return false;
    if (FVector::Dist2D(Player->GetActorLocation(),E.Actor->GetActorLocation())>=680) return false;
    FHitResult Hit; FCollisionQueryParams Query; Query.AddIgnoredActor(E.Actor);
    const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,E.Actor->GetActorLocation(),Player->GetActorLocation(),ECC_Visibility,Query);
    return !Blocked || Hit.GetActor()==Player;
}

void ADarkRelicEncounter::PlayCue(float Frequency, float Duration, float Gain, int32 Texture, const FVector* Position, dark_relic::CuePriority Priority)
{
    if (FParse::Param(FCommandLine::Get(),TEXT("nosound"))) return;
    const int32 Victim=dark_relic::cue_victim(ActiveSounds.Num(),Priority,[&](size_t I){ return ActiveSounds[static_cast<int32>(I)].Priority; });
    if (Victim==-2) { ++CueDropped; return; }
    if (Victim>=0)
    {
        auto* Old=ActiveSounds[Victim].Component.Get();
        if (IsValid(Old)) { Old->Stop(); Old->DestroyComponent(); }
        ActiveSounds.RemoveAt(Victim); ++CueEvicted;
    }
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
    auto* Component=NewObject<UAudioComponent>(this);
    Component->bAutoActivate=false;
    Component->bAutoDestroy=false;
    Component->bIsUISound=Position==nullptr;
    Component->bAllowSpatialization=Position!=nullptr;
    if (Position)
    {
        Component->bOverrideAttenuation=true;
        Component->AttenuationOverrides.bAttenuate=true;
        Component->AttenuationOverrides.bSpatialize=true;
        Component->AttenuationOverrides.AttenuationShapeExtents=FVector(200);
        Component->AttenuationOverrides.FalloffDistance=1400;
    }
    Component->RegisterComponent();
    if (Position) Component->SetWorldLocation(*Position);
    Component->SetSound(Wave);
    Component->Play();
    FDarkRelicTimedSound Sound;
    Sound.Component=Component; Sound.Remaining=Duration+0.05f; Sound.Priority=Priority;
    ActiveSounds.Add(Sound); ++CueAdmitted;
}

void ADarkRelicEncounter::PlayHeroVoice(EDarkRelicVoice Event)
{
    const auto* Binding=HeroVoices.FindByPredicate([Event](const FDarkRelicVoiceBinding& B){ return B.Event==Event && B.Sound; });
    if (!Binding) return;
    if (!dark_relic::voice_may_interrupt(VoicePriority(Event),VoicePriority(LastVoice),VoiceRemaining))
    { ++VoiceDropped; return; }
    const bool Pain=Event==EDarkRelicVoice::Pain || Event==EDarkRelicVoice::HeavyPain;
    if (Pain && PainVoiceCooldown>0) return;
    if (Pain) PainVoiceCooldown=0.18f;
    LastVoice=Event; ++VoiceCount;
    VoiceRemaining=FMath::Clamp(Binding->Sound->GetDuration()+0.1f,0.2f,5.f);
    if (!FParse::Param(FCommandLine::Get(),TEXT("nosound")))
    {
        if (!IsValid(HeroVoiceComponent))
        {
            HeroVoiceComponent=NewObject<UAudioComponent>(this);
            HeroVoiceComponent->bAutoActivate=false;
            HeroVoiceComponent->bAutoDestroy=false;
            HeroVoiceComponent->bIsUISound=true;
            HeroVoiceComponent->bAllowSpatialization=false;
            HeroVoiceComponent->RegisterComponent();
        }
        HeroVoiceComponent->Stop();
        HeroVoiceComponent->SetSound(Binding->Sound);
        HeroVoiceComponent->SetVolumeMultiplier(FMath::Clamp(VoiceVolume,0.f,2.f));
        HeroVoiceComponent->Play();
    }
    UE_LOG(LogTemp,Display,TEXT("DARK_RELIC_VOICE event=%d sound=%s playing=%d"),static_cast<int32>(Event),*Binding->Sound->GetPathName(),IsValid(HeroVoiceComponent) && HeroVoiceComponent->IsPlaying());
}

bool ADarkRelicEncounter::DamagePlayer(float Amount, const FVector& Source, bool Heavy)
{
    if (!IsValid(Player) || !Run->ReceiveDamage(Amount)) return false;
    HitFlash=0.25f;
    Impact(Player->GetActorLocation(),Heavy);
    if (Run->GetSnapshot().Phase==EDarkRelicPhase::Dead) return true;
    RecoilDirection=(Player->GetActorLocation()-Source).GetSafeNormal2D();
    if (RecoilDirection.IsNearlyZero()) RecoilDirection=-Player->GetActorForwardVector().GetSafeNormal2D();
    RecoilRemaining=0.16f;
    ++RecoilCount;
    PlayHeroVoice(Heavy ? EDarkRelicVoice::HeavyPain : EDarkRelicVoice::Pain);
    return true;
}

void ADarkRelicEncounter::TickRecoil(float Dt)
{
    if (!IsValid(Player) || RecoilRemaining<=0 || Dt<=0) return;
    float Step=FMath::Min(Dt,RecoilRemaining);
    RecoilRemaining=FMath::Max(0.f,RecoilRemaining-Step);
    FHitResult Hit;
    Player->AddActorWorldOffset(RecoilDirection*FMath::Clamp(HitRecoilDistance,0.f,100.f)*(Step/0.16f),true,&Hit);
    if (Hit.bBlockingHit) RecoilRemaining=0;
}

void ADarkRelicEncounter::ClearEnemyFeedback(FDarkRelicEnemy& E)
{
    E.RecoilRemaining=0; E.PainCooldown=0; E.VoiceRemaining=0;
    if (IsValid(E.VoiceComponent)) { E.VoiceComponent->Stop(); E.VoiceComponent->DestroyComponent(); }
    E.VoiceComponent=nullptr;
}

void ADarkRelicEncounter::ReactEnemyHit(FDarkRelicEnemy& E, const FVector& Source, bool Heavy)
{
    if (!IsValid(E.Actor)) return;
    if (E.Health>0)
    {
        E.RecoilDirection=(E.Actor->GetActorLocation()-Source).GetSafeNormal2D();
        if (E.RecoilDirection.IsNearlyZero()) E.RecoilDirection=-E.Actor->GetActorForwardVector().GetSafeNormal2D();
        E.RecoilDistance=E.Role==2 ? (Heavy ? 22.f : 12.f) : (Heavy ? 75.f : 45.f);
        E.RecoilRemaining=0.18f;
        E.Actor->GetCharacterMovement()->StopMovementImmediately();
        E.Actor->ConsumeMovementInputVector();
        ++E.RecoilCount;
    }
    else E.RecoilRemaining=0;
    const auto* Binding=EnemyVoices.FindByPredicate([&E](const FDarkRelicEnemyVoiceBinding& B){ return B.Role==E.Role; });
    if (!Binding || Binding->PainSounds.IsEmpty() || E.PainCooldown>0) return;
    const int32 Index=(E.LastPainIndex+1)%Binding->PainSounds.Num();
    auto* Sound=Binding->PainSounds[Index].Get();
    if (!Sound) return;
    E.LastPainIndex=Index; E.PainCooldown=0.22f; ++E.VoiceCount;
    const float Pitch=FMath::Clamp(Binding->Pitch+(Index%3-1)*0.025f,0.5f,1.3f);
    E.VoiceRemaining=FMath::Clamp(Sound->GetDuration()/Pitch+0.1f,0.2f,5.f);
    if (!FParse::Param(FCommandLine::Get(),TEXT("nosound")))
    {
        if (!IsValid(E.VoiceComponent))
        {
            E.VoiceComponent=NewObject<UAudioComponent>(E.Actor);
            E.VoiceComponent->bAutoActivate=false;
            E.VoiceComponent->bAutoDestroy=false;
            E.VoiceComponent->bAllowSpatialization=true;
            E.VoiceComponent->bOverrideAttenuation=true;
            E.VoiceComponent->AttenuationOverrides.bAttenuate=true;
            E.VoiceComponent->AttenuationOverrides.bSpatialize=true;
            E.VoiceComponent->AttenuationOverrides.AttenuationShapeExtents=FVector(200);
            E.VoiceComponent->AttenuationOverrides.FalloffDistance=1400;
            E.VoiceComponent->RegisterComponent();
            E.VoiceComponent->AttachToComponent(E.Actor->GetRootComponent(),FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        }
        E.VoiceComponent->Stop();
        E.VoiceComponent->SetSound(Sound);
        E.VoiceComponent->SetPitchMultiplier(Pitch);
        E.VoiceComponent->SetVolumeMultiplier(FMath::Clamp(Binding->Volume,0.f,1.f));
        E.VoiceComponent->Play();
    }
    UE_LOG(LogTemp,Display,TEXT("DARK_RELIC_ENEMY_PAIN role=%d variant=%d pitch=%.3f playing=%d"),E.Role,Index,Pitch,IsValid(E.VoiceComponent) && E.VoiceComponent->IsPlaying());
}

void ADarkRelicEncounter::TickEnemyFeedback(FDarkRelicEnemy& E, float Dt)
{
    if (!IsValid(E.Actor) || Dt<=0) return;
    E.PainCooldown=FMath::Max(0.f,E.PainCooldown-Dt);
    E.VoiceRemaining=FMath::Max(0.f,E.VoiceRemaining-Dt);
    if (E.VoiceRemaining<=0 && IsValid(E.VoiceComponent)) E.VoiceComponent->Stop();
    if (E.Health<=0) { E.RecoilRemaining=0; return; }
    if (E.RecoilRemaining<=0) return;
    const float Step=FMath::Min(Dt,E.RecoilRemaining);
    E.RecoilRemaining=FMath::Max(0.f,E.RecoilRemaining-Step);
    E.Actor->GetCharacterMovement()->StopMovementImmediately();
    E.Actor->ConsumeMovementInputVector();
    const FVector Offset=E.RecoilDirection*E.RecoilDistance*(Step/0.18f);
    const FVector End=E.Actor->GetActorLocation()+Offset;
    FCollisionQueryParams Query; Query.AddIgnoredActor(E.Actor); Query.AddIgnoredActor(Player);
    for (const auto& Other : Enemies) if (IsValid(Other.Actor)) Query.AddIgnoredActor(Other.Actor);
    FHitResult Floor;
    const float HalfHeight=E.Actor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (!GetWorld()->LineTraceSingleByChannel(Floor,End,End-FVector(0,0,HalfHeight+65),ECC_Visibility,Query) || Floor.ImpactNormal.Z<0.6f)
    { E.RecoilRemaining=0; return; }
    FHitResult Hit;
    E.Actor->AddActorWorldOffset(Offset,true,&Hit);
    if (Hit.bBlockingHit) E.RecoilRemaining=0;
}

void ADarkRelicEncounter::EndPlay(const EEndPlayReason::Type Reason)
{
    UE_LOG(LogTemp,Display,TEXT("DARK_RELIC_AUDIO admitted=%d dropped=%d evicted=%d voiceDropped=%d"),CueAdmitted,CueDropped,CueEvicted,VoiceDropped);
    for (const auto& Mesh : FuryMeshes) if (IsValid(Mesh)) Mesh->DestroyComponent();
    FuryMeshes.Empty();
    if (IsValid(FuryLight)) FuryLight->DestroyComponent();
    if (IsValid(HeroVoiceComponent)) { HeroVoiceComponent->Stop(); HeroVoiceComponent->DestroyComponent(); }
    for (auto& E : Enemies) ClearEnemyFeedback(E);
    for (auto& Sound : ActiveSounds) if (IsValid(Sound.Component)) { Sound.Component->Stop(); Sound.Component->DestroyComponent(); }
    ActiveSounds.Empty();
    if (IsValid(Player)) if (auto* Camera=Player->FindComponentByClass<UCameraComponent>()) Camera->ClearAdditiveOffset();
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
    for (auto& E : Enemies) TickEnemyFeedback(E,Dt);
    HexImpactRemaining=FMath::Max(0.f,HexImpactRemaining-Dt);
    PainVoiceCooldown=FMath::Max(0.f,PainVoiceCooldown-Dt);
    VoiceRemaining=FMath::Max(0.f,VoiceRemaining-Dt);
    if (VoiceRemaining<=0 && IsValid(HeroVoiceComponent)) HeroVoiceComponent->Stop();
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
    TickRecoil(Dt);
    if (HealingVoicePending && S.Action!=EDarkRelicAction::Heal)
    {
        HealingVoicePending=false;
        if (S.Health>HealingStartHealth) PlayHeroVoice(EDarkRelicVoice::Healed);
    }
    FootstepRemaining-=Dt;
    if (Player->GetVelocity().Size2D()>80 && Player->GetCharacterMovement()->IsMovingOnGround() && FootstepRemaining<=0)
    { PlayCue(95,0.12f,0.10f,2,nullptr,dark_relic::CuePriority::Ambient); FootstepRemaining=0.34f; }
    AmbienceRemaining-=Dt;
    if (AmbienceRemaining<=0) { PlayCue(70,5,0.12f,3,nullptr,dark_relic::CuePriority::Ambient); AmbienceRemaining=4.8f; }
    if (S.Phase==EDarkRelicPhase::Extracting)
    {
        BellRemaining-=Dt;
        if (BellRemaining<=0)
        {
            float Progress=1.f-S.ExtractionRemaining/FMath::Max(0.1f,Run->Tuning.ExtractionSeconds);
            PlayCue(220+Progress*110,0.7f,0.14f+Progress*0.07f,0,nullptr,dark_relic::CuePriority::Warning);
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
    AuraCapture = FParse::Param(FCommandLine::Get(), TEXT("DarkRelicAuraCapture"));
    PolishCapture = FParse::Param(FCommandLine::Get(), TEXT("DarkRelicPolishCapture"));
    if (Smoke || PolishCapture)
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
    if (RequireEnemyVoices)
    {
        bool Valid=EnemyVoices.Num()==3;
        for (int32 EnemyRole=0;EnemyRole<3;++EnemyRole)
        {
            const auto* B=EnemyVoices.FindByPredicate([EnemyRole](const FDarkRelicEnemyVoiceBinding& V){ return V.Role==EnemyRole; });
            Valid &= B && B->PainSounds.Num()>=3;
            if (B) for (const auto& Sound : B->PainSounds) Valid &= Sound && Sound->GetDuration()>0;
        }
        if (!Valid)
        {
            UE_LOG(LogTemp,Error,TEXT("Required enemy pain recordings missing"));
            if (Smoke) { SmokeCheck(TEXT("required cooked enemy voices are present"),false); FinishSmoke(); }
            return false;
        }
    }
    if (RequireHeroVoices)
    {
        bool Valid=true;
        for (int32 I=0;I<=static_cast<int32>(EDarkRelicVoice::Cheer);++I)
            Valid &= HeroVoices.ContainsByPredicate([I](const FDarkRelicVoiceBinding& B){ return static_cast<int32>(B.Event)==I && B.Sound && B.Sound->GetDuration()>0; });
        if (!Valid)
        {
            UE_LOG(LogTemp,Error,TEXT("DARK_RELIC_VOICE_BINDINGS_INVALID"));
            if (Smoke) { SmokeCheck(TEXT("required cooked hero voices are present"),false); FinishSmoke(); }
            return false;
        }
    }
    if (RequireCharacterVisuals && (!ValidVisuals(HeroVisuals) || EnemyVisuals.Num()!=3 ||
        EnemyVisuals.ContainsByPredicate([](const FDarkRelicCharacterVisuals& V){return !ValidVisuals(V);}) ||
        !HeroVisuals.HeavyAttack || !HeroVisuals.Dodge))
    {
        UE_LOG(LogTemp, Error, TEXT("DARK_RELIC_CHARACTER_BINDINGS_INVALID"));
        if (Smoke) { SmokeCheck(TEXT("required character bindings are compatible"),false); FinishSmoke(); }
        return false;
    }
    if (ValidVisuals(HeroVisuals)) ApplyVisuals(Player,HeroVisuals);
    if (RequireFuryVisuals && (!FuryMaterial || !FuryAnimation || !HeroVisuals.Mesh || FuryAnimation->GetSkeleton()!=HeroVisuals.Mesh->GetSkeleton()))
    {
        UE_LOG(LogTemp,Error,TEXT("DARK_RELIC_FURY_BINDINGS_INVALID"));
        if (Smoke) { SmokeCheck(TEXT("required fury bindings are compatible"),false); FinishSmoke(); }
        return false;
    }
    InitializeFury();
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
    for (auto& E : Enemies) { ClearEnemyFeedback(E); if (IsValid(E.Actor)) E.Actor->Destroy(); }
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
    ClearCues();
    FootstepRemaining=0; AmbienceRemaining=0; HexImpactRemaining=0; HitFlash=0;
    RunStartingCredits=Run->GetSnapshot().Credits; RunEarnedCredits=0;
    RecoilRemaining=0; RecoilDirection=FVector::ZeroVector;
    PainVoiceCooldown=0; VoiceRemaining=0; HealingVoicePending=false;
    if (IsValid(HeroVoiceComponent)) HeroVoiceComponent->Stop();
    AttackRemaining = 0;
    HeroAnimationRemaining = 0;
    TickFury();
    AttackBurst=false; AttackFinisher=false; BurstVisualRemaining=0;
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
    for (auto& E : Enemies) { ClearEnemyFeedback(E); E.Windup=0; E.BellAttack.cancel(); }
    ClearCues(); HexImpactRemaining=0;
    RunEarnedCredits=Escaped ? FMath::Max(0,Run->GetSnapshot().Credits-RunStartingCredits) : 0;
    TickFury();
    RecoilRemaining=0; HealingVoicePending=false;
    PlayHeroVoice(Escaped ? EDarkRelicVoice::Cheer : EDarkRelicVoice::Death);
    AttackRemaining = 0;
    BurstVisualRemaining=0;
    if (Escaped)
    {
        CelebrationRemaining=5; WardPulse=1;
        PlayCue(330,1.8f,0.18f,0,nullptr,dark_relic::CuePriority::Terminal); PlayCue(440,2.2f,0.12f,0,nullptr,dark_relic::CuePriority::Terminal); PlayCue(660,2.6f,0.08f,0,nullptr,dark_relic::CuePriority::Terminal);
    }
    if (!Escaped && PlayCharacterAction(Player,HeroVisuals.Death,1.6f)) HeroAnimationRemaining=100000;
    if (Player) Player->GetCharacterMovement()->DisableMovement();
    if (!Run->SaveBank()) Notify(TEXT("Bank save failed. Keep this session open."));
    else Notify(Escaped ? TEXT("Extraction complete. Your spoils are banked.") : TEXT("You fell. Carried loot was lost; your bank is safe."));
    UE_LOG(LogTemp, Display, TEXT("DARK_RELIC_RUN_ENDED escaped=%d credits=%d"), Escaped, Run->GetSnapshot().Credits);
}

void ADarkRelicEncounter::Attack(bool Heavy)
{
    if (!Player || !Run->TryAction(Heavy ? EDarkRelicAction::Heavy : EDarkRelicAction::Light)) return;
    const auto S=Run->GetSnapshot();
    AttackFinisher=S.Finisher; AttackBurst=false;
    Heavy=Heavy || AttackFinisher;
    PlayHeroVoice(Heavy ? EDarkRelicVoice::Heavy : EDarkRelicVoice::Light);
    AttackRemaining = S.ActionRemaining * (Heavy ? 0.47f : 0.36f);
    AttackDamage = S.AttackDamage;
    AttackHeavy = Heavy;
    if (AttackFinisher) Notify(TEXT("SUNDER: combo finisher"));
    PlayCue(Heavy ? 130.f : 220.f,0.22f,0.08f);
    if (HeroVisuals.Mesh)
    {
        HeroAnimationRemaining = S.ActionRemaining * 0.94f;
        PlayCharacterAction(Player,Heavy ? HeroVisuals.HeavyAttack.Get() : HeroVisuals.LightAttack.Get(),HeroAnimationRemaining);
        return;
    }
    if (Player && Player->GetMesh()->GetAnimInstance())
    {
        auto* Animation=LoadObject<UAnimSequence>(nullptr,Heavy ? TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack") : TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
        if (Animation) Player->GetMesh()->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(Animation,TEXT("DefaultSlot"),0.05f,0.12f);
    }
}

void ADarkRelicEncounter::RelicBurst()
{
    if (!Player) return;
    if (!Run->TryAction(EDarkRelicAction::RelicBurst))
    { Notify(TEXT("Relic burst unavailable: wait for recovery/cooldown; requires 35 stamina.")); return; }
    const auto S=Run->GetSnapshot();
    AttackRemaining=S.ActionRemaining*0.5f; AttackDamage=S.AttackDamage;
    AttackHeavy=true; AttackBurst=true; AttackFinisher=false;
    PlayHeroVoice(EDarkRelicVoice::Burst);
    HeroAnimationRemaining=S.ActionRemaining*0.94f;
    PlayCharacterAction(Player,HeroVisuals.HeavyAttack,HeroAnimationRemaining);
    PlayCue(440,0.35f,0.12f);
    Notify(TEXT("RELIC BURST"));
}

void ADarkRelicEncounter::InitializeFury()
{
    if (!FuryMaterial || FuryMeshes.Num()>0) return;
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (!Sphere) return;
    FuryDynamicMaterial=UMaterialInstanceDynamic::Create(FuryMaterial,this);
    for (int32 I=0;I<9;++I)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(Player);
        Mesh->SetStaticMesh(Sphere);
        Mesh->SetMaterial(0,FuryDynamicMaterial);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);
        Mesh->SetVisibility(false);
        Mesh->RegisterComponent();
        Mesh->AttachToComponent(Player->GetRootComponent(),FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        FuryMeshes.Add(Mesh);
    }
    FuryLight=NewObject<UPointLightComponent>(Player);
    FuryLight->SetIntensity(0);
    FuryLight->SetLightColor(FLinearColor(1.f,0.24f,0.045f));
    FuryLight->SetAttenuationRadius(320);
    FuryLight->SetCastShadows(false);
    FuryLight->RegisterComponent();
    FuryLight->AttachToComponent(Player->GetRootComponent(),FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    FuryLight->SetRelativeLocation(FVector(0,0,45));
}

void ADarkRelicEncounter::TickFury()
{
    const auto S=Run->GetSnapshot();
    const bool Live=S.Phase==EDarkRelicPhase::Running || S.Phase==EDarkRelicPhase::Extracting;
    FuryIntensity=static_cast<float>(dark_relic::fury_aura(S.RallyRemaining,Run->Tuning.RallyDuration,Live));
    if (!FuryDynamicMaterial) return;
    const float Age=FMath::Max(0.f,Run->Tuning.RallyDuration-S.RallyRemaining);
    const float Breath=ReducedMotion ? 1.f : 0.94f+0.06f*FMath::Sin(Age*5);
    FuryDynamicMaterial->SetScalarParameterValue(TEXT("Strength"),FuryIntensity*Breath);
    FuryLight->SetIntensity(850*FuryIntensity*Breath);
    for (int32 I=0;I<FuryMeshes.Num();++I)
    {
        auto* Mesh=FuryMeshes[I].Get();
        Mesh->SetVisibility(FuryIntensity>0.001f);
        if (I==0)
        {
            Mesh->SetRelativeScale3D(FVector(1.05f,1.05f,2.15f)*(0.98f+0.02f*Breath));
            Mesh->SetRelativeLocation(FVector(0,0,0));
        }
        else
        {
            const float Angle=I*2*PI/8+(ReducedMotion ? 0.f : Age*0.7f);
            const float Rise=ReducedMotion ? I/8.f : FMath::Frac(Age*0.55f+I/8.f);
            Mesh->SetRelativeLocation(FVector(FMath::Cos(Angle)*52,FMath::Sin(Angle)*52,-72+Rise*165));
            Mesh->SetRelativeScale3D(FVector(0.06f,0.06f,0.28f+0.12f*FMath::Sin(Rise*PI)));
        }
    }
}

void ADarkRelicEncounter::Rally()
{
    if (!Run->TryAction(EDarkRelicAction::Rally))
    { Notify(TEXT("Warden fury unavailable: wait for recovery/cooldown; requires 20 stamina.")); return; }
    PlayCue(165,0.7f,0.12f); PlayCue(330,0.5f,0.08f);
    PlayHeroVoice(EDarkRelicVoice::Fury);
    if (PlayCharacterAction(Player,FuryAnimation,1.1f)) HeroAnimationRemaining=1.1f;
    if (!ReducedMotion) { ShakeRemaining=0.16f; ShakeStrength=0.38f; }
    Notify(TEXT("WARDEN FURY: stronger attacks, reduced incoming damage"));
}

void ADarkRelicEncounter::Heal()
{
    if (!Run->TryAction(EDarkRelicAction::Heal)) { Notify(TEXT("Healing unavailable: check charges, health and recovery.")); return; }
    HealingVoicePending=true; HealingStartHealth=Run->GetSnapshot().Health;
    PlayHeroVoice(EDarkRelicVoice::Heal);
}

void ADarkRelicEncounter::Dodge()
{
    if (!Player || !Run->TryAction(EDarkRelicAction::Dodge)) return;
    RecoilRemaining=0;
    PlayHeroVoice(EDarkRelicVoice::Dodge);
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
    TickFury();
    MessageRemaining = FMath::Max(0.f, MessageRemaining-Dt);
    HitFlash = FMath::Max(0.f, HitFlash-Dt);
    BurstVisualRemaining=FMath::Max(0.f,BurstVisualRemaining-Dt);
    auto* PC = Cast<APlayerController>(Player->GetController());
    const auto State = Run->GetSnapshot();
    const bool Live = State.Phase == EDarkRelicPhase::Running || State.Phase == EDarkRelicPhase::Extracting;
    if (HeroVisuals.Mesh) TickCharacterVisuals(Player,HeroVisuals,HeroAnimationRemaining,Dt);
    for (auto& E : Enemies)
        if (IsValid(E.Actor) && E.Health>0 && EnemyVisuals.IsValidIndex(E.Role))
            TickCharacterVisuals(E.Actor,EnemyVisuals[E.Role],E.AnimationRemaining,Dt);
    if (AuraCapture)
    {
        const float Previous=CaptureElapsed;
        CaptureElapsed+=Dt;
        if (Previous<1 && CaptureElapsed>=1) Rally();
        const float Times[]={1.45f,2.5f,6.f,7.4f};
        const TCHAR* Names[]={TEXT("charge"),TEXT("peak"),TEXT("fade"),TEXT("off")};
        for (int32 I=0;I<4;++I)
            if (Previous<Times[I] && CaptureElapsed>=Times[I])
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("IntegrationEvidence/aura-%s.png"),Names[I]),true,false);
        if (CaptureElapsed>9) FPlatformMisc::RequestExit(false);
    }
    if (PolishCapture)
    {
        const float Previous=CaptureElapsed;
        CaptureElapsed+=Dt;
        for (auto& E : Enemies) { E.Cooldown=100; E.Windup=0; E.BellAttack.remaining=0; E.BellAttack.cooldown=100; }
        auto Crossed=[&](float T){ return Previous<T && CaptureElapsed>=T; };
        if (Crossed(1.4f)) Rally();
        if (CaptureElapsed>=1.5f && CaptureElapsed<3.2f)
        {
            HitFlash=0.25f;
            Enemies[2].AreaCenter=Player->GetActorLocation()+FVector(200,0,0);
            Enemies[2].BellAttack.remaining=1.2f; Enemies[2].BellAttack.duration=1.6f;
            Enemies[2].Actor->TeleportTo(Enemies[2].AreaCenter,FRotator::ZeroRotator,false,true);
            Enemies[1].Actor->TeleportTo(Player->GetActorLocation()+FVector(350,200,0),FRotator::ZeroRotator,false,true);
            Enemies[1].Windup=0.6f;
        }
        if (Crossed(3.5f)) { Player->TeleportTo(ExtractionCenter,FRotator::ZeroRotator,false,true); Run->SetInExtractionZone(true); }
        if (Crossed(5.5f)) Run->CollectLoot(EDarkRelicItem::Blackbell,1,777);
        if (Crossed(7.5f)) Interact();
        if (Crossed(11.5f)) { Run->BuyUpgrade(); }
        if (Crossed(13.5f)) { Restart(); Run->ReceiveDamage(10000); }
        const float Times[]={1.f,2.7f,4.5f,6.5f,8.2f,10.5f,12.5f,14.5f};
        const TCHAR* Names[]={TEXT("neutral"),TEXT("warnings"),TEXT("ward-locked"),TEXT("ward-ready"),TEXT("extracting"),TEXT("victory"),TEXT("upgrade"),TEXT("death")};
        for (int32 I=0;I<8;++I)
            if (Crossed(Times[I])) FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("IntegrationEvidence/polish-%s.png"),Names[I]),true,false);
        if (CaptureElapsed>16)
        {
            UGameplayStatics::DeleteGameInSlot(SmokeSlot,0);
            FPlatformMisc::RequestExit(false);
        }
        return;
    }
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
            if (PC->WasInputKeyJustPressed(EKeys::F)) RelicBurst();
            if (PC->WasInputKeyJustPressed(EKeys::R)) Rally();
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
            {
                if (AttackBurst)
                {
                    BurstCenter=Player->GetActorLocation()-FVector(0,0,75);
                    BurstVisualRemaining=0.5f;
                    PlayCue(110,0.5f,0.16f,1);
                    Impact(Player->GetActorLocation(),true);
                }
                for (auto& E : Enemies)
                {
                    if (!IsValid(E.Actor) || E.Health <= 0) continue;
                    FVector To = E.Actor->GetActorLocation()-Player->GetActorLocation();
                    if (AttackBurst)
                    {
                        if (To.Size2D()>450 || FMath::Abs(To.Z)>180) continue;
                    }
                    else if (To.Size() > (AttackHeavy ? 240 : 200) || FVector::DotProduct(To.GetSafeNormal2D(),Player->GetActorForwardVector()) < 0.15f) continue;
                    FHitResult Hit; FCollisionQueryParams Query; Query.AddIgnoredActor(Player);
                    bool Blocked = GetWorld()->LineTraceSingleByChannel(Hit,Player->GetActorLocation(),E.Actor->GetActorLocation(),ECC_Visibility,Query);
                    if (Blocked && Hit.GetActor() != E.Actor) continue;
                    E.Health -= AttackDamage;
                    ReactEnemyHit(E,Player->GetActorLocation(),AttackHeavy || AttackBurst || AttackFinisher);
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
                    else Notify(FString::Printf(TEXT("%s hit: %.0f"),AttackBurst ? TEXT("Relic burst") : AttackFinisher ? TEXT("Sunder") : AttackHeavy ? TEXT("Heavy") : TEXT("Light"),AttackDamage));
                }
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
                    PlayCue(70,0.9f,0.23f,1,nullptr,dark_relic::CuePriority::Warning);
                    E.Actor->GetCharacterMovement()->MaxWalkSpeed=290;
                }
                const bool WasArea=E.BellAttack.remaining>0;
                if (E.BellAttack.tick(Dt))
                {
                    FHitResult Hit; FCollisionQueryParams Query; Query.AddIgnoredActor(E.Actor);
                    bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,E.AreaCenter,Player->GetActorLocation(),ECC_Visibility,Query);
                    FVector Delta=Player->GetActorLocation()-E.AreaCenter;
                    if (E.BellAttack.hits(Delta.Size2D(),Delta.Z,!Blocked || Hit.GetActor()==Player)) DamagePlayer(E.BellAttack.damage(),E.AreaCenter,true);
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
                    PlayCue(110,0.6f,0.18f,0,&E.AreaCenter,dark_relic::CuePriority::Warning);
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
                    if (Distance < Range+30 && (!Blocked || Hit.GetActor() == Player))
                    {
                        const bool Landed=DamagePlayer(E.Role == 2 ? 30 : 14,E.Actor->GetActorLocation(),E.Role==2);
                        if (Landed && E.Role==1)
                        { HexImpactRemaining=0.22f; HexImpactPosition=Player->GetActorLocation()+FVector(0,0,25); }
                    }
                    E.Cooldown = E.Role == 2 ? (E.BellAttack.enraged ? 1.f : 1.4f) : 1.8f;
                }
            }
            else if (Distance < 1000)
            {
                if (Distance > Range) { if (E.RecoilRemaining<=0) E.Actor->AddMovementInput(To.GetSafeNormal2D(),1,true); }
                else if (E.Cooldown <= 0)
                {
                    E.Windup = E.Role == 2 ? (E.BellAttack.enraged ? 0.9f : 1.1f) : 0.8f;
                    E.Actor->SetActorRotation(To.Rotation());
                    E.Actor->GetCharacterMovement()->StopMovementImmediately();
                    FVector CuePosition=E.Actor->GetActorLocation();
                    PlayCue(E.Role==0 ? 180.f : E.Role==1 ? 620.f : 90.f,0.35f,0.12f,E.Role==0 ? 1 : 0,&CuePosition,dark_relic::CuePriority::Warning);
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
    if (SmokeStage>=30 && SmokeStage<=36)
        for (auto& E : Enemies) { E.Cooldown=100; E.Windup=0; E.BellAttack.cancel(); E.BellAttack.cooldown=100; }
    auto S = Run->GetSnapshot();
    if (SmokeElapsed > 65) { SmokeCheck(TEXT("runtime completed within deadline"),false); FinishSmoke(); return; }
    if (SmokeStage == 0 && SmokeElapsed > 1)
    {
        SmokeCheck(TEXT("real player and HUD"),Player && UGameplayStatics::GetPlayerController(this,0)->GetHUD()->IsA<ADarkRelicHUD>());
        SmokeCheck(TEXT("three live enemies and four pickups"),Enemies.Num()==3 && Pickups.Num()==4);
        const FVector HeroBefore=Player->GetActorLocation(), HexBefore=Enemies[1].Actor->GetActorLocation();
        Player->TeleportTo(FVector(-300,-200,110),FRotator::ZeroRotator,false,true);
        auto& Hex=Enemies[1];
        Hex.Actor->TeleportTo(FVector(0,-200,110),FRotator::ZeroRotator,false,true);
        Hex.Windup=0.8f;
        SmokeCheck(TEXT("live ranged windup identifies visible caster"),HexTellVisible(Hex));
        Hex.Windup=0;
        SmokeCheck(TEXT("interrupted cast removes tell immediately"),!HexTellVisible(Hex));
        Hex.Windup=0.8f; Hex.Health=0;
        SmokeCheck(TEXT("dead caster cannot retain tell"),!HexTellVisible(Hex));
        Hex.Health=Hex.MaxHealth;
        Hex.Actor->TeleportTo(FVector(600,-200,110),FRotator::ZeroRotator,false,true);
        SmokeCheck(TEXT("leaving ranged reach removes tell"),!HexTellVisible(Hex));
        Hex.Actor->TeleportTo(FVector(0,-200,110),FRotator::ZeroRotator,false,true);
        auto* Cover=GetWorld()->SpawnActor<AStaticMeshActor>(FVector(-150,-200,110),FRotator::ZeroRotator);
        Cover->SetMobility(EComponentMobility::Movable);
        Cover->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Cover->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        SmokeCheck(TEXT("blocking cover removes ranged tell"),!HexTellVisible(Hex));
        Cover->Destroy();
        Hex.Windup=0;
        Hex.Actor->TeleportTo(HexBefore,FRotator::ZeroRotator,false,true);
        Player->TeleportTo(HeroBefore,FRotator::ZeroRotator,false,true);
        ClearCues();
        if (!FParse::Param(FCommandLine::Get(),TEXT("nosound")))
        {
            for(int32 I=0;I<16;++I) PlayCue(90,0.15f,0.001f,0,nullptr,dark_relic::CuePriority::Ambient);
            const int32 Evicted=CueEvicted, Dropped=CueDropped;
            FVector CuePosition=Enemies[1].Actor->GetActorLocation();
            PlayCue(620,0.15f,0.001f,0,&CuePosition,dark_relic::CuePriority::Warning);
            SmokeCheck(TEXT("saturated warning evicts ambient and remains bounded"),CueEvicted==Evicted+1 && ActiveSounds.Num()==16);
            auto* Spatial=ActiveSounds.Last().Component.Get();
            SmokeCheck(TEXT("positioned warning uses single spatial attenuation"),Spatial && Spatial->bAllowSpatialization && Spatial->bOverrideAttenuation && Spatial->AttenuationOverrides.bAttenuate && Spatial->GetComponentLocation().Equals(CuePosition,0.1));
            PlayCue(90,0.15f,0.001f,0,nullptr,dark_relic::CuePriority::Ambient);
            SmokeCheck(TEXT("ambient admission cannot evict equal or higher cues"),CueDropped==Dropped+1 && ActiveSounds.Num()==16);
        }
        ClearCues();
        SmokeCheck(TEXT("cue reset destroys all active playback"),ActiveSounds.IsEmpty());
        if (RequireHeroVoices)
        {
            PlayHeroVoice(EDarkRelicVoice::Death);
            const int32 Before=VoiceCount;
            PlayHeroVoice(EDarkRelicVoice::Light);
            SmokeCheck(TEXT("terminal voice survives routine effort"),VoiceCount==Before && LastVoice==EDarkRelicVoice::Death);
            VoiceRemaining=0;
            PlayHeroVoice(EDarkRelicVoice::Pain);
            PlayHeroVoice(EDarkRelicVoice::Dodge);
            SmokeCheck(TEXT("pain voice survives routine dodge"),LastVoice==EDarkRelicVoice::Pain);
            VoiceRemaining=0; PainVoiceCooldown=0;
            if (IsValid(HeroVoiceComponent)) HeroVoiceComponent->Stop();
        }
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
        if (RequireHeroVoices) SmokeCheck(TEXT("healing start voice is bound and requested"),LastVoice==EDarkRelicVoice::Heal && VoiceCount>0);
    }
    else if (SmokeStage == 1 && S.Action == EDarkRelicAction::None)
    {
        SmokeCheck(TEXT("world tick completes healing"),S.Health==S.MaxHealth && S.Heals==1);
        if (RequireHeroVoices) SmokeCheck(TEXT("healing completion voice follows restored health"),LastVoice==EDarkRelicVoice::Healed);
        Dodge(); SmokeCheck(TEXT("dodge blocks live damage"),!Run->ReceiveDamage(25));
        const int32 BeforeVoice=VoiceCount; const int32 BeforeRecoil=RecoilCount;
        SmokeCheck(TEXT("dodged enemy hit emits no pain or recoil"),!DamagePlayer(14,Player->GetActorLocation()-FVector(100,0,0)) && VoiceCount==BeforeVoice && RecoilCount==BeforeRecoil && RecoilRemaining==0);
        if (RequireHeroVoices) SmokeCheck(TEXT("successful dodge vocalizes"),LastVoice==EDarkRelicVoice::Dodge);
        SmokeStage=2;
    }
    else if (SmokeStage == 2 && S.Action == EDarkRelicAction::None)
    {
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->TeleportTo(FVector(-300,-200,110),FRotator::ZeroRotator,false,true);
        Enemies[0].Actor->TeleportTo(FVector(-160,-200,110),FRotator(0,180,0),false,true);
        Attack(false); SmokeStage=10;
        if (RequireHeroVoices) SmokeCheck(TEXT("light strike vocalizes"),LastVoice==EDarkRelicVoice::Light);
        if(RequireCharacterVisuals) SmokeCheck(TEXT("light attack plays Greystone sequence"),Player->GetMesh()->GetSingleNodeInstance()->GetCurrentAsset()==HeroVisuals.LightAttack);
    }
    else if(SmokeStage==10 && S.Action==EDarkRelicAction::None)
    {
        SmokeCheck(TEXT("light attack contacts enemy in world"),Enemies[0].Health==35);
        SmokeCheck(TEXT("landed light strike physically recoils minion"),Enemies[0].RecoilCount==1 && Enemies[0].Actor->GetActorLocation().X>-155);
        if (RequireEnemyVoices) SmokeCheck(TEXT("landed light strike requests minion pain"),Enemies[0].VoiceCount==1);
        Attack(true); SmokeStage=11;
        if (RequireHeroVoices) SmokeCheck(TEXT("heavy strike vocalizes"),LastVoice==EDarkRelicVoice::Heavy);
        if(RequireCharacterVisuals) SmokeCheck(TEXT("heavy attack plays Greystone sequence"),Player->GetMesh()->GetSingleNodeInstance()->GetCurrentAsset()==HeroVisuals.HeavyAttack);
    }
    else if(SmokeStage==11 && S.Action==EDarkRelicAction::None)
    {
        SmokeCheck(TEXT("heavy attack kills and disables enemy"),Enemies[0].Health<=0 && !Enemies[0].Actor->GetActorEnableCollision());
        SmokeCheck(TEXT("lethal strike cancels minion recoil"),Enemies[0].RecoilRemaining==0);
        Enemies[1].Actor->TeleportTo(Player->GetActorLocation()+FVector(100,0,0),FRotator::ZeroRotator,false,true);
        Enemies[1].Cooldown=0; Enemies[1].Windup=0; SmokeStage=13;
    }
    else if(SmokeStage==13 && S.Health<S.MaxHealth)
    {
        SmokeCheck(TEXT("enemy telegraph resolves into real damage"),S.Health==S.MaxHealth-14);
        SmokeCheck(TEXT("live enemy hit starts directional recoil"),RecoilCount>0 && RecoilRemaining>0 && !RecoilDirection.IsNearlyZero());
        if (RequireHeroVoices) SmokeCheck(TEXT("live enemy hit vocalizes pain"),LastVoice==EDarkRelicVoice::Pain);
        Enemies[1].Cooldown=100; Enemies[1].Windup=0;
        Enemies[1].Actor->TeleportTo(Enemies[1].Home,FRotator::ZeroRotator,false,true);
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
        if (RequireHeroVoices) SmokeCheck(TEXT("boss area hit vocalizes heavy pain"),LastVoice==EDarkRelicVoice::HeavyPain);
        SmokeCheck(TEXT("landed attacks emit impact feedback"),ImpactCount>=5);
        for (auto& E : Enemies) { E.Cooldown=100; E.Windup=0; E.BellAttack.cancel(); E.BellAttack.cooldown=100; }
        Enemies[2].Health=500; Enemies[2].MaxHealth=500;
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->TeleportTo(FVector(-300,-200,110),FRotator::ZeroRotator,false,true);
        Enemies[2].Actor->TeleportTo(FVector(-160,-200,110),FRotator::ZeroRotator,false,true);
        SmokeStage=30;
    }
    else if (SmokeStage==30 && S.Action==EDarkRelicAction::None && S.Stamina>=95 && VoiceRemaining<=0)
    {
        Attack(false); SmokeStage=31;
    }
    else if (SmokeStage==31 && S.Action==EDarkRelicAction::None)
    {
        SmokeCheck(TEXT("combo first hit damages live boss"),Enemies[2].Health==475);
        Attack(false); SmokeStage=32;
    }
    else if (SmokeStage==32 && S.Action==EDarkRelicAction::None)
    {
        SmokeCheck(TEXT("combo second hit damages live boss"),Enemies[2].Health==450);
        Attack(false); SmokeStage=33;
        if (RequireHeroVoices) SmokeCheck(TEXT("Sunder finisher uses heavy effort voice"),LastVoice==EDarkRelicVoice::Heavy);
        SmokeCheck(TEXT("finisher uses production rules and Greystone heavy sequence"),Run->GetSnapshot().Finisher && AttackDamage==50 && (!RequireCharacterVisuals || Player->GetMesh()->GetSingleNodeInstance()->GetCurrentAsset()==HeroVisuals.HeavyAttack));
    }
    else if (SmokeStage==33 && S.Action==EDarkRelicAction::None)
    {
        SmokeCheck(TEXT("finisher resolves exactly once"),Enemies[2].Health==400);
        Rally(); SmokeStage=34;
        if (RequireFuryVisuals)
        {
            SmokeCheck(TEXT("Fury plays bound expressive sequence"),Player->GetMesh()->GetSingleNodeInstance()->GetCurrentAsset()==FuryAnimation && HeroAnimationRemaining>1);
            SmokeCheck(TEXT("Fury creates bounded collision-free aura and light"),FuryMeshes.Num()==9 && FuryDynamicMaterial && FuryLight && FuryMeshes[0]->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
        }
        if (RequireHeroVoices) SmokeCheck(TEXT("Fury power-up vocalizes"),LastVoice==EDarkRelicVoice::Fury);
        SmokeCheck(TEXT("fury input reaches live component"),Run->GetSnapshot().RallyRemaining>0 && Run->GetSnapshot().RallyCooldown>0);
        SmokeAbilityHealth=Run->GetSnapshot().Health;
        Run->ReceiveDamage(20);
        SmokeCheck(TEXT("fury mitigates incoming live damage"),FMath::IsNearlyEqual(Run->GetSnapshot().Health,SmokeAbilityHealth-15));
    }
    else if (SmokeStage==34 && S.Action==EDarkRelicAction::None)
    {
        if (RequireFuryVisuals) SmokeCheck(TEXT("Fury aura reaches active intensity from real timer"),FuryIntensity>0.95f && FuryLight->Intensity>0 && FuryMeshes[0]->IsVisible());
        Enemies[1].Health=200; Enemies[1].MaxHealth=200;
        Enemies[1].Actor->TeleportTo(FVector(-600,-200,110),FRotator::ZeroRotator,false,true);
        RelicBurst(); SmokeStage=35;
        if (RequireHeroVoices) SmokeCheck(TEXT("Relic Burst vocalizes"),LastVoice==EDarkRelicVoice::Burst);
        SmokeCheck(TEXT("burst starts buffed damage and cooldown"),AttackBurst && FMath::IsNearlyEqual(AttackDamage,60.75f) && Run->GetSnapshot().BurstCooldown>0);
    }
    else if (SmokeStage==35 && S.Action==EDarkRelicAction::None)
    {
        SmokeCheck(TEXT("buffed relic burst hits live boss once"),FMath::IsNearlyEqual(Enemies[2].Health,339.25f));
        SmokeCheck(TEXT("relic burst recoils boss with heavyweight distance"),Enemies[2].RecoilCount>=4 && Enemies[2].RecoilDistance==22.f);
        SmokeCheck(TEXT("relic shockwave hits enemy behind Warden"),FMath::IsNearlyEqual(Enemies[1].Health,139.25f));
        SmokeCheck(TEXT("relic cast produces visible pulse while fury is active"),BurstVisualRemaining>0 && S.RallyRemaining>0);
        if (!FParse::Param(FCommandLine::Get(),TEXT("NullRHI"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("IntegrationEvidence/warden-abilities-frame.png"),true,false);
        const float Before=Enemies[2].Health;
        const int32 BeforeVoice=VoiceCount;
        RelicBurst(); Rally();
        SmokeCheck(TEXT("rejected cooldown actions do not vocalize"),VoiceCount==BeforeVoice);
        SmokeCheck(TEXT("cooldown rejects repeat cast without pending hit"),Run->GetSnapshot().Action==EDarkRelicAction::None && AttackRemaining<=0 && Enemies[2].Health==Before);
        SmokeStage=36;
    }
    else if (SmokeStage==36 && S.RallyRemaining<=0)
    {
        TickFury();
        if (RequireFuryVisuals) SmokeCheck(TEXT("Fury expiry extinguishes aura and light"),FuryIntensity==0 && FuryLight->Intensity==0 && !FuryMeshes[0]->IsVisible());
        SmokeAbilityHealth=S.Health; Run->ReceiveDamage(4);
        SmokeCheck(TEXT("fury expiry restores incoming damage"),FMath::IsNearlyEqual(Run->GetSnapshot().Health,SmokeAbilityHealth-4));
        Player->GetCharacterMovement()->StopMovementImmediately();
        const FVector Start=Player->GetActorLocation();
        PainVoiceCooldown=0;
        DamagePlayer(1,Start-FVector(100,0,0));
        TickRecoil(0.16f);
        const FVector Delta=Player->GetActorLocation()-Start;
        SmokeCheck(TEXT("hit recoil moves 55 cm away without vertical launch"),FMath::IsNearlyEqual(Delta.X,HitRecoilDistance,1.f) && FMath::Abs(Delta.Y)<1 && FMath::Abs(Delta.Z)<1 && RecoilRemaining==0);
        auto* Wall=GetWorld()->SpawnActor<AStaticMeshActor>(Player->GetActorLocation()+FVector(65,0,0),FRotator::ZeroRotator);
        Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Wall->SetActorScale3D(FVector(0.1f,3,4));
        Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        const FVector WallStart=Player->GetActorLocation();
        DamagePlayer(1,WallStart-FVector(100,0,0)); TickRecoil(0.16f);
        const float WallTravel=Player->GetActorLocation().X-WallStart.X;
        SmokeCheck(TEXT("recoil sweep stops at a wall without penetration"),WallTravel>=0 && WallTravel<HitRecoilDistance-5 && RecoilRemaining==0);
        Wall->Destroy();
        if (RequireHeroVoices && !FParse::Param(FCommandLine::Get(),TEXT("nosound")))
        {
            PlayHeroVoice(EDarkRelicVoice::Heavy);
            SmokeCheck(TEXT("packaged voice component starts real audio playback"),IsValid(HeroVoiceComponent) && HeroVoiceComponent->IsPlaying() && HeroVoiceComponent->Sound);
        }
        const FVector HeroBefore=Player->GetActorLocation();
        auto* TestFloor=GetWorld()->SpawnActor<AStaticMeshActor>(FVector(0,0,1900),FRotator::ZeroRotator);
        TestFloor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        TestFloor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        TestFloor->SetActorScale3D(FVector(20,20,0.2f));
        TestFloor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        Player->TeleportTo(FVector(-800,-200,2010),FRotator::ZeroRotator,false,true);
        auto& Target=Enemies[1];
        Target.Actor->TeleportTo(FVector(-300,-200,2010),FRotator::ZeroRotator,false,true);
        const FVector EnemyStart=Target.Actor->GetActorLocation();
        Target.PainCooldown=0;
        ReactEnemyHit(Target,EnemyStart-FVector(100,0,0),false);
        const int32 FirstVariant=Target.LastPainIndex;
        const int32 FirstVoiceCount=Target.VoiceCount;
        if (RequireEnemyVoices && !FParse::Param(FCommandLine::Get(),TEXT("nosound")))
            SmokeCheck(TEXT("enemy pain component starts spatial audio playback"),IsValid(Target.VoiceComponent) && Target.VoiceComponent->IsPlaying() && Target.VoiceComponent->bAllowSpatialization);
        ReactEnemyHit(Target,EnemyStart-FVector(100,0,0),false);
        SmokeCheck(TEXT("rapid hits throttle pain without suppressing recoil"),Target.VoiceCount==FirstVoiceCount && Target.RecoilRemaining>0);
        TickEnemyFeedback(Target,0.06f); TickEnemyFeedback(Target,0.12f);
        const FVector EnemyDelta=Target.Actor->GetActorLocation()-EnemyStart;
        UE_LOG(LogTemp,Display,TEXT("DARK_RELIC_RECOIL_MEASURE minion_light=%.3f y=%.3f z=%.3f remaining=%.6f"),EnemyDelta.X,EnemyDelta.Y,EnemyDelta.Z,Target.RecoilRemaining);
        SmokeCheck(TEXT("minion recoil travels 45 cm across split frame times"),FMath::IsNearlyEqual(EnemyDelta.X,45.f,1.f) && FMath::Abs(EnemyDelta.Z)<1 && Target.RecoilRemaining<=0.00001f);
        Target.PainCooldown=0;
        ReactEnemyHit(Target,Target.Actor->GetActorLocation()-FVector(100,0,0),true);
        if (RequireEnemyVoices) SmokeCheck(TEXT("successive pain uses a different recording"),Target.LastPainIndex!=FirstVariant && Target.VoiceCount==FirstVoiceCount+1);
        const FVector HeavyStart=Target.Actor->GetActorLocation();
        TickEnemyFeedback(Target,1.f);
        UE_LOG(LogTemp,Display,TEXT("DARK_RELIC_RECOIL_MEASURE minion_heavy=%.3f"),Target.Actor->GetActorLocation().X-HeavyStart.X);
        SmokeCheck(TEXT("heavy minion recoil is bounded at 75 cm even on long frame"),FMath::IsNearlyEqual(Target.Actor->GetActorLocation().X-HeavyStart.X,75.f,1.f) && Target.RecoilRemaining==0);
        auto* EnemyWall=GetWorld()->SpawnActor<AStaticMeshActor>(Target.Actor->GetActorLocation()+FVector(55,0,0),FRotator::ZeroRotator);
        EnemyWall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        EnemyWall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        EnemyWall->SetActorScale3D(FVector(0.1f,3,4));
        EnemyWall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        const FVector BlockStart=Target.Actor->GetActorLocation();
        ReactEnemyHit(Target,BlockStart-FVector(100,0,0),true); TickEnemyFeedback(Target,0.18f);
        const float BlockTravel=Target.Actor->GetActorLocation().X-BlockStart.X;
        SmokeCheck(TEXT("enemy recoil sweep stops before wall penetration"),BlockTravel>=0 && BlockTravel<30 && Target.RecoilRemaining==0);
        EnemyWall->Destroy();
        Target.Actor->TeleportTo(FVector(-300,-200,4000),FRotator::ZeroRotator,false,true);
        const FVector AirStart=Target.Actor->GetActorLocation();
        ReactEnemyHit(Target,AirStart-FVector(100,0,0),false); TickEnemyFeedback(Target,0.18f);
        SmokeCheck(TEXT("enemy recoil rejects unsupported ground"),Target.Actor->GetActorLocation().Equals(AirStart,0.01f) && Target.RecoilRemaining==0);
        auto& Boss=Enemies[2];
        Boss.Actor->TeleportTo(FVector(-300,100,2035),FRotator::ZeroRotator,false,true);
        const FVector BossStart=Boss.Actor->GetActorLocation();
        Boss.BellAttack.remaining=0.8; Boss.AreaCenter=BossStart;
        ReactEnemyHit(Boss,BossStart-FVector(100,0,0),true); TickEnemyFeedback(Boss,0.18f);
        SmokeCheck(TEXT("boss recoil preserves committed area warning and center"),Boss.BellAttack.remaining==0.8 && Boss.AreaCenter.Equals(BossStart));
        SmokeCheck(TEXT("boss recoils only 22 cm on a heavy hit"),FMath::IsNearlyEqual(Boss.Actor->GetActorLocation().X-BossStart.X,22.f,1.f));
        Boss.BellAttack.cancel();
        if (RequireEnemyVoices)
        {
            const auto* BossVoice=EnemyVoices.FindByPredicate([](const FDarkRelicEnemyVoiceBinding& B){return B.Role==2;});
            const auto* MinionVoice=EnemyVoices.FindByPredicate([](const FDarkRelicEnemyVoiceBinding& B){return B.Role==1;});
            SmokeCheck(TEXT("Bellkeeper uses lower pitched pain than minions"),BossVoice && MinionVoice && BossVoice->Pitch<MinionVoice->Pitch);
        }
        Target.Health=0; Target.RecoilRemaining=0.18f; TickEnemyFeedback(Target,0.18f);
        SmokeCheck(TEXT("dead enemy never continues recoil"),Target.RecoilRemaining==0);
        TickEnemyFeedback(Target,6.f);
        SmokeCheck(TEXT("enemy voice component stops after bounded duration"),Target.VoiceRemaining==0 && (!IsValid(Target.VoiceComponent) || !Target.VoiceComponent->IsPlaying()));
        Player->TeleportTo(HeroBefore,FRotator::ZeroRotator,false,true);
        TestFloor->Destroy();
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
        SmokeCheck(TEXT("results reconcile run earnings"),RunEarnedCredits==S.Credits-RunStartingCredits && RunEarnedCredits>0);
        SmokeCheck(TEXT("escape clears all Warden ability state"),S.ComboStep==0 && S.RallyRemaining==0 && S.BurstCooldown==0 && S.RallyCooldown==0);
        SmokeCheck(TEXT("extraction bells and escape celebration triggered"),BellCount>0 && CelebrationRemaining>0);
        if (RequireHeroVoices) SmokeCheck(TEXT("extraction success vocalizes"),LastVoice==EDarkRelicVoice::Cheer);
        auto* Fresh=NewObject<UDarkRelicRunComponent>(this); Fresh->SaveSlot=SmokeSlot;
        SmokeCheck(TEXT("fresh component reloads non-empty bank"),Fresh->LoadBank() && Fresh->GetSnapshot().Credits==S.Credits && Fresh->GetSnapshot().Banked[3]==1);
        SmokeCheck(TEXT("upgrade purchased from earned bank"),Run->BuyUpgrade());
        SmokeCheck(TEXT("upgraded bank saves"),Run->SaveBank());
        Restart(); Run->CollectLoot(EDarkRelicItem::Salt,1,99); Run->ReceiveDamage(10000);
        SmokeCheck(TEXT("death loses carried loot and keeps bank"),Run->GetSnapshot().Phase==EDarkRelicPhase::Dead && Run->GetSnapshot().Carried[2]==0 && Run->GetSnapshot().Banked[3]==1);
        if (RequireHeroVoices) SmokeCheck(TEXT("death vocalizes and clears recoil"),LastVoice==EDarkRelicVoice::Death && RecoilRemaining==0);
        Restart(); SmokeCheck(TEXT("restart restores upgraded health"),Run->GetSnapshot().Health==Run->GetSnapshot().MaxHealth && Run->GetSnapshot().Upgrade==1);
        SmokeCheck(TEXT("new run resets Warden abilities"),Run->GetSnapshot().ComboStep==0 && Run->GetSnapshot().RallyRemaining==0 && Run->GetSnapshot().BurstCooldown==0);
        SmokeCheck(TEXT("restart clears run earnings and stale cast effects"),RunEarnedCredits==0 && HexImpactRemaining==0 && ActiveSounds.IsEmpty());
        SmokeCheck(TEXT("restart clears recoil healing and voice playback"),RecoilRemaining==0 && !HealingVoicePending && (!IsValid(HeroVoiceComponent) || !HeroVoiceComponent->IsPlaying()));
        for (const auto& E : Enemies) SmokeCheck(FString::Printf(TEXT("restart clears enemy %d feedback"),E.Role),E.RecoilRemaining==0 && E.VoiceCount==0 && !IsValid(E.VoiceComponent));
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
    const float Scale=FMath::Clamp(FMath::Min(Canvas->ClipY/1080.f,Canvas->ClipX/1440.f),0.5f,2.f);
    const bool Live=S.Phase==EDarkRelicPhase::Running || S.Phase==EDarkRelicPhase::Extracting;
    const bool HasRelic=S.Carried.IsValidIndex(3) && S.Carried[3]>0;
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
    if(Game->HitFlash>0) DrawRect(FLinearColor(0.5f,0.05f,0.02f,Game->HitFlash),0,0,Canvas->ClipX,Canvas->ClipY);
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
    if (!Live)
    {
        const float X=Canvas->ClipX/Scale*0.5f-350, Y=Canvas->ClipY/Scale*0.34f;
        DrawRect(Ink,X*Scale,(Y-24)*Scale,700*Scale,290*Scale);
        Label(S.Phase==EDarkRelicPhase::Escaped?TEXT("BLACKBELL RECLAIMED"):TEXT("THE FEN CLAIMED YOU"),X+28,Y,1.4f,Gold);
        Label(S.Phase==EDarkRelicPhase::Escaped ? FString::Printf(TEXT("This run: +%d credits secured"),Game->RunEarnedCredits) : TEXT("Carried spoils lost. Your bank is safe."),X+28,Y+50,0.9f,Text);
        Label(FString::Printf(TEXT("Available bank: %d credits"),S.Credits),X+28,Y+88,0.9f,Text);
        const FString Upgrade=S.Upgrade>0 ? TEXT("Resolve owned: +20 max health on each new run") : S.Credits>=dark_relic::Rules::UpgradeCost ? TEXT("U  Buy Resolve: 100 credits, +20 max health next run") : FString::Printf(TEXT("Resolve: %d / 100 credits, +20 max health next run"),S.Credits);
        Label(Upgrade,X+28,Y+130,0.82f,Text);
        Label(TEXT("R  NEW RUN"),X+28,Y+198,1.2f,Gold);
        Label(TEXT("Esc  Quit"),X+460,Y+206,0.85f,Text);
        if (Game->MessageRemaining>0) Label(Game->Message,40,Canvas->ClipY/Scale-80,0.85f,Text);
        return;
    }
    DrawRect(Ink,24*Scale,66*Scale,340*Scale,138*Scale);
    DrawRect(Ink,24*Scale,210*Scale,535*Scale,73*Scale);
    if (S.InZone) DrawRect(Ink,24*Scale,293*Scale,535*Scale,78*Scale);
    DrawRect(Ink,Canvas->ClipX-380*Scale,66*Scale,356*Scale,(S.ComboStep>0 ? 205 : 148)*Scale);
    Label(TEXT("Health"),40,76,0.9f,Text); Bar(40,100,260,16,S.Health/S.MaxHealth,Red);
    Label(TEXT("Stamina"),40,123,0.9f,Text); Bar(40,147,260,12,S.Stamina/S.MaxStamina,Green);
    Label(FString::Printf(TEXT("Heals %d   |   Bank %d   |   Resolve %d/1"),S.Heals,S.Credits,S.Upgrade),40,176,0.8f,Text);
    const bool AbilityLive=S.Phase==EDarkRelicPhase::Running || S.Phase==EDarkRelicPhase::Extracting;
    if (AbilityLive)
    {
        const float AbilityX=Canvas->ClipX/Scale-360;
        auto AbilityStatus=[&](float Cooldown,float Cost)
        {
            if (Cooldown>0) return FString::Printf(TEXT("%.1fs"),Cooldown);
            if (S.Action!=EDarkRelicAction::None) return FString(TEXT("Recovering"));
            if (S.Stamina<Cost) return FString(TEXT("Low stamina"));
            return FString(TEXT("Ready"));
        };
        Label(TEXT("WARDEN RELIC ARTS"),AbilityX,76,0.9f,Text);
        Label(TEXT("F  Burst  ")+AbilityStatus(S.BurstCooldown,Game->Run->Tuning.BurstCost),AbilityX,110,0.85f,Gold);
        Bar(AbilityX,139,280,9,1-S.BurstCooldown/Game->Run->Tuning.BurstCooldown,Gold);
        Label(S.RallyRemaining>0 ? FString::Printf(TEXT("R  Fury  ACTIVE %.1fs"),S.RallyRemaining) : TEXT("R  Fury  ")+AbilityStatus(S.RallyCooldown,Game->Run->Tuning.RallyCost),AbilityX,164,0.85f,Gold);
        Bar(AbilityX,193,280,9,S.RallyRemaining>0 ? S.RallyRemaining/Game->Run->Tuning.RallyDuration : 1-S.RallyCooldown/Game->Run->Tuning.RallyCooldown,Gold);
        if (S.ComboStep>0)
        {
            Label(S.ComboStep==3 ? TEXT("SUNDER") : FString::Printf(TEXT("Sword chain %d/3"),S.ComboStep),AbilityX,220,0.9f,Text);
            Bar(AbilityX,250,280,8,S.ComboRemaining/(S.ComboStep==3 ? Game->Run->Tuning.FinisherSeconds+Game->Run->Tuning.ComboWindow : Game->Run->Tuning.LightSeconds+Game->Run->Tuning.ComboWindow),Gold);
        }
        if (S.RallyRemaining>0) Ring(Game->Player->GetActorLocation()-FVector(0,0,75),75,Gold,2);
        if (Game->BurstVisualRemaining>0)
            Ring(Game->BurstCenter,450*(1-Game->BurstVisualRemaining/0.5f),FLinearColor(0.76f,0.65f,0.93f,Game->BurstVisualRemaining*2),5);
    }
    Label(HasRelic ? TEXT("REACH THE NORTHERN WARD") : TEXT("DEFEAT THE BELLKEEPER. RECOVER BLACKBELL."),40,220,0.9f,Gold);
    if (S.Carried.Num()==4) Label(FString::Printf(TEXT("Iron %d   Tallow %d   Salt %d   Blackbell %d"),S.Carried[0],S.Carried[1],S.Carried[2],S.Carried[3]),40,253,0.8f,Text);
    float Bottom=Canvas->ClipY/Scale;
    DrawRect(Ink,24*Scale,(Bottom-104)*Scale,Canvas->ClipX-48*Scale,80*Scale);
    Label(TEXT("WASD Move  |  Mouse Look  |  E Interact  |  Q Heal  |  M Shake  |  Esc Quit"),40,Bottom-91,0.82f,Text);
    Label(TEXT("LMB Chain x3  |  RMB Heavy  |  Shift Dodge  |  F Relic Burst  |  R Fury"),40,Bottom-59,0.82f,Text);
    if (Game->MessageRemaining>0) Label(Game->Message,40,Bottom-141,1.f,Gold);
    if (S.Phase==EDarkRelicPhase::Extracting)
    { Label(FString::Printf(TEXT("HOLD THE WARD  %.1fs"),S.ExtractionRemaining),40,302,1.2f,Gold); Bar(40,340,300,14,1-S.ExtractionRemaining/Game->Run->Tuning.ExtractionSeconds,Gold); }
    else if (dark_relic::ward_prompt(Live,false,S.InZone,HasRelic)==dark_relic::WardPrompt::Ready)
        Label(TEXT("E  Begin extraction"),40,300,1.1f,Gold);
    else if (S.InZone) Label(TEXT("WARD SEALED: recover Blackbell first"),40,300,0.9f,Text);
    if (HasRelic && !S.InZone)
    {
        FVector Mark=Project(Game->ExtractionCenter+FVector(0,0,130));
        const float X=FMath::Clamp(static_cast<float>(Mark.X/Scale),600.f,Canvas->ClipX/Scale-440.f);
        const float Y=FMath::Clamp(static_cast<float>(Mark.Y/Scale),310.f,Canvas->ClipY/Scale-220.f);
        if (Mark.Z>0 && Mark.X>=0 && Mark.X<=Canvas->ClipX)
        {
            DrawRect(Ink,(X-10)*Scale,(Y-5)*Scale,250*Scale,38*Scale);
            Label(FString::Printf(TEXT("WARD  %.0fm"),FVector::Dist2D(Game->Player->GetActorLocation(),Game->ExtractionCenter)/100),X,Y,0.85f,Gold);
        }
        else
        {
            const auto* PC=Cast<APlayerController>(Game->Player->GetController());
            const FVector Right=PC ? FRotationMatrix(PC->GetControlRotation()).GetUnitAxis(EAxis::Y) : Game->Player->GetActorRightVector();
            const bool TurnRight=FVector::DotProduct(Game->ExtractionCenter-Game->Player->GetActorLocation(),Right)>=0;
            const float Center=Canvas->ClipX/Scale*0.5f;
            DrawRect(Ink,(Center-180)*Scale,300*Scale,360*Scale,42*Scale);
            Label(TurnRight ? TEXT("TURN RIGHT TO THE WARD  >") : TEXT("<  TURN LEFT TO THE WARD"),Center-165,307,0.85f,Gold);
        }
    }
    if (Game->HexImpactRemaining>0)
    {
        FVector P=Project(Game->HexImpactPosition);
        if (P.Z>0)
        {
            float R=24*Scale*(1-Game->HexImpactRemaining/0.22f);
            DrawLine(P.X-R,P.Y-R,P.X+R,P.Y+R,FLinearColor(0.7f,0.8f,1),3*Scale);
            DrawLine(P.X-R,P.Y+R,P.X+R,P.Y-R,FLinearColor(0.7f,0.8f,1),3*Scale);
        }
    }
    for (const auto& E : Game->Enemies)
    {
        if (!IsValid(E.Actor)||E.Health<=0) continue;
        bool Area=E.Role==2 && E.BellAttack.remaining>0;
        if (Area)
        {
            FVector Ground=E.AreaCenter-FVector(0,0,75);
            const FLinearColor Danger(1.f,0.38f,0.18f);
            Ring(Ground,static_cast<float>(dark_relic::BellkeeperAttack::Radius),Danger,5);
            for (int32 I=0;I<12;++I)
            {
                const float A=I*PI/6;
                const FVector Dir(FMath::Cos(A),FMath::Sin(A),0);
                FVector P=Project(Ground+Dir*325), Q=Project(Ground+Dir*395);
                if (P.Z>0 && Q.Z>0) DrawLine(P.X,P.Y,Q.X,Q.Y,Danger,4*Scale);
            }
            Ring(Ground,static_cast<float>(dark_relic::BellkeeperAttack::Radius*(1-E.BellAttack.remaining/E.BellAttack.duration)),Red,3);
        }
        FVector P=Project(E.Actor->GetActorLocation()+FVector(0,0,120));
        if(P.Z<=0) continue;
        float X=P.X/Scale-65,Y=P.Y/Scale;
        const bool Hex=Game->HexTellVisible(E);
        if (Hex)
        {
            FVector Cast=Project(E.Actor->GetActorLocation()+E.Actor->GetActorForwardVector()*40+FVector(0,0,45));
            if (Cast.Z>0)
            {
                const float R=(12+18*(1-E.Windup/0.8f))*Scale;
                const FLinearColor Tell(0.72f,0.8f,1.f);
                DrawLine(Cast.X,Cast.Y-R,Cast.X+R,Cast.Y,Tell,3*Scale);
                DrawLine(Cast.X+R,Cast.Y,Cast.X,Cast.Y+R,Tell,3*Scale);
                DrawLine(Cast.X,Cast.Y+R,Cast.X-R,Cast.Y,Tell,3*Scale);
                DrawLine(Cast.X-R,Cast.Y,Cast.X,Cast.Y-R,Tell,3*Scale);
            }
        }
        DrawRect(Ink,(X-8)*Scale,(Y-4)*Scale,(Area ? 235 : E.Role==2 ? 295 : 210)*Scale,(Area ? 48 : 38)*Scale);
        Label(Area ? TEXT("!  LEAVE THE RING") : Hex ? TEXT("!  HEX CAST: DODGE") : E.Windup>0 && E.Role!=1 ? TEXT("!  DODGE") : E.Role==2 ? (E.BellAttack.enraged ? TEXT("BELLKEEPER: ENRAGED") : TEXT("BELLKEEPER")) : E.Role==1 ? TEXT("HEXBOUND") : TEXT("DREG"),X,Y,0.75f,(Area||E.Windup>0)?Gold:Text);
        Bar(X,Y+22,130,8,E.Health/E.MaxHealth,Red);
        if (Area) Bar(X,Y+33,130,5,static_cast<float>(1-E.BellAttack.remaining/E.BellAttack.duration),Gold);
    }
}
