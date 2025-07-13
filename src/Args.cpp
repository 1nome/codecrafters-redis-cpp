#include "Args.h"
#include <unordered_map>
#include <string>
#include "Database.h"

typedef bool (*Arg)(int, char**, int&);

bool dir(const int argc, char** argv, int& i)
{
    i++;
    if (i >= argc)
    {
        return false;
    }
    config_key_vals["dir"] = std::string(argv[i]);
    return true;
}

bool dbfilename(const int argc, char** argv, int& i)
{
    i++;
    if (i >= argc)
    {
        return false;
    }
    config_key_vals["dbfilename"] = std::string(argv[i]);
    return true;
}

bool port(const int argc, char** argv, int& i)
{
    i++;
    if (i >= argc)
    {
        return false;
    }
    config_key_vals["port"] = std::string(argv[i]);
    return true;
}

const std::unordered_map<std::string, Arg> arg_map = {
    {"--dir", dir},
    {"--dbfilename", dbfilename},
    {"--port", port}
};

bool process_args(const int argc, char** argv)
{
    bool ret = true;
    for (int i = 1; i < argc; i++)
    {
        if (std::string arg = argv[i]; arg_map.contains(arg))
        {
            ret &= arg_map.at(arg)(argc, argv, i);
        }
    }
    return ret;
}
