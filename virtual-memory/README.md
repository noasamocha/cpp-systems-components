# Virtual Memory — Hierarchical Page Tables

A virtual memory library that uses hierarchical page tables of arbitrary depth over simulated physical memory, with no global variables and no dynamic allocation. The page tables themselves live in frames of the simulated RAM. The root table is always in frame 0, and a table entry of 0 means "not present".

## Interface

`VirtualMemory.cpp` implements:

| Function | Description |
| --- | --- |
| `VMinitialize()` | Clear the root table (frame 0). |
| `VMread(virtualAddress, value)` | Read a word, swapping pages in as needed. |
| `VMwrite(virtualAddress, value)` | Write a word, swapping pages in as needed. |
| `VMgetMapping(virtualPage)` | The frame currently holding a page, or 0 if it isn't resident. |

## Design

**Translation:** `getFrame` translates an address by walking from the root (frame 0) down to the leaf, one level per `OFFSET_WIDTH` bits. On a page fault it gets a frame, links it to its parent, and continues. Leaves are restored with `PMrestore`; table frames are zeroed.

**Choosing a frame:** `findFrame` picks a frame with a single DFS over the tree, applying three priorities:

1. An empty table frame. It is unlinked from its parent and reused.
2. An unused frame, found from the largest frame index referenced anywhere in the tree.
3. Eviction of the page with the maximal cyclical distance from the incoming page. Ties go to the lowest page number.

The frame filled one level up in the current translation is protected from reuse.

**Bit manipulation:** all of it is isolated in small helpers (`physicalAddress`, `pagePart`, `distance`) and depends only on the memory constants. So the code supports any memory size and tree depth.

## Files not included (course-provided)

- `VirtualMemory.h`: the API declarations
- `PhysicalMemory.h` / `PhysicalMemory.cpp`: the simulated physical memory (`PMread`, `PMwrite`, `PMevict`, `PMrestore`)
- `MemoryConstants.h`: memory sizes and derived constants (`PAGE_SIZE`, `NUM_FRAMES`, `TABLES_DEPTH`, ...)
