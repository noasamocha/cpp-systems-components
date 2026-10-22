# MapReduce Framework

A multithreaded MapReduce framework in C++20. A client supplies `map` and `reduce` functions. The framework splits the work across threads, sorts and groups the intermediate pairs by key, runs `reduce` on each group, and reports progress while the job runs.

## Files

```
MapReduceJob.h / .cpp    The job: thread function and the Map, Sort, Shuffle and Reduce phases.
MapContext.h / .cpp      Passed to the client's map(); addIntermediate() appends to the
                         calling thread's own intermediate vector.
ReduceContext.h / .cpp   Passed to the client's reduce(); addOutput() appends to the
                         shared output vector under a mutex.
```

## Design

The constructor creates `multiThreadLevel` threads, and the job starts right away. Each thread runs Map, then Sort, then Reduce. Thread 0 also runs Shuffle between Sort and Reduce, while the other threads wait.

### Work distribution

In Map, each thread takes input pairs using an atomic counter (`fetch_add`). Each thread gets a different index, so no mutex is needed. In Reduce, threads take shuffled vectors the same way.

In Map, each thread writes only to its own vector, so there is no shared data to protect. The only shared write is to the output vector in Reduce, and a mutex in `addOutput` protects it.

### Job state

The state has three parts: stage, total, and processed. All three are kept in one `std::atomic<uint64_t>`: 2 bits for the stage, 31 bits for total, and 31 bits for processed. Because it is a single atomic value, `getState()` reads all three parts at once and never sees a half-updated state. An atomic is used instead of a mutex because the state is read and updated very often.

### Synchronization

The framework uses two barriers, both of size `multiThreadLevel`:

- **`_barrier`:** all threads reach it after Map and Sort. The last thread to arrive runs `prepareShuffleStage()`, which counts the intermediate pairs and sets the SHUFFLE stage. So Shuffle starts only after all threads have finished Map and Sort.
- **`_reduceBarrier`:** reached after Shuffle. The last thread to arrive runs `prepareReduceStage()`, which sets the REDUCE stage.

The end of the job doesn't use a barrier. Each thread calls `markThreadDone()` after Reduce, which adds 1 to an atomic counter. The thread that brings the counter to `multiThreadLevel` sets the atomic `_done` flag.

`wait()` is thread-safe: a mutex (`_waitMutex`) and a `_joined` flag make sure the threads are joined only once, even if two threads call `wait()` at the same time. The destructor calls `wait()`, so it blocks. A job must not be destroyed before it is done, since the threads still use its fields. `getOutput()` also calls `wait()` first, then returns the output vector sorted by key.

### Memory management

All keys and values use `std::shared_ptr`, so memory is freed automatically when no one uses it anymore and there are no `new`/`delete` calls. Checked with Valgrind: no leaks and no errors.

### Progress percentage

`getState()` computes the percentage as `100 * processed / total` for the current stage:

- **Map:** input pairs done out of the input size.
- **Shuffle:** intermediate pairs moved out of the total.
- **Reduce:** pairs in the groups already reduced, out of the same total.

The Sort phase has no state of its own.

## Files not included (course-provided)

- `MapReduceClient.h`: the client interface and the `K1/V1`, `K2/V2`, `K3/V3` key/value base classes
- `MapReduceKeys.h`: shared type definitions (`IntermediateVec`, `OutputVec`, etc.)
- Sample client and tests
