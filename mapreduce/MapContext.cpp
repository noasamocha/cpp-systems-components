#include "MapContext.h"

MapContext::MapContext(IntermediateVec &threadVec)
    : _threadVec(threadVec)
{
}

void MapContext::addIntermediate(std::shared_ptr<K2> key, std::shared_ptr<V2> value)
{
    _threadVec.emplace_back(std::move(key), std::move(value));
}
