#include "ReduceContext.h"

ReduceContext::ReduceContext(OutputVec &outputVec, std::mutex &outputMutex)
    : _outputVec(outputVec), _outputMutex(outputMutex)
{
}

void ReduceContext::addOutput(std::shared_ptr<K3> key, std::shared_ptr<V3> value)
{
    std::lock_guard<std::mutex> lock(_outputMutex);
    _outputVec.emplace_back(std::move(key), std::move(value));
}
