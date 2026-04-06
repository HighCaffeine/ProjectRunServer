#pragma once

#include "Npc.h"
#include "UserManager.h"
#include "Packet.h"

#include <functional>

void CopyUserID(char* userID, const Actor& user);
void CopyUserID(char* userID, const std::string& userID_);
void CopyUserID(char* userID, const char* userID_);

class Room
{
	// 들어올 때는 6m, 나갈 때는 7.5m (1.5m의 여유 버퍼)
	//const float ENTER_RANGE = 6.0f;
	//const float LEAVE_RANGE = 7.5f;
	const float ENTER_RANGE = 99999.0f;
	const float LEAVE_RANGE = 99999.0f;
public:
	Room() = default;
	~Room() = default;

	INT32 GetMaxUserCount() { return mMaxUserCount; }
	INT32 GetCurrentUserCount() { return mCurrentUserCount; }
	INT32 GetRoomNumber() { return mRoomNum; }

	void Init(const INT32 roomNum_, const INT32 maxUserCount_, const std::string& navMeshFileName)
	{
		mRoomNum = roomNum_;
		mMaxUserCount = maxUserCount_;
		mSlots.assign(maxUserCount_, nullptr);
		mIsReady.assign(maxUserCount_, false);
		mIsEscaped.assign(maxUserCount_, false);
	}

	UINT16 EnterUser(User* user_)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		if (mCurrentUserCount >= mMaxUserCount)
		{
			return (UINT16)ERROR_CODE::ENTER_ROOM_FULL_USER;
		}

		mUserList.push_back(user_);
		++mCurrentUserCount;

		//빈 자리중 가장 앞자리에 배치
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] == nullptr)
			{
				mSlots[i] = user_;
				// user_->SetRoomSlotIndex(i); 유저 객체에 번호 저장 가능
				break;
			}
		}

		// 방에 처음 들어온 사람이면 방장으로 임명
		if (mCurrentUserCount == 1)
		{
			mHostUUID = user_->GetNetConnIdx();
		}

		user_->EnterRoom(mRoomNum);

		Vector3 snappedPos;
		if (NavMeshManager::GetInstance()->GetValidMovePosition(user_->GetPosition(), user_->GetPosition(), snappedPos))
		{
			user_->SetPosition(snappedPos);
		}

		// [입장 처리]
		// 기존 유저들에 대해 CanSee(6.0m) 체크 후 시야 등록
		for (auto pRoomUser : mUserList)
		{
			if (pRoomUser == nullptr || pRoomUser == user_) continue;

			// 6.0m 이내에 있는가?
			if (CanSee(user_, pRoomUser) == false) continue;

			// 서로 시야 목록에 추가
			user_->mVisibleList.insert(pRoomUser->GetNetConnIdx());
			pRoomUser->mVisibleList.insert(user_->GetNetConnIdx());

			// 나에게 상대방 정보 보내기 (Create)
			ROOM_USER_INFO_NTF_PACKET roomUserInfoNtf;
			roomUserInfoNtf.userUUID = pRoomUser->GetNetConnIdx();
			CopyUserID(roomUserInfoNtf.userID, *pRoomUser);
			roomUserInfoNtf.position = pRoomUser->GetPosition();
			roomUserInfoNtf.rotation = pRoomUser->GetRotation();
			SendPacketFunc(user_->GetNetConnIdx(), roomUserInfoNtf.PacketLength, (char*)&roomUserInfoNtf);
		}

		// NPC 처리
		for (auto pRoomNpc : mNpcList)
		{
			if (pRoomNpc == nullptr) continue;

			if (CanSee(user_, pRoomNpc) == false) continue;

			ROOM_USER_INFO_NTF_PACKET roomUserInfoNtf;
			roomUserInfoNtf.userUUID = pRoomNpc->GetNetConnIdx();
			CopyUserID(roomUserInfoNtf.userID, *pRoomNpc);
			roomUserInfoNtf.position = pRoomNpc->GetPosition();
			roomUserInfoNtf.rotation = pRoomNpc->GetRotation();
			SendPacketFunc(user_->GetNetConnIdx(), roomUserInfoNtf.PacketLength, (char*)&roomUserInfoNtf);
		}

		BroadcastHostInfo();

		return (UINT16)ERROR_CODE::NONE;
	}

	Npc* CreateNpc()
	{
		INT32 uuid = 10000 + mNpcList.size();
		auto npmID = std::to_string(uuid);
		Npc* npc = new Npc();

		npc->Init(uuid);
		npc->SetLogin(npmID.c_str());

		mNpcList.push_back(npc);

		return npc;
	}

	UINT16 EnterNpc()
	{
		Npc* newNpc = CreateNpc();
		newNpc->EnterRoom(mRoomNum);
		NotifyUserEnter(newNpc->GetNetConnIdx(), newNpc->GetUserId());
		return (UINT16)ERROR_CODE::NONE;
	}

	void LeaveUser(User* leaveUser_)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		mUserList.remove_if([leaveUserId = leaveUser_->GetUserId()](User* pUser) {
			return leaveUserId == pUser->GetUserId();
			});

		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] == leaveUser_)
			{
				mSlots[i] = nullptr;
				break;
			}
		}
		mUserList.remove(leaveUser_);

		--mCurrentUserCount;

		for (int i = 0; i < mMaxUserCount; ++i) 
		{
			if (mSlots[i] == leaveUser_) 
			{
				mIsReady[i] = false;
				break;
			}
		}
		CheckAllReady();

		ROOM_LEAVE_USER_NTF_PACKET notifyPkt;
		notifyPkt.userUUID = leaveUser_->GetNetConnIdx();
		CopyUserID(notifyPkt.userID, *leaveUser_);
		bool EXCEPT_ME = true;
		SendToAllUser(notifyPkt.PacketLength, (char*)&notifyPkt, notifyPkt.userUUID, EXCEPT_ME);

		//나간 사람이 방장이면 가장 앞번호에게 방장 넘김
		if (leaveUser_->GetNetConnIdx() == mHostUUID)
		{
			if (mCurrentUserCount <= 0) 
			{
				mHostUUID = -1;
				return; 
			}

			mHostUUID = -1; // 초기화
			for (int i = 0; i < mMaxUserCount; ++i)
			{
				if (mSlots[i] != nullptr)
				{
					mHostUUID = mSlots[i]->GetNetConnIdx();
					break;
				}
			}

			// 남은 유저가 있다면 새로운 방장 알림
			if (mHostUUID != -1)
			{
				BroadcastHostInfo();
			}
		}
	}

	void BroadcastHostInfo()
	{
		ROOM_HOST_NTF_PACKET hostPkt;
		hostPkt.hostUUID = mHostUUID;

		for (auto pTarget : mUserList)
		{
			if (pTarget != nullptr)
			{
				SendPacketFunc((UINT32)pTarget->GetNetConnIdx(), hostPkt.PacketLength, (char*)&hostPkt);
			}
		}
	}

	void NotifyChat(INT32 clientIndex_, const char* userID_, const char* msg_)
	{
		ROOM_CHAT_NOTIFY_PACKET roomChatNtfyPkt;
		CopyMemory(roomChatNtfyPkt.Msg, msg_, sizeof(roomChatNtfyPkt.Msg));
		CopyUserID(roomChatNtfyPkt.userID, userID_);
		SendToAllUser(sizeof(roomChatNtfyPkt), (char*)&roomChatNtfyPkt, clientIndex_, false);
	}

	void NotifyUserEnter(INT32 clientIndex_, const std::string& userID)
	{
		ROOM_NEW_USER_NTF_PACKET roomNewUserNtfPkt;
		roomNewUserNtfPkt.userUUID = clientIndex_;
		CopyUserID(roomNewUserNtfPkt.userID, userID);
		bool EXCEPT_ME = false;
		SendToAllUser(roomNewUserNtfPkt.PacketLength, (char*)&roomNewUserNtfPkt, clientIndex_, EXCEPT_ME);
	}

	std::function<void(UINT32, UINT32, char*)> SendPacketFunc;

	// 타겟팅용 UUID로 액터 찾기
	Actor* GetActorByUUID(INT32 uuid)
	{
		for (auto u : mUserList) { if (u->GetNetConnIdx() == uuid) return u; }
		for (auto n : mNpcList) { if (n->GetNetConnIdx() == uuid) return n; }
		return nullptr;
	}

	// 큐브 소환 함수
	//UINT16 EnterCube(float x, float z)
	//{
	//	std::lock_guard<std::recursive_mutex> guard(mLock);
	//	Npc* newNpc = CreateNpc();
	//	newNpc->EnterRoom(mRoomNum);

	//	// 큐브 속성 부여 및 좌표 설정
	//	newNpc->mIsCube = true;
	//	newNpc->SetPosition({ x, 0.0f, z });

	//	// TileCache에 동적 장애물 구역으로 등록
	//	newNpc->mObstacleRef = NavMeshManager::GetInstance()->AddObstacle(newNpc->GetPosition(), 1.0f, 2.0f);

	//	NotifyUserEnter(newNpc->GetNetConnIdx(), newNpc->GetUserId());
	//	return (UINT16)ERROR_CODE::NONE;
	//}

	void BroadcastPacket(UINT16 packetSize, char* pPacket)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		for (auto pTarget : mUserList)
		{
			if (pTarget != nullptr)
			{
				SendPacketFunc((UINT32)pTarget->GetNetConnIdx(), packetSize, pPacket);
			}
		}
	}

	void SendToAllUser(const UINT16 dataSize_, char* data_, const INT32 passUserIndex_, bool exceptMe)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		User* sender = nullptr;
		if (passUserIndex_ != -1)
		{
			for (auto u : mUserList) { if (u->GetNetConnIdx() == passUserIndex_) { sender = u; break; } }
		}

		for (auto pUser : mUserList)
		{
			if (pUser == nullptr) continue;
			if (exceptMe && pUser->GetNetConnIdx() == passUserIndex_) continue;
			
			if (sender != nullptr)
			{
				if (pUser != sender)
				{
					if (pUser->mVisibleList.find(sender->GetNetConnIdx()) == pUser->mVisibleList.end())
					{
						continue;
					}
				}
			}

			SendPacketFunc((UINT32)pUser->GetNetConnIdx(), (UINT32)dataSize_, data_);
		}
	}

	void Update(float dt)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		// 물리 연산
		for (auto pUser : mUserList)
		{
			if (pUser == nullptr) continue;
			pUser->UpdateServerPhysics(dt);
		}

		for (auto pNpc : mNpcList)
		{
			if (pNpc == nullptr) continue;
			pNpc->UpdateServerPhysics(dt);
		}

		// AOI 관리
		for (auto pViewer : mUserList) // pViewer: 나
		{
			if (pViewer == nullptr) continue;

			for (auto pTarget : mUserList) // pTarget: 상대방
			{
				if (pTarget == nullptr || pViewer == pTarget) continue;

				// 실제 거리 계산
				float dist = Vector3_Distance2D(pViewer->GetPosition(), pTarget->GetPosition());

				// 현재 시야 목록에 있는지 확인
				bool wasVisible = (pViewer->mVisibleList.find(pTarget->GetNetConnIdx()) != pViewer->mVisibleList.end());
				bool canSee = CanSee(pViewer, pTarget);

				// 안 보이다가 -> 6.0m 안으로 들어옴 (Enter)
				if (!wasVisible)
				{
					if (dist <= ENTER_RANGE && CanSee(pViewer, pTarget))
					{
						pViewer->mVisibleList.insert(pTarget->GetNetConnIdx());

						ROOM_USER_INFO_NTF_PACKET infoPkt;
						infoPkt.userUUID = pTarget->GetNetConnIdx();
						CopyUserID(infoPkt.userID, *pTarget);
						infoPkt.position = pTarget->GetPosition();
						infoPkt.rotation = pTarget->GetRotation();

						SendPacketFunc(pViewer->GetNetConnIdx(), infoPkt.PacketLength, (char*)&infoPkt);
					}
				}
				// 보이다가 -> 7.5m 밖으로 나감 (Leave) / 부쉬에 들어가서 안보임
				else
				{
					if (dist > LEAVE_RANGE || !canSee)
					{
						pViewer->mVisibleList.erase(pTarget->GetNetConnIdx());

						ROOM_LEAVE_USER_NTF_PACKET leavePkt;
						leavePkt.userUUID = pTarget->GetNetConnIdx();
						CopyUserID(leavePkt.userID, *pTarget);

						SendPacketFunc(pViewer->GetNetConnIdx(), leavePkt.PacketLength, (char*)&leavePkt);
					}
				}
			}
		}

		// 이동 동기화 패킷 전송 (기존 루프 대체)
		for (auto pMover : mUserList)
		{
			if (pMover == nullptr) continue;

			// 1. [핵심] 마지막으로 패킷을 보낸 위치와 현재 위치의 거리를 직접 계산
			// Vector3_Distance2D가 있다면 사용하시고, 없으면 아래처럼 계산하세요.
			Vector3 curPos = pMover->GetPosition();
			Vector3 lastPos = pMover->mLastSentPos; // User 클래스에 이 멤버 변수가 있어야 합니다.

			float dx = curPos.x - lastPos.x;
			float dz = curPos.z - lastPos.z;
			float moveDistSq = (dx * dx) + (dz * dz); // 루트 안 씌운 제곱값으로 비교 (연산 절약)

			// 2. 전송 조건: 0.01m(제곱값 0.0001f) 이상 움직였거나, 강제 전송 플래그(Dirty)가 켜졌을 때만
			if (moveDistSq > 0.0001f || pMover->mIsDirty)
			{
				UPDATE_PLAYER_MOVEMENT_PACKET syncPkt;
				syncPkt.lastInputSeq = pMover->GetLastInputSeq();
				syncPkt.userUUID = pMover->GetNetConnIdx();
				syncPkt.currentPos = curPos;
				syncPkt.currentRot = pMover->GetRotation();
				syncPkt.axis = pMover->GetAxis();

				// 나를 보고 있는 사람(AOI)에게만 전송
				for (auto targetIdx : pMover->mVisibleList)
				{
					SendPacketFunc((UINT32)targetIdx, syncPkt.PacketLength, (char*)&syncPkt);
				}

				// 나 자신에게도 확정 좌표 전송 (보정용)
				SendPacketFunc((UINT32)pMover->GetNetConnIdx(), syncPkt.PacketLength, (char*)&syncPkt);

				// 3. [중요] 전송 완료 후 현재 좌표를 '마지막 전송 좌표'로 저장
				pMover->mLastSentPos = curPos;
				pMover->mIsDirty = false;
			}
		}

		for (auto pNpc : mNpcList)
		{
			if (pNpc == nullptr) continue;

			Vector3 curPos = pNpc->GetPosition();
			Vector3 lastPos = pNpc->mLastSentPos;
			float moveDistSq = (curPos.x - lastPos.x) * (curPos.x - lastPos.x) + (curPos.z - lastPos.z) * (curPos.z - lastPos.z);

			if (moveDistSq > 0.0001f || pNpc->mIsDirty)
			{
				UPDATE_PLAYER_MOVEMENT_PACKET syncPkt;
				syncPkt.lastInputSeq = pNpc->GetLastInputSeq();
				syncPkt.userUUID = pNpc->GetNetConnIdx();
				syncPkt.currentPos = curPos;
				syncPkt.currentRot = pNpc->GetRotation();
				syncPkt.axis = pNpc->GetAxis();

				for (auto pTarget : mUserList) {
					if (pTarget) SendPacketFunc((UINT32)pTarget->GetNetConnIdx(), syncPkt.PacketLength, (char*)&syncPkt);
				}

				pNpc->mLastSentPos = curPos;
				pNpc->mIsDirty = false;
			}
		}

		// 이동 동기화 패킷 전송
		//for (auto pUser : mUserList)
		//{
		//	if (pUser == nullptr || !pUser->GetIsMoving()) continue;

		//	UPDATE_PLAYER_MOVEMENT_PACKET syncPkt;
		//	syncPkt.lastInputSeq = pUser->GetLastInputSeq();
		//	syncPkt.userUUID = pUser->GetNetConnIdx();
		//	syncPkt.currentPos = pUser->GetPosition();
		//	syncPkt.currentRot = pUser->GetRotation();
		//	syncPkt.axis = pUser->GetAxis();
		//	
		//	//syncPkt.isMoving = pUser->GetIsMoving();
		//	//syncPkt.currentSpeed = pUser->GetCurrentSpeed();
		//	//SendToAllUser(syncPkt.PacketLength, (char*)&syncPkt, pUser->GetNetConnIdx(), false);
		//	for (auto pTarget : mUserList) 
		//	{
		//		if (pTarget) 
		//		{
		//			SendPacketFunc((UINT32)pTarget->GetNetConnIdx(), syncPkt.PacketLength, (char*)&syncPkt);
		//		}
		//	}
		//}

		//for (auto pNpc : mNpcList)
		//{
		//	if (pNpc == nullptr || !pNpc->GetIsMoving()) continue;

		//	UPDATE_PLAYER_MOVEMENT_PACKET syncPkt;
		//	syncPkt.lastInputSeq = pNpc->GetLastInputSeq();
		//	syncPkt.userUUID = pNpc->GetNetConnIdx();
		//	syncPkt.currentPos = pNpc->GetPosition();
		//	syncPkt.currentRot = pNpc->GetRotation();
		//	syncPkt.axis = pNpc->GetAxis();

		//	//syncPkt.isMoving = pNpc->GetIsMoving();
		//	//syncPkt.currentSpeed = pNpc->GetCurrentSpeed();
		//	//SendToAllUser(syncPkt.PacketLength, (char*)&syncPkt, pNpc->GetNetConnIdx(), false);
		//	for (auto pTarget : mUserList)
		//	{
		//		if (pTarget)
		//		{
		//			SendPacketFunc((UINT32)pTarget->GetNetConnIdx(), syncPkt.PacketLength, (char*)&syncPkt);
		//		}
		//	}
		//}

		if (mCountdownTimer > 0) 
		{
			mCountdownTimer -= dt;
			int currentSec = (int)ceil(mCountdownTimer);

			if (currentSec < mLastAnnouncedSecond) 
			{
				mLastAnnouncedSecond = currentSec;

				GAME_START_COUNTDOWN_NTF_PACKET ntf;
				ntf.remainSeconds = currentSec;
				BroadcastPacket(ntf.PacketLength, (char*)&ntf);
			}

			if (mCountdownTimer <= 0)
			{
				mCountdownTimer = -1.0f;
				// 던전 진입 명령 (첫 번째 던전 MapId = 1)
				GAME_START_NTF_PACKET startNtf;
				startNtf.mapId = 1;
				BroadcastPacket(startNtf.PacketLength, (char*)&startNtf);
				printf("[Room %d] Dungeon Start!\n", mRoomNum);
			}
		}
	}

	void ProcessPlayerReady(User* user, bool isReady) 
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		// 해당 유저의 슬롯 인덱스 찾기
		int slotIdx = -1;
		for (int i = 0; i < mMaxUserCount; ++i) 
		{
			if (mSlots[i] == user) 
			{
				slotIdx = i;
				break;
			}
		}
		if (slotIdx == -1) return;

		mIsReady[slotIdx] = isReady;

		// 준비 상태 브로드캐스트
		ROOM_READY_STATUS_NTF_PACKET ntf;
		ntf.userUUID = user->GetNetConnIdx();
		ntf.isReady = isReady;
		BroadcastPacket(ntf.PacketLength, (char*)&ntf);

		// 전원 준비 체크
		CheckAllReady();
	}

	void CheckAllReady() 
	{
		if (mCurrentUserCount == 0) return;

		bool allReady = true;
		for (int i = 0; i < mMaxUserCount; ++i) 
		{
			// 접속 중인 슬롯인데 준비가 안 됐다면 false
			if (mSlots[i] != nullptr && !mIsReady[i]) 
			{
				allReady = false;
				break;
			}
		}

		if (allReady && mCountdownTimer < 0) 
		{
			// 카운트다운 시작
			mCountdownTimer = 5.0f;
			mLastAnnouncedSecond = 6; // 처음 5초 알림을 위해 설정
			printf("[Room %d] All players ready! Starting countdown.\n", mRoomNum);
		}
		else if (!allReady && mCountdownTimer > 0) 
		{
			// 누군가 준비 영역을 나감 -> 카운트다운 취소
			mCountdownTimer = -1.0f;
			PACKET_HEADER cancelPkt(sizeof(PACKET_HEADER), PACKET_ID::GAME_READY_CANCEL_NTF);
			BroadcastPacket(cancelPkt.PacketLength, (char*)&cancelPkt);
			printf("[Room %d] Countdown canceled.\n", mRoomNum);
		}
	}

	void ProcessEscapeRequest(User* user)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		// 1. 패킷을 보낸 유저가 몇 번 슬롯인지 찾기
		int slotIdx = -1;
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] == user)
			{
				slotIdx = i;
				break;
			}
		}
		if (slotIdx == -1) return;

		// 2. 해당 유저 탈출 완료 마킹
		mIsEscaped[slotIdx] = true;
		printf("[Room %d] User %s arrived at escape zone.\n", mRoomNum, user->GetUserId().c_str());

		// 3. 방에 있는 전원이 탈출했는지 체크 (CheckAllEscaped 로직)
		bool allEscaped = true;
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			// 접속 중인 슬롯(nullptr 아님)인데 아직 안 들어왔다면 false
			if (mSlots[i] != nullptr && !mIsEscaped[i])
			{
				allEscaped = false;
				break;
			}
		}

		// 4. 모두 모였다면 클리어 처리
		if (allEscaped && mCurrentUserCount > 0)
		{
			printf("[Room %d] All users escaped! Dungeon Cleared.\n", mRoomNum);

			// 던전 클리어 패킷 브로드캐스트
			DUNGEON_CLEAR_NTF_PACKET clearPkt;
			BroadcastPacket(clearPkt.PacketLength, (char*)&clearPkt);

			// 상태 초기화 (다음 게임을 위해)
			std::fill(mIsReady.begin(), mIsReady.end(), false);
			std::fill(mIsEscaped.begin(), mIsEscaped.end(), false);
			mCountdownTimer = -1.0f;
		}
	}

	void SyncRoomUsers(User* user_)
	{
		if (user_ == nullptr) return;

		for (auto pRoomUser : mSlots)
		{
			if (pRoomUser == nullptr || pRoomUser == user_) continue;

			// 방금 던전 로딩이 끝난 유저에게 -> 기존 유저의 모습을 보여줌
			ROOM_USER_INFO_NTF_PACKET infoForNew;
			infoForNew.userUUID = pRoomUser->GetNetConnIdx();
			CopyUserID(infoForNew.userID, *pRoomUser);
			infoForNew.position = pRoomUser->GetPosition();
			infoForNew.rotation = pRoomUser->GetRotation();
			SendPacketFunc(user_->GetNetConnIdx(), infoForNew.PacketLength, (char*)&infoForNew);

			// 기존 유저에게 -> 방금 던전 로딩이 끝난 유저의 모습을 보여줌
			ROOM_USER_INFO_NTF_PACKET infoForOld;
			infoForOld.userUUID = user_->GetNetConnIdx();
			CopyUserID(infoForOld.userID, *user_);
			infoForOld.position = user_->GetPosition();
			infoForOld.rotation = user_->GetRotation();
			SendPacketFunc(pRoomUser->GetNetConnIdx(), infoForOld.PacketLength, (char*)&infoForOld);
		}
	}

private:
	bool CanSee(User* viewer, Actor* target)
	{
		if (viewer == target) return true; // 자기 자신은 항상 보임

		float dist = Vector3_Distance2D(viewer->GetPosition(), target->GetPosition());

		bool isTargetInBush = NavMeshManager::GetInstance()->IsInBush(target->GetPosition());

		if (isTargetInBush)
		{
			bool isViewerInBush = NavMeshManager::GetInstance()->IsInBush(viewer->GetPosition());

			if (!isViewerInBush) return false;
		}

		return true;
	}

	Vector3 bossLinePos;

	std::recursive_mutex mLock;
	INT32 mRoomNum = -1;

	std::list<User*> mUserList;
	std::list<Npc*> mNpcList;


	INT32 mMaxUserCount = 2;

	UINT16 mCurrentUserCount = 0;

	std::vector<User*> mSlots;
	std::vector<bool> mIsReady;
	std::vector<bool> mIsEscaped;
	INT64 mHostUUID = -1;

	float mCountdownTimer = -1.0f; // -1이면 카운트다운 중 아님
	int mLastAnnouncedSecond = 0;
};


void CopyUserID(char* userID, const Actor& user)
{
	CopyUserID(userID, user.GetUserId());
}

void CopyUserID(char* userID, const std::string& userID_)
{
	CopyMemory(userID, userID_.c_str(), userID_.size() + 1);
}

void CopyUserID(char* userID, const char* userID_)
{
	CopyMemory(userID, userID_, (MAX_USER_ID_LEN + 1));
}