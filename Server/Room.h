#pragma once

#include "Npc.h"
#include "UserManager.h"
#include "Packet.h"
#include "gimmickdata.h"
#include "unity.h"

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

	void Init(const INT32 roomNum_, const INT32 maxUserCount_)
	{
		mRoomNum = roomNum_;
		mMaxUserCount = maxUserCount_;
		mSlots.assign(maxUserCount_, nullptr);
		mIsReady.assign(maxUserCount_, false);
		mIsEscaped.assign(maxUserCount_, false);
	}

	//UINT16 EnterUser(User* user_)
	//{
	//	std::lock_guard<std::recursive_mutex> guard(mLock);
	//	if (mCurrentUserCount >= mMaxUserCount)
	//	{
	//		return (UINT16)ERROR_CODE::ENTER_ROOM_FULL_USER;
	//	}

	//	mUserList.push_back(user_);
	//	++mCurrentUserCount;

	//	//빈 자리중 가장 앞자리에 배치
	//	int slotIndex = -1;
	//	for (int i = 0; i < mMaxUserCount; ++i)
	//	{
	//		if (mSlots[i] == nullptr)
	//		{
	//			mSlots[i] = user_;
	//			slotIndex = i;
	//			break;
	//		}
	//	}

	//	// 방에 처음 들어온 사람이면 방장으로 임명
	//	if (mCurrentUserCount == 1)
	//	{
	//		mHostUUID = user_->GetNetConnIdx();
	//	}

	//	user_->EnterRoom(mRoomNum);

	//	Vector3 snappedPos;
	//	if (NavMeshManager::GetInstance()->GetValidMovePosition(user_->GetPosition(), user_->GetPosition(), snappedPos))
	//	{
	//		user_->SetPosition(snappedPos);
	//	}

	//	// [입장 처리]
	//	// 기존 유저들에 대해 CanSee(6.0m) 체크 후 시야 등록
	//	for (auto pRoomUser : mUserList)
	//	{
	//		if (pRoomUser == nullptr || pRoomUser == user_) continue;

	//		// 6.0m 이내에 있는가?
	//		if (CanSee(user_, pRoomUser) == false) continue;

	//		// 서로 시야 목록에 추가
	//		user_->mVisibleList.insert(pRoomUser->GetNetConnIdx());
	//		pRoomUser->mVisibleList.insert(user_->GetNetConnIdx());



	//		// 나에게 상대방 정보 보내기 (Create)
	//		ROOM_USER_INFO_NTF_PACKET roomUserInfoNtf;
	//		roomUserInfoNtf.userUUID = pRoomUser->GetNetConnIdx();
	//		CopyUserID(roomUserInfoNtf.userID, *pRoomUser);
	//		roomUserInfoNtf.position = pRoomUser->GetPosition();
	//		roomUserInfoNtf.rotation = pRoomUser->GetRotation();
	//		SendPacketFunc(user_->GetNetConnIdx(), roomUserInfoNtf.PacketLength, (char*)&roomUserInfoNtf);
	//	}

	//	// NPC 처리
	//	for (auto pRoomNpc : mNpcList)
	//	{
	//		if (pRoomNpc == nullptr) continue;

	//		if (CanSee(user_, pRoomNpc) == false) continue;

	//		ROOM_USER_INFO_NTF_PACKET roomUserInfoNtf;
	//		roomUserInfoNtf.userUUID = pRoomNpc->GetNetConnIdx();
	//		CopyUserID(roomUserInfoNtf.userID, *pRoomNpc);
	//		roomUserInfoNtf.position = pRoomNpc->GetPosition();
	//		roomUserInfoNtf.rotation = pRoomNpc->GetRotation();
	//		SendPacketFunc(user_->GetNetConnIdx(), roomUserInfoNtf.PacketLength, (char*)&roomUserInfoNtf);
	//	}

	//	BroadcastHostInfo();

	//	return (UINT16)ERROR_CODE::NONE;
	//}

	// Room.h 내부 EnterUser 함수 구현 수정
	UINT16 EnterUser(User* pNewUser)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		// 1. 방 정원 체크
		if (mCurrentUserCount >= mMaxUserCount)
		{
			return (UINT16)ERROR_CODE::ENTER_ROOM_FULL_USER;
		}

		// 2. 빈 슬롯에 유저 등록
		int slotIndex = -1;
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] == nullptr)
			{
				mSlots[i] = pNewUser;
				slotIndex = i;
				break;
			}
		}

		if (slotIndex == -1) return (UINT16)ERROR_CODE::ENTER_ROOM_FULL_USER;

		pNewUser->SetDomainState(User::DOMAIN_STATE::ROOM);
		pNewUser->EnterRoom(mRoomNum);

		// ---------------------------------------------------------
		// 3. 상호 동기화 (Mutual Notification) 핵심 로직
		// ---------------------------------------------------------

		// 신규 유저용 응답 패킷 준비 (본인에게 성공 알림)
		// P_RoomEnterResponse 등 기존 응답 처리는 PacketManager에서 수행됨을 전제

		for (auto pExistingUser : mUserList)
		{
			if (pExistingUser == nullptr || pExistingUser == pNewUser) continue;

			// A. 기존 유저들에게 -> "신규 유저(pNewUser) 정보"를 전송
			ROOM_USER_INFO_NTF_PACKET ntfToOld;
			ntfToOld.userUUID = pNewUser->GetNetConnIdx();
			CopyUserID(ntfToOld.userID, *pNewUser);
			ntfToOld.position = pNewUser->GetPosition();
			ntfToOld.rotation = pNewUser->GetRotation();

			SendPacketFunc(pExistingUser->GetNetConnIdx(), ntfToOld.PacketLength, (char*)&ntfToOld);

			// B. 신규 유저에게 -> "기존 유저(pExistingUser) 정보"를 전송
			ROOM_USER_INFO_NTF_PACKET ntfToNew; // Packets.cs의 CREATE_MATCH_PLAYER 대응
			ntfToNew.userUUID = pExistingUser->GetNetConnIdx();
			CopyUserID(ntfToNew.userID, *pExistingUser);
			ntfToNew.position = pExistingUser->GetPosition();
			ntfToNew.rotation = pExistingUser->GetRotation();

			SendPacketFunc(pNewUser->GetNetConnIdx(), ntfToNew.PacketLength, (char*)&ntfToNew);
		}

		// 4. 유저 목록에 추가
		mUserList.push_back(pNewUser);
		mCurrentUserCount++;

		if (mCurrentUserCount == 1)
		{
			mHostUUID = pNewUser->GetNetConnIdx();
		}

		BroadcastHostInfo();


		return (UINT16)ERROR_CODE::NONE;
	}

	void SyncRoomStateToUser(User* pTargetUser)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		// 1. 방금 씬 로딩을 마친 유저(pTargetUser)에게 기존 유저들의 정보를 쏴줍니다.
		for (auto pOther : mUserList)
		{
			if (pOther == nullptr || pOther == pTargetUser) continue;
			printf("3. [서버 발송] %s 에게 기존 유저 %s 정보(ROOM_USER_INFO_NTF) 전송!\\n", pTargetUser->GetUserId().c_str(), pOther->GetUserId().c_str());
			ROOM_USER_INFO_NTF_PACKET ntfToNew;
			ntfToNew.userUUID = pOther->GetNetConnIdx();
			CopyUserID(ntfToNew.userID, *pOther);
			ntfToNew.position = pOther->GetPosition();
			ntfToNew.rotation = pOther->GetRotation();

			SendPacketFunc(pTargetUser->GetNetConnIdx(), ntfToNew.PacketLength, (char*)&ntfToNew);
		}

		// 2. [선택] 기존 유저들에게도 "얘 로딩 다 끝났으니 이제 화면에 띄워라!" 라고 알려줍니다.
		ROOM_USER_INFO_NTF_PACKET ntfToOld;
		ntfToOld.userUUID = pTargetUser->GetNetConnIdx();
		CopyUserID(ntfToOld.userID, *pTargetUser);
		ntfToOld.position = pTargetUser->GetPosition();
		ntfToOld.rotation = pTargetUser->GetRotation();

		for (auto pOther : mUserList)
		{
			if (pOther == nullptr || pOther == pTargetUser) continue;
			SendPacketFunc(pOther->GetNetConnIdx(), ntfToOld.PacketLength, (char*)&ntfToOld);
		}
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

		for (auto pUser : mUserList)
		{
			if (pUser != nullptr)
			{
				pUser->mVisibleList.erase(leaveUser_->GetNetConnIdx());
			}
		}
		leaveUser_->mVisibleList.clear();

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
		bool EXCEPT_ME = false;
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
		bool EXCEPT_ME = true;
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

		// 이동 동기화 패킷 전송
		for (auto pMover : mUserList)
		{
			if (pMover == nullptr) continue;

			Vector3 curPos = pMover->GetPosition();
			Vector3 lastPos = pMover->mLastSentPos;
			Quaternion curRot = pMover->GetRotation();    // 현재 회전값
			Quaternion lastRot = pMover->mLastSentRot;    // 이전 회전값

			float dx = curPos.x - lastPos.x;
			float dy = curPos.y - lastPos.y;
			float dz = curPos.z - lastPos.z;
			float moveDistSq = (dx * dx) + (dy * dy) + (dz * dz);

			// 회전했는지
			float rotDiffSq = (curRot.x - lastRot.x) * (curRot.x - lastRot.x) +
				(curRot.y - lastRot.y) * (curRot.y - lastRot.y) +
				(curRot.z - lastRot.z) * (curRot.z - lastRot.z) +
				(curRot.w - lastRot.w) * (curRot.w - lastRot.w);

			// 위치가 변했거나, 회전이 변했거나, 강제 전송 플래그가 켜졌을 때 검사
			if (moveDistSq > 0.0001f || rotDiffSq > 0.0001f || pMover->mIsDirty)
			{
				UPDATE_PLAYER_MOVEMENT_PACKET syncPkt;
				syncPkt.lastInputSeq = pMover->GetLastInputSeq();
				syncPkt.userUUID = pMover->GetNetConnIdx();
				syncPkt.currentPos = curPos;
				syncPkt.currentRot = curRot;
				syncPkt.axisH = pMover->GetAxis().x;
				syncPkt.axisV = pMover->GetAxis().y;
				syncPkt.isMoving = (syncPkt.axisH != 0 || syncPkt.axisV != 0);

				for (auto targetIdx : pMover->mVisibleList)
				{
					SendPacketFunc((UINT32)targetIdx, syncPkt.PacketLength, (char*)&syncPkt);
				}
				SendPacketFunc((UINT32)pMover->GetNetConnIdx(), syncPkt.PacketLength, (char*)&syncPkt);

				// 갱신
				pMover->mLastSentPos = curPos;
				pMover->mLastSentRot = curRot;
				pMover->mIsDirty = false;
			}
		}

		for (auto pNpc : mNpcList)
		{
			if (pNpc == nullptr) continue;

			Vector3 curPos = pNpc->GetPosition();
			Vector3 lastPos = pNpc->mLastSentPos;
			Quaternion curRot = pNpc->GetRotation();    // 현재 회전값
			Quaternion lastRot = pNpc->mLastSentRot;    // 이전 회전값

			float dx = curPos.x - lastPos.x;
			float dz = curPos.z - lastPos.z;
			float moveDistSq = (dx * dx) + (dz * dz);

			// 회전했는지
			float rotDiffSq = (curRot.x - lastRot.x) * (curRot.x - lastRot.x) +
				(curRot.y - lastRot.y) * (curRot.y - lastRot.y) +
				(curRot.z - lastRot.z) * (curRot.z - lastRot.z) +
				(curRot.w - lastRot.w) * (curRot.w - lastRot.w);

			if (moveDistSq > 0.0001f || rotDiffSq > 0.0001f || pNpc->mIsDirty)
			{
				UPDATE_PLAYER_MOVEMENT_PACKET syncPkt;
				syncPkt.lastInputSeq = pNpc->GetLastInputSeq();
				syncPkt.userUUID = pNpc->GetNetConnIdx();
				syncPkt.currentPos = curPos;
				syncPkt.currentRot = pNpc->GetRotation();
				syncPkt.axisH = pNpc->GetAxis().x;
				syncPkt.axisV = pNpc->GetAxis().y;
				syncPkt.isMoving = (syncPkt.axisH != 0 || syncPkt.axisV != 0);

				for (auto pTarget : mUserList) 
				{
					if (pTarget) SendPacketFunc((UINT32)pTarget->GetNetConnIdx(), syncPkt.PacketLength, (char*)&syncPkt);
				}

				pNpc->mLastSentPos = curPos;
				pNpc->mIsDirty = false;
			}
		}

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
						gimmick.currentState = 0; // 상태를 0(Off/복구)으로 변경
						gimmick.gimmickRecoverTime = 0.0f;

						PLAYER_GIMMICK_INTERACT_NTF_PACKET ntfPkt;
						ntfPkt.activeUUID = -1; 
						ntfPkt.gimmickID = gimmick.gimmickID;
						ntfPkt.gimmickKey = gimmick.type;
						ntfPkt.state = (byte)eGimmickState::Restore; // 0
						ntfPkt.param = 0.0f;

						ntfPkt.targetPos = { gimmick.position.x, gimmick.position.y, gimmick.position.z };

						// AOI 거리 기반 전송 (반경 40m)
						const float AOI_RANGE_SQ = 40.0f * 40.0f;
						for (auto pTarget : mUserList)
						{
							if (pTarget == nullptr) continue;

							float dx = pTarget->GetPosition().x - ntfPkt.targetPos.x;
							float dy = pTarget->GetPosition().y - ntfPkt.targetPos.y;
							float dz = pTarget->GetPosition().z - ntfPkt.targetPos.z;
							float distSq = (dx * dx) + (dy * dy) + (dz * dz);

							if (distSq <= AOI_RANGE_SQ)
							{
								SendPacketFunc((UINT32)pTarget->GetNetConnIdx(), ntfPkt.PacketLength, (char*)&ntfPkt);
							}
						}

						// printf("[Room %d] 추락 발판(ID: %d) 복구 완료. State 0 브로드캐스트.\n", mRoomNum, gimmick.gimmickID);
					}
				}
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
			if (mSlots[i] == nullptr || !mIsReady[i])
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
			GAME_READY_CANCEL_NTF_PACKET cancelPkt;
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

	void LoadMapData(const std::string& mapFilePath)
	{
		FILE* fp = fopen(mapFilePath.c_str(), "rb");
		if (fp == nullptr)
		{
			printf("[Error] 맵 JSON 파일을 찾을 수 없습니다: %s\n", mapFilePath.c_str());
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

		printf("[Room %d] 맵 데이터 로딩 완료! 총 기믹 수: %zu개\n", mRoomNum, mGimmicks.size());
	}

	void ProcessGimmickInteract(User* pUser, PLAYER_GIMMICK_INTERACT_REQUEST_PACKET* pReq)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		auto it = mGimmicks.find(pReq->gimmickID);
		if (it == mGimmicks.end())
		{
			printf("[Warning] 존재하지 않는 기믹 ID(%d) 조작 요청!\n", pReq->gimmickID);
			return;
		}

		if (pReq->state != (byte)eGimmickState::Sync && it->second.currentState == pReq->state)
		{
			return;
		}

		it->second.currentState = pReq->state;

		if (it->second.type == (int)eGimmickKey::FallingPlatform && pReq->state == 1)
		{
			it->second.gimmickRecoverTime = 7.0f;
		}

		PLAYER_GIMMICK_INTERACT_NTF_PACKET ntfPkt;
		ntfPkt.activeUUID = pReq->activeUUID;
		ntfPkt.gimmickID = pReq->gimmickID;
		ntfPkt.gimmickKey = pReq->gimmickKey;
		ntfPkt.state = pReq->state;
		ntfPkt.param = pReq->param;

		if (ntfPkt.gimmickKey == eGimmickKey::MovePlatform)
		{
			if (pReq->state == (byte)eGimmickState::Sync)
			{
				ntfPkt.targetPos = pReq->targetPos;
			}
			else
			{
				ntfPkt.targetPos = { it->second.position.x, it->second.position.y, it->second.position.z };
			}

			//BroadcastPacket(ntfPkt.PacketLength, (char*)&ntfPkt);
			BroadcastPacketInRange(ntfPkt.PacketLength, (char*)&ntfPkt, pReq->targetPos, 30.0f);
		}
		else
		{
			BroadcastPacket(ntfPkt.PacketLength, (char*)&ntfPkt);
		}
	}

	void BroadcastPacketInRange(int len, char* pkt, Vector3 center, float range)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		float rangeSq = range * range;

		for (auto pTarget : mUserList)
		{
			if (pTarget == nullptr) continue;

			Vector3 pos = pTarget->GetPosition();
			float dx = pos.x - center.x;
			float dy = pos.y - center.y; 
			float dz = pos.z - center.z;

			float distSq = (dx * dx) + (dy * dy) + (dz * dz);

			if (distSq <= rangeSq)
			{
				SendPacketFunc((UINT32)pTarget->GetNetConnIdx(), len, pkt);
			}
		}
	}
private:
	bool CanSee(User* viewer, Actor* target)
	{
		if (viewer == target) return true; // 자기 자신은 항상 보임

		float dist = Vector3_Distance2D(viewer->GetPosition(), target->GetPosition());

		return true;
		bool isTargetInBush = NavMeshManager::GetInstance()->IsInBush(target->GetPosition());

		if (isTargetInBush)
		{
			bool isViewerInBush = NavMeshManager::GetInstance()->IsInBush(viewer->GetPosition());

			if (!isViewerInBush) return false;
		}

		return true;
	}

	std::unordered_map<int, ServerGimmickData> mGimmicks;

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