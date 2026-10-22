#ifndef REDUCE_CONTEXT_H
#define REDUCE_CONTEXT_H

#include "MapReduceKeys.h"
#include <mutex>

class ReduceContext
{
public:
    ReduceContext(OutputVec &outputVec, std::mutex &outputMutex);

    void addOutput(std::shared_ptr<K3> key, std::shared_ptr<V3> value);

    /*
    You can change everything else, including the constructor/desturctor
    You can also add fields here (even public ones)
    */
private:
    OutputVec &_outputVec;
    std::mutex &_outputMutex;
};

#endif // REDUCE_CONTEXT_H
