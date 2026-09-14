#include "World/MazeWorld.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureCube.h"
#include "ECS/MazeECSSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlainMaterial(
	    TEXT("/Game/Materials/M_MazePlain.M_MazePlain"));

	Floor->SetStaticMesh(Cube.Object);
	Floor->SetMaterial(0, PlainMaterial.Object);
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
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
		if (auto* Ground = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_MazeGround.M_MazeGround")))
			Floor->SetMaterial(0, Ground);
		else
			UE_LOG(LogTemp, Error, TEXT("Missing maze ground material. Run Scripts/create_night_environment.py."));

		auto* Moon = NewObject<UDirectionalLightComponent>(this, TEXT("MoonLight"));

		Moon->SetupAttachment(RootComponent);
		Moon->SetMobility(EComponentMobility::Movable);
		// Direction of the brightest HDRI pixel, converted from long-lat to UE axes.
		Moon->SetWorldRotation((-FVector(0.291328, -0.393440, 0.871971)).Rotation());
		Moon->SetIntensity(0.35f);
		Moon->SetLightColor(FLinearColor(0.68f, 0.78f, 1.f));
		Moon->LightSourceAngle = 0.55f;
		Moon->RegisterComponent();

		auto* NightMaterial =
		    LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_MoonNight.M_MoonNight"));
		auto* NightCubemap = LoadObject<UTextureCube>(nullptr, TEXT("/Game/Environment/T_MoonNight.T_MoonNight"));

		if (NightMaterial && NightCubemap)
		{
			auto* Dome = NewObject<UStaticMeshComponent>(this, TEXT("NightSky"));

			Dome->SetupAttachment(RootComponent);
			Dome->SetStaticMesh(Floor->GetStaticMesh());
			Dome->SetMaterial(0, NightMaterial);
			Dome->SetRelativeScale3D(FVector(20000.f));
			Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Dome->SetCastShadow(false);
			Dome->SetVisibleInRayTracing(false);
			Dome->SetCanEverAffectNavigation(false);
			Dome->bAffectDistanceFieldLighting = false;
			Dome->RegisterComponent();

			auto* Sky = NewObject<USkyLightComponent>(this, TEXT("NightSkyLight"));

			Sky->SetupAttachment(RootComponent);
			Sky->SetMobility(EComponentMobility::Movable);
			Sky->SourceType = SLS_SpecifiedCubemap;
			Sky->SetCubemap(NightCubemap);
			Sky->SetIntensity(0.08f);
			Sky->SetLightColor(FLinearColor(0.65f, 0.76f, 1.f));
			Sky->SetLowerHemisphereColor(FLinearColor::Black);
			Sky->RegisterComponent();
		}
		else
			UE_LOG(LogTemp, Error, TEXT("Missing night sky assets. Run Scripts/create_night_environment.py."));

		auto* Fog = NewObject<UExponentialHeightFogComponent>(this, TEXT("NightHaze"));

		Fog->SetupAttachment(RootComponent);
		Fog->SetFogDensity(0.008f);
		Fog->SetFogHeightFalloff(0.3f);
		Fog->SetFogInscatteringColor(FLinearColor(0.003f, 0.005f, 0.01f));
		Fog->SetFogMaxOpacity(0.35f);
		Fog->RegisterComponent();

		auto* Exposure = NewObject<UPostProcessComponent>(this, TEXT("NightExposure"));

		Exposure->SetupAttachment(RootComponent);
		Exposure->bUnbound = true;
		Exposure->Priority = 10.f;
		// Extended luminance range is enabled: equal EV100 bounds keep night dark.
		Exposure->Settings.bOverride_AutoExposureMinBrightness = true;
		Exposure->Settings.bOverride_AutoExposureMaxBrightness = true;
		Exposure->Settings.AutoExposureMinBrightness = -2.f;
		Exposure->Settings.AutoExposureMaxBrightness = -2.f;
		Exposure->Settings.bOverride_AutoExposureBias = true;
		Exposure->Settings.AutoExposureBias = 0.f;
		Exposure->RegisterComponent();
	}

	InitializeMaze();
}

void AMazeWorld::InitializeMaze()
{
	if (HasAuthority() || Seed != 0)
		Build();
}

void AMazeWorld::EndPlay(const EEndPlayReason::Type Reason)
{
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
	return ECSSubsystem ? ECSSubsystem->ReadMaze(MazeEntity).Cell : 875.f;
}

void AMazeWorld::Build()
{
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
