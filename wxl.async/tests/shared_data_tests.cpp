

#include <gtest/gtest.h>



import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

struct record_status {
    enum enum_ {
        new_,
        reuse,
        sent
    };
};

/// Test Record.
struct record {
    record()
        : status_(record_status::new_)
        , reused_count_(0)
    {}

    record_status::enum_ status_;
    unsigned long long reused_count_;
};

typedef wxl::async::safe_pool<record> pool;
typedef pool::element_type entry;
typedef wxl::core::pool_ptr<entry> entry_ptr;
typedef wxl::async::mpsc_channel<entry> channel;

#ifdef NDEBUG
const size_t number_of_records_per_producer = 10000;
#else
const size_t number_of_records_per_producer = 1000;
#endif

class producer : public threaded_component
{
public:
    producer(thread_group* manager, channel* chan)
        : threaded_component("Producer", manager)
        , pool_()
        , channel_(chan)
    {}

    ~producer() override {
        dispose();
    }

protected:
    void run() override {
        for (size_t i = 0; i < number_of_records_per_producer; ++i) {
            entry_ptr record(pool_.get());

            EXPECT_EQ(record->reused_count_ ? record_status::reuse : record_status::new_, record->status_);

            record->status_ = record_status::sent;
            channel_->send(record);
        }
    }

private:
    pool pool_;
    channel* channel_;
};

const size_t producer_count = 8;

class consumer : public threaded_component
{
public:
    consumer(thread_group* manager, channel* chan)
        : threaded_component("Consumer", manager)
        , channel_(chan)
    {}

    ~consumer() override {
        dispose();
    }

protected:
    void run() override {
        for (size_t i = 0; i < number_of_records_per_producer*producer_count; ++i) {
            entry_ptr record;

            // receive() may wake with nothing, as a condition variable does.
            while (!channel_->receive(record)) {}

            EXPECT_EQ(record_status::sent, record->status_);

            record->status_ = record_status::reuse;
            record->reused_count_++;
        }
    }

private:
    channel* channel_;
};

};

// --run_test=System/sharedDataTest --log_level=test_suite
TEST(SharedDataTest, shared_data_test)
{
    thread_scope manager;
    channel chan;

    consumer consumer_tester(manager.get(), &chan);
    consumer_tester.start_async();

    typedef std::unique_ptr<producer> producer_ptr;
    producer_ptr producers[producer_count];

    for (size_t i = 0; i < producer_count; ++i) {
        producers[i].reset(new producer(manager.get(), &chan));
        producers[i]->start_async();
    }

    manager->join_all();
}
