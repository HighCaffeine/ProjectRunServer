#include "RedisManager.h"


class InventoryManager
{
public:
	InventoryManager() = default;
	~InventoryManager() = default;

	bool Run(const UINT32 threadCount_)
	{
		mIsTaskRun = true;

		for (UINT32 i = 0; i < threadCount_; i++)
		{
			mTaskThreads.emplace_back([this]() { TaskProcessThread(); });
		}

		// Redis Sub 용 Thread
		mTaskThreads.emplace_back([this]() { SubscribeThread(); });

		printf("InventoryManager::Run() 정상 작동중...\n");
		return true;
	}

	void PushTask(RedisTask task_)
	{
		std::lock_guard<std::mutex> guard(mReqLock);
		mRequestTask.push_back(task_);
	}

	RedisTask TakeResponseTask()
	{
		std::lock_guard<std::mutex> guard(mResLock);

		if (mResponseTask.empty())
		{
			return RedisTask();
		}

		auto task = mResponseTask.front();
		mResponseTask.pop_front();

		return task;
	}
	RedisTask TakeRequestTask()
	{
		std::lock_guard<std::mutex> guard(mReqLock);

		if (mRequestTask.empty())
		{
			return RedisTask();
		}

		auto task = mRequestTask.front();
		mRequestTask.pop_front();

		return task;
	}


	void TaskProcessThread()
	{
		while (mIsTaskRun)
		{
			bool isIdle = true;
			if (auto task = TakeRequestTask(); task.TaskID != RedisTaskID::INVALID)
			{
				isIdle = false;
				switch (task.TaskID)
				{

				}
			}


			if (isIdle)
				std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}

	void SubscribeThread()
	{

	}



private:
	RedisInvenRes ResponsePlayerInventory(const RedisInvenReq& req);

	bool AddItemToInventory(INT32 clientIndex_, ItemID item);

	bool RemoveItemFromInventory(INT32 clientIndex_, ItemID item);

	UINT16* GetRandomItems(int count);


	bool		mIsTaskRun = false;
	std::vector<std::thread> mTaskThreads;

	std::mutex mReqLock;
	std::deque<RedisTask> mRequestTask;

	std::mutex mResLock;
	std::deque<RedisTask> mResponseTask;

}