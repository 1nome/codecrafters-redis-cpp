#include "Database.h"

#include <unordered_map>
#include <string>

std::unordered_map<std::string, std::pair<std::string, Timestamp>> key_vals;
std::unordered_map<std::string, std::string> config_key_vals;
