// create an interface with commands to run

// probably want to use a header file instead of direct file
#include "stream.cpp"
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <iomanip> // quoted
#include <iterator>

// I probably would have just used boosts tokenizer if I didn't want to do this.
std::vector<std::string> tokenize_whitespace(const std::string& text) {
    std::stringstream ss(text);
    std::string token;
    std::vector<std::string> tokens;
    while (ss >> std::quoted(token)) {
        tokens.push_back(token);
    }
    return tokens;
}

// parse long long from string
static bool parse_ll(const std::string& s, long long &out) {
    if (s.empty()) return false;
    std::stringstream ss(s);
    ss >> out;
    return !ss.fail() && ss.eof();
}

static bool parse_bound(const std::string& s, long long &out) {
    if (s == "-") { out = LLONG_MIN; return true; }
    if (s == "+") { out = LLONG_MAX; return true; }
    return parse_ll(s, out);
}

void command_interpreter(const std::vector<std::string>& toks, redisStream& stream) {
    if (toks.empty()) return;
    const std::string &op = toks[0];

    if (op == "XADD") {
        if (toks.size() < 2) { std::cerr << "XADD requires a key\n"; return; }
        const std::string key = toks[1];
        FieldsStructure data;
        for (size_t i = 2; i < toks.size(); i += 2) {
            const std::string &field = toks[i];
            const std::string value = (i + 1 < toks.size()) ? toks[i + 1] : std::string{};
            data.emplace_back(field, value);
        }
        std::cout << stream.xadd(key, data) << std::endl;
        return;
    } else if (op == "XREAD") {
        std::optional<long long> count, block_time;
        size_t i = 1;
        // parse options until STREAMS
        for (; i < toks.size(); ++i) {
            const std::string &tk = toks[i];
            if (tk == "COUNT" || tk == "BLOCK") {
                if (i + 1 >= toks.size()) { std::cerr << tk << " needs a number\n"; return; }
                long long v;
                if (!parse_ll(toks[i + 1], v)) { std::cerr << tk << " invalid number: " << toks[i + 1] << '\n'; return; }
                if (tk == "COUNT") count = v; else block_time = v;
                ++i;
            } else if (tk == "STREAMS") {
                ++i;
                break;
            } else {
                std::cerr << "Unknown XREAD option: " << tk << '\n'; return;
            }
        }

        if (i >= toks.size()) { std::cerr << "XREAD STREAMS requires stream names and ids\n"; return; }
        // remaining tokens: keys ... ids ...
        std::vector<std::string> rest(toks.begin() + i, toks.end());
        // find split point the first token that parses as long long
        size_t split = rest.size();
        long long tmp;
        for (size_t j = 0; j < rest.size(); ++j) {
            if (parse_ll(rest[j], tmp)) { split = j; break; }
        }
        if (split == rest.size()) { std::cerr << "STREAMS requires ids after keys\n"; return; }
        std::vector<std::string> keys(rest.begin(), rest.begin() + split);
        std::vector<long long> ids;
        // since I know the number of ids I'm going to be adding might as well
        // reserve for a minor boost allocating capacity only once instead
        ids.reserve(rest.size() - split);
        for (size_t j = split; j < rest.size(); ++j) {
            long long id;
            if (!parse_ll(rest[j], id)) { std::cerr << "Invalid id: " << rest[j] << '\n'; return; }
            ids.push_back(id);
        }
        if (keys.size() != ids.size()) { std::cerr << "Number of keys and ids must match\n"; return; }
        // run command xread
        auto result = stream.xread(keys, ids, block_time, count);
        // pretty print results
        size_t key_index = 1;
        for (const auto &p : result) {
            std::cout << key_index << ") \"" << p.first << "\"\n";
            size_t id_index = 0;
            for (const auto &entry : p.second) {
                std::cout << "\t" << id_index << ") " << entry.first << '\n';
                size_t field_idx = 1;
                for (const auto &fv : entry.second) {
                    std::cout << "\t\t" << field_idx << ") " << fv.first << ":" << fv.second << '\n';
                    ++field_idx;
                }
                ++id_index;
            }
            ++key_index;
        }
        return;
    } else if (op == "XRANGE") {
        if (toks.size() < 4) { std::cerr << "XRANGE requires key, start, and end\n"; return; }
        const std::string key = toks[1];
        long long start_id, end_id;
        if (!parse_bound(toks[2], start_id)) { std::cerr << "Invalid start id\n"; return; }
        if (!parse_bound(toks[3], end_id)) { std::cerr << "Invalid end id\n"; return; }
        std::optional<long long> count;
        if (toks.size() >= 6) {
            if (toks[4] != "COUNT") { std::cerr << "Unrecognized XRANGE option: " << toks[4] << '\n'; return; }
            long long v;
            if (!parse_ll(toks[5], v)) { std::cerr << "Invalid COUNT\n"; return; }
            count = v;
        }

        auto result = stream.xrange(key, start_id, end_id, count);
        size_t id_index = 1;
        for (const auto &pr : result) {
            std::cout << id_index << ") " << pr.first << '\n';
            size_t field_index = 1;
            for (const auto &fv : pr.second) {
                std::cout << '\t' << field_index << ") " << fv.first << ":" << fv.second << '\n';
                ++field_index;
            }
            std::cout << '\n';
            ++id_index;
        }
        return;
    } else if (op == "XLEN") {
        if (toks.size() != 2) { std::cerr << "XLEN requires a key\n"; return; }
        std::cout << stream.xlen(toks[1]) << std::endl;
        return;
    } else if (op == "XDEL") {
        if (toks.size() < 3) { std::cerr << "XDEL requires key and at least one id\n"; return; }
        const std::string key = toks[1];
        std::vector<long long> ids;
        for (size_t j = 2; j < toks.size(); ++j) {
            long long v;
            if (!parse_ll(toks[j], v)) { std::cerr << "Invalid id: " << toks[j] << '\n'; return; }
            ids.push_back(v);
        }
        std::cout << stream.xdel(key, ids) << std::endl;
        return;
    } else if (op == "XTRIM") {
        if (toks.size() < 4) { std::cerr << "XTRIM requires key, strategy, and threshold\n"; return; }
        const std::string key = toks[1];
        const std::string strategy = toks[2];
        long long threshold;
        if (!parse_ll(toks[3], threshold)) { std::cerr << "Invalid threshold\n"; return; }
        if (strategy == "MINID") {
            std::cout << stream.xtrim(key, MINID, threshold) << std::endl;
        } else if (strategy == "MAXLEN") {
            // shouldn't run this when max length threshold exceeds current length
            std::cout << stream.xtrim(key, MAXLEN, threshold) << std::endl;
        } else {
            std::cerr << "Unknown XTRIM strategy: " << strategy << '\n';
        }
        return;
    }
    std::cerr << "Unknown command: " << op << '\n';
}

int main() {
    std::cout << "Hi there, welcome to my toy redis stream implementation!\n";
    std::cout << "Type a command!\n";
    redisStream stream;
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        auto toks = tokenize_whitespace(line);
        if (toks.empty()) continue;
        command_interpreter(toks, stream);
    }
    return 0;
}
