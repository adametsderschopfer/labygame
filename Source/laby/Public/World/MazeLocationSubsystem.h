#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RenderCommandFence.h"
#include "World/MazeLocationSettings.h"
#include "World/MazePreparationStatus.h"
#include "MazeLocationSubsystem.generated.h"

struct FStreamableHandle;

// Per-world engine resource adapter. Does not own session permissions or gameplay state.
UCLASS()
class LABY_API UMazeLocationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override
	{
		return true;
	}

	void ReportReady(UObject* Participant, bool bReady, int32 Completed = 0, int32 Total = 0);
	void ReportBlockingStage(EMazePreparationStage Stage);
	FMazePreparationStatus ReadPreparationStatus() const;
	void RemoveParticipant(UObject* Participant);
	bool AreAssetsReady() const
	{
		return bAssetsReady;
	}

	bool IsReady() const
	{
		return bReady;
	}

	const FString& GetFailure() const
	{
		return Failure;
	}

	void Fail(const FString& Reason);
	EMazeLocationMode GetMode() const
	{
		return Mode;
	}

	int32 GetPendingPSOs() const
	{
		return PendingPSOs;
	}

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;

private:
	TSharedPtr<FStreamableHandle> AssetLoad;
	TArray<FSoftObjectPath> Dependencies;
	struct FParticipant
	{
		bool bReady = false;
		int32 Completed = 0;
		int32 Total = 0;
	};
	TMap<TWeakObjectPtr<UObject>, FParticipant> Participants;
	EMazeLocationMode Mode = EMazeLocationMode::Whole;
	FString Failure;
	int32 StableFrames = 0;
	int32 PendingPSOs = 0;
	bool bAssetsReady = false;
	bool bReady = false;
	bool bReported = false;
	FRenderCommandFence PreparationFence;
	bool bFenceStarted = false;
};
