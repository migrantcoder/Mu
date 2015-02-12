#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <mu/lf/queue.h>

using namespace std;

/// Benchmark queue implementation with the following runtime parameters
///
/// - consumers (1 thread per consumer)
/// - producers (1 thread per producer)
/// - total number of elements to produce
/// - iterations against a single queue

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

#ifdef BOOST_LFQ

#include <boost/lockfree/queue.hpp>

template<typename T>
class boost_lfq_wrapper {
public:
    boost_lfq_wrapper() : q_(mu::lf::queue<T>::DEFAULT_INITIAL_CAPACITY) {}

    mu::optional<T> pop()
    {
        T v;
        mu::optional<T> o;
        if (q_.pop(v))
            o = experimental::make_optional<T>(move(v));
        return o;
    }

    void push(T const & v) { q_.push(v); }
    bool empty() { return q_.empty(); }
    size_t capacity() { return 0; }

private:
    boost::lockfree::queue<T> q_;
};

#endif // BOOST_LFQ

template<typename T>
class locking_queue {
public:
    locking_queue() : q_(mu::lf::queue<T>::DEFAULT_INITIAL_CAPACITY)
    {
        q_.resize(0);  // Reduce size, but not, necessarily, capacity.
    }

    mu::optional<T> pop()
    {
        T v;
        mu::optional<T> o;
        bool popped = false;
        {
            lock_guard<mutex> _(m_);
            if (!q_.empty()) {
                v = q_.front();
                q_.pop_front();
                popped = true;
            }
        }
        if (popped)
            o = experimental::make_optional<T>(move(v));
        return o;
    }

    void push(T const & v)
    {
        lock_guard<mutex> _(m_);
        q_.push_back(v);
    }

    bool empty() { return q_.empty(); }
    size_t capacity() { return 0; }

private:
    mutex m_;
    list<T> q_;
};

#if defined(BOOST_LFQ)
using queue boost_lfq_wrapper<foo>;
constexpr static const char* g_queue_type = "boost::lockfree::queue";
#elif defined(LOCKING)
using queue = locking_queue<foo>;
constexpr static const char* g_queue_type = "locking_queue";
#else
using queue = mu::lf::queue<foo>;
constexpr static const char* g_queue_type = "mu::lf::queue";
#endif

// Synchronize output stream operations.
static mutex g_io_mutex;

void produce(size_t element_count, size_t id_offset, queue& q)
{
    size_t id = id_offset;

    {
        lock_guard<mutex> _(g_io_mutex);
        cout << this_thread::get_id() << " - produce from ID " << id << endl;
    }

    for (size_t i = 0; i < element_count; ++i) {
        q.push(id++);
    }
    --id;

    {
        lock_guard<mutex> _(g_io_mutex);
        cout << this_thread::get_id() << " - produced to ID " << id << endl;
    }
}

void consume(size_t element_count, queue& q, vector<size_t>& consumed)
{
    {
        lock_guard<mutex> _(g_io_mutex);
        cout << this_thread::get_id() << " - consume" << endl;
    }

    size_t attempt_count = 0;
    size_t consumed_count = 0;
    while (consumed_count < element_count) {
        mu::optional<foo> e = q.pop();
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
        lock_guard<mutex> _(g_io_mutex);
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
    // The queue instance to test.
    queue q;

    for (size_t i = 0; i < iterations; ++i) {
        size_t count_per_consumer = element_count / consumer_count;
        size_t count_per_producer = element_count / producer_count;

        // Track consumed IDs.
        vector<size_t> consumed;
        consumed.reserve(element_count);
        for (size_t j = 0; j < element_count; ++j) {
            consumed[j] = 0;
        }

        // Produce.
        vector<thread> producers;
        for (size_t j = 0; j < producer_count; ++j) {
            size_t offset = (element_count / producer_count) * j;
            producers.emplace_back(
                    thread(produce, count_per_producer, offset, ref(q)));
        }

        // Consume.
        vector<thread> consumers;
        for (size_t j = 0; j < consumer_count; ++j) {
            consumers.emplace_back(
                    thread(consume, count_per_consumer, ref(q), ref(consumed)));
        }

        // Wait.
        for (auto &t : consumers) {
            t.join();
        }
        for (auto &t : producers) {
            t.join();
        }

        // Verify.
        bool found_unconsumed = false;
        for (size_t j = 0; j < element_count; ++j) {
            if (!consumed[j]) {
                if (!found_unconsumed) {
                    cerr << "unconsumed: ";
                }
                found_unconsumed = true;
                cerr << j << " ";
            }
        }
        if (found_unconsumed) {
            cerr << endl;
            cerr << "stopping" << endl;
            break;
        }

        assert(q.empty());
    }

    cout << "capacity " << q.capacity() << endl;
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
    if (producer_count < 1 || consumer_count < 1 || element_count < 1 ||
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
    cout << "using " << g_queue_type << endl;
    test_concurrent_producers_consumers(
            static_cast<size_t>(producer_count),
            static_cast<size_t>(consumer_count),
            static_cast<size_t>(element_count),
            static_cast<size_t>(iterations));
    return 0;
}
