#include "Scheduler.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <new>

constexpr int MAIN_TID = 0;

static void system_error(const char *msg)
{
    std::cerr << "system error: " << msg << std::endl;
    std::exit(1);
}

static void library_error(const char *msg)
{
    std::cerr << "thread library error: " << msg << std::endl;
}

static void on_alloc_failure()
{
    system_error("memory allocation failed");
}

static void timer_handler(int)
{
    g_scheduler.on_timer_tick();
}

Scheduler g_scheduler;

int Scheduler::init(int quantum_usecs)
{
    if (initialized_)
    {
        library_error("uthread_init must be called exactly once");
        return -1;
    }
    if (quantum_usecs <= 0)
    {
        library_error("quantum_usecs must be positive");
        return -1;
    }
    initialized_ = true;
    quantum_usecs_ = quantum_usecs;
    total_quantums_ = 1;

    std::set_new_handler(&on_alloc_failure);

    threads_[MAIN_TID] = std::make_unique<Thread>();
    running_tid_ = MAIN_TID;

    sigemptyset(&signal_set_);
    sigaddset(&signal_set_, SIGVTALRM);

    struct sigaction sa{};
    sa.sa_handler = &timer_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        system_error("sigaction failed");
    }

    timer_.it_value.tv_sec = quantum_usecs / 1000000;
    timer_.it_value.tv_usec = quantum_usecs % 1000000;
    timer_.it_interval = timer_.it_value;
    arm_timer();

    if (sigsetjmp(exit_env_, 1) != 0)
    {
        std::exit(0);
    }
    return 0;
}

int Scheduler::spawn(thread_entry_point entry_point)
{
    block_signals();

    if (entry_point == nullptr)
    {
        library_error("entry_point cannot be null");
        unblock_signals();
        return -1;
    }

    int tid = find_free_tid();
    if (tid == -1)
    {
        library_error("exceeded maximum number of threads");
        unblock_signals();
        return -1;
    }

    threads_[tid] = std::make_unique<Thread>(tid, entry_point);
    ready_queue_.push_back(tid);

    unblock_signals();
    return tid;
}

int Scheduler::terminate(int tid)
{
    block_signals();
    if (!validate_tid(tid))
    {
        unblock_signals();
        return -1;
    }

    if (tid == MAIN_TID)
    {
        struct itimerval zero{};
        if (setitimer(ITIMER_VIRTUAL, &zero, nullptr) < 0)
        {
            system_error("setitimer failed");
        }

        if (running_tid_ != MAIN_TID)
        {
            pending_delete_ = std::move(threads_[running_tid_]);
            siglongjmp(exit_env_, 1);
        }
        std::exit(0);
    }

    if (tid == running_tid_)
    {
        pending_delete_ = std::move(threads_[tid]);
        switch_threads(SwitchReason::TERMINATED);
        return 0;
    }

    threads_[tid].reset();
    remove_from_ready(tid);

    unblock_signals();
    return 0;
}

int Scheduler::block(int tid)
{
    block_signals();
    if (!validate_tid(tid))
    {
        unblock_signals();
        return -1;
    }
    if (tid == MAIN_TID)
    {
        library_error("it is an error to try blocking the main thread (tid == 0).");
        unblock_signals();
        return -1;
    }

    Thread &t = *threads_[tid];
    t.externally_blocked = true;

    if (t.state == ThreadState::BLOCKED)
    {
        unblock_signals();
        return 0;
    }

    if (t.state == ThreadState::READY)
    {
        remove_from_ready(tid);
        t.state = ThreadState::BLOCKED;
        unblock_signals();
        return 0;
    }

    t.state = ThreadState::BLOCKED;
    switch_threads(SwitchReason::BLOCKED);

    unblock_signals();
    return 0;
}

int Scheduler::resume(int tid)
{
    block_signals();
    if (!validate_tid(tid))
    {
        unblock_signals();
        return -1;
    }

    Thread &t = *threads_[tid];
    if (!t.externally_blocked)
    {
        unblock_signals();
        return 0;
    }
    t.externally_blocked = false;

    if (t.sleep_until > 0)
    {
        unblock_signals();
        return 0;
    }

    t.state = ThreadState::READY;
    ready_queue_.push_back(tid);

    unblock_signals();
    return 0;
}

int Scheduler::sleep(int num_quantums)
{
    block_signals();

    if (running_tid_ == MAIN_TID && num_quantums != 0)
    {
        library_error("it is an error to try blocking the main thread (tid == 0).");
        unblock_signals();
        return -1;
    }
    if (num_quantums < 0)
    {
        library_error("num_quantums cannot be negative");
        unblock_signals();
        return -1;
    }

    if (num_quantums == 0)
    {
        switch_threads(SwitchReason::PREEMPT);
    }
    else
    {
        Thread &t = *threads_[running_tid_];
        t.sleep_until = total_quantums_ + num_quantums + 1;
        t.state = ThreadState::BLOCKED;
        switch_threads(SwitchReason::BLOCKED);
    }

    unblock_signals();
    return 0;
}

int Scheduler::quantums_of(int tid)
{
    block_signals();
    if (!validate_tid(tid))
    {
        unblock_signals();
        return -1;
    }
    int q = threads_[tid]->quantums_run;
    unblock_signals();
    return q;
}

void Scheduler::on_timer_tick()
{
    switch_threads(SwitchReason::PREEMPT);
}

// helpers

void Scheduler::switch_threads(SwitchReason reason)
{
    if (reason != SwitchReason::TERMINATED)
    {
        if (sigsetjmp(threads_[running_tid_]->env, 1) != 0)
        {
            return;
        }
        if (reason == SwitchReason::PREEMPT)
        {
            threads_[running_tid_]->state = ThreadState::READY;
            ready_queue_.push_back(running_tid_);
        }
    }

    ++total_quantums_;

    int next_tid = ready_queue_.front();
    ready_queue_.pop_front();

    wake_up_sleeping();

    running_tid_ = next_tid;
    threads_[next_tid]->state = ThreadState::RUNNING;
    ++threads_[next_tid]->quantums_run;
    arm_timer();

    if (reason != SwitchReason::TERMINATED)
    {
        unblock_signals();
    }
    siglongjmp(threads_[next_tid]->env, 1);
}

void Scheduler::wake_up_sleeping()
{
    for (auto &t : threads_)
    {
        if (!t || t->sleep_until == 0)
        {
            continue;
        }
        if (total_quantums_ >= t->sleep_until)
        {
            t->sleep_until = 0;
            if (!t->externally_blocked)
            {
                t->state = ThreadState::READY;
                ready_queue_.push_back(t->tid);
            }
        }
    }
}

int Scheduler::find_free_tid() const
{
    for (int i = 1; i < MAX_THREAD_NUM; ++i)
    {
        if (!threads_[i])
        {
            return i;
        }
    }
    return -1;
}

bool Scheduler::validate_tid(int tid)
{
    if (tid < 0 || tid >= MAX_THREAD_NUM || !threads_[tid])
    {
        std::cerr << "thread library error: Thread with tid " << tid
                  << " doesn't exist." << std::endl;
        return false;
    }
    return true;
}

void Scheduler::remove_from_ready(int tid)
{
    auto it = std::find(ready_queue_.begin(), ready_queue_.end(), tid);
    if (it != ready_queue_.end())
    {
        ready_queue_.erase(it);
    }
}

void Scheduler::arm_timer()
{
    if (setitimer(ITIMER_VIRTUAL, &timer_, nullptr) < 0)
    {
        system_error("setitimer failed");
    }
}

void Scheduler::block_signals()
{
    if (sigprocmask(SIG_BLOCK, &signal_set_, nullptr) < 0)
    {
        system_error("sigprocmask failed");
    }
}

void Scheduler::unblock_signals()
{
    if (sigprocmask(SIG_UNBLOCK, &signal_set_, nullptr) < 0)
    {
        system_error("sigprocmask failed");
    }
}