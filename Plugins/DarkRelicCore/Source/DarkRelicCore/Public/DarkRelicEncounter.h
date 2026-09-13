#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "DarkRelicRunComponent.h"
#include "Portable/BellkeeperAttack.h"
#include "Portable/Presentation.h"
#include "DarkRelicEncounter.generated.h"

class ACharacter;
class APlayerController;
class USkeletalMesh;
class UAnimSequence;
class UBlendSpace;
class UAudioComponent;
class USoundBase;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EDarkRelicVoice : uint8
{
    Pain, HeavyPain, Light, Heavy, Dodge, Burst, Fury, Heal, Healed, Death, Cheer
};

USTRUCT(BlueprintType)
struct FDarkRelicVoiceBinding
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDarkRelicVoice Event = EDarkRelicVoice::Pain;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USoundBase> Sound;
};

USTRUCT()
struct FDarkRelicTimedSound
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UAudioComponent> Component;
    float Remaining = 0;
    dark_relic::CuePriority Priority = dark_relic::CuePriority::Action;
};

USTRUCT(BlueprintType)
struct FDarkRelicEnemyVoiceBinding
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Role = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<USoundBase>> PainSounds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Pitch = 0.85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Volume = 0.65f;
};

struct FDarkRelicImpact
{
    FVector Position = FVector::ZeroVector;
    float Remaining = 0.3f;
    bool Heavy = false;
};

USTRUCT(BlueprintType)
struct FDarkRelicCharacterVisuals
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USkeletalMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> Move;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UBlendSpace> Locomotion;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> LightAttack;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> HeavyAttack;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> Dodge;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> Death;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector LocomotionAxes = FVector(0,1,0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LocomotionMaxSpeed = 600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MeshScale = 1;
};

USTRUCT()
struct FDarkRelicEnemy
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<ACharacter> Actor;
    FVector Home = FVector::ZeroVector;
    float Health = 60;
    float MaxHealth = 60;
    float Cooldown = 1;
    float Windup = 0;
    int32 Role = 0;
    float AnimationRemaining = 0;
    dark_relic::BellkeeperAttack BellAttack;
    FVector AreaCenter = FVector::ZeroVector;
    FVector RecoilDirection = FVector::ZeroVector;
    float RecoilRemaining = 0;
    float RecoilDistance = 0;
    float PainCooldown = 0;
    float VoiceRemaining = 0;
    int32 VoiceCount = 0;
    int32 RecoilCount = 0;
    int32 LastPainIndex = -1;
    UPROPERTY() TObjectPtr<UAudioComponent> VoiceComponent;
};

UCLASS()
class DARKRELICCORE_API ADarkRelicEncounter : public AActor
{
    GENERATED_BODY()
public:
    ADarkRelicEncounter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dark Relic") TObjectPtr<UDarkRelicRunComponent> Run;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") FVector ExtractionCenter = FVector(0,1600,100);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") FVector PlayerStart = FVector(-650,-450,120);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Characters") FDarkRelicCharacterVisuals HeroVisuals;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Characters") TArray<FDarkRelicCharacterVisuals> EnemyVisuals;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Characters") bool RequireCharacterVisuals = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Voice") TArray<FDarkRelicVoiceBinding> HeroVoices;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Voice") bool RequireHeroVoices = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Voice", meta=(ClampMin="0",ClampMax="2")) float VoiceVolume = 0.85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Feedback", meta=(ClampMin="0",ClampMax="100")) float HitRecoilDistance = 55.f;
    UPROPERTY() TObjectPtr<UAudioComponent> HeroVoiceComponent;
    EDarkRelicVoice LastVoice = EDarkRelicVoice::Pain;
    int32 VoiceCount = 0;
    int32 RecoilCount = 0;
    FVector RecoilDirection = FVector::ZeroVector;
    float RecoilRemaining = 0;
    float PainVoiceCooldown = 0;
    float VoiceRemaining = 0;
    bool HealingVoicePending = false;
    float HealingStartHealth = 0;
    FVector SmokeRecoilStart = FVector::ZeroVector;
    int32 SmokeVoiceCount = 0;
    void PlayHeroVoice(EDarkRelicVoice Event);
    bool DamagePlayer(float Amount, const FVector& Source, bool Heavy = false);
    void TickRecoil(float DeltaSeconds);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Enemy Feedback") TArray<FDarkRelicEnemyVoiceBinding> EnemyVoices;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Enemy Feedback") bool RequireEnemyVoices = false;
    void ReactEnemyHit(FDarkRelicEnemy& Enemy, const FVector& Source, bool Heavy);
    void TickEnemyFeedback(FDarkRelicEnemy& Enemy, float DeltaSeconds);
    void ClearEnemyFeedback(FDarkRelicEnemy& Enemy);
    UPROPERTY() TObjectPtr<ACharacter> Player;
    UPROPERTY() TArray<FDarkRelicEnemy> Enemies;
    UPROPERTY() TArray<TObjectPtr<AActor>> Pickups;
    FString Message;
    float MessageRemaining = 0;
    float HitFlash = 0;
    float AttackRemaining = 0;
    float AttackDamage = 0;
    bool AttackHeavy = false;
    bool AttackBurst = false;
    bool AttackFinisher = false;
    float BurstVisualRemaining = 0;
    FVector BurstCenter = FVector::ZeroVector;
    bool Smoke = false;
    bool SmokeFailed = false;
    bool Capture = false;
    float CaptureElapsed = 0;
    int32 SmokeStage = 0;
    int32 SmokeChecks = 0;
    float SmokeElapsed = 0;
    FString SmokeSlot;
    float HeroAnimationRemaining = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Fury") TObjectPtr<UAnimSequence> FuryAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Fury") TObjectPtr<UMaterialInterface> FuryMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Fury") bool RequireFuryVisuals = false;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FuryDynamicMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> PreviousFuryOverlay;
    UPROPERTY() TObjectPtr<UPointLightComponent> FuryLight;
    float FuryIntensity = 0;
    bool AuraCapture = false;
    void InitializeFury();
    void TickFury();
    UPROPERTY() TArray<FDarkRelicTimedSound> ActiveSounds;
    TArray<FDarkRelicImpact> Impacts;
    float FootstepRemaining = 0;
    float AmbienceRemaining = 0;
    float BellRemaining = 0;
    float WardPulse = 0;
    float CelebrationRemaining = 0;
    float ShakeRemaining = 0;
    float ShakeStrength = 0;
    bool ReducedMotion = false;
    int32 ImpactCount = 0;
    int32 BellCount = 0;
    int32 AreaAttackCount = 0;
    float SmokeAreaHealth = 0;
    float SmokeAbilityHealth = 0;
    int32 CueAdmitted = 0;
    int32 CueDropped = 0;
    int32 CueEvicted = 0;
    int32 VoiceDropped = 0;
    int32 RunStartingCredits = 0;
    int32 RunEarnedCredits = 0;
    float HexImpactRemaining = 0;
    FVector HexImpactPosition = FVector::ZeroVector;
    bool PolishCapture = false;
    bool HexTellVisible(const FDarkRelicEnemy& Enemy) const;
    void ClearCues();
    void PlayCue(float Frequency, float Duration, float Gain, int32 Texture = 0, const FVector* Position = nullptr, dark_relic::CuePriority Priority = dark_relic::CuePriority::Action);
    void FeedbackTick(float DeltaSeconds);
    void Impact(const FVector& Position, bool Heavy);
    UFUNCTION() void EndRun(bool Escaped);
    void Restart();
    void Attack(bool Heavy);
    void RelicBurst();
    void Rally();
    void Interact();
    void Heal();
    void Dodge();
    void Notify(const FString& Text);
    void ResetEncounter();
    void SmokeTick(float DeltaSeconds);
    void SmokeCheck(const FString& Name, bool Passed);
private:
    bool Initialized = false;
    bool InitializePlayer();
    void FinishSmoke();
};

UCLASS()
class DARKRELICCORE_API ADarkRelicHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
