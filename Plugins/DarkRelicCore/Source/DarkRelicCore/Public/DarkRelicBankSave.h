#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DarkRelicBankSave.generated.h"

UCLASS()
class DARKRELICCORE_API UDarkRelicBankSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) int32 Credits = 0;
    UPROPERTY(SaveGame) int32 Upgrade = 0;
    UPROPERTY(SaveGame) TArray<int32> Items;
};
