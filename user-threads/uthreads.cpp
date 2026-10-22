#include "uthreads.h"
#include "Scheduler.h"

int uthread_init(int quantum_usecs)
{
    return g_scheduler.init(quantum_usecs);
}

int uthread_spawn(thread_entry_point entry_point)
{
    return g_scheduler.spawn(entry_point);
}

int uthread_terminate(int tid)
{
    return g_scheduler.terminate(tid);
}

int uthread_block(int tid)
{
    return g_scheduler.block(tid);
}

int uthread_resume(int tid)
{
    return g_scheduler.resume(tid);
}

int uthread_sleep(int num_quantums)
{
    return g_scheduler.sleep(num_quantums);
}

int uthread_get_tid()
{
    return g_scheduler.running_tid();
}

int uthread_get_total_quantums()
{
    return g_scheduler.total_quantums();
}

int uthread_get_quantums(int tid)
{
    return g_scheduler.quantums_of(tid);
}
