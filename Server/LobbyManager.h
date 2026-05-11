#pragma once
#pragma once
#include "User.h"
#include "Packet.h"
#include "RedisManager.h"
#include "UserManager.h"
#include <mutex>
#include <functional>

class LobbyManager
{
public:
	LobbyManager() = default;
	~LobbyManager() = default;

	// PacketManager에서 받아올 Redis 포인터와 전송 함수
	void Init(RedisManager* redisMgr, UserManager* userMgr, std::function<void(UINT32, UINT32, char*)> sendFunc)
	{
		mRedisMgr = redisMgr;
		mUserManager = userMgr;
		SendPacketFunc = sendFunc;
	}

	// ==========================================
	// 1. 클라이언트 -> 서버 요청 처리 (TCP 패킷)
	// ==========================================

	// 예: 상점 구매 요청 처리
	void ProcessShopBuyRequest(User* pUser, char* pData)
	{
		// [기존 PacketManager에 있던 구매 검증 및 Redis 요청 로직을 여기로 이동]
		/*
		auto pReq = (SHOP_BUY_REQUEST_PACKET*)pData;
		// 검증 로직...

		RedisTask task;
		task.TaskID = RedisTaskID::REQUEST_SHOP_BUY;
		// 데이터 세팅 후 Redis로 푸시
		mRedisMgr->PushRequest(task);
		*/
	}

	// 예: 거래 요청 처리
	void ProcessTradeRequest(User* pUser, char* pData)
	{
		// [기존 거래 로직 이동]
	}

	// ==========================================
	// 2. Redis -> 서버 결과 처리 (DB 콜백)
	// ==========================================

	void ProcessShopDBResult(RedisTask& task)
	{
		// [기존 PacketManager에 있던 DB 결과 패킷 전송 로직 이동]
		/*
		auto pRes = (RedisShopRes*)task.pData;
		SHOP_BUY_RESPONSE_PACKET resPkt;
		// 결과 세팅...
		SendPacketFunc(task.UserIndex, resPkt.PacketLength, (char*)&resPkt);
		*/
	}

private:
	RedisManager* mRedisMgr = nullptr;
	UserManager* mUserManager = nullptr;
	std::function<void(UINT32, UINT32, char*)> SendPacketFunc;
};