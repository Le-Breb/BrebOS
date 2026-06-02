#include <iostream>
#include <stdexcept>


void test()
{
    throw std::runtime_error("custom error :D");
}

int main()
{
    bool caught = false;
    try
    {
        test();
    }
    catch (std::exception& e)
    {
        caught = true;
        std::cout << "Exception succesffully catched, with the following error \
            message: " << e.what() << std::endl;
    }

    if (!caught)
        std::cerr << "Exception not caught. FAIL" << std::endl;

    return 0;
}
