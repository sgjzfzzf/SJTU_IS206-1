#include "memory.h"

int main(int argc, char *argv[])
{
    minit();

    char c;
    uint64 size, addr;
    while (1)
    {
        printf("> ");
        do
        {
            c = getchar();
        } while (c == '\n' || c == '\t' || c == ' ');
        switch (c)
        {
        case 'f':
            scanf("%lx %lx", &size, &addr);
            printf("size: %lx, addr: %lx\n", size, addr);
            lfree(size, (char *)(uint64)addr);
            lprint();
            break;
        case 'm':
            scanf("%lx", &size);
            lmalloc(size);
            lprint();
            break;
        }
    }

    mfree();
    return 0;
}