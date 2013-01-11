/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define C_LUCY_SORTEXTERNAL
#include "Lucy/Util/ToolSet.h"

#include "Lucy/Util/SortExternal.h"

// Refill the main cache, drawing from the caches of all runs.
static void
S_refill_cache(SortExternal *self);

// Absorb all the items which are "in-range" from all the Runs into the main
// cache.
static void
S_absorb_slices(SortExternal *self, Obj **endpost);

// Return the address for the item in one of the runs' caches which is the
// highest in sort order, but which we can guarantee is lower in sort order
// than any item which has yet to enter a run cache.
static Obj**
S_find_endpost(SortExternal *self);

// Determine how many buffered items are less than or equal to `endpost`.
static uint32_t
S_find_slice_size(SortExternal *self, Obj **endpost);

SortExternal*
SortEx_init(SortExternal *self) {
    self->mem_thresh   = UINT32_MAX;
    self->buffer       = NULL;
    self->buf_cap      = 0;
    self->buf_max      = 0;
    self->buf_tick     = 0;
    self->scratch      = NULL;
    self->scratch_cap  = 0;
    self->runs         = VA_new(0);
    self->slice_sizes  = NULL;
    self->slice_starts = NULL;
    self->num_slices   = 0;
    self->flipped      = false;

    ABSTRACT_CLASS_CHECK(self, SORTEXTERNAL);
    return self;
}

void
SortEx_destroy(SortExternal *self) {
    FREEMEM(self->scratch);
    FREEMEM(self->slice_sizes);
    FREEMEM(self->slice_starts);
    if (self->buffer) {
        SortEx_Clear_Buffer(self);
        FREEMEM(self->buffer);
    }
    DECREF(self->runs);
    SUPER_DESTROY(self, SORTEXTERNAL);
}

void
SortEx_clear_buffer(SortExternal *self) {
    Obj **const buffer = self->buffer;
    for (uint32_t i = self->buf_tick, max = self->buf_max; i < max; i++) {
        DECREF(buffer[i]);
    }
    self->buf_max  = 0;
    self->buf_tick = 0;
}

void
SortEx_feed(SortExternal *self, Obj *item) {
    if (self->buf_max == self->buf_cap) {
        size_t amount = Memory_oversize(self->buf_max + 1, sizeof(Obj*));
        SortEx_Grow_Buffer(self, amount);
    }
    self->buffer[self->buf_max] = item;
    self->buf_max++;
}

static INLINE Obj*
SI_peek(SortExternal *self) {
    if (self->buf_tick >= self->buf_max) {
        S_refill_cache(self);
    }

    if (self->buf_max > 0) {
        return self->buffer[self->buf_tick];
    }
    else {
        return NULL;
    }
}

Obj*
SortEx_fetch(SortExternal *self) {
    Obj *item = SI_peek(self);
    self->buf_tick++;
    return item;
}

Obj*
SortEx_peek(SortExternal *self) {
    return SI_peek(self);
}

void
SortEx_sort_buffer(SortExternal *self) {
    if (self->buf_tick != 0) {
        THROW(ERR, "Cant Sort_Buffer() after fetching %u32 items", self->buf_tick);
    }
    if (self->buf_max != 0) {
        VTable *vtable = SortEx_Get_VTable(self);
        Lucy_Sort_Compare_t compare
            = (Lucy_Sort_Compare_t)METHOD_PTR(vtable, Lucy_SortEx_Compare);
        if (self->scratch_cap < self->buf_cap) {
            self->scratch_cap = self->buf_cap;
            self->scratch = (Obj**)REALLOCATE(
                                self->scratch,
                                self->scratch_cap * sizeof(Obj*));
        }
        Sort_mergesort(self->buffer, self->scratch, self->buf_max,
                       sizeof(Obj*), compare, self);
    }
}

void
SortEx_flip(SortExternal *self) {
    SortEx_Flush(self);
    self->flipped = true;
}

void
SortEx_add_run(SortExternal *self, SortExternal *run) {
    VA_Push(self->runs, (Obj*)run);
    uint32_t num_runs = VA_Get_Size(self->runs);
    self->slice_sizes = (uint32_t*)REALLOCATE(
                            self->slice_sizes,
                            num_runs * sizeof(uint32_t));
    self->slice_starts = (Obj***)REALLOCATE(
                             self->slice_starts,
                             num_runs * sizeof(Obj**));
}

void
SortEx_shrink(SortExternal *self) {
    if (self->buf_max - self->buf_tick > 0) {
        size_t buf_count = SortEx_Buffer_Count(self);
        size_t size        = buf_count * sizeof(Obj*);
        if (self->buf_tick > 0) {
            Obj **start = self->buffer + self->buf_tick;
            memmove(self->buffer, start, size);
        }
        self->buffer   = (Obj**)REALLOCATE(self->buffer, size);
        self->buf_tick = 0;
        self->buf_max  = buf_count;
        self->buf_cap  = buf_count;
    }
    else {
        FREEMEM(self->buffer);
        self->buffer   = NULL;
        self->buf_tick = 0;
        self->buf_max  = 0;
        self->buf_cap  = 0;
    }
    self->scratch_cap = 0;
    FREEMEM(self->scratch);
    self->scratch = NULL;

    for (uint32_t i = 0, max = VA_Get_Size(self->runs); i < max; i++) {
        SortExternal *run = (SortExternal*)VA_Fetch(self->runs, i);
        SortEx_Shrink(run);
    }
}

static void
S_refill_cache(SortExternal *self) {
    // Reset cache vars.
    SortEx_Clear_Buffer(self);

    // Make sure all runs have at least one item in the cache.
    uint32_t i = 0;
    while (i < VA_Get_Size(self->runs)) {
        SortExternal *const run = (SortExternal*)VA_Fetch(self->runs, i);
        if (SortEx_Buffer_Count(run) > 0 || SortEx_Refill(run) > 0) {
            i++; // Run has some elements, so keep.
        }
        else {
            VA_Excise(self->runs, i, 1);
        }
    }

    // Absorb as many elems as possible from all runs into main cache.
    if (VA_Get_Size(self->runs)) {
        Obj **endpost = S_find_endpost(self);
        S_absorb_slices(self, endpost);
    }
}

static Obj**
S_find_endpost(SortExternal *self) {
    Obj **endpost = NULL;

    for (uint32_t i = 0, max = VA_Get_Size(self->runs); i < max; i++) {
        // Get a run and retrieve the last item in its cache.
        SortExternal *const run = (SortExternal*)VA_Fetch(self->runs, i);
        const uint32_t tick = run->buf_max - 1;
        if (tick >= run->buf_cap || run->buf_max < 1) {
            THROW(ERR, "Invalid SortExternal cache access: %u32 %u32 %u32", tick,
                  run->buf_max, run->buf_cap);
        }
        else {
            // Cache item with the highest sort value currently held in memory
            // by the run.
            Obj **candidate = run->buffer + tick;

            // If it's the first run, item is automatically the new endpost.
            if (i == 0) {
                endpost = candidate;
            }
            // If it's less than the current endpost, it's the new endpost.
            else if (SortEx_Compare(self, candidate, endpost) < 0) {
                endpost = candidate;
            }
        }
    }

    return endpost;
}

static void
S_absorb_slices(SortExternal *self, Obj **endpost) {
    uint32_t    num_runs     = VA_Get_Size(self->runs);
    Obj      ***slice_starts = self->slice_starts;
    uint32_t   *slice_sizes  = self->slice_sizes;
    VTable     *vtable       = SortEx_Get_VTable(self);
    Lucy_Sort_Compare_t compare
        = (Lucy_Sort_Compare_t)METHOD_PTR(vtable, Lucy_SortEx_Compare);

    if (self->buf_max != 0) { THROW(ERR, "Can't refill unless empty"); }

    // Move all the elements in range into the main cache as slices.
    for (uint32_t i = 0; i < num_runs; i++) {
        SortExternal *const run = (SortExternal*)VA_Fetch(self->runs, i);
        uint32_t slice_size = S_find_slice_size(run, endpost);

        if (slice_size) {
            // Move slice content from run cache to main cache.
            if (self->buf_max + slice_size > self->buf_cap) {
                size_t cap = Memory_oversize(self->buf_max + slice_size,
                                             sizeof(Obj*));
                SortEx_Grow_Buffer(self, cap);
            }
            memcpy(self->buffer + self->buf_max,
                   run->buffer + run->buf_tick,
                   slice_size * sizeof(Obj*));
            run->buf_tick += slice_size;
            self->buf_max += slice_size;

            // Track number of slices and slice sizes.
            slice_sizes[self->num_slices++] = slice_size;
        }
    }

    // Transform slice starts from ticks to pointers.
    uint32_t total = 0;
    for (uint32_t i = 0; i < self->num_slices; i++) {
        slice_starts[i] = self->buffer + total;
        total += slice_sizes[i];
    }

    // The main cache now consists of several slices.  Sort the main cache,
    // but exploit the fact that each slice is already sorted.
    if (self->scratch_cap < self->buf_cap) {
        self->scratch_cap = self->buf_cap;
        self->scratch = (Obj**)REALLOCATE(
                            self->scratch, self->scratch_cap * sizeof(Obj*));
    }

    // Exploit previous sorting, rather than sort cache naively.
    // Leave the first slice intact if the number of slices is odd. */
    while (self->num_slices > 1) {
        uint32_t i = 0;
        uint32_t j = 0;

        while (i < self->num_slices) {
            if (self->num_slices - i >= 2) {
                // Merge two consecutive slices.
                const uint32_t merged_size = slice_sizes[i] + slice_sizes[i + 1];
                Sort_merge(slice_starts[i], slice_sizes[i],
                           slice_starts[i + 1], slice_sizes[i + 1], self->scratch,
                           sizeof(Obj*), compare, self);
                slice_sizes[j]  = merged_size;
                slice_starts[j] = slice_starts[i];
                memcpy(slice_starts[j], self->scratch, merged_size * sizeof(Obj*));
                i += 2;
                j += 1;
            }
            else if (self->num_slices - i >= 1) {
                // Move single slice pointer.
                slice_sizes[j]  = slice_sizes[i];
                slice_starts[j] = slice_starts[i];
                i += 1;
                j += 1;
            }
        }
        self->num_slices = j;
    }

    self->num_slices = 0;
}

void
SortEx_grow_buffer(SortExternal *self, uint32_t size) {
    if (size > self->buf_cap) {
        self->buffer = (Obj**)REALLOCATE(self->buffer, size * sizeof(Obj*));
        self->buf_cap = size;
    }
}

static uint32_t
S_find_slice_size(SortExternal *self, Obj **endpost) {
    int32_t          lo     = self->buf_tick - 1;
    int32_t          hi     = self->buf_max;
    Obj            **buffer = self->buffer;
    SortEx_Compare_t compare
        = METHOD_PTR(SortEx_Get_VTable(self), Lucy_SortEx_Compare);

    // Binary search.
    while (hi - lo > 1) {
        const int32_t mid   = lo + ((hi - lo) / 2);
        const int32_t delta = compare(self, buffer + mid, endpost);
        if (delta > 0) { hi = mid; }
        else           { lo = mid; }
    }

    // If lo is still -1, we didn't find anything.
    return lo == -1
           ? 0
           : (lo - self->buf_tick) + 1;
}

void
SortEx_set_mem_thresh(SortExternal *self, uint32_t mem_thresh) {
    self->mem_thresh = mem_thresh;
}

uint32_t
SortEx_buffer_count(SortExternal *self) {
    return self->buf_max - self->buf_tick;
}


