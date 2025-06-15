// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "CameraLockOnComponent.generated.h"

class ABaseEnemy;
class UCameraComponent;
class USpringArmComponent;
class UWidgetComponent;

// 락온 상태 열거형
UENUM(BlueprintType)
enum class ELockOnState : uint8
{
	None        UMETA(DisplayName = "None"),
	Searching   UMETA(DisplayName = "Searching"),
	Locked      UMETA(DisplayName = "Locked")
};

// 락온 이벤트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockOnTargetChanged, AActor*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockOnStateChanged, ELockOnState, NewState);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LOGIC_API UCameraLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCameraLockOnComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// === 락온 설정 ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On Settings")
	float LockOnRange = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On Settings")
	float LockOnAngle = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On Settings")
	float LockOnSwitchRange = 300.0f;  // 타겟 전환 시 검색 범위

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On Settings")
	float CameraTransitionSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On Settings")
	float LockOnBreakDistance = 1500.0f;  // 이 거리 이상 멀어지면 락온 해제

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On Settings")
	bool bAutoLockOnNearestEnemy = false;

	// === 카메라 설정 ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	float CameraAssistanceStrength = 0.3f;  // 카메라 보조 강도 (0.0 = 없음, 1.0 = 완전 고정)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	float CameraAssistanceRadius = 45.0f;   // 카메라 보조 반경 (각도)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	float CameraReturnSpeed = 2.0f;         // 타겟 중심으로 돌아가는 속도

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	float CameraOffsetInterpSpeed = 2.0f;

	// === 상황별 락온 강도 ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	float SkillCastAssistance = 0.8f;       // 스킬 시전 중 보조 강도

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	float CombatAssistance = 0.5f;          // 전투 중 보조 강도

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	float IdleAssistance = 0.2f;            // 평상시 보조 강도

	// === UI 설정 ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Settings")
	TSubclassOf<UUserWidget> LockOnMarkerWidgetClass;

	// 카메라 락온 시 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	float LockedCameraDistance = 300.0f; // 기본 락온 카메라 거리

	// 카메라 중심점 편향
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	float CenterBias = 0.3f; // 기본 중심점 편향

protected:
	// === 현재 상태 ===
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lock On State")
	ELockOnState CurrentLockOnState = ELockOnState::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lock On State")
	AActor* CurrentLockOnTarget = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lock On State")
	TArray<AActor*> PotentialTargets;

	// === 컴포넌트 참조 ===
	UPROPERTY()
	UCameraComponent* PlayerCamera;

	UPROPERTY()
	USpringArmComponent* CameraBoom;

	UPROPERTY()
	APawn* OwnerPawn;

	// === UI 관련 ===
	UPROPERTY()
	UWidgetComponent* LockOnMarkerWidget;

	// === 카메라 원본 설정 저장 ===
	float OriginalCameraDistance;
	FVector OriginalCameraOffset;
	bool bHasStoredOriginalSettings = false;

	// 시야에서 벗어난 시간 추적
	float TimeOutOfView = 0.0f;

	// 시야에서 벗어날 수 있는 최대 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On Settings")
	float MaxTimeOutOfView = 2.0f; // 2초 동안 시야에서 벗어나면 해제

public:
	// === 델리게이트 ===
	UPROPERTY(BlueprintAssignable)
	FOnLockOnTargetChanged OnLockOnTargetChanged;

	UPROPERTY(BlueprintAssignable)
	FOnLockOnStateChanged OnLockOnStateChanged;

	// === 공개 함수 ===
	UFUNCTION(BlueprintCallable, Category = "Lock On")
	void ToggleLockOn();

	UFUNCTION(BlueprintCallable, Category = "Lock On")
	void SetLockOnTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "Lock On")
	void ClearLockOn();

	// UFUNCTION(BlueprintCallable, Category = "Lock On")
	// void SwitchToNextTarget();

	// UFUNCTION(BlueprintCallable, Category = "Lock On")
	// void SwitchToPreviousTarget();

	// UFUNCTION(BlueprintCallable, Category = "Lock On")
	// void SwitchTargetInDirection(FVector2D InputDirection);

	// === Getter 함수들 ===
	UFUNCTION(BlueprintPure, Category = "Lock On")
	bool IsLockedOn() const { return CurrentLockOnState == ELockOnState::Locked && CurrentLockOnTarget != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Lock On")
	AActor* GetCurrentTarget() const { return CurrentLockOnTarget; }

	UFUNCTION(BlueprintPure, Category = "Lock On")
	ELockOnState GetLockOnState() const { return CurrentLockOnState; }

	UFUNCTION(BlueprintPure, Category = "Lock On")
	FVector GetLockOnDirection() const;

	UFUNCTION(BlueprintPure, Category = "Lock On")
	float GetDistanceToTarget() const;

protected:
	// === 내부 함수들 ===
	void InitializeComponents();
	void FindPotentialTargets();
	AActor* FindBestLockOnTarget();
	AActor* FindNearestTarget();
	AActor* FindTargetInDirection(FVector2D Direction);

	void UpdateCameraForLockOn(float DeltaTime);
	void UpdateLockOnMarker();

	void SetLockOnState(ELockOnState NewState);
	void StoreOriginalCameraSettings();
	void RestoreOriginalCameraSettings();

	bool IsValidLockOnTarget(AActor* Target) const;
	bool IsTargetInRange(AActor* Target) const;
	bool IsTargetInView(AActor* Target) const;

	// === 블루프린트 이벤트 ===
	UFUNCTION(BlueprintImplementableEvent, Category = "Lock On Events")
	void OnLockOnActivated(AActor* Target);

	UFUNCTION(BlueprintImplementableEvent, Category = "Lock On Events")
	void OnLockOnDeactivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Lock On Events")
	void OnTargetSwitched(AActor* OldTarget, AActor* NewTarget);

	UFUNCTION(BlueprintImplementableEvent, Category = "Lock On Events")
	void OnTargetLost(AActor* LostTarget);
}; 