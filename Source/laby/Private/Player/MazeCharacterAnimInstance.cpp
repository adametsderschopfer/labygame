#include "Player/MazeCharacterAnimInstance.h"
#include "ECS/MazePlayerControlDefinition.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TwoBoneIK.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FMazeRestartBlend : FAnimNode_TwoWayBlend
	{
		FMazeRestartBlend()
		{
			bResetChildOnActivation = true;
		}
	};

	// Applies a crouched stance to the same directional stride, preserving foot
	// orientation and using the engine's two-bone solver for knee articulation.
	struct FMazeCrouchPose : FAnimNode_Base
	{
		FPoseLink Input;
		FBoneReference Pelvis;
		FBoneReference Legs[2][3];
		float Weight = 0.f;
		float Drop = 70.f;

		FMazeCrouchPose()
		{
			Pelvis.BoneName = TEXT("Hips");
			const TCHAR* Names[2][3] = {{TEXT("L_up_leg"), TEXT("L_knee"), TEXT("L_foot")},
			                            {TEXT("R_up_leg"), TEXT("R_knee"), TEXT("R_foot")}};

			for (int32 Side = 0; Side < 2; ++Side)
				for (int32 Joint = 0; Joint < 3; ++Joint)
					Legs[Side][Joint].BoneName = Names[Side][Joint];
		}

		virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override
		{
			Input.Initialize(Context);
		}

		virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
		{
			Input.CacheBones(Context);
			const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
			Pelvis.Initialize(Bones);

			for (auto& Leg : Legs)
				for (auto& Bone : Leg)
					Bone.Initialize(Bones);
		}

		virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override
		{
			Input.Update(Context);
		}

		virtual void Evaluate_AnyThread(FPoseContext& Output) override
		{
			Input.Evaluate(Output);
			const FBoneContainer& Bones = Output.Pose.GetBoneContainer();

			if (Weight <= UE_SMALL_NUMBER || !Pelvis.IsValidToEvaluate(Bones))
				return;

			for (const auto& Leg : Legs)
				for (const auto& Bone : Leg)
					if (!Bone.IsValidToEvaluate(Bones))
						return;

			TArray<FTransform, TInlineAllocator<80>> Original;
			TArray<FTransform, TInlineAllocator<80>> Desired;
			Original.SetNum(Output.Pose.GetNumBones());
			Desired.SetNum(Original.Num());

			for (int32 I = 0; I < Original.Num(); ++I)
			{
				const FCompactPoseBoneIndex Index(I);
				const int32 Parent = Bones.GetParentBoneIndex(Index).GetInt();
				Original[I] = Parent == INDEX_NONE ? Output.Pose[Index] : Output.Pose[Index] * Original[Parent];
			}

			const int32 Hip = Pelvis.GetCompactPoseIndex(Bones).GetInt();
			const FVector Offset(0.f, -8.f * Weight, -Drop * Weight);
			FTransform Solved[2][3];
			int32 Indices[2][3];

			for (int32 Side = 0; Side < 2; ++Side)
			{
				for (int32 Joint = 0; Joint < 3; ++Joint)
				{
					Indices[Side][Joint] = Legs[Side][Joint].GetCompactPoseIndex(Bones).GetInt();
					Solved[Side][Joint] = Original[Indices[Side][Joint]];
					Solved[Side][Joint].AddToTranslation(Offset);
				}

				FVector Foot = Original[Indices[Side][2]].GetLocation();
				// Shorten the stride in crouch instead of stretching the bent legs.
				Foot.Y *= 1.f - 0.45f * Weight;
				Foot.X = FMath::Lerp(Foot.X, Side == 0 ? 12.0 : -12.0, 0.45 * Weight);
				const FVector Pole = Solved[Side][0].GetLocation() + FVector(0.f, 100.f, 0.f);
				AnimationCore::SolveTwoBoneIK(
				    Solved[Side][0], Solved[Side][1], Solved[Side][2], Pole, Foot, false, 1.f, 1.f);
			}

			for (int32 I = 0; I < Original.Num(); ++I)
			{
				const FCompactPoseBoneIndex Index(I);
				const int32 Parent = Bones.GetParentBoneIndex(Index).GetInt();
				Desired[I] = Parent == INDEX_NONE ? Output.Pose[Index] : Output.Pose[Index] * Desired[Parent];

				if (I == Hip)
					Desired[I].AddToTranslation(Offset);

				for (int32 Side = 0; Side < 2; ++Side)
					for (int32 Joint = 0; Joint < 3; ++Joint)
						if (I == Indices[Side][Joint])
							Desired[I] = Solved[Side][Joint];
			}

			for (int32 I = 0; I < Original.Num(); ++I)
			{
				const FCompactPoseBoneIndex Index(I);
				const int32 Parent = Bones.GetParentBoneIndex(Index).GetInt();
				Output.Pose[Index] =
				    Parent == INDEX_NONE ? Desired[I] : Desired[I].GetRelativeTransform(Desired[Parent]);
				Output.Pose[Index].NormalizeRotation();
			}
		}
	};

	struct FMazeCharacterAnimProxy : FAnimInstanceProxy
	{
		FAnimNode_BlendSpacePlayer_Standalone Locomotion;
		FAnimNode_SequencePlayer_Standalone Jump, Fall, Land;
		FMazeRestartBlend JumpFall, GroundAir, Landing;
		FMazeCrouchPose Crouch;
		bool bWasFalling = false;
		float LandingTime = 10.f;

		explicit FMazeCharacterAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
		{
			JumpFall.A.SetLinkNode(&Jump);
			JumpFall.B.SetLinkNode(&Fall);
			GroundAir.A.SetLinkNode(&Locomotion);
			GroundAir.B.SetLinkNode(&JumpFall);
			Landing.A.SetLinkNode(&GroundAir);
			Landing.B.SetLinkNode(&Land);
			Crouch.Input.SetLinkNode(&Landing);
		}

		virtual FAnimNode_Base* GetCustomRootNode() override
		{
			return &Crouch;
		}

		virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override
		{
			Nodes = {&Locomotion, &Jump, &Fall, &Land, &JumpFall, &GroundAir, &Landing, &Crouch};
		}

		virtual void Initialize(UAnimInstance* Instance) override
		{
			const auto* Animation = CastChecked<UMazeCharacterAnimInstance>(Instance);
			Locomotion.SetBlendSpace(Animation->Locomotion);
			Locomotion.SetLoop(true);
			Jump.SetSequence(Animation->Jump);
			Jump.SetLoopAnimation(false);
			Fall.SetSequence(Animation->Fall);
			Fall.SetLoopAnimation(true);
			Land.SetSequence(Animation->Land);
			Land.SetLoopAnimation(false);
			bWasFalling = false;
			LandingTime = 10.f;
			Crouch.Weight = GroundAir.Alpha = Landing.Alpha = JumpFall.Alpha = 0.f;
			FAnimInstanceProxy::Initialize(Instance);
		}

		virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
		{
			FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
			const auto* Character = Cast<ACharacter>(Instance->GetOwningActor());

			if (!Character)
				return;

			const auto* Movement = Character->GetCharacterMovement();
			const FVector Velocity = Character->GetActorRotation().UnrotateVector(Movement->Velocity);
			const float Speed = Velocity.Size2D();
			const bool bFalling = Movement->IsFalling();
			const bool bCrouched = Movement->IsCrouching();
			const float Scale = FMath::Max(0.01f, Instance->GetSkelMeshComponent()->GetComponentScale().Z);
			const float Run = bCrouched ? 0.f
			                            : FMath::Clamp((Speed - FMazePlayerControlDefinition::WalkSpeed) /
			                                               (FMazePlayerControlDefinition::SprintSpeed -
			                                                FMazePlayerControlDefinition::WalkSpeed),
			                                           0.f,
			                                           1.f);
			const float Direction = Speed > 3.f ? FMath::RadiansToDegrees(FMath::Atan2(Velocity.Y, Velocity.X)) : 0.f;
			Locomotion.SetPosition(FVector(Direction, FMath::Min(Speed / 60.f, 1.f) + Run, 0.f));
			// Measured displacement / clip duration before making the baked clips in-place.
			const float StrideSpeed = FMath::Lerp(311.f, 621.f, Run) * Scale * (1.f - 0.45f * Crouch.Weight);
			Locomotion.SetPlayRate(Speed > 3.f ? FMath::Clamp(Speed / StrideSpeed, 0.15f, 2.f) : 1.f);
			Crouch.Weight = FMath::FInterpConstantTo(Crouch.Weight, bCrouched ? 1.f : 0.f, DeltaSeconds, 1.f / 0.55f);
			const auto* Defaults = Character->GetClass()->GetDefaultObject<ACharacter>();
			Crouch.Drop = 2.f *
			              (Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() -
			               FMazePlayerControlDefinition::CrouchedHalfHeight) /
			              Scale;

			if (!bFalling && bWasFalling)
				LandingTime = 0.f;
			else
				LandingTime += DeltaSeconds;

			const float LandingWeight = !bFalling ? FMath::Clamp(1.f - LandingTime / 0.3f, 0.f, 1.f) : 0.f;
			Landing.Alpha = FMath::FInterpTo(Landing.Alpha, LandingWeight, DeltaSeconds, 25.f);
			GroundAir.Alpha = FMath::FInterpTo(GroundAir.Alpha, bFalling ? 1.f : 0.f, DeltaSeconds, 16.f);
			JumpFall.Alpha = FMath::FInterpTo(JumpFall.Alpha, Velocity.Z < 20.f ? 1.f : 0.f, DeltaSeconds, 12.f);
			bWasFalling = bFalling;
		}
	};
}

UMazeCharacterAnimInstance::UMazeCharacterAnimInstance()
{
	static ConstructorHelpers::FObjectFinder<UBlendSpace> MoveAsset(
	    TEXT("/Game/Characters/HazmatSuit3/Animations/BS_HazmatLocomotion"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpAsset(
	    TEXT("/Game/Characters/HazmatSuit3/Animations/HZ_MM_Jump"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> FallAsset(
	    TEXT("/Game/Characters/HazmatSuit3/Animations/HZ_MM_Fall_Loop"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> LandAsset(
	    TEXT("/Game/Characters/HazmatSuit3/Animations/HZ_MM_Land"));

	Locomotion = MoveAsset.Object;
	Jump = JumpAsset.Object;
	Fall = FallAsset.Object;
	Land = LandAsset.Object;
}

FAnimInstanceProxy* UMazeCharacterAnimInstance::CreateAnimInstanceProxy()
{
	return new FMazeCharacterAnimProxy(this);
}

void UMazeCharacterAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
	delete static_cast<FMazeCharacterAnimProxy*>(Proxy);
}
