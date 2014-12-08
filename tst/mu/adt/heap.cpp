// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder 


#include <cassert>
#include <limits>

#include <mu/adt/heap.h>

using namespace std;
using namespace mu::adt;

typedef size_t element;

struct noncopyable {
    noncopyable() : element_(0) {}
    noncopyable(const element& lhs) : element_(lhs) {}
    noncopyable(noncopyable&& lhs) { element_ = lhs.element_; }
    noncopyable& operator=(noncopyable&& lhs)
    {
        element_ = lhs.element_;
        return *this;
    }

    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
    ~noncopyable() = default;

    element element_;
};

bool operator<(const noncopyable& lht, const noncopyable& rhs)
{
    return lht.element_ < rhs.element_;
}

static void test_emplace()
{
    heap<noncopyable> h;
    noncopyable a(1);
    h.emplace(move(a));
    noncopyable b = std::move(h.top());
}

static void test(const vector<element>& es, const vector<element>& expected_order)
{
    heap<element> h;
    for (const auto& e : es) {
        h.push(e);
    }
    for (const auto& e : expected_order) {
        assert(h.top() == e);
        h.pop();
    }
}

static void tests()
{
    constexpr static const element MAX = numeric_limits<element>::max();
    constexpr static const element MIN = numeric_limits<element>::min();

    test({}, {});
    test({MIN}, {MIN});
    test({MIN, MIN}, {MIN, MIN});
    test({MAX}, {MAX});
    test({MAX, MAX}, {MAX, MAX});
    test({MIN, MAX}, {MIN, MAX});
    test({MAX, MIN}, {MIN, MAX});
    test({1, 2}, {1, 2});
    test({3, 5, 0}, {0, 3, 5});
    test(
            {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});

    test_emplace();
}

int main(const int, const char** const)
{
    tests();
    return 0;
}
