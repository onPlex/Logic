// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "BaseEnemy.generated.h"

class UBehaviorTree;
class UBlackboardComponent;
class AAIController;
class UPawnSensingComponent;
class UWidgetComponent;

// 적 상태 열거형
UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Patrolling  UMETA(DisplayName = "Patrolling"), 
	Chasing     UMETA(DisplayName = "Chasing"),
	Attacking   UMETA(DisplayName = "Attacking"),
	Stunned     UMETA(DisplayName = "Stunned"),
	Dead        UMETA(DisplayName = "Dead")
};

// 적 등급 열거형
UENUM(BlueprintType)
enum class EEnemyRank : uint8
{
	Normal      UMETA(DisplayName = "Normal"),
	Elite       UMETA(DisplayName = "Elite"),
	Boss        UMETA(DisplayName = "Boss"),
	MiniBoss    UMETA(DisplayName = "MiniBoss")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDeath, ABaseEnemy*, DeadEnemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyTakeDamage, ABaseEnemy*, Enemy, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyStateChanged, EEnemyState, NewState);

UCLASS(BlueprintType, Blueprintable)
class LOGIC_API ABaseEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	ABaseEnemy();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	// === 스탯 관련 ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy Stats")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Stats")
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Stats")
	float AttackRange = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Stats")
	float DetectionRange = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Stats")
	float MovementSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Stats")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Stats")
	int32 ExperienceReward = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Stats")
	EEnemyRank EnemyRank = EEnemyRank::Normal;

protected:
	// === 상태 관리 ===
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy State")
	EEnemyState CurrentState = EEnemyState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy State")
	bool bIsDead = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy State")
	bool bIsStunned = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy State")
	float StunDuration = 2.0f;

	// === AI 관련 ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	UBehaviorTree* BehaviorTree;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	AAIController* EnemyAIController;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UBlackboardComponent* BlackboardComponent;

	// === 감지 시스템 ===
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Detection")
	UPawnSensingComponent* PawnSensingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Detection")
	APawn* TargetPlayer;

	// === UI 관련 ===
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* HealthBarWidget;

	// === 타이머 ===
	FTimerHandle StunTimerHandle;
	FTimerHandle AttackCooldownHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackCooldown = 2.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bCanAttack = true;

public:
	// === 델리게이트 ===
	UPROPERTY(BlueprintAssignable)
	FOnEnemyDeath OnEnemyDeath;

	UPROPERTY(BlueprintAssignable)
	FOnEnemyTakeDamage OnEnemyTakeDamage;

	UPROPERTY(BlueprintAssignable)
	FOnEnemyStateChanged OnEnemyStateChanged;

	// === 공개 함수 ===
	// Unreal Engine의 표준 TakeDamage 함수 오버라이드
	virtual float TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// 간편한 데미지 함수 (블루프린트용)
	UFUNCTION(BlueprintCallable, Category = "Enemy Combat")
	virtual void ApplyDamage(float DamageAmount, AActor* DamageDealer);

	UFUNCTION(BlueprintCallable, Category = "Enemy Combat")
	virtual void Die();

	UFUNCTION(BlueprintCallable, Category = "Enemy Combat")
	virtual void Attack();

	UFUNCTION(BlueprintCallable, Category = "Enemy State")
	virtual void SetEnemyState(EEnemyState NewState);

	UFUNCTION(BlueprintCallable, Category = "Enemy State")
	virtual void StunEnemy(float Duration = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Enemy AI")
	virtual void SetTarget(APawn* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "Enemy AI")
	virtual void StartChasing();

	UFUNCTION(BlueprintCallable, Category = "Enemy AI")
	virtual void StopChasing();

	// === Getter 함수들 ===
	UFUNCTION(BlueprintPure, Category = "Enemy Stats")
	float GetHealthPercentage() const { return CurrentHealth / MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Enemy Stats")
	bool IsAlive() const { return !bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Enemy State")
	EEnemyState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Enemy AI")
	APawn* GetTarget() const { return TargetPlayer; }

	UFUNCTION(BlueprintPure, Category = "Enemy Combat")
	bool CanAttack() const { return bCanAttack && !bIsDead && !bIsStunned; }

	UFUNCTION(BlueprintPure, Category = "Enemy Combat")
	float GetDistanceToTarget() const;

protected:
	// === 보호된 함수들 ===
	UFUNCTION()
	virtual void OnPawnSeen(APawn* SeenPawn);

	UFUNCTION()
	virtual void OnPawnLost(APawn* LostPawn);

	virtual void InitializeStats();
	virtual void InitializeAI();
	virtual void InitializeComponents();

	virtual void UpdateHealthBar();
	virtual void OnStunEnd();
	virtual void OnAttackCooldownEnd();

	// === 블루프린트 이벤트 ===
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy Events")
	void OnDeathEvent();

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy Events")
	void OnTakeDamageEvent(float Damage);

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy Events")
	void OnAttackEvent();

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy Events")
	void OnStateChangedEvent(EEnemyState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy Events")
	void OnTargetFoundEvent(APawn* Target);

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy Events")
	void OnTargetLostEvent();
}; 