// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseEnemy.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Perception/PawnSensingComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/DamageEvents.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ABaseEnemy::ABaseEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// 기본 컴포넌트 초기화
	InitializeComponents();
}

void ABaseEnemy::BeginPlay()
{
	Super::BeginPlay();
	
	// 초기화 함수들 호출
	InitializeStats();
	InitializeAI();
	
	// 체력바 업데이트
	UpdateHealthBar();
}

void ABaseEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 타겟이 있을 때 거리 체크 및 상태 업데이트
	if (TargetPlayer && IsAlive())
	{
		float DistanceToTarget = GetDistanceToTarget();
		
		// 공격 범위 내에 있을 때
		if (DistanceToTarget <= AttackRange && CurrentState == EEnemyState::Chasing)
		{
			SetEnemyState(EEnemyState::Attacking);
		}
		// 감지 범위를 벗어났을 때
		else if (DistanceToTarget > DetectionRange && CurrentState == EEnemyState::Chasing)
		{
			StopChasing();
		}
	}
}

void ABaseEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Enemy는 입력이 필요없으므로 비워둠
}

void ABaseEnemy::InitializeComponents()
{
	// PawnSensing 컴포넌트 설정
	PawnSensingComponent = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("PawnSensingComponent"));
	PawnSensingComponent->SetPeripheralVisionAngle(90.0f);
	PawnSensingComponent->SightRadius = DetectionRange;
	PawnSensingComponent->OnSeePawn.AddDynamic(this, &ABaseEnemy::OnPawnSeen);

	// 체력바 위젯 컴포넌트
	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootComponent);
	HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);

	// 캐릭터 설정
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	
	// AI Controller 클래스 설정
	AIControllerClass = AAIController::StaticClass();
}

void ABaseEnemy::InitializeStats()
{
	CurrentHealth = MaxHealth;
	
	// 레벨에 따른 스탯 조정
	if (Level > 1)
	{
		float LevelMultiplier = 1.0f + (Level - 1) * 0.15f; // 레벨당 15% 증가
		MaxHealth *= LevelMultiplier;
		CurrentHealth = MaxHealth;
		AttackDamage *= LevelMultiplier;
		ExperienceReward = 50 + (Level - 1) * 10;
	}

	// 적 등급에 따른 스탯 조정
	switch (EnemyRank)
	{
	case EEnemyRank::Elite:
		MaxHealth *= 2.0f;
		CurrentHealth = MaxHealth;
		AttackDamage *= 1.5f;
		ExperienceReward *= 2;
		break;
	case EEnemyRank::MiniBoss:
		MaxHealth *= 3.0f;
		CurrentHealth = MaxHealth;
		AttackDamage *= 2.0f;
		ExperienceReward *= 3;
		break;
	case EEnemyRank::Boss:
		MaxHealth *= 5.0f;
		CurrentHealth = MaxHealth;
		AttackDamage *= 3.0f;
		ExperienceReward *= 5;
		break;
	default:
		break;
	}

	// 이동 속도 설정
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = MovementSpeed;
	}
}

void ABaseEnemy::InitializeAI()
{
	// AI Controller 가져오기
	EnemyAIController = Cast<AAIController>(GetController());
	
	if (EnemyAIController)
	{
		BlackboardComponent = EnemyAIController->GetBlackboardComponent();
		
		// Behavior Tree 실행
		if (BehaviorTree)
		{
			EnemyAIController->RunBehaviorTree(BehaviorTree);
		}
	}
}

float ABaseEnemy::TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead) return 0.0f;

	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	
	// 간편한 데미지 처리 함수 호출
	ApplyDamage(ActualDamage, DamageCauser);
	
	return ActualDamage;
}

void ABaseEnemy::ApplyDamage(float DamageAmount, AActor* DamageDealer)
{
	if (bIsDead) return;

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	
	// 델리게이트 호출
	OnEnemyTakeDamage.Broadcast(this, DamageAmount);
	
	// 블루프린트 이벤트 호출
	OnTakeDamageEvent(DamageAmount);
	
	// 체력바 업데이트
	UpdateHealthBar();
	
	// 데미지 딜러를 타겟으로 설정 (플레이어인 경우)
	if (DamageDealer && DamageDealer->IsA<APawn>())
	{
		SetTarget(Cast<APawn>(DamageDealer));
		StartChasing();
	}
	
	// 체력이 0 이하면 죽음
	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void ABaseEnemy::Die()
{
	if (bIsDead) return;

	bIsDead = true;
	SetEnemyState(EEnemyState::Dead);

	// AI 중지
	if (EnemyAIController)
	{
		EnemyAIController->BrainComponent->StopLogic("Dead");
	}

	// 충돌 비활성화
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 델리게이트 호출
	OnEnemyDeath.Broadcast(this);
	
	// 블루프린트 이벤트 호출
	OnDeathEvent();

	// 일정 시간 후 액터 삭제 (선택사항)
	FTimerHandle DestroyTimerHandle;
	GetWorldTimerManager().SetTimer(DestroyTimerHandle, [this]()
	{
		Destroy();
	}, 5.0f, false);
}

void ABaseEnemy::Attack()
{
	if (!CanAttack() || !TargetPlayer) return;

	// 공격 쿨다운 시작
	bCanAttack = false;
	GetWorldTimerManager().SetTimer(AttackCooldownHandle, this, &ABaseEnemy::OnAttackCooldownEnd, AttackCooldown, false);

	// 블루프린트 이벤트 호출
	OnAttackEvent();

	// 타겟에게 데미지 (기본 구현 - 자식 클래스에서 오버라이드 가능)
	if (GetDistanceToTarget() <= AttackRange)
	{
		// 여기서 실제 데미지 적용 로직 구현
		// 예: TargetPlayer->TakeDamage(...);
	}
}

void ABaseEnemy::SetEnemyState(EEnemyState NewState)
{
	if (CurrentState == NewState) return;

	EEnemyState OldState = CurrentState;
	CurrentState = NewState;

	// 델리게이트 호출
	OnEnemyStateChanged.Broadcast(NewState);
	
	// 블루프린트 이벤트 호출
	OnStateChangedEvent(NewState);

	// 블랙보드 업데이트
	if (BlackboardComponent)
	{
		BlackboardComponent->SetValueAsEnum("EnemyState", (uint8)NewState);
	}
}

void ABaseEnemy::StunEnemy(float Duration)
{
	if (bIsDead) return;

	bIsStunned = true;
	float ActualDuration = Duration > 0.0f ? Duration : StunDuration;

	SetEnemyState(EEnemyState::Stunned);

	// AI 일시 정지
	if (EnemyAIController)
	{
		EnemyAIController->BrainComponent->PauseLogic("Stunned");
	}

	// 스턴 타이머 설정
	GetWorldTimerManager().SetTimer(StunTimerHandle, this, &ABaseEnemy::OnStunEnd, ActualDuration, false);
}

void ABaseEnemy::SetTarget(APawn* NewTarget)
{
	TargetPlayer = NewTarget;

	// 블랙보드 업데이트
	if (BlackboardComponent && TargetPlayer)
	{
		BlackboardComponent->SetValueAsObject("TargetPlayer", TargetPlayer);
		OnTargetFoundEvent(TargetPlayer);
	}
	else if (BlackboardComponent)
	{
		BlackboardComponent->ClearValue("TargetPlayer");
		OnTargetLostEvent();
	}
}

void ABaseEnemy::StartChasing()
{
	if (bIsDead || bIsStunned || !TargetPlayer) return;

	SetEnemyState(EEnemyState::Chasing);

	// AI 재시작
	if (EnemyAIController && EnemyAIController->BrainComponent->IsPaused())
	{
		EnemyAIController->BrainComponent->ResumeLogic("StartChasing");
	}
}

void ABaseEnemy::StopChasing()
{
	SetTarget(nullptr);
	SetEnemyState(EEnemyState::Idle);
}

float ABaseEnemy::GetDistanceToTarget() const
{
	if (!TargetPlayer) return FLT_MAX;
	
	return FVector::Dist(GetActorLocation(), TargetPlayer->GetActorLocation());
}

void ABaseEnemy::OnPawnSeen(APawn* SeenPawn)
{
	// 플레이어인지 확인 (태그 또는 클래스로 판단)
	if (SeenPawn && SeenPawn->ActorHasTag("Player"))
	{
		SetTarget(SeenPawn);
		if (TargetPlayer) // TargetPlayer가 유효한지 확인
		{
			StartChasing();
		}
	}
}

void ABaseEnemy::OnPawnLost(APawn* LostPawn)
{
	if (LostPawn == TargetPlayer)
	{
		// 일정 시간 후에 타겟을 잃도록 설정 (즉시 잃지 않음)
		FTimerHandle LoseTargetTimer;
		GetWorldTimerManager().SetTimer(LoseTargetTimer, [this]()
		{
			StopChasing();
		}, 3.0f, false);
	}
}

void ABaseEnemy::UpdateHealthBar()
{
	// 체력바 위젯 업데이트 (블루프린트에서 구현)
	if (HealthBarWidget && HealthBarWidget->GetWidget())
	{
		// 블루프린트 위젯에서 UpdateHealthBar 함수 호출 등
	}
}

void ABaseEnemy::OnStunEnd()
{
	bIsStunned = false;
	
	// AI 재시작
	if (EnemyAIController && EnemyAIController->BrainComponent->IsPaused())
	{
		EnemyAIController->BrainComponent->ResumeLogic("StunEnd");
	}

	// 이전 상태로 복귀 또는 Idle 상태로
	if (TargetPlayer)
	{
		SetEnemyState(EEnemyState::Chasing);
	}
	else
	{
		SetEnemyState(EEnemyState::Idle);
	}
}

void ABaseEnemy::OnAttackCooldownEnd()
{
	bCanAttack = true;
} 