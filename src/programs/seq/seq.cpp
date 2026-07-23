#include <charconv>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdio.h>
#include <system_error>
#include <format>

[[noreturn]]
int usage_err()
{
    std::cerr << "Usage:\n    seq LAST\n    seq FIRST LAST\n    seq FIRST INCREMENT LAST" << std::endl;
    exit(1);
}

[[noreturn]]
void parse_err(const char* arg_name, const char* arg_value, const char* err_msg)
{
    std::cout << std::format("failed to parse {} (value is '{}'). Reason: {}", arg_name, arg_value, err_msg) << std::endl;
    exit(1);
}

void seq(int start, int last, int step)
{
    for (int i = start; i <= last; i += step)
        printf("%d\n", i);
}

int parse_num(const char* str, const char* name)
{
    int val;
    const auto [ptr, ec] = std::from_chars(str, str + strlen(str), val);
    if (ec == std::errc())
        return val;
    parse_err(name, str, std::make_error_code(ec).message().c_str());
}

int main(int argc, char** argv)
{
    int start, last, step;
    if (argc == 2)
    {
        start = 1;
        last = parse_num(argv[1], "last");
        step = 1;
    }
    else if (argc == 3)
    {
        start = parse_num(argv[1], "first");
        last = parse_num(argv[2], "last");
        step = 1;
    }
    else if (argc == 4)
    {
        start = parse_num(argv[1], "first");;
        last = parse_num(argv[1], "last");;
        step = parse_num(argv[1], "step");;
    }
    else
        usage_err();

    seq(start, last, step);
    return 0;
}
