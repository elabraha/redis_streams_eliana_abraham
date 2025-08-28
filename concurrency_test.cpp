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

void test_expected_dead_lock() {
    redisStream stream;

    // this test is expected to deadlock, so we will not assert anything.
    std::thread t1([&stream]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        stream.xadd("deadlock_stream", {{"field", "value"}});
    });

    std::thread t2([&stream]() {
        stream.xread({"deadlock_stream"}, {0}, 100);
    });

    t1.join();
    t2.join();
}

//eventually test for possible race conditions with many threads and periodic or
// random delays

int main() {
    test_concurrency_for_xadd();
    test_concurrency_for_xread_blocking_when_data_added();
    test_xrange_with_multiple_threads();
    test_expected_dead_lock();
    std::cout << "All concurrency tests passed!" << std::endl;
    return 0;
}
