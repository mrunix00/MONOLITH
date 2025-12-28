#include <sys/syscall.h>
#include <unistd.h>

int main()
{
    while (1) {
        usleep(1000);
        syscall0(SYSCALL_TEST);
    }
}
