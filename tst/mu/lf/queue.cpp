#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <mu/lf/queue.h>

using namespace std;
using mu::lf::queue;

struct foo {
    foo() : id_(0) {}
    foo(size_t id) : id_(id) {}
    foo(const foo&) = default;
    foo(foo&& f) : id_(f.id_) {}
    foo& operator=(const foo&) = default;
    foo& operator=(foo&& lhs) { id_ = lhs.id_; return *this; }
    bool operator==(foo const& lhs) const { return id_ == lhs.id_; }
    size_t id_;
};

typedef queue<foo> q_t;
typedef q_t::value_type e_t;

void test_singleton()
{
    q_t q;
    assert(q.empty());

    e_t e(42);
    q.push(e);
    auto const popped = q.pop();
    assert(popped);
    assert(*popped == e);
}

void test_combinations(size_t const n)
{
    // For each s in 0...n
    //      for each c in 0..s
    //          create initial queue of s elements
    //          deque c elements
    //          queue c elements
    //          deque s elements
    for (size_t s = 0; s < n; ++s) {
        for (size_t c = 0; c < s; ++c) {
            q_t q;
            list<e_t> control;
            size_t id = 0;
            for (size_t i = 0; i < s; ++i) {
                e_t e(id);
                q.push(e);
                control.push_back(e);
                id++;
            }
            for (size_t i = 0; i < c; ++i) {
                auto e = q.pop();
                assert(e->id_ == control.front().id_);
                control.pop_front();
            }
            for (size_t i = 0; i < c; ++i) {
                e_t e(id);
                q.push(e);
                control.push_back(e);
                id++;
            }
            for (size_t i = 0; i < s; ++i) {
                auto e = q.pop();
                assert(e->id_ == control.front().id_);
                control.pop_front();
            }
        }
    }
}

void test_capacity_plus_n(size_t const n)
{
    q_t q;
    auto const size = q.capacity() + n;

    for (size_t i = 0; i < size; ++i) {
        q.push(e_t(i));
    }
    for (size_t i = 0; i < size; ++i) {
        auto const popped = q.pop();
        assert(popped);
        assert(popped == e_t(i));
    }
}

void run_tests()
{
    test_singleton();
    test_combinations(5);
    test_capacity_plus_n(0);
    test_capacity_plus_n(1);
}

int main(int const, char const** const)
{
    run_tests();
    return 0;
}
