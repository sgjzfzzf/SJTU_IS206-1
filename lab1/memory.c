#include "memory.h"

void *mem;
struct map *head, *cur;

void minit()
{
    mem = malloc(MEM_SIZE);
    head = malloc(sizeof(struct map));
    head->m_size = MEM_SIZE;
    head->m_addr = (char *)mem;
    head->prior = head;
    head->next = head;
    cur = head;
}

void mfree()
{
    listfree(head);
    free(mem);
}

struct map *findaddr(char *addr)
{
    if (IS_PRI_MAP(cur, addr))
    {
        return cur;
    }
    else
    {
        struct map *p = cur;
        while (!IS_PRI_MAP(p, addr) && p != cur)
        {
            p = p->next;
        }
        if (p == cur)
        {
            return NULL;
        }
        else
        {
            return p;
        }
    }
}

struct map *findsize(uint size)
{
    if (cur->m_size >= size)
    {
        return cur;
    }
    else
    {
        struct map *p = cur;
        while (p->m_size >= size && p != cur)
        {
            p = p->next;
        }
        if (p == cur)
        {
            return NULL;
        }
        else
        {
            return p;
        }
    }
}

struct map *insertnode(struct map *prior)
{
    struct map *next = prior->next, *p = malloc(sizeof(struct map));
    prior->next = p;
    next->prior = p;
    p->prior = prior;
    p->next = next;
    return p;
}

void deletenode(struct map *p)
{
    struct map *prior = p->prior, *next = p->next;
    prior->next = next;
    next->prior = prior;
    free(p);
}

void allocnode(struct map *p, uint size)
{
    p->m_size -= size;
}

void subnode(struct map *p, uint size)
{
    p->m_size += size;
}

struct map *listalloc(uint size)
{
    struct map *p = findsize(size);
    if (p == NULL)
    {
        fprintf(stderr, "cannot find enough memory\n");
    }
    else
    {
        allocnode(p, size);
    }
    return p;
}

void listfree(struct map *node)
{
    struct map *tmp;
    while (node != head)
    {
        tmp = node;
        node = node->next;
        free(tmp);
    }
}

void *lmalloc(uint size)
{
    struct map *p = listalloc(size);
    if (p == NULL)
    {
        fprintf(stderr, "cannot find suitable memory\n");
    }
    cur = p->next;
    return ALLOC_START(p);
}

void lfree(uint size, char *addr)
{
    struct map *p = findaddr(addr);
    if (p == NULL)
    {
        fprintf(stderr, "cannot find related map\n");
    }
    else if (ALLOC_SIZE(p) == size)
    {
        if (ALLOC_START(p) == addr)
        {
            deletenode(p);
        }
        else
        {
            fprintf(stderr, "free too much memory2\n");
        }
    }
    else if (ALLOC_SIZE(p) > size)
    {
        if ((uint64)addr == 0 && (uint64)p->next->m_addr > size || IS_TAIL(p) && (uint64)addr + size - (uint64)mem == MEM_SIZE || (uint64)ALLOC_START(p) < (uint64)addr && (uint64)addr + size < (uint64)NEXT_ADDR(p))
        {
            struct map *p = insertnode(p);
            p->m_size = size;
            p->m_addr = addr;
        }
        else if (ALLOC_START(p) == addr)
        {
            p->m_size += size;
        }
        else if (!IS_TAIL(p) && (uint64)addr + size == (uint64)NEXT_ADDR(p))
        {
            p->next->m_size += size;
            p->next->m_addr -= size;
        }
        else
        {
            fprintf(stderr, "free too much memory3\n");
        }
    }
}

void lprint()
{
    uint64 counter = 0;
    struct map *p = head, *tmp;
    if (p != NULL)
    {
        do
        {
            printf("free block: %lx, address: %lx, size: %lx\n", counter++, (uint64)p->m_addr, (uint64)p->m_size);
            p = p->next;
        } while (p != head);
    }
    printf("\n");
}