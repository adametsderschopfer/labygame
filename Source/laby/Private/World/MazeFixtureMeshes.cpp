#include "World/MazeFixtureMeshes.h"
#include "Maze/MazeInterior.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

void MazeFixtureMeshes::Rebuild(AActor& Owner, const FMazeInterior& Interior)
{
	const FName Tag(TEXT("MazeImportedFixture"));
	TArray<UInstancedStaticMeshComponent*> Existing;

	Owner.GetComponents(Existing);

	for (auto* Component : Existing)
		if (Component->ComponentHasTag(Tag))
			Component->DestroyComponent();

	const auto Add = [&](const TCHAR* Path, const TArray<FTransform>& Transforms)
	{
		auto* Mesh = LoadObject<UStaticMesh>(nullptr, Path);

		if (!Mesh)
		{
			UE_LOG(LogTemp, Error, TEXT("Missing fixture mesh %s; run Scripts/import_lab_fixtures.py"), Path);

			return;
		}

		auto* Component = NewObject<UInstancedStaticMeshComponent>(&Owner);
		Component->ComponentTags.Add(Tag);
		Component->SetupAttachment(Owner.GetRootComponent());
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Owner.AddInstanceComponent(Component);
		Component->RegisterComponent();
		Component->AddInstances(Transforms, false, false, false);
	};

	Add(TEXT("/Game/Fixtures/SM_LabSocket.SM_LabSocket"), Interior.SocketTransforms);
	Add(TEXT("/Game/Fixtures/SM_LabSmokeDetector.SM_LabSmokeDetector"), Interior.DetectorTransforms);
}
