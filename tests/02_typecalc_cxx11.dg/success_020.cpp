/*
<testinfo>
# This test fails in g++ 4.8 but it is supposed to work in the future
test_generator="config/mercurium-cxx11 gxx_fails"
</testinfo>
*/

template <typename R1, typename R2>
struct W1 { };

template <template <typename, typename> class W>
struct A { };

template <template <typename, typename> class ...W>
struct B { };

template <template <typename, typename> class ...W>
void f(A<W...> &a, B<W...> &b);

void g()
{
    A<W1> a;
    B<W1> b;

    ::f(a, b);
}
