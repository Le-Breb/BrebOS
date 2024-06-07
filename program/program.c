int p_main()
{
	void* esp;

	asm volatile ("mov %%esp, %0" : "=r" (esp));
	char* str = "Hello, World!\n";
	char* str2 = "Hello, World, but the second one\n";

	__asm__ volatile("int $0x80" : : "a"(2), "b"(str)); // printf of str
	__asm__ volatile("int $0x80" : : "a"(2), "b"(str2)); // printf of str2

	return 0;
}