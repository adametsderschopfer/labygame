#pragma once

class AActor;

// Local sound presentation. Owns no gameplay state or replicated rules.
namespace MazeWindAudio
{
	void Start(AActor& Owner);
	void Stop(AActor& Owner);
}
