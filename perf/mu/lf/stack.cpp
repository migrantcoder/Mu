// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include <mu/lf/stack.h>

#define LOCKFREE 1

/// Benchmark stack implementations with the following runtime parameters
///
/// - consumers (1 thread per consumer)
/// - producers (1 thread per producer)
/// - total number of elements to  produce
///
/// The stack implementation can be changed at compile time.

using namespace std;
using namespace std::experimental;

static mutex g_io_mutex;

struct foo {
    foo() : id_(0) {}
    foo(size_t id) : id_(id) {}
    size_t id_;
};

// Cache line sized bool.
struct bool_cl {
    bool_cl() : bool_(0), padding_(0) {}
    bool_cl(bool_cl const& lhs) : bool_(lhs.bool_), padding_(0) {}
    ~bool_cl() = default;

    bool_cl& operator=(bool lhs)
    {
        bool_ = lhs;
        padding_ = 0;
        return *this;
    }
    operator bool() { return bool_; }

    size_t bool_;
    size_t padding_;
};

/// Implementation wrapper.
template <typename T>
class stack {
public:
    stack() : stack_() {}

    void push(T& e)
    {
#ifdef LOCKFREE
        stack_.push(e);
#else
        lock_guard<mutex> l(g_io_mutex);
        stack_.push_front(e);
#endif
    }

    void emplace(T&& e)
    {
#ifdef LOCKFREE
        stack_.emplace(move(e));
#else
        lock_guard<mutex> l(g_io_mutex);
        stack_.push_front(e);
#endif
    }

    bool pop(T& e)
    {
#ifdef LOCKFREE
        return stack_.pop(e);
#else
        lock_guard<mutex> l(g_io_mutex);
        if (stack_.empty()) {
            return false;
        }
        e = stack_.front();
        stack_.pop_front();
        return true;
#endif
    }

    mu::optional<T> pop()
    {
#ifdef LOCKFREE
        return stack_.pop();
#else
        lock_guard<mutex> l(g_io_mutex);
        if (stack_.empty()) {
            return false;
        }
        e = stack_.front();
        stack_.pop();
        return make_optional<T>(move(e));
#endif
    }

    bool empty() const { return stack_.empty(); }

private:
#ifdef LOCKFREE
    mu::lf::stack<T> stack_;
#else
    mutex _mutex;
    list<T> stack_;
#endif
};

/// Produce \c element_count foos.
void produce(size_t element_count, size_t id_offset, stack<foo>& foos)
{
    size_t id = id_offset;

    {
        lock_guard<mutex> l(g_io_mutex);
        cout << this_thread::get_id() << " - produce from ID " << id << endl;
    }

    for (size_t i = 0; i < element_count; ++i) {
        foos.emplace(foo(id++));
    }

    {
        lock_guard<mutex> l(g_io_mutex);
        cout << this_thread::get_id() << " - produced to ID " << id << endl;
    }
}

/// Consume count elements.
void consume(
        size_t element_count,
        stack<foo>& foos,
        vector<bool_cl>& consumed)
{
    {
        lock_guard<mutex> l(g_io_mutex);
        cout << this_thread::get_id() << " - consume" << endl;
    }

    foo e;
    size_t n = 0;
    while (n < element_count) {
        auto foo = foos.pop();
        if (foo) {
            ++n;
            consumed[foo->id_] = true;
        }
    }

    {
        lock_guard<mutex> l(g_io_mutex);
        cout << this_thread::get_id() << " - consumed " << n << endl;
    }
}

void test_concurrent_produce_consume(
        const size_t producer_count,
        const size_t consumer_count,
        const size_t element_count,
        const size_t iterations)
{
    // The stack instance to test.
    stack<foo> stack;

    for (size_t j = 0; j < iterations; ++j) {
        // Track consumed IDs.
        vector<bool_cl> consumed;
        consumed.reserve(element_count);
        for (size_t i = 0; i < element_count; ++i) {
            consumed[i] = false;
        }

        size_t const elements_per_producer = element_count/producer_count;
        size_t const elements_per_consumer = element_count/consumer_count;

        // Produce.
        vector<thread> producers;
        for (size_t i = 0; i < producer_count; ++i) {
            size_t id_offset = elements_per_producer * i;
            producers.emplace_back(
                    thread(
                            produce,
                            elements_per_producer,
                            id_offset,
                            ref(stack)));
        }

        // Consume.
        vector<thread> consumers;
        for (size_t i = 0; i < consumer_count; ++i) {
            consumers.emplace_back(
                    thread(
                            consume,
                            elements_per_consumer,
                            ref(stack),
                            ref(consumed)));
        }

        // Wait.
        for (auto &c : consumers) {
            c.join();
        }
        for (auto &p : producers) {
            p.join();
        }

        // Verify.
        bool found_unconsumed = false;
        for (size_t i = 0; i < element_count; ++i) {
            if (!consumed[i]) {
                if (!found_unconsumed) {
                    cerr << "unconsumed: ";
                }
                found_unconsumed = true;
                cerr << i << " ";
            }
        }
        if (found_unconsumed) {
            cerr << endl;
            cerr << "stopping" << endl;
            break;
        }

        assert(stack.empty());
    }
}

string usage(char const * const program)
{
        return string("usage: ") + program + " PRODUCERS CONSUMERS ELEMENTS "
                "[ITERATIONS]";
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        cerr << usage(argv[0]) << endl;
        exit(1);
    }
    int producer_count = atoi(argv[1]);
    int consumer_count = atoi(argv[2]);
    int element_count = atoi(argv[3]);
    int iterations = argc > 4 ? atoi(argv[4]) : 1;
    if (producer_count < 1 ||
            consumer_count < 1 ||
            element_count < 1 || 
            iterations < 1) {
        cerr << "parameters must each be > 0" << endl;
        cerr << usage(argv[0]) << endl;
        exit(1);
    }
    if (producer_count > element_count) {
        cerr << "PRODUCERS must be <= ELEMENTS" << endl;
        cerr << usage(argv[0]) << endl;
        exit(1);
    }
    if (consumer_count > element_count) {
        cerr << "CONSUMERS must be <= ELEMENTS" << endl;
        cerr << usage(argv[0]) << endl;
        exit(1);
    }
    test_concurrent_produce_consume(
            static_cast<size_t>(producer_count),
            static_cast<size_t>(consumer_count),
            static_cast<size_t>(element_count),
            static_cast<size_t>(iterations));
    return 0;
}
