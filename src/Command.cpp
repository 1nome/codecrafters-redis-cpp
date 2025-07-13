#include "Command.h"
#include "Resp.h"
#include "Database.h"

#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <ranges>

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
    if (resp.array.size() < 2)
    {
        return bulk_string("");
    }

    return bulk_string(resp.array[1].string);
}

std::string set(const RESP_data& resp)
{
    if (resp.array.size() < 3)
    {
        return bulk_string("");
    }

    key_vals[resp.array[1].string] = resp.array[2].string;
    if (key_expiry.contains(resp.array[1].string))
    {
        key_expiry.erase(resp.array[1].string);
    }

    if (resp.array.size() > 4)
    {
        std::string mod = resp.array[3].string;
        to_upper(mod);
        if (mod == "PX")
        {
            key_expiry[resp.array[1].string] = std::chrono::system_clock::now() + std::chrono::milliseconds(
            std::stoi(resp.array[4].string));
        }
    }

    return OK_simple;
}

bool is_active(const std::string& key)
{
    if (!key_expiry.contains(key))
    {
        return true;
    }
    if (key_expiry[key] >= std::chrono::system_clock::now())
    {
        return true;
    }
    return false;
}

void remove_key(const std::string& key)
{
    key_expiry.erase(key);
    key_vals.erase(key);
}

std::string get(const RESP_data& resp)
{
    if (resp.array.size() < 2)
    {
        return bulk_string("");
    }

    if (key_vals.contains(resp.array[1].string))
    {
        if (is_active(resp.array[1].string))
        {
            return bulk_string(key_vals[resp.array[1].string]);
        }
        remove_key(resp.array[1].string);
    }
    return null_bulk_string;
}

std::string config_get(const RESP_data& resp)
{
    std::vector<std::string> ret;
    for (size_t i = 2; i < resp.array.size(); i++)
    {
        ret.push_back(bulk_string(resp.array[i].string));
        if (config_key_vals.contains(resp.array[i].string))
        {
            ret.push_back(bulk_string(config_key_vals[resp.array[i].string]));
            continue;
        }
        ret.push_back(null_bulk_string);
    }
    return array(ret);
}

const std::unordered_map<std::string, Cmd> config_cmd_map = {
    {"GET", config_get}
};

std::string config(const RESP_data& resp)
{
    if (resp.array.size() < 2)
    {
        return bulk_string("");
    }

    std::string cmd = resp.array[1].string;
    to_upper(cmd);
    if (!config_cmd_map.contains(cmd))
    {
        // temp value
        return bulk_string("");
    }
    return config_cmd_map.at(cmd)(resp);
}

std::string keys(const RESP_data& resp)
{
    if (resp.array.size() < 2)
    {
        return bulk_string("");
    }

    if (resp.array[1].string != "*")
    {
        return bulk_string("Filtering not supported yet :)");
    }

    std::vector<std::string> ret;
    std::vector<std::string> expired_keys;
    for (const auto& key : key_vals | std::views::keys)
    {
        if (is_active(key))
        {
            ret.push_back(bulk_string(key));
        }
        else
        {
            expired_keys.push_back(key);
        }
    }
    for (const auto& key : expired_keys)
    {
        remove_key(key);
    }
    return array(ret);
}

const std::unordered_map<std::string, Cmd> cmd_map = {
    {"PING", ping},
    {"ECHO", echo},
    {"SET", set},
    {"GET", get},
    {"CONFIG", config},
    {"KEYS", keys}
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
