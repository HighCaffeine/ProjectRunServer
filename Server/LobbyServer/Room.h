#pragma once

#include "UserModels\UserManager.h"
#include "Packet\Packet.h"
#include "Utility\unity.h"
#include <functional>
#include <vector>
#include <list>
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

		// 기존 방 인원들에게 신규 유저 정보 전송, 신규 유저에게 기존 인원 정보 전송
		for (auto pExistingUser : mUserList)
		{
			if (pExistingUser == nullptr || pExistingUser == pNewUser) continue;

			ROOM_USER_INFO_NTF_PACKET ntfToOld;
			ntfToOld.userUUID = pNewUser->GetNetConnIdx();
			CopyUserID(ntfToOld.userID, *pNewUser);
			SendPacketFunc(pExistingUser->GetNetConnIdx(), ntfToOld.PacketLength, (char*)&ntfToOld);

			ROOM_USER_INFO_NTF_PACKET ntfToNew;
			ntfToNew.userUUID = pExistingUser->GetNetConnIdx();
			CopyUserID(ntfToNew.userID, *pExistingUser);
			SendPacketFunc(pNewUser->GetNetConnIdx(), ntfToNew.PacketLength, (char*)&ntfToNew);
		}

		mUserList.push_back(pNewUser);
		mCurrentUserCount++;

		if (mCurrentUserCount == 1) mHostUUID = pNewUser->GetNetConnIdx();

		BroadcastHostInfo();
		return (UINT16)ERROR_CODE::NONE;
	}

	void LeaveUser(User* leaveUser_)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);

		mUserList.remove(leaveUser_);
		--mCurrentUserCount;

		for (int i = 0; i < mMaxUserCount; ++i)
		{
			if (mSlots[i] == leaveUser_)
			{
				mSlots[i] = nullptr;
				mIsReady[i] = false;
				break;
			}
		}

		ROOM_LEAVE_USER_NTF_PACKET notifyPkt;
		notifyPkt.userUUID = leaveUser_->GetNetConnIdx();
		CopyUserID(notifyPkt.userID, *leaveUser_);
		BroadcastPacket(notifyPkt.PacketLength, (char*)&notifyPkt);

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
			if (mHostUUID != -1) BroadcastHostInfo();
		}
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

	void NotifyUserEnter(INT32 clientIndex_, const std::string& userID)
	{
		ROOM_NEW_USER_NTF_PACKET roomNewUserNtfPkt;
		roomNewUserNtfPkt.userUUID = clientIndex_;
		CopyUserID(roomNewUserNtfPkt.userID, userID);

		for (auto pUser : mUserList)
		{
			if (pUser && pUser->GetNetConnIdx() != clientIndex_)
				SendPacketFunc((UINT32)pUser->GetNetConnIdx(), roomNewUserNtfPkt.PacketLength, (char*)&roomNewUserNtfPkt);
		}
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
			if (mSlots[i] != nullptr && mIsReady[i] == false)
			{
				return false; // 한 명이라도 레디 안 했으면 false
			}
		}
		return true;
	}

	// LobbyManager가 세션 만들 때 유저 정보를 빼갈 수 있게 반환
	std::vector<User*> GetRoomUserList()
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		std::vector<User*> users;
		for (auto pUser : mUserList)
		{
			if (pUser != nullptr) users.push_back(pUser);
		}
		return users;
	}

	void BroadcastPacket(UINT16 packetSize, char* pPacket)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		for (auto pTarget : mUserList)
		{
			if (pTarget != nullptr) SendPacketFunc((UINT32)pTarget->GetNetConnIdx(), packetSize, pPacket);
		}
	}

	void SendToAllUser(const UINT16 dataSize_, char* data_, const INT32 passUserIndex_, bool exceptMe)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		for (auto pUser : mUserList)
		{
			if (pUser == nullptr) continue;
			if (exceptMe && pUser->GetNetConnIdx() == passUserIndex_) continue;
			SendPacketFunc((UINT32)pUser->GetNetConnIdx(), (UINT32)dataSize_, data_);
		}
	}

	std::function<void(UINT32, UINT32, char*)> SendPacketFunc;

private:
	std::recursive_mutex mLock;
	INT32 mRoomNum = -1;
	std::list<User*> mUserList;

	INT32 mMaxUserCount = 2;
	UINT16 mCurrentUserCount = 0;

	std::vector<User*> mSlots;
	std::vector<bool> mIsReady;
	INT64 mHostUUID = -1;
};