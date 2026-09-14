#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"

// Editor-only file observation; never runs Play or an ordinary DLL build.
class FMazeAutoLiveCoding
{
public:
	void Start();
	void Stop();

private:
	bool Tick(float DeltaSeconds);
	TMap<FString, FDateTime> Snapshot() const;

	TMap<FString, FDateTime> Files;
	FTSTicker::FDelegateHandle Ticker;
	double LastChange = 0;
	bool bPending = false;
	bool bNeedsReview = false;
};
