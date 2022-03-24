#include "memory.h"

int main(int argc, char *argv[])
{
    minit();

    char c;
    uint size, addr;
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
            scanf("%u %u", &size, &addr);
            lfree(size, (char *)(uint64)addr);
            lprint();
            break;
        case 'm':
            scanf("%u", &size);
            lmalloc(size);
            lprint();
            break;
        }
    }

    mfree();
    return 0;
}