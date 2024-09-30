_Noreturn void lib_main()
{
	/* Expected memory layout:
	 * ESP
	 * ...     => local variables
	 * EBP     => previous EBP
	 * EBP + 4 => mod_addr
	 * EBP + 8 => symbol offset
	 */

	// Get EBP
	int* ebp;
	__asm__ volatile("mov %%ebp, %0" : "=r"(ebp));

	// Get args
	int offset = *(ebp + 2);
	int mod_addr = *(ebp + 1);

	// Get symbol address - run libdynlk
	unsigned int symbol_addr;
	__asm__ volatile("int $0x80" : "=a"(symbol_addr) : "a"(7), "b"(offset), "c"(mod_addr));

	// Execute function - restore previous EBP, pop args and jump to function
	__asm__ volatile(
			"mov %0, %%esp\n" // ebp -> esp
			"pop %%ebp\n" // restore previous ebp
			"movl %%esp, %%eax\n"
			"addl $8, %%eax\n"
			"movl %%eax, %%esp\n" // add 8 to eax to pop args
			"jmp %1" // jump to function
			: : "r"(ebp), "r"(symbol_addr));


	__builtin_unreachable();
}
