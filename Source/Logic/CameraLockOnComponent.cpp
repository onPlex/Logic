#include "CameraLockOnComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "BaseEnemy.h"
#include "GameFramework/Controller.h"
#include "DrawDebugHelpers.h"

UCameraLockOnComponent::UCameraLockOnComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    CurrentLockOnState = ELockOnState::None;
    CurrentTarget = nullptr;
}

void UCameraLockOnComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn)
    {
        SpringArm = OwnerPawn->FindComponentByClass<USpringArmComponent>();
        Camera = OwnerPawn->FindComponentByClass<UCameraComponent>();

        if (SpringArm)
        {
            SpringArm->bDoCollisionTest = true;
            SpringArm->ProbeSize = 12.f;
            SpringArm->ProbeChannel = ECC_Camera;
        }
    }
}

void UCameraLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (CurrentLockOnState == ELockOnState::Locked)
    {
        if (!CurrentTarget || !IsValidTarget(CurrentTarget))
        {
            ClearLockOn();
            return;
        }

        float Dist = FVector::Distance(OwnerPawn->GetActorLocation(), CurrentTarget->GetActorLocation());
        if (Dist > LockOnBreakDistance)
        {
            ClearLockOn();
            return;
        }

        if (!IsTargetInView(CurrentTarget))
        {
            TimeOutOfView += DeltaTime;
            if (TimeOutOfView >= MaxTimeOutOfView)
            {
                ClearLockOn();
                return;
            }
        }
        else
        {
            TimeOutOfView = 0.0f;
        }

        UpdateCameraLockOn(DeltaTime);
    }
    else if (CurrentLockOnState == ELockOnState::Searching)
    {
        SearchTimer += DeltaTime;
        if (SearchTimer >= LockOnSearchInterval)
        {
            SearchTimer = 0.0f;
            FindPotentialTargets();
            AActor* BestTarget = FindBestTarget();
            if (BestTarget)
            {
                CurrentTarget = BestTarget;
                CurrentLockOnState = ELockOnState::Locked;
                OnLockOnTargetChanged.Broadcast(CurrentTarget);
                OnLockOnStateChanged.Broadcast(CurrentLockOnState);
            }
            else
            {
                CurrentLockOnState = ELockOnState::None;
                OnLockOnStateChanged.Broadcast(CurrentLockOnState);
            }
        }
    }

    if (bShowDebugInfo)
    {
        DrawDebugInfo();
    }
}

void UCameraLockOnComponent::ToggleLockOn()
{
    if (CurrentLockOnState == ELockOnState::Locked)
    {
        ClearLockOn();
    }
    else
    {
        CurrentLockOnState = ELockOnState::Searching;
        OnLockOnStateChanged.Broadcast(CurrentLockOnState);
    }
}

void UCameraLockOnComponent::ClearLockOn()
{
    if (!OwnerPawn || !SpringArm || !Camera) return;

    // 1. 카메라 설정 복구
    RestoreOriginalCameraSettings();

    // 2. 전투 카메라 비활성화
    if (CombatCamera)
    {
        CombatCamera->SetActive(false);
    }

    // 3. 이동 카메라 활성화
    if (MoveCamera)
    {
        MoveCamera->SetActive(true);
    }

    // 4. 부드러운 전환을 위한 보간
    FVector CurrentLocation = SpringArm->GetComponentLocation();
    FVector TargetLocation = OwnerPawn->GetActorLocation() + FVector(0, 0, HeightOffset);
    FRotator CurrentRotation = OwnerPawn->GetControlRotation();
    FRotator TargetRotation = FRotator(0, CurrentRotation.Yaw, 0);

    // 5. 컨트롤러 회전 설정 복구
    if (AController* Controller = OwnerPawn->GetController())
    {
        Controller->SetControlRotation(TargetRotation);
    }

    // 6. 상태 초기화
    CurrentTarget = nullptr;
    CurrentLockOnState = ELockOnState::None;
    TimeOutOfView = 0.0f;

    // 7. 이벤트 브로드캐스트
    OnLockOnTargetChanged.Broadcast(nullptr);
    OnLockOnStateChanged.Broadcast(CurrentLockOnState);

    // 8. 로그 출력
    UE_LOG(LogTemp, Log, TEXT("Lock-on cleared"));
}

void UCameraLockOnComponent::RestoreOriginalCameraSettings()
{
    if (!SpringArm || !OwnerPawn) return;

    // 1. SpringArm 기본 설정 복구
    SpringArm->TargetArmLength = TargetArmLength;
    SpringArm->SocketOffset = FVector(0.0f, 0.0f, HeightOffset);
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->bInheritPitch = true;
    SpringArm->bInheritYaw = true;
    SpringArm->bInheritRoll = false;

    // 2. 카메라 위치 초기화
    FVector OwnerLocation = OwnerPawn->GetActorLocation();
    SpringArm->SetWorldLocation(OwnerLocation + FVector(0, 0, HeightOffset));

    // 3. 카메라 회전 초기화
    FRotator CurrentRotation = OwnerPawn->GetControlRotation();
    FRotator TargetRotation = FRotator(0, CurrentRotation.Yaw, 0);
    OwnerPawn->GetController()->SetControlRotation(TargetRotation);
}

void UCameraLockOnComponent::SwitchTarget(const FVector2D& InputDirection)
{
    if (!IsLockedOn() || !OwnerPawn || !Camera) return;

    FindPotentialTargets();
    if (PotentialTargets.Num() <= 1) return;

    FVector CameraForward = Camera->GetForwardVector();
    FVector CameraRight = Camera->GetRightVector();
    FVector InputDirection3D = (CameraRight * InputDirection.X + CameraForward * InputDirection.Y).GetSafeNormal();

    AActor* BestTarget = nullptr;
    float BestScore = -1.0f;

    for (AActor* Target : PotentialTargets)
    {
        if (Target == CurrentTarget) continue;

        FVector ToTarget = (Target->GetActorLocation() - OwnerPawn->GetActorLocation()).GetSafeNormal();
        float DotProduct = FVector::DotProduct(InputDirection3D, ToTarget);
        float Distance = FVector::Dist(OwnerPawn->GetActorLocation(), Target->GetActorLocation());
        float Score = DotProduct - (Distance / LockOnRadius);

        if (Score > BestScore)
        {
            BestScore = Score;
            BestTarget = Target;
        }
    }

    if (BestTarget)
    {
        CurrentTarget = BestTarget;
        OnLockOnTargetChanged.Broadcast(CurrentTarget);
    }
}

void UCameraLockOnComponent::FindPotentialTargets()
{
    PotentialTargets.Empty();

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseEnemy::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        if (IsValidTarget(Actor) && IsTargetInRange(Actor) && IsTargetInView(Actor))
        {
            PotentialTargets.Add(Actor);
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

AActor* UCameraLockOnComponent::FindBestTarget()
{
    if (PotentialTargets.Num() == 0) return nullptr;

    AActor* Closest = nullptr;
    float MinDist = FLT_MAX;

    for (AActor* Target : PotentialTargets)
    {
        float Dist = FVector::Distance(OwnerPawn->GetActorLocation(), Target->GetActorLocation());
        if (Dist < MinDist)
        {
            MinDist = Dist;
            Closest = Target;
        }
    }
    return Closest;
}

bool UCameraLockOnComponent::IsValidTarget(AActor* Target) const
{
    if (!Target) return false;

    const ABaseEnemy* Enemy = Cast<ABaseEnemy>(Target);
    return Enemy && Enemy->IsAlive();
}

bool UCameraLockOnComponent::IsTargetInRange(AActor* Target) const
{
    if (!Target || !OwnerPawn) return false;

    FVector OwnerLoc = OwnerPawn->GetActorLocation();
    FVector TargetLoc = Target->GetActorLocation();

    FVector Forward = OwnerPawn->GetActorForwardVector();
    FVector ToTarget = (TargetLoc - OwnerLoc).GetSafeNormal();

    float Angle = FMath::RadiansToDegrees(acosf(FVector::DotProduct(Forward, ToTarget)));
    float Distance = FVector::Distance(OwnerLoc, TargetLoc);

    return (Angle <= LockOnAngle) && (Distance <= LockOnRadius);
}

bool UCameraLockOnComponent::IsTargetInView(AActor* Target) const
{
    if (!Target || !OwnerPawn || !Camera) return false;

    FVector CameraLocation = Camera->GetComponentLocation();
    FVector CameraForward = Camera->GetForwardVector();
    FVector ToTarget = (Target->GetActorLocation() - CameraLocation).GetSafeNormal();

    float DotProduct = FVector::DotProduct(CameraForward, ToTarget);
    float AngleInDegrees = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

    if (AngleInDegrees > LockOnAngle) return false;

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerPawn);
    QueryParams.AddIgnoredActor(Target);

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        CameraLocation,
        Target->GetActorLocation(),
        ECC_Visibility,
        QueryParams
    );

    return !bHit;
}

void UCameraLockOnComponent::UpdateCameraLockOn(float DeltaTime)
{
    if (!SpringArm || !CurrentTarget || !OwnerPawn) return;

    FVector OwnerPos = OwnerPawn->GetActorLocation();
    FVector TargetPos = CurrentTarget->GetActorLocation();

    // 1. 중심점 보간 (플레이어와 타겟 사이의 35% 지점)
    FVector CenterPoint = FMath::Lerp(OwnerPos, TargetPos, CenterBias);
    CenterPoint.Z += HeightOffset;

    // 2. 카메라 거리 계산 (거리에 따른 동적 조정)
    float DistanceToTarget = FVector::Distance(OwnerPos, TargetPos);
    float TargetDistance = FMath::Lerp(MinCameraDistance, MaxCameraDistance, 
        FMath::Clamp((DistanceToTarget - MinLockOnDistance) / (MaxLockOnDistance - MinLockOnDistance), 0.0f, 1.0f));

    // 3. 부드러운 거리 보간
    float CurrentLength = SpringArm->TargetArmLength;
    float NewLength = FMath::FInterpTo(CurrentLength, TargetDistance, DeltaTime, ArmLengthInterpSpeed);
    SpringArm->TargetArmLength = NewLength;

    // 4. 카메라 위치 업데이트
    SpringArm->SetWorldLocation(CenterPoint);

    // 5. 카메라 회전 계산
    FVector LookDir = (TargetPos - CenterPoint).GetSafeNormal();
    FRotator TargetRot = LookDir.Rotation();

    // 6. 부드러운 회전 보간
    FRotator CurrentRot = OwnerPawn->GetControlRotation();
    FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, CameraRotationInterpSpeed);

    // 7. 피치 각도 제한
    NewRot.Pitch = FMath::Clamp(NewRot.Pitch, CameraPitchMin, CameraPitchMax);

    // 8. 컨트롤러 회전 설정
    OwnerPawn->GetController()->SetControlRotation(NewRot);

    // 9. 충돌 처리
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerPawn);
    QueryParams.AddIgnoredActor(CurrentTarget);

    FVector Start = CenterPoint;
    FVector End = CenterPoint + (LookDir * NewLength);

    if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams))
    {
        float SafeDistance = HitResult.Distance * 0.8f;
        SpringArm->TargetArmLength = FMath::Min(NewLength, SafeDistance);
    }
}

void UCameraLockOnComponent::DrawDebugInfo()
{
    if (!OwnerPawn || !CurrentTarget) return;

    // 락온 범위 표시
    DrawDebugSphere(GetWorld(), OwnerPawn->GetActorLocation(), LockOnRadius, 32, FColor::Yellow, false, -1.0f, 0, 2.0f);

    // 현재 타겟 표시
    DrawDebugSphere(GetWorld(), CurrentTarget->GetActorLocation(), 50.0f, 16, FColor::Red, false, -1.0f, 0, 2.0f);

    // 플레이어-타겟 연결선
    DrawDebugLine(GetWorld(), OwnerPawn->GetActorLocation(), CurrentTarget->GetActorLocation(), FColor::Green, false, -1.0f, 0, 2.0f);

    // 시야각 표시
    FVector Forward = OwnerPawn->GetActorForwardVector();
    FVector Right = OwnerPawn->GetActorRightVector();
    float AngleRad = FMath::DegreesToRadians(LockOnAngle);
    FVector LeftDir = Forward.RotateAngleAxis(LockOnAngle, FVector::UpVector);
    FVector RightDir = Forward.RotateAngleAxis(-LockOnAngle, FVector::UpVector);
    DrawDebugLine(GetWorld(), OwnerPawn->GetActorLocation(), OwnerPawn->GetActorLocation() + LeftDir * LockOnRadius, FColor::Blue, false, -1.0f, 0, 2.0f);
    DrawDebugLine(GetWorld(), OwnerPawn->GetActorLocation(), OwnerPawn->GetActorLocation() + RightDir * LockOnRadius, FColor::Blue, false, -1.0f, 0, 2.0f);
}
