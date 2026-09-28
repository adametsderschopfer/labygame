#include "World/MazeWorld.h"
#include "World/MazeChunkView.h"
#include "World/MazeLocationSubsystem.h"
#include "World/MazeLocationSettings.h"
#include "ECS/MazeChunkSystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "World/MazeWindAudio.h"
#include "World/MazeLampAudio.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ECS/MazeECSSubsystem.h"
#include "Maze/MazeInterior.h"
#include "World/MazeFixtureMeshes.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AMazeWorld::AMazeWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	bAlwaysRelevant = true;
	Walls = CreateDefaultSubobject<UMazeCollisionMeshComponent>(TEXT("Walls"));
	SetRootComponent(Walls);
	Floor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(Walls);
	Ceiling = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ceiling"));
	Ceiling->SetupAttachment(Walls);
	HeadlampBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadlampBody"));
	HeadlampBody->SetupAttachment(Walls);
	HeadlampLens = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadlampLens"));
	HeadlampLens->SetupAttachment(Walls);
	DoorFrames = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DoorFrames"));
	DoorFrames->SetupAttachment(Walls);
	DoorLeaves = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DoorLeaves"));
	DoorLeaves->SetupAttachment(Walls);
	DoorHandles = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DoorHandles"));
	DoorHandles->SetupAttachment(Walls);
	DoorCollision = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DoorCollision"));
	DoorCollision->SetupAttachment(Walls);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlainMaterial(
	    TEXT("/Game/Materials/M_MazePlain.M_MazePlain"));

	Floor->SetStaticMesh(Cube.Object);
	HeadlampBody->SetStaticMesh(Cylinder.Object);
	HeadlampBody->SetRelativeScale3D(FVector(.12f, .12f, .32f));
	HeadlampBody->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	HeadlampBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadlampBody->SetCollisionResponseToAllChannels(ECR_Ignore);
	HeadlampBody->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HeadlampBody->SetCanEverAffectNavigation(false);
	HeadlampLens->SetStaticMesh(Cylinder.Object);
	HeadlampLens->SetRelativeScale3D(FVector(.145f, .145f, .035f));
	HeadlampLens->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	HeadlampLens->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadlampLens->SetCollisionResponseToAllChannels(ECR_Ignore);
	HeadlampLens->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HeadlampLens->SetCanEverAffectNavigation(false);
	HeadlampBody->SetVisibility(false);
	HeadlampLens->SetVisibility(false);
	Floor->SetMaterial(0, PlainMaterial.Object);
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
	Ceiling->SetStaticMesh(Cube.Object);
	Ceiling->SetMaterial(0, PlainMaterial.Object);
	Ceiling->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ceiling->SetCanEverAffectNavigation(false);
	Walls->SetMaterial(0, PlainMaterial.Object);
	Walls->SetCollisionProfileName(TEXT("BlockAll"));
	Walls->bUseComplexAsSimpleCollision = true;
	Floor->SetCanEverAffectNavigation(false);
	Walls->SetCanEverAffectNavigation(false);

	for (auto* Component : {DoorFrames.Get(), DoorLeaves.Get(), DoorHandles.Get()})
	{
		Component->SetStaticMesh(Cube.Object);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetMobility(EComponentMobility::Movable);
	}

	DoorCollision->SetStaticMesh(Cube.Object);
	DoorCollision->SetCollisionProfileName(TEXT("BlockAll"));
	DoorCollision->SetCanEverAffectNavigation(false);
	DoorCollision->SetMobility(EComponentMobility::Movable);
	DoorCollision->SetHiddenInGame(true);
	DoorCollision->SetVisibility(false);
	// Resident physics is independent of camera-local rendering and streaming.
	Walls->SetHiddenInGame(true);
	Floor->SetHiddenInGame(true);
	Ceiling->SetHiddenInGame(true);
	Walls->SetVisibility(false);
	Floor->SetVisibility(false);
	Ceiling->SetVisibility(false);
}

void AMazeWorld::BeginPlay()
{
	Super::BeginPlay();

	// Presentation only: owned components follow this world's maze lifetime.
	// Loading here also applies the material to existing CDOs after Live Coding.
	if (GetNetMode() != NM_DedicatedServer)
	{
		auto* Exposure = NewObject<UPostProcessComponent>(this, TEXT("NightExposure"));

		Exposure->SetupAttachment(RootComponent);
		Exposure->bUnbound = true;
		Exposure->Priority = 10.f;
		// Fixed interior exposure prevents bright ceiling panels from pumping eye adaptation.
		Exposure->Settings.bOverride_AutoExposureMinBrightness = true;
		Exposure->Settings.bOverride_AutoExposureMaxBrightness = true;
		Exposure->Settings.AutoExposureMinBrightness = 0.5f;
		Exposure->Settings.AutoExposureMaxBrightness = 0.5f;
		Exposure->Settings.bOverride_AutoExposureBias = true;
		Exposure->Settings.AutoExposureBias = 0.f;
		// Keep GI/reflection quality under the user's engine scalability profile.
		Exposure->Settings.bOverride_LumenFinalGatherQuality = false;
		Exposure->Settings.LumenFinalGatherQuality = 2.f;
		Exposure->Settings.bOverride_LumenReflectionQuality = false;
		Exposure->Settings.LumenReflectionQuality = 2.f;
		Exposure->Settings.bOverride_SceneFringeIntensity = true;
		Exposure->Settings.SceneFringeIntensity = 0.f;
		Exposure->RegisterComponent();
	}

	InitializeMaze();
}

void AMazeWorld::InitializeMaze()
{
	GetWorld()->GetSubsystem<UMazeLocationSubsystem>()->ReportReady(this, false);

	if (HasAuthority() || Seed != 0)
		Build();
}

void AMazeWorld::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearChunks();
	DoorFrames->ClearInstances();
	DoorLeaves->ClearInstances();
	DoorHandles->ClearInstances();
	DoorCollision->ClearInstances();
	AppliedDoorOpenAmounts.Reset();
	HeadlampBody->SetVisibility(false);
	HeadlampLens->SetVisibility(false);
	GetWorld()->GetSubsystem<UMazeLocationSubsystem>()->RemoveParticipant(this);
	MazeWindAudio::Stop(*this);
	MazeLampAudio::Stop(*this);

	if (ECSSubsystem)
		ECSSubsystem->DestroyMaze(MazeEntity);

	MazeEntity = FMassEntityHandle();
	ECSSubsystem = nullptr;
	Super::EndPlay(Reason);
}

void AMazeWorld::OnRep_Seed()
{
	InitializeMaze();
}

void AMazeWorld::OnRep_HeadlampAvailable()
{
	if (ECSSubsystem)
		ECSSubsystem->ReceiveWorldItemAvailability(MazeEntity, bReplicatedHeadlampAvailable);

	RefreshWorldItem();
}

bool AMazeWorld::IsPickupComponent(const UPrimitiveComponent* Component) const
{
	return Component && (Component == HeadlampBody || Component == HeadlampLens) && ECSSubsystem &&
	       ECSSubsystem->ReadWorldItem(MazeEntity).bAvailable;
}

bool AMazeWorld::GetPickupOutline(TArray<FVector>& OutPoints) const
{
	if (!IsPickupComponent(HeadlampBody))
		return false;

	OutPoints.Reset();

	for (const UStaticMeshComponent* Part : {HeadlampBody.Get(), HeadlampLens.Get()})
		for (const float End : {-50.f, 50.f})
			for (int32 Step = 0; Step < 16; ++Step)
			{
				const float Angle = 2.f * PI * Step / 16.f;
				const FVector Local(50.f * FMath::Cos(Angle), 50.f * FMath::Sin(Angle), End);

				OutPoints.Add(Part->GetComponentTransform().TransformPosition(Local));
			}

	return true;
}

void AMazeWorld::RefreshWorldItem()
{
	if (!ECSSubsystem)
		return;

	const FMazeWorldItemView Item = ECSSubsystem->ReadWorldItem(MazeEntity);

	if (HasAuthority() && bReplicatedHeadlampAvailable != Item.bAvailable)
	{
		bReplicatedHeadlampAvailable = Item.bAvailable;
		ForceNetUpdate();
	}

	const bool bVisible = Item.bAvailable && GetNetMode() != NM_DedicatedServer;

	if (!HeadlampBody->GetComponentLocation().Equals(Item.Location))
	{
		HeadlampBody->SetWorldLocation(Item.Location);
		HeadlampLens->SetWorldLocation(Item.Location + FVector(17.f, 0.f, 0.f));
	}

	if (HeadlampBody->IsVisible() != bVisible)
	{
		HeadlampBody->SetVisibility(bVisible);
		HeadlampLens->SetVisibility(bVisible);
	}

	const ECollisionEnabled::Type Collision =
	    Item.bAvailable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;

	if (HeadlampBody->GetCollisionEnabled() != Collision)
		HeadlampBody->SetCollisionEnabled(Collision);

	if (HeadlampLens->GetCollisionEnabled() != Collision)
		HeadlampLens->SetCollisionEnabled(Collision);
}

void AMazeWorld::OnRep_DoorTargets()
{
	if (ECSSubsystem)
		ECSSubsystem->ReceiveDoorTargets(MazeEntity, ReplicatedDoorTargets);
}

void AMazeWorld::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMazeWorld, Seed);
	DOREPLIFETIME(AMazeWorld, ReplicatedDoorTargets);
	DOREPLIFETIME(AMazeWorld, bReplicatedHeadlampAvailable);
}

FVector AMazeWorld::StartLocation() const
{
	if (ECSSubsystem)
	{
		const auto Maze = ECSSubsystem->ReadMaze(MazeEntity);

		if (Maze.Data)
			return Maze.Origin + Maze.Data->Start;
	}

	return GetActorLocation();
}

TSharedPtr<const FMazeGeneratedData, ESPMode::ThreadSafe> AMazeWorld::GetGeneratedData() const
{
	return ECSSubsystem ? ECSSubsystem->ReadMaze(MazeEntity).Data : nullptr;
}

float AMazeWorld::GetCellSize() const
{
	return ECSSubsystem ? ECSSubsystem->ReadMaze(MazeEntity).Cell : FMazeGenerationFragment().Cell;
}

void AMazeWorld::Build()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Maze_WorldBuild);

	if (!ECSSubsystem)
		ECSSubsystem = GetWorld()->GetSubsystem<UMazeECSSubsystem>();

	check(ECSSubsystem);

	const auto OldData = GetGeneratedData();
	auto* Location = GetWorld()->GetSubsystem<UMazeLocationSubsystem>();

	Location->ReportBlockingStage(EMazePreparationStage::Topology);

	if (!MazeEntity.IsSet())
		MazeEntity = ECSSubsystem->CreateMaze(Seed, GetActorLocation());
	else
		ECSSubsystem->RegenerateMaze(MazeEntity, Seed, GetActorLocation());

	const auto Maze = ECSSubsystem->ReadMaze(MazeEntity);

	if (!HasAuthority())
		ECSSubsystem->ReceiveWorldItemAvailability(MazeEntity, bReplicatedHeadlampAvailable);

	RefreshWorldItem();

	Seed = Maze.Seed; // Engine replication mirrors the ECS seed.

	const auto Data = Maze.Data;

	if (!Data || Data == OldData)
		return;

	const auto& Layout = Data->Layout;

	Location->ReportBlockingStage(EMazePreparationStage::Collision);

	Walls->ClearAllMeshSections();
	// Also refresh old editor archetypes after Live Coding: all ceiling physics
	// now belongs to the resident box instances, not the legacy single cube.
	Ceiling->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ceiling->SetHiddenInGame(true);
	Ceiling->SetVisibility(false);
	Floor->ClearInstances();
	Floor->AddInstances(Data->FloorTransforms, false);
	Floor->AddInstances(Data->CeilingTransforms, false);
	Floor->AddInstances(Data->RoomGeometry.Ceilings, false);

	for (const FMazeRoomBox& Box : Data->RoomGeometry.Boxes)
		if (Box.bCollision)
			Floor->AddInstance(Box.Transform, false);

	ClearChunks();
	bInitialChunksReady = false;
	bPresentationStarted = false;
	GetWorld()->GetSubsystem<UMazeLocationSubsystem>()->ReportReady(this, false);

	// Synchronous resident collision guarantees safe spawning and remote physics.
	// Generated CPU buffers are released after copying to the engine component.
	const auto Surface = ECSSubsystem->BuildMazeCollision(MazeEntity);

	if (Surface)
		Walls->CreateMeshSection(0,
		                         Surface->Vertices,
		                         Surface->Triangles,
		                         Surface->Normals,
		                         TArray<FVector2D>(),
		                         TArray<FColor>(),
		                         TArray<FProcMeshTangent>(),
		                         true);

	RebuildDoorInstances();

	if (!HasAuthority() && !ReplicatedDoorTargets.IsEmpty())
		ECSSubsystem->ReceiveDoorTargets(MazeEntity, ReplicatedDoorTargets);

	Location->ReportBlockingStage(EMazePreparationStage::Geometry);

	// Labels are local visual components; topology alone is replicated.
	TArray<UTextRenderComponent*> OldLabels;

	GetComponents(OldLabels);

	for (auto* Label : OldLabels)
		Label->DestroyComponent();

	for (int32 I = 0; I < Data->ExitPositions.Num(); ++I)
	{
		auto* Label = NewObject<UTextRenderComponent>(this);

		Label->SetupAttachment(RootComponent);
		Label->SetRelativeLocation(Data->ExitPositions[I]);
		Label->SetRelativeRotation(Data->ExitRotations[I]);
		Label->SetText(NSLOCTEXT("Maze.World", "Exit", "EXIT"));
		Label->SetTextRenderColor(FColor(60, 230, 120));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(65);
		Label->RegisterComponent();
	}

	UE_LOG(LogTemp,
	       Display,
	       TEXT("Maze generated: seed=%d cells=%d exits=1 rooms=%d holes=%d wall triangles=%d"),
	       Seed,
	       Layout.Walls.Num(),
	       Layout.Rooms.Num(),
	       Layout.NumHoles(),
	       Surface ? Surface->Triangles.Num() / 3 : 0);
}

void AMazeWorld::PrepareMaterials()
{
	const auto Maze = ECSSubsystem->ReadMaze(MazeEntity);
	{
		auto* Panels = LoadObject<UMaterialInterface>(
		    nullptr, TEXT("/Game/Materials/Laboratory/MI_LabCeilingMineral.MI_LabCeilingMineral"));

		if (Panels)
		{
			// Presentation mirrors of generation facts, refreshed with each immutable payload.
			auto* Material = Ceiling->CreateDynamicMaterialInstance(0, Panels);
			const uint32 PatternSeed = static_cast<uint32>(Maze.Seed);

			Material->SetScalarParameterValue(TEXT("CellSizeCm"), Maze.Cell);
			Material->SetScalarParameterValue(TEXT("WallThicknessCm"), Maze.WallThickness);
			Material->SetScalarParameterValue(TEXT("TargetPanelSizeCm"), FMazeInterior::PanelTargetCm);
			Material->SetVectorParameterValue(TEXT("MazeOrigin"),
			                                  FLinearColor(Maze.Origin.X, Maze.Origin.Y, Maze.Origin.Z));
			Material->SetVectorParameterValue(TEXT("MazeSeed"),
			                                  FLinearColor(PatternSeed & 0xffff, PatternSeed >> 16, 0.f));
		}
		else
			UE_LOG(LogTemp, Error, TEXT("Missing square ceiling material. Run Scripts/create_ceiling_material.py."));

		if (auto* Ceramic = LoadObject<UMaterialInterface>(
		        nullptr, TEXT("/Game/Materials/Laboratory/MI_MazeWallCeramic.MI_MazeWallCeramic")))
		{
			auto* Material = Walls->CreateDynamicMaterialInstance(0, Ceramic);
			const int32 Courses = FMath::Max(1, FMath::RoundToInt(Maze.WallHeight / 15.f));

			Material->SetScalarParameterValue(TEXT("TileHeightCm"), Maze.WallHeight / Courses);

			// Align complete courses downward from the actual ECS wall/ceiling junction.
			Material->SetVectorParameterValue(
			    TEXT("TileOrigin"), FLinearColor(Maze.Origin.X, Maze.Origin.Y, Maze.Origin.Z + Maze.WallHeight));
		}
		else
			UE_LOG(LogTemp, Error, TEXT("Missing aligned wall ceramic. Run Scripts/create_wall_material.py."));
	}

	auto* Ground =
	    LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Laboratory/MI_LabVinylSatin.MI_LabVinylSatin"));

	VisualMaterials = {
	    Walls->GetMaterial(0),
	    Ground,
	    LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Laboratory/MI_LabTrimMetal.MI_LabTrimMetal")),
	    LoadObject<UMaterialInterface>(nullptr,
	                                   TEXT("/Game/Materials/Laboratory/MI_LabServiceIvory.MI_LabServiceIvory")),
	    LoadObject<UMaterialInterface>(nullptr,
	                                   TEXT("/Game/Materials/Laboratory/MI_LabServiceRecess.MI_LabServiceRecess")),
	    Ground,
	    Ceiling->GetMaterial(0),
	    GetDefault<UMazeLocationSettings>()->PoolTileMaterial().Get(),
	    GetDefault<UMazeLocationSettings>()->WaterMaterial().Get()};

	if (!VisualMaterials[7] || !VisualMaterials[8])
	{
		GetWorld()->GetSubsystem<UMazeLocationSubsystem>()->Fail(TEXT("Room presentation materials are unavailable"));

		return;
	}

	HeadlampBody->SetMaterial(0, VisualMaterials[2]);
	HeadlampLens->SetMaterial(0, VisualMaterials[3]);

	if (const auto Lamps = ECSSubsystem->BuildMazeLampLocations(MazeEntity))
		MazeLampAudio::Rebuild(*this, *Lamps);

	MazeWindAudio::Start(*this);
	PrepareDoorAssets();
	bPresentationStarted = true;
}
