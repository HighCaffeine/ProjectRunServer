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
	mRecvFuntionDictionary[(int)PACKET_ID::SYS_TIME_SYNC_REQ] = &PacketManager::ProcessTimeSync;
	
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_ENTER_REQUEST] = &PacketManager::ProcessEnterRoom;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_LEAVE_REQUEST] = &PacketManager::ProcessLeaveRoom;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_CHAT_REQUEST] = &PacketManager::ProcessRoomChatMessage;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_LIST_REQ] = &PacketManager::ProcessRoomListRequest;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_CHAR_SELECT_REQ] = &PacketManager::ProcessCharSelect;

	mRecvFuntionDictionary[(int)PACKET_ID::GAME_START_REQUEST] = &PacketManager::ProcessGameStartRequest;

	mRecvFuntionDictionary[(int)PACKET_ID::PLAYER_READY_REQUEST] = &PacketManager::ProcessPlayerReady;

//거래 & 상점 패킷처리 안써서 비활성화
#if 0
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
#endif

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
	const char* redisIp = std::getenv("REDIS_IP");
	if (redisIp == nullptr) redisIp = "127.0.0.1";

	if (!mRedisMgr->Run(redisIp, 6379, 1))
	{
		printf("[Error] PacketManager::Run() Redis 연결 실패. IP: %s\n", redisIp);
		return false;
	}

	mIsRunProcessThread = true;
	mProcessThread = std::thread([this]() { ProcessPacket(); });

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

	if (mProcessThread.joinable())
	{
		mProcessThread.join();
	}
}

void PacketManager::ClearConnectionInfo(INT32 clientIndex_)
{
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (pUser == nullptr) return;

	if (pUser->GetDomainState() == User::DOMAIN_STATE::GAME)
	{
		printf("[Lobby] 유저 %d(%s)가 게임 서버에서 접속 종료.\n", clientIndex_, pUser->GetUserId().c_str());
	}
	else if (pUser->GetDomainState() == User::DOMAIN_STATE::ROOM)
	{
		auto roomNum = pUser->GetCurrentRoom();
		mRoomManager->LeaveUser(roomNum, pUser);
	}

	mUserManager->DeleteUserInfo(pUser);
	printf("[Lobby] 유저 %d 접속 종료 완벽 처리 (메모리 삭제 완료)\n", clientIndex_);
}

void PacketManager::ReceivePacketData(const UINT32 clientIndex_, const UINT32 size_, char* pData_)
{
	m_TotalRecvBytes += size_;
	m_GrandTotalRecvBytes += size_;

	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->SetPacketData(size_, pData_);

	int count = pUser->GetAndEnqueuePendingCount();
	for (int i = 0; i < count; i++)
	{
		EnqueuePacketData(clientIndex_);
	}
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
	//CopyUserID(dbReq.UserID, "[GM]");
	//StringCbCopyA(dbReq.UserID, sizeof(dbReq.UserID), "[GM]");
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

		while (true)
		{
			auto packetData = DequePacketData();
			//if (packetData.PacketId <= (UINT16)PACKET_ID::SYS_END) break;
			if (packetData.PacketId == 0) break;
			isIdle = false;
			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);
			delete[] packetData.pDataPtr;
		}

		while (true)
		{
			auto packetData = DequeSystemPacketData();
			if (packetData.PacketId == 0) break;
			isIdle = false;
			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);
			if (packetData.pDataPtr != nullptr) delete[] packetData.pDataPtr;
		}

		while (true)
		{
			auto task = mRedisMgr->TakeResponseTask();
			if (task.TaskID == RedisTaskID::INVALID) break;
			isIdle = false;
			ProcessRecvPacket(task.UserIndex, (UINT16)task.TaskID, task.DataSize, task.pData);
			task.Release();
		}

		if (isIdle)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
	}
}

//void PacketManager::ProcessPacket()
//{
//	while (mIsRunProcessThread)
//	{
//		bool isIdle = true;
//
//		// 1. TCP 패킷 처리
//		while (true)
//		{
//			auto packetData = DequePacketData();
//			if (packetData.PacketId <= (UINT16)PACKET_ID::SYS_END) break;
//
//			isIdle = false;
//			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);
//		}
//
//		// 2. 시스템 패킷 처리
//		while (true)
//		{
//			auto packetData = DequeSystemPacketData();
//			if (packetData.PacketId == 0) break;
//
//			isIdle = false;
//			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);
//
//			if (packetData.pDataPtr != nullptr)
//				delete[] packetData.pDataPtr;
//		}
//
//		// 3. Redis 응답 처리
//		while (true)
//		{
//			auto task = mRedisMgr->TakeResponseTask();
//			if (task.TaskID == RedisTaskID::INVALID) break;
//
//			isIdle = false;
//			ProcessRecvPacket(task.UserIndex, (UINT16)task.TaskID, task.DataSize, task.pData);
//			task.Release();
//		}
//
//		// 4. 할 일이 없었을 때만 아주 잠깐 쉬기 (서버 성능 핵심!)
//		if (isIdle)
//		{
//			std::this_thread::sleep_for(std::chrono::milliseconds(1));
//		}
//	}
//}

void PacketManager::ProcessRecvPacket(const UINT32 clientIndex_, const UINT16 packetId_, const UINT16 packetSize_, char* pPacket_)
{
	printf("[Debug] Packet Received. Index: %d, ID: %d, Size: %d\n", clientIndex_, packetId_, packetSize_);

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

void PacketManager::ProcessTimeSync(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pReq = reinterpret_cast<TIME_SYNC_REQ_PACKET*>(pPacket_);
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (pUser) 
	{
		pUser->SetPing(pReq->currentPing);
	}

	TIME_SYNC_RES_PACKET res;

	// 1. 클라이언트가 보낸 시간표를 그대로 반환 (클라이언트의 Ping 계산용)
	res.clientTimestamp = pReq->clientTimestamp;

	// 2. 서버의 현재 시간(밀리초)을 구해서 세팅 (클라이언트의 시간 동기화용)
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

	res.serverTimestamp = millis; // 실제 서버 시간 주입!

	SendPacketFunc(clientIndex_, sizeof(res), (char*)&res);
}

void PacketManager::ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	mLobbyManager->ProcessLogin(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	mLobbyManager->ProcessLoginDBResult(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessNoticeDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	mLobbyManager->ProcessNoticeDBResult(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessGameStartRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	mLobbyManager->ProcessGameStartRequest(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessEnterRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	mLobbyManager->ProcessEnterRoom(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessLeaveRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	mLobbyManager->ProcessLeaveRoom(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessPlayerReady(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	mLobbyManager->ProcessPlayerReady(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessCharSelect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	mLobbyManager->ProcessCharacterSelect(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessRoomChatMessage(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	mLobbyManager->ProcessRoomChatMessage(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessRoomListRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	mLobbyManager->ProcessRoomListRequest(clientIndex_, packetSize_, pPacket_);
}
//거래 & 상점 패킷처리 안써서 비활성화
#if 0
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

#endif

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