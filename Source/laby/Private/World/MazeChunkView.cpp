#include "World/MazeChunkView.h"
#include "Maze/MazeChunk.h"
#include "World/MazeFixtureMeshes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "LocalVertexFactory.h"
#include "PSOPrecache.h"

FPrimitiveSceneProxy* UMazeVisualMeshComponent::CreateSceneProxy()
{
	if (CheckPSOPrecachingAndBoostPriority() &&
	    GetPSOPrecacheProxyCreationStrategy() == EPSOPrecacheProxyCreationStrategy::DelayUntilPSOPrecached)
		return nullptr;

	return Super::CreateSceneProxy();
}

void UMazeVisualMeshComponent::CollectPSOPrecacheData(const FPSOPrecacheParams& Params,
                                                      FMaterialInterfacePSOPrecacheParamsList& OutParams)
{
	for (int32 Index = 0; Index < GetNumMaterials(); ++Index)
		if (auto* Material = GetMaterial(Index))
		{
			auto& Entry = OutParams.AddDefaulted_GetRef();
			Entry.MaterialInterface = Material;
			Entry.Priority = EPSOPrecachePriority::High;
			Entry.PSOPrecacheParams = Params;
			Entry.VertexFactoryDataList.Add(FPSOPrecacheVertexFactoryData(&FLocalVertexFactory::StaticType));
		}
}

AMazeChunkView::AMazeChunkView()
{
	Mesh = CreateDefaultSubobject<UMazeVisualMeshComponent>(TEXT("VisualMesh"));
	SetRootComponent(Mesh);
	Floor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("VisualFloor"));
	Floor->SetupAttachment(Mesh);
	Ceiling = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualCeiling"));
	Ceiling->SetupAttachment(Mesh);

	for (UPrimitiveComponent* Component : {static_cast<UPrimitiveComponent*>(Mesh),
	                                       static_cast<UPrimitiveComponent*>(Floor),
	                                       static_cast<UPrimitiveComponent*>(Ceiling)})
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
	}
}

void AMazeChunkView::Apply(const FMazeChunkData& Data, UStaticMesh* Cube, const TArray<UMaterialInterface*>& Materials)
{
	check(Materials.Num() == 9);
	GeometryBytes = Data.GetGeometryBytes();

	for (int32 Index = 0; Index < 5; ++Index)
	{
		const auto& Surface = Index == 0 ? Data.Walls : Data.Interior.Sections[Index - 1];

		Mesh->SetMaterial(Index, Materials[Index]);
		Mesh->CreateMeshSection(Index,
		                        Surface.Vertices,
		                        Surface.Triangles,
		                        Surface.Normals,
		                        TArray<FVector2D>(),
		                        TArray<FColor>(),
		                        TArray<FProcMeshTangent>(),
		                        false);
	}

	Mesh->PrecachePSOs();
	Floor->SetStaticMesh(Cube);
	Floor->SetMaterial(0, Materials[5]);
	Floor->AddInstances(Data.Floors, false, false, false);
	Floor->PrecachePSOs();
	Ceiling->SetStaticMesh(nullptr);

	const auto AddInstances =
	    [this, Cube, &Materials](FName Name, const TArray<FTransform>& Instances, int32 MaterialIndex)
	{
		if (Instances.IsEmpty())
			return;

		auto* Component = NewObject<UInstancedStaticMeshComponent>(this, Name);

		Component->SetupAttachment(Mesh);
		Component->SetStaticMesh(Cube);
		Component->SetMaterial(0, Materials[MaterialIndex]);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		AddInstanceComponent(Component);
		Component->RegisterComponent();
		Component->AddInstances(Instances, false, false, false);
		Component->PrecachePSOs();
	};

	AddInstances(TEXT("CeilingInstances"), Data.Ceilings, 6);

	const int32 MaterialIndices[] = {5, 0, 7, 8, 2};
	const TCHAR* Names[] = {
	    TEXT("ShallowFloor"), TEXT("RoomCeramic"), TEXT("PoolTile"), TEXT("Water"), TEXT("BridgeMetal")};

	for (int32 Surface = 0; Surface < static_cast<int32>(EMazeRoomSurface::Count); ++Surface)
		AddInstances(FName(Names[Surface]), Data.RoomSurfaces[Surface], MaterialIndices[Surface]);

	MazeFixtureMeshes::Rebuild(*this, Data.Interior);
}
