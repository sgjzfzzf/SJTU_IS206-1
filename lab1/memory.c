#include "memory.h"

void minit()
{
    mem = malloc(MEM_SIZE);
    head = malloc(sizeof(struct map));
    head->isused = 0;
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

struct map *findfree(struct map *p)
{
    while (p->isused)
    {
        p = p->next;
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
    free(head);
}

void *lmalloc(uint64 size)
{
    struct map *p = cur;
    if (p->isused || p->m_size < size)
    {
        p = cur->next;
        while (p != cur && (p->isused || p->m_size < size))
        {
            p = p->next;
        }
        if (p == cur)
        {
            fprintf(stderr, "cannot find enough memory\n");
            return NULL;
        }
    }
    cur = findfree(p->next);
    if (p->m_size == size)
    {
        p->isused = 1;
        return p->m_addr;
    }
    else
    {
        struct map *q = insertnode(p);
        p->m_size -= size;
        q->isused = 1;
        q->m_addr = p->m_addr + p->m_size;
        q->m_size = size;
        return q->m_addr;
    }
}

void lfree(uint64 size, char *addr)
{
    struct map *p = cur;
    if (!p->isused || p->m_addr != addr || p->m_size != size)
    {
        p = cur->next;
        while (p != cur && (!p->isused || p->m_addr != addr || p->m_size != size))
        {
            p = p->next;
        }
        if (p == cur)
        {
            fprintf(stderr, "cannot find assigned used block\n");
            return;
        }
    }
    // case 0:  ///free/// ///released/// ///free///
    if (!IS_HEAD(p) && !IS_TAIL(p) && !p->prior->isused && !p->next->isused)
    {
        struct map *prior = p->prior, *next = p->next;
        prior->m_size += (p->m_size + next->m_size);
        deletenode(p);
        deletenode(next);
    }
    // case 1:  ///free/// ///released/// ///used/// or ///free/// ///released/// end
    else if (!IS_HEAD(p) && !p->prior->isused && (!IS_TAIL(p) && p->next->isused || IS_TAIL(p)))
    {
        p->prior->m_size += p->m_size;
        deletenode(p);
    }
    // case 2:  ///used/// ///released/// ///free/// or start ///released/// ///free///
    else if (!IS_TAIL(p) && !p->next->isused && (!IS_HEAD(p) && p->prior->isused || IS_HEAD(p)))
    {
        p->next->m_addr = p->m_addr;
        p->next->m_size += p->m_size;
        deletenode(p);
    }
    // case 3:  ///used/// ///released/// ///used/// or start ///released/// ///used/// or ///used/// ///released/// end
    else if (IS_HEAD(p) || IS_TAIL(p) || !IS_HEAD(p) && !IS_TAIL(p) && p->prior->isused && p->next->isused)
    {
        p->isused = 0;
    }
    else
    {
        fprintf(stderr, "illegal release\n");
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
            printf("block: %lx, is used:%lx, address: %lx, size: %lx, prior: %lx, next: %lx\n", counter++, p->isused, (uint64)p->m_addr, (uint64)p->m_size, (uint64)p->prior->m_addr, (uint64)p->next->m_addr);
            p = p->next;
        } while (p != head);
    }
    printf("\n");
}