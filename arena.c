#ifndef ARENA_C_
#define ARENA_C_

#include <stdint.h>
#include <stddef.h>

typedef struct Arena {
  uint8_t *base;
  size_t cap;
  size_t off;
} Arena;

typedef struct ArenaMark {
  Arena *a;
  size_t off;
} ArenaMark;

void arena_init(Arena *a, void *backing, size_t cap);
void *arena_alloc_align(Arena *a, size_t size, size_t align);
void *arena_alloc(Arena *a, size_t size);
void arena_reset(Arena *a);
ArenaMark arena_mark(Arena *a);
void arena_rewind(ArenaMark m);

#ifndef ARENA_IMPLEMENTATION
extern uint8_t g_buffer[];
extern Arena g_arena;
#endif

void arena_init_global(void);

#ifdef ARENA_IMPLEMENTATION

#define GB (1024ULL * 1024 * 1024)

static uint8_t g_buffer[1 * GB];
static Arena g_arena;

static inline uintptr_t align_forward_uintptr(uintptr_t p, size_t align) {
  return (p + (align - 1)) & ~(uintptr_t)(align - 1);
}

void arena_init(Arena *a, void *backing, size_t cap) {
  a->base = (uint8_t *)backing;
  a->cap = cap;
  a->off = 0;
}

void *arena_alloc_align(Arena *a, size_t size, size_t align) {
  uintptr_t cur = (uintptr_t)a->base + (uintptr_t)a->off;
  uintptr_t aligned = align_forward_uintptr(cur, align);
  size_t newOff = (size_t)(aligned - (uintptr_t)a->base) + size;
  if (newOff > a->cap)
    return NULL;
  a->off = newOff;
  return (void *)aligned;
}

void *arena_alloc(Arena *a, size_t size) {
  return arena_alloc_align(a, size, 2 * sizeof(void *));
}

void arena_reset(Arena *a) { a->off = 0; }

ArenaMark arena_mark(Arena *a) {
  ArenaMark m = {a, a->off};
  return m;
}

void arena_rewind(ArenaMark m) { m.a->off = m.off; }

void arena_init_global(void) {
  arena_init(&g_arena, g_buffer, sizeof(g_buffer));
}

#endif // ARENA_IMPLEMENTATION

#endif // ARENA_C_
