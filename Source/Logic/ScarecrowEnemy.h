#pragma once

#include "CoreMinimal.h"
#include "BaseEnemy.h"
#include "ScarecrowEnemy.generated.h"

UCLASS()
class LOGIC_API AScarecrowEnemy : public ABaseEnemy
{
    GENERATED_BODY()

public:
    AScarecrowEnemy();

protected:
    virtual void BeginPlay() override;

public:
    // Override functions to disable AI behavior
    virtual void StartChasing() override;
    virtual void Attack() override;

    // Function to reset health for repeated testing
    UFUNCTION(BlueprintCallable, Category = "Testing")
    void ResetHealth();
}; 