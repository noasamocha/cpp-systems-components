#ifndef MAP_REDUCE_JOB_H
#define MAP_REDUCE_JOB_H

#include "MapReduceClient.h"
#include <atomic>
#include <barrier>
#include <cmath>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

enum MapReduceStage
{
	UNDEFINED_STAGE, // 0
	MAP_STAGE, // 1
	SHUFFLE_STAGE, // 2
	REDUCE_STAGE // 3
};

class MapReduceState
{
public:
	MapReduceStage stage;
	double percentage;

	inline bool operator==(const MapReduceState &other) const
	{
		return this->stage == other.stage && std::abs(this->percentage - other.percentage) < 1e-6;
	}

	inline bool operator!=(const MapReduceState &other) const
	{
		return !(*this == other);
	}
};

class MapReduceJob
{
public:
	/*
	You CAN NOT change or add properties to this part (public API).
	*/

	MapReduceJob(const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel);

	~MapReduceJob();

	MapReduceState getState(void) const;

	bool isDone(void) const;
	
	void wait(void);

	OutputVec getOutput(void);

private:
	static constexpr int STAGE_SHIFT = 62;
	static constexpr int TOTAL_SHIFT = 31;
	static constexpr uint64_t COUNTER_MASK = 0x7FFFFFFF;

	void threadEntry(int threadId);
	void startThreads();
	void mapPhase(int threadId);
	void sortPhase(int threadId);
	void reducePhase();
	void markThreadDone();
	void prepareShuffleStage() noexcept;
	void prepareReduceStage() noexcept;

	void resetState(MapReduceStage stage, uint64_t total);
	static uint64_t packState(MapReduceStage stage, uint64_t total, uint64_t processed);

	const MapReduceClient &_client;
	const InputVec &_inputVec;
	int _multiThreadLevel;

	std::vector<std::thread> _threads;

	std::vector<IntermediateVec> _intermediateVecs;
	std::vector<IntermediateVec> _shuffleQueue;
	OutputVec _outputVec;

	std::atomic<uint64_t> _stateCounter;

	std::atomic<uint64_t> _inputIndex;
	std::atomic<uint64_t> _shuffleIndex;
	uint64_t _totalIntermediatePairs;

	std::barrier<std::function<void()>> _barrier;
	std::barrier<std::function<void()>> _reduceBarrier;

	std::mutex _outputMutex;

	mutable std::mutex _waitMutex;
	bool _joined;
	std::atomic<int> _finishedThreads;
	std::atomic<bool> _done;

	bool keysEqual(const K2 &a, const K2 &b) const;

	void shufflePhase();

	const K2 *findMaxKey() const;
	IntermediateVec collectKey(const K2 &key);
};
	
#endif // MAP_REDUCE_JOB_H
