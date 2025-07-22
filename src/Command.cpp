#include "Command.h"
#include "Resp.h"
#include "Database.h"
#include "Replication.h"

#include <unordered_map>
#include <algorithm>
#include <ranges>
#include <thread>

const std::string bad_cmd = bulk_string("bad command");

typedef std::string (*Cmd)(const RESP_data&, Rel_data&);

void to_upper(std::string& s)
{
    std::ranges::transform(s, s.begin(), toupper);
}

std::string ping(const RESP_data& resp, Rel_data& data)
{
    data.repeat = false;
    if (resp.array.size() > 1)
    {
        return bulk_string(resp.array[1].string);
    }

    return simple_string("PONG");
}

std::string echo(const RESP_data& resp, Rel_data& data)
{
    data.repeat = false;
    if (resp.array.size() < 2)
    {
        return bad_cmd;
    }

    return bulk_string(resp.array[1].string);
}

std::string set(const RESP_data& resp, Rel_data& data)
{
    if (resp.array.size() < 3)
    {
        return bad_cmd;
    }

    data.repeat = true;
    key_vals()[resp.array[1].string] = resp.array[2].string;
    if (key_expiry().contains(resp.array[1].string))
    {
        key_expiry().erase(resp.array[1].string);
    }

    if (resp.array.size() > 4)
    {
        std::string mod = resp.array[3].string;
        to_upper(mod);
        if (mod == "PX")
        {
            key_expiry()[resp.array[1].string] = std::chrono::system_clock::now() + std::chrono::milliseconds(
            std::stoi(resp.array[4].string));
        }
    }

    return OK_simple;
}

bool is_active(const std::string& key)
{
    if (!key_expiry().contains(key))
    {
        return true;
    }
    if (key_expiry()[key] >= std::chrono::system_clock::now())
    {
        return true;
    }
    return false;
}

void remove_key(const std::string& key)
{
    key_expiry().erase(key);
    key_vals().erase(key);
}

std::string get(const RESP_data& resp, Rel_data& data)
{
    data.repeat = false;
    if (resp.array.size() < 2)
    {
        return bad_cmd;
    }

    if (key_vals().contains(resp.array[1].string))
    {
        if (is_active(resp.array[1].string))
        {
            return bulk_string(key_vals()[resp.array[1].string]);
        }
        remove_key(resp.array[1].string);
    }
    return null_bulk_string;
}

std::string config_get(const RESP_data& resp, Rel_data& data)
{
    std::vector<std::string> ret;
    for (size_t i = 2; i < resp.array.size(); i++)
    {
        ret.push_back(bulk_string(resp.array[i].string));
        if (config_key_vals().contains(resp.array[i].string))
        {
            ret.push_back(bulk_string(config_key_vals()[resp.array[i].string]));
            continue;
        }
        ret.push_back(null_bulk_string);
    }
    return array(ret);
}

const std::unordered_map<std::string, Cmd> config_cmd_map = {
    {"GET", config_get}
};

std::string config(const RESP_data& resp, Rel_data& data)
{
    data.repeat = false;
    if (resp.array.size() < 2)
    {
        return bad_cmd;
    }

    std::string cmd = resp.array[1].string;
    to_upper(cmd);
    if (!config_cmd_map.contains(cmd))
    {
        // temp value
        return bad_cmd;
    }
    return config_cmd_map.at(cmd)(resp, data);
}

std::string keys(const RESP_data& resp, Rel_data& data)
{
    data.repeat = false;
    if (resp.array.size() < 2)
    {
        return bad_cmd;
    }

    if (resp.array[1].string != "*")
    {
        return bulk_string("Filtering not supported yet :)");
    }

    std::vector<std::string> ret;
    std::vector<std::string> expired_keys;
    for (const auto& key : key_vals() | std::views::keys)
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

std::string info(const RESP_data& resp, Rel_data& data)
{
    data.repeat = false;
    std::string str;
    if (resp.array.size() > 1)
    {
        if (resp.array[1].string != "replication")
        {
            return bulk_string("Filtering not supported yet :)\n");
        }
    }

    str += "# Replication\n";

    if (is_slave())
    {
        str += "role:slave\n";
    }
    else
    {
        str += "role:master\n";
    }
    str += "master_replid:" + master_replid + "\n";
    str += "master_repl_offset:" + std::to_string(master_repl_offset()) + "\n";

    return bulk_string(str);
}

std::string replconf_getack(const RESP_data& resp, Rel_data& data)
{
    return command({"REPLCONF", "ACK", std::to_string(master_repl_offset())});
}

std::string replconf_ack(const RESP_data& resp, Rel_data& data)
{
    replica_acked();
    return OK_simple;
}

const std::unordered_map<std::string, Cmd> replconf_cmd_map = {
    {"GETACK", replconf_getack},
    {"ACK", replconf_ack}
};

std::string replconf(const RESP_data& resp, Rel_data& data)
{
    data.repeat = false;
    data.respond = true;
    std::string cmd = resp.array[1].string;
    to_upper(cmd);
    if (!replconf_cmd_map.contains(cmd))
    {
        return OK_simple;
    }
    return replconf_cmd_map.at(cmd)(resp, data);
}

std::string psync(const RESP_data& resp, Rel_data& data)
{
    data.repeat = false;
    if (resp.array.size() < 3)
    {
        return bad_cmd;
    }

    if (resp.array[1].string == "?" && resp.array[2].string == "-1")
    {
        data.send_rdb = true;
        data.client_is_replica = true;
        slave_count()++;
        return simple_string("FULLRESYNC " + master_replid + " " + std::to_string(master_repl_offset()));
    }

    // temp
    return bulk_string("not supported yet");
}

std::string wait(const RESP_data& resp, Rel_data& data)
{
    data.repeat = false;
    if (resp.array.size() < 3)
    {
        return bad_cmd;
    }

    if (!send_getack())
    {
        return integer(slave_count());
    }
    // will probably not work if multiple clients send WAITs
    send_getack() = false;
    const long numreplicas = std::stol(resp.array[1].string);
    if (numreplicas <= 0)
    {
        return integer(0);
    }
    const unsigned int timeout = std::stol(resp.array[2].string);

    add_command(command({"REPLCONF", "GETACK", "*"}), true, timeout);
    reset_acks();

    const auto millis = std::chrono::milliseconds(timeout);
    const auto start = std::chrono::system_clock::now();
    while (std::chrono::system_clock::now() - start < millis)
    {
        if (n_acks() >= numreplicas)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return integer(n_acks());
}

const std::unordered_map<std::string, Cmd> cmd_map = {
    {"PING", ping},
    {"ECHO", echo},
    {"SET", set},
    {"GET", get},
    {"CONFIG", config},
    {"KEYS", keys},
    {"INFO", info},
    {"REPLCONF", replconf},
    {"PSYNC", psync},
    {"WAIT", wait}
};

std::string process_command(const RESP_data& resp, Rel_data& data)
{
    std::string cmd = resp.array[0].string;
    to_upper(cmd);
    if (!cmd_map.contains(cmd))
    {
        // temp value
        return bad_cmd;
    }
    return cmd_map.at(cmd)(resp, data);
}
