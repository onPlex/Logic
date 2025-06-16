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

    // 컴포넌트 참조 가져오기
    OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn)
    {
        SpringArm = OwnerPawn->FindComponentByClass<USpringArmComponent>();
        Camera = OwnerPawn->FindComponentByClass<UCameraComponent>();
        
        // 카메라 컴포넌트 찾기
        TArray<UCameraComponent*> CameraComponents;
        OwnerPawn->GetComponents<UCameraComponent>(CameraComponents);
        
        for (UCameraComponent* Cam : CameraComponents)
        {
            if (Cam->GetName().Contains(TEXT("Combat")))
            {
                CombatCamera = Cam;
            }
            else if (Cam->GetName().Contains(TEXT("Move")))
            {
                MoveCamera = Cam;
            }
        }

        // 초기 카메라 설정
        if (SpringArm)
        {
            TargetArmLength = SpringArm->TargetArmLength;
            HeightOffset = SpringArm->SocketOffset.Z;
        }

        // 초기에는 이동 카메라 활성화
        if (MoveCamera)
        {
            MoveCamera->SetActive(true);
        }
        if (CombatCamera)
        {
            CombatCamera->SetActive(false);
        }
    }
}

void UCameraLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 카메라 컴포넌트 유효성 검사
    if (!CombatCamera || !MoveCamera)
    {
        TArray<UCameraComponent*> CameraComponents;
        OwnerPawn->GetComponents<UCameraComponent>(CameraComponents);
        
        for (UCameraComponent* Cam : CameraComponents)
        {
            if (Cam->GetName().Contains(TEXT("Combat")))
            {
                CombatCamera = Cam;
            }
            else if (Cam->GetName().Contains(TEXT("Move")))
            {
                MoveCamera = Cam;
            }
        }
    }

    // 현재 활성화된 카메라 정보 표시
    bool bCombatCameraActive = CombatCamera && CombatCamera->IsActive() && CombatCamera->IsVisible();
    bool bMoveCameraActive = MoveCamera && MoveCamera->IsActive() && MoveCamera->IsVisible();

    // 카메라 상태 디버그 메시지
    FString CameraState = FString::Printf(TEXT("Camera State - Combat: %s, Move: %s"), 
        bCombatCameraActive ? TEXT("Active") : TEXT("Inactive"),
        bMoveCameraActive ? TEXT("Active") : TEXT("Inactive"));
    GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, CameraState);

    // 카메라 상태 확인 및 자동 활성화
    if (!bCombatCameraActive && !bMoveCameraActive)
    {
        // 두 카메라 모두 비활성화된 경우
        if (CurrentLockOnState == ELockOnState::Locked || CurrentLockOnState == ELockOnState::Searching)
        {
            if (CombatCamera)
            {
                CombatCamera->SetActive(true);
                if (MoveCamera) MoveCamera->SetActive(false);
                GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, TEXT("Current Camera: Combat Camera (Auto-activated)"));
            }
        }
        else
        {
            if (MoveCamera)
            {
                MoveCamera->SetActive(true);
                if (CombatCamera) CombatCamera->SetActive(false);
                GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, TEXT("Current Camera: Movement Camera (Auto-activated)"));
            }
        }
    }
    else if (bCombatCameraActive && bMoveCameraActive)
    {
        // 두 카메라 모두 활성화된 경우
        if (CurrentLockOnState == ELockOnState::Locked || CurrentLockOnState == ELockOnState::Searching)
        {
            if (MoveCamera) MoveCamera->SetActive(false);
        }
        else
        {
            if (CombatCamera) CombatCamera->SetActive(false);
        }
    }

    // 현재 활성화된 카메라 표시
    if (bCombatCameraActive)
    {
        GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, TEXT("Current Camera: Combat Camera"));
    }
    else if (bMoveCameraActive)
    {
        GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, TEXT("Current Camera: Movement Camera"));
    }

    // 현재 타겟 정보 표시
    if (CurrentTarget)
    {
        ABaseEnemy* Enemy = Cast<ABaseEnemy>(CurrentTarget);
        if (Enemy)
        {
            FString TargetInfo = FString::Printf(TEXT("Current Target: %s (Enemy)"), *Enemy->GetName());
            GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, TargetInfo);
        }
        else
        {
            // BaseEnemy가 아닌 경우 락온 해제
            ToggleLockOn();
            return;
        }
    }
    else if (CurrentLockOnState == ELockOnState::Searching)
    {
        GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, TEXT("Searching for enemy target..."));
    }
    else
    {
        GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, TEXT("No target"));
    }

    if (CurrentLockOnState == ELockOnState::Locked)
    {
        if (!CurrentTarget || !IsValidTarget(CurrentTarget))
        {
            ToggleLockOn();
            return;
        }

        float Dist = FVector::Distance(OwnerPawn->GetActorLocation(), CurrentTarget->GetActorLocation());
        if (Dist > LockOnBreakDistance)
        {
            ToggleLockOn();
            return;
        }

        if (!IsTargetInView(CurrentTarget))
        {
            TimeOutOfView += DeltaTime;
            if (TimeOutOfView >= MaxTimeOutOfView)
            {
                ToggleLockOn();
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
        
        // 0.2초 이내에 타겟을 찾지 못하면 락온 해제
        if (SearchTimer >= 0.2f)
        {
            // 타겟을 찾지 못한 경우 락온 해제
            if (CombatCamera)
            {
                CombatCamera->SetActive(false);
            }
            if (MoveCamera)
            {
                MoveCamera->SetActive(true);
            }
            RestoreOriginalCameraSettings();
            
            CurrentTarget = nullptr;
            CurrentLockOnState = ELockOnState::None;
            TimeOutOfView = 0.0f;
            SearchTimer = 0.0f;
            
            OnLockOnTargetChanged.Broadcast(nullptr);
            OnLockOnStateChanged.Broadcast(CurrentLockOnState);
            
            return;
        }

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
        }
    }
    else if (CurrentLockOnState == ELockOnState::None)
    {
        // 이동 카메라 상태에서의 카메라 업데이트
        if (MoveCamera && MoveCamera->IsActive())
        {
            // 플레이어가 화면 중앙에 오도록 카메라 위치 조정
            FVector OwnerLocation = OwnerPawn->GetActorLocation();
            FVector2D ScreenLocation;
            if (UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), OwnerLocation, ScreenLocation))
            {
                FVector2D ViewportSize;
                GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);
                
                FVector2D CenterOffset = ScreenLocation - (ViewportSize * 0.5f);
                float DistanceFromCenter = CenterOffset.Size();
                
                if (DistanceFromCenter > ViewportSize.X * 0.2f)
                {
                    FVector LookDir = (OwnerLocation - SpringArm->GetComponentLocation()).GetSafeNormal();
                    FRotator TargetRot = LookDir.Rotation();
                    FRotator CurrentRot = OwnerPawn->GetControlRotation();
                    FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, CameraRotationInterpSpeed);
                    OwnerPawn->GetController()->SetControlRotation(NewRot);
                }
            }
        }
        else
        {
            // 이동 카메라가 활성화되지 않은 경우 활성화
            if (MoveCamera)
            {
                MoveCamera->SetActive(true);
            }
            if (CombatCamera)
            {
                CombatCamera->SetActive(false);
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
    if (!OwnerPawn || !SpringArm || !Camera) return;

    if (CurrentLockOnState == ELockOnState::Locked)
    {
        // 락온 해제 시
        // 1. 먼저 전투 카메라 비활성화
        if (CombatCamera)
        {
            CombatCamera->SetActive(false);
        }

        // 2. 이동 카메라 활성화
        if (MoveCamera)
        {
            MoveCamera->SetActive(true);
        }

        // 3. 카메라 설정 복구
        RestoreOriginalCameraSettings();

        // 4. 상태 초기화
        CurrentTarget = nullptr;
        CurrentLockOnState = ELockOnState::None;
        TimeOutOfView = 0.0f;
        SearchTimer = 0.0f;

        // 5. 이벤트 브로드캐스트
        OnLockOnTargetChanged.Broadcast(nullptr);
        OnLockOnStateChanged.Broadcast(CurrentLockOnState);

        UE_LOG(LogTemp, Log, TEXT("Lock-on toggled off"));
    }
    else
    {
        // 락온 시작 시
        // 1. 먼저 이동 카메라 비활성화
        if (MoveCamera)
        {
            MoveCamera->SetActive(false);
        }

        // 2. 전투 카메라 활성화
        if (CombatCamera)
        {
            CombatCamera->SetActive(true);
        }

        // 3. 락온 상태로 전환
        CurrentLockOnState = ELockOnState::Searching;
        SearchTimer = 0.0f;
        TimeOutOfView = 0.0f;

        // 4. 이벤트 브로드캐스트
        OnLockOnStateChanged.Broadcast(CurrentLockOnState);

        UE_LOG(LogTemp, Log, TEXT("Lock-on toggled on - searching for target"));
    }
}

void UCameraLockOnComponent::RestoreOriginalCameraSettings()
{
    if (!SpringArm || !OwnerPawn || !Camera) return;

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

    // 4. 플레이어가 화면 중앙에 오도록 카메라 위치 조정
    FVector2D ScreenLocation;
    if (UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), OwnerLocation, ScreenLocation))
    {
        FVector2D ViewportSize;
        GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);
        
        // 화면 중앙에서의 거리 계산
        FVector2D CenterOffset = ScreenLocation - (ViewportSize * 0.5f);
        float DistanceFromCenter = CenterOffset.Size();
        
        // 화면 중앙에서 너무 멀어졌을 경우 카메라 위치 조정
        if (DistanceFromCenter > ViewportSize.X * 0.2f) // 화면의 20% 이상 벗어났을 때
        {
            // 카메라 회전을 플레이어 방향으로 조정
            FVector LookDir = (OwnerLocation - SpringArm->GetComponentLocation()).GetSafeNormal();
            FRotator TargetRot = LookDir.Rotation();
            FRotator NewRot = FMath::RInterpTo(CurrentRotation, TargetRot, GetWorld()->GetDeltaSeconds(), CameraRotationInterpSpeed);
            OwnerPawn->GetController()->SetControlRotation(NewRot);
        }
    }
}

void UCameraLockOnComponent::SwitchTarget(const FVector2D& InputDirection)
{
    if (!IsLockedOn() || !OwnerPawn || !Camera || PotentialTargets.Num() <= 1)
    {
        return;
    }

    // 현재 타겟의 인덱스 찾기
    int32 CurrentIndex = PotentialTargets.Find(CurrentTarget);
    if (CurrentIndex == INDEX_NONE)
    {
        return;
    }

    // 입력 방향에 따라 다음 타겟 선택
    int32 NextIndex = CurrentIndex;
    if (InputDirection.X > 0.0f) // 오른쪽
    {
        NextIndex = (CurrentIndex + 1) % PotentialTargets.Num();
    }
    else if (InputDirection.X < 0.0f) // 왼쪽
    {
        NextIndex = (CurrentIndex - 1 + PotentialTargets.Num()) % PotentialTargets.Num();
    }

    // 새 타겟 설정
    if (NextIndex != CurrentIndex)
    {
        CurrentTarget = PotentialTargets[NextIndex];
        OnLockOnTargetChanged.Broadcast(CurrentTarget);
    }
}

void UCameraLockOnComponent::FindPotentialTargets()
{
    if (!OwnerPawn || !Camera)
    {
        return;
    }

    PotentialTargets.Empty();

    // BaseEnemy 클래스의 액터만 검색
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseEnemy::StaticClass(), FoundActors);

    FVector OwnerLocation = OwnerPawn->GetActorLocation();
    FVector CameraForward = Camera->GetForwardVector();

    for (AActor* Actor : FoundActors)
    {
        // BaseEnemy로 캐스팅 확인
        ABaseEnemy* Enemy = Cast<ABaseEnemy>(Actor);
        if (!Enemy || Actor == OwnerPawn || !IsValidTarget(Enemy))
        {
            continue;
        }

        FVector DirectionToTarget = (Enemy->GetActorLocation() - OwnerLocation).GetSafeNormal();
        float DotProduct = FVector::DotProduct(CameraForward, DirectionToTarget);
        float Angle = FMath::Acos(DotProduct) * (180.0f / PI);

        if (Angle <= LockOnAngle)
        {
            float Distance = FVector::Distance(OwnerLocation, Enemy->GetActorLocation());
            if (Distance <= LockOnRadius)
            {
                PotentialTargets.Add(Enemy);
            }
        }
    }
}

AActor* UCameraLockOnComponent::FindBestTarget()
{
    if (PotentialTargets.Num() == 0)
    {
        return nullptr;
    }

    AActor* BestTarget = nullptr;
    float BestScore = -1.0f;

    FVector OwnerLocation = OwnerPawn->GetActorLocation();
    FVector CameraForward = Camera->GetForwardVector();

    for (AActor* Target : PotentialTargets)
    {
        FVector DirectionToTarget = (Target->GetActorLocation() - OwnerLocation).GetSafeNormal();
        float DotProduct = FVector::DotProduct(CameraForward, DirectionToTarget);
        float Distance = FVector::Distance(OwnerLocation, Target->GetActorLocation());
        
        // 거리에 따른 가중치 계산 (가까울수록 높은 점수)
        float DistanceScore = 1.0f - (Distance / LockOnRadius);
        
        // 방향에 따른 가중치 계산 (정면에 있을수록 높은 점수)
        float DirectionScore = (DotProduct + 1.0f) * 0.5f;
        
        // 최종 점수 계산
        float FinalScore = (DistanceScore * 0.4f) + (DirectionScore * 0.6f);

        if (FinalScore > BestScore)
        {
            BestScore = FinalScore;
            BestTarget = Target;
        }
    }

    return BestTarget;
}

bool UCameraLockOnComponent::IsValidTarget(AActor* Target) const
{
    if (!Target || !IsValid(Target))
    {
        return false;
    }

    // BaseEnemy 클래스인지 확인
    const ABaseEnemy* Enemy = Cast<ABaseEnemy>(Target);
    if (!Enemy)
    {
        return false;
    }

    // 적이 살아있는지 확인
    return Enemy->IsAlive();
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
    if (!Target || !OwnerPawn || !Camera)
    {
        return false;
    }

    FVector2D ScreenLocation;
    if (UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), Target->GetActorLocation(), ScreenLocation))
    {
        FVector2D ViewportSize;
        GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);
        
        // 화면 경계를 벗어났는지 확인
        return ScreenLocation.X >= 0 && ScreenLocation.X <= ViewportSize.X &&
               ScreenLocation.Y >= 0 && ScreenLocation.Y <= ViewportSize.Y;
    }

    return false;
}

void UCameraLockOnComponent::UpdateCameraLockOn(float DeltaTime)
{
    if (!SpringArm || !CurrentTarget || !OwnerPawn || !Camera) return;

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

    // 10. 뷰포트 내 타겟 위치 확인 및 조정
    FVector2D ScreenLocation;
    if (UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), TargetPos, ScreenLocation))
    {
        FVector2D ViewportSize;
        GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);
        
        // 화면 중앙에서의 거리 계산
        FVector2D CenterOffset = ScreenLocation - (ViewportSize * 0.5f);
        float DistanceFromCenter = CenterOffset.Size();
        
        // 화면 중앙에서 너무 멀어졌을 경우 카메라 위치 조정
        if (DistanceFromCenter > ViewportSize.X * 0.3f) // 화면의 30% 이상 벗어났을 때
        {
            // 카메라 회전을 타겟 방향으로 더 강하게 조정
            float AdjustmentStrength = FMath::Clamp(DistanceFromCenter / (ViewportSize.X * 0.5f), 0.0f, 1.0f);
            FRotator AdjustedRot = FMath::RInterpTo(NewRot, TargetRot, DeltaTime, CameraRotationInterpSpeed * (1.0f + AdjustmentStrength));
            OwnerPawn->GetController()->SetControlRotation(AdjustedRot);
        }
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
