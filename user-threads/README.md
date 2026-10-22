# uthreads — User-Level Threads Library

A preemptive user-level threads library for Linux x86_64.

Threads are switched in user space using `sigsetjmp` / `siglongjmp`. Preemption is driven by `SIGVTALRM` from an `ITIMER_VIRTUAL` timer, and scheduling is Round-Robin.

## Interface

The library implements the following API. The header that declares it (`uthreads.h`) was provided by the course and is not included in this repository.

| Function | Description |
| --- | --- |
| `uthread_init(quantum_usecs)` | Initialize the library with a fixed quantum length. |
| `uthread_spawn(entry_point)` | Create a new thread; returns its tid (the smallest free id). |
| `uthread_terminate(tid)` | Terminate a thread (tid 0 terminates the process). |
| `uthread_block(tid)` | Move a thread to BLOCKED. |
| `uthread_resume(tid)` | Move an externally-blocked thread back to READY. |
| `uthread_sleep(num_quantums)` | Block the running thread for N quantums. |
| `uthread_get_tid()` | Tid of the calling thread. |
| `uthread_get_total_quantums()` | Total quantums since `uthread_init`. |
| `uthread_get_quantums(tid)` | Number of quantums `tid` has run. |

Limits (defined in the course header): `MAX_THREAD_NUM = 100`, `STACK_SIZE = 4096` bytes.

## Source layout

```
uthreads.cpp                 Public API; thin wrappers over the Scheduler.
Scheduler.h / Scheduler.cpp  Scheduling, timer, signal masking, state transitions.
Thread.h    / Thread.cpp     Per-thread state, stack, and saved context (sigjmp_buf).
```

The scheduler is a single global instance (`g_scheduler`) defined in `Scheduler.cpp`.

## Design notes

- **Context switch:** `switch_threads()` saves the running thread's context with `sigsetjmp` and jumps to the next thread with `siglongjmp`. A new thread's `sigjmp_buf` is prepared by hand: its stack pointer and program counter are set to the top of its own stack and to its entry point, using glibc's pointer-mangling transform (`translate_address`).
- **Atomicity:** every library call blocks `SIGVTALRM` while it changes scheduler state, so the timer can't preempt a thread in the middle of an update.
- **Blocked vs. sleeping:** a thread can be blocked by `uthread_block`, sleeping because of `uthread_sleep`, or both. The two are tracked separately (`externally_blocked`, `sleep_until`). A thread returns to READY only when both conditions are cleared.
- **Self-termination:** a thread that terminates itself is still running on its own stack, so its `Thread` object is moved to `pending_delete_` and freed later instead of right away.

## Files not included (course-provided)

- `uthreads.h`: the API declarations and constants
- Course test programs
