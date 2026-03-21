#pragma once

#include "Packet.h"
#include "NavMeshManager.h"

#include <string>

class Actor
{
public:
	enum class DOMAIN_STATE
	{
		NONE = 0,
		LOGIN = 1,
		ROOM = 2
	};

	enum class ModifierType { ADD, MULTIPLY, FLAT };

	struct SpeedModifier
	{
		ModifierType type;
		float value;
		float duration; // 남은 시간 (초)
		int sourceID;   // 중복 적용 방지용 (예: 같은 스킬 슬로우 중첩 불가)
	};


	Actor() = default;
	~Actor() = default;

	void Init(const INT32 index)
	{
		mIndex = index;
		position.x = 0.0f;
		position.y = -1.0f;
		position.z = 0.0f;

		serverPos.x = 0.0f;
		serverPos.y = -1.0f;
		serverPos.z = 0.0f;

		targetPos = serverPos;
		inputX = 0.0f;
		inputZ = 0.0f;
		isMoving = false;

		mForceDuration = 0.0f;
		mForceVelocity = { 0.0f, 0.0f, 0.0f };
		mSpeedModifiers.clear();
	}

	void Clear()
	{
		mRoomIndex = -1;
		mUserID = "";
		mIsConfirm = false;
		mCurDomainState = DOMAIN_STATE::NONE;
	}

	int SetLogin(const char* userID_)
	{
		mCurDomainState = DOMAIN_STATE::LOGIN;
		mUserID = userID_;

		return 0;
	}

	void EnterRoom(INT32 roomIndex_)
	{
		mRoomIndex = roomIndex_;
		mCurDomainState = DOMAIN_STATE::ROOM;
	}

	void SetDomainState(DOMAIN_STATE value_) { mCurDomainState = value_; }

	INT32 GetCurrentRoom()
	{
		return mRoomIndex;
	}

	INT32 GetNetConnIdx()
	{
		return mIndex;
	}

	std::string GetUserId() const
	{
		return  mUserID;
	}

	DOMAIN_STATE GetDomainState()
	{
		return mCurDomainState;
	}

	const UINT32& GetLastInputSeq() const { return lastInputSeq; }
	const bool& GetIsMoving() const { return isMoving; }
	const Vector3& GetPosition() const { return serverPos; }
	const Quaternion& GetRotation() const { return rotation; }

	//임시 테스트 함수
	Vector3 UpdateMovement(float dx, float dy, Quaternion& rotation_)
	{
		const float SPEED = 20.0f;

		dx *= (dx <= 1.0f);
		dy *= (dy <= 1.0f);

		// same as the client-sided calculation
		Vector3 right = Quaternion_Multiply(rotation_, Vector3_right());
		Vector3 forward = Quaternion_Multiply(rotation_, Vector3_forward());
		Vector3 mx = Vector3_Multiply(right, dx);
		Vector3 my = Vector3_Multiply(forward, dy);
		Vector3 motion = Vector3_Addition(mx, my);
		motion = Vector3_Multiply(motion, FIXED_DELTA_TIME * SPEED);


		position = Vector3_Addition(position, motion);
		rotation = rotation_;

		return motion;
	}


	//마우스 이동 설정
	void SetTarget(Vector3& clickedPos, UINT32 seq)
	{
		targetPos = clickedPos;
		lastInputSeq = seq;
		isMoving = true;
	}

	// WASD 이동 설정
	void SetInput(float dx, float dy, UINT32 seq)
	{
		inputX = dx;
		inputZ = dy;
		lastInputSeq = seq;
	}

	//속도 모디파이어
	std::vector<SpeedModifier> mSpeedModifiers;

	void AddSpeedModifier(ModifierType type, float value, float duration)
	{
		mSpeedModifiers.push_back({ type, value, duration });
	}

	// 매 틱마다 최종 속도 계산
	float GetCurrentSpeed()
	{
		float addValue = 0.0f;
		float multiplier = 1.0f;

		for (auto& mod : mSpeedModifiers)
		{
			if (mod.type == ModifierType::ADD) addValue += mod.value;
			else if (mod.type == ModifierType::MULTIPLY) multiplier *= mod.value;
		}

		// (기본 속도 + 합연산) * 곱연산
		return (BASE_SPEED + addValue) * multiplier;
	}

	// 20ms 틱마다 호출
	//void UpdateServerPhysics(float dt, bool isMoveMouse = false)
	//{
	//	if (inputX == 0.0f && inputZ == 0.0f)
	//	{
	//		isMoving = false;
	//		return;
	//	}
	//	for (auto it = mSpeedModifiers.begin(); it != mSpeedModifiers.end(); )
	//	{
	//		it->duration -= dt;
	//		if (it->duration <= 0.0f)
	//		{
	//			it = mSpeedModifiers.erase(it);
	//		}
	//		else
	//		{
	//			++it;
	//		}
	//	}
	//	const float currentSpeed = GetCurrentSpeed();
	//	Vector3 currentPos = GetPosition();	//현재 서버 좌표
	//	Vector3 nextPos = currentPos;		//목표 좌표
	//	if (isMoveMouse)
	//	{
	//		if (!isMoving) return;
	//		Vector3 dir = { targetPos.x - nextPos.x, 0, targetPos.z - nextPos.z };
	//		float dist = sqrt(dir.x * dir.x + dir.z * dir.z);
	//		if (dist < 0.1f)
	//		{
	//			nextPos = targetPos;
	//			isMoving = false;
	//			return;
	//		}
	//		isMoving = true;
	//		dir.x /= dist; dir.z /= dist;
	//		nextPos.x += dir.x * currentSpeed * dt;
	//		nextPos.z += dir.z * currentSpeed * dt;
	//	}
	//	else
	//	{
	//		if (inputX == 0 && inputZ == 0)
	//		{
	//			isMoving = false;
	//			return;
	//		}
	//		isMoving = true;
	//		float mag = sqrt(inputX * inputX + inputZ * inputZ);
	//		if (mag > 0.01f)
	//		{
	//			float dirX = inputX / mag;
	//			float dirZ = inputZ / mag;
	//			serverPos.x += dirX * currentSpeed * dt;
	//			serverPos.z += dirZ * currentSpeed * dt;
	//		}
	//	}
	//	if (mForceDuration > 0.0f)
	//	{
	//		nextPos.x += mForceVelocity.x * dt;
	//		nextPos.z += mForceVelocity.z * dt;
	//		mForceDuration -= dt;
	//		isMoving = true;
	//		// 날아가다가 멈추는 시점
	//		if (mForceDuration <= 0.0f)
	//		{
	//			mForceDuration = 0.0f;
	//			if (mIsCube && mObstacleRef == 0) 
	//			{
	//				mObstacleRef = NavMeshManager::GetInstance()->AddObstacle(serverPos, 1.0f, 2.0f);
	//			}
	//		}
	//	}
	//	Vector3 realPos;
	//	if (NavMeshManager::GetInstance()->GetValidMovePosition(currentPos, nextPos, realPos))
	//	{
	//		serverPos = realPos;
	//	}
	//	else
	//	{
	//		serverPos = nextPos;
	//	}
	//}

	// 20ms 틱마다 호출
	void UpdateServerPhysics(float dt, bool isMoveMouse = false)
	{
		for (auto it = mSpeedModifiers.begin(); it != mSpeedModifiers.end(); )
		{
			it->duration -= dt;
			if (it->duration <= 0.0f) it = mSpeedModifiers.erase(it);
			else ++it;
		}

		const float currentSpeed = GetCurrentSpeed();
		Vector3 currentPos = GetPosition();
		Vector3 nextPos = currentPos;

		bool hasInputMove = false;

		// 키보드/마우스 입력 이동
		if (isMoveMouse)
		{
			Vector3 dir = { targetPos.x - nextPos.x, 0, targetPos.z - nextPos.z };
			float dist = sqrt(dir.x * dir.x + dir.z * dir.z);
			if (dist > 0.1f)
			{
				dir.x /= dist; dir.z /= dist;
				nextPos.x += dir.x * currentSpeed * dt;
				nextPos.z += dir.z * currentSpeed * dt;
				hasInputMove = true;
			}
		}
		else
		{
			if (inputX != 0.0f || inputZ != 0.0f)
			{
				float mag = sqrt(inputX * inputX + inputZ * inputZ);
				if (mag > 0.01f)
				{
					float dirX = inputX / mag;
					float dirZ = inputZ / mag;
					nextPos.x += dirX * currentSpeed * dt;
					nextPos.z += dirZ * currentSpeed * dt;
					hasInputMove = true;
				}
			}
		}

		// 외부 물리력 (밀치기/당기기) 강제 이동
		bool hasForceMove = false;
		if (mForceDuration > 0.0f)
		{
			nextPos.x += mForceVelocity.x * dt;
			nextPos.z += mForceVelocity.z * dt;
			mForceDuration -= dt;
			hasForceMove = true;

			if (mForceDuration <= 0.0f)
			{
				mForceDuration = 0.0f;
				if (mIsCube && mObstacleRef == 0)
				{
					mObstacleRef = NavMeshManager::GetInstance()->AddObstacle(serverPos, 1.0f, 2.0f);
				}
			}
		}

		isMoving = (hasInputMove || hasForceMove);
		if (!isMoving) return;

		Vector3 realPos;
		if (NavMeshManager::GetInstance()->GetValidMovePosition(currentPos, nextPos, realPos))
		{
			serverPos = realPos;
		}
		else
		{
			mForceDuration = 0.0f;
		}
	}

	bool mIsCube = false;
	dtObstacleRef mObstacleRef = 0;
	Vector3 mForceVelocity = { 0.0f, 0.0f, 0.0f };
	float mForceDuration = 0.0f;

	// 위치 강제 세팅
	void SetPosition(Vector3 pos) 
	{
		serverPos = pos;
		position = pos;
	}

	// 자력 적용
	void ApplyForce(Vector3 dir, float power, float duration)
	{
		float len = sqrt(dir.x * dir.x + dir.z * dir.z);
		if (len > 0.0f) { dir.x /= len; dir.z /= len; }

		mForceVelocity.x = dir.x * power;
		mForceVelocity.z = dir.z * power;
		mForceDuration = duration;
		isMoving = true;

		// 길을 막지 않도록 임시 해제
		if (mIsCube && mObstacleRef != 0) 
		{
			NavMeshManager::GetInstance()->RemoveObstacle(mObstacleRef);
			mObstacleRef = 0;
		}
	}

private:
#pragma region Move
	bool isMoving = false;
	Vector3 serverPos;   // 서버 확정 현재 위치
	Vector3 targetPos;   // 마우스로 클릭된 최종 목적지

	float inputX = 0, inputZ = 0;

	INT32 lastInputSeq;	//보정용 번호

	//임시 함수용 변수
	// position of the player
	Vector3 position;

	// rotation of the player
	Quaternion rotation;
#pragma endregion

	INT32 mIndex = -1;
	INT32 mRoomIndex = -1;

	std::string mUserID;
	bool mIsConfirm = false;
	std::string mAuthToken;

	DOMAIN_STATE mCurDomainState = DOMAIN_STATE::NONE;

};

