#include "World/MazeWorld.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "World/MazeWindAudio.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ECS/MazeECSSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AMazeWorld::AMazeWorld()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Walls = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Walls"));
	SetRootComponent(Walls);
	Floor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(Walls);
	Ceiling = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ceiling"));
	Ceiling->SetupAttachment(Walls);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlainMaterial(
	    TEXT("/Game/Materials/M_MazePlain.M_MazePlain"));

	Floor->SetStaticMesh(Cube.Object);
	Floor->SetMaterial(0, PlainMaterial.Object);
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
	Ceiling->SetStaticMesh(Cube.Object);
	Ceiling->SetMaterial(0, PlainMaterial.Object);
	Ceiling->SetCollisionProfileName(TEXT("BlockAll"));
	Ceiling->SetCanEverAffectNavigation(false);
	Walls->SetMaterial(0, PlainMaterial.Object);
	Walls->SetCollisionProfileName(TEXT("BlockAll"));
	Walls->bUseComplexAsSimpleCollision = true;
	Floor->SetCanEverAffectNavigation(false);
	Walls->SetCanEverAffectNavigation(false);
}

void AMazeWorld::BeginPlay()
{
	Super::BeginPlay();

	// Presentation only: owned components follow this world's maze lifetime.
	// Loading here also applies the material to existing CDOs after Live Coding.
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (auto* Ground = LoadObject<UMaterialInterface>(
		        nullptr, TEXT("/Game/Materials/Laboratory/MI_LabVinylSatin.MI_LabVinylSatin")))
			Floor->SetMaterial(0, Ground);
		else
			UE_LOG(
			    LogTemp, Error, TEXT("Missing laboratory floor material. Run Scripts/create_laboratory_materials.py."));

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
		// Moderate emissive panels still need sufficient GI/reflection sampling.
		Exposure->Settings.bOverride_LumenFinalGatherQuality = true;
		Exposure->Settings.LumenFinalGatherQuality = 2.f;
		Exposure->Settings.bOverride_LumenReflectionQuality = true;
		Exposure->Settings.LumenReflectionQuality = 2.f;
		Exposure->Settings.bOverride_SceneFringeIntensity = true;
		Exposure->Settings.SceneFringeIntensity = 0.f;
		Exposure->RegisterComponent();
	}

	InitializeMaze();
	MazeWindAudio::Start(*this);
}

void AMazeWorld::InitializeMaze()
{
	if (HasAuthority() || Seed != 0)
		Build();
}

void AMazeWorld::EndPlay(const EEndPlayReason::Type Reason)
{
	MazeWindAudio::Stop(*this);

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

void AMazeWorld::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMazeWorld, Seed);
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

TSharedPtr<const FMazeGeneratedData> AMazeWorld::GetGeneratedData() const
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

	if (!MazeEntity.IsSet())
		MazeEntity = ECSSubsystem->CreateMaze(Seed, GetActorLocation());
	else
		ECSSubsystem->RegenerateMaze(MazeEntity, Seed, GetActorLocation());

	const auto Maze = ECSSubsystem->ReadMaze(MazeEntity);

	Seed = Maze.Seed; // Engine replication mirrors the ECS seed.

	const auto Data = Maze.Data;

	if (!Data || Data == OldData)
		return;

	const auto& Layout = Data->Layout;

	Walls->ClearAllMeshSections();
	Floor->ClearInstances();
	Floor->AddInstances(Data->FloorTransforms, false);
	Ceiling->SetRelativeTransform(Data->CeilingTransform);

	if (GetNetMode() != NM_DedicatedServer)
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
			Material->SetScalarParameterValue(TEXT("TargetPanelSizeCm"), 120.f);
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

	const FMazeSurface& Surface = Data->Surface;

	Walls->CreateMeshSection(0,
	                         Surface.Vertices,
	                         Surface.Triangles,
	                         Surface.Normals,
	                         TArray<FVector2D>(),
	                         TArray<FColor>(),
	                         TArray<FProcMeshTangent>(),
	                         true);

	// Labels are local visual components; topology alone is replicated.
	TArray<UTextRenderComponent*> OldLabels;

	GetComponents(OldLabels);

	for (auto* Label : OldLabels)
		Label->DestroyComponent();

	auto* StartLabel = NewObject<UTextRenderComponent>(this);

	StartLabel->SetupAttachment(RootComponent);
	StartLabel->SetRelativeLocation(StartLocation() - GetActorLocation() + FVector(0, 0, -95));
	StartLabel->SetRelativeRotation(FRotator(90, 0, 0));
	StartLabel->SetText(NSLOCTEXT("Maze.World", "Start", "A / START"));
	StartLabel->SetTextRenderColor(FColor(50, 160, 255));
	StartLabel->SetHorizontalAlignment(EHTA_Center);
	StartLabel->SetWorldSize(60);
	StartLabel->RegisterComponent();

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
	       Surface.Triangles.Num() / 3);
}
