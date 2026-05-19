#include "GimmickManager.h"
#include "Room.h"

void GimmickManager::LoadMapData(const std::string& path, INT32 mapNum)
{
	std::lock_guard<std::recursive_mutex> guard(mGimmickLock);

	FILE* fp; 
	fopen_s(&fp, path.c_str(), "rb");
	if (fp == nullptr)
	{
		printf("[Error] 맵 JSON 파일을 찾을 수 없습니다: %s\n", path.c_str());
		return;
	}

	// RapidJSON
	char readBuffer[65536];
	rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
	rapidjson::Document doc;
	doc.ParseStream(is);
	fclose(fp);

	if (doc.HasParseError() || !doc.IsObject())
	{
		printf("[Error] JSON 파싱 실패.\n");
		return;
	}

	// gimmicks 파싱
	if (doc.HasMember("gimmicks") && doc["gimmicks"].IsArray())
	{
		const rapidjson::Value& gimmicksArray = doc["gimmicks"];

		for (rapidjson::SizeType i = 0; i < gimmicksArray.Size(); i++)
		{
			const rapidjson::Value& g = gimmicksArray[i];
			ServerGimmickData data;

			// 초기값 세팅
			data.currentState = 0;
			data.startPos = { 0,0,0 };
			data.endPos = { 0,0,0 };

			// ID 파싱
			if (g.HasMember("gimmick_id") && g["gimmick_id"].IsInt())
			{
				data.gimmickID = g["gimmick_id"].GetInt();
			}

			// Type 파싱 (String -> Enum)
			if (g.HasMember("type") && g["type"].IsString()) {
				data.type = ConvertGimmickTypeToEnum(g["type"].GetString());
			}

			// Position 파싱 (_Vector3 객체)
			if (g.HasMember("position") && g["position"].IsObject())
			{
				const auto& pos = g["position"];
				data.position.x = pos.HasMember("x") ? pos["x"].GetFloat() : 0.0f;
				data.position.y = pos.HasMember("y") ? pos["y"].GetFloat() : 0.0f;
				data.position.z = pos.HasMember("z") ? pos["z"].GetFloat() : 0.0f;
			}

			// StartPos 파싱 (있는 경우에만)
			if (g.HasMember("start_pos") && g["start_pos"].IsObject())
			{
				const auto& spos = g["start_pos"];
				data.startPos.x = spos.HasMember("x") ? spos["x"].GetFloat() : 0.0f;
				data.startPos.y = spos.HasMember("y") ? spos["y"].GetFloat() : 0.0f;
				data.startPos.z = spos.HasMember("z") ? spos["z"].GetFloat() : 0.0f;
			}

			// EndPos 파싱 (있는 경우에만)
			if (g.HasMember("end_pos") && g["end_pos"].IsObject())
			{
				const auto& epos = g["end_pos"];
				data.endPos.x = epos.HasMember("x") ? epos["x"].GetFloat() : 0.0f;
				data.endPos.y = epos.HasMember("y") ? epos["y"].GetFloat() : 0.0f;
				data.endPos.z = epos.HasMember("z") ? epos["z"].GetFloat() : 0.0f;
			}

			// Properties 파싱
			if (g.HasMember("properties") && g["properties"].IsObject())
			{
				for (auto it = g["properties"].MemberBegin(); it != g["properties"].MemberEnd(); ++it)
				{
					if (it->value.IsNumber())
					{
						data.properties[it->name.GetString()] = it->value.GetFloat();
					}
				}

				if (data.properties.find("HP") != data.properties.end())
				{
					data.hp = (int)data.properties["HP"];
				}

				if (data.properties.find("Weight") != data.properties.end())
				{
					data.weight = (int)data.properties["Weight"];
				}

				if (data.properties.find("ActivationType") != data.properties.end())
				{
					data.activationType = (int)data.properties["ActivationType"];
				}
				if (data.properties.find("WaitTime") != data.properties.end())
				{
					data.waitTime = data.properties["WaitTime"];
				}

				if (data.properties.find("SpawnGimmickKey") != data.properties.end())
				{
					data.spawnGimmickKey = data.properties["SpawnGimmickKey"];
				}
			}

			// 맵에 최종 등록
			mGimmicks[data.gimmickID] = data;
		}
	}

	printf("[Room %d] 맵 데이터 로딩 완료! 총 기믹 수: %zu개\n", mapNum, mGimmicks.size());
}

void GimmickManager::ProcessGimmickInteract(User* pUser, PLAYER_GIMMICK_INTERACT_REQUEST_PACKET* pReq, Room* pRoom)
{
	PLAYER_GIMMICK_INTERACT_NTF_PACKET ntfPkt;
	bool shouldBroadcastNow = true;

	{
		std::lock_guard<std::recursive_mutex> guard(mGimmickLock);

		auto it = mGimmicks.find(pReq->gimmickID);
		if (it == mGimmicks.end())
		{
			return;
		}

		ServerGimmickData& gimmick = it->second;
		if (gimmick.hp <= 0) return;

		it->second.currentState = pReq->state;

		if (it->second.type == (int)eGimmickKey::FallingPlatform && pReq->state == 1)
		{
			if (it->second.gimmickRecoverTime <= 0.0f)
			{
				it->second.gimmickRecoverTime = 7.0f;
			}
		}

		if (gimmick.type == (int)eGimmickKey::MovePlatform)
		{
			if (gimmick.activationType == 1 && pReq->state == 1)
			{
				if (!gimmick.isMoveTriggered)
				{
					gimmick.isMoveTriggered = true;
					gimmick.moveDelayTimer = gimmick.waitTime > 0.0f ? gimmick.waitTime : 1.0f;
					gimmick.currentState = 1;
				}
				shouldBroadcastNow = false;
			}
		}

		if (pReq->state == (UINT8)eGimmickState::GimmickPush)
		{
			// 중복 참여 체크
			bool alreadyIn = false;
			for (uint64_t uuid : gimmick.interactorUUIDs) 
			{
				if (uuid == pReq->activeUUID) alreadyIn = true;
			}

			if (!alreadyIn) 
			{
				if (!gimmick.isInteracting) 
				{
					gimmick.isInteracting = true;
					gimmick.interactWindowTimer = 0.2f; // 0.2초 동안 다른 유저 입력 대기
					gimmick.baseForce = pReq->param;
				}
				gimmick.interactorUUIDs.push_back(pReq->activeUUID);

				// 클라에서 보낸 targetPos 합산
				gimmick.totalDirX += pReq->targetPos.x;
				gimmick.totalDirZ += pReq->targetPos.z;
			}

			printf("[ProcessGimmickInteract] gimmickID=%d, state=%d, type=%d\n", pReq->gimmickID, pReq->state, gimmick.type);
			shouldBroadcastNow = false;
		}
		else 
		{
			// state가 3이 아닐 때만 브로드캐스트 패킷 세팅
			ntfPkt.activeUUID = pReq->activeUUID;
			ntfPkt.gimmickID = pReq->gimmickID;
			ntfPkt.gimmickKey = pReq->gimmickKey;
			ntfPkt.state = pReq->state;
			ntfPkt.param = pReq->param;
			ntfPkt.targetPos = (pReq->state == (UINT8)eGimmickState::Sync) ? pReq->targetPos : gimmick.position;
		}
	}

	if (shouldBroadcastNow && pRoom != nullptr)
	{
		pRoom->BroadcastPacket(ntfPkt.PacketLength, (char*)&ntfPkt);
	}
}

void GimmickManager::UpdateGimmicks(float dt, Room* pRoom)
{
	std::lock_guard<std::recursive_mutex> guard(mGimmickLock);

	for (auto& pair : mGimmicks)
	{
		ServerGimmickData& gimmick = pair.second;

		if (gimmick.type == (int)eGimmickKey::FallingPlatform && gimmick.currentState == 1)
		{
			if (gimmick.gimmickRecoverTime > 0.0f)
			{
				gimmick.gimmickRecoverTime -= dt;

				if (gimmick.gimmickRecoverTime <= 0.0f)
				{
					gimmick.currentState = 0; // Off/복구 상태로 변경
					gimmick.gimmickRecoverTime = 0.0f;

					PLAYER_GIMMICK_INTERACT_NTF_PACKET ntfPkt;
					ntfPkt.activeUUID = -1;
					ntfPkt.gimmickID = gimmick.gimmickID;
					ntfPkt.gimmickKey = gimmick.type;
					ntfPkt.state = (UINT8)eGimmickState::Restore;
					ntfPkt.param = 0.0f;
					ntfPkt.targetPos = { gimmick.position.x, gimmick.position.y, gimmick.position.z };

					pRoom->BroadcastPacket(ntfPkt.PacketLength, (char*)&ntfPkt);
					//pRoom->BroadcastPacketInRange(ntfPkt.PacketLength, (char*)&ntfPkt, ntfPkt.targetPos, 40.0f);
				}
			}
		}

		if (gimmick.type == (int)eGimmickKey::MovePlatform && gimmick.activationType == 1 && gimmick.isMoveTriggered)
		{
			gimmick.moveDelayTimer -= dt;
			

			if (gimmick.type == (int)eGimmickKey::MovePlatform && gimmick.activationType == 1)
			{
				if (gimmick.isMoveTriggered)
				{
					gimmick.moveDelayTimer -= dt;
					if (gimmick.moveDelayTimer <= 0.0f)
					{
						gimmick.isMoveTriggered = false;

						PLAYER_GIMMICK_INTERACT_NTF_PACKET ntfPkt;
						ntfPkt.activeUUID = -1;
						ntfPkt.gimmickID = gimmick.gimmickID;
						ntfPkt.gimmickKey = gimmick.type;
						ntfPkt.state = (UINT8)eGimmickState::TriggerMove;
						ntfPkt.targetPos = gimmick.endPos;
						ntfPkt.param = 0.0f;
						pRoom->BroadcastPacket(ntfPkt.PacketLength, (char*)&ntfPkt);

						gimmick.isReturning = true;
						gimmick.returnDelayTimer = 2.0f;
					}
				}

				// isMoveTriggered와 분리해서 별도로 처리
				if (gimmick.isReturning)
				{
					gimmick.returnDelayTimer -= dt;
					if (gimmick.returnDelayTimer <= 0.0f)
					{
						gimmick.isReturning = false;

						PLAYER_GIMMICK_INTERACT_NTF_PACKET ntfPkt;
						ntfPkt.activeUUID = -1;
						ntfPkt.gimmickID = gimmick.gimmickID;
						ntfPkt.gimmickKey = gimmick.type;
						ntfPkt.state = (UINT8)eGimmickState::Restore;
						ntfPkt.targetPos = gimmick.startPos;
						ntfPkt.param = 0.0f;
						pRoom->BroadcastPacket(ntfPkt.PacketLength, (char*)&ntfPkt);
					}
				}
			}
		}

		if (gimmick.isInteracting)
		{
			gimmick.interactWindowTimer -= dt;
			if (gimmick.interactWindowTimer <= 0.0f)
			{
				int playerCount = gimmick.interactorUUIDs.size();
				float finalForce = gimmick.baseForce;

				// 무게 처리
				if (gimmick.weight == 2 && playerCount < 2) 
				{
					finalForce = 0.0f; // 혼자 밀면 0
				}
				else if (playerCount >= 2) 
				{
					finalForce *= 1.5f; // 두 명이 밀면 1.5배
				}

				bool isBreakable = (gimmick.type == (int)eGimmickKey::BreakableWall || gimmick.type == (int)eGimmickKey::BreakableObj);

				if (isBreakable && !gimmick.isBombOnly)
				{
					gimmick.hp -= 1;
				}

				PLAYER_GIMMICK_INTERACT_NTF_PACKET ntfPkt;
				ntfPkt.activeUUID = -1;
				ntfPkt.gimmickID = gimmick.gimmickID;
				ntfPkt.gimmickKey = gimmick.type;

				if (gimmick.hp <= 0) 
				{
					ntfPkt.state = 99; // 파괴 상태
				}
				else 
				{
					ntfPkt.state = (UINT8)eGimmickState::GimmickPush;
					ntfPkt.targetPos.x = gimmick.totalDirX;
					ntfPkt.targetPos.y = gimmick.position.y;
					ntfPkt.targetPos.z = gimmick.totalDirZ;

					// 기믹 서버 좌표 최신화
					gimmick.position = ntfPkt.targetPos;
				}

				pRoom->BroadcastPacket(ntfPkt.PacketLength, (char*)&ntfPkt);

				gimmick.isInteracting = false;
				gimmick.interactorUUIDs.clear();
				gimmick.totalDirX = 0.0f;
				gimmick.totalDirZ = 0.0f;
			}
		}
	}
}
