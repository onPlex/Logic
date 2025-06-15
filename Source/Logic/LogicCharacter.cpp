// Copyright Epic Games, Inc. All Rights Reserved.

#include "LogicCharacter.h"
#include "CameraLockOnComponent.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ALogicCharacter

ALogicCharacter::ALogicCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Add Player tag for enemy detection
	Tags.Add(FName("Player"));
	
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a move camera
	MoveCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("MoveCamera"));
	MoveCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	MoveCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Create a combat camera
	CombatCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CombatCamera"));
	CombatCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	CombatCamera->bUsePawnControlRotation = false;
	CombatCamera->SetActive(false); // Initially inactive

	// Create lock on component
	LockOnComponent = CreateDefaultSubobject<UCameraLockOnComponent>(TEXT("LockOnComponent"));

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// Input

void ALogicCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ALogicCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALogicCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALogicCharacter::Look);

		// Lock On
		EnhancedInputComponent->BindAction(LockOnAction, ETriggerEvent::Started, this, &ALogicCharacter::LockOn);

		// Switch Target
		// EnhancedInputComponent->BindAction(SwitchTargetAction, ETriggerEvent::Started, this, &ALogicCharacter::SwitchTarget);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ALogicCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ALogicCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// Lock On 상태가 아닐 때만 자유 카메라 회전 허용
		if (!LockOnComponent->IsLockedOn())
		{
			// add yaw and pitch input to controller
			AddControllerYawInput(LookAxisVector.X);
			AddControllerPitchInput(LookAxisVector.Y);
		}
		else
		{
			// Lock On 상태일 때는 타겟 전환에 사용
			if (FMath::Abs(LookAxisVector.X) > 0.5f || FMath::Abs(LookAxisVector.Y) > 0.5f)
			{
				// LockOnComponent->SwitchTargetInDirection(LookAxisVector);
			}
		}
	}
}

void ALogicCharacter::LockOn(const FInputActionValue& Value)
{
	if (LockOnComponent)
	{
		LockOnComponent->ToggleLockOn();
	}
}

void ALogicCharacter::SwitchTarget(const FInputActionValue& Value)
{
	if (LockOnComponent && LockOnComponent->IsLockedOn())
	{
		// 오른쪽 스틱 또는 방향키로 타겟 전환
		FVector2D SwitchDirection = Value.Get<FVector2D>();
		
		if (SwitchDirection.X > 0.5f)
		{
			// LockOnComponent->SwitchToNextTarget();
		}
		else if (SwitchDirection.X < -0.5f)
		{
			// LockOnComponent->SwitchToPreviousTarget();
		}
	}
}
