void clear_screen()
{
    __asm__ volatile("int $0x80" :  : "a"(13));
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    clear_screen();

    return 0;
}