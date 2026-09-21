#include "World/MazeWorld.h"
#include "World/MazeChunkView.h"
#include "World/MazeLocationSubsystem.h"
#include "World/MazeLocationSettings.h"
#include "ECS/MazeChunkSystem.h"
#include "ECS/MazeECSSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

void AMazeWorld::ClearChunks()
{
	for (auto& Pair : ChunkViews)
		if (IsValid(Pair.Value))
			Pair.Value->Destroy();

	ChunkViews.Reset();
	// The worker owns its immutable snapshot and never calls back into a destroyed world.
	PendingChunk.Reset();
	VisualMaterials.Reset();
}

int64 AMazeWorld::GetResidentGeometryBytes() const
{
	int64 Total = 0;

	for (const auto& Pair : ChunkViews)
		if (IsValid(Pair.Value))
			Total += Pair.Value->GeometryBytes;

	return Total;
}

void AMazeWorld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!ECSSubsystem || !GetGeneratedData())
		return;

	auto* Location = GetWorld()->GetSubsystem<UMazeLocationSubsystem>();

	if (!Location->GetFailure().IsEmpty())
		return;

	if (GetNetMode() == NM_DedicatedServer)
	{
		Location->ReportReady(this, true);

		return;
	}

	if (!Location->AreAssetsReady())
		return;

	if (!bPresentationStarted)
		PrepareMaterials();

	UpdateChunks();
}

void AMazeWorld::UpdateChunks()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Maze_StreamViews);

	const auto* Settings = GetDefault<UMazeLocationSettings>();
	const auto Data = GetGeneratedData();
	const int32 Count = FMath::DivideAndRoundUp(Data->Layout.Size, Settings->ChunkCells);
	const float Width = GetCellSize() * Settings->ChunkCells;
	TArray<FIntPoint> Sources;

	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (auto* PC = It->Get(); PC && PC->IsLocalController())
		{
			const FVector Position = PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : StartLocation();
			const FVector Local = Position - GetActorLocation();
			Sources.AddUnique(FIntPoint(FMath::Clamp(FMath::FloorToInt(Local.X / Width), 0, Count - 1),
			                            FMath::Clamp(FMath::FloorToInt(Local.Y / Width), 0, Count - 1)));
		}

	if (Sources.IsEmpty())
		Sources.Add(FIntPoint(FMath::Clamp(FMath::FloorToInt(Data->Start.X / Width), 0, Count - 1),
		                      FMath::Clamp(FMath::FloorToInt(Data->Start.Y / Width), 0, Count - 1)));

	const auto Distance = [&Sources](FIntPoint Coordinate)
	{
		int32 Result = MAX_int32;

		for (FIntPoint Source : Sources)
			Result = FMath::Min(Result,
			                    FMath::Max(FMath::Abs(Source.X - Coordinate.X), FMath::Abs(Source.Y - Coordinate.Y)));

		return Result;
	};
	TArray<FIntPoint> Wanted;

	for (int32 Y = 0; Y < Count; ++Y)
		for (int32 X = 0; X < Count; ++X)
			if (Distance(FIntPoint(X, Y)) <= Settings->LoadRadius)
				Wanted.Add(FIntPoint(X, Y));

	Wanted.Sort(
	    [&Distance](FIntPoint A, FIntPoint B)
	    {
		    const int32 DA = Distance(A), DB = Distance(B);

		    return DA != DB ? DA < DB : (A.Y != B.Y ? A.Y < B.Y : A.X < B.X);
	    });

	// One worker per maze bounds peak scratch memory and prevents unbounded task queues.
	if (PendingChunk && PendingChunk->Result.IsReady())
	{
		const auto Chunk = ECSSubsystem->TakeMazeChunk(PendingChunk);

		PendingChunk.Reset();

		if (Chunk && Wanted.Contains(Chunk->Coordinate) && !ChunkViews.Contains(Chunk->Coordinate))
		{
			const double Begin = FPlatformTime::Seconds();
			auto* View = GetWorld()->SpawnActor<AMazeChunkView>(GetActorLocation(), GetActorRotation());

			if (!View)
			{
				GetWorld()->GetSubsystem<UMazeLocationSubsystem>()->Fail(TEXT("Unable to create chunk view"));

				return;
			}

			View->SetOwner(this);

			TArray<UMaterialInterface*> Materials;

			for (const auto& Material : VisualMaterials)
				Materials.Add(Material.Get());

			View->Apply(*Chunk, Floor->GetStaticMesh(), Materials);
			ChunkViews.Add(Chunk->Coordinate, View);

			const double Milliseconds = (FPlatformTime::Seconds() - Begin) * 1000;

			if (Milliseconds > Settings->CommitBudgetMs)
				UE_LOG(LogTemp, Verbose, TEXT("Chunk commit exceeded soft budget: %.2f ms"), Milliseconds);
		}
	}

	if (!PendingChunk)
		for (FIntPoint Coordinate : Wanted)
			if (!ChunkViews.Contains(Coordinate))
			{
				PendingChunk = ECSSubsystem->RequestMazeChunk(MazeEntity, Coordinate, Settings->ChunkCells);
				break;
			}

	TArray<FIntPoint> Evict;

	for (const auto& Pair : ChunkViews)
		if (Distance(Pair.Key) > Settings->UnloadRadius ||
		    (ChunkViews.Num() > Settings->MaxRetainedChunks && !Wanted.Contains(Pair.Key)))
			Evict.Add(Pair.Key);

	Evict.Sort(
	    [&Distance](FIntPoint A, FIntPoint B)
	    {
		    return Distance(A) > Distance(B);
	    });

	// Destruction also has an engine cost: release at most one view per frame.
	if (!Evict.IsEmpty())
	{
		const auto View = ChunkViews.FindAndRemoveChecked(Evict[0]);

		if (IsValid(View))
			View->Destroy();
	}

	int32 Present = 0;

	for (FIntPoint Coordinate : Wanted)
		Present += ChunkViews.Contains(Coordinate) ? 1 : 0;

	// First entry waits for the entire preload area. Later movement only gates a missing current chunk.
	if (Present == Wanted.Num())
		bInitialChunksReady = true;

	bool bCurrentPresent = true;

	for (FIntPoint Source : Sources)
		bCurrentPresent &= ChunkViews.Contains(Source);

	GetWorld()->GetSubsystem<UMazeLocationSubsystem>()->ReportReady(
	    this, bInitialChunksReady && bCurrentPresent, Present, Wanted.Num());
}
