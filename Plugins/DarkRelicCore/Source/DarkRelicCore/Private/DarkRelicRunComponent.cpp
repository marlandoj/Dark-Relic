#include "DarkRelicRunComponent.h"
#include "DarkRelicBankSave.h"
#include "Kismet/GameplayStatics.h"

UDarkRelicRunComponent::UDarkRelicRunComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

bool UDarkRelicRunComponent::ApplyTuning()
{
    dark_relic::Tuning T;
    T.health = Tuning.Health; T.stamina = Tuning.Stamina; T.stamina_regen = Tuning.StaminaRegen;
    T.light_cost = Tuning.LightCost; T.heavy_cost = Tuning.HeavyCost; T.dodge_cost = Tuning.DodgeCost;
    T.light_seconds = Tuning.LightSeconds; T.heavy_seconds = Tuning.HeavySeconds;
    T.dodge_seconds = Tuning.DodgeSeconds; T.invulnerable_seconds = Tuning.InvulnerableSeconds;
    T.heal_seconds = Tuning.HealSeconds; T.heal_amount = Tuning.HealAmount;
    T.heal_charges = Tuning.HealCharges;
    T.extraction_seconds = Tuning.ExtractionSeconds;
    const bool Ok = Rules.configure(T);
    if (Ok) OnStateChanged.Broadcast(GetSnapshot());
    return Ok;
}

bool UDarkRelicRunComponent::StartRun()
{
    if (!ApplyTuning() || !Rules.start()) return false;
    OnStateChanged.Broadcast(GetSnapshot());
    return true;
}

void UDarkRelicRunComponent::Publish(dark_relic::Phase Previous)
{
    const auto Current = Rules.snapshot().phase;
    OnStateChanged.Broadcast(GetSnapshot());
    if (Previous != Current && (Current == dark_relic::Phase::Escaped || Current == dark_relic::Phase::Dead))
        OnRunEnded.Broadcast(Current == dark_relic::Phase::Escaped);
}

void UDarkRelicRunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!Rules.live()) return;
    const auto Previous = Rules.snapshot().phase;
    if (Rules.tick(DeltaTime)) Publish(Previous);
}

bool UDarkRelicRunComponent::TryAction(EDarkRelicAction Action)
{
    if (!Rules.act(static_cast<dark_relic::Action>(Action))) return false;
    OnStateChanged.Broadcast(GetSnapshot());
    OnActionStarted.Broadcast(Action);
    return true;
}

bool UDarkRelicRunComponent::ReceiveDamage(float Amount)
{
    const auto Previous = Rules.snapshot().phase;
    if (!Rules.damage(Amount)) return false;
    Publish(Previous);
    return true;
}

bool UDarkRelicRunComponent::CollectLoot(EDarkRelicItem Item, int32 Count, int32 PickupId)
{
    if (PickupId <= 0 || !Rules.pickup(static_cast<dark_relic::Item>(Item), Count, static_cast<std::uint64_t>(PickupId))) return false;
    OnStateChanged.Broadcast(GetSnapshot());
    return true;
}

bool UDarkRelicRunComponent::SetInExtractionZone(bool Present)
{
    const auto Previous = Rules.snapshot().phase;
    if (!Rules.zone(Present)) return false;
    Publish(Previous);
    return true;
}

bool UDarkRelicRunComponent::BeginExtraction()
{
    if (!Rules.extract()) return false;
    OnStateChanged.Broadcast(GetSnapshot());
    return true;
}

bool UDarkRelicRunComponent::BuyUpgrade()
{
    if (!Rules.buy_upgrade()) return false;
    OnStateChanged.Broadcast(GetSnapshot());
    return true;
}

bool UDarkRelicRunComponent::SaveBank()
{
    if (Rules.live() || SaveSlot.IsEmpty()) return false;
    auto* Save = Cast<UDarkRelicBankSave>(UGameplayStatics::CreateSaveGameObject(UDarkRelicBankSave::StaticClass()));
    if (!Save) return false;
    const auto& Bank = Rules.snapshot().bank;
    Save->Version = Bank.version; Save->Credits = Bank.credits; Save->Upgrade = Bank.upgrade;
    for (int Item : Bank.items) Save->Items.Add(Item);
    return UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0);
}

bool UDarkRelicRunComponent::LoadBank()
{
    if (Rules.live() || SaveSlot.IsEmpty()) return false;
    auto* Save = Cast<UDarkRelicBankSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
    if (!Save || Save->Items.Num() != 4) return false;
    dark_relic::Bank Bank;
    Bank.version = Save->Version; Bank.credits = Save->Credits; Bank.upgrade = Save->Upgrade;
    for (int32 I = 0; I < 4; ++I) Bank.items[I] = Save->Items[I];
    if (!Rules.load(Bank)) return false;
    OnStateChanged.Broadcast(GetSnapshot());
    return true;
}

FDarkRelicSnapshot UDarkRelicRunComponent::GetSnapshot() const
{
    const auto& S = Rules.snapshot();
    FDarkRelicSnapshot Result;
    Result.Phase = static_cast<EDarkRelicPhase>(S.phase);
    Result.Action = static_cast<EDarkRelicAction>(S.action);
    Result.Health = static_cast<float>(S.health); Result.MaxHealth = static_cast<float>(S.max_health);
    Result.Stamina = static_cast<float>(S.stamina); Result.MaxStamina = static_cast<float>(Rules.tuning().stamina);
    Result.ActionRemaining = static_cast<float>(S.action_remaining);
    Result.InvulnerableRemaining = static_cast<float>(S.invulnerable_remaining);
    Result.ExtractionRemaining = static_cast<float>(S.extraction_remaining);
    Result.Heals = S.heals; Result.InZone = S.in_zone;
    Result.Credits = S.bank.credits; Result.Upgrade = S.bank.upgrade;
    for (int Item : S.carried) Result.Carried.Add(Item);
    for (int Item : S.bank.items) Result.Banked.Add(Item);
    return Result;
}
