#ifndef THREAD_H
#define THREAD_H

#include "uthreads.h"
#include <setjmp.h>
#include <memory>

enum class ThreadState { READY, RUNNING, BLOCKED };

class Thread
{
public:
    Thread();
    Thread(int tid, thread_entry_point entry_point);

private:
    friend class Scheduler;

    int tid;
    ThreadState state;
    std::unique_ptr<char[]> stack;
    int quantums_run;
    int sleep_until;
    bool externally_blocked;
    sigjmp_buf env;
};

#endif
