#pragma once

class AActor;

struct FMazeInterior;

namespace MazeFixtureMeshes
{
	void Rebuild(AActor& Owner, const FMazeInterior& Interior);
}
