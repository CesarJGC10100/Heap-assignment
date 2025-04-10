
/*
Name: Cesar Gallegos
Student ID: 1002050355
*/
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN4(s) (((((s) - 1) >> 2) << 2) + 4)
#define BLOCK_DATA(b) ((b) + 1)
#define BLOCK_HEADER(ptr) ((struct _block *)(ptr) - 1)

static int atexit_registered = 0;
static int num_mallocs = 0;
static int num_frees = 0;
static int num_reuses = 0;
static int num_grows = 0;
static int num_splits = 0;
static int num_coalesces = 0;
static int num_blocks = 0;
static int num_requested = 0;
static int max_heap = 0;

void printStatistics(void)
{
    printf("\nheap management statistics\n");
    printf("mallocs:\t\t%d\n", num_mallocs);
    printf("frees:\t\t%d\n", num_frees);
    printf("reuses:\t\t%d\n", num_reuses);
    printf("grows:\t\t%d\n", num_grows);
    printf("splits:\t\t%d\n", num_splits);
    printf("coalesces:\t%d\n", num_coalesces);
    printf("blocks:\t\t%d\n", num_blocks);
    printf("requested:\t%d\n", num_requested);
    printf("max heap:\t%d\n", max_heap);
}

struct _block
{
    size_t size; /* Size of the allocated _block of memory in bytes     */
    struct _block *next;/* Pointer to the next _block of allocated memory      */
    bool free;/* Is this _block free?                                */
    char padding[3];/* Padding: IENTRTMzMjAgU3jMDEED                       */
};

struct _block *heapList = NULL;
struct _block *last_alloc = NULL;

static int countFreeBlocks(void)
{
    int count = 0;
    struct _block *curr = heapList;
    while (curr)
    {
        if (curr->free)
        {
            count++;
        }
        curr = curr->next;
    }
    return count;
}

void coalesceFreeBlocks()
{
    struct _block *curr = heapList;
    while (curr && curr->next)
    {
        if (curr->free && curr->next->free)
        {
            curr->size += sizeof(struct _block) + curr->next->size;
            curr->next = curr->next->next;
            num_coalesces++;
        }
        else
        {
            curr = curr->next;
        }
    }
}

struct _block *findFreeBlock(struct _block **last, size_t size)
{
    struct _block *curr = heapList;

#if defined FIT && FIT == 0
    while (curr && !(curr->free && curr->size >= size))
    {
        *last = curr;
        curr = curr->next;
    }
#endif

#if defined BEST && BEST == 0
    struct _block *best = NULL;
    size_t best_size = (size_t)-1;
    while (curr)
    {
        if (curr->free && curr->size >= size && curr->size < best_size)
        {
            best = curr;
            best_size = curr->size;
        }
        *last = curr;
        curr = curr->next;
    }
    curr = best;
#endif

#if defined WORST && WORST == 0
    struct _block *worst = NULL;
    size_t worst_size = 0;
    while (curr)
    {
        if (curr->free && curr->size >= size && curr->size > worst_size)
        {
            worst = curr;
            worst_size = curr->size;
        }
        *last = curr;
        curr = curr->next;
    }
    curr = worst;
#endif

#if defined NEXT && NEXT == 0
    if (!heapList)
    {
        return NULL;
    }

    if (!last_alloc)
    {
        last_alloc = heapList;
    }

    struct _block *start = last_alloc;
    curr = NULL;

    do
    {
        if (last_alloc->free && last_alloc->size >= size)
        {
            curr = last_alloc;
            *last = last_alloc;
            break;
        }

        *last = last_alloc;
        last_alloc = last_alloc->next ? last_alloc->next : heapList;
    } while (last_alloc != start);
#endif

    return curr;
}

struct _block *growHeap(struct _block *last, size_t size)
{
    /* Request more space from OS */
    struct _block *curr = (struct _block *)sbrk(0);
    void *request = sbrk(sizeof(struct _block) + size);
    if (request == (void *)-1)
    {
        return NULL;
    }

    if (heapList == NULL)
    {
        heapList = curr;
    }

    if (last)
    {
        last->next = curr;
    }

    curr->size = size;
    curr->next = NULL;
    curr->free = false;

    num_grows++;
    max_heap = (char *)sbrk(0) - (char *)heapList;

    return curr;
}

void *malloc(size_t size)
{
    if (!atexit_registered)
    {
        atexit(printStatistics);
        atexit_registered = 1;
    }

    size = ALIGN4(size);
    if (size == 0)
    {
        return NULL;
    }

    num_requested += size;

    coalesceFreeBlocks();

    struct _block *last = heapList;
    struct _block *block = findFreeBlock(&last, size);

    if (block)
    {
        num_reuses++;

        if (block->size >= size + sizeof(struct _block) + 4)
        {
            struct _block *split = (struct _block *)((char *)BLOCK_DATA(block) + size);
            split->size = block->size - size - sizeof(struct _block);
            split->next = block->next;
            split->free = true;

            block->size = size;
            block->next = split;

            num_splits++;
        }

        block->free = false;
        last_alloc = block;
    }
    else
    {
        block = growHeap(last, size);
        if (!block)
        {
            printf("malloc: growHeap failed\n");
            return NULL;
        }
        last_alloc = block;
    }

    num_mallocs++;
    return BLOCK_DATA(block);
}

void free(void *ptr)
{
    if (!ptr)
    {
        return;
    }

    struct _block *curr = BLOCK_HEADER(ptr);
    assert(!curr->free);
    curr->free = true;

    num_frees++;
    num_blocks = countFreeBlocks();
}
void *realloc(void *ptr, size_t size)
{
    if (!ptr)
        return malloc(size);

    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    struct _block *block = BLOCK_HEADER(ptr);

    if (block->size >= size)
        return ptr;

    void *new_ptr = malloc(size);
    if (!new_ptr)
        return NULL;

    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total_size = nmemb * size;

    // Overflow check 
    if (nmemb != 0 && total_size / nmemb != size)
        return NULL;

    void *ptr = malloc(total_size);
    if (ptr)
    {
        memset(ptr, 0, total_size);
    }

    return ptr;
}

