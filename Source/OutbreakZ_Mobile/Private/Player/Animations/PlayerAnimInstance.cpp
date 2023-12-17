// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/Animations/PlayerAnimInstance.h"

#include "Player/SurvivalCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	SetReferences();
}

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!PlayerRef) return;

	DeltaTimeX = DeltaSeconds;
	
	SetEssentialMovementData();
	DetermineLocomotionState();
	TrackLocomotionStates();
}

void UPlayerAnimInstance::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();
	if (!PlayerRef) return;

	UpdateCharacterTransform();
	ResetTransition();
}

void UPlayerAnimInstance::SetReferences()
{
	if (const auto NewPlayerRef = Cast<ASurvivalCharacter>(TryGetPawnOwner()))
	{
		PlayerRef = NewPlayerRef;
		CharacterMovementRef = PlayerRef->GetCharacterMovement();
	}
}

void UPlayerAnimInstance::SetEssentialMovementData()
{
	Velocity = CharacterMovementRef->Velocity;
	bIsFalling = CharacterMovementRef->IsFalling();
	bIsCrouch = CharacterMovementRef->IsCrouching();
	MaxSpeed = CharacterMovementRef->GetMaxSpeed();

	InputVector = CharacterMovementRef->GetLastInputVector();

	GroundSpeed = bIsFalling ? 0.f : Velocity.Size2D();
}

void UPlayerAnimInstance::DetermineLocomotionState()
{
	PrevLocomotionState = LocomotionState;

	if (bIsFalling)
	{
		LocomotionState = ELocomotionState::ELS_Jump;
		return;
	}

	TimeInLocomotionState += GetDeltaSeconds();
	if (!UKismetMathLibrary::Greater_DoubleDouble(TimeInLocomotionState, MinTimeInLocomotionState)) return;

	DetermineGroundLocomotionState();
}

void UPlayerAnimInstance::TrackLocomotionStates()
{
	TrackLocomotionState(ELocomotionState::ELS_Idle, EntryFlags.IdleFlag,
	                     &UPlayerAnimInstance::OnEntryIdle,
	                     &UPlayerAnimInstance::OnExitIdle,
	                     &UPlayerAnimInstance::WhileTrueIdle,
	                     &UPlayerAnimInstance::WhileFalseIdle);

	TrackLocomotionState(ELocomotionState::ELS_Walk, EntryFlags.WalkFlag,
	                     &UPlayerAnimInstance::OnEntryWalk,
	                     &UPlayerAnimInstance::OnExitWalk,
	                     &UPlayerAnimInstance::WhileTrueWalk,
	                     &UPlayerAnimInstance::WhileFalseWalk);

	TrackLocomotionState(ELocomotionState::ELS_Jog, EntryFlags.JogFlag,
	                     &UPlayerAnimInstance::OnEntryJog,
	                     &UPlayerAnimInstance::OnExitJog,
	                     &UPlayerAnimInstance::WhileTrueJog,
	                     &UPlayerAnimInstance::WhileFalseJog);

	TrackLocomotionState(ELocomotionState::ELS_Crouch, EntryFlags.CrouchFlag,
	                     &UPlayerAnimInstance::OnEntryCrouch,
	                     &UPlayerAnimInstance::OnExitCrouch,
	                     &UPlayerAnimInstance::WhileTrueCrouch,
	                     &UPlayerAnimInstance::WhileFalseCrouch);

	TrackLocomotionState(ELocomotionState::ELS_Jump, EntryFlags.JumpFlag,
	                     &UPlayerAnimInstance::OnEntryJump,
	                     &UPlayerAnimInstance::OnExitJump,
	                     &UPlayerAnimInstance::WhileTrueJump,
	                     &UPlayerAnimInstance::WhileFalseJump);
}

void UPlayerAnimInstance::UpdateCharacterTransform()
{
	if (InLocomotionState())
	{
		MoveRotationBehavior();
	}
	else
	{
		ResetTargetRotations();
	}
}

void UPlayerAnimInstance::ResetTransition()
{
	bPlayStartAnim = false;
	bPlayGaitTransitionAnim = false;
}

FRotator UPlayerAnimInstance::GetTargetRotation() const
{
	float Yaw;
	if (bDoInputRotation)
	{
		Yaw = UKismetMathLibrary::MakeRotFromX(InputVector).Yaw;
	}
	else
	{
		Yaw = UKismetMathLibrary::MakeRotFromX(Velocity).Yaw;
	}
	return FRotator(0.f, Yaw, 0.f);
}

void UPlayerAnimInstance::MoveRotationBehavior()
{
	if (LocomotionState != ELocomotionState::ELS_Walk &&
		LocomotionState != ELocomotionState::ELS_Jog &&
		LocomotionState != ELocomotionState::ELS_Crouch)
		return;

	const auto TargetRotation = GetTargetRotation();

	PrimaryTargetRotation = UKismetMathLibrary::RInterpTo_Constant(PrimaryTargetRotation, TargetRotation, DeltaTimeX,
	                                                               PrimaryRotationInterpSpeed);
	SecondaryTargetRotation = UKismetMathLibrary::RInterpTo(SecondaryTargetRotation, PrimaryTargetRotation, DeltaTimeX,
	                                                        SecondaryRotationInterpSpeed);

	const auto RotationDelta = GetCurveValue(MoveDataRotationName);

	const FRotator NewRotation = FRotator(
		SecondaryTargetRotation.Pitch,
		SecondaryTargetRotation.Yaw + RotationDelta,
		SecondaryTargetRotation.Roll
	);
	PlayerRef->SetActorRotation(NewRotation);
}

void UPlayerAnimInstance::UpdateStop()
{
	if (PrevLocomotionState == ELocomotionState::ELS_Crouch)
	{
		StopAnim = CrouchStopAnim;
		AnimStartTime = CrouchAnimStartTime;
		PostStopLocomotionState = ELocomotionState::ELS_Crouch;
		return;
	}

	if (GroundSpeed > JogStopSpeedLimit)
	{
		StopAnim = JogStopAnim;
		AnimStartTime = JogAnimStartTime;
	}
	else
	{
		StopAnim = WalkStopAnim;
		AnimStartTime = WalkAnimStartTime;
	}

	PostStopLocomotionState = ELocomotionState::ELS_Idle;
}

void UPlayerAnimInstance::ResetTargetRotations()
{
	PrimaryTargetRotation = SecondaryTargetRotation = PlayerRef->GetActorRotation();
}

void UPlayerAnimInstance::UpdateLocomotionValues()
{
	const float SpeedCurveValue = GetCurveValue(MoveDataSpeedName);
	PlayRate = UKismetMathLibrary::SafeDivide(GroundSpeed, SpeedCurveValue);
}

void UPlayerAnimInstance::UpdateOnWalkEntry()
{
	UpdateEntryVariables();
	UpdateStartAnim(WalkStartAnim,
	                WalkStart90LAnim, WalkStart90LAnimTime,
	                WalkStart180LAnim, WalkStart180LAnimTime,
	                WalkStart90RAnim, WalkStart90RAnimTime,
	                WalkStart180RAnim, WalkStart180RAnimTime,
	                WalkStartFAnim, WalkStartFAnimTime);
}

void UPlayerAnimInstance::UpdateOnJogEntry()
{
	UpdateEntryVariables();
	UpdateStartAnim(JogStartAnim,
					JogStart90LAnim, JogStart90LAnimTime,
					JogStart180LAnim, JogStart180LAnimTime,
					JogStart90RAnim, JogStart90RAnimTime,
					JogStart180RAnim, JogStart180RAnimTime,
					JogStartFAnim, JogStartFAnimTime);
}

void UPlayerAnimInstance::UpdateOnCrouchEntry()
{
	UpdateEntryVariables();
	UpdateStartAnim(CrouchStartAnim,
					CrouchStart90LAnim, CrouchStart90LAnimTime,
					CrouchStart180LAnim, CrouchStart180LAnimTime,
					CrouchStart90RAnim, CrouchStart90RAnimTime,
					CrouchStart180RAnim, CrouchStart180RAnimTime,
					CrouchStartFAnim, CrouchStartFAnimTime);
}

void UPlayerAnimInstance::UpdateEntryVariables()
{
	StartRotation = PlayerRef->GetActorRotation();
	PrimaryTargetRotation = SecondaryTargetRotation = UKismetMathLibrary::MakeRotFromX(InputVector);

	StartAngle = UKismetMathLibrary::NormalizedDeltaRotator(PrimaryTargetRotation, StartRotation).Yaw;
}

void UPlayerAnimInstance::UpdateCrouchTransitionAnim()
{
	switch (PrevLocomotionState)
	{
	case ELocomotionState::ELS_Walk:
		TransitionToCrouchAnim = WalkToCrouchAnim;
		AnimStartTime = GetCurveValue(TransitionWalkToCrouchCurveName);
		break;
	case ELocomotionState::ELS_Jog:
		TransitionToCrouchAnim = JogToWalkAnim;
		AnimStartTime = JogToWalkAnimStartTime;
		break;
	default:
		break;
	}
}

void UPlayerAnimInstance::UpdateWalkTransitionAnim()
{
	switch (PrevLocomotionState)
	{
	case ELocomotionState::ELS_Crouch:
		TransitionToWalkAnim = CrouchToWalkAnim;
		AnimStartTime = GetCurveValue(TransitionWalkToCrouchCurveName);
		break;
	case ELocomotionState::ELS_Jog:
		TransitionToWalkAnim = JogToWalkAnim;
		AnimStartTime = JogToWalkAnimStartTime;
		break;
	default:
		break;
	}
}

void UPlayerAnimInstance::UpdateJogTransitionAnim()
{
	switch (PrevLocomotionState)
	{
	case ELocomotionState::ELS_Crouch:
		TransitionToJogAnim = CrouchToJogAnim;
		AnimStartTime = GetCurveValue(TransitionWalkToJogCurveName);
		break;
	case ELocomotionState::ELS_Walk:
		TransitionToJogAnim = WalkToJogAnim;
		AnimStartTime = WalkToJogAnimStartTime;
		break;
	default:
		break;
	}
}

void UPlayerAnimInstance::DetermineGroundLocomotionState()
{
	const auto NormalizedVelocity = UKismetMathLibrary::Normal(Velocity);
	const auto NormalizedCurrentAcceleration = UKismetMathLibrary::Normal(
		CharacterMovementRef->GetCurrentAcceleration());

	const auto VelocityAccelerationDotProduct = FVector::DotProduct(NormalizedVelocity, NormalizedCurrentAcceleration);

	if (UKismetMathLibrary::Less_DoubleDouble(VelocityAccelerationDotProduct, -0.5))
	{
		LocomotionState = ELocomotionState::ELS_Idle;
		return;
	}

	if (bIsCrouch)
	{
		if (IsMovementWithinThresholds(CrouchMinCurrentSpeed, CrouchMinMaxSpeed, CrouchMinInputAcceleration))
		{
			LocomotionState = ELocomotionState::ELS_Crouch;
			return;
		}
	}
	else
	{
		if (IsMovementWithinThresholds(JogMinCurrentSpeed, JogMinMaxSpeed, JogMinInputAcceleration))
		{
			LocomotionState = ELocomotionState::ELS_Jog;
			return;
		}

		if (IsMovementWithinThresholds(WalkMinCurrentSpeed, WalkMinMaxSpeed, WalkMinInputAcceleration))
		{
			LocomotionState = ELocomotionState::ELS_Walk;
			return;
		}
	}

	LocomotionState = ELocomotionState::ELS_Idle;
}

#pragma region LOCOMOTION_STATE_CALLBACKS
#pragma region IDLE
void UPlayerAnimInstance::OnEntryIdle()
{
	ResetTargetRotations();
	UpdateStop();
}

void UPlayerAnimInstance::OnExitIdle()
{
}

void UPlayerAnimInstance::WhileTrueIdle()
{
}

void UPlayerAnimInstance::WhileFalseIdle()
{
}
#pragma endregion

#pragma region WALK
void UPlayerAnimInstance::OnEntryWalk()
{
	if(PrevLocomotionState == ELocomotionState::ELS_Idle)
	{
		UpdateOnWalkEntry();
		bPlayStartAnim = true;
	}
	else
	{
		if (UKismetMathLibrary::Less_DoubleDouble(GroundSpeed, MaxSpeedForPlayingStartWalkAnim))
		{
			UpdateOnWalkEntry();
			bPlayStartAnim = true;
		}
		else
		{
			bPlayGaitTransitionAnim = true;
			UpdateWalkTransitionAnim();
		}
	}
}

void UPlayerAnimInstance::OnExitWalk()
{
}

void UPlayerAnimInstance::WhileTrueWalk()
{
	UpdateLocomotionValues();
}

void UPlayerAnimInstance::WhileFalseWalk()
{
}
#pragma endregion

#pragma region JOG
void UPlayerAnimInstance::OnEntryJog()
{
	if(PrevLocomotionState == ELocomotionState::ELS_Idle)
	{
		UpdateOnJogEntry();
		bPlayStartAnim = true;
	}
	else
	{
		if (UKismetMathLibrary::Less_DoubleDouble(GroundSpeed, MaxSpeedForPlayingStartJogAnim))
		{
			UpdateOnJogEntry();
			bPlayStartAnim = true;
		}
		else
		{
			bPlayGaitTransitionAnim = true;
			UpdateJogTransitionAnim();
		}
	}
}

void UPlayerAnimInstance::OnExitJog()
{
}

void UPlayerAnimInstance::WhileTrueJog()
{
	UpdateLocomotionValues();
}

void UPlayerAnimInstance::WhileFalseJog()
{
}
#pragma endregion

#pragma region CROUCH
void UPlayerAnimInstance::OnEntryCrouch()
{
	if(PrevLocomotionState == ELocomotionState::ELS_Idle)
	{
		UpdateOnCrouchEntry();
		bPlayStartAnim = true;
	}
	else
	{
		if (UKismetMathLibrary::Less_DoubleDouble(GroundSpeed, MaxSpeedForPlayingStartWalkAnim))
		{
			UpdateOnCrouchEntry();
			bPlayStartAnim = true;
		}
		else
		{
			bPlayGaitTransitionAnim = true;
			UpdateCrouchTransitionAnim();
		}
	}
}

void UPlayerAnimInstance::OnExitCrouch()
{
}

void UPlayerAnimInstance::WhileTrueCrouch()
{
	UpdateLocomotionValues();
}

void UPlayerAnimInstance::WhileFalseCrouch()
{
}
#pragma endregion

#pragma region JUMP
void UPlayerAnimInstance::OnEntryJump()
{
}

void UPlayerAnimInstance::OnExitJump()
{
}

void UPlayerAnimInstance::WhileTrueJump()
{
}

void UPlayerAnimInstance::WhileFalseJump()
{
}
#pragma endregion
#pragma endregion

bool UPlayerAnimInstance::IsMovementWithinThresholds(float MinCurrentSpeed, float MinMaxSpeed,
                                                     float MinInputAcceleration) const
{
	return UKismetMathLibrary::LessEqual_DoubleDouble(MinCurrentSpeed, GroundSpeed) &&
		UKismetMathLibrary::LessEqual_DoubleDouble(MinMaxSpeed, MaxSpeed) &&
		UKismetMathLibrary::LessEqual_DoubleDouble(MinInputAcceleration, InputVector.Size());
}

void UPlayerAnimInstance::TrackLocomotionState(ELocomotionState TracedState, bool& EnterFlag,
                                               void (UPlayerAnimInstance::*OnEnterCallback)(),
                                               void (UPlayerAnimInstance::*OnExitCallback)(),
                                               void (UPlayerAnimInstance::*WhileTrueCallback)(),
                                               void (UPlayerAnimInstance::*WhileFalseCallback)())
{
	if (LocomotionState == TracedState)
	{
		if (EnterFlag != true)
		{
			EnterFlag = true;
			(this->*OnEnterCallback)();
			TimeInLocomotionState = 0.f;
		}
		else
		{
			(this->*WhileTrueCallback)();
		}
	}
	else
	{
		if (EnterFlag != false)
		{
			EnterFlag = false;
			(this->*OnExitCallback)();
		}
		else
		{
			(this->*WhileFalseCallback)();
		}
	}
}

void UPlayerAnimInstance::UpdateStartAnim(UAnimSequence*& FinishAnim, UAnimSequence* Start90LAnim,
                                          float Start90LAnimTime, UAnimSequence* Start180LAnim, float Start180LAnimTime,
                                          UAnimSequence* Start90RAnim,
                                          float Start90RAnimTime, UAnimSequence* Start180RAnim, float Start180RAnimTime,
                                          UAnimSequence* StartFAnim,
                                          float StartFAnimTime)
{
	if (UKismetMathLibrary::InRange_FloatFloat(StartAngle, -135, -45, false, true))
	{
		FinishAnim = Start90LAnim;
		AnimStartTime = Start90LAnimTime;
	}
	else if (UKismetMathLibrary::InRange_FloatFloat(StartAngle, -180, -135, true, true))
	{
		FinishAnim = Start180LAnim;
		AnimStartTime = Start180LAnimTime;
	}
	else if (UKismetMathLibrary::InRange_FloatFloat(StartAngle, 45, 135, true, false))
	{
		FinishAnim = Start90RAnim;
		AnimStartTime = Start90RAnimTime;
	}
	else if (UKismetMathLibrary::InRange_FloatFloat(StartAngle, 135, 180, true, true))
	{
		FinishAnim = Start180RAnim;
		AnimStartTime = Start180RAnimTime;
	}
	else
	{
		FinishAnim = StartFAnim;
		AnimStartTime = StartFAnimTime;
	}
}