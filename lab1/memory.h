#ifndef _MEMORY
#define _MEMORY

#define MEM_SIZE 0x10000
#define IS_HEAD(p) (p->m_addr == mem)
#define IS_TAIL(p) ((uint64)p->next->m_addr <= (uint64)p->m_addr)
typedef unsigned long uint64;

#include <stdio.h>
#include <malloc.h>

struct map
{
    uint64 isused;
    uint64 m_size;
    char *m_addr;
    struct map *next, *prior;
};

void minit();
void mfree();
struct map *insertnode(struct map *);
void deletenode(struct map *);
struct map *findfree(struct map *);
void listfree(struct map *);
void *lmalloc(uint64 size);
void lfree(uint64 size, char *addr);
void lprint();

#endif