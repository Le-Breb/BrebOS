#include <iostream>
#include <cxxabi.h>
#include <string.h>

class A
{
public:
    virtual ~A() = default;
    virtual const char* say_my_name()
    {
        return typeid(*this).name();
    }
};

class B : public A
{
public:
    const char* say_my_name() override
    {
        return typeid(*this).name();
    }
};

class C
{

};

int main()
{
    auto a = new A();
    auto b = new B();
    auto c  = new C();

    A* a2 = new B;
    if (typeid(*a2) != typeid(B))
        std::cerr << "RTTI failure\n";
    delete a2;

    int status;
    const char* a_realname = abi::__cxa_demangle(a->say_my_name(), nullptr, nullptr, &status);
    const std::string_view a_realname_v{a_realname};
    if (a_realname_v != "A")
        std::cerr << "Typeid name failure\n";
    free(const_cast<void*>(static_cast<const void*>(a_realname)));
    const char* b_realname = abi::__cxa_demangle(b->say_my_name(), nullptr, nullptr, &status);
    const std::string_view b_realname_v{b_realname};
    if (b_realname_v != "B")
        std::cerr << "Typeid name failure\n";
    free(const_cast<void*>(static_cast<const void*>(b_realname)));


    const bool a_is_b = dynamic_cast<B*>(a);
    const bool b_is_a = dynamic_cast<A*>(b);
    const bool b_is_a_is_b = b_is_a && dynamic_cast<B*>(dynamic_cast<A*>(b));
    const bool b_is_c = dynamic_cast<C*>(b);

    if (const bool checks = !a_is_b && b_is_a && b_is_a_is_b && !b_is_c; checks)
        std::cout << "Checks passed\n";
    else
        std::cerr << "One or more check failed\n";

    delete a;
    delete b;
    delete c;

    return 0;
}
