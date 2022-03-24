#ifndef _MEMORY
#define _MEMORY

#define MEM_SIZE 0x10000
#define IS_TAIL(p) ((uint64)p->next->m_addr <= (uint64)p->m_addr)
#define IS_ALL_FREE(p) ((uint64)p->next->m_addr - (uint64)p->m_addr - (uint64)p->m_size)
#define NEXT_ADDR(p) (IS_TAIL(p) ? ((void *)(MEM_SIZE + (uint64)mem)) : (p->next->m_addr))
#define ALLOC_START(p) ((void *)((uint64)p->m_addr + (uint64)p->m_size))
#define ALLOC_SIZE(p) ((uint64)NEXT_ADDR(p) - (uint64)ALLOC_START(p))
#define IS_PRI_MAP(p, addr) (IS_TAIL(p) ? ((uint64)p->m_addr <= (uint64)addr) : ((((uint64)p->m_addr <= ((uint64)addr)) && ((uint64)p->next->m_addr >= (uint64)addr))))
typedef unsigned uint;
typedef unsigned long uint64;

#include <stdio.h>
#include <malloc.h>

struct map
{
    uint m_size;
    char *m_addr;
    struct map *next, *prior;
};

void minit();
void mfree();
struct map *findaddr(char *addr);
struct map *findsize(uint size);
struct map *insertnode(struct map *);
void deletenode(struct map *);
void allocnode(struct map *, uint);
void subnode(struct map *, uint);
struct map *listalloc(uint size);
void listfree(struct map *);
void *lmalloc(uint size);
void lfree(uint size, char *addr);
void lprint();

#endif