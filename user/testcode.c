#include "kernel/types.h"
#include "user.h"

int main(int argc, char* argv[])
{
    unsigned int i = 0x00646c72;
    printf("H%x Wo%s", 57616, &i);
    return 0;
}