#include "VirtualMemory.h"
#include "PhysicalMemory.h"

static const uint64_t NO_FRAME = 0;
static const uint64_t ROOT_FRAME = 0;

struct FrameRef
{
    uint64_t frame = NO_FRAME;
    uint64_t parent = ROOT_FRAME;
    uint64_t offset = 0;
};

struct EvictionCandidate
{
    uint64_t page = NUM_PAGES;
    uint64_t distance = 0;
    FrameRef victim;
};

struct SearchState
{
    uint64_t page = 0;
    uint64_t forbidden = ROOT_FRAME;
    uint64_t highestFrame = ROOT_FRAME;
    FrameRef empty;
    EvictionCandidate evict;
};

static uint64_t physicalAddress(uint64_t frame, uint64_t offset)
{
    return frame * PAGE_SIZE + offset;
}

static uint64_t pagePart(uint64_t page, int depth)
{
    int shift = (TABLES_DEPTH - 1 - depth) * OFFSET_WIDTH;
    return (page >> shift) & (PAGE_SIZE - 1);
}

static uint64_t distance(uint64_t first, uint64_t second)
{
    uint64_t diff = first > second ? first - second : second - first;
    if (diff > NUM_PAGES / 2)
    {
        return NUM_PAGES - diff;
    }
    return diff;
}

static void clearFrame(uint64_t frame)
{
    for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset)
    {
        PMwrite(physicalAddress(frame, offset), 0);
    }
}

static void updateEvictionCandidate(SearchState &state, uint64_t frame,
                                     uint64_t page, uint64_t parent,
                                     uint64_t offset)
{
    uint64_t currentDistance = distance(state.page, page);
    if (currentDistance < state.evict.distance)
    {
        return;
    }
    if (currentDistance == state.evict.distance && page >= state.evict.page)
    {
        return;
    }

    state.evict.distance = currentDistance;
    state.evict.page = page;
    state.evict.victim.frame = frame;
    state.evict.victim.parent = parent;
    state.evict.victim.offset = offset;
}

static void rememberEmptyFrame(SearchState &state, uint64_t frame,
                               uint64_t parent, uint64_t offset)
{
    if (frame == ROOT_FRAME || frame == state.forbidden
        || state.empty.frame != NO_FRAME)
    {
        return;
    }

    state.empty.frame = frame;
    state.empty.parent = parent;
    state.empty.offset = offset;
}

static void scanFrames(uint64_t frame, int depth, uint64_t page,
                        uint64_t parent, uint64_t parentOffset,
                        SearchState &state)
{
    if (frame > state.highestFrame)
    {
        state.highestFrame = frame;
    }

    if (depth == TABLES_DEPTH)
    {
        updateEvictionCandidate(state, frame, page, parent, parentOffset);
        return;
    }

    bool empty = true;
    for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset)
    {
        word_t nextFrame;
        PMread(physicalAddress(frame, offset), &nextFrame);
        if (nextFrame != NO_FRAME)
        {
            empty = false;
            uint64_t nextPage = (page << OFFSET_WIDTH) + offset;
            scanFrames(nextFrame, depth + 1, nextPage, frame, offset, state);
        }
    }

    if (empty)
    {
        rememberEmptyFrame(state, frame, parent, parentOffset);
    }
}

static uint64_t findFrame(uint64_t page, uint64_t forbiddenFrame,
                           bool &frameIsClean)
{
    SearchState state;
    state.page = page;
    state.forbidden = forbiddenFrame;

    scanFrames(ROOT_FRAME, 0, 0, ROOT_FRAME, 0, state);

    frameIsClean = false;
    if (state.empty.frame != NO_FRAME)
    {
        PMwrite(physicalAddress(state.empty.parent, state.empty.offset), 0);
        frameIsClean = true;
        return state.empty.frame;
    }

    if(state.highestFrame + 1 < NUM_FRAMES)
    {
        return state.highestFrame + 1;
    }

    PMevict(state.evict.victim.frame, state.evict.page);
    PMwrite(physicalAddress(state.evict.victim.parent, state.evict.victim.offset), 0);
    return state.evict.victim.frame;
}

static uint64_t getFrame(uint64_t virtualAddress)
{
    uint64_t page = virtualAddress >> OFFSET_WIDTH;
    uint64_t frame = ROOT_FRAME;

    for (int depth = 0; depth < TABLES_DEPTH; ++depth)
    {
        uint64_t offset = pagePart(page, depth);
        uint64_t address = physicalAddress(frame, offset);

        word_t next;
        PMread(address, &next);

        if (next == NO_FRAME)
        {
            bool cleanFrame;
            uint64_t newFrame = findFrame(page, frame, cleanFrame);

            if (depth == TABLES_DEPTH - 1)
            {
                PMrestore(newFrame, page);
            }
            else if (!cleanFrame)
            {
                clearFrame(newFrame);
            }

            PMwrite(address, (word_t) newFrame);
            next = (word_t) newFrame;
        }
        frame = next;
    }
    return frame;
}

void VMinitialize()
{
    clearFrame(ROOT_FRAME);
}

int VMread(uint64_t virtualAddress, word_t *value)
{
    if (value == nullptr || virtualAddress >= VIRTUAL_MEMORY_SIZE)
    {
        return NO_FRAME;
    }

    uint64_t frame = getFrame(virtualAddress);
    PMread(physicalAddress(frame, virtualAddress & (PAGE_SIZE - 1)), value);
    return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value)
{
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
    {
        return 0;
    }

    uint64_t frame = getFrame(virtualAddress);
    PMwrite(physicalAddress(frame, virtualAddress & (PAGE_SIZE - 1)), value);
    return 1;
}

uint64_t VMgetMapping(uint64_t virtualPage)
{
    if (virtualPage >= NUM_PAGES)
    {
        return 0;
    }

    uint64_t frame = ROOT_FRAME;
    for (int depth = 0; depth < TABLES_DEPTH; ++depth)
    {
        uint64_t offset = pagePart(virtualPage, depth);

        word_t next;
        PMread(physicalAddress(frame, offset), &next);
        if (next == NO_FRAME)
        {
            return NO_FRAME;
        }
        frame = next;
    }
    return frame;
}