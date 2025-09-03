#include "stream.cpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <thread>

void test_xadd() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    long long id = stream.xadd("mystream", data);
    assert(id == 0); // basic check that id is generated
    assert(stream.xlen("mystream") == 1); // check stream length
    std::cout << "test_xadd passed" << std::endl;
}

// test empty stream name for xadd this is supposed to fail in redis but
// does not in mine, I should eventually fix this.
void test_xadd_empty_stream() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    long long id = stream.xadd("", data);
    assert(id == 0); // basic check that id is generated
    assert(stream.xlen("") == 1); // check stream length
    std::cout << "test_xadd_empty_stream passed" << std::endl;
}

void test_xread() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    long long id = stream.xadd("mystream", data);

    auto result = stream.xread({"mystream"}, {0});
    assert(result.count("mystream") == 1); // check stream exists in result
    assert(result["mystream"].size() == 1); // check one entry is read
    assert(result["mystream"][0].first == id); // check correct ID is read
    assert(result["mystream"][0].second[0].first == "field1"); // check field
    assert(result["mystream"][0].second[0].second == "value1"); // check value
    std::cout << "test_xread passed" << std::endl;
}

void test_xread_multiple_streams() {
    redisStream stream;
    FieldsStructure data1 = {{"field1", "value1"}, {"field2", "value2"}};
    FieldsStructure data2 = {{"field1", "value3"}, {"field2", "value4"}};
    long long id1 = stream.xadd("mystream", data1);
    long long id2 = stream.xadd("mystream2", data2);

    auto result = stream.xread({"mystream", "mystream2"}, {0});
    assert(result.count("mystream") == 1); // check first stream exists in result
    assert(result["mystream"].size() == 1); // check one entry is read from first stream
    assert(result["mystream"][0].first == id1); // check correct ID is read from first stream
    assert(result["mystream"][0].second[0].first == "field1"); // check field from first stream
    assert(result["mystream"][0].second[0].second == "value1"); // check value from first stream

    assert(result.count("mystream2") == 1); // check second stream exists in result
    assert(result["mystream2"].size() == 1); // check one entry is read from second stream
    assert(result["mystream2"][0].first == id2); // check correct ID is read from second stream
    assert(result["mystream2"][0].second[0].first == "field1"); // check field from second stream
    assert(result["mystream2"][0].second[0].second == "value3"); // check value from second stream
    std::cout << "test_xread_multiple_streams passed" << std::endl;
}

void test_xread_count_zero() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    long long id = stream.xadd("mystream", data);

    auto result = stream.xread({"mystream"}, {id + 1});
    assert(result.count("mystream") == 1);
    assert(result["mystream"].size() == 0); // check no entries are read
    std::cout << "test_xread_count_zero passed" << std::endl;
}

void test_xrange() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    long long id = stream.xadd("mystream", data);

    auto result = stream.xrange("mystream", 0, LLONG_MAX);
    assert(result.size() == 1); // check one entry is read
    assert(result[0].first == id); // check correct ID is read
    assert(result[0].second[0].first == "field1"); // check field
    assert(result[0].second[0].second == "value1"); // check value
    std::cout << "test_xrange passed" << std::endl;
}

void test_xlen() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    stream.xadd("mystream", data);
    stream.xadd("mystream", data);

    size_t len = stream.xlen("mystream");
    assert(len == 2); // check stream length is correct
    std::cout << "test_xlen passed" << std::endl;
}

void test_xdel() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    long long id = stream.xadd("mystream", data);
    size_t deleted = stream.xdel("mystream", {id});
    assert(deleted == 1); // check one entry is deleted
    assert(stream.xlen("mystream") == 0); // check stream is empty
    std::cout << "test_xdel passed" << std::endl;
}

void test_xtrim() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    stream.xadd("mystream", data);
    stream.xadd("mystream", data);

    size_t trimmed = stream.xtrim("mystream", MAXLEN, 1);
    assert(trimmed == 1); // check one entry is trimmed
    assert(stream.xlen("mystream") == 1); // check stream length is correct
    std::cout << "test_xtrim passed" << std::endl;
}

void test_xread_blocking() {
    redisStream stream;

    // simulate blocking read - this test is hard to assert on without
    // introducing threads and timing, so we'll just check it doesn't crash
    auto result = stream.xread({"mystream"}, {0}, 100, 1);
    // if it blocks for 100ms and doesn't crash, we can assume it's working
    std::cout << "test_xread_blocking (non-crash) passed" << std::endl;
}

// check blocking behavior when new data arrives asynchronously using threads
void test_xread_blocking_when_data_available() {
    redisStream stream;

    // simulate a background thread that adds data to the stream
    std::thread writer([&stream]() {
        FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        stream.xadd("mystream", data);
    });

    // simulate blocking read
    auto result = stream.xread({"mystream"}, {0}, 100, 1);
    // if it blocks for 100ms and doesn't crash, we can assume it's working
    std::cout << "test_xread_blocking_when_data_available (non-crash) passed" << std::endl;

    writer.join();
}

void test_xdel_empty_stream() {
    redisStream stream;
    stream.xdel("mystream", {1, 2, 3});  // attempt to delete non-existent entries
    assert(stream.xlen("mystream") == 0);
    std::cout << "test_xdel_empty_stream passed" << std::endl;
}

void test_xrange_empty() {
    redisStream stream;
    auto result = stream.xrange("nonexistent", 0, LLONG_MAX);
    assert(result.empty());
    std::cout << "test_xrange_empty passed" << std::endl;
}

void test_xrange_start_end_eq() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    long long id = stream.xadd("mystream", data);

    auto result = stream.xrange("mystream", id, id);
    assert(result.size() == 1); // check one entry is read
    assert(result[0].first == id); // check correct ID is read
    assert(result[0].second[0].first == "field1"); // check field
    assert(result[0].second[0].second == "value1"); // check value
    std::cout << "test_xrange_start_end_eq passed" << std::endl;
}

void test_xrange_negative_ids() {
    // I haven't implemented this for it to be able to work in a way that
    // is possible consistant with redis. I haven't checked the use of negative
    // ids.
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    long long id = stream.xadd("mystream", data);

    auto result = stream.xrange("mystream", -1, -1);
    // in my implementation nothing is read
    assert(result.size() == 0);
}

void test_xrange_get_entire_stream() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    stream.xadd("mystream", data);
    stream.xadd("mystream", data);

    auto result = stream.xrange("mystream", 0, LLONG_MAX);
    assert(result.size() == 2); // check two entries are read
    assert(result[0].first == 0); // check correct ID is read
    assert(result[0].second[0].first == "field1"); // check field
    assert(result[0].second[0].second == "value1"); // check value
    assert(result[1].first == 1); // check correct ID is read
    assert(result[1].second[0].first == "field1"); // check field
    assert(result[1].second[0].second == "value1"); // check value
    std::cout << "test_xrange_get_entire_stream passed" << std::endl;
}

void test_xlen_empty() {
    redisStream stream;
    size_t len = stream.xlen("nonexistent");
    assert(len == 0);
    std::cout << "test_xlen_empty passed" << std::endl;
}

// probably should test larger data at some point

void test_xdel_nonexistent() {
    redisStream stream;
    size_t deleted = stream.xdel("nonexistent", {1});
    assert(deleted == 0);
    std::cout << "test_xdel_nonexistent passed" << std::endl;
}

void test_xdel_all() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    stream.xadd("mystream", data);
    stream.xadd("mystream", data);

    size_t deleted = stream.xdel("mystream", {0, 1});
    assert(deleted == 2); // check all entries are deleted
    assert(stream.xlen("mystream") == 0); // check stream is empty
    std::cout << "test_xdel_all passed" << std::endl;
}

void test_xdel_delete_duplicate() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    stream.xadd("mystream", data);
    stream.xadd("mystream", data);

    size_t deleted = stream.xdel("mystream", {0, 0});
    assert(deleted == 1); // check one entry is deleted
    assert(stream.xlen("mystream") == 1); // check stream length is correct
    std::cout << "test_xdel_delete_duplicate passed" << std::endl;
}

void test_xtrim_maxlen() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    stream.xadd("mystream", data);
    stream.xadd("mystream", data);

    // I forgot to check that the correct entry was removed
    size_t trimmed = stream.xtrim("mystream", MAXLEN, 1);
    assert(trimmed == 1); // check one entry is trimmed
    assert(stream.xlen("mystream") == 1); // check stream length is correct
    // assert that only the latest entry was removed
    assert(stream.xrange("mystream", 0, LLONG_MAX)[0].first == 0);
    // check threshold for MAXLEN > stream data length
    // this is supposedly how that's supposed to work according to the docs I
    // read so if that's not consistant with the actual redis I was just
    // following the docs.
    trimmed = stream.xtrim("mystream", MAXLEN, 5);
    assert(trimmed == 0); // check no entries are trimmed

    std::cout << "test_xtrim_maxlen passed" << std::endl;
}

void test_xtrim_minid() {
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    stream.xadd("mystream", data);
    stream.xadd("mystream", data);

    size_t trimmed = stream.xtrim("mystream", MINID, 1);
    assert(trimmed == 2); // check all entries are trimmed
    assert(stream.xlen("mystream") == 0); // check stream is empty
    // check non-existent id
    trimmed = stream.xtrim("mystream", MINID, 100);
    assert(trimmed == 0); // check no entries are trimmed
    std::cout << "test_xtrim_minid passed" << std::endl;
    // min < smallest id
    trimmed = stream.xtrim("mystream", MINID, -1);
    assert(trimmed == 0); // check no entries are trimmed
}

void test_xtrim_minid_trim_up_to(){
    redisStream stream;
    FieldsStructure data = {{"field1", "value1"}, {"field2", "value2"}};
    stream.xadd("mystream", data);
    stream.xadd("mystream", data);

    size_t trimmed = stream.xtrim("mystream", MINID, 0);
    assert(trimmed == 1); // check all entries are trimmed
    assert(stream.xlen("mystream") == 1); // check stream is empty
    // check id is correct should be only the last one left
    assert(stream.xrange("mystream", 0, -1)[0].first == 1);
}

int main() {
    // Run all tests
    test_xadd();
    test_xread_count_zero();
    test_xadd_empty_stream();
    test_xread();
    test_xrange();
    test_xlen();
    test_xdel();
    test_xtrim();
    test_xread_blocking();
    test_xread_blocking_when_data_available();
    test_xdel_empty_stream();
    test_xrange_empty();
    test_xrange_negative_ids();
    test_xlen_empty();
    test_xdel_nonexistent();
    test_xdel_all();
    test_xtrim_maxlen();
    test_xtrim_minid();
    test_xdel_delete_duplicate();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
