#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "MassEntityQuery.h"
#include "ECS/MazeECSFragments.h"
#include "ECS/MazeItems.h"
#include "ECS/MazeDiagnostics.h"
#include "ECS/MazeDoorSystem.h"
#include "MazeECSSubsystem.generated.h"

enum class EMazeInputAxis
{
	Forward,
	Right,
	Yaw,
	Pitch
};

struct FMazeInterior;
struct FMazeChunkJob;
struct FMazeChunkData;

enum class EMazeInputAction
{
	Sprint,
	Crouch,
	Jump
};

// Game-thread ECS scheduling, lifecycle and engine bridge API for this world.
UCLASS()
class LABY_API UMazeECSSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	FMassEntityHandle CreatePlayer();
	void DestroyPlayer(FMassEntityHandle Entity);
	FMazeItemsSnapshot ReadItems(FMassEntityHandle Entity) const;
	bool ReadHeadlampEnabled(FMassEntityHandle Entity) const;
	bool ToggleHeadlamp(FMassEntityHandle Entity);
	FMazeWorldItemView ReadWorldItem(FMassEntityHandle Maze) const;
	bool PickupWorldItem(FMassEntityHandle Player,
	                     FMassEntityHandle Maze,
	                     int32 ItemId,
	                     const FVector& EyeLocation,
	                     const FVector& AimDirection);
	void ReceiveWorldItemAvailability(FMassEntityHandle Maze, bool bAvailable);
	void ReceiveItems(FMassEntityHandle Entity, const FMazeItemsSnapshot& Snapshot);
	FMazeVitals ReadVitals(FMassEntityHandle Entity) const;
	void SetLocomotion(FMassEntityHandle Entity, bool bRunning, bool bOnGround);
	bool SpendJumpStamina(FMassEntityHandle Entity);
	float ApplyDamage(FMassEntityHandle Entity, float Amount);
	void SetInputAxis(FMassEntityHandle Entity, EMazeInputAxis Axis, float Value);
	void SetInputAction(FMassEntityHandle Entity, EMazeInputAction Action, bool bPressed);
	void ClearPlayerInput();
	void ClearInput(FMassEntityHandle Entity);
	FMazePlayerCommandFragment ResolvePlayer(FMassEntityHandle Entity,
	                                         const FMazePlayerPoseFragment& Pose,
	                                         const FVector& FeetLocation,
	                                         const FVector& EyeLocation);
	bool RequestSignal(FMassEntityHandle Entity, FMazeSignalSnapshot& OutSignal);
	bool ReceiveSignal(FMassEntityHandle Entity, const FMazeSignalSnapshot& Signal);
	FMazeSignalView ReadSignal(FMassEntityHandle Entity) const;
	int32 ReadReachedExit(FMassEntityHandle Entity) const;
	FMassEntityHandle CreateMaze(int32 Seed, FVector Origin);
	void DestroyMaze(FMassEntityHandle Entity);
	void RegenerateMaze(FMassEntityHandle Entity, int32 Seed, FVector Origin);
	FMazeGenerationFragment ReadMaze(FMassEntityHandle Entity) const;
	TArray<FMazeDoorView> ReadDoors(FMassEntityHandle Maze) const;
	bool ToggleDoor(FMassEntityHandle Player, int32 DoorIndex, const FVector& Eye, const FVector& Aim);
	TArray<uint8> ReadDoorTargets(FMassEntityHandle Maze) const;
	void ReceiveDoorTargets(FMassEntityHandle Maze, TConstArrayView<uint8> Targets);
	TSharedPtr<const FMazeInterior> BuildMazeLampLocations(FMassEntityHandle Entity) const;
	TSharedPtr<const FMazeSurface> BuildMazeCollision(FMassEntityHandle Entity) const;
	TSharedPtr<FMazeChunkJob> RequestMazeChunk(FMassEntityHandle Entity, FIntPoint Chunk, int32 ChunkCells) const;
	TSharedPtr<const FMazeChunkData, ESPMode::ThreadSafe> TakeMazeChunk(const TSharedPtr<FMazeChunkJob>& Job) const;
	FMazeSessionFragment ReadSession() const;
	FMazeDiagnosticsSnapshot ReadDiagnostics() const;
	void SetSessionStarted(bool bStarted);
	void SetMenu(bool bOpen, bool bSettings = false);
	void SetMapOpen(bool bOpen);
	const FMazeExplorationFragment* ReadExploration(FMassEntityHandle Entity) const;
	FMazeRoomFragment ReadRoom() const;
	FString RoomAdmissionError() const;
	void ReceiveRoom(const FMazeRoomFragment& Room);
	void OpenRoom();
	bool AddRoomMember(int32 Id, const FString& Name, bool bHost);
	void RemoveRoomMember(int32 Id);
	bool StartRoom(int32 Requester);
	FVector RoomSpawn(int32 Id) const;
	bool IsSprintHeld(FMassEntityHandle Entity) const;
	void ReceivePlayer(FMassEntityHandle Entity, const FMazeVitals& Vitals, int32 Exit);

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	UPROPERTY()
	TObjectPtr<class UMassEntitySubsystem> MassSubsystem;
	TUniquePtr<FMassEntityQuery> VitalsQuery;
	TUniquePtr<FMassEntityQuery> GenerationQuery;
	TUniquePtr<FMassEntityQuery> InputQuery;
	TUniquePtr<FMassEntityQuery> SignalQuery;
	TUniquePtr<FMassEntityQuery> DoorQuery;
	FMassArchetypeHandle PlayerArchetype;
	FMassArchetypeHandle MazeArchetype;
	FMassArchetypeHandle DoorArchetype;
	FMassArchetypeHandle WorldItemArchetype;
	FMassEntityHandle SessionEntity;
	TArray<FMassEntityHandle> DoorEntities;
	TArray<FMassEntityHandle> WorldItemEntities;

	FMazeVitals* FindVitals(FMassEntityHandle Entity) const;
	void GeneratePending();
	void RebuildDoors(FMassEntityHandle MazeEntity);
	void DestroyDoors(FMassEntityHandle MazeEntity);
	void RebuildWorldItems(FMassEntityHandle MazeEntity);
	void DestroyWorldItems(FMassEntityHandle MazeEntity);
	void UpdateProgress(FMassEntityHandle Entity);
	template <typename T> T* FindFragment(FMassEntityHandle Entity) const;
};
