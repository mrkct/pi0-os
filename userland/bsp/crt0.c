extern void __libc_init_array(void);
extern int main(int argc, const char *argv[]);
extern void exit(int status);

void _my_start(void)
{
    __libc_init_array();

    const char *argv[] = {"hello.exe", "\0"};
    int result = main(1, argv);
    exit(result);
}
