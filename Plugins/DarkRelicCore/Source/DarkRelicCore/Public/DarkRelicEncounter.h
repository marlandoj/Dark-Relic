#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "DarkRelicRunComponent.h"
#include "DarkRelicEncounter.generated.h"

class ACharacter;
class APlayerController;
class USkeletalMesh;
class UAnimSequence;
class UBlendSpace;

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
};

UCLASS()
class DARKRELICCORE_API ADarkRelicEncounter : public AActor
{
    GENERATED_BODY()
public:
    ADarkRelicEncounter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dark Relic") TObjectPtr<UDarkRelicRunComponent> Run;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") FVector ExtractionCenter = FVector(0,1600,100);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") FVector PlayerStart = FVector(-650,-450,120);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Characters") FDarkRelicCharacterVisuals HeroVisuals;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Characters") TArray<FDarkRelicCharacterVisuals> EnemyVisuals;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Characters") bool RequireCharacterVisuals = false;
    UPROPERTY() TObjectPtr<ACharacter> Player;
    UPROPERTY() TArray<FDarkRelicEnemy> Enemies;
    UPROPERTY() TArray<TObjectPtr<AActor>> Pickups;
    FString Message;
    float MessageRemaining = 0;
    float HitFlash = 0;
    float AttackRemaining = 0;
    float AttackDamage = 0;
    bool AttackHeavy = false;
    bool Smoke = false;
    bool SmokeFailed = false;
    bool Capture = false;
    float CaptureElapsed = 0;
    int32 SmokeStage = 0;
    int32 SmokeChecks = 0;
    float SmokeElapsed = 0;
    FString SmokeSlot;
    float HeroAnimationRemaining = 0;
    UFUNCTION() void EndRun(bool Escaped);
    void Restart();
    void Attack(bool Heavy);
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
