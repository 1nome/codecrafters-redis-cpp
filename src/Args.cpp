#include "Args.h"
#include <string>
#include <unordered_set>

#include "Database.h"

typedef bool (*Arg)(int, char**, int&);

const std::unordered_set<std::string> simple_args = {
    "--dir",
    "--dbfilename",
    "--port",
    "--replicaof"
};

bool process_args(const int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (std::string arg = argv[i]; simple_args.contains(arg))
        {
            i++;
            if (i >= argc)
            {
                return false;
            }
            config_key_vals[arg.substr(2)] = std::string(argv[i]);
        }
    }
    return true;
}
