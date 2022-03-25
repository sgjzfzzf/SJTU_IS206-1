#ifndef _MEMORY
#define _MEMORY

// The size of memory.
#define MEM_SIZE 0x10000
#define IS_HEAD(p) (p->m_addr == mem)
#define IS_TAIL(p) ((uint64)p->next->m_addr <= (uint64)p->m_addr)
typedef unsigned long uint64;

#include <stdio.h>
#include <malloc.h>

struct map
{
    // If this map is used, this value will be set to 1, else to 0.
    uint64 isused;
    // The size that map manages.
    uint64 m_size;
    // The real start address.
    char *m_addr;
    struct map *next, *prior;
};

// The whole memory.
void *mem;
// The head of map list and the latest allocated map of map list.
struct map *head, *cur;

// Initialize the memory and map list.
void minit();
// Free the memory and map list.
void mfree();
// Add node after assigned node.
struct map *insertnode(struct map *);
// Delete assigned note.
void deletenode(struct map *);
// Find the next free map since assigned map.
struct map *findfree(struct map *);
// Free the total map list.
void listfree(struct map *);
// Malloc memory.
void *lmalloc(uint64 size);
// Free memory.
void lfree(uint64 size, char *addr);
// Print the total map list.
void lprint();

#endif