#include "MazeWorld.h"
#include "MazeECSSubsystem.h"
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
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlainMaterial(TEXT("/Game/Materials/M_MazePlain.M_MazePlain"));
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
	InitializeMaze();
}

void AMazeWorld::InitializeMaze()
{
	if (HasAuthority() || Seed != 0) Build();
}

void AMazeWorld::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ECSSubsystem) ECSSubsystem->DestroyMaze(MazeEntity);
	MazeEntity = FMassEntityHandle();
	ECSSubsystem = nullptr;
	Super::EndPlay(Reason);
}

void AMazeWorld::OnRep_Seed() { InitializeMaze(); }

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
		if (Maze.Data) return Maze.Origin + Maze.Data->Start;
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
	if (!ECSSubsystem) ECSSubsystem = GetWorld()->GetSubsystem<UMazeECSSubsystem>();
	check(ECSSubsystem);
	const auto OldData = GetGeneratedData();
	if (!MazeEntity.IsSet()) MazeEntity = ECSSubsystem->CreateMaze(Seed, GetActorLocation());
	else ECSSubsystem->RegenerateMaze(MazeEntity, Seed, GetActorLocation());
	const auto Maze = ECSSubsystem->ReadMaze(MazeEntity);
	Seed = Maze.Seed; // Engine replication mirrors the ECS seed.
	const auto Data = Maze.Data;
	if (!Data || Data == OldData) return;
	const auto& Layout = Data->Layout;
	Walls->ClearAllMeshSections();
	Floor->ClearInstances();
	Floor->AddInstance(Data->FloorTransform);
	const FMazeSurface& Surface = Data->Surface;
	Walls->CreateMeshSection(0, Surface.Vertices, Surface.Triangles, Surface.Normals,
		TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
	// Labels are local visual components; topology alone is replicated.
	TArray<UTextRenderComponent*> OldLabels;
	GetComponents(OldLabels);
	for (auto* Label : OldLabels) Label->DestroyComponent();
	auto* StartLabel = NewObject<UTextRenderComponent>(this);
	StartLabel->SetupAttachment(RootComponent);
	StartLabel->SetRelativeLocation(StartLocation() - GetActorLocation() + FVector(0, 0, -95));
	StartLabel->SetRelativeRotation(FRotator(90, 0, 0));
	StartLabel->SetText(FText::FromString(TEXT("A / START")));
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
		Label->SetText(FText::FromString(FString::Printf(TEXT("EXIT %d"), I + 1)));
		Label->SetTextRenderColor(FColor(60, 230, 120));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(65);
		Label->RegisterComponent();
	}
	UE_LOG(LogTemp, Display, TEXT("Maze generated: seed=%d cells=%d exits=3 wall triangles=%d"), Seed, Layout.Walls.Num(), Surface.Triangles.Num() / 3);
}
