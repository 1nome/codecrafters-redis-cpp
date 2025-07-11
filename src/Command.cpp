#include "Command.h"
#include "Resp.h"
#include "Database.h"

#include <unordered_map>
#include <algorithm>

typedef std::string (*Cmd)(const RESP_data&);

void to_upper(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), toupper);
}

std::string ping(const RESP_data& resp)
{
    if (resp.array.size() > 1)
    {
        return bulk_string(resp.array[1].string);
    }
    return simple_string("PONG");
}

std::string echo(const RESP_data& resp)
{
    if (resp.array.size() > 1)
    {
        return bulk_string(resp.array[1].string);
    }
    return bulk_string("");
}

std::string set(const RESP_data& resp)
{
    if (resp.array.size() > 2)
    {
        key_vals[resp.array[1].string].first = resp.array[2].string;
        key_vals[resp.array[1].string].second = std::chrono::time_point<std::chrono::system_clock>::max();
        if (resp.array.size() > 4)
        {
            std::string mod = resp.array[3].string;
            to_upper(mod);
            if (mod == "PX")
            {
                key_vals[resp.array[1].string].second = std::chrono::system_clock::now() + std::chrono::milliseconds(
                    std::stoi(resp.array[4].string));
            }
        }
        return OK_simple;
    }
    return bulk_string("");
}

std::string get(const RESP_data& resp)
{
    if (resp.array.size() > 1)
    {
        if (key_vals.contains(resp.array[1].string))
        {
            if (key_vals[resp.array[1].string].second >= std::chrono::system_clock::now())
            {
                return bulk_string(key_vals[resp.array[1].string].first);
            }
        }
        return null_bulk_string;
    }
    return bulk_string("");
}

const std::unordered_map<std::string, Cmd> cmd_map = {
    {"PING", ping},
    {"ECHO", echo},
    {"SET", set},
    {"GET", get}
};

std::string process_command(const RESP_data& resp)
{
    std::string cmd = resp.array[0].string;
    to_upper(cmd);
    if (!cmd_map.contains(cmd))
    {
        // temp value
        return bulk_string("");
    }
    return cmd_map.at(cmd)(resp);
}
