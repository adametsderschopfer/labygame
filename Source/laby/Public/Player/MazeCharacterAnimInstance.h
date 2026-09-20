#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MazeCharacterAnimInstance.generated.h"

// Presentation only. CharacterMovement supplies observed velocity and stance.
UCLASS(Transient)
class LABY_API UMazeCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	UMazeCharacterAnimInstance();
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;

	UPROPERTY()
	TObjectPtr<class UBlendSpace> Locomotion;

	UPROPERTY()
	TObjectPtr<class UAnimSequence> Jump;

	UPROPERTY()
	TObjectPtr<class UAnimSequence> Fall;

	UPROPERTY()
	TObjectPtr<class UAnimSequence> Land;
};
