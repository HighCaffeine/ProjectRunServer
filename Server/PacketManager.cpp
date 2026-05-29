#define _CRT_SECURE_NO_WARNINGS

#include <cstdlib>
#include <utility>
#include <cstring>
#include <sstream>
#include <chrono>

#include "UserManager.h"
#include "RoomManager.h"
#include "PacketManager.h"
#include "RedisManager.h"
#include "LogManager.h"
#include "LobbyManager.h"

#include <strsafe.h>


void PacketManager::Init(const UINT32 maxClient_)
{
	mRecvFuntionDictionary = std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION>();

	mRecvFuntionDictionary[(int)PACKET_ID::SYS_USER_CONNECT] = &PacketManager::ProcessUserConnect;
	mRecvFuntionDictionary[(int)PACKET_ID::SYS_USER_DISCONNECT] = &PacketManager::ProcessUserDisConnect;

	mRecvFuntionDictionary[(int)PACKET_ID::SYS_TIME_SYNC_REQ] = &PacketManager::ProcessTimeSync;

	mRecvFuntionDictionary[(int)PACKET_ID::LOGIN_REQUEST] = &PacketManager::ProcessLogin;
	mRecvFuntionDictionary[(int)RedisTaskID::RESPONSE_LOGIN] = &PacketManager::ProcessLoginDBResult;
	mRecvFuntionDictionary[(int)RedisTaskID::RESPONSE_NOTICE] = &PacketManager::ProcessNoticeDBResult;
	
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_ENTER_REQUEST] = &PacketManager::ProcessEnterRoom;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_LEAVE_REQUEST] = &PacketManager::ProcessLeaveRoom;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_CHAT_REQUEST] = &PacketManager::ProcessRoomChatMessage;
	mRecvFuntionDictionary[(int)PACKET_ID::PLAYER_MOVEMENT] = &PacketManager::ProcessPlayerMovement;
	mRecvFuntionDictionary[(int)PACKET_ID::PLAYER_STATUS_NTF] = &PacketManager::ProcessPlayerStateChange;
	mRecvFuntionDictionary[(int)PACKET_ID::DUNGEON_ESCAPE_REQ] = &PacketManager::ProcessDungeonEscape;
	mRecvFuntionDictionary[(int)PACKET_ID::SCENE_SYNC_REQ] = &PacketManager::ProcessSceneSync;

	mRecvFuntionDictionary[(int)PACKET_ID::PLAYER_READY_REQUEST] = &PacketManager::ProcessPlayerReady;

	//플레이어 물리 / 기믹 처리
	mRecvFuntionDictionary[(int)PACKET_ID::PLAYER_ACTION_REQUEST] = &PacketManager::ProcessPlayerAction;
	mRecvFuntionDictionary[(int)PACKET_ID::PLAYER_GIMMICK_INTERACT_REQUEST] = &PacketManager::ProcessGimmickInteract;
	mRecvFuntionDictionary[(int)PACKET_ID::PLAYER_DEAD_REQ] = &PacketManager::ProcessPlayerDead;

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


	//몬스터 패킷
	mRecvFuntionDictionary[(int)PACKET_ID::MONSTER_DEAD_REQ] = &PacketManager::ProcessMonsterDeadRequest;
	mRecvFuntionDictionary[(int)PACKET_ID::MONSTER_STATE_NTF] = &PacketManager::ProcessMonsterStateChange;
	mRecvFuntionDictionary[(int)PACKET_ID::MONSTER_MOVEMENT] = &PacketManager::ProcessMonsterMovement;

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
	/*const char* redisIp = std::getenv("REDIS_IP");
	if (mRedisMgr->Run(redisIp ? redisIp : "host.docker.internal", 6379, 1) == false)
	{
		return false;
	}*/

	int retryCount = 0;
	//const char* redisIp = std::getenv("REDIS_IP");

	
	/*std::string redisIpStr;
	char* buf = nullptr;
	size_t s = 0;

	if (_dupenv_s(&buf, &s, "REDIS_IP") == 0 && buf != nullptr)
	{
		redisIpStr = buf;
		free(buf);
	}

	Sleep(5000);

	const char* redisIp = redisIpStr.c_str();*/

	const char* redisIp = std::getenv("REDIS_IP");

	//while (true)
	//{
	//	if (mRedisMgr->Run(redisIp ? redisIp : "host.docker.internal", 6379, 2))
	//	{
	//		printf("[SUCCESS] Redis Connected!\n");
	//		break;
	//	}

	//	retryCount++;
	//	printf("[RETRY %d] Redis connection failed. Retrying in 1s...\n", retryCount);

	//	if (retryCount > 15)
	//	{
	//		printf("[FATAL] Redis connection failed after 10 attempts.\n");
	//		return false;
	//	}

	//	Sleep(1000); // 1초 대기 후 다시 시도
	//}

	if (UDPRun() == false) return false;

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

bool PacketManager::UDPRun()
{
	mUdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	sockaddr_in udpServerAddr;
	udpServerAddr.sin_family = AF_INET;
	udpServerAddr.sin_port = htons(5025);
	udpServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (::bind(mUdpSocket, (sockaddr*)&udpServerAddr, sizeof(udpServerAddr)) == SOCKET_ERROR)
	{
		printf("[Error] UDP Bind Failed: %d\n", WSAGetLastError());
		return false;
	}

	mIsRunProcessThread = true;
	mIsRunLogicThread = true; // 플래그 활성화

	mProcessThread = std::thread([this]() { ProcessPacket(); });
	mLogicThread = std::thread([this]() { LogicThread(); });
	mUdpRecvThread = std::thread([this]() { UDPRecvThread(); }); // UDP 수신 스레드

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

	if (mUdpRecvThread.joinable())
	{
		closesocket(mUdpSocket);
		mUdpRecvThread.join();
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

		/*auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCheckTime).count() >= 1)
		{
			lastCheckTime = now;

			int cmdValue = -1;
			RedisTask task;
			task.TaskID = RedisTaskID::REQUEST_SHOP_UPDATE;
			task.DataSize = sizeof(int);
			task.pData = new char[sizeof(int)];
			memcpy(task.pData, &cmdValue, sizeof(int));
			mRedisMgr->PushTask(task);
		}*/

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

void PacketManager::ProcessTimeSync(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	if (sizeof(TIME_SYNC_REQ_PACKET) != packetSize_) return;

	auto pReq = reinterpret_cast<TIME_SYNC_REQ_PACKET*>(pPacket_);

	TIME_SYNC_RES_PACKET resPkt;
	resPkt.clientTimestamp = pReq->clientTimestamp;

	// 서버의 현재 유닉스 타임스탬프 생성
	auto now = std::chrono::system_clock::now();
	resPkt.serverTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

	SendPacketFunc(clientIndex_, sizeof(TIME_SYNC_RES_PACKET), (char*)&resPkt);
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

void PacketManager::ProcessPlayerMovement(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	//UNREFERENCED_PARAMETER(packetSize_);
	//UNREFERENCED_PARAMETER(pPacket_);

	//auto playerMovement = reinterpret_cast<PLAYER_MOVEMENT_PACKET*>(pPacket_);

	//if (playerMovement->userUUID != clientIndex_)
	//{
	//	printf("[ProcessPlayerMovement] userUUID(%lld) != clientIndex_(%ld)\n", playerMovement->userUUID, clientIndex_);
	//	return;
	//}


	//printf("[ProcessPlayerMovement] userUUID(%lld) dx=%f, dy=%f, rx:%f, ry:%f, rz:%f \n", playerMovement->userUUID, 
	//	playerMovement->dx, playerMovement->dy, playerMovement->rotation.x, playerMovement->rotation.y, playerMovement->rotation.z);

	//auto reqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	//auto roomNum = reqUser->GetCurrentRoom();

	//auto pRoom = mRoomManager->GetRoomByNumber(roomNum);
	//if (pRoom == nullptr)
	//{
	//	printf("[ProcessPlayerMovement] pRoom == nullptr userUUID(%lld), roomNum(%d)\n", playerMovement->userUUID, roomNum);
	//	return;
	//}

	//UPDATE_PLAYER_MOVEMENT_PACKET updateMovement;
	//updateMovement.userUUID = playerMovement->userUUID;
	//updateMovement.rotation = playerMovement->rotation;
	//// Movement 처리
	//updateMovement.motion = reqUser->UpdateMovement(playerMovement->dx, playerMovement->dy, playerMovement->rotation);
	//
	//pRoom->SendToAllUser(updateMovement.PacketLength, (char*)&updateMovement, clientIndex_, false);

	auto pMovePkt = reinterpret_cast<PLAYER_MOVEMENT_PACKET*>(pPacket_);
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (pUser) 
	{
		//물리 변경으로 최종 확정 좌표를 client로부터 받아서 이동

		// 서버의 Actor 객체에 목적지 좌표만 설정함
		// 실제 이동은 LogicThread -> Room::Update -> Actor::UpdateServerPhysics에서 처리
		//pUser->SetInput(pMovePkt->dx, pMovePkt->dz, pMovePkt->inputSeq);
		pUser->SetTarget(pMovePkt->currentPos, pMovePkt->currentRot, pMovePkt->axisH, pMovePkt->axisV, pMovePkt->inputSeq);
	}
}

void PacketManager::ProcessPlayerStateChange(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pReq = (PLAYER_STATUS_NTF_PACKET*)pPacket_;
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (!pUser) return;

	auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
	if (pRoom)
	{
		//상태 업데이트 수정으로 송신측 ID X
		//pReq->userUUID = clientIndex_;

		//브로드캐스트
		//pRoom->SendToAllUser(pReq->PacketLength, pPacket_, clientIndex_, true);
		pRoom->BroadcastPacket(pReq->PacketLength, pPacket_);
		printf("[State Sync] User %d changed state to %d\n", clientIndex_, pReq->newState);
	}
}

void PacketManager::ProcessDungeonEscape(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (!pUser) return;

	auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
	if (pRoom)
	{
		pRoom->ProcessEscapeRequest(pUser);
	}
}

void PacketManager::ProcessSceneSync(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (!pUser) return;

	auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
	if (pRoom != nullptr) 
	{
		printf("2. [서버 수신] %s 유저의 SCENE_SYNC_REQ 받음. (Room: %d)\\n", pUser->GetUserId().c_str(), pRoom->GetRoomNumber());
		pRoom->SyncRoomStateToUser(pUser);
	}
	else 
	{
		printf("2. [서버 에러] %s 유저의 방을 찾을 수 없음!\\n", pUser->GetUserId().c_str());
	}
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

void PacketManager::ProcessPlayerAction(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	//auto pReq = (PLAYER_ACTION_REQUEST_PACKET*)pPacket_;
	//auto reqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	//if (!reqUser) return;

	//auto pRoom = mRoomManager->GetRoomByNumber(reqUser->GetCurrentRoom());
	//if (!pRoom) return;

	//Actor* target = pRoom->GetActorByUUID(pReq->targetUUID);
	//if (target)
	//{
	//	bool isPush = (pReq->actionType == ACTION_TYPE::PUSH);

	//	Vector3 myPos = reqUser->GetPosition();
	//	Vector3 targetPos = target->GetPosition();

	//	// 타겟 위치에서 내 위치를 바라보는 방향 벡터
	//	Vector3 toMe = { myPos.x - targetPos.x, 0.0f, myPos.z - targetPos.z };
	//	float dist = sqrt(toMe.x * toMe.x + toMe.z * toMe.z);
	//	if (dist > 0) { toMe.x /= dist; toMe.z /= dist; }

	//	// 타겟의 정면 벡터
	//	Vector3 tForward = Quaternion_Multiply(target->GetRotation(), Vector3_forward());

	//	// 내적 값이 0 이하면, 내가 타겟의 시야 반대편에 있음
	//	float dot = (tForward.x * toMe.x) + (tForward.z * toMe.z);

	//	if (dot >= 0.5f)
	//	{
	//		printf("[Skill] 뒤통수 판정 성공 \n");
	//		Vector3 dir = { targetPos.x - myPos.x, 0.0f, targetPos.z - myPos.z };
	//		float dist = sqrt(dir.x * dir.x + dir.z * dir.z);

	//		if (dist > 0.0f) { dir.x /= dist; dir.z /= dist; }

	//		if (isPush)
	//		{
	//			// N극-N극 밀어내기 (30의 힘으로 넉백)
	//			target->ApplyForce(dir, 30.0f, 0.5f);
	//			printf("[Physics] User %d Push User %d\n", clientIndex_, pReq->targetUUID);
	//		}
	//		else
	//		{
	//			// N극-S극 당겨오기 (딱 내 앞까지만 오도록 거리 계산)
	//			Vector3 pullDir = { -dir.x, 1.0f, -dir.z };
	//			// 내 위치 기준 1.5m 앞까지만 당김 (나랑 완벽히 겹치는 것 방지)
	//			float pullDist = (dist > 1.5f) ? (dist - 1.5f) : 0.0f;
	//			// 0.5초 동안 당김 속도 = 거리 / 시간
	//			float pullSpeed = pullDist / 0.5f;
	//			target->ApplyForce(pullDir, pullSpeed, 0.5f);

	//			printf("[Physics] User %d Pull User %d\n", clientIndex_, pReq->targetUUID);
	//		}
	//	}
	//}

	auto pReq = (PLAYER_ACTION_REQUEST_PACKET*)pPacket_;
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (!pUser) return;

	auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
	if (pRoom)
	{
		PLAYER_ACTION_NTF_PACKET ntfPkt;
		ntfPkt.attackerUUID = clientIndex_;   // 시전자
		ntfPkt.targetUUID = pReq->targetUUID; // 피격자
		ntfPkt.actionType = pReq->actionType;

		pRoom->BroadcastPacket(ntfPkt.PacketLength, (char*)&ntfPkt);

		printf("[Action] %lld used skill(type:%d) on %lld\n", ntfPkt.attackerUUID, ntfPkt.actionType, ntfPkt.targetUUID);
	}
}

void PacketManager::ProcessGimmickInteract(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pReq = (PLAYER_GIMMICK_INTERACT_REQUEST_PACKET*)pPacket_;
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (!pUser) return;

	auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());

	if (pRoom)
	{
		if (pReq->gimmickKey == eGimmickKey::NextZone)
		{
			pUser->SetPosition(pReq->targetPos);

			PLAYER_GIMMICK_INTERACT_NTF_PACKET ntfPkt;
			ntfPkt.activeUUID = pReq->activeUUID;
			ntfPkt.gimmickID = pReq->gimmickID;
			ntfPkt.gimmickKey = pReq->gimmickKey;

			ntfPkt.state = 2;
			ntfPkt.targetPos = pReq->targetPos;

			ntfPkt.param = pReq->param;

			pRoom->BroadcastPacket(ntfPkt.PacketLength, (char*)&ntfPkt);

			printf("[Teleport] User %lld Moved via Portal to %.2f, %.2f, %.2f\n",
				pReq->activeUUID, pReq->targetPos.x, pReq->targetPos.y, pReq->targetPos.z);

			return;
		}

		pRoom->ProcessGimmickInteract(pUser, pReq);
	}
}

void PacketManager::ProcessPlayerDead(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pReq = (PLAYER_DEAD_REQ_PACKET*)pPacket_;
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (!pUser) return;

	auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
	if (pRoom)
	{
		pUser->SetPosition(pReq->respawnPos); // 서버 좌표 시체 위치 -> 부활 위치 갱신
		Vector3 p;
		p.x = 0; p.y = 0; p.z = 0;
		PLAYER_STATUS_NTF_PACKET statusPkt;
		statusPkt.userUUID = clientIndex_;
		statusPkt.newState = eState::Teleport;
		statusPkt.targetDir = pReq->respawnPos; // 부활 좌표를 담음
		statusPkt.powerOrTime = 0.0f;
		statusPkt.isPull = 0;
		statusPkt.casterPos = p;

		pRoom->BroadcastPacket(statusPkt.PacketLength, (char*)&statusPkt);

		printf("[Respawn] User %d Respawned at: %.2f, %.2f, %.2f\n",
			clientIndex_, pReq->respawnPos.x, pReq->respawnPos.y, pReq->respawnPos.z);
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

//UDP client 접속 받는 곳
void PacketManager::UDPRecvThread()
{
	sockaddr_in clientAddr;
	int addrLen = sizeof(clientAddr);
	char buf[2048];
	printf("[System] UDP Recv Thread Started on Port 5025\n");
	while (mIsRunLogicThread)
	{
		int recvLen = recvfrom(mUdpSocket, buf, 2048, 0, (sockaddr*)&clientAddr, &addrLen);

		if (recvLen > 0)
		{
			auto pHeader = (PACKET_HEADER*)buf;
			m_TotalRecvBytes += recvLen;
			m_GrandTotalSendBytes += m_TotalRecvBytes;

			//printf("[UDP] Packet Recv! ID:%d, Len:%d\n", pHeader->PacketId, recvLen);

			if (pHeader->PacketId == (UINT16)PACKET_ID::PLAYER_MOVEMENT)
			{
				// 이동 패킷은 지연 없이 UDP 스레드에서 직접 처리
				auto pMovePkt = (PLAYER_MOVEMENT_PACKET*)buf;
				//printf("[UDP] Move Packet -> UserUUID: %lld\n", pMovePkt->userUUID);
				if (pMovePkt->userUUID < 0 || pMovePkt->userUUID >= mUserManager->GetMaxUserCnt())
				{
					continue;
				}

				auto pUser = mUserManager->GetUserByConnIdx(pMovePkt->userUUID);
				if (pUser)
				{
					//printf("[UDP] User Found! Setting Input...\n");
					pUser->SetPosition(pMovePkt->currentPos);
					pUser->SetRotation(pMovePkt->currentRot);
					pUser->SetDirty(true);
					pUser->SetTarget(pMovePkt->currentPos, pMovePkt->currentRot, pMovePkt->axisH, pMovePkt->axisV, pMovePkt->inputSeq);

					if (!pUser->isUdpActive)
					{
						pUser->SetUDPAddr(clientAddr);
						pUser->isUdpActive = true;
					}
				}
				else
				{
					printf("[UDP Error] User Not Found! UUID: %lld / MaxUser: %d\n",
						pMovePkt->userUUID, mUserManager->GetMaxUserCnt());
				}
			}
			else
			{
				// 이동 패킷 외 모든 UDP 패킷은 SystemPacketQueue에 넣어서 ProcessPacket에서 처리
				UINT32 clientIndex = 0;

				// 패킷 종류별로 clientIndex 추출
				switch ((PACKET_ID)pHeader->PacketId)
				{
					case PACKET_ID::PLAYER_GIMMICK_INTERACT_REQUEST:
					{
						auto pPkt = (PLAYER_GIMMICK_INTERACT_REQUEST_PACKET*)buf;
						clientIndex = (UINT32)pPkt->activeUUID;
						break;
					}
					case PACKET_ID::PLAYER_STATUS_NTF:
					{
						auto pPkt = (PLAYER_STATUS_NTF_PACKET*)buf;
						clientIndex = (UINT32)pPkt->userUUID;
						break;
					}
					case PACKET_ID::MONSTER_MOVEMENT:
					{
						auto pPkt = (MONSTER_MOVEMENT_PACKET*)buf;
						clientIndex = (UINT32)pPkt->userUUID;
						break;
					}
					// UDP로 추가되는 패킷은 여기에 case 추가
					
					default:
						printf("[UDP] Unhandled Packet ID: %d, dropping.\n", pHeader->PacketId);
						continue;
				}

				PacketInfo pktInfo;
				pktInfo.ClientIndex = clientIndex;
				pktInfo.PacketId = pHeader->PacketId;
				pktInfo.DataSize = recvLen;
				pktInfo.pDataPtr = new char[recvLen];
				memcpy(pktInfo.pDataPtr, buf, recvLen);

				PushSystemPacket(pktInfo);
			}
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

void PacketManager::ProcessMonsterDeadRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pReq = (MONSTER_DEAD_REQ_PACKET*)pPacket_;
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (!pUser) return;

	auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
	if (pRoom)
	{
		MONSTER_DEAD_NTF_PACKET pNtf;
		pNtf.userUUID = pReq->userUUID;
		pNtf.monsterID = pReq->monsterID;

		pRoom->BroadcastPacket(pNtf.PacketLength, (char*)&pNtf);
		printf("[State Sync] Monster %d Dead\n", clientIndex_, pReq->monsterID);
	}
}

void PacketManager::ProcessMonsterStateChange(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pReq = (MONSTER_STATE_PACKET*)pPacket_;
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	
	if (!pUser) return;

	auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
	if (pRoom)
	{
		pRoom->BroadcastPacket(pReq->PacketLength, pPacket_);
		printf("[State Sync] Monster %d changed state to %d\n", clientIndex_, pReq->newState);
	}
}

void PacketManager::ProcessMonsterMovement(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pReq = (MONSTER_MOVEMENT_PACKET*)pPacket_;
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (!pUser) return;

	auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
	if (pRoom)
	{
		pRoom->BroadcastPacket(pReq->PacketLength, pPacket_);
	}
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