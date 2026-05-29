#define _CRT_SECURE_NO_WARNINGS

#include <cstdlib>
#include <utility>
#include <cstring>
#include <sstream>
#include <chrono>

#include "UserModels\UserManager.h"
#include "RoomManager.h"
#include "Lobby_PacketManager.h"
#include "Database\RedisManager.h"
#include "Utility\LogManager.h"
#include "LobbyManager.h"

#include <strsafe.h>


void PacketManager::Init(const UINT32 maxClient_)
{
	mRecvFuntionDictionary = std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION>();

	mRecvFuntionDictionary[(int)PACKET_ID::SYS_USER_CONNECT] = &PacketManager::ProcessUserConnect;
	mRecvFuntionDictionary[(int)PACKET_ID::SYS_USER_DISCONNECT] = &PacketManager::ProcessUserDisConnect;
	mRecvFuntionDictionary[(int)PACKET_ID::LOGIN_REQUEST] = &PacketManager::ProcessLogin;
	mRecvFuntionDictionary[(int)RedisTaskID::RESPONSE_LOGIN] = &PacketManager::ProcessLoginDBResult;
	mRecvFuntionDictionary[(int)RedisTaskID::RESPONSE_NOTICE] = &PacketManager::ProcessNoticeDBResult;
	
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_ENTER_REQUEST] = &PacketManager::ProcessEnterRoom;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_LEAVE_REQUEST] = &PacketManager::ProcessLeaveRoom;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_CHAT_REQUEST] = &PacketManager::ProcessRoomChatMessage;

	mRecvFuntionDictionary[(int)PACKET_ID::PLAYER_READY_REQUEST] = &PacketManager::ProcessPlayerReady;

	//레디스 응답 패킷
	mRecvFuntionDictionary[(int)RedisTaskID::RESPONSE_LOAD_INVENTORY] = &PacketManager::ProcessInventoryDBResult;
	mRecvFuntionDictionary[(int)RedisTaskID::RESPONSE_TRADE_EXCHANGE] = &PacketManager::ProcessTradeDBResult;
	mRecvFuntionDictionary[(int)RedisTaskID::RESPONSE_SHOP_UPDATE] = &PacketManager::ProcessShopUpdateDBResult;
	mRecvFuntionDictionary[(int)RedisTaskID::RESPONSE_SHOP_BUY] = &PacketManager::ProcessShopBuyDBResult;

	mRecvFuntionDictionary[(int)PACKET_ID::SHOP_BUY_REQUEST] = &PacketManager::ProcessShopBuyRequest;

	//거래 패킷
	mRecvFuntionDictionary[(int)PACKET_ID::TRADE_REQUEST] = &PacketManager::ProcessTradeRequest;
	mRecvFuntionDictionary[(int)PACKET_ID::TRADE_RESPONSE] = &PacketManager::ProcessTradeResponse;
	mRecvFuntionDictionary[(int)PACKET_ID::TRADE_ITEM_UPDATE] = &PacketManager::ProcessTradeItemUpdate;
	mRecvFuntionDictionary[(int)PACKET_ID::TRADE_LOCK] = &PacketManager::ProcessTradeLock;
	mRecvFuntionDictionary[(int)PACKET_ID::TRADE_CONFIRM] = &PacketManager::ProcessTradeConfirm;

	CreateCompent(maxClient_);

	mRedisMgr = new RedisManager;// std::make_unique<RedisManager>();

	mLobbyManager = new LobbyManager();
	mLobbyManager->Init(mRedisMgr, mUserManager, mRoomManager, SendPacketFunc);
}

void PacketManager::CreateCompent(const UINT32 maxClient_)
{
	mUserManager = new UserManager;
	mUserManager->Init(maxClient_);

	LogManager::Init();
		
	UINT32 startRoomNummber = 0;
	UINT32 maxRoomCount = 2;
	UINT32 maxRoomUserCount = 2;
	mRoomManager = new RoomManager;
	mRoomManager->SendPacketFunc = SendPacketFunc;
	mRoomManager->Init(startRoomNummber, maxRoomCount, maxRoomUserCount);
}

bool PacketManager::Run()
{
	int retryCount = 0;
	const char* redisIp = std::getenv("REDIS_IP");

	//상점 업데이트용
	/*int cmdValue = -1;
	RedisTask task;
	task.TaskID = RedisTaskID::REQUEST_SHOP_UPDATE;
	task.DataSize = sizeof(int);
	task.pData = new char[sizeof(int)];
	memcpy(task.pData, &cmdValue, sizeof(int));
	mRedisMgr->PushTask(task);*/


	return true;
}

void PacketManager::End()
{
	double finalRecvMB = m_GrandTotalRecvBytes / (1024.0 * 1024.0);
	double finalSendMB = m_GrandTotalSendBytes / (1024.0 * 1024.0);

	spdlog::info("==================================================");
	spdlog::info("[Server Closed] Final Total Bandwidth -> In: {:.2f} MB | Out: {:.2f} MB", finalRecvMB, finalSendMB);
	spdlog::info("==================================================");

	mRedisMgr->End();
	if (mLobbyManager) delete mLobbyManager;

	mIsRunProcessThread = false;
	mIsRunLogicThread = false;

	if (mProcessThread.joinable())
	{
		mProcessThread.join();
	}

	if (mLogicThread.joinable())
	{
		mLogicThread.join();
	}
}

void PacketManager::ClearConnectionInfo(INT32 clientIndex_)
{
	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (pReqUser == nullptr) return;

	if (pReqUser->GetDomainState() == User::DOMAIN_STATE::ROOM)
	{
		auto roomNum = pReqUser->GetCurrentRoom();
		mRoomManager->LeaveUser(roomNum, pReqUser);
	}

	mUserManager->DeleteUserInfo(pReqUser);
}

void PacketManager::ReceivePacketData(const UINT32 clientIndex_, const UINT32 size_, char* pData_)
{
	m_TotalRecvBytes += size_;
	m_GrandTotalRecvBytes += size_;

	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->SetPacketData(size_, pData_);

	EnqueuePacketData(clientIndex_);
}

void PacketManager::EnqueuePacketData(const UINT32 clientIndex_)
{
	std::lock_guard<std::mutex> guard(mLock);
	mInComingPacketUserIndex.push_back(clientIndex_);
}

PacketInfo PacketManager::DequePacketData()
{
	UINT32 userIndex = 0;

	{
		std::lock_guard<std::mutex> guard(mLock);
		if (mInComingPacketUserIndex.empty())
		{
			return PacketInfo();
		}

		userIndex = mInComingPacketUserIndex.front();
		mInComingPacketUserIndex.pop_front();
	}

	auto pUser = mUserManager->GetUserByConnIdx(userIndex);
	auto packetData = pUser->GetPacket();
	packetData.ClientIndex = userIndex;
	return packetData;
}

void PacketManager::PushSystemPacket(PacketInfo packet_)
{
	std::lock_guard<std::mutex> guard(mLock);
	mSystemPacketQueue.push_back(packet_);
}

PacketInfo PacketManager::DequeSystemPacketData()
{

	std::lock_guard<std::mutex> guard(mLock);
	if (mSystemPacketQueue.empty())
	{
		return PacketInfo();
	}

	auto packetData = mSystemPacketQueue.front();
	mSystemPacketQueue.pop_front();

	return packetData;
}

void PacketManager::RedisReqNotice(User& user, const std::string noticeMsg)
{
	RedisNoticeReq dbReq;
	CopyUserID(dbReq.UserID, "[GM]");
	StringCbCopyA(dbReq.UserID, sizeof(dbReq.UserID), "[GM]");
	StringCbCopyA(dbReq.Message, sizeof(dbReq.Message), noticeMsg.c_str());

	RedisTask task;
	task.UserIndex = user.GetNetConnIdx();
	task.TaskID = RedisTaskID::REQUEST_NOTICE;
	task.DataSize = sizeof(RedisNoticeReq);
	task.pData = new char[task.DataSize];
	CopyMemory(task.pData, (char*)&dbReq, task.DataSize);
	mRedisMgr->PushTask(task);

	printf("[Redis Request] Notice. userUUID(%d), userID(%s), msg:%s\n", user.GetNetConnIdx(), user.GetUserId(), noticeMsg.c_str());
}


void PacketManager::ProcessPacket()
{
	static auto lastCheckTime = std::chrono::steady_clock::now();

	while (mIsRunProcessThread)
	{
		bool isIdle = true;

		if (auto packetData = DequePacketData(); packetData.PacketId > (UINT16)PACKET_ID::SYS_END)
		{
			isIdle = false;
			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);
		}

		if (auto packetData = DequeSystemPacketData(); packetData.PacketId != 0)
		{
			isIdle = false;
			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);

			if (packetData.pDataPtr != nullptr)
			{
				delete[] packetData.pDataPtr;
			}
		}

		if (auto task = mRedisMgr->TakeResponseTask(); task.TaskID != RedisTaskID::INVALID)
		{
			isIdle = false;
			ProcessRecvPacket(task.UserIndex, (UINT16)task.TaskID, task.DataSize, task.pData);
			task.Release();
		}

		if(isIdle)
		{
			std::this_thread::yield();
		}
	}
}

void PacketManager::ProcessRecvPacket(const UINT32 clientIndex_, const UINT16 packetId_, const UINT16 packetSize_, char* pPacket_)
{
	//printf("[Debug] Packet Received. Index: %d, ID: %d, Size: %d\n", clientIndex_, packetId_, packetSize_);

	auto iter = mRecvFuntionDictionary.find(packetId_);
	if (iter != mRecvFuntionDictionary.end())
	{
		(this->*(iter->second))(clientIndex_, packetSize_, pPacket_);
	}
	else
	{
		printf("[Error] Unregistered Packet ID: %d\n", packetId_);
	}
}

void PacketManager::ProcessUserConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	printf("[ProcessUserConnect] clientIndex: %d\n", clientIndex_);
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->Clear();
}

void PacketManager::ProcessUserDisConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	printf("[ProcessUserDisConnect] clientIndex: %d\n", clientIndex_);
	ClearConnectionInfo(clientIndex_);
}

//redis 로그인
//void PacketManager::ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
//{ 
//	if (LOGIN_REQUEST_PACKET_SIZE != packetSize_)
//	{
//		return;
//	}
//
//	auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPacket_);
//
//	auto pUserID = pLoginReqPacket->userID;
//	printf("requested user id = %s\n", pUserID);
//
//	LOGIN_RESPONSE_PACKET loginResPacket;
//
//	if (mUserManager->GetCurrentUserCnt() >= mUserManager->GetMaxUserCnt()) 
//	{ 
//		//접속자수가 최대수를 차지해서 접속불가
//		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_USED_ALL_OBJ;
//		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET) , (char*)&loginResPacket);
//		return;
//	}
//
//	//여기에서 이미 접속된 유저인지 확인하고, 접속된 유저라면 실패한다.
//	//if (mUserManager->FindUserIndexByID(pUserID) == -1) 
//	//{ 
//	//	RedisLoginReq dbReq;
//	//	CopyUserID(dbReq.UserID, pLoginReqPacket->userID);
//	//	CopyMemory(dbReq.UserPW, pLoginReqPacket->userPW, (MAX_USER_PW_LEN + 1));
//
//	//	RedisTask task;
//	//	task.UserIndex = clientIndex_;
//	//	task.TaskID = RedisTaskID::REQUEST_LOGIN;
//	//	task.DataSize = sizeof(RedisLoginReq);
//	//	task.pData = new char[task.DataSize];
//	//	CopyMemory(task.pData, (char*)&dbReq, task.DataSize);
//	//	mRedisMgr->PushTask(task);
//
//	//	printf("Login To Redis user id = %s\n", pUserID);
//	//}
//	//else 
//	//{
//	//	//접속중인 유저여서 실패를 반환한다.
//	//	loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_ALREADY;
//	//	SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
//	//	return;
//	//}
//	
//
//	RedisLoginRes bodyData;
//	memset(&bodyData, 0, sizeof(RedisLoginRes));
//	bodyData.Result = (UINT16)ERROR_CODE::NONE;
//
//	RedisTask resTask;
//	resTask.UserIndex = clientIndex_;
//	resTask.TaskID = RedisTaskID::RESPONSE_LOGIN;
//	resTask.DataSize = sizeof(RedisLoginRes);
//	resTask.pData = new char[resTask.DataSize];
//	CopyMemory(resTask.pData, (char*)&bodyData, resTask.DataSize);
//
//	mRedisMgr->PushResponse(resTask);
//}

void PacketManager::ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	if (sizeof(LOGIN_REQUEST_PACKET) != packetSize_) return;

	auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPacket_);
	auto pUserID = pLoginReqPacket->userID;

	printf("Dummy Login Attempt: %s\n", pUserID);

	// 접속자 수 체크
	if (mUserManager->GetCurrentUserCnt() >= mUserManager->GetMaxUserCnt())
	{
		LOGIN_RESPONSE_PACKET res;
		res.Result = (UINT16)ERROR_CODE::LOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(clientIndex_, sizeof(res), (char*)&res);
		return;
	}

	// Redis 거치지 않고 즉시 유저 객체에 로그인 정보 세팅
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (pUser) 
	{
		pUser->SetLogin(pUserID);
	}

	// 즉시 클라이언트에 로그인 성공 패킷 전송
	LOGIN_RESPONSE_PACKET loginResPacket;

	loginResPacket.Result = clientIndex_;

	SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);

	printf("Dummy Login Success: UserIndex(%d) ID(%s)\n", clientIndex_, pUserID);
}

void PacketManager::ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	printf("ProcessLoginDBResult. UserIndex: %d\n", clientIndex_);

	auto pBody = (RedisLoginRes*)pPacket_;

	//if (pBody->Result == (UINT16)ERROR_CODE::NONE)
	//{
	//	//로그인 완료로 변경한다
	//	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	//	pUser->SetLogin(pBody->UserID);
	//}

	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->SetLogin(pBody->UserID);

	LOGIN_RESPONSE_PACKET loginResPacket;
	//loginResPacket.Result = pBody->Result;
	// Unity3D 대응용
	loginResPacket.Result = clientIndex_;
	SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
}

void PacketManager::ProcessNoticeDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	printf("ProcessNoticeDBResult. UserIndex: %d\n", clientIndex_);

	auto pBody = (RedisNoticeRes*)pPacket_;

	ROOM_CHAT_NOTIFY_PACKET roomChatNtfyPkt;
	StringCbCopyA(roomChatNtfyPkt.userID, sizeof(roomChatNtfyPkt.userID), "[GM]");
	StringCbCopyA(roomChatNtfyPkt.Msg, sizeof(roomChatNtfyPkt.Msg), pBody->Message);

	mRoomManager->SendToAllUser(roomChatNtfyPkt.PacketLength, (char*)&roomChatNtfyPkt, clientIndex_, false);
}



void PacketManager::ProcessEnterRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);

	auto pRoomEnterReqPacket = reinterpret_cast<ROOM_ENTER_REQUEST_PACKET*>(pPacket_);
	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (!pReqUser || pReqUser == nullptr) 
	{
		return;
	}

	auto roomNumber = pRoomEnterReqPacket->RoomNumber;
	
	INT32 currentRoom = pReqUser->GetCurrentRoom();
	if (currentRoom != -1)
	{
		mRoomManager->LeaveUser(currentRoom, pReqUser);
	}

	Vector3 zeroPos = { 0.0f, 0.0f, 0.0f };
	Quaternion zeroRot = { 0.0f, 0.0f, 0.0f, 1.0f };
	float zeroAxis = 0.0f;

	pReqUser->SetPosition(zeroPos);
	pReqUser->mLastSentPos = zeroPos;
	pReqUser->SetTarget(zeroPos, zeroRot, zeroAxis, zeroAxis, 0);
			
	// Room::EnterUser()에서 입장하는 유저에게 방안 유저 리스트를 전송한다
	auto enterResult = mRoomManager->EnterUser(roomNumber, pReqUser);

	{
		ROOM_ENTER_RESPONSE_PACKET roomEnterResPacket;
		roomEnterResPacket.Result = enterResult;
		SendPacketFunc(clientIndex_, sizeof(ROOM_ENTER_RESPONSE_PACKET), (char*)&roomEnterResPacket);
	}
	printf("Response Packet Sended");

	if (enterResult != (UINT16)ERROR_CODE::NONE)
	{
		spdlog::warn("[Enter] User({}) Failed. Error: {}", clientIndex_, enterResult);
		return; 
	}
	else
	{
		spdlog::info("[Enter] User({}) Entered Room Number [{}]", clientIndex_, roomNumber);
	}
	auto pRoom = mRoomManager->GetRoomByNumber(roomNumber);


	// 방안 유저들에게 입장하는 유저 정보 전송
	pRoom->NotifyUserEnter(clientIndex_, pReqUser->GetUserId());

	//인벤토리 처리
	if (enterResult == (UINT16)ERROR_CODE::NONE)
	{
		RedisInvenReq req;
		memset(&req, 0, sizeof(RedisInvenReq));

		req.UserIndex = clientIndex_;
		strncpy_s(req.UserID, MAX_USER_ID_LEN + 1, pReqUser->GetUserId().c_str(), _TRUNCATE);

		RedisTask task;
		task.TaskID = RedisTaskID::REQUEST_LOAD_INVENTORY;
		task.DataSize = sizeof(RedisInvenReq);
		task.pData = new char[task.DataSize];
		memcpy(task.pData, &req, task.DataSize);
		task.UserIndex = clientIndex_;
		mRedisMgr->PushTask(task);
		printf("[Debug] Room Enter Success -> Request Inventory Load for User %d\n", clientIndex_);

		int cmdValue = -2;
		RedisTask shopReq;
		shopReq.TaskID = RedisTaskID::REQUEST_SHOP_UPDATE;
		shopReq.DataSize = sizeof(int);
		shopReq.pData = new char[sizeof(int)];
		memcpy(shopReq.pData, &cmdValue, sizeof(int));
		shopReq.UserIndex = clientIndex_;
		mRedisMgr->PushTask(shopReq);
	}

	SHOP_INFO_PACKET shopPkt;
	shopPkt.currentItemID = mLobbyManager->mCurrentShopItemID;
	shopPkt.nextUpdateTime = mLobbyManager->mNextShopUpdateTime;

	SendPacketFunc(clientIndex_, sizeof(shopPkt), (char*)&shopPkt);
}


void PacketManager::ProcessLeaveRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);
	UNREFERENCED_PARAMETER(pPacket_);

	ROOM_LEAVE_RESPONSE_PACKET roomLeaveResPacket;

	auto reqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	auto roomNum = reqUser->GetCurrentRoom();
				
	roomLeaveResPacket.Result = mRoomManager->LeaveUser(roomNum, reqUser);
	SendPacketFunc(clientIndex_, sizeof(ROOM_LEAVE_RESPONSE_PACKET), (char*)&roomLeaveResPacket);

	spdlog::info("[Leave] User({}) Left Room", clientIndex_);
}

void PacketManager::ProcessPlayerReady(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto* req = reinterpret_cast<PLAYER_READY_REQUEST_PACKET*>(pPacket_);

	// 유저 찾기
	auto user = mUserManager->GetUserByConnIdx(clientIndex_);
	if (user == nullptr)
	{
		return;
	}

	// 유저가 속한 방 찾기
	auto room = mRoomManager->GetRoomByNumber(user->GetCurrentRoom());
	if (room != nullptr)
	{
		room->ProcessPlayerReady(user, req->isReady);
	}
}

void PacketManager::ProcessRoomChatMessage(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);

	auto pRoomChatReqPacketet = reinterpret_cast<ROOM_CHAT_REQUEST_PACKET*>(pPacket_);
		
	ROOM_CHAT_RESPONSE_PACKET roomChatResPacket;
	roomChatResPacket.Result = (INT16)ERROR_CODE::NONE;

	auto reqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	auto roomNum = reqUser->GetCurrentRoom();

	auto pRoom = mRoomManager->GetRoomByNumber(roomNum);
	if (pRoom == nullptr)
	{
		roomChatResPacket.Result = (INT16)ERROR_CODE::CHAT_ROOM_INVALID_ROOM_NUMBER;
		SendPacketFunc(clientIndex_, sizeof(ROOM_CHAT_RESPONSE_PACKET), (char*)&roomChatResPacket);
		return;
	}

	// 특수 명령 "/c"
	const std::string cmdMessage = pRoomChatReqPacketet->Message;
	if (cmdMessage.find("/c", 0) == 0)
	{
		// Npc를 생성한다
		pRoom->EnterNpc();
		return;
	}

	// 공지 "/n"
	//const std::string cmdMessage = pRoomChatReqPacketet->Message;
	if (cmdMessage.find("/n", 0) == 0)
	{
		// 앞에 "/n"로 시작하는 부분을 잘라낸다
		const std::string noticeMsg = cmdMessage.substr(2);
		RedisReqNotice(*reqUser, noticeMsg);
		return;
	}

	// 큐브 소환 명령어
	/*if (cmdMessage.find("/spawn cube") == 0)
	{
		float cx = 5.0f, cz = 5.0f;
		sscanf_s(cmdMessage.c_str(), "/spawn cube %f %f", &cx, &cz);
		pRoom->EnterCube(cx, cz);
		return;
	}*/
	
	//shop 업데이트
	if (cmdMessage.find("/shop_reset", 0) == 0)
	{
		printf("[GM Command] Shop Reset Req by %d\n", clientIndex_);

		RedisTask task;
		task.TaskID = RedisTaskID::REQUEST_SHOP_UPDATE;
		task.UserIndex = clientIndex_;
		task.DataSize = 0;
		task.pData = nullptr;
		mRedisMgr->PushTask(task);

		return;
	}

	if (cmdMessage.find("/t add") == 0)
	{
		std::string s = cmdMessage.substr(7);
		int time = std::stoi(s);

		printf("[GM Command] Time Add %d hours Req by %d\n", time, clientIndex_);

		RedisTask task;
		task.TaskID = RedisTaskID::REQUEST_SHOP_UPDATE;
		task.DataSize = sizeof(int);
		task.pData = new char[sizeof(int)];
		memcpy(task.pData, &time, sizeof(int));

		mRedisMgr->PushTask(task);

		return;
	}

	SendPacketFunc(clientIndex_, sizeof(ROOM_CHAT_RESPONSE_PACKET), (char*)&roomChatResPacket);

	pRoom->NotifyChat(clientIndex_, reqUser->GetUserId().c_str(), pRoomChatReqPacketet->Message);		
}

void PacketManager::ProcessInventoryDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pBody = (RedisInvenRes*)pPacket_;
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (pUser == nullptr)
	{
		printf("[Error] ProcessInventoryDBResult: User Not Found. Index: %d\n", clientIndex_);
		return;
	}

	INVENTORY_INFO_PACKET p;
	p.userUUID = clientIndex_;

	for (int i = 0; i < INVENTORY_SIZE; i++)
	{
		int itemID = pBody->ItemSlots[i];

		pUser->SetInventory(i, itemID);
		p.itemIDs[i] = itemID;
	}

	SendPacketFunc(clientIndex_, sizeof(p), (char*)&p);
	printf("[Inventory] Loaded for User Index: %d\n", clientIndex_);
}

//50ms마다 게임 상태 업데이트
void PacketManager::LogicThread()
{
	auto nextTick = std::chrono::steady_clock::now();
	const auto tickInterval = std::chrono::milliseconds(20); // 50Hz (0.02s)

	auto lastBandwidthCheckTime = std::chrono::steady_clock::now();
	while (mIsRunLogicThread) 
	{
		auto now = std::chrono::steady_clock::now();

		if (now >= nextTick) 
		{
			// 모든 방(Room)의 물리 및 로직 업데이트
			// mRoomManager 내의 모든 Room을 순회하며 Update(0.02f) 호출
			for (int i = 0; i < mRoomManager->GetMaxRoomCount(); ++i) 
			{
				if (auto pRoom = mRoomManager->GetRoomByNumber(i)) 
				{
					pRoom->Update(FIXED_DELTA_TIME);
					
					//nav 사용 X
					//NavMeshManager::GetInstance()->UpdateTileCache(FIXED_DELTA_TIME);
				}
			}

			nextTick += tickInterval;
		}

		if (std::chrono::duration_cast<std::chrono::seconds>(now - lastBandwidthCheckTime).count() >= 1)
		{
			// 1초 동안 모인 데이터를 KB 단위로 변환 (현재 속도)
			double recvKBps = m_TotalRecvBytes / 1024.0;
			double sendKBps = m_TotalSendBytes / 1024.0;

			// 누적 총 데이터를 MB 단위로 변환 (총 대역폭)
			double totalRecvMB = m_GrandTotalRecvBytes / (1024.0 * 1024.0);
			double totalSendMB = m_GrandTotalSendBytes / (1024.0 * 1024.0);

			if (m_TotalRecvBytes > 0 || m_TotalSendBytes > 0)
			{
				spdlog::info("[Bandwidth] Speed - In: {:.2f} KB/s | Out: {:.2f} KB/s  ||  Total - In: {:.2f} MB | Out: {:.2f} MB",
					recvKBps, sendKBps, totalRecvMB, totalSendMB);
			}

			m_TotalRecvBytes = 0;
			m_TotalSendBytes = 0;

			// 타이머 갱신
			lastBandwidthCheckTime = now;
		}

		// CPU 과점유 방지
		now = std::chrono::steady_clock::now();
		if (now < nextTick)
		{
			std::this_thread::sleep_for(nextTick - now);
		}
	}
}

void PacketManager::ProcessTradeRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_) 
{
	mLobbyManager->ProcessTradeRequest(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessTradeResponse(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_) 
{
	mLobbyManager->ProcessTradeResponse(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessTradeItemUpdate(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	mLobbyManager->ProcessTradeItemUpdate(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessTradeLock(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_) 
{
	mLobbyManager->ProcessTradeLock(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessTradeConfirm(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_) 
{
	mLobbyManager->ProcessTradeConfirm(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessTradeDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_) 
{
	mLobbyManager->ProcessTradeDBResult(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessShopBuyRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_) 
{
	mLobbyManager->ProcessShopBuyRequest(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessShopBuyDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_) 
{
	mLobbyManager->ProcessShopBuyDBResult(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessShopUpdateDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_) 
{
	mLobbyManager->ProcessShopUpdateDBResult(clientIndex_, packetSize_, pPacket_);
}

Vector3 stringToVector3(const std::string& s) 
{
	std::stringstream ss(s);
	char discardChar; // To consume parentheses and commas
	float x, y, z;

	// Expected format: "x, y, z"
	ss >> x >> discardChar >> y >> discardChar >> z;

	if (ss.fail()) {
		std::cerr << "Error parsing Vector3 string: " << s << std::endl;
		return Vector3(); // Return a default Vector3 or throw an exception
	}
	return Vector3{ x, y, z };
}