#pragma once

#include "CoreMinimal.h"

struct FMazeSurface;

namespace MazeMeshPrimitives
{
	void Quad(FMazeSurface& Mesh, FVector A, FVector B, FVector C, FVector D, FVector Normal);
	void Box(FMazeSurface& Mesh, FVector Position, FVector U, FVector V, FVector Normal, FVector Size);
}
