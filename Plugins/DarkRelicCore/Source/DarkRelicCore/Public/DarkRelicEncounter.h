#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "DarkRelicRunComponent.h"
#include "DarkRelicEncounter.generated.h"

class ACharacter;
class APlayerController;

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
