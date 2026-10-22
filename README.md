# Operating Systems Projects (C++)

Three system components I implemented in C++ as part of a university Operating Systems course:

| Project | What it is | Key techniques |
| --- | --- | --- |
| [`user-threads/`](user-threads) | A user-level threads library with preemptive scheduling | `sigsetjmp`/`siglongjmp` context switches, Round-Robin scheduling driven by `SIGVTALRM`, signal masking for atomicity |
| [`mapreduce/`](mapreduce) | A multithreaded MapReduce framework | `std::thread`, `std::barrier`, lock-free work distribution with atomics, a packed 64-bit atomic job state |
| [`virtual-memory/`](virtual-memory) | A virtual memory system with hierarchical page tables | Arbitrary-depth page-table trees stored in simulated physical memory, no globals or dynamic allocation, cyclic-distance eviction |

## What is (and isn't) in this repository

This repository contains **only the code I wrote**.

Each project implements an API defined by header files that the course staff provided, and links against support code from the course: a client interface for MapReduce and a physical-memory simulator for the virtual memory project. **Those course-provided files are not included here**, so the code does not build on its own. Each project's README describes the interface it implements and lists the files that are missing.

## Requirements

Linux x86_64, C++ (the MapReduce project uses C++20 for `std::barrier`).
