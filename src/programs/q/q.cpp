[[noreturn]] void shutdown()
{
    __asm__ volatile("int $0x80" : : "a"(6));

    __builtin_unreachable();
}

[[noreturn]] int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    shutdown();
}