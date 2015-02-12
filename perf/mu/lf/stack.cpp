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

/// Benchmark stack implementations with the following runtime parameters
///
/// - consumers (1 thread per consumer)
/// - producers (1 thread per producer)
/// - total number of elements to produce
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

/// Word sized bool for use in concurrent access table to avoid lost updates.
union bool_t {
    bool_t() : bool_(0) {}
    bool_t(bool_t const& lhs) : bool_(lhs.bool_) {}
    ~bool_t() = default;

    bool_t& operator=(bool lhs) { bool_ = lhs; return *this; }
    operator bool() { return bool_; }

    bool bool_;
    uint64_t padding_;
};

/// Implementation wrapper.
template <typename T>
class locking_stack {
public:
    locking_stack() : stack_() {}

    void push(T& e)
    {
        lock_guard<mutex> _(g_io_mutex);
        stack_.push_front(e);
    }

    void emplace(T&& e)
    {
        lock_guard<mutex> _(g_io_mutex);
        stack_.push_front(e);
    }

    bool pop(T& e)
    {
        lock_guard<mutex> _(g_io_mutex);
        if (stack_.empty()) {
            return false;
        }
        e = stack_.front();
        stack_.pop_front();
        return true;
    }

    mu::optional<T> pop()
    {
        T e;
        mu::optional<T> o;
        if (pop(e)) {
            o = make_optional<T>(move(e));
        }
        return o;
    }

    bool empty() const { return stack_.empty(); }

private:
    mutex _mutex;
    list<T> stack_;
};

#if defined(LOCKING)
using stack = locking_stack<foo>;
constexpr static const char* g_stack_type = "locking_stack";
#else
using stack = mu::lf::stack<foo>;
constexpr static const char* g_stack_type = "mu::lf::stack";
#endif

/// Produce \c element_count foos.
void produce(size_t element_count, size_t id_offset, stack& foos)
{
    size_t id = id_offset;

    {
        lock_guard<mutex> _(g_io_mutex);
        cout << this_thread::get_id() << " - produce from ID " << id << endl;
    }

    for (size_t i = 0; i < element_count; ++i) {
        foos.emplace(foo(id++));
    }

    {
        lock_guard<mutex> _(g_io_mutex);
        cout << this_thread::get_id() << " - produced to ID " << id << endl;
    }
}

/// Consume count elements.
void consume(
        size_t element_count,
        stack& foos,
        vector<bool_t>& consumed)
{
    {
        lock_guard<mutex> _(g_io_mutex);
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
        lock_guard<mutex> _(g_io_mutex);
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
    stack stack;

    for (size_t j = 0; j < iterations; ++j) {
        // Track consumed IDs.
        vector<bool_t> consumed;
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
    cout << "using " << g_stack_type << endl;
    test_concurrent_produce_consume(
            static_cast<size_t>(producer_count),
            static_cast<size_t>(consumer_count),
            static_cast<size_t>(element_count),
            static_cast<size_t>(iterations));
    return 0;
}
