extern void __libc_init_array(void);
extern int main(int argc, const char *argv[]);
extern void exit(int status);

void _init()
{
}

void _fini()
{
}

void _start(void)
{
    __libc_init_array();

    const char *argv[] = {"\0"};
    int result = main(0, argv);
    exit(result);
}
