// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseEnemy.h"
#include "BasicSlime.generated.h"

/**
 * 기본 슬라임 몬스터 클래스
 * 원신 스타일의 간단한 적 예시
 */
UCLASS(BlueprintType, Blueprintable)
class LOGIC_API ABasicSlime : public ABaseEnemy
{
	GENERATED_BODY()

public:
	ABasicSlime();

protected:
	virtual void BeginPlay() override;

	// 공격 오버라이드
	virtual void Attack() override;

	// 점프 공격 관련
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime Combat")
	float JumpAttackForce = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime Combat")
	float JumpAttackRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime Combat")
	bool bCanJumpAttack = true;

	// 점프 공격 실행
	UFUNCTION(BlueprintCallable, Category = "Slime Combat")
	void PerformJumpAttack();

	// 착지 시 데미지 처리
	UFUNCTION(BlueprintCallable, Category = "Slime Combat")
	void HandleJumpLanding();

	// 슬라임 특성
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime Properties")
	float BounceHeight = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime Properties")
	float BounceDamping = 0.5f;

private:
	// 점프 공격 타이머
	FTimerHandle JumpAttackTimerHandle;
	
	// 점프 공격 중인지 확인
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime State", meta = (AllowPrivateAccess = "true"))
	bool bIsJumpAttacking = false;

	// 점프 공격 쿨다운
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime Combat", meta = (AllowPrivateAccess = "true"))
	float JumpAttackCooldown = 3.0f;

	// 점프 공격 쿨다운 완료
	void OnJumpAttackCooldownEnd();

	// 주기적인 바운스 효과
	void StartBounceEffect();
	
	FTimerHandle BounceTimerHandle;
	void PerformBounce();
}; 