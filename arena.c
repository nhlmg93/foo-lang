#ifndef FOO_ARENA_H
#define FOO_ARENA_H

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void *arena_alloc(size_t size);
void *arena_realloc(void *ptr, size_t size);

#endif

#ifdef ARENA_IMPLEMENTATION

#ifndef PANIC
#error "arena.c requires PANIC(condition, message...)"
#endif

#define ARENA_MEMORY_CAPACITY ((size_t)16 * 1024 * 1024)
#define ARENA_ALIGNMENT alignof(max_align_t)
#define ARENA_HEADER_SIZE                                                    \
  ((sizeof(size_t) + ARENA_ALIGNMENT - 1) / ARENA_ALIGNMENT * ARENA_ALIGNMENT)

static alignas(max_align_t) uint8_t arena_memory[ARENA_MEMORY_CAPACITY];
static size_t arena_memory_used;

static size_t arena_align_forward(size_t value) {
  const size_t remainder = value % ARENA_ALIGNMENT;
  return remainder == 0 ? value : value + ARENA_ALIGNMENT - remainder;
}

void *arena_alloc(size_t size) {
  const size_t header_offset = arena_align_forward(arena_memory_used);
  const size_t data_offset = header_offset + ARENA_HEADER_SIZE;

  PANIC(header_offset <= ARENA_MEMORY_CAPACITY &&
            ARENA_HEADER_SIZE <= ARENA_MEMORY_CAPACITY - header_offset &&
            size <= ARENA_MEMORY_CAPACITY - data_offset,
        "static arena exhausted (requested=%zu, used=%zu, capacity=%zu)", size,
        arena_memory_used, ARENA_MEMORY_CAPACITY);

  size_t *const allocation_size = (size_t *)&arena_memory[header_offset];
  *allocation_size = size;
  arena_memory_used = data_offset + size;
  return &arena_memory[data_offset];
}

void *arena_realloc(void *ptr, size_t size) {
  if (ptr == NULL)
    return arena_alloc(size);
  if (size == 0)
    return NULL;

  uint8_t *const old_data = ptr;
  size_t *const old_size_ptr = (size_t *)(old_data - ARENA_HEADER_SIZE);
  const size_t old_size = *old_size_ptr;
  const size_t old_end = (size_t)(old_data - arena_memory) + old_size;

  if (old_end == arena_memory_used) {
    const size_t data_offset = (size_t)(old_data - arena_memory);
    PANIC(size <= ARENA_MEMORY_CAPACITY - data_offset,
          "static arena exhausted (requested=%zu, used=%zu, capacity=%zu)",
          size, arena_memory_used, ARENA_MEMORY_CAPACITY);

    *old_size_ptr = size;
    arena_memory_used = data_offset + size;
    return ptr;
  }

  if (size <= old_size) {
    *old_size_ptr = size;
    return ptr;
  }

  void *const new_ptr = arena_alloc(size);
  memcpy(new_ptr, ptr, old_size);
  return new_ptr;
}

#endif
