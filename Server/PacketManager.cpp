#include <utility>
#include <cstring>
#include <sstream>
#include <chrono>

#include "UserManager.h"
#include "RoomManager.h"
#include "PacketManager.h"
#include "RedisManager.h"
#include "LogManager.h"

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
	mRecvFuntionDictionary[(int)PACKET_ID::PLAYER_MOVEMENT] = &PacketManager::ProcessPlayerMovement;
	mRecvFuntionDictionary[(int)PACKET_ID::PLAYER_ACTION_REQUEST] = &PacketManager::ProcessPlayerAction;
	
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
}

void PacketManager::CreateCompent(const UINT32 maxClient_)
{
	mUserManager = new UserManager;
	mUserManager->Init(maxClient_);

	LogManager::Init();
		
	UINT32 startRoomNummber = 0;
	UINT32 maxRoomCount = 10;
	UINT32 maxRoomUserCount = 4;
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
	const char* redisIp = std::getenv("REDIS_IP");
	while (true)
	{
		if (mRedisMgr->Run(redisIp ? redisIp : "host.docker.internal", 6379, 2))
		{
			printf("[SUCCESS] Redis Connected!\n");
			break;
		}

		retryCount++;
		printf("[RETRY %d] Redis connection failed. Retrying in 1s...\n", retryCount);

		if (retryCount > 10)
		{
			printf("[FATAL] Redis connection failed after 10 attempts.\n");
			return false;
		}

		Sleep(1000); // 1초 대기 후 다시 시도
	}

	if (UDPRun() == false) return false;

	//상점 업데이트용
	int cmdValue = -1;
	RedisTask task;
	task.TaskID = RedisTaskID::REQUEST_SHOP_UPDATE;
	task.DataSize = sizeof(int);
	task.pData = new char[sizeof(int)];
	memcpy(task.pData, &cmdValue, sizeof(int));
	mRedisMgr->PushTask(task);


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

	if (pReqUser->GetDomainState() == User::DOMAIN_STATE::ROOM)
	{
		auto roomNum = pReqUser->GetCurrentRoom();
		mRoomManager->LeaveUser(roomNum, pReqUser);
	}

	if (pReqUser->GetDomainState() != User::DOMAIN_STATE::NONE)
	{
		mUserManager->DeleteUserInfo(pReqUser);
	}
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
		}

		if (auto task = mRedisMgr->TakeResponseTask(); task.TaskID != RedisTaskID::INVALID)
		{
			isIdle = false;
			ProcessRecvPacket(task.UserIndex, (UINT16)task.TaskID, task.DataSize, task.pData);
			task.Release();
		}

		auto now = std::chrono::steady_clock::now();
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
		}

		if(isIdle)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

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

void PacketManager::ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{ 
	if (LOGIN_REQUEST_PACKET_SIZE != packetSize_)
	{
		return;
	}

	auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPacket_);

	auto pUserID = pLoginReqPacket->userID;
	printf("requested user id = %s\n", pUserID);

	LOGIN_RESPONSE_PACKET loginResPacket;

	if (mUserManager->GetCurrentUserCnt() >= mUserManager->GetMaxUserCnt()) 
	{ 
		//접속자수가 최대수를 차지해서 접속불가
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET) , (char*)&loginResPacket);
		return;
	}

	//여기에서 이미 접속된 유저인지 확인하고, 접속된 유저라면 실패한다.
	if (mUserManager->FindUserIndexByID(pUserID) == -1) 
	{ 
		RedisLoginReq dbReq;
		CopyUserID(dbReq.UserID, pLoginReqPacket->userID);
		CopyMemory(dbReq.UserPW, pLoginReqPacket->userPW, (MAX_USER_PW_LEN + 1));

		RedisTask task;
		task.UserIndex = clientIndex_;
		task.TaskID = RedisTaskID::REQUEST_LOGIN;
		task.DataSize = sizeof(RedisLoginReq);
		task.pData = new char[task.DataSize];
		CopyMemory(task.pData, (char*)&dbReq, task.DataSize);
		mRedisMgr->PushTask(task);

		printf("Login To Redis user id = %s\n", pUserID);
	}
	else 
	{
		//접속중인 유저여서 실패를 반환한다.
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_ALREADY;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
		return;
	}
}

void PacketManager::ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	printf("ProcessLoginDBResult. UserIndex: %d\n", clientIndex_);

	auto pBody = (RedisLoginRes*)pPacket_;

	if (pBody->Result == (UINT16)ERROR_CODE::NONE)
	{
		//로그인 완료로 변경한다
		auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
		pUser->SetLogin(pBody->UserID);
	}

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
		spdlog::info("[Enter] User({}) Entered Room Number [{}]", clientIndex_, roomNumber);
		return;
	}
	else
	{
		spdlog::warn("[Enter] User({}) Failed. Error: {}", clientIndex_, enterResult);
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
	shopPkt.currentItemID = mCurrentShopItemID;
	shopPkt.nextUpdateTime = mNextShopUpdateTime;

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
		// 서버의 Actor 객체에 목적지 좌표만 설정함
		// 실제 이동은 LogicThread -> Room::Update -> Actor::UpdateServerPhysics에서 처리
		pUser->SetInput(pMovePkt->dx, pMovePkt->dz, pMovePkt->inputSeq);
	}
}

void PacketManager::ProcessPlayerAction(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pReq = (PLAYER_ACTION_REQUEST_PACKET*)pPacket_;
	auto reqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (!reqUser) return;

	auto pRoom = mRoomManager->GetRoomByNumber(reqUser->GetCurrentRoom());
	if (!pRoom) return;

	Actor* target = pRoom->GetActorByUUID(pReq->targetUUID);
	if (target)
	{
		bool isPush = (pReq->actionType == ACTION_TYPE::PUSH);

		Vector3 myPos = reqUser->GetPosition();
		Vector3 targetPos = target->GetPosition();

		// 타겟 위치에서 내 위치를 바라보는 방향 벡터
		Vector3 toMe = { myPos.x - targetPos.x, 0.0f, myPos.z - targetPos.z };
		float dist = sqrt(toMe.x * toMe.x + toMe.z * toMe.z);
		if (dist > 0) { toMe.x /= dist; toMe.z /= dist; }

		// 타겟의 정면 벡터
		Vector3 tForward = Quaternion_Multiply(target->GetRotation(), Vector3_forward());

		// 내적 값이 0 이하면, 내가 타겟의 시야 반대편에 있음
		float dot = (tForward.x * toMe.x) + (tForward.z * toMe.z);

		if (dot >= 0.5f)
		{
			printf("[Skill] 뒤통수 판정 성공 \n");
			Vector3 dir = { targetPos.x - myPos.x, 0.0f, targetPos.z - myPos.z };
			float dist = sqrt(dir.x * dir.x + dir.z * dir.z);

			if (dist > 0.0f) { dir.x /= dist; dir.z /= dist; }

			if (isPush)
			{
				// N극-N극 밀어내기 (30의 힘으로 넉백)
				target->ApplyForce(dir, 30.0f, 0.5f);
				printf("[Physics] User %d Push User %d\n", clientIndex_, pReq->targetUUID);
			}
			else
			{
				// N극-S극 당겨오기 (딱 내 앞까지만 오도록 거리 계산)
				Vector3 pullDir = { -dir.x, 1.0f, -dir.z };
				// 내 위치 기준 1.5m 앞까지만 당김 (나랑 완벽히 겹치는 것 방지)
				float pullDist = (dist > 1.5f) ? (dist - 1.5f) : 0.0f;
				// 0.5초 동안 당김 속도 = 거리 / 시간
				float pullSpeed = pullDist / 0.5f;
				target->ApplyForce(pullDir, pullSpeed, 0.5f);

				printf("[Physics] User %d Pull User %d\n", clientIndex_, pReq->targetUUID);
			}
		}
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
	if (cmdMessage.find("/spawn cube") == 0)
	{
		float cx = 5.0f, cz = 5.0f;
		sscanf_s(cmdMessage.c_str(), "/spawn cube %f %f", &cx, &cz);
		pRoom->EnterCube(cx, cz);
		return;
	}
	
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

void PacketManager::ProcessTradeRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	auto pReq = (TRADE_REQUEST_PACKET*)pPacket_;
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);

	TRADE_REQUEST_NTF_PACKET p;
	p.reqUUID = clientIndex_;
	strncpy_s(p.reqName, pUser->GetUserId().c_str(), MAX_USER_ID_LEN);

	SendPacketFunc(pReq->targetUUID, sizeof(p), (char*)&p);
	SendPacketFunc(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessTradeResponse(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
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

void PacketManager::ProcessTradeItemUpdate(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
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

void PacketManager::ProcessTradeLock(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
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

void PacketManager::ProcessTradeConfirm(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
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

void PacketManager::ProcessTradeDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
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

void PacketManager::ProcessShopUpdateDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
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

void PacketManager::ProcessShopBuyRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
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

void PacketManager::ProcessShopBuyDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
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
					NavMeshManager::GetInstance()->UpdateTileCache(FIXED_DELTA_TIME);
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

		if (recvLen > 0) {
			auto pHeader = (PACKET_HEADER*)buf;
			m_TotalRecvBytes += recvLen;
			m_GrandTotalSendBytes += m_TotalRecvBytes;

			printf("[UDP] Packet Recv! ID:%d, Len:%d\n", pHeader->PacketId, recvLen);
			if (pHeader->PacketId == (UINT16)PACKET_ID::PLAYER_MOVEMENT) 
			{
				auto pMovePkt = (PLAYER_MOVEMENT_PACKET*)buf;
				printf("[UDP] Move Packet -> UserUUID: %lld\n", pMovePkt->userUUID);
				if (pMovePkt->userUUID < 0 || pMovePkt->userUUID >= mUserManager->GetMaxUserCnt())
				{
					continue; 
				}

				/*spdlog::info("[RUDP] User : {} | Seq : {} Pos : {:.2f}, {:.2f}",
					pMovePkt->userUUID, pMovePkt->inputSeq, pMovePkt->dx, pMovePkt->dz);*/

				// 패킷 내의 userUUID나 clientAddr를 통해 유저 식별
				auto pUser = mUserManager->GetUserByConnIdx(pMovePkt->userUUID);
				if (pUser) 
				{
					printf("[UDP] User Found! Setting Input...\n");
					// 유저의 UDP 주소가 처음 왔다면 등록 (응답 전송용)
					if (!pUser->isUdpActive)
					{
						pUser->SetUDPAddr(clientAddr);
						pUser->isUdpActive = true;
						printf("[UDP] New User(%lld) UDP Address Registered!\n", pMovePkt->userUUID);
					}

					// 서버측 Actor에 목적지 좌표 설정
					//pUser->SetTarget(pMovePkt->targetPos, pMovePkt->inputSeq);	미니맵 이동
					pUser->SetInput(pMovePkt->dx, pMovePkt->dz, pMovePkt->inputSeq);
				}
				else
				{
					// 유저를 못 찾음
					printf("[UDP Error] User Not Found! UUID: %lld / MaxUser: %d\n",
						pMovePkt->userUUID, mUserManager->GetMaxUserCnt());
				}
			}
		}
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