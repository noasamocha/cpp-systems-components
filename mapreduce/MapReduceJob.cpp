#include "MapReduceJob.h"
#include <algorithm>

namespace
{
bool intermediateKeyLess(const IntermediatePair &a, const IntermediatePair &b)
{
    return *a.first < *b.first;
}

bool outputKeyLess(const OutputPair &a, const OutputPair &b)
{
    return *a.first < *b.first;
}
}

MapReduceJob::MapReduceJob(const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel)
    : _client(client),
    _inputVec(inputVec),
    _multiThreadLevel(multiThreadLevel),
    _intermediateVecs(multiThreadLevel),
    _stateCounter(packState(UNDEFINED_STAGE, 0, 0)),
    _inputIndex(0),
    _shuffleIndex(0),
    _totalIntermediatePairs(0),
    _barrier(multiThreadLevel, [this]() noexcept { prepareShuffleStage(); }),
    _reduceBarrier(multiThreadLevel, [this]() noexcept { prepareReduceStage(); }),
    _joined(false),
    _finishedThreads(0),
    _done(false)
{
    resetState(MAP_STAGE, _inputVec.size());
    startThreads();
}

void MapReduceJob::startThreads()
{
    _threads.reserve(_multiThreadLevel);
    for (int i = 0; i < _multiThreadLevel; ++i) {
        _threads.emplace_back(&MapReduceJob::threadEntry, this, i);
    }
}

void MapReduceJob::prepareShuffleStage() noexcept
{
    uint64_t totalPairs = 0;
    for (const IntermediateVec &vec : _intermediateVecs)
    {
        totalPairs += vec.size();
    }

    _totalIntermediatePairs = totalPairs;
    resetState(SHUFFLE_STAGE, totalPairs);
}

void MapReduceJob::prepareReduceStage() noexcept
{
    resetState(REDUCE_STAGE, _totalIntermediatePairs);
}

void MapReduceJob::threadEntry(int threadId)
{
    mapPhase(threadId);
    sortPhase(threadId);

    _barrier.arrive_and_wait();

    if (threadId == 0)
    {
        shufflePhase();
    }

    _reduceBarrier.arrive_and_wait();

    reducePhase();
    markThreadDone();
}

void MapReduceJob::mapPhase(int threadId)
{
    MapContext context(_intermediateVecs[threadId]);
    uint64_t index;
    while ((index = _inputIndex.fetch_add(1)) < _inputVec.size())
    {
        const InputPair &pair = _inputVec[index];
        _client.map(pair.first, pair.second, context);
        _stateCounter.fetch_add(1);
    }
}

void MapReduceJob::sortPhase(int threadId)
{
    IntermediateVec &myVec = _intermediateVecs[threadId];
    std::sort(myVec.begin(), myVec.end(), intermediateKeyLess);
}

void MapReduceJob::reducePhase()
{
    ReduceContext reduceContext(_outputVec, _outputMutex);
    uint64_t qIndex;
    while ((qIndex = _shuffleIndex.fetch_add(1)) < _shuffleQueue.size())
    {
        const IntermediateVec &group = _shuffleQueue[qIndex];
        _client.reduce(group, reduceContext);
        _stateCounter.fetch_add(group.size());
    }
}

void MapReduceJob::markThreadDone()
{
    if (_finishedThreads.fetch_add(1) + 1 == _multiThreadLevel)
    {
        _done.store(true);
    }
}

MapReduceState MapReduceJob::getState(void) const
{
    uint64_t packed = _stateCounter.load();

    MapReduceStage stage = static_cast<MapReduceStage>(packed >> STAGE_SHIFT);
    uint64_t total       = (packed >> TOTAL_SHIFT) & COUNTER_MASK;
    uint64_t processed   = packed & COUNTER_MASK;

    double percentage = (stage != UNDEFINED_STAGE) ? 100.0 : 0.0;
    if (total > 0)
    {
        percentage = 100.0 * static_cast<double>(processed) / static_cast<double>(total);
    }

    return MapReduceState{stage, percentage};
}

void MapReduceJob::wait(void)
{
    std::lock_guard<std::mutex> lock(_waitMutex);
    if (!_joined)
    {
        for (std::thread &t : _threads)
        {
            t.join();
        }
        _joined = true;
    }
}

OutputVec MapReduceJob::getOutput(void)
{
    wait();

    std::lock_guard<std::mutex> lock(_outputMutex);
    std::sort(_outputVec.begin(), _outputVec.end(), outputKeyLess);

    return _outputVec;
}

bool MapReduceJob::isDone(void) const
{
    return _done.load();
}

MapReduceJob::~MapReduceJob()
{
    wait();
}

uint64_t MapReduceJob::packState(MapReduceStage stage, uint64_t total, uint64_t processed)
{
    return (static_cast<uint64_t>(stage) << STAGE_SHIFT)
        | (total << TOTAL_SHIFT)
        | processed;
}

void MapReduceJob::resetState(MapReduceStage stage, uint64_t total)
{
    _stateCounter.store(packState(stage, total, 0));
}

bool MapReduceJob::keysEqual(const K2 &a, const K2 &b) const
{
    return !(a < b) && !(b < a);
}

const K2 *MapReduceJob::findMaxKey() const
{
    const K2 *maxKey = nullptr;
    for (const IntermediateVec &vec : _intermediateVecs)
    {
        if (vec.empty())
        {
            continue;
        }
        const K2 *candidate = vec.back().first.get();
        if (maxKey == nullptr || *maxKey < *candidate)
        {
            maxKey = candidate;
        }
    }
    return maxKey;
}

IntermediateVec MapReduceJob::collectKey(const K2 &key)
{
    IntermediateVec sameKeyVec;
    for (IntermediateVec &vec : _intermediateVecs)
    {
        while (!vec.empty() && keysEqual(*vec.back().first, key))
        {
            sameKeyVec.push_back(vec.back());
            vec.pop_back();
        }
    }
    return sameKeyVec;
}

void MapReduceJob::shufflePhase()
{
    while (true)
    {
        const K2 *maxKey = findMaxKey();
        if (maxKey == nullptr)
        {
            break;
        }

        IntermediateVec group = collectKey(*maxKey);
        _stateCounter.fetch_add(group.size());
        _shuffleQueue.push_back(std::move(group));
    }
}