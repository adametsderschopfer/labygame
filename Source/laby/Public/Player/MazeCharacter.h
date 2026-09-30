#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ECS/MazeVitals.h"
#include "ECS/MazeItems.h"
#include "ECS/MazeSignal.h"
#include "Mass/EntityHandle.h"
#include "InputCoreTypes.h"
#include "Player/MazeCameraMotion.h"
#include "ECS/MazeNoise.h"
#include "MazeCharacter.generated.h"

UCLASS()
class LABY_API AMazeCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AMazeCharacter();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void UnPossessed() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual float TakeDamage(float DamageAmount,
	                         const FDamageEvent& DamageEvent,
	                         AController* EventInstigator,
	                         AActor* DamageCauser) override;
	FMazeVitals GetVitals() const;
	FMazeItemsSnapshot GetItems() const;
	bool GetFocusedPickup(TArray<FVector>& OutOutline, FVector& OutLocation, EMazeItemKind* OutKind = nullptr) const;
	bool GetFocusedDoor(FVector& OutHandle, bool& bOutOpen, bool* bOutLocked = nullptr) const;
	FMassEntityHandle GetPlayerEntity() const
	{
		return PlayerEntity;
	}

	void ClearLocalInput();
	void RestoreHeldMovementInput();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	void ToggleDevelopmentCamera();
	virtual void FellOutOfWorld(const class UDamageType& DamageType) override;
#endif
	int32 GetReachedExit() const;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	FMazeCameraMotion CameraMotion;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	bool bDevelopmentThirdPerson = false;

	void SetDevelopmentThirdPerson(bool bEnabled);

#endif

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<class USpotLightComponent> HeadlampLight;

	UPROPERTY()
	TObjectPtr<class UStaticMeshComponent> HeldCard;

	UPROPERTY()
	TObjectPtr<class UStaticMeshComponent> HeldCardStripe;

	UPROPERTY(ReplicatedUsing = OnRep_Items)
	FMazeItemsSnapshot ReplicatedItems;

	UFUNCTION()
	void OnRep_Items();
	void RefreshHeadlamp();

	UPROPERTY(ReplicatedUsing = OnRep_PlayerSnapshot)
	FMazeVitals ReplicatedVitals;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerSnapshot)
	int32 ReplicatedExit = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Signal)
	FMazeSignalSnapshot ReplicatedSignal;

	UPROPERTY(ReplicatedUsing = OnRep_Noise)
	FMazeNoiseSnapshot ReplicatedNoise;

	UFUNCTION()
	void OnRep_Noise();

	bool bPendingNoiseSnapshot = false;

	UFUNCTION()
	void OnRep_PlayerSnapshot();
	UFUNCTION()
	void OnRep_Signal();
	UFUNCTION(Server, Reliable)
	void ServerSetSprint(bool bHeld);
	UFUNCTION(Server, Reliable)
	void ServerToggleHeadlamp();
	UFUNCTION(Server, Reliable)
	void ServerSelectInventorySlot(int32 Slot);
	UFUNCTION(Server, Reliable)
	void ServerCycleInventorySlot(int32 Step);
	UFUNCTION(Server, Reliable)
	void ServerPickupItem();
	UFUNCTION(Server, Reliable)
	void ServerDropItem();
	UFUNCTION(Server, Reliable)
	void ServerToggleDoor(int32 DoorIndex);
	UFUNCTION(Server, Reliable)
	void ServerRequestSignal();
	void ToggleHeadlamp();
	void SelectInventorySlot(FKey Key);
	void PickupItem();
	void DropItem();
	void GetInteractionView(FVector& OutEye, FVector& OutAim) const;
	class AMazeWorld* TraceFocusedWorldItem(int32* OutItemId = nullptr) const;
	void CycleInventorySlot(FKey Key);
	int32 TraceFocusedDoor(FVector& OutHandle, bool& bOutOpen) const;
	void RequestSignal();
	void PlaySignal(const FMazeSignalSnapshot& Signal);
	void InitializeSignalAudio();

	UPROPERTY()
	TObjectPtr<class USoundBase> SignalSound;

	UPROPERTY(Transient)
	TObjectPtr<class USoundAttenuation> SignalAttenuation;

	UPROPERTY(Transient)
	TObjectPtr<class USoundSubmix> SignalReverbSubmix;

	UPROPERTY(Transient)
	TObjectPtr<class USubmixEffectReverbPreset> SignalReverbPreset;

	UPROPERTY(Transient)
	FMassEntityHandle PlayerEntity;

	UPROPERTY(Transient)
	TObjectPtr<class UMazeECSSubsystem> ECSSubsystem;

	void Forward(float Value);
	void Right(float Value);
	void LookUp(float Value);
	void Turn(float Value);
	void SprintStart();
	void SprintStop();
	void CrouchStart();
	void CrouchStop();
	void RefreshStancePresentation();
	void JumpStart();
	void JumpStop();
};
