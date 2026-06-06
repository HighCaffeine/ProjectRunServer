#pragma once

#include "UserModels\UserManager.h"
#include "Packet\Packet.h"
#include "Utility\unity.h"
#include <functional>
#include <vector>
#include <mutex>

class Room
{
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

		mCharIDs.assign(maxUserCount_, 0);
		if (maxUserCount_ > 1) mCharIDs[1] = 1;

		mCurrentUserCount = 0;
		mHostUUID = -1;
		mIsPlaying = false;
		memset(mTitle, 0, sizeof(mTitle));
	}

	UINT16 EnterUser(User* pNewUser)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		if (mCurrentUserCount >= mMaxUserCount) return (UINT16)ERROR_CODE::ENTER_ROOM_FULL_USER;

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

		// 기존 유저들에게 신규 유저 정보 전송, 신규 유저에게 기존 유저 정보 전송
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			User* pExistingUser = mSlots[i];
			if (pExistingUser == nullptr || pExistingUser == pNewUser) continue;

			// 기존 유저 → 새 유저에게 기존 유저 정보 전송
			ROOM_USER_INFO_NTF_PACKET ntfToNew;
			ntfToNew.userUUID = pExistingUser->GetNetConnIdx();
			CopyUserID(ntfToNew.userID, pExistingUser->GetUserId());
			ntfToNew.characterID = mCharIDs[i];
			SendPacketFunc(pNewUser->GetNetConnIdx(), ntfToNew.PacketLength, (char*)&ntfToNew);

			// 새 유저 → 기존 유저에게 새 유저 정보 전송
			ROOM_USER_INFO_NTF_PACKET ntfToOld;
			ntfToOld.userUUID = pNewUser->GetNetConnIdx();
			CopyUserID(ntfToOld.userID, pNewUser->GetUserId());
			ntfToOld.characterID = mCharIDs[slotIndex];
			SendPacketFunc(pExistingUser->GetNetConnIdx(), ntfToOld.PacketLength, (char*)&ntfToOld);
		}

		mCurrentUserCount++;

		if (mCurrentUserCount == 1) mHostUUID = pNewUser->GetNetConnIdx();

		BroadcastHostInfo();
		return (UINT16)ERROR_CODE::NONE;
	}

	void SetTitle(const char* title)
	{
		strncpy_s(mTitle, sizeof(mTitle), title, _TRUNCATE);
	}
	const char* GetTitle() { return mTitle; }

	void LeaveUser(User* leaveUser_)
	{
		if (leaveUser_ == nullptr) return;

		std::lock_guard<std::recursive_mutex> guard(mLock);

		bool isUserFound = false;

		// 슬롯에서 유저 찾아서 지우기
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] == leaveUser_)
			{
				mSlots[i] = nullptr;
				mIsReady[i] = false;
				isUserFound = true;
				break;
			}
		}

		if (isUserFound)
		{
			--mCurrentUserCount;

			// 3. 방에 아무도 안 남았을 때
			if (mCurrentUserCount <= 0)
			{
				mCurrentUserCount = 0;

				if (mIsPlaying)
				{
					// 게임 중에는 방 데이터 유지
					printf("[Room] 게임 중 방 인원 0명 - 방 데이터 유지\n");
				}
				else
				{
					// 대기 중에 방이 비면 완전 초기화
					memset(mTitle, 0, sizeof(mTitle));
					mHostUUID = -1;
					printf("[Room] 방 인원이 0명이 되어 방 데이터를 초기화\n");
				}

				mIsPlaying = false;
				return;
			}

			// 남은 파티원들에게 퇴장 알림 브로드캐스트 (방이 유지될 때만)
			ROOM_LEAVE_USER_NTF_PACKET notifyPkt;
			notifyPkt.userUUID = leaveUser_->GetNetConnIdx();
			CopyUserID(notifyPkt.userID, leaveUser_->GetUserId());
			BroadcastPacket(notifyPkt.PacketLength, (char*)&notifyPkt);

			// 호스트가 나갔다면 새로운 방장 위임
			if (leaveUser_->GetNetConnIdx() == mHostUUID)
			{
				mHostUUID = -1;
				for (int i = 0; i < mMaxUserCount; ++i)
				{
					if (mSlots[i] != nullptr)
					{
						mHostUUID = mSlots[i]->GetNetConnIdx();
						break;
					}
				}
				if (mHostUUID != -1)
				{
					BroadcastHostInfo();
				}
			}
		}
	}

	User* GetUserBySlot(int slotIndex)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		if (slotIndex < 0 || slotIndex >= mMaxUserCount) return nullptr;
		return mSlots[slotIndex];
	}

	bool IsSlotReady(int slotIndex)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		if (slotIndex < 0 || slotIndex >= mMaxUserCount) return false;
		return mIsReady[slotIndex];
	}

	void BroadcastHostInfo()
	{
		ROOM_HOST_NTF_PACKET hostPkt;
		hostPkt.hostUUID = mHostUUID;
		BroadcastPacket(hostPkt.PacketLength, (char*)&hostPkt);
	}

	void NotifyChat(INT32 clientIndex_, const char* userID_, const char* msg_)
	{
		ROOM_CHAT_NOTIFY_PACKET roomChatNtfyPkt;
		CopyMemory(roomChatNtfyPkt.Msg, msg_, sizeof(roomChatNtfyPkt.Msg));
		CopyUserID(roomChatNtfyPkt.userID, userID_);
		BroadcastPacket(sizeof(roomChatNtfyPkt), (char*)&roomChatNtfyPkt);
	}

	void ProcessPlayerReady(User* user, bool isReady)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

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

		ROOM_READY_STATUS_NTF_PACKET ntf;
		ntf.userUUID = user->GetNetConnIdx();
		ntf.isReady = isReady;
		BroadcastPacket(ntf.PacketLength, (char*)&ntf);

		// todo 유저 모두 준비 시 인증토큰 로직 추가
	}

	bool IsAllReady()
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		if (mCurrentUserCount < mMaxUserCount) return false;

		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] == nullptr) continue;
			if (mSlots[i]->GetNetConnIdx() == mHostUUID) continue; // 호스트 제외
			if (mIsReady[i] == false) return false;
		}
		return true;
	}

	// LobbyManager가 세션 만들 때 유저 정보를 빼갈 수 있게 반환
	std::vector<User*> GetRoomUserList()
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		std::vector<User*> users;
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] != nullptr) users.push_back(mSlots[i]);
		}
		return users;
	}

	void BroadcastPacket(UINT16 packetSize, char* pPacket)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] != nullptr) SendPacketFunc((UINT32)mSlots[i]->GetNetConnIdx(), packetSize, pPacket);
		}
	}

	void SendToAllUser(const UINT16 dataSize_, char* data_, const INT32 passUserIndex_, bool exceptMe)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] == nullptr) continue;
			if (exceptMe && mSlots[i]->GetNetConnIdx() == passUserIndex_) continue;
			SendPacketFunc((UINT32)mSlots[i]->GetNetConnIdx(), (UINT32)dataSize_, data_);
		}
	}

	INT32 GetHostCharID()
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] != nullptr && mSlots[i]->GetNetConnIdx() == mHostUUID)
			{
				return mCharIDs[i];
			}
		}
		return 0;
	}

	INT32 GetGuestCharID()
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] != nullptr && mSlots[i]->GetNetConnIdx() != mHostUUID)
			{
				return mCharIDs[i];
			}
		}
		return 1;
	}

	void SetUserCharID(User* pUser, INT32 charID)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] == pUser)
			{
				mCharIDs[i] = charID;
				break;
			}
		}
	}

	// 슬롯 번호로 캐릭터 ID 꺼내오는 함수 (목록 전송용)
	INT32 GetCharacterIDBySlot(int slotIndex)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		if (slotIndex < 0 || slotIndex >= mMaxUserCount) return 0;
		return mCharIDs[slotIndex];
	}

	INT64 GetHostUUID() { return mHostUUID; }
	void Update(float dt) {}

	std::function<void(UINT32, UINT32, char*)> SendPacketFunc;

	bool GetIsPlaying() { return mIsPlaying; }
	void SetIsPlaying(bool isPlaying) { mIsPlaying = isPlaying; }
private:
	std::vector<INT32> mCharIDs;
	INT64 mHostUUID = -1;
	//INT32 mCharIDs[2] = { 0, 1 };

	std::recursive_mutex mLock;
	INT32 mRoomNum = -1;
	bool mIsPlaying = false;

	char mTitle[32] = { 0, };

	INT32 mMaxUserCount = 2;
	UINT16 mCurrentUserCount = 0;

	std::vector<User*> mSlots;
	std::vector<bool> mIsReady;
};