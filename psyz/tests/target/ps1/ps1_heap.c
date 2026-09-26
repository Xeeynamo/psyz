// Minimal malloc/free lib for PS1 tests, instead of the crappy BIOS one.
#include <stddef.h>

typedef struct Block {
    unsigned int size;
    int free;
    struct Block* next;
} Block;

#define ALIGN_UP(n, a) (((n) + ((a) - 1)) & ~((unsigned int)(a) - 1))
#define HDR ALIGN_UP(sizeof(Block), 8)

static Block* heap_head;
static unsigned char* heap_base;
static unsigned int heap_size;

void psyz_heap_init(void* base, unsigned int size) {
    heap_base = (unsigned char*)base;
    heap_size = size;
    heap_head = (Block*)heap_base;
    heap_head->size = size;
    heap_head->free = 1;
    heap_head->next = NULL;
}

// Merges block with any free neighbours
static void coalesce(Block* b) {
    while (b->next != NULL && b->next->free) {
        b->size += b->next->size;
        b->next = b->next->next;
    }
}

void* malloc(size_t want) {
    if (want == 0 || heap_head == NULL) {
        return NULL;
    }
    const unsigned int need = ALIGN_UP((unsigned int)want, 8) + HDR;

    for (Block* b = heap_head; b != NULL; b = b->next) {
        if (!b->free) {
            continue;
        }
        if (b->size < need) {
            coalesce(b); // a neighbour may have been freed since
            if (b->size < need) {
                continue;
            }
        }
        // split when the remainder can hold a header plus a usable payload
        if (b->size >= need + HDR + 8) {
            Block* rest = (Block*)((unsigned char*)b + need);
            rest->size = b->size - need;
            rest->free = 1;
            rest->next = b->next;
            b->size = need;
            b->next = rest;
        }
        b->free = 0;
        return (unsigned char*)b + HDR;
    }
    return NULL;
}

void free(void* p) {
    if (p == NULL) {
        return;
    }
    unsigned char* q = (unsigned char*)p;
    if (q < heap_base + HDR || q >= heap_base + heap_size) {
        return;
    }
    Block* b = (Block*)(q - HDR);
    b->free = 1;
    coalesce(b);
}

void* calloc(size_t n, size_t size) {
    const unsigned int total = (unsigned int)n * (unsigned int)size;
    unsigned char* p = (unsigned char*)malloc(total);
    if (p != NULL) {
        for (unsigned int i = 0; i < total; i++) {
            p[i] = 0;
        }
    }
    return p;
}

void* realloc(void* p, size_t want) {
    if (p == NULL) {
        return malloc(want);
    }
    if (want == 0) {
        free(p);
        return NULL;
    }
    Block* b = (Block*)((unsigned char*)p - HDR);
    const unsigned int have = b->size - HDR;
    if (have >= (unsigned int)want) {
        return p;
    }
    unsigned char* dst = (unsigned char*)malloc(want);
    if (dst == NULL) {
        return NULL;
    }
    const unsigned char* src = (const unsigned char*)p;
    for (unsigned int i = 0; i < have; i++) {
        dst[i] = src[i];
    }
    free(p);
    return dst;
}
