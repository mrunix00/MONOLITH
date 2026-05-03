void _init(void);
void _fini(void);
int main(void);
void exit(int status);

void _start(void)
{
    _init();
    int status = main();
    _fini();
    exit(status);
}
