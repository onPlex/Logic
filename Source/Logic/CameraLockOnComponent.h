#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CameraLockOnComponent.generated.h"

// 열거형을 먼저 선언
UENUM(BlueprintType)
enum class ELockOnState : uint8
{
    None,
    Searching,
    Locked
};

// 열거형 선언 후 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockOnTargetChanged, AActor*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockOnStateChanged, ELockOnState, NewState);

class ABaseEnemy;
class UCameraComponent;
class USpringArmComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LOGIC_API UCameraLockOnComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCameraLockOnComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Camera|LockOn")
    void ToggleLockOn();

    UFUNCTION(BlueprintCallable, Category = "Camera|LockOn")
    void SwitchTarget(const FVector2D& InputDirection);

    UFUNCTION(BlueprintCallable, Category = "Camera|LockOn")
    bool IsLockedOn() const { return CurrentLockOnState == ELockOnState::Locked; }

    UFUNCTION(BlueprintCallable, Category = "Camera|LockOn")
    AActor* GetCurrentTarget() const { return CurrentTarget; }

    UFUNCTION(BlueprintCallable, Category = "Camera|LockOn")
    ELockOnState GetLockOnState() const { return CurrentLockOnState; }

    // 락온 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|LockOn|Settings")
    float LockOnRadius = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|LockOn|Settings")
    float LockOnAngle = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|LockOn|Settings")
    float LockOnBreakDistance = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|LockOn|Settings")
    float LockOnSearchInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|LockOn|Settings")
    float MaxTimeOutOfView = 1.0f;

    // 카메라 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
    float CenterBias = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
    float HeightOffset = 170.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
    float TargetArmLength = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
    float ArmLengthInterpSpeed = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
    float CameraRotationInterpSpeed = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
    float CameraPitchMin = -30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
    float CameraPitchMax = 60.0f;

    // 카메라 거리 설정
    UPROPERTY(EditAnywhere, Category = "Camera|Distance", meta = (ClampMin = "100.0", ClampMax = "500.0"))
    float MinCameraDistance = 200.0f;

    UPROPERTY(EditAnywhere, Category = "Camera|Distance", meta = (ClampMin = "300.0", ClampMax = "1000.0"))
    float MaxCameraDistance = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Camera|Distance", meta = (ClampMin = "50.0", ClampMax = "200.0"))
    float MinLockOnDistance = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Camera|Distance", meta = (ClampMin = "500.0", ClampMax = "2000.0"))
    float MaxLockOnDistance = 1000.0f;

    // 디버그 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Debug")
    bool bShowDebugInfo = false;

    // 이벤트
    UPROPERTY(BlueprintAssignable, Category = "Camera|Events")
    FOnLockOnTargetChanged OnLockOnTargetChanged;

    UPROPERTY(BlueprintAssignable, Category = "Camera|Events")
    FOnLockOnStateChanged OnLockOnStateChanged;

protected:
    UPROPERTY()
    USpringArmComponent* SpringArm;

    UPROPERTY()
    UCameraComponent* Camera;

    UPROPERTY()
    APawn* OwnerPawn;

    UPROPERTY()
    UCameraComponent* CombatCamera;

    UPROPERTY()
    UCameraComponent* MoveCamera;

    UPROPERTY()
    ELockOnState CurrentLockOnState;

    UPROPERTY()
    AActor* CurrentTarget;

    UPROPERTY()
    TArray<AActor*> PotentialTargets;

    float SearchTimer;
    float TimeOutOfView;

    void FindPotentialTargets();
    AActor* FindBestTarget();
    bool IsValidTarget(AActor* Target) const;
    bool IsTargetInRange(AActor* Target) const;
    bool IsTargetInView(AActor* Target) const;
    void UpdateCameraLockOn(float DeltaTime);
    void RestoreOriginalCameraSettings();
    void DrawDebugInfo();
};
