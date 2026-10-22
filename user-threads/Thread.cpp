#include "Thread.h"
#include <signal.h>

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

static address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
                 : "=g"(ret)
                 : "0"(addr));
    return ret;
}

Thread::Thread()
    : tid(0),
      state(ThreadState::RUNNING),
      stack(nullptr),
      quantums_run(1),
      sleep_until(0),
      externally_blocked(false)
{
}

Thread::Thread(int tid, thread_entry_point entry_point)
    : tid(tid),
      state(ThreadState::READY),
      stack(std::make_unique<char[]>(STACK_SIZE)),
      quantums_run(0),
      sleep_until(0),
      externally_blocked(false)
{
    address_t sp = (address_t)stack.get() + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t)entry_point;

    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address(sp);
    (env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);
}
