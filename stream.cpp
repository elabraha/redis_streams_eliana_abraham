#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <optional>
#include <unordered_map>
#include <condition_variable>
#include <chrono>
#include <climits>

using FieldsStructure = std::vector<std::pair<std::string, std::string>>;
using MapStructure = std::map<long long, FieldsStructure>;
using StreamDataStructure = std::map<std::string, MapStructure>;
using VectorPairStructure = std::vector<std::pair<long long, FieldsStructure>>;
using ResultStructure = std::map<std::string, VectorPairStructure>;

enum trimmingStrategy
{
    MAXLEN,
    MINID,
};

class redisStream
{
private:
    std::mutex mutex_; // switch to shared mutex and unique and shared lock
    // in the future to better allow concurrent readers
    StreamDataStructure stream_data_;
    std::condition_variable new_data_condition_;
    // not globally unique and probably should use timestamps instead but
    // I'm keeping it simple with sequence number instead
    // this is a toy problem and I don't plan on testing this with very
    // amounts of data this is a single instance and stores in memory
    // however, I still made the id and counter long long types
    std::unordered_map<std::string, std::atomic<long long>> counters_;
    // don't know if I'll be using this for xread
    std::unordered_map<std::string, std::atomic<long long>> most_recent_ids_;

    // I need to only call this for a particular stream otherwise it's a
    // different count.
    long long generate_id_(const std::string &stream_name)
    {
        return counters_[stream_name].fetch_add(1);
    }

    long long set_most_recent_id_(const std::string &stream_name, long long id)
    {
        most_recent_ids_[stream_name] = id;
        return id;
    }
    long long get_most_recent_id_(const std::string &stream_name)
    {
        return most_recent_ids_[stream_name].load();
    }

    ResultStructure get_results_(std::unique_lock<std::mutex> &lock,
                                 const std::vector<std::string> &stream_names,
                                 const std::vector<long long> &last_ids,
                                 std::optional<long long> count = std::nullopt)
    {
        ResultStructure result;
        auto it_sns = stream_names.begin();
        auto it_ids = last_ids.begin();
        for (; it_sns != stream_names.end() && it_ids != last_ids.end();
             it_sns++, it_ids++)
        {
            const std::string &stream_name = *it_sns;
            long long start_id = *it_ids;
            if (stream_data_.find(stream_name) != stream_data_.end())
            {
                result[stream_name] = {};
                auto it_start = stream_data_[stream_name].lower_bound(start_id);
                for (auto it = it_start;
                     it != stream_data_[stream_name].end();
                     it++)
                {
                    result[stream_name].push_back(std::make_pair(it->first,
                                                                 it->second));
                    // stop at count
                    if (count && result[stream_name].size() >= *count)
                        break;
                }
            }
        }
        return result;
    }
    // helper function to trim entries from the stream
    size_t trim_entries_(StreamDataStructure::mapped_type &stream,
                         long long threshold,
                         bool is_maxlen)
    {
        size_t count = 0;
        auto end = stream.end();
        auto it = end;
        if (!is_maxlen) {
            it = stream.upper_bound(threshold);
        }
        while (it != stream.begin())
        {
            it--; // decrement iterator before potential erase
            if (is_maxlen && count >= threshold)
                break;
            if (!is_maxlen && count > threshold)
                break;

            stream.erase(it->first); // erase by key
            count++;
            end = stream.end(); // update end after erase
            it = end;
        }
        return count;
    }

public:
    // not sure what to do with the constructor classes yet
    redisStream() {}
    ~redisStream() {}

    // Not exactly xadd verbatim, I decided not to allow users to specify as an
    // argument, xadd will always generate one for you. also no other args added
    // like what's expected for Redis' actual xadd
    long long xadd(const std::string &stream_name,
                   const FieldsStructure &data)
    {

        std::lock_guard<std::mutex> lock(mutex_);
        long long id = generate_id_(stream_name);
        stream_data_[stream_name].insert({id, data});
        set_most_recent_id_(stream_name, id);
        new_data_condition_.notify_all();
        return id;
    }

    ResultStructure xread(
        const std::vector<std::string> &stream_names,
        const std::vector<long long> &last_ids,
        std::optional<long long> block_time = std::nullopt,
        std::optional<long long> count = std::nullopt)
    {

        ResultStructure result;
        std::unique_lock<std::mutex> lock(mutex_);

        result = get_results_(lock, stream_names, last_ids, count);

        if (block_time)
        {
            // if result is empty wait, else return result
            // this is going to be hard to test I might run out of time
            if (result.empty())
            {
                new_data_condition_.wait_for(
                    lock, std::chrono::milliseconds(*block_time),
                    [this, &lock, &stream_names, &last_ids, &count, &result]
                    {
                        result = get_results_(lock,
                                              stream_names,
                                              last_ids,
                                              count);
                        // notify
                        return !result.empty();
                    });
            }
        }
        // implement block later if I have time.
        return result;
    }

    VectorPairStructure xrange(const std::string &stream_name,
                               long long start_id = LLONG_MIN,
                               long long end_id = LLONG_MAX,
                               std::optional<long long> count = std::nullopt)
    {

        VectorPairStructure result;
        std::lock_guard<std::mutex> lock(mutex_);

        // I meant to implement the special id symbol $ where you can use
        // that to get the most recently added message id.
        // However, I'm not sure I'm going to have enough time to do that
        // yet.

        if (stream_data_.find(stream_name) != stream_data_.end())
        {
            auto it_start = stream_data_[stream_name].lower_bound(start_id);
            for (auto it = it_start;
                 it != stream_data_[stream_name].end() && it->first <= end_id;
                 ++it)
            {
                result.push_back(std::make_pair(it->first, it->second));
                // stop at count is also optional
                if (count && result.size() >= static_cast<size_t>(*count))
                    break;
                // if end_id is reached, stop
                if (it->first > end_id)
                    break;
            }
        }
        return result;
    }

    size_t xlen(const std::string &stream_name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_data_[stream_name].size();
    }
    /* I am not implementing this as a radix tree so deletes are a lot
    easier. don't have to delay deletion until all "macro nodes" are
    deleted internal handling of map deletion which underneath is a red
    black tree is good enough for a toy project. building a radix tree
    from scratch is not something I can do in a short time and I don't
    know of any particular libraries that implement thread safe radix trees.
    */
    size_t xdel(const std::string &stream_name,
                const std::vector<long long> &ids)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // iterate ids and delete one by one
        size_t entries_deleted = 0;
        for (auto id : ids)
        {
            // check if key exists first I'm guessing we don't count
            // keys that have already been deleted or don't exist.
            auto found = stream_data_[stream_name].find(id);
            if (found != stream_data_[stream_name].end())
            {
                stream_data_[stream_name].erase(id);
                entries_deleted++;
            }
        }
        return entries_deleted;
    }
    // xtrim with only basic required arguments because all those optional
    // arguments take a lot more time and I only really want to implement
    // base functionality for now.
    size_t xtrim(const std::string &stream_name,
                 trimmingStrategy strategy,
                 long long threshold)
    {
        // for MAXLEN arg is threshold, for MINID it's id.
        if (strategy == MAXLEN)
        {
            // trims latest 1000 which means maybe the last iterator
            // order is preserved for maps by keys, it's sorted and the
            // higher the id number the more recent it is.

            // only trim when the length exceeds the threshold
            if (xlen(stream_name) > threshold)
            {
                return trim_entries_(
                    stream_data_[stream_name],
                    threshold,
                    true);
            }
        }
        else if (strategy == MINID)
        {
            // trims where upper bound is min id and evicts any ids where they
            // lower than the min id
            // threshold would be acting as id to start from
            return trim_entries_(stream_data_[stream_name], threshold, false);
        }
        return 0;
    }
};
