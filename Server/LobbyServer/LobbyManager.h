#pragma once
#pragma once
#include "UserModels\User.h"
#include "UserModels\UserManager.h"
#include "Packet\Packet.h"
#include "Packet\ErrorCode.h"
#include "Database\RedisManager.h"

#include <mutex>
#include <functional>
#include <strsafe.h>
#include <windows.h>

class RoomManager;

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

	void Init(RedisManager* redisMgr, UserManager* userMgr, RoomManager* roomMgr, std::function<void(UINT32, UINT32, char*)> sendFunc)
	{
		mRedisMgr = redisMgr;
		mUserManager = userMgr;
		mRoomManager = roomMgr;
		SendPacketFunc = sendFunc;

		mRedisMgr->OnRoomDeleteCallback = [this](INT32 roomNum) {
			if (mRoomManager != nullptr)
			{
				mRoomManager->ClearRoom(roomNum);
			}
			};

		std::random_device rd;
		mRandomEngine = std::mt19937(rd());
	}

	void ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPacket_);

		if (mUserManager->GetCurrentUserCnt() >= mUserManager->GetMaxUserCnt())
		{
			LOGIN_RESPONSE_PACKET res;
			res.Result = (UINT16)ERROR_CODE::LOGIN_USER_USED_ALL_OBJ;
			SendPacketFunc(clientIndex_, sizeof(res), (char*)&res);
			return;
		}

		auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
		if (pUser) pUser->SetLogin(pLoginReqPacket->userID);

		LOGIN_RESPONSE_PACKET loginResPacket;
		loginResPacket.Result = (UINT16)ERROR_CODE::NONE;
		loginResPacket.userUUID = clientIndex_;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
	}

	void ProcessEnterRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		printf("[ProcessEnterRoom] START clientIndex: %d\n", clientIndex_);

		auto pReq = reinterpret_cast<ROOM_ENTER_REQUEST_PACKET*>(pPacket_);

		printf("[ProcessEnterRoom] RoomNumber: %d\n", pReq->RoomNumber);

		auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);

		printf("[ProcessEnterRoom] pReqUser: %s\n", pReqUser ? "OK" : "NULL");

		if (!pReqUser) return;

		INT32 currentRoom = pReqUser->GetCurrentRoom();
		if (currentRoom != -1) mRoomManager->LeaveUser(currentRoom, pReqUser);

		auto enterResult = mRoomManager->EnterUser(pReq->RoomNumber, pReqUser);

		if (enterResult == (UINT16)ERROR_CODE::NONE && pReq->RoomNumber == -1)
		{
			auto pRoom = mRoomManager->GetRoomByNumber(pReqUser->GetCurrentRoom());
			if (pRoom)
			{
				pRoom->SetTitle(pReq->title); // 클라이언트가 보낸 제목 저장
			}
		}

		ROOM_ENTER_RESPONSE_PACKET res;
		res.Result = enterResult;
		SendPacketFunc(clientIndex_, sizeof(res), (char*)&res);

		BroadcastRoomList();
	}

	void ProcessLeaveRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);
		if (!pReqUser) return;

		ROOM_LEAVE_RESPONSE_PACKET res;
		res.Result = mRoomManager->LeaveUser(pReqUser->GetCurrentRoom(), pReqUser);
		SendPacketFunc(clientIndex_, sizeof(res), (char*)&res);

		BroadcastRoomList();
	}

	void ProcessPlayerReady(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto* pReq = reinterpret_cast<PLAYER_READY_REQUEST_PACKET*>(pPacket_);
		auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
		if (!pUser) return;

		auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
		if (pRoom)
		{
			pRoom->ProcessPlayerReady(pUser, pReq->isReady);

			//이제 호스트가 시작
			/*if (pRoom->IsAllReady())
			{
				std::vector<User*> roomUsers = pRoom->GetRoomUserList();
				GenerateGameSession(pRoom->GetRoomNumber(), roomUsers);
			}*/
		}
	}

	void ProcessCharacterSelect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pReq = (ROOM_CHAR_SELECT_REQ_PACKET*)pPacket_;
		auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
		if (!pUser) return;

		auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
		if (pRoom)
		{
			pRoom->SetUserCharID(pUser, pReq->charID);

			if (pRoom->GetHostUUID() == clientIndex_)
			{
				INT32 guestForceCharID = (pReq->charID == 0) ? 1 : 0; // 방장과 반대 캐릭터
				for (int i = 0; i < pRoom->GetMaxUserCount(); ++i)
				{
					User* pSlotUser = pRoom->GetUserBySlot(i);
					if (pSlotUser != nullptr && pSlotUser->GetNetConnIdx() != clientIndex_)
					{
						pRoom->SetUserCharID(pSlotUser, guestForceCharID);
					}
				}
			}

			ROOM_CHAR_SELECT_NTF_PACKET ntf;
			ntf.userUUID = clientIndex_;
			ntf.charID = pReq->charID;
			pRoom->BroadcastPacket(ntf.PacketLength, (char*)&ntf);

			printf("[Lobby] %d User Change Character : %d \n", clientIndex_, pReq->charID);
		}
	}

	void BroadcastRoomList()
	{
		ROOM_LIST_RES_PACKET res;
		memset(res.rooms, 0, sizeof(res.rooms));
		res.roomCount = 0;

		int maxRoomCount = mRoomManager->GetMaxRoomCount();
		for (int i = 0; i < maxRoomCount; ++i)
		{
			auto pRoom = mRoomManager->GetRoomByNumber(i);
			if (pRoom && pRoom->GetCurrentUserCount() > 0)
			{
				res.rooms[res.roomCount].roomNum = pRoom->GetRoomNumber();
				strncpy_s(res.rooms[res.roomCount].title, 32, pRoom->GetTitle(), _TRUNCATE);
				res.rooms[res.roomCount].curUser = pRoom->GetCurrentUserCount();
				res.rooms[res.roomCount].maxUser = pRoom->GetMaxUserCount();
				res.rooms[res.roomCount].isPlaying = pRoom->GetIsPlaying();

				auto pHost = pRoom->GetUserBySlot(0);
				res.rooms[res.roomCount].hostPing = pHost ? pHost->GetPing() : 0;

				if (pRoom->GetCurrentUserCount() == 1)
					res.rooms[res.roomCount].guestReadyState = 0;
				else if (pRoom->GetCurrentUserCount() == 2)
					res.rooms[res.roomCount].guestReadyState = pRoom->IsSlotReady(1) ? 2 : 1;

				res.roomCount++;
			}
		}

		// 로비 상태(LOGIN)인 모든 유저에게 전송
		int maxUsers = mUserManager->GetMaxUserCnt();
		for (int i = 0; i < maxUsers; ++i)
		{
			auto pUser = mUserManager->GetUserByConnIdx(i);
			if (pUser && pUser->GetDomainState() == User::DOMAIN_STATE::LOGIN)
			{
				SendPacketFunc(pUser->GetNetConnIdx(), sizeof(res), (char*)&res);
			}
		}
	}

	void ProcessGameStartRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pReq = reinterpret_cast<GAME_START_REQ_PACKET*>(pPacket_);
		auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
		if (!pUser) return;

		auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
		if (!pRoom) return;

		// 요청 유저가 호스트인지 체크
		auto pHost = pRoom->GetUserBySlot(0);
		if (pHost == nullptr || pHost->GetNetConnIdx() != clientIndex_) return;

		if (pRoom->IsAllReady())
		{
			pRoom->SetIsPlaying(true);

			// 게임 세션 할당 및 토큰 발급 진행
			std::vector<User*> roomUsers = pRoom->GetRoomUserList();
			GenerateGameSession(pRoom->GetRoomNumber(), roomUsers);
		}
	}

	void GenerateGameSession(INT32 roomNum, const std::vector<User*>& roomUsers)
	{
		printf("[LobbyManager] 방 %d번 전원 준비 완료 게임 세션 할당 시작.\n", roomNum);

		// 방마다 포트 동적 할당 (11021, 11022, ...)
		UINT16 allocatedPort = mNextGamePort++;
		LaunchGameServer(allocatedPort, roomNum);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		std::uniform_int_distribution<int> dis(1000, 9999);

		Room* pRoom = mRoomManager->GetRoomByNumber(roomNum);
		if (pRoom) pRoom->SetIsPlaying(true);

		for (auto* user : roomUsers)
		{
			if (!user) continue;
			user->SetDomainState(User::DOMAIN_STATE::GAME);
			// 고유 토큰 생성 (TOKEN_유저인덱스_난수)
			std::string tokenStr = "TOKEN_" + std::to_string(user->GetNetConnIdx()) + "_" + std::to_string(dis(mRandomEngine));

			// Redis에 토큰 저장
			RedisAuthTokenReq dbReq;
			dbReq.UserIndex = user->GetNetConnIdx();
			StringCbCopyA(dbReq.Token, sizeof(dbReq.Token), tokenStr.c_str());

			RedisTask task;
			task.UserIndex = user->GetNetConnIdx();
			task.TaskID = RedisTaskID::REQUEST_SET_AUTH_TOKEN;
			task.DataSize = sizeof(RedisAuthTokenReq);
			task.pData = new char[task.DataSize];
			CopyMemory(task.pData, (char*)&dbReq, task.DataSize);
			mRedisMgr->PushTask(task);

			printf("[Auth] Redis 토큰 발급 요청 큐 삽입 : %s\n", tokenStr.c_str());

			// 클라이언트에게 접속할 포트와 인증 토큰 발송
			MATCH_START_NTF_PACKET ntf;
			ntf.gameServerPort = allocatedPort;		//TCP
			ntf.gameServerUdpPort = allocatedPort;	//UDP
			StringCbCopyA(ntf.authToken, sizeof(ntf.authToken), tokenStr.c_str());

			SendPacketFunc(user->GetNetConnIdx(), sizeof(ntf), (char*)&ntf);
		}
	}

	void ProcessRoomChatMessage(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		auto pReq = reinterpret_cast<ROOM_CHAT_REQUEST_PACKET*>(pPacket_);
		auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
		if (!pUser) return;

		auto pRoom = mRoomManager->GetRoomByNumber(pUser->GetCurrentRoom());
		if (pRoom)
		{
			pRoom->NotifyChat(clientIndex_, pUser->GetUserId().c_str(), pReq->Message);
		}
	}

	void ProcessRoomListRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		ROOM_LIST_RES_PACKET res;
		memset(res.rooms, 0, sizeof(res.rooms));
		res.roomCount = 0;

		int maxRoomCount = mRoomManager->GetMaxRoomCount();
		for (int i = 0; i < maxRoomCount; ++i)
		{
			auto pRoom = mRoomManager->GetRoomByNumber(i);

			if (pRoom && pRoom->GetCurrentUserCount() > 0)
			{
				res.rooms[res.roomCount].roomNum = pRoom->GetRoomNumber();
				strncpy_s(res.rooms[res.roomCount].title, 32, pRoom->GetTitle(), _TRUNCATE);
				res.rooms[res.roomCount].curUser = pRoom->GetCurrentUserCount();
				res.rooms[res.roomCount].maxUser = pRoom->GetMaxUserCount();
				res.rooms[res.roomCount].isPlaying = pRoom->GetIsPlaying();

				// 호스트의 핑 가져오기
				auto pHost = pRoom->GetUserBySlot(0);
				res.rooms[res.roomCount].hostPing = pHost ? pHost->GetPing() : 0;

				INT32 hostCharID = 0;
				INT32 guestCharID = 1; // 기본값 디폴트 세팅
				INT64 currentHostUUID = pRoom->GetHostUUID();

				for (int slot = 0; slot < pRoom->GetMaxUserCount(); ++slot)
				{
					auto pUser = pRoom->GetUserBySlot(slot);
					if (pUser != nullptr)
					{
						if (pUser->GetNetConnIdx() == currentHostUUID)
						{
							hostCharID = pRoom->GetCharacterIDBySlot(slot);
						}
						else
						{
							guestCharID = pRoom->GetCharacterIDBySlot(slot);
						}
					}
				}

				// 확실하게 가공된 동적 데이터 주입
				res.rooms[res.roomCount].hostCharID = hostCharID;
				res.rooms[res.roomCount].guestCharID = guestCharID;

				// 게스트 상태 및 준비 여부 세팅 (0=빈방, 1=준비중, 2=준비완료)
				if (pRoom->GetCurrentUserCount() == 1)
				{
					res.rooms[res.roomCount].guestReadyState = 0;
				}
				else if (pRoom->GetCurrentUserCount() == 2)
				{
					bool isReady = pRoom->IsSlotReady(1);
					res.rooms[res.roomCount].guestReadyState = isReady ? 2 : 1;
				}

				res.roomCount++;
				if (res.roomCount >= 20) break;
			}
		}

		SendPacketFunc(clientIndex_, sizeof(res), (char*)&res);
	}

	void ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		// Redis에서 로그인/인증 처리가 끝나고 돌아온 응답
		auto pBody = reinterpret_cast<RedisLoginRes*>(pPacket_);

		LOGIN_RESPONSE_PACKET res;
		res.Result = pBody->Result;
		SendPacketFunc(clientIndex_, sizeof(res), (char*)&res);
	}

	void ProcessNoticeDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
	{
		// 공지사항 등 DB 요청 후 콜백 처리 (현 단계에선 로깅만)
		printf("[LobbyManager] 공지사항 DB 처리 완료. Client: %d\n", clientIndex_);
	}

#pragma region GameServer Launcher
	void LaunchGameServer(UINT16 port, INT32 roomNum)
	{
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		std::string cmdLine = "GameServer.exe " + std::to_string(port) + " " + std::to_string(roomNum);

		char cmdArgs[256];
		strncpy_s(cmdArgs, cmdLine.c_str(), sizeof(cmdArgs));

		if (!CreateProcessA(
			NULL,					// 실행 파일 경로 (NULL이면 cmdArgs 첫 번째 단어 사용)
			cmdArgs,				// 커맨드 라인 인자
			NULL, NULL, FALSE,
			CREATE_NEW_CONSOLE,		// 새 콘솔 창을 열어서 실행
			NULL, NULL, &si, &pi))
		{
			printf("[LobbyManager] 게임 서버 구동 실패 포트: %d, 에러코드: %lu\n", port, GetLastError());
		}
		else
		{
			printf("[LobbyManager] 게임 서버 구동 성공 포트: %d (PID: %lu)\n", port, pi.dwProcessId);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}

#pragma endregion

	//거래 & 상점 처리 안써서 비활성화함
#if 0
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
#endif
	//Shop
	int mCurrentShopItemID = 101;
	INT64 mNextShopUpdateTime = 0;


private:
	std::mt19937 mRandomEngine;

	UINT16 mNextGamePort = 11021; // 방마다 동적으로 포트 할당

	RedisManager* mRedisMgr = nullptr;
	UserManager* mUserManager = nullptr;
	RoomManager* mRoomManager = nullptr;
	std::function<void(UINT32, UINT32, char*)> SendPacketFunc;
};