#include "World/MazeOnlineGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Engine/NetDriver.h"

void UMazeOnlineGameInstance::Init()
{
	Super::Init();
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::BeginLoadingScreen);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::EndLoadingScreen);
	NetworkHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::OnNetworkFailure);
	TravelHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::OnTravelFailure);
}

void UMazeOnlineGameInstance::Shutdown()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	ClearLoadingScreen();

	if (Identity.IsValid())
		Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);

	if (Sessions.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}

	GEngine->OnNetworkFailure().Remove(NetworkHandle);
	GEngine->OnTravelFailure().Remove(TravelHandle);
	Super::Shutdown();
}

void UMazeOnlineGameInstance::Finish(const FText& Message)
{
	bBusy = false;
	Status = Message;
}

bool UMazeOnlineGameInstance::Prepare(bool bHost, const FString& Code)
{
	if (bBusy)
		return false;

	if (GetWorld()->GetNetMode() != NM_Standalone)
		return false;

	bLocalTransport = false;

	auto* OSS = Online::GetSubsystem(GetWorld(), FName(TEXT("EOS")));

	if (!OSS)
	{
		Finish(NSLOCTEXT("Maze.Online", "EosNotConfigured", "Online play is not configured yet."));

		return false;
	}

	Sessions = OSS->GetSessionInterface();
	Identity = OSS->GetIdentityInterface();

	if (!Sessions.IsValid() || !Identity.IsValid())
	{
		Finish(NSLOCTEXT("Maze.Online", "EosUnavailable", "Online services are unavailable."));

		return false;
	}

	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Finish(NSLOCTEXT("Maze.Online", "LeaveCurrentRoom", "Leave your current room first."));

		return false;
	}

	bBusy = true;
	bHosting = bHost;
	RoomCode = Code;

	if (!CreateHandle.IsValid())
	{
		CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		    FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreate));
		FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		    FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFind));
		JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		    FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoin));
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		    FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroy));
		LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(
		    0, FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::OnLogin));
	}

	if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
		ContinueOperation();
	else
	{
		Status = NSLOCTEXT("Maze.Online", "SigningIn", "Signing in to Epic Games...");

		if (!Identity->Login(0, FOnlineAccountCredentials(TEXT("accountportal"), TEXT(""), TEXT(""))))
			Finish(NSLOCTEXT("Maze.Online", "SignInStartFailed", "Could not start Epic Games sign-in."));
	}

	return true;
}

void UMazeOnlineGameInstance::Host()
{
	Prepare(true, FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10).ToUpper());
}

void UMazeOnlineGameInstance::Join(const FString& Code)
{
	if (bBusy)
		return;

	FString Clean = Code.TrimStartAndEnd().ToUpper();

	if (Clean.Len() != 10)
	{
		Finish(NSLOCTEXT("Maze.Online", "CodeLength", "Enter a 10-character room code."));

		return;
	}

	for (TCHAR C : Clean)
		if (!FChar::IsHexDigit(C))
		{
			Finish(NSLOCTEXT("Maze.Online", "CodeCharacters", "Room codes contain only 0-9 and A-F."));

			return;
		}

	Prepare(false, Clean);
}

void UMazeOnlineGameInstance::OnLogin(int32 User, bool bSuccess, const FUniqueNetId& Id, const FString& Error)
{
	if (!bBusy || bLeaving)
		return;

	if (bSuccess)
		ContinueOperation();
	else
		Finish(NSLOCTEXT("Maze.Online", "SignInFailed", "Sign-in was cancelled or is unavailable. Please try again."));
}

void UMazeOnlineGameInstance::ContinueOperation()
{
	if (bHosting)
	{
		Status = NSLOCTEXT("Maze.Online", "CreatingRoom", "Creating room...");

		FOnlineSessionSettings Settings;

		Settings.NumPublicConnections = 4;
		Settings.bIsLANMatch = false;
		Settings.bShouldAdvertise = true;
		Settings.bAllowJoinInProgress = true;
		Settings.bUsesPresence = true;
		Settings.bAllowJoinViaPresence = true;
		Settings.bUseLobbiesIfAvailable = true;
		Settings.Set(SETTING_HOST_MIGRATION, false, EOnlineDataAdvertisementType::DontAdvertise);
		Settings.Set(FName(TEXT("LABY_CODE")), RoomCode, EOnlineDataAdvertisementType::ViaOnlineService);

		if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
			Finish(NSLOCTEXT("Maze.Online", "CreateFailed", "Could not create the room."));
	}
	else
	{
		Status = NSLOCTEXT("Maze.Online", "FindingRoom", "Finding room...");
		Search = MakeShared<FOnlineSessionSearch>();
		Search->MaxSearchResults = 20;
		Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
		Search->QuerySettings.Set(FName(TEXT("LABY_CODE")), RoomCode, EOnlineComparisonOp::Equals);

		if (!Sessions->FindSessions(0, Search.ToSharedRef()))
			Finish(NSLOCTEXT("Maze.Online", "SearchStartFailed", "Could not start the room search."));
	}
}

void UMazeOnlineGameInstance::OnCreate(FName Name, bool bSuccess)
{
	if (!bSuccess)
	{
		Finish(NSLOCTEXT("Maze.Online", "CreateError", "Room creation failed."));

		return;
	}

	Finish(FText::GetEmpty());
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Maze"), true, TEXT("listen?Room=1"));
}

void UMazeOnlineGameInstance::OnFind(bool bSuccess)
{
	if (!bSuccess || !Search.IsValid())
	{
		Finish(NSLOCTEXT("Maze.Online", "SearchError", "Room search failed."));

		return;
	}

	for (const auto& Result : Search->SearchResults)
	{
		FString Code;

		Result.Session.SessionSettings.Get(FName(TEXT("LABY_CODE")), Code);

		if (Code == RoomCode)
		{
			Status = NSLOCTEXT("Maze.Online", "Connecting", "Connecting...");

			if (!Sessions->JoinSession(0, NAME_GameSession, Result))
				Finish(NSLOCTEXT("Maze.Online", "ConnectFailed", "Could not connect."));

			return;
		}
	}

	Finish(NSLOCTEXT("Maze.Online", "RoomNotFound", "The room was not found, is full, or has already started."));
}

void UMazeOnlineGameInstance::OnJoin(FName Name, EOnJoinSessionCompleteResult::Type Result)
{
	FString Address;

	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions->GetResolvedConnectString(Name, Address))
	{
		Status = NSLOCTEXT("Maze.Online", "JoinFailed", "Could not join: the room is unavailable or full.");
		Leave();

		return;
	}

	Finish(FText::GetEmpty());

	if (auto* PC = GetFirstLocalPlayerController())
		PC->ClientTravel(Address, TRAVEL_Absolute);
}

void UMazeOnlineGameInstance::CloseAdmission()
{
	if (bLocalTransport)
		return;

	if (!Sessions.IsValid())
		return;

	if (auto* Session = Sessions->GetNamedSession(NAME_GameSession))
	{
		auto Settings = Session->SessionSettings;

		Settings.bShouldAdvertise = false;
		Settings.bAllowJoinInProgress = false;
		Settings.bAllowJoinViaPresence = false;
		Sessions->UpdateSession(NAME_GameSession, Settings, true);
	}
}

void UMazeOnlineGameInstance::Leave()
{
	if (bLeaving)
		return;

	bLeaving = true;
	bBusy = true;

	if (bLocalTransport)
	{
		OnDestroy(NAME_GameSession, true);

		return;
	}

	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession) ||
	    !Sessions->DestroySession(NAME_GameSession))
		OnDestroy(NAME_GameSession, true);
}

void UMazeOnlineGameInstance::OnDestroy(FName Name, bool bSuccess)
{
	bLeaving = false;
	bBusy = false;
	RoomCode.Empty();
	bLocalTransport = false;
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Maze"), true);
}

void UMazeOnlineGameInstance::OnNetworkFailure(UWorld* World,
                                               UNetDriver* Driver,
                                               ENetworkFailure::Type Type,
                                               const FString& Error)
{
	if (World != GetWorld())
		return;

	Status = NSLOCTEXT("Maze.Online", "ConnectionLost", "Connection lost or the host closed the room.");
	ClearLoadingScreen();
	Leave();
}

void UMazeOnlineGameInstance::OnTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
	if (World != GetWorld())
		return;

	Status = NSLOCTEXT("Maze.Online", "TravelFailed", "Could not load the multiplayer map.");
	ClearLoadingScreen();
	Leave();
}

void UMazeOnlineGameInstance::HostLocal()
{
	if (bBusy || GetWorld()->GetNetMode() != NM_Standalone)
		return;

	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession))
	{
		Finish(NSLOCTEXT("Maze.Online", "LeaveCurrentRoom", "Leave your current room first."));

		return;
	}

	bLocalTransport = true;
	bBusy = true;
	RoomCode.Empty();
	Status = NSLOCTEXT("Maze.Local", "Starting", "Starting local server...");
	// UE's EOS driver explicitly passes through to IP sockets for this URL option.
	UGameplayStatics::OpenLevel(
	    this, TEXT("/Game/Maps/Maze"), true, TEXT("listen?Room=1?Local=1?bUseIPSockets?Port=7777"));
}

void UMazeOnlineGameInstance::JoinLocal(const FString& Address)
{
	if (bBusy || GetWorld()->GetNetMode() != NM_Standalone)
		return;

	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession))
	{
		Finish(NSLOCTEXT("Maze.Online", "LeaveCurrentRoom", "Leave your current room first."));

		return;
	}

	FString Host = Address.TrimStartAndEnd();
	FString PortString;
	int32 Port = 7777;

	if (Host.Contains(TEXT(":")))
	{
		FString HostOnly;

		Host.Split(TEXT(":"), &HostOnly, &PortString);
		Host = HostOnly;

		if (PortString.IsEmpty() || PortString.Len() > 5)
			Host.Empty();

		for (TCHAR C : PortString)
			if (C < '0' || C > '9')
				Host.Empty();

		Port = FCString::Atoi(*PortString);
	}

	if (Host.Equals(TEXT("localhost"), ESearchCase::IgnoreCase))
		Host = TEXT("127.0.0.1");

	TArray<FString> Octets;

	Host.ParseIntoArray(Octets, TEXT("."), false);

	bool bValid = Octets.Num() == 4 && Port > 0 && Port <= 65535;

	for (const FString& Octet : Octets)
	{
		bValid &= !Octet.IsEmpty() && Octet.Len() <= 3;

		for (TCHAR C : Octet)
			bValid &= C >= '0' && C <= '9';

		bValid &= FCString::Atoi(*Octet) <= 255;
	}

	if (!bValid)
	{
		Finish(NSLOCTEXT("Maze.Local", "InvalidAddress", "Enter an IPv4 address, such as 127.0.0.1:7777."));

		return;
	}

	auto* PC = GetFirstLocalPlayerController();

	if (!PC)
		return;

	bLocalTransport = true;
	bBusy = true;
	Status = NSLOCTEXT("Maze.Local", "Connecting", "Connecting to local server...");
	PC->ClientTravel(FString::Printf(TEXT("%s:%d?Local=1?bUseIPSockets"), *Host, Port), TRAVEL_Absolute);
}

void UMazeOnlineGameInstance::LocalRoomReady()
{
	if (bLocalTransport && GetWorld()->GetNetMode() != NM_Standalone)
		Finish(FText::GetEmpty());
}

FString UMazeOnlineGameInstance::LocalAddress() const
{
	FString Host = TEXT("127.0.0.1");
	int32 Port = GetWorld()->URL.Port;

	if (auto* Driver = GetWorld()->GetNetDriver())
	{
		const TSharedPtr<const FInternetAddr> Address = Driver->GetLocalAddr();

		if (Address.IsValid())
			Port = Address->GetPort();
	}

	if (auto* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		bool bCanBindAll = false;
		const auto Address = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBindAll);

		if (Address->IsValid() && !Address->ToString(false).Contains(TEXT(":")))
			Host = Address->ToString(false);
	}

	return FString::Printf(TEXT("%s:%d"), *Host, Port);
}
