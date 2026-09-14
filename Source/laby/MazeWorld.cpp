#include "MazeWorld.h"
#include "MazeSurface.h"
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
	if (HasAuthority())
	{
		Seed = static_cast<int32>(FPlatformTime::Cycles64() & 0x7fffffff);
		if (Seed == 0) Seed = 1;
		Build();
	}
	else if (Seed != 0) Build();
}

void AMazeWorld::OnRep_Seed() { if (Seed != 0) Build(); }

void AMazeWorld::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMazeWorld, Seed);
}

FVector AMazeWorld::StartLocation() const
{
	return GetActorLocation() + FVector((Layout.Size / 2 + 0.5f) * Cell, (Layout.Size / 2 + 0.5f) * Cell, 100.f);
}

void AMazeWorld::Build()
{
	Layout.Generate(Seed);
	Walls->ClearAllMeshSections();
	Floor->ClearInstances();
	const float Span = Layout.Size * Cell;
	Floor->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Span / 2, Span / 2, -25), FVector((Span + 2400) / 100, (Span + 2400) / 100, 0.5f)));
	FMazeSurface Surface;
	Surface.Build(Layout, Cell, 50.f, 1000.f);
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
	for (int32 I = 0; I < 3; ++I)
	{
		int32 C = Layout.Exits[I];
		FVector Position((C % Layout.Size + 0.5f) * Cell, (C / Layout.Size + 0.5f) * Cell, 300);
		const FVector Outward[] = {FVector(0,-1,0), FVector(1,0,0), FVector(0,1,0)};
		Position += Outward[I] * (Cell / 2 + 50);
		auto* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(RootComponent);
		Label->SetRelativeLocation(Position);
		Label->SetRelativeRotation((-Outward[I]).Rotation());
		Label->SetText(FText::FromString(FString::Printf(TEXT("EXIT %d"), I + 1)));
		Label->SetTextRenderColor(FColor(60, 230, 120));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(65);
		Label->RegisterComponent();
	}
	UE_LOG(LogTemp, Display, TEXT("Maze generated: seed=%d cells=%d exits=3 wall triangles=%d"), Seed, Layout.Walls.Num(), Surface.Triangles.Num() / 3);
}

int32 AMazeWorld::ExitAt(const FVector& Location) const
{
	FVector P = Location - GetActorLocation();
	const float Span = Layout.Size * Cell;
	for (int32 I = 0; I < Layout.Exits.Num(); ++I)
	{
		int32 C = Layout.Exits[I];
		float X = (C % Layout.Size + 0.5f) * Cell, Y = (C / Layout.Size + 0.5f) * Cell;
		if ((I == 0 && P.Y < -50 && FMath::Abs(P.X - X) < (Cell - 50.f) / 2) ||
			(I == 1 && P.X > Span + 50 && FMath::Abs(P.Y - Y) < (Cell - 50.f) / 2) ||
			(I == 2 && P.Y > Span + 50 && FMath::Abs(P.X - X) < (Cell - 50.f) / 2)) return I + 1;
	}
	return 0;
}
