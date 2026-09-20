#include "World/MazeLocationSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "ShaderPipelineCache.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "World/MazeOnlineGameInstance.h"

void UMazeLocationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const auto* Settings = GetDefault<UMazeLocationSettings>();

	if (!Settings->Validate(Failure))
	{
		Fail(Failure);

		return;
	}

	FString Package = GetWorld()->GetOutermost()->GetName();
	// PIE prefixes only the short package name.
	const FString Prefix = GetWorld()->StreamingLevelsPrefix;

	if (!Prefix.IsEmpty())
		Package.ReplaceInline(*Prefix, TEXT(""));

	for (const auto& Location : Settings->Locations)
		if (Location.Map.GetLongPackageName() == Package)
		{
			Mode = Location.Mode;
			Dependencies = Location.Assets;
			break;
		}

	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
		Dependencies.Reset();

	if (Dependencies.IsEmpty())
		bAssetsReady = true;
	else
		AssetLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Dependencies);

	if (!Dependencies.IsEmpty() && !AssetLoad)
		Fail(TEXT("Unable to request location dependencies"));
}

void UMazeLocationSubsystem::Fail(const FString& Reason)
{
	Failure = Reason;
	bReady = false;
	UE_LOG(LogTemp, Error, TEXT("Location preparation failed: %s"), *Failure);
}

void UMazeLocationSubsystem::ReportReady(UObject* Participant, bool bParticipantReady)
{
	if (!IsValid(Participant) || Participant->GetWorld() != GetWorld())
		return;

	Participants.FindOrAdd(Participant) = bParticipantReady;

	if (!bParticipantReady)
	{
		StableFrames = 0;
		bFenceStarted = false;
		bReady = false;
	}
}

void UMazeLocationSubsystem::RemoveParticipant(UObject* Participant)
{
	Participants.Remove(Participant);
}

void UMazeLocationSubsystem::Tick(float DeltaSeconds)
{
	if (!bAssetsReady && AssetLoad && AssetLoad->HasLoadCompleted())
	{
		bAssetsReady = true;

		for (const auto& Path : Dependencies)
			if (!Path.ResolveObject())
			{
				Fail(FString::Printf(TEXT("Missing location dependency: %s"), *Path.ToString()));
				break;
			}
	}

	bool bComplete = bAssetsReady && Failure.IsEmpty();

	if (Mode == EMazeLocationMode::Procedural && Participants.IsEmpty())
		bComplete = false; // Clients must wait for the replicated world adapter and seed.

	for (auto It = Participants.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
			It.RemoveCurrent();
		else
			bComplete &= It.Value();
	}

	if (Mode == EMazeLocationMode::WorldPartition)
	{
		if (!GetWorld()->GetWorldPartition() && Failure.IsEmpty())
		{
			Fail(TEXT("Location declares WorldPartition but the map is not partitioned"));
			bComplete = false;
		}

		if (const auto* Partition = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>())
			bComplete &= Partition->IsStreamingCompleted();
	}

	PendingPSOs = GetWorld()->GetNetMode() == NM_DedicatedServer
	                  ? 0
	                  : static_cast<int32>(FShaderPipelineCache::NumPrecompilesRemaining());

	// Only initial/revision readiness waits globally; unrelated later PSOs do not stop gameplay.
	if (!bReady)
	{
		if (!bComplete || PendingPSOs != 0)
			bFenceStarted = false;

		StableFrames = bComplete && PendingPSOs == 0 ? StableFrames + 1 : 0;

		if (StableFrames >= 2 && !bFenceStarted)
		{
			PreparationFence.BeginFence();
			bFenceStarted = true;
		}

		bReady = StableFrames >= 3 && bFenceStarted && PreparationFence.IsFenceComplete();
	}

	if (bReady && !bReported)
	{
		UE_LOG(
		    LogTemp, Display, TEXT("Location ready: %s, dependencies=%d"), *GetWorld()->GetName(), Dependencies.Num());
		bReported = true;
	}

	if (auto* Online = GetWorld()->GetGameInstance<UMazeOnlineGameInstance>())
		Online->UpdatePreparationScreen(GetWorld(), !bReady, Failure);
}

void UMazeLocationSubsystem::Deinitialize()
{
	if (AssetLoad)
		AssetLoad->CancelHandle();

	AssetLoad.Reset();
	Participants.Reset();
	Super::Deinitialize();
}

TStatId UMazeLocationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(MazeLocationPreparation, STATGROUP_Tickables);
}

bool UMazeLocationSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE;
}
