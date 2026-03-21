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
		//NavMeshManager::GetInstance()->Init(navMeshFileName);
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

		--mCurrentUserCount;

		ROOM_LEAVE_USER_NTF_PACKET notifyPkt;
		notifyPkt.userUUID = leaveUser_->GetNetConnIdx();
		CopyUserID(notifyPkt.userID, *leaveUser_);
		bool EXCEPT_ME = true;
		SendToAllUser(notifyPkt.PacketLength, (char*)&notifyPkt, notifyPkt.userUUID, EXCEPT_ME);
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
	UINT16 EnterCube(float x, float z)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		Npc* newNpc = CreateNpc();
		newNpc->EnterRoom(mRoomNum);

		// 큐브 속성 부여 및 좌표 설정
		newNpc->mIsCube = true;
		newNpc->SetPosition({ x, 0.0f, z });

		// TileCache에 동적 장애물 구역으로 등록
		newNpc->mObstacleRef = NavMeshManager::GetInstance()->AddObstacle(newNpc->GetPosition(), 1.0f, 2.0f);

		NotifyUserEnter(newNpc->GetNetConnIdx(), newNpc->GetUserId());
		return (UINT16)ERROR_CODE::NONE;
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
		for (auto pUser : mUserList)
		{
			if (pUser == nullptr || !pUser->GetIsMoving()) continue;

			UPDATE_PLAYER_MOVEMENT_PACKET syncPkt;
			syncPkt.lastInputSeq = pUser->GetLastInputSeq();
			syncPkt.userUUID = pUser->GetNetConnIdx();
			syncPkt.currentPos = pUser->GetPosition();
			syncPkt.isMoving = pUser->GetIsMoving();
			syncPkt.currentSpeed = pUser->GetCurrentSpeed();

			//SendToAllUser(syncPkt.PacketLength, (char*)&syncPkt, pUser->GetNetConnIdx(), false);
			for (auto pTarget : mUserList) 
			{
				if (pTarget) 
				{
					SendPacketFunc((UINT32)pTarget->GetNetConnIdx(), syncPkt.PacketLength, (char*)&syncPkt);
				}
			}
		}

		for (auto pNpc : mNpcList)
		{
			if (pNpc == nullptr || !pNpc->GetIsMoving()) continue;

			UPDATE_PLAYER_MOVEMENT_PACKET syncPkt;
			syncPkt.lastInputSeq = pNpc->GetLastInputSeq();
			syncPkt.userUUID = pNpc->GetNetConnIdx();
			syncPkt.currentPos = pNpc->GetPosition();
			syncPkt.isMoving = pNpc->GetIsMoving();
			syncPkt.currentSpeed = pNpc->GetCurrentSpeed();

			//SendToAllUser(syncPkt.PacketLength, (char*)&syncPkt, pNpc->GetNetConnIdx(), false);
			for (auto pTarget : mUserList)
			{
				if (pTarget)
				{
					SendPacketFunc((UINT32)pTarget->GetNetConnIdx(), syncPkt.PacketLength, (char*)&syncPkt);
				}
			}
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

	std::recursive_mutex mLock;
	INT32 mRoomNum = -1;

	std::list<User*> mUserList;
	std::list<Npc*> mNpcList;


	INT32 mMaxUserCount = 0;

	UINT16 mCurrentUserCount = 0;
};


void CopyUserID(char* userID, const Actor& user)
{
	CopyUserID(userID, user.GetUserId());
}

void CopyUserID(char* userID, const std::string& userID_)
{
	CopyMemory(userID, userID_.c_str(), sizeof(userID));
}

void CopyUserID(char* userID, const char* userID_)
{
	CopyMemory(userID, userID_, (MAX_USER_ID_LEN + 1));
}