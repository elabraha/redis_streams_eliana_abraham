// create an interface with commands to run

// probably want to use a header file instead of direct file
#include "stream.cpp"
#include <iostream>
#include <sstream>
#include <map>
#include <string>
#include <stdexcept>

void command_interpreter(std::string command, redisStream& stream) {

    // find a way to do with with function pointers and arguments
    // rather than directly printing to the command line
    // or maybe a generic return type idk yet
    std::stringstream ss(command);
    std::string op;
    ss >> op;
    if (op == "XADD") {
        std::string key;
        ss >> key;
        std::string field, value;
        FieldsStructure data;
        while (ss >> field) {
            if (!(ss >> value)) {
                // odd number of tokens: treat missing value as empty string
                value.clear();
            }
            data.emplace_back(field, value);
        }
        std::cout << stream.xadd(key, data) << std::endl;
    } else if (op == "XREAD") {
        std::string optional_arg;
        std::optional<long long> count, block_time;
        while (ss >> optional_arg) {
            long long number_input;
            if (optional_arg == "BLOCK" || optional_arg == "COUNT") {
                try {
                    ss >> number_input;
                } catch (const std::invalid_argument& e) {
                    std::cerr << optional_arg + " provided is invalid" << std::endl;
                    return;
                } catch (const std::out_of_range& e) {
                    std::cerr <<  optional_arg + " is out of range" << std::endl;
                    return;
                } catch (const std::exception& e) {
                    std::cerr <<  optional_arg + " provided was caught by std exception" << std::endl;
                    return;
                } catch (...) {
                    std::cerr <<  optional_arg + " has unknown exception." << std::endl;
                    return;
                }
                count = (optional_arg == "COUNT") ? std::optional<long long>(number_input) : std::nullopt;
                block_time = (optional_arg == "BLOCK") ? std::optional<long long>(number_input) : std::nullopt;          
            } else if (optional_arg == "STREAMS") {
                std::string key_or_id;
                long long id;
                std::vector<std::string> keys;
                std::vector<long long> list_ids;
                while (ss >> key_or_id) {
                    /* need to determine when I have reached the end of a stream
                    name and probably the best way is to actually use the list
                    of existing stream names but then we run into other errors
                    with ids if you include a fake stream name or one that does
                    not exist. maybe I try to convert to a long long and if it
                    does convert then it's an id. maybe idk that seems not the
                    best but the other way doesn't seem great either. */
                    try {
                        id = std::stoll(key_or_id);
                        list_ids.push_back(id);
                        break;
                    } catch (std::exception& e) {
                        //continue to add keys
                        keys.push_back(key_or_id);
                    }
                }
                while(ss >> id) {
                    /* I have to do this because you can't have interlaces ids
                    and keys as far as I know.*/
                    try {
                        id = std::stoll(key_or_id);
                        list_ids.push_back(id);
                        break;
                    } catch (...) {
                        /* being a bit more general here I don't feel like
                        making more error messages.*/
                        std::cerr << "An exception while trying to convert id to long long" << std::endl;
                    }
                }
                // verify vectors are the same length
                if (keys.size() == list_ids.size()) {
                    // don't feel like using the full type
                    auto result = stream.xread(keys, list_ids, block_time, count);
                    size_t key_index = 1;
                    // the output on the actual redis cli is so ugly so I'm
                    // going to ignore it and do what I like.
                    for (auto [key, list_data]: result) {
                        std::cout << key_index << ") ";
                        std::cout << "\"" + key + "\"" << std::endl;
                        size_t id_index = 0;
                        for (const auto& entry: list_data) {
                            std::cout << "\t" << id_index << ") ";
                            std::cout << entry.first << std::endl;
                            size_t field_idx = 1;
                            for (auto [key, val]: entry.second) {
                                std::cout << "\t\t" << field_idx << ") ";
                                std::cout << key << ":" << val << std::endl;
                                field_idx++;
                            }
                            id_index++;
                        }
                        key_index++;
                    }
                } else {
                    std::cerr << "Did not provide ids for every stream or\n";
                    std::cerr << "provided too many." << std::endl;
                }
            } else {
                // better error but I don't want to stop the program so I print to
                // std error
                std::string error_msg = " This option does not exist.";
                std::cerr << error_msg << std::endl;
            }
        }
    } else if (op == "XRANGE") {
        // handle xrange command
        std::string key;
        std::string start_id_str;
        std::string end_id_str;
        long long start_id, end_id;
        if (ss >> key >> start_id_str >> end_id_str) {
            // or we are give + or minus instead of numbers
            if (start_id_str != "-" && start_id_str != "+") {
                try {
                    start_id = std::stoll(start_id_str);
                } catch (const std::invalid_argument& e) {
                    std::cerr << "Start ID provided is invalid" << std::endl;
                    return;
                } catch (const std::out_of_range& e) {
                    std::cerr << "Start ID is out of range" << std::endl;
                    return;
                } catch (const std::exception& e) {
                    std::cerr << "Start ID provided was caught by std exception" << std::endl;
                    return;
                } catch (...) {
                    std::cerr << "Start ID has unknown exception." << std::endl;
                    return;
                }
            } else if (start_id_str == "+") {
                // Handle special case for start ID where it is max
                // likely this will return an empty range
                start_id = LLONG_MAX;
            } else if (start_id_str == "-") {
                // Handle special case for start ID where it is min
                // basically this will likely return the full range
                start_id = LLONG_MIN;
            }
            if (end_id_str != "-" && end_id_str != "+") {
                try {
                    end_id = std::stoll(end_id_str);
                } catch (const std::invalid_argument& e) {
                    std::cerr << "End ID provided is invalid" << std::endl;
                    return;
                } catch (const std::out_of_range& e) {
                    std::cerr << "End ID is out of range" << std::endl;
                    return;
                } catch (const std::exception& e) {
                    std::cerr << "End ID provided was caught by std exception" << std::endl;
                    return;
                } catch (...) {
                    std::cerr << "End ID has unknown exception." << std::endl;
                    return;
                }
            } else if (end_id_str == "-") {
                // Handle special case for end ID where it is min
                // basically this will likely return an empty range
                end_id = LLONG_MIN;
            } else if (end_id_str == "+") {
                // Handle special case for end ID where it is max
                // likely this will return the full range
                end_id = LLONG_MAX;
            }
        }
        // add optional count to read
        std::string count_str;
        std::optional<long long> count;
        try {
            if (ss >> count_str && count_str == "COUNT" && ss >> count_str) {
                count = std::stoll(count_str);
                // not going to verify numbers if you put in the wrong count that's
                // on you.
            }
        } catch (const std::exception& e) {
            std::cerr << "Count provided was caught by std exception" << std::endl;
            return;
        } catch (...) {
            std::cerr << "Count has unknown exception." << std::endl;
            return;
        }
        // by default the range is set unbounded with start and end
        // as LLONG_MIN and LLONG_MAX
        auto result = stream.xrange(key, start_id, end_id, count);
        size_t id_index = 1;
        for (const auto& [id, fields] : result) {
            std::cout << id_index << ") ";
            std::cout << id << std::endl;
            size_t field_index = 1;
            for (const auto& [field, value] : fields) {
                // why are not all fields printing?
                std::cout << '\t' << field_index << ") ";
                std::cout << field << ":" << value << std::endl;
                field_index++;
            }
            std::cout << std::endl;
            id_index++;
        }
    } else if(op == "XLEN"){
        std::string key;
        ss >> key;
        std::cout << stream.xlen(key) << std::endl;
    } else if(op == "XDEL"){
        // probably should error if you add anything else but meh
        std::string key;
        ss >> key;
        long long id;
        std::vector<long long> ids;
        while(ss >> id) {
            ids.push_back(id);
        }
        std::cout << stream.xdel(key, ids) << std::endl;
    } else {
        // better way to deal with errors but I don't have time right now
        std::cerr << "Command does not exist." << std::endl;
    }
}

int main() {
    std::cout << "Hi there, welcome to my toy redis stream implementation!" << std::endl;
    std::cout << "Type a command!" << std::endl;
    std::string command;
    redisStream stream;
    while(true)
    {
        std::cout << "> ";
        std::getline(std::cin, command);
        command_interpreter(command, stream);
    }
}
