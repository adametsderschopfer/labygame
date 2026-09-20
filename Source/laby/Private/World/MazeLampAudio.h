#pragma once

class AActor;

struct FMazeInterior;

namespace MazeLampAudio
{
	void Rebuild(AActor& Owner, const FMazeInterior& Interior);
	void Stop(AActor& Owner);
}
