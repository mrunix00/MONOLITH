void _init(void);
void _fini(void);
int main(int argc, char *argv[]);
void exit(int status);

void _start(void)
{
    int argc;
    char **argv;

#if defined(MONOLITH_ARCH_IA32) || defined(__i386__)
    /* i386: argc and argv are on the stack */
    __asm__ volatile("movl 4(%%ebp), %0\n"
                     "leal 8(%%ebp), %1\n"
                     : "=r"(argc), "=r"(argv)
                     :
                     : "memory");
#else
    /* x86_64: argc in rdi, argv (pointer to argv array) in rsi */
    __asm__ volatile("" : "=D"(argc), "=S"(argv));
#endif

    _init();
    int status = main(argc, argv);
    _fini();
    exit(status);
}
