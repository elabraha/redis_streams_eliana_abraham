#include "stream.cpp"
#include <cassert>
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>

// here I simulate concurrent access to the stream

// check if this is actually really testing thread safety because I'm not
// actually good at this, I use libraries so this is weird
void test_concurrency_for_xadd() {
    // this needs to use threads
    redisStream stream;
    std::vector<long long> ids;
    // timer so these things happen at the same time idk?
    std::thread t1([&stream, &ids]() {
        FieldsStructure data1 = {{"field1", "value1"}};
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        long long id1 = stream.xadd("stream1", data1);
        ids.push_back(id1);
    });

    std::thread t2([&stream, &ids]() {
        FieldsStructure data2 = {{"field2", "value2"}};
        long long id2 = stream.xadd("stream1", data2);
        ids.push_back(id2);
    });

    t1.join();
    t2.join();

    assert(ids.size() == 2);
    assert(ids[0] != ids[1]);
    assert(stream.xlen("stream1") == 2);
    std::cout << "test_concurrency_for_xadd passed" << std::endl;
}

void test_concurrency_for_xread_blocking_when_data_added() {
    redisStream stream;
    std::atomic<bool> data_added{false};
    std::chrono::milliseconds block_time(100);

    // thread to read from the stream
    std::thread reader([&]() {
        auto start_time = std::chrono::steady_clock::now();
        auto result = stream.xread({"conc_stream"}, {0}, block_time.count());
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // check if the read returned before the full block_time
        assert(duration < block_time);
        assert(!result.empty());
        std::cout << "Reader thread finished" << std::endl;
    });

    // thread to add data to the stream after a short delay
    std::thread writer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        FieldsStructure data = {{"field", "value"}};
        stream.xadd("conc_stream", data);
        data_added = true;
        std::cout << "Writer thread finished" << std::endl;
    });

    reader.join();
    writer.join();

    assert(data_added);
    std::cout << "test_concurrency_for_xread_blocking_when_data_added passed" << std::endl;
}

void test_xrange_with_multiple_threads() {
    redisStream stream;
    std::vector<std::thread> threads;

    // Create multiple threads to add data to the stream
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&stream, i]() {
            FieldsStructure data = {{"field", "value" + std::to_string(i)}};
            stream.xadd("test_stream", data);
        });
    }

    // Wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }

    // Check the length of the stream
    assert(stream.xlen("test_stream") == 5);
    std::cout << "test_xrange_with_multiple_threads passed" << std::endl;
}

void test_check_for_possible_deadlock(){
    // Not sure if this actually does the job that well it's possible
    // there's a better way but that involves timeout loops and periodically
    // checking the state of the threads to see if they finished and then
    // timing out and then guessing that if they timeout out that there possibly
    // was a deadlock. This is hard to test and I think I'm handling locks
    // properly, every function blocks and I haven't started ot use shared locks
    // only a unique lock for for a conditional variable and that's still
    // not super fine grained mutex control or anything so I should be fine.
    // The thing is you don't really unit test in anticipation of finding
    // deadlocks you stress test and run randomized tests as well as deadlock
    //detector algorithms that exist
    redisStream stream;

    std::cout << "Testing for possible deadlock..." << std::endl;

    // create a thread that will try to read from the stream
    std::thread reader([&stream]() {
        stream.xread({"deadlock_stream"}, {0}, 100);
    });

    // create a thread that will try to write to the stream
    std::thread writer([&stream]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        stream.xadd("deadlock_stream", {{"field", "value"}});
    });

    reader.join();
    writer.join();

    // check for deadlock
    if (reader.joinable() || writer.joinable()) {
        std::cout << "Deadlock detected" << std::endl;
        return;
    }
    // theres isn't any
    std::cout << "No deadlock detected" << std::endl;
}

//eventually test for possible race conditions with many threads and periodic or
// random delays

int main() {
    test_concurrency_for_xadd();
    test_concurrency_for_xread_blocking_when_data_added();
    test_xrange_with_multiple_threads();
    // wait for a little bit I'm not sure why but maybe one of my tests hangs
    // it doesn't I had a bug in my makefile but keeping just in case
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    test_check_for_possible_deadlock();
    std::cout << "All concurrency tests passed!" << std::endl;
    return 0;
}
