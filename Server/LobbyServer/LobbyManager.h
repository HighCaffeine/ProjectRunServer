#pragma once
#pragma once
#include "UserModels\User.h"
#include "UserModels\UserManager.h"
#include "Packet\Packet.h"
#include "Database\RedisManager.h"

#include <mutex>
#include <functional>

class LobbyManager
{
#pragma region Trade session
	struct TradeSession
	{
		int userA, userB;	//A B의 id
		bool isLockA = false, isLockB = false;	//lock상태
		bool isConfirmA = false, isConfirmB = false;	//confirm상태
		std::vector<int> itemsA, itemsB;	//올린 아이템들
		std::vector<int> itemsASlot, itemsBSlot;
	};
	TradeSession curTS;
#pragma endregion

public:
	LobbyManager() = default;
	~LobbyManager() = default;

	void Init(RedisManager* redisMgr, UserManager* userMgr, std::function<void(UINT32, UINT32, char*)> sendFunc)
	{
		mRedisMgr = redisMgr;
		mUserManager = userMgr;
		SendPacketFunc = sendFunc;
	}


	void ProcessTradeRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pReq = (TRADE_REQUEST_PACKET*)pPacket_;
		auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);

		TRADE_REQUEST_NTF_PACKET p;
		p.reqUUID = clientIndex_;
		strncpy_s(p.reqName, pUser->GetUserId().c_str(), MAX_USER_ID_LEN);

		SendPacketFunc(pReq->targetUUID, sizeof(p), (char*)&p);
		SendPacketFunc(clientIndex_, packetSize_, pPacket_);
	}

	void ProcessTradeResponse(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pData = (TRADE_RESPONSE_PACKET*)pPacket_;
		TRADE_RESPONSE_PACKET p;
		p.isAccept = pData->isAccept;
		p.tradeUUID = clientIndex_;

		SendPacketFunc(clientIndex_, sizeof(p), (char*)&p); // 자신에게 보냄
		SendPacketFunc(pData->tradeUUID, sizeof(p), (char*)&p); // 상대방에게 보냄

		if (p.isAccept)
		{
			TradeSession ts;
			ts.userA = clientIndex_;
			ts.userB = pData->tradeUUID;
			ts.itemsA.resize(TRADE_INVENTORY_SIZE, EMPTYITEM);
			ts.itemsB.resize(TRADE_INVENTORY_SIZE, EMPTYITEM);
			ts.itemsASlot.resize(TRADE_INVENTORY_SIZE, EMPTYITEM);
			ts.itemsBSlot.resize(TRADE_INVENTORY_SIZE, EMPTYITEM);
			curTS = ts;

			TRADE_START_NTF_PACKET startToA;
			startToA.tradeUUID = curTS.userB;
			auto pTarget = mUserManager->GetUserByConnIdx(curTS.userB);
			strncpy_s(startToA.reqName, pTarget->GetUserId().c_str(), MAX_USER_ID_LEN);

			SendPacketFunc(clientIndex_, sizeof(startToA), (char*)&startToA);


			// [수정] B에게 보내는 패킷 (상대방 A의 이름 포함)
			TRADE_START_NTF_PACKET startToB;
			startToB.tradeUUID = curTS.userA;
			auto pRequester = mUserManager->GetUserByConnIdx(curTS.userA);
			strncpy_s(startToB.reqName, pRequester->GetUserId().c_str(), MAX_USER_ID_LEN);

			SendPacketFunc(pData->tradeUUID, sizeof(startToB), (char*)&startToB);
		}
	}

	void ProcessTradeItemUpdate(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pData = (TRADE_ITEM_UPDATE_PACKET*)pPacket_;
		int other;

		if (curTS.userA == clientIndex_)
		{
			other = curTS.userB;

			curTS.itemsA[pData->tradeSlot] = pData->itemID;

			if (pData->itemID != 0)
			{
				curTS.itemsASlot[pData->tradeSlot] = pData->invenSlot;
			}
			else
			{
				curTS.itemsASlot[pData->tradeSlot] = EMPTYITEM;
			}
		}
		else if (curTS.userB == clientIndex_)
		{
			other = curTS.userA;

			curTS.itemsB[pData->tradeSlot] = pData->itemID;

			// B도 똑같이 저장
			if (pData->itemID != 0)
			{
				curTS.itemsBSlot[pData->tradeSlot] = pData->invenSlot;
			}
			else
			{
				curTS.itemsBSlot[pData->tradeSlot] = EMPTYITEM;
			}
		}

		TRADE_ITEM_UPDATE_PACKET p;
		p.tradeSlot = pData->tradeSlot;
		p.invenSlot = pData->invenSlot;
		p.itemID = pData->itemID;

		//상대에게는 trade 슬룻만
		TRADE_ITEM_NTF_PACKET p2;
		p2.index = pData->tradeSlot;
		p2.itemID = pData->itemID;

		SendPacketFunc(clientIndex_, sizeof(p), (char*)&p);
		SendPacketFunc(other, sizeof(p2), (char*)&p2);
	}

	void ProcessTradeLock(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pData = (TRADE_LOCK_PACKET*)pPacket_;


		TRADE_LOCK_NTF_PACKET p;
		p.isLock = pData->isLock;

		TRADE_LOCK_PACKET selfP;
		selfP.isLock = pData->isLock;

		int other;
		if (clientIndex_ == curTS.userA)
		{
			other = curTS.userB;
			if (pData->isLock)
			{
				curTS.isLockB = true;
			}
		}
		else if (clientIndex_ == curTS.userB)
		{
			other = curTS.userA;
			if (pData->isLock)
			{
				curTS.isLockA = true;
			}
		}

		SendPacketFunc(clientIndex_, sizeof(selfP), (char*)&selfP); // 자신에게 Lock을 보냄
		SendPacketFunc(other, sizeof(p), (char*)&p); // 상대방에게 자신의 Lock을 보냄
	}

	void ProcessTradeConfirm(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pData = (TRADE_CONFIRM_PACKET*)pPacket_;
		TRADE_CONFIRM_PACKET p;
		p.isConfirm = pData->isConfirm;

		TRADE_CONFIRM_NTF_PACKET resP;
		resP.isConfirm = pData->isConfirm;
		resP.confirmUserUUID = clientIndex_;

		int other;

		if (clientIndex_ == curTS.userA)
		{
			other = curTS.userB;
			if (pData->isConfirm) curTS.isConfirmA = true;
		}
		else if (clientIndex_ == curTS.userB)
		{
			other = curTS.userA;
			if (pData->isConfirm) curTS.isConfirmB = true;
		}

		SendPacketFunc(other, sizeof(resP), (char*)&resP);
		SendPacketFunc(clientIndex_, sizeof(resP), (char*)&resP);


		if (curTS.isConfirmA && curTS.isConfirmB)
		{
			RedisTradeReq req;
			memset(&req, 0, sizeof(RedisTradeReq));

			std::fill(req.ItemsASlot, req.ItemsASlot + INVENTORY_SIZE, -1);
			std::fill(req.ItemsBSlot, req.ItemsBSlot + INVENTORY_SIZE, -1);

			auto pUserA = mUserManager->GetUserByConnIdx(curTS.userA);
			auto pUserB = mUserManager->GetUserByConnIdx(curTS.userB);

			if (pUserA) strncpy_s(req.UserAID, MAX_USER_ID_LEN + 1, pUserA->GetUserId().c_str(), _TRUNCATE);
			if (pUserB) strncpy_s(req.UserBID, MAX_USER_ID_LEN + 1, pUserB->GetUserId().c_str(), _TRUNCATE);

			for (int i = 0; i < curTS.itemsA.size(); i++)
			{
				if (curTS.itemsA[i] != EMPTYITEM)
				{
					req.ItemsAID[i] = curTS.itemsA[i];
					req.ItemsASlot[i] = curTS.itemsASlot[i];
				}
				if (curTS.itemsB[i] != EMPTYITEM)
				{
					req.ItemsBID[i] = curTS.itemsB[i];
					req.ItemsBSlot[i] = curTS.itemsBSlot[i];
				}
			}

			RedisTask task;
			task.TaskID = RedisTaskID::REQUEST_TRADE_EXCHANGE;
			task.DataSize = sizeof(RedisTradeReq);
			task.pData = new char[task.DataSize];
			memcpy(task.pData, &req, task.DataSize);

			task.UserIndex = curTS.userA;
			mRedisMgr->PushTask(task);

			printf("[Trade] Both Confirmed. Request sent to Redis.\n");
		}
	}

	void ProcessTradeDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		//이거 위에 confirm으로 옮김.
		// 두번째 확인 패킷이 왔을 때 처리
		//if (curTS.isConfirmA && curTS.isConfirmB) // 둘다 confirm이 됐을 경우
		//{
		//	RedisTradeReq req;
		//	// 유저 찾는 작업
		//	req.UserA = curTS.userA;
		//	std::string tempA = mUserManager->GetUserByConnIdx(curTS.userA)->GetUserId(); 
		//	strcpy_s(req.UserAID, tempA.length(), tempA.c_str());
		//	req.UserB = curTS.userB;
		//	std::string tempB = mUserManager->GetUserByConnIdx(curTS.userB)->GetUserId();
		//	strcpy_s(req.UserAID, tempA.length(), tempA.c_str());
		//	for (int i = 0; i < INVENTORY_SIZE; i++)
		//	{
		//		req.ItemsBID[i] = curTS.itemsB[i];
		//		req.ItemsAID[i] = curTS.itemsA[i];
		//		req.ItemsBSlot[i] = -1;
		//		req.ItemsASlot[i] = -1;
		//		if (req.ItemsBID[i] != EMPTYITEM)
		//		{
		//			req.ItemsBSlot[i] = i;
		//		}
		//		if (req.ItemsAID[i] != EMPTYITEM)
		//		{
		//			req.ItemsASlot[i] = i;
		//		}
		//	}

		//	RedisTask task;
		//	task.TaskID = RedisTaskID::REQUEST_TRADE_EXCHANGE;
		//	task.DataSize = sizeof(RedisTradeReq);
		//	task.pData = (char*)&req;

		//	mRedisMgr->PushTask(task);
		//}

		// 레디스에서 처리가 끝나고 온 결과 데이터
		// 초기화하고 거래 결과 받음
		auto pBody = (RedisTradeRes*)pPacket_;

		TRADE_RESULT_PACKET resultPkt;
		resultPkt.isSuccess = pBody->IsSuccess;

		printf("[Trade] DB Result Received. Success: %d\n", pBody->IsSuccess);

		//거래 성공시 두 유저 인벤토리 업데이트 요청
		if (pBody->IsSuccess)
		{
			auto pUserA = mUserManager->GetUserByConnIdx(curTS.userA);
			if (pUserA)
			{
				RedisInvenReq reqA;
				reqA.UserIndex = curTS.userA;
				strncpy_s(reqA.UserID, MAX_USER_ID_LEN + 1, pUserA->GetUserId().c_str(), _TRUNCATE);

				RedisTask taskA;
				taskA.TaskID = RedisTaskID::REQUEST_LOAD_INVENTORY;
				taskA.UserIndex = curTS.userA;
				taskA.DataSize = sizeof(RedisInvenReq);
				taskA.pData = new char[taskA.DataSize];
				memcpy(taskA.pData, &reqA, taskA.DataSize);

				mRedisMgr->PushTask(taskA);
			}

			auto pUserB = mUserManager->GetUserByConnIdx(curTS.userB);
			if (pUserB)
			{
				RedisInvenReq reqB;
				reqB.UserIndex = curTS.userB;
				strncpy_s(reqB.UserID, MAX_USER_ID_LEN + 1, pUserB->GetUserId().c_str(), _TRUNCATE);

				RedisTask taskB;
				taskB.TaskID = RedisTaskID::REQUEST_LOAD_INVENTORY;
				taskB.UserIndex = curTS.userB;
				taskB.DataSize = sizeof(RedisInvenReq);
				taskB.pData = new char[taskB.DataSize];
				memcpy(taskB.pData, &reqB, taskB.DataSize);

				mRedisMgr->PushTask(taskB);
			}

			printf("[Trade] Inventory Update %d & %d\n", curTS.userA, curTS.userB);
		}


		SendPacketFunc(curTS.userA, sizeof(resultPkt), (char*)&resultPkt);
		SendPacketFunc(curTS.userB, sizeof(resultPkt), (char*)&resultPkt);

		curTS.isConfirmA = false;
		curTS.isConfirmB = false;
		curTS.isLockA = false;
		curTS.isLockB = false;

		std::fill(curTS.itemsA.begin(), curTS.itemsA.end(), EMPTYITEM);
		std::fill(curTS.itemsB.begin(), curTS.itemsB.end(), EMPTYITEM);
	}

	void ProcessShopUpdateDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pBody = (RedisShopRes*)pPacket_;

		mCurrentShopItemID = pBody->ItemID;
		mNextShopUpdateTime = pBody->NextUpdateTime;

		SHOP_INFO_PACKET p;
		p.currentItemID = pBody->ItemID;
		p.nextUpdateTime = pBody->NextUpdateTime;

		mRoomManager->SendToAllUser(p.PacketLength, (char*)&p, -1, false);
		printf("[Redis] Shop Update Broadcast. Item: %d\n", p.currentItemID);
	}

	void ProcessShopBuyRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pReqPacket = reinterpret_cast<SHOP_BUY_REQUEST_PACKET*>(pPacket_);

		auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
		if (pUser == nullptr) return;

		RedisShopBuyReq dbReq;
		memset(&dbReq, 0, sizeof(RedisShopBuyReq));
		strncpy_s(dbReq.UserID, MAX_USER_ID_LEN + 1, pUser->GetUserId().c_str(), _TRUNCATE);
		dbReq.itemID = pReqPacket->itemID;

		RedisTask task;
		task.TaskID = RedisTaskID::REQUEST_SHOP_BUY;
		task.UserIndex = clientIndex_;
		task.DataSize = sizeof(RedisShopBuyReq);
		task.pData = new char[task.DataSize];
		memcpy(task.pData, &dbReq, task.DataSize);

		mRedisMgr->PushTask(task);

		printf("[Shop] Buy Request Pushed. User: %s(%d), Item: %d\n", dbReq.UserID, clientIndex_, dbReq.itemID);
	}

	void ProcessShopBuyDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pBody = (RedisShopBuyRes*)pPacket_;

		SHOP_BUY_RESPONSE_PACKET pkt;
		pkt.isSuccess = pBody->isSuccess;

		SendPacketFunc(clientIndex_, sizeof(pkt), (char*)&pkt);

		if (pkt.isSuccess)
		{
			printf("[Shop] Buy Success User: %d\n", clientIndex_);
		}
		else
		{
			printf("[Shop] Buy Failed User: %d\n", clientIndex_);
		}

		int cmdValue = -1;
		RedisTask task;
		task.TaskID = RedisTaskID::REQUEST_SHOP_UPDATE;
		task.DataSize = sizeof(int);
		task.pData = new char[sizeof(int)];
		memcpy(task.pData, &cmdValue, sizeof(int));
		mRedisMgr->PushTask(task);
	}

	//Shop
	int mCurrentShopItemID = 101;
	INT64 mNextShopUpdateTime = 0;


private:
	RedisManager* mRedisMgr = nullptr;
	UserManager* mUserManager = nullptr;
	std::function<void(UINT32, UINT32, char*)> SendPacketFunc;
};