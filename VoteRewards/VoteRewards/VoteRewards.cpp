#include "VoteRewards.h"
#include "VoteRewardsConfig.h"

void RewardPoints(const int64 SteamID, const int Points)
{
	if (HasShop)
	{
#ifdef ATLAS
		AtlasShop::Points::AddPoints(Points, SteamID);
#else
		ArkShop::Points::AddPoints(Points, SteamID);
#endif
	}
}

void RewardPlayer(const std::string& VoteSite, const int64 SteamID, AShooterPlayerController* player)
{
	try
	{
		const auto& Vote = VoteConfig.value(VoteSite, nlohmann::json::array());
		if (Vote.empty()) return;
		int Points = Vote.value("MinPointsReward", 0) == Vote.value("MaxPointsReward", 0) ? Vote.value("MinPointsReward", 0) :
			RandomNumber<int>(Vote.value("MinPointsReward", 0), Vote.value("MaxPointsReward", 0));
		if (!Vote.value("ChanceofPointsOrItems", false) || RandomNumber(0, 1) == 0)
		{
			if (!Vote.value("ChanceofPointsOrItems", false) && Points != 0) RewardPoints(SteamID, Points);
			int Quantity;
			float Quality;
			bool IsBP;
			std::string sBlueprint;
			FString fBlueprint;
#ifndef ATLAS
			TArray<UPrimalItem*> Out;
#endif
			const auto& Items = Vote.value("Items", nlohmann::json::array());
			if (Items.empty()) return;
			for (const auto& Item : Items)
			{
				if (Item.value("RewardChance", 0) <= 0 || RandomNumber(0, Item.value("RewardChance", 0)) == 0)
				{
					Quantity = Item.value("MaxQuantity", 0) == Item.value("MinQuantity", 0) ? Item.value("MinQuantity", 0) : RandomNumber<int>(Item.value("MinQuantity", 0), Item.value("MaxQuantity", 0));

					if (Quantity > 0)
					{
						sBlueprint = Item.value("Blueprint", "");
						fBlueprint = sBlueprint.c_str();

						Quality = Item.value("MaxQuality", 0.f) == Item.value("MinQuality", 0.f) ? Item.value("MinQuality", 0.f) : RandomNumber<float>(Item.value("MinQuality", 0.f), Item.value("MaxQuality", 0.f));

						IsBP = Item.value("MaxIsBP", 0) == 0 ? false : (Item.value("MinIsBP", 0) == Item.value("MaxIsBP", 0) ? (Item.value("MinIsBP", 0) == 1) : RandomNumber<int>(Item.value("MinIsBP", 0), Item.value("MaxIsBP", 0)) == Item.value("MinIsBP", 0));
#ifdef ATLAS
						player->GiveItem(&fBlueprint, Quantity, Quality, IsBP);
#else
						player->GiveItem(&Out, &fBlueprint, Quantity, Quality, IsBP, false, 0);
#endif
					}
				}
			}


			int Level;
			const auto& Dinos = Vote.value("Dinos", nlohmann::json::array());
			for (const auto& Dino : Dinos)
			{
				if (Dino["RewardChance"] <= 0 || RandomNumber<int>(0, Dino["RewardChance"]) == 0)
				{
					sBlueprint = Dino["Blueprint"];
					fBlueprint = sBlueprint.c_str();
					Level = Dino["MaxLevel"] == Dino["MinLevel"] ? Dino["MinLevel"] : RandomNumber<int>(Dino["MinLevel"], Dino["MaxLevel"]);

					ArkApi::GetApiUtils().SpawnDino(player, fBlueprint, nullptr, Level, Dino["Tamed"], Dino["Neutered"]);
				}
			}
		}
		else if (Points != 0) RewardPoints(SteamID, Points);
		ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), *GetMsg(VoteSite, 1));
	}
	catch (std::exception exc)
	{
		Log::GetLog()->error(exc.what());
	}
}

void HttpVoteCallBack(bool Success, std::string Response, const int VoteSiteIndex, const VoteSiteConfig& voteSiteConfig, AShooterPlayerController* player, const int64 SteamID, const bool IsLast)
{
	if (!player) return;

	if (!Success)
	{
		ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), *GetMsg(voteSiteConfig.Site, 0));
		return;
	}

	if (voteSiteConfig.ExactMatch && Response == voteSiteConfig.Response || !voteSiteConfig.ExactMatch && Response.find(voteSiteConfig.Response) != std::string::npos)
	{
		PlayerData[player->LinkedPlayerIDField()].HasRewarded = true;
		const long double nNow = ArkApi::GetApiUtils().GetWorld()->GetTimeSeconds();
		PlayerData[player->LinkedPlayerIDField()].NextVoteTime[VoteSiteIndex] = nNow + ((static_cast<long double>(VoteConfig[voteSiteConfig.Site]["VoteDelayHours"]) * 3600) + 120);
		RewardPlayer(voteSiteConfig.Site, SteamID, player);
	}
	else if (!PlayerData[player->LinkedPlayerIDField()].HasRewarded && IsLast)
	{
		ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), *GetMsg("VoteRewards", 1));
	}
}

#ifdef ATLAS
void AtlasSubmitVoteRequest(const int VoteSiteIndex, const VoteSiteConfig& voteSiteConfig, const std::string& VoteKey, const int64 SteamID, AShooterPlayerController* player, const bool IsLast)
{
	switch (VoteSiteIndex)
	{
	case 0:	
		API::Requests::Get().CreateGetRequest(fmt::format(
			"https://atlasserverslist.com/api-{}_{}.json",
			VoteKey.c_str(), SteamID), std::bind(&HttpVoteCallBack,
				std::placeholders::_1, std::placeholders::_2, VoteSiteIndex, voteSiteConfig, player, SteamID, IsLast));
		break;
	case 1:
		API::Requests::Get().CreateGetRequest(fmt::format(
			"http://www.api.trackyserver.com/vote/?action=claim&key={}&steamid={}",
			VoteKey.c_str(), SteamID), std::bind(&HttpVoteCallBack,
				std::placeholders::_1, std::placeholders::_2, VoteSiteIndex, voteSiteConfig, player, SteamID, IsLast));
		break;
	case 2:
		API::Requests::Get().CreateGetRequest(fmt::format(
			"https://atlasserverlist.com/rewards?claim&serverkey={}&steamid={}",
			VoteKey.c_str(), SteamID), std::bind(&HttpVoteCallBack,
				std::placeholders::_1, std::placeholders::_2, VoteSiteIndex, voteSiteConfig, player, SteamID, IsLast));
		break;
	}
}
#else
void ArkSubmitVoteRequest(const int VoteSiteIndex, const VoteSiteConfig& voteSiteConfig, const std::string& VoteKey, const int64 SteamID, AShooterPlayerController* player, const bool IsLast)
{
	switch (VoteSiteIndex)
	{
	case 0:
		API::Requests::Get().CreateGetRequest(fmt::format(
			"https://ark-servers.net/api/?action=post&object=votes&element=claim&key={}&steamid={}",
			VoteKey.c_str(), SteamID), std::bind(&HttpVoteCallBack,
				std::placeholders::_1, std::placeholders::_2, VoteSiteIndex, voteSiteConfig, player, SteamID, IsLast));
		break;
	case 1:
		API::Requests::Get().CreateGetRequest(fmt::format(
			"http://www.api.trackyserver.com/vote/?action=claim&key={}&steamid={}",
			VoteKey.c_str(), SteamID), std::bind(&HttpVoteCallBack,
				std::placeholders::_1, std::placeholders::_2, VoteSiteIndex, voteSiteConfig, player, SteamID, IsLast));
		break;
	case 2:
		API::Requests::Get().CreateGetRequest(fmt::format(
			"https://toparkservers.com/rewards?claim&serverkey={}&steamid={}",
			VoteKey.c_str(), SteamID), std::bind(&HttpVoteCallBack,
				std::placeholders::_1, std::placeholders::_2, VoteSiteIndex, voteSiteConfig, player, SteamID, IsLast));
		break;
	}
}
#endif

void CheckVoteReward(AShooterPlayerController* player)
{
	const long double nNow = ArkApi::GetApiUtils().GetWorld()->GetTimeSeconds();
	const auto playerData = PlayerData.emplace(player->LinkedPlayerIDField(), PlayerDataS{ {0, }, false });
	const int64 SteamID = ArkApi::GetApiUtils().GetSteamIDForPlayerID(static_cast<int>(player->LinkedPlayerIDField()));
	PlayerData[player->LinkedPlayerIDField()].HasRewarded = false;
	bool VotesChecked = false;
	long double LowestNextVoteTime = -1;
	int VoteSiteCount = 0;
	for (const auto& VoteSiteConfig : VoteSites)
	{
		const int VoteSiteIndex = GetVoteSiteIndex(VoteSiteConfig.Site);
		if (VoteSiteIndex != -1)
		{
			const long double NextVoteTime = playerData.first->second.NextVoteTime[VoteSiteIndex];
			if (nNow > NextVoteTime)
			{
				const bool IsLast = VoteSiteCount == TotalVoteSitesInConfig;
				PlayerData[player->LinkedPlayerIDField()].NextVoteTime[VoteSiteIndex] = nNow + 30;
				auto VoteCnf = VoteConfig.value(VoteSiteConfig.Site, nlohmann::json::array());
				if (!VoteCnf.empty())
				{
					VoteSiteCount++;
					auto VoteKeys = VoteCnf.value("VoteKeys", nlohmann::json::array());
					if (!VoteKeys.empty())
					for (const auto& VoteKey : VoteKeys)
					{
#ifdef ATLAS
						AtlasSubmitVoteRequest(VoteSiteIndex, VoteSiteConfig, VoteKey, SteamID, player, IsLast);
#else
						ArkSubmitVoteRequest(VoteSiteIndex, VoteSiteConfig, VoteKey, SteamID, player, IsLast);
#endif
						VotesChecked = true;
					}
				}
			}
			else if (LowestNextVoteTime == -1 || LowestNextVoteTime > NextVoteTime) LowestNextVoteTime = NextVoteTime;
		}
		else if (player->bIsAdmin()()) ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), L"Vote Site does not exist: {}", VoteSiteConfig.Site);
	}

	if (VotesChecked)
	{
		ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), *GetMsg("VoteRewards", 0));
	}
	else
	{
		const long double SecondsRemaining = LowestNextVoteTime - nNow,
			hours = (SecondsRemaining > 3600 ? (SecondsRemaining / 3600) : 0),
			mins = (hours > 0 && (SecondsRemaining - (hours * 3600)) > 60) ?
			((SecondsRemaining - (hours * 3600)) / 60) : (SecondsRemaining > 60 ? (SecondsRemaining / 60) : 0);
		if (mins != 0 || hours != 0) ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), *GetMsg("VoteRewards", 4), static_cast<int>(hours), static_cast<int>(mins));
		else ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), *GetMsg("VoteRewards", 5), static_cast<int>(SecondsRemaining));
	}
}

void CheckVoteCMD(AShooterPlayerController* player, FString* message, int mode)
{
	if (player->PlayerStateField() && player->GetPlayerCharacter())
		CheckVoteReward(player);
}

void VoteSitesCMD(AShooterPlayerController* player, FString* message, int mode)
{
	if (player->PlayerStateField() && player->GetPlayerCharacter())
	{
		ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), *GetMsg("VoteRewards", 2));
		for (const auto& VoteSiteConfig : VoteSites)
		{
			auto VoteCnf = VoteConfig.value(VoteSiteConfig.Site, nlohmann::json::array());
			if (!VoteCnf.empty())
			{
				auto VoteUrls = VoteCnf.value("VoteUrls", nlohmann::json::array());
				if (!VoteUrls.empty())
				for (const auto& VoteUrl : VoteUrls)
					ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), L"{}", *FString(ArkApi::Tools::Utf8Decode(VoteUrl).c_str()));
			}
		}
		ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), *GetMsg("VoteRewards", 3));
	}
}

void ReloadConfigCMD(AShooterPlayerController* player, FString* message, int mode)
{
	if (player->PlayerStateField() && player->bIsAdmin()())
	{
		LoadConfig();
		ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), L"Config Reloaded!");
	}
}

void TestRewardCMD(AShooterPlayerController* player, FString* message, int mode)
{
	if (player->PlayerStateField() && player->GetPlayerCharacter() && player->bIsAdmin()())
	{
		const int64 SteamID = ArkApi::GetApiUtils().GetSteamIDForPlayerID(static_cast<int>(player->LinkedPlayerIDField()));
		for (const auto& VoteSiteConfig : VoteSites)
		{
			const int VoteSiteIndex = GetVoteSiteIndex(VoteSiteConfig.Site);
			if (VoteSiteIndex != -1)
			{
				if(!VoteConfig.value(VoteSiteConfig.Site, nlohmann::json::array()).empty())
					RewardPlayer(VoteSiteConfig.Site, SteamID, player);
			}
			else ArkApi::GetApiUtils().SendChatMessage(player, *GetConfig("ServerName"), L"Vote Site does not exist: {}", VoteSiteConfig.Site);
		}
	}
}

int Counter = 0;
void PlayerVoteDataCleanUp()
{
	if (Counter++ > 86400)
	{
		Counter = 0;
		const long double nNow = ArkApi::GetApiUtils().GetWorld()->GetTimeSeconds();
		bool CanRemove = true;
		for(auto& itr = PlayerData.begin(); itr != PlayerData.end();)
		{
			for (int i = 0; i < TotalVoteSites; i++) if (itr->second.NextVoteTime[i] > nNow) CanRemove = false;
			if (CanRemove)
			{
				PlayerData.erase(itr++);
				continue;
			}
			CanRemove = true;
			++itr;
		}
	}
}

void Load()
{
	Log::Get().Init("VoteRewards");
	LoadConfig();
	ArkApi::GetCommands().AddChatCommand(GetConfig("CheckCommand"), &CheckVoteCMD);
	ArkApi::GetCommands().AddChatCommand(GetConfig("VoteSitesCommand"), &VoteSitesCMD);
	ArkApi::GetCommands().AddChatCommand("/vrreload", &ReloadConfigCMD);
	ArkApi::GetCommands().AddChatCommand("/vrtest", &TestRewardCMD);
	ArkApi::GetCommands().AddOnTimerCallback("ClearOldVotes", &PlayerVoteDataCleanUp);
}

void Unload()
{
	PlayerData.clear();
	ArkApi::GetCommands().RemoveChatCommand(GetConfig("CheckCommand"));
	ArkApi::GetCommands().RemoveChatCommand(GetConfig("VoteSitesCommand"));
	ArkApi::GetCommands().RemoveChatCommand("/vrreload");
	ArkApi::GetCommands().RemoveChatCommand("/vrtest");
	ArkApi::GetCommands().RemoveOnTimerCallback("ClearOldVotes");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Load();
		break;
	case DLL_PROCESS_DETACH:
		Unload();
		break;
	}
	return TRUE;
}