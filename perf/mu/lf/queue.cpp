#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <mu/lf/queue.h>

using namespace std;

using namespace mu::lf;
using namespace mu;


/// Benchmark queue implementation with the following runtime parameters
///
/// - consumers (1 thread per consumer)
/// - producers (1 thread per producer)
/// - total number of elements to  produce
/// - iterations agains a single stack

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

// Synchronize output stream operations.
static mutex g_io_mutex;

void produce(size_t element_count, size_t id_offset, q_t& q)
{
    size_t id = id_offset;

    {
        lock_guard<mutex> l(g_io_mutex);
        cout << this_thread::get_id() << " - produce from ID " << id << endl;
    }

    for (size_t i = 0; i < element_count; ++i) {
        q.emplace(e_t(id++));
    }
    --id;

    {
        lock_guard<mutex> l(g_io_mutex);
        cout << this_thread::get_id() << " - produced to ID " << id << endl;
    }
}

void consume(size_t element_count, q_t& q, vector<size_t>& consumed)
{
    {
        lock_guard<mutex> l(g_io_mutex);
        cout << this_thread::get_id() << " - consume" << endl;
    }

    size_t attempt_count = 0;
    size_t consumed_count = 0;
    while (consumed_count < element_count) {
        optional<e_t> e = q.pop();
        if (e) {
            ++consumed_count;
            attempt_count = 0;
            consumed[e->id_] = true;
        } else {
            ++attempt_count;
            if (attempt_count > 1000'000'000) {
                cerr << this_thread::get_id() << " - timed out on pop" << endl;
                cerr << this_thread::get_id() << " - stopping" << endl;
                return;
            }
        }
    }

    {
        lock_guard<mutex> l(g_io_mutex);
        cout << this_thread::get_id() << " - consumed "
                << consumed_count << endl;
    }
}

void test_concurrent_producers_consumers(
        size_t producer_count,
        size_t consumer_count,
        size_t element_count,
        size_t iterations)
{
    q_t q;
    for (size_t i = 0; i < iterations; ++i) {
        size_t count_per_consumer = element_count / consumer_count;
        size_t count_per_producer = element_count / producer_count;

        vector<size_t> consumed;
        consumed.reserve(element_count);
        for (size_t j = 0; j < element_count; ++j) {
            consumed[j] = 0;
        }

        vector<thread> producers;
        for (size_t j = 0; j < producer_count; ++j) {
            size_t offset = (element_count / producer_count) * j;
            producers.emplace_back(thread(produce, count_per_producer, offset, ref(q)));
        }
        vector<thread> consumers;
        for (size_t j = 0; j < consumer_count; ++j) {
            consumers.emplace_back(thread(std::bind(consume, count_per_consumer, ref(q), ref(consumed))));
        }

        for (auto &t : producers) {
            t.join();
        }
        for (auto &t : consumers) {
            t.join();
        }

        for (size_t j = 0; j < element_count; ++j) {
            if (!consumed[j]) {
                cout << j << " unconsumed" << endl;
            }
        }
    }
    if (!q.empty()) {
        cout << "queue not empty: " << q << endl;
    }
    assert(q.empty());
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
    if (producer_count < 1 || consumer_count < 1 || element_count < 1 || iterations < 1) {
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
    test_concurrent_producers_consumers(
            static_cast<size_t>(producer_count),
            static_cast<size_t>(consumer_count),
            static_cast<size_t>(element_count),
            static_cast<size_t>(iterations));
    return 0;
}
