#include <stdint.h>
#include <stdlib.h>

typedef struct block {
  uint32_t size;
  uint32_t free;
  struct block* next;
} block_t;

extern uint8_t heap_start[];
extern uint8_t heap_end[];

static block_t* heap = (block_t*)0;

void heap_init() {
  heap = (block_t*)heap_start;
  heap->size = (heap_end - heap_start) - sizeof(block_t);
  heap->free = 1;
  heap->next = (block_t*)0;
}

void* alloc(uint32_t size) {
  block_t* curr = heap;
  
  while (curr) {
    if (curr->free && curr->size >= size) {
      if (curr->size > size + sizeof(block_t)) {
        block_t* new_block = (block_t*)((uint8_t*)curr + sizeof(block_t) + size);

        new_block->size = curr->size - size - sizeof(block_t);
        new_block->free = 1;
        new_block->next = curr->next;

        curr->next = new_block;
        curr->size = size;
      }

      curr->free = 0;
      return (uint8_t*)curr + sizeof(block_t);
    }

    curr = curr->next;
  }

  return (void*)0;
}

void free(void* ptr) {
  if (!ptr) return;

  block_t* block = (block_t*)((uint8_t*)ptr - sizeof(block_t));
  block->free = 1;
}