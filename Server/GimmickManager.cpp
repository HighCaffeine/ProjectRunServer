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

	{
		std::lock_guard<std::recursive_mutex> guard(mGimmickLock);

		auto it = mGimmicks.find(pReq->gimmickID);
		if (it == mGimmicks.end())
		{
			return;
		}

		it->second.currentState = pReq->state;

		ntfPkt.gimmickID = pReq->gimmickID;
		ntfPkt.gimmickKey = pReq->gimmickKey;
		ntfPkt.state = pReq->state;
		ntfPkt.param = pReq->param;

		if (ntfPkt.gimmickKey == eGimmickKey::MovePlatform)
		{
			ntfPkt.targetPos = (pReq->state == (UINT8)eGimmickState::Sync) ? pReq->targetPos : it->second.position;
		}
		else
		{
			ntfPkt.targetPos = it->second.position;
		}
	}

	if (pRoom != nullptr)
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

					// pRoom->BroadcastPacketInRange(...) 등을 호출
					pRoom->BroadcastPacketInRange(ntfPkt.PacketLength, (char*)&ntfPkt, ntfPkt.targetPos, 40.0f);
				}
			}
		}
	}
}
