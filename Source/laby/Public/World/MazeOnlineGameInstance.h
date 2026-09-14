#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSessionSettings.h"
#include "MazeOnlineGameInstance.generated.h"

// Persistent transport adapter. Authoritative room rules live in the world ECS.
UCLASS()
class LABY_API UMazeOnlineGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	virtual void Init() override;
	virtual void Shutdown() override;
	void Host();
	void Join(const FString& Code);
	void Leave();
	void CloseAdmission();

	FString RoomCode;
	FText Status;
	bool bBusy = false;

private:
	bool Prepare(bool bHost, const FString& Code);
	void ContinueOperation();
	void Finish(const FText& Message);
	void OnLogin(int32 User, bool bSuccess, const FUniqueNetId& Id, const FString& Error);
	void OnCreate(FName Name, bool bSuccess);
	void OnFind(bool bSuccess);
	void OnJoin(FName Name, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroy(FName Name, bool bSuccess);
	void OnNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);

	IOnlineSessionPtr Sessions;
	IOnlineIdentityPtr Identity;
	TSharedPtr<FOnlineSessionSearch> Search;
	FDelegateHandle LoginHandle, CreateHandle, FindHandle, JoinHandle, DestroyHandle;
	FDelegateHandle NetworkHandle, TravelHandle;
	bool bHosting = false;
	bool bLeaving = false;
};
