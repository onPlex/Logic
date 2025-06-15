// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraLockOnComponent.h"
#include "BaseEnemy.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "LogicCharacter.h"

UCameraLockOnComponent::UCameraLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UCameraLockOnComponent::BeginPlay()
{
	Super::BeginPlay();
	
	InitializeComponents();
}

void UCameraLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CurrentLockOnState == ELockOnState::Locked)
	{
		// 현재 타겟이 유효한지 확인
		if (!IsValidLockOnTarget(CurrentLockOnTarget))
		{
			ClearLockOn();
			return;
		}

		// 타겟이 너무 멀어졌는지 확인
		if (GetDistanceToTarget() > LockOnBreakDistance)
		{
			ClearLockOn();
			return;
		}

		// 타겟이 시야에서 벗어났는지 확인
		if (!IsTargetInView(CurrentLockOnTarget))
		{
			// 일정 시간 동안 시야에서 벗어나면 해제
			if (TimeOutOfView >= MaxTimeOutOfView)
			{
				ClearLockOn();
				return;
			}
			else
			{
				TimeOutOfView += DeltaTime;
			}
		}
		else
		{
			TimeOutOfView = 0.0f; // 시야 내에 있으면 타이머 초기화
		}

		// 카메라 업데이트
		UpdateCameraForLockOn(DeltaTime);
		
		// UI 마커 업데이트
		UpdateLockOnMarker();
	}
	else if (bAutoLockOnNearestEnemy && CurrentLockOnState == ELockOnState::None)
	{
		// 자동 락온 기능이 켜져 있으면 주변 적 자동 락온
		AActor* NearestEnemy = FindNearestTarget();
		if (NearestEnemy)
		{
			SetLockOnTarget(NearestEnemy);
		}
	}
}

void UCameraLockOnComponent::InitializeComponents()
{
	OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	// SpringArm 컴포넌트 찾기
	CameraBoom = OwnerPawn->FindComponentByClass<USpringArmComponent>();
	
	// 카메라 컴포넌트 찾기
	PlayerCamera = OwnerPawn->FindComponentByClass<UCameraComponent>();
	
	// 원본 카메라 설정 저장
	StoreOriginalCameraSettings();
}

void UCameraLockOnComponent::ToggleLockOn()
{
	if (CurrentLockOnState == ELockOnState::Locked)
	{
		UE_LOG(LogTemp, Log, TEXT("Lock On State: Locked -> Clearing Lock On"));
		ClearLockOn();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Lock On State: None -> Finding Potential Targets"));
		FindPotentialTargets();
		AActor* BestTarget = FindBestLockOnTarget();
		if (BestTarget)
		{
			UE_LOG(LogTemp, Log, TEXT("Lock On State: Setting New Target"));
			SetLockOnTarget(BestTarget);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Lock On State: No Valid Target Found"));
		}
	}
}

void UCameraLockOnComponent::SetLockOnTarget(AActor* NewTarget)
{
	if (!IsValidLockOnTarget(NewTarget))
	{
		return;
	}

	AActor* OldTarget = CurrentLockOnTarget;
	CurrentLockOnTarget = NewTarget;
	SetLockOnState(ELockOnState::Locked);

	// 전투 카메라 활성화
	if (OwnerPawn)
	{
		ALogicCharacter* LogicCharacter = Cast<ALogicCharacter>(OwnerPawn);
		if (LogicCharacter)
		{
			APlayerController* PlayerController = Cast<APlayerController>(OwnerPawn->GetController());
			if (PlayerController)
			{
				PlayerController->SetViewTargetWithBlend(LogicCharacter, 0.5f, EViewTargetBlendFunction::VTBlend_Cubic);
			}
		}
	}

	// 델리게이트 호출
	OnLockOnTargetChanged.Broadcast(NewTarget);
	
	// 블루프린트 이벤트 호출
	if (OldTarget != NewTarget)
	{
		if (OldTarget)
		{
			OnTargetSwitched(OldTarget, NewTarget);
		}
		else
		{
			OnLockOnActivated(NewTarget);
		}
	}

	// 락온 된 대상의 이름을 디버그 메시지로 출력
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Locked On Target: %s"), *NewTarget->GetName()));
	}

	UE_LOG(LogTemp, Log, TEXT("Lock On Target Set: %s"), NewTarget ? *NewTarget->GetName() : TEXT("None"));
}

void UCameraLockOnComponent::ClearLockOn()
{
	AActor* OldTarget = CurrentLockOnTarget;
	CurrentLockOnTarget = nullptr;
	SetLockOnState(ELockOnState::None);

	// 카메라 원본 설정 복원
	RestoreOriginalCameraSettings();

	// 델리게이트 호출
	OnLockOnTargetChanged.Broadcast(nullptr);
	
	// 블루프린트 이벤트 호출
	OnLockOnDeactivated();

	// 기본 3인칭 카메라 상태로 복구
	if (OwnerPawn)
	{
		ALogicCharacter* LogicCharacter = Cast<ALogicCharacter>(OwnerPawn);
		if (LogicCharacter)
		{
			APlayerController* PlayerController = Cast<APlayerController>(OwnerPawn->GetController());
			if (PlayerController)
			{
				// 전투 카메라 비활성화 및 이동 카메라 활성화
				LogicCharacter->GetCombatCamera()->Deactivate();
				LogicCharacter->GetMoveCamera()->Activate();

				// 이동 카메라로 부드럽게 전환
				PlayerController->SetViewTargetWithBlend(LogicCharacter, 0.5f, EViewTargetBlendFunction::VTBlend_Cubic);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Lock On Cleared"));
}

FVector UCameraLockOnComponent::GetLockOnDirection() const
{
	if (!CurrentLockOnTarget || !OwnerPawn) return FVector::ZeroVector;
	
	FVector Direction = CurrentLockOnTarget->GetActorLocation() - OwnerPawn->GetActorLocation();
	return Direction.GetSafeNormal();
}

float UCameraLockOnComponent::GetDistanceToTarget() const
{
	if (!CurrentLockOnTarget || !OwnerPawn) return 0.0f;
	
	return FVector::Dist(OwnerPawn->GetActorLocation(), CurrentLockOnTarget->GetActorLocation());
}

void UCameraLockOnComponent::FindPotentialTargets()
{
	PotentialTargets.Empty();
	
	if (!OwnerPawn) return;

	// 월드의 모든 BaseEnemy 찾기
	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseEnemy::StaticClass(), FoundEnemies);

	for (AActor* Enemy : FoundEnemies)
	{
		if (IsValidLockOnTarget(Enemy) && IsTargetInRange(Enemy) && IsTargetInView(Enemy))
		{
			PotentialTargets.Add(Enemy);
		}
	}

	// 거리 순으로 정렬
	PotentialTargets.Sort([this](const AActor& A, const AActor& B)
	{
		float DistA = FVector::Dist(OwnerPawn->GetActorLocation(), A.GetActorLocation());
		float DistB = FVector::Dist(OwnerPawn->GetActorLocation(), B.GetActorLocation());
		return DistA < DistB;
	});
}

AActor* UCameraLockOnComponent::FindBestLockOnTarget()
{
	if (PotentialTargets.Num() == 0) return nullptr;
	
	// 가장 가까운 적을 반환
	return PotentialTargets[0];
}

AActor* UCameraLockOnComponent::FindNearestTarget()
{
	FindPotentialTargets();
	return FindBestLockOnTarget();
}

AActor* UCameraLockOnComponent::FindTargetInDirection(FVector2D Direction)
{
	if (!OwnerPawn || !PlayerCamera) return nullptr;

	FindPotentialTargets();
	if (PotentialTargets.Num() == 0) return nullptr;

	AActor* BestTarget = nullptr;
	float BestScore = -1.0f;

	FVector CameraForward = PlayerCamera->GetForwardVector();
	FVector CameraRight = PlayerCamera->GetRightVector();
	FVector InputDirection3D = (CameraRight * Direction.X + CameraForward * Direction.Y).GetSafeNormal();

	for (AActor* Target : PotentialTargets)
	{
		if (Target == CurrentLockOnTarget) continue;

		FVector ToTarget = (Target->GetActorLocation() - OwnerPawn->GetActorLocation()).GetSafeNormal();
		float DotProduct = FVector::DotProduct(InputDirection3D, ToTarget);
		
		// 입력 방향과 타겟 방향의 일치도 + 거리 고려
		float Distance = FVector::Dist(OwnerPawn->GetActorLocation(), Target->GetActorLocation());
		float Score = DotProduct - (Distance / LockOnSwitchRange);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Target;
		}
	}

	return BestTarget;
}

void UCameraLockOnComponent::UpdateCameraForLockOn(float DeltaTime)
{
	if (!CameraBoom || !CurrentLockOnTarget || !OwnerPawn) return;

	// 타겟 위치 계산
	FVector TargetLocation = CurrentLockOnTarget->GetActorLocation();
	FVector OwnerLocation = OwnerPawn->GetActorLocation();

	// 중심점 계산
	FVector CenterPosition = FMath::Lerp(OwnerLocation, TargetLocation, CenterBias);

	// 카메라 방향 벡터 계산
	FVector DirectionToTarget = (TargetLocation - CenterPosition).GetSafeNormal();

	// 카메라 위치 계산
	FVector CameraPosition = CenterPosition
							- DirectionToTarget * FMath::Lerp(350.0f, 450.0f, 0.5f)
							+ FVector::UpVector * FMath::Lerp(100.0f, 150.0f, 0.5f);

	// 카메라 위치를 화면의 70% 영역 내에 유지
	FVector2D ViewportSize;
	GEngine->GameViewport->GetViewportSize(ViewportSize);
	FVector2D ScreenPosition;
	UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), CenterPosition, ScreenPosition);

	float ScreenCenterX = ViewportSize.X * 0.5f;
	float ScreenCenterY = ViewportSize.Y * 0.5f;
	float MaxOffsetX = ScreenCenterX * 0.7f;
	float MaxOffsetY = ScreenCenterY * 0.7f;

	if (FMath::Abs(ScreenPosition.X - ScreenCenterX) > MaxOffsetX || FMath::Abs(ScreenPosition.Y - ScreenCenterY) > MaxOffsetY)
	{
		CenterPosition = FMath::Lerp(OwnerLocation, TargetLocation, 0.5f);
	}

	// 목표 회전값 계산 (타겟을 바라보는 방향)
	FRotator TargetRotation = UKismetMathLibrary::FindLookAtRotation(CameraPosition, TargetLocation);

	// 현재 카메라 회전값
	FRotator CurrentRotation = CameraBoom->GetComponentRotation();

	// 부드럽게 회전 보간
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, CameraTransitionSpeed);

	// 회전 적용
	if (OwnerPawn->GetController())
	{
		OwnerPawn->GetController()->SetControlRotation(NewRotation);
	}

	// 카메라 위치 적용
	CameraBoom->SetWorldLocation(CameraPosition);
}

void UCameraLockOnComponent::UpdateLockOnMarker()
{
	// 락온 마커 UI 업데이트 (블루프린트에서 구현)
	// 여기서는 기본적인 처리만 수행
}

void UCameraLockOnComponent::SetLockOnState(ELockOnState NewState)
{
	if (CurrentLockOnState == NewState) return;

	CurrentLockOnState = NewState;
	OnLockOnStateChanged.Broadcast(NewState);
}

void UCameraLockOnComponent::StoreOriginalCameraSettings()
{
	if (!CameraBoom || bHasStoredOriginalSettings) return;

	OriginalCameraDistance = CameraBoom->TargetArmLength;
	OriginalCameraOffset = CameraBoom->SocketOffset;
	bHasStoredOriginalSettings = true;
}

void UCameraLockOnComponent::RestoreOriginalCameraSettings()
{
	if (!CameraBoom || !bHasStoredOriginalSettings) return;

	CameraBoom->TargetArmLength = OriginalCameraDistance;
	CameraBoom->SocketOffset = OriginalCameraOffset;
}

bool UCameraLockOnComponent::IsValidLockOnTarget(AActor* Target) const
{
	if (!Target) return false;

	// BaseEnemy인지 확인
	ABaseEnemy* Enemy = Cast<ABaseEnemy>(Target);
	if (!Enemy) return false;

	// 살아있는지 확인
	if (!Enemy->IsAlive()) return false;

	return true;
}

bool UCameraLockOnComponent::IsTargetInRange(AActor* Target) const
{
	if (!Target || !OwnerPawn) return false;

	float Distance = FVector::Dist(OwnerPawn->GetActorLocation(), Target->GetActorLocation());
	return Distance <= LockOnRange;
}

bool UCameraLockOnComponent::IsTargetInView(AActor* Target) const
{
	if (!Target || !OwnerPawn || !PlayerCamera) return false;

	FVector CameraLocation = PlayerCamera->GetComponentLocation();
	FVector CameraForward = PlayerCamera->GetForwardVector();
	FVector ToTarget = (Target->GetActorLocation() - CameraLocation).GetSafeNormal();

	float DotProduct = FVector::DotProduct(CameraForward, ToTarget);
	float AngleInDegrees = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

	// 시야각 내에 있는지 확인
	if (AngleInDegrees > LockOnAngle) return false;

	// 라인 트레이스로 장애물 확인
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerPawn);
	QueryParams.AddIgnoredActor(Target);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		CameraLocation,
		Target->GetActorLocation(),
		ECollisionChannel::ECC_Visibility,
		QueryParams
	);

	// 장애물이 없으면 시야에 있음
	return !bHit;
} 