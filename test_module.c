const char* test_string = "test string";
static int x;

extern void uart_putchar(char);

const char* test()
{
	return test_string;
}
int* test2()
{
	x = 2812;
	return &x;
}
