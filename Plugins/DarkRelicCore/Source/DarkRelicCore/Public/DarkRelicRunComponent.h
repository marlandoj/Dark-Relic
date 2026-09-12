#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Portable/DarkRelicRules.h"
#include "DarkRelicRunComponent.generated.h"

UENUM(BlueprintType)
enum class EDarkRelicPhase : uint8 { Ready, Running, Extracting, Escaped, Dead };
UENUM(BlueprintType)
enum class EDarkRelicAction : uint8 { None, Light, Heavy, Dodge, Heal, RelicBurst, Rally };
UENUM(BlueprintType)
enum class EDarkRelicItem : uint8 { Iron, Tallow, Salt, Blackbell };

USTRUCT(BlueprintType)
struct DARKRELICCORE_API FDarkRelicTuning
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float Health = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float Stamina = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float StaminaRegen = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float LightCost = 15;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float HeavyCost = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float DodgeCost = 25;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float LightSeconds = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float HeavySeconds = 0.9f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float DodgeSeconds = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float InvulnerableSeconds = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float HealSeconds = 0.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float HealAmount = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") int32 HealCharges = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") float ExtractionSeconds = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float ComboWindow = 0.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float FinisherDamage = 50;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float FinisherCost = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float FinisherSeconds = 0.7f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float BurstDamage = 45;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float BurstCost = 35;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float BurstSeconds = 0.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float BurstCooldown = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float RallyCost = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float RallySeconds = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float RallyDuration = 6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float RallyCooldown = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float RallyDamageMultiplier = 1.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic|Warden") float RallyDamageTaken = 0.75f;
};

USTRUCT(BlueprintType)
struct DARKRELICCORE_API FDarkRelicSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") EDarkRelicPhase Phase = EDarkRelicPhase::Ready;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") EDarkRelicAction Action = EDarkRelicAction::None;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") float Health = 100;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") float MaxHealth = 100;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") float Stamina = 100;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") float MaxStamina = 100;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") float ActionRemaining = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") float InvulnerableRemaining = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") float ExtractionRemaining = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") int32 Heals = 2;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") bool InZone = false;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") TArray<int32> Carried;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") TArray<int32> Banked;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") int32 Credits = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic") int32 Upgrade = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic|Warden") int32 ComboStep = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic|Warden") bool Finisher = false;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic|Warden") float ComboRemaining = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic|Warden") float AttackDamage = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic|Warden") float BurstCooldown = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic|Warden") float RallyCooldown = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dark Relic|Warden") float RallyRemaining = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDarkRelicStateChanged, FDarkRelicSnapshot, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDarkRelicRunEnded, bool, Escaped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDarkRelicActionStarted, EDarkRelicAction, Action);

UCLASS(ClassGroup=(DarkRelic), meta=(BlueprintSpawnableComponent))
class DARKRELICCORE_API UDarkRelicRunComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UDarkRelicRunComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") FDarkRelicTuning Tuning;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Relic") FString SaveSlot = TEXT("DarkRelicBank");
    UPROPERTY(BlueprintAssignable, Category="Dark Relic") FDarkRelicStateChanged OnStateChanged;
    UPROPERTY(BlueprintAssignable, Category="Dark Relic") FDarkRelicRunEnded OnRunEnded;
    UPROPERTY(BlueprintAssignable, Category="Dark Relic") FDarkRelicActionStarted OnActionStarted;
    UFUNCTION(BlueprintCallable, Category="Dark Relic") bool ApplyTuning();
    UFUNCTION(BlueprintCallable, Category="Dark Relic") bool StartRun();
    UFUNCTION(BlueprintCallable, Category="Dark Relic") bool TryAction(EDarkRelicAction Action);
    UFUNCTION(BlueprintCallable, Category="Dark Relic") bool ReceiveDamage(float Amount);
    UFUNCTION(BlueprintCallable, Category="Dark Relic") bool CollectLoot(EDarkRelicItem Item, int32 Count, int32 PickupId);
    UFUNCTION(BlueprintCallable, Category="Dark Relic") bool SetInExtractionZone(bool Present);
    UFUNCTION(BlueprintCallable, Category="Dark Relic") bool BeginExtraction();
    UFUNCTION(BlueprintCallable, Category="Dark Relic") bool BuyUpgrade();
    UFUNCTION(BlueprintCallable, Category="Dark Relic") bool SaveBank();
    UFUNCTION(BlueprintCallable, Category="Dark Relic") bool LoadBank();
    UFUNCTION(BlueprintPure, Category="Dark Relic") FDarkRelicSnapshot GetSnapshot() const;
protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
    void Publish(dark_relic::Phase Previous);
    dark_relic::Rules Rules;
};
