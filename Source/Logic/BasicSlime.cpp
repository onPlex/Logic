// Copyright Epic Games, Inc. All Rights Reserved.

#include "BasicSlime.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"

ABasicSlime::ABasicSlime()
{
	// 슬라임 기본 스탯 설정
	MaxHealth = 60.0f;
	AttackDamage = 15.0f;
	AttackRange = 120.0f;
	DetectionRange = 600.0f;
	MovementSpeed = 250.0f;
	EnemyRank = EEnemyRank::Normal;

	// 슬라임 물리 설정
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 1.2f;
		GetCharacterMovement()->JumpZVelocity = 400.0f;
		GetCharacterMovement()->AirControl = 0.2f;
	}

	// 캐릭터 크기 조정 (슬라임은 작음)
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCapsuleSize(35.0f, 60.0f);
	}
}

void ABasicSlime::BeginPlay()
{
	Super::BeginPlay();
	
	// 바운스 효과 시작
	StartBounceEffect();
}

void ABasicSlime::Attack()
{
	if (!CanAttack() || !TargetPlayer) return;

	// 50% 확률로 점프 공격 또는 일반 공격
	if (bCanJumpAttack && FMath::RandBool())
	{
		PerformJumpAttack();
	}
	else
	{
		// 기본 공격 수행
		Super::Attack();
		
		// 기본 공격 시 타겟에게 데미지 적용
		if (GetDistanceToTarget() <= AttackRange)
		{
			// 여기서 플레이어에게 실제 데미지를 적용
			// 예시: UGameplayStatics::ApplyDamage(TargetPlayer, AttackDamage, GetController(), this, UDamageType::StaticClass());
			
			UE_LOG(LogTemp, Warning, TEXT("BasicSlime dealt %f damage to player!"), AttackDamage);
		}
	}
}

void ABasicSlime::PerformJumpAttack()
{
	if (!TargetPlayer || bIsJumpAttacking) return;

	bIsJumpAttacking = true;
	bCanJumpAttack = false;

	// AI 일시 정지 (점프 공격 중)
	if (EnemyAIController)
	{
		EnemyAIController->BrainComponent->PauseLogic("JumpAttack");
	}

	// 타겟 방향으로 점프
	FVector TargetLocation = TargetPlayer->GetActorLocation();
	FVector CurrentLocation = GetActorLocation();
	FVector JumpDirection = (TargetLocation - CurrentLocation).GetSafeNormal();

	// 점프 힘 적용
	if (GetCharacterMovement())
	{
		FVector JumpVelocity = JumpDirection * JumpAttackForce + FVector(0, 0, JumpAttackForce * 0.8f);
		GetCharacterMovement()->AddImpulse(JumpVelocity, true);
	}

	// 착지 감지를 위한 타이머 설정
	GetWorldTimerManager().SetTimer(JumpAttackTimerHandle, this, &ABasicSlime::HandleJumpLanding, 0.1f, true);

	// 점프 공격 쿨다운 타이머
	FTimerHandle JumpCooldownTimer;
	GetWorldTimerManager().SetTimer(JumpCooldownTimer, this, &ABasicSlime::OnJumpAttackCooldownEnd, JumpAttackCooldown, false);

	// 블루프린트 이벤트 호출
	OnAttackEvent();
}

void ABasicSlime::HandleJumpLanding()
{
	// 땅에 착지했는지 확인
	if (GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround())
	{
		bIsJumpAttacking = false;
		
		// 타이머 정리
		GetWorldTimerManager().ClearTimer(JumpAttackTimerHandle);

		// AI 재개
		if (EnemyAIController && EnemyAIController->BrainComponent->IsPaused())
		{
			EnemyAIController->BrainComponent->ResumeLogic("JumpAttackEnd");
		}

		// 착지 지점 주변의 적들에게 데미지
		TArray<FHitResult> HitResults;
		FVector StartLocation = GetActorLocation();
		FVector EndLocation = StartLocation + FVector(0, 0, -10);

		// 구체 형태로 충돌 감지
		FCollisionShape CollisionShape = FCollisionShape::MakeSphere(JumpAttackRadius);
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		bool bHit = GetWorld()->SweepMultiByChannel(
			HitResults,
			StartLocation,
			EndLocation,
			FQuat::Identity,
			ECollisionChannel::ECC_Pawn,
			CollisionShape,
			QueryParams
		);

		if (bHit)
		{
			for (const FHitResult& Hit : HitResults)
			{
				if (AActor* HitActor = Hit.GetActor())
				{
					// 플레이어에게 데미지 적용
					if (HitActor->ActorHasTag("Player"))
					{
						// 실제 데미지 적용
						// UGameplayStatics::ApplyDamage(HitActor, AttackDamage * 1.5f, GetController(), this, UDamageType::StaticClass());
						
						UE_LOG(LogTemp, Warning, TEXT("BasicSlime jump attack hit player for %f damage!"), AttackDamage * 1.5f);
					}
				}
			}
		}

		// 디버그 시각화 (개발 중에만)
		#if WITH_EDITOR
		if (GetWorld())
		{
			DrawDebugSphere(GetWorld(), StartLocation, JumpAttackRadius, 12, FColor::Red, false, 2.0f);
		}
		#endif
	}
}

void ABasicSlime::OnJumpAttackCooldownEnd()
{
	bCanJumpAttack = true;
}

void ABasicSlime::StartBounceEffect()
{
	// 주기적으로 살짝 바운스하는 효과
	GetWorldTimerManager().SetTimer(BounceTimerHandle, this, &ABasicSlime::PerformBounce, 
		FMath::RandRange(2.0f, 4.0f), true);
}

void ABasicSlime::PerformBounce()
{
	// 죽었거나 공격 중이면 바운스하지 않음
	if (bIsDead || bIsJumpAttacking || CurrentState == EEnemyState::Attacking) 
		return;

	// 땅에 있을 때만 바운스
	if (GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround())
	{
		// 작은 점프로 바운스 효과
		FVector BounceVelocity = FVector(0, 0, BounceHeight);
		GetCharacterMovement()->AddImpulse(BounceVelocity, true);
	}

	// 다음 바운스 시간 랜덤 설정
	GetWorldTimerManager().SetTimer(BounceTimerHandle, this, &ABasicSlime::PerformBounce, 
		FMath::RandRange(2.0f, 4.0f), false);
} 