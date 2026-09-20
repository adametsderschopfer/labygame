#include "Player/MazeCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void AMazeCharacter::ToggleDevelopmentCamera()
{
	if (IsLocallyControlled())
		SetDevelopmentThirdPerson(!bDevelopmentThirdPerson);
}

void AMazeCharacter::SetDevelopmentThirdPerson(bool bEnabled)
{
	bDevelopmentThirdPerson = bEnabled;
	CameraMotion = FMazeCameraMotion();

	TInlineComponentArray<UCameraComponent*> Cameras(this);

	for (auto* Camera : Cameras)
	{
		if (Camera->GetFName() == TEXT("FirstPersonCamera"))
			Camera->SetActive(!bEnabled);
		else if (Camera->GetFName() == TEXT("DevelopmentThirdPersonCamera"))
			Camera->SetActive(bEnabled);
	}

	GetMesh()->SetOwnerNoSee(!bEnabled);

	TInlineComponentArray<USkeletalMeshComponent*> BodyMeshes(this);

	for (auto* Body : BodyMeshes)
		if (Body->GetFName() == TEXT("FirstPersonBody"))
			Body->SetVisibility(!bEnabled);
}

#endif
