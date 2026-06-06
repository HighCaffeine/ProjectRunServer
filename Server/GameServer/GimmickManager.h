#pragma once
#include <unordered_map>
#include <string>
#include <mutex>
#include <chrono>

#include "GimmickData.h"
#include "Packet\Packet.h"
#include "UserModels\user.h"
#include "Utility\unity.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

class Room;

class GimmickManager
{
private:
	std::unordered_map<int, ServerGimmickData> mGimmicks;
	std::recursive_mutex mGimmickLock;

public:
	GimmickManager() = default;
	~GimmickManager() = default;

	void ResetGimmicks(int count, int* gimmickIDs);
	void LoadMapData(const std::string& path, INT32 mapNum);
	void ProcessGimmickInteract(User* pUser, PLAYER_GIMMICK_INTERACT_REQUEST_PACKET* pReq, Room* pRoom);
	void UpdateGimmicks(float dt, Room* pRoom);

};