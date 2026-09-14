#include "LiveCoding/MazeAutoLiveCoding.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

namespace
{
	TAutoConsoleVariable<int32> AutoCompile(
	    TEXT("laby.AutoLiveCoding"), 1, TEXT("Automatically request Live Coding after source saves. 0 disables it."));
	TAutoConsoleVariable<float> QuietSeconds(
	    TEXT("laby.AutoLiveCodingDelay"),
	    5.f,
	    TEXT("Seconds without source changes before automatic Live Coding (minimum 2)."));
}

TMap<FString, FDateTime> FMazeAutoLiveCoding::Snapshot() const
{
	TArray<FString> Paths;

	IFileManager::Get().FindFilesRecursive(Paths, *FPaths::GameSourceDir(), TEXT("*"), true, false);
	Paths.Add(FPaths::GetProjectFilePath());

	TMap<FString, FDateTime> Result;

	for (const FString& Path : Paths)
	{
		const FString Extension = FPaths::GetExtension(Path).ToLower();

		if (Extension == TEXT("cpp") || Extension == TEXT("h") || Extension == TEXT("hpp") ||
		    Extension == TEXT("inl") || Extension == TEXT("cs") || Extension == TEXT("uproject"))
			Result.Add(Path, IFileManager::Get().GetTimeStamp(*Path));
	}

	return Result;
}

void FMazeAutoLiveCoding::Start()
{
	Files = Snapshot();
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMazeAutoLiveCoding::Tick), 1.f);
}

void FMazeAutoLiveCoding::Stop()
{
	FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
}

bool FMazeAutoLiveCoding::Tick(float DeltaSeconds)
{
	TMap<FString, FDateTime> Current = Snapshot();
	bool bChanged = false;
	bool bSensitiveChange = false;

	for (const auto& Entry : Current)
	{
		const FDateTime* Previous = Files.Find(Entry.Key);

		if (!Previous || *Previous != Entry.Value)
		{
			bChanged = true;
			bSensitiveChange |= !Previous || !Entry.Key.EndsWith(TEXT(".cpp"));
		}
	}

	for (const auto& Entry : Files)
	{
		if (!Current.Contains(Entry.Key))
		{
			bChanged = true;
			bSensitiveChange = true;
		}
	}

	Files = MoveTemp(Current);

	if (bChanged)
	{
		LastChange = FPlatformTime::Seconds();
		bPending = true;

		if (bSensitiveChange)
		{
			bNeedsReview = true;
			UE_LOG(LogTemp,
			       Warning,
			       TEXT("Laby Auto Live Coding paused: headers, build rules or file set changed. Review Live Coding "
			            "compatibility; dependency changes require a full build. After review, toggle "
			            "laby.AutoLiveCoding 0 then 1 to resume."));
		}
	}

	if (AutoCompile.GetValueOnGameThread() == 0)
	{
		bPending = false;
		bNeedsReview = false;

		return true;
	}

	if (!bPending || bNeedsReview ||
	    FPlatformTime::Seconds() - LastChange < FMath::Max(2.f, QuietSeconds.GetValueOnGameThread()))
		return true;

#if WITH_LIVE_CODING
	auto* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);

	if (LiveCoding && LiveCoding->IsCompiling())
		return true;

	// One attempt per batch; a failed compile must not cause an endless retry loop.
	bPending = false;

	if (!LiveCoding || !LiveCoding->IsEnabledForSession())
	{
		UE_LOG(LogTemp,
		       Warning,
		       TEXT("Laby Auto Live Coding: enable Live Coding in Editor Preferences before saving again."));

		return true;
	}

	ELiveCodingCompileResult Result = ELiveCodingCompileResult::NotStarted;
	const bool bStarted = LiveCoding->Compile(ELiveCodingCompileFlags::None, &Result);

	UE_LOG(LogTemp,
	       Display,
	       TEXT("Laby Auto Live Coding: request accepted=%d result=%d. Compilation and patch results are in the Live "
	            "Coding log."),
	       bStarted,
	       static_cast<int32>(Result));
#else
	bPending = false;
#endif

	return true;
}
