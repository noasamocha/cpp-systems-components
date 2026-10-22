#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "Thread.h"
#include "uthreads.h"
#include <array>
#include <deque>
#include <memory>
#include <signal.h>
#include <sys/time.h>

enum class SwitchReason { PREEMPT, BLOCKED, TERMINATED };

class Scheduler
{
public:
    Scheduler() = default;

    int init(int quantum_usecs);
    int spawn(thread_entry_point entry_point);
    int terminate(int tid);
    int block(int tid);
    int resume(int tid);
    int sleep(int num_quantums);
    int quantums_of(int tid);

    int running_tid() const { return running_tid_; }
    int total_quantums() const { return total_quantums_; }

    void on_timer_tick();

private:
    void switch_threads(SwitchReason reason);
    void wake_up_sleeping();
    int find_free_tid() const;
    bool validate_tid(int tid);
    void remove_from_ready(int tid);
    void arm_timer();

    void block_signals();
    void unblock_signals();

    bool initialized_{false};
    int quantum_usecs_{0};
    int running_tid_{0};
    int total_quantums_{0};
    std::array<std::unique_ptr<Thread>, MAX_THREAD_NUM> threads_{};
    std::deque<int> ready_queue_{};
    std::unique_ptr<Thread> pending_delete_{};
    sigset_t signal_set_{};
    struct itimerval timer_{};
    sigjmp_buf exit_env_{};
};

extern Scheduler g_scheduler;

#endif
