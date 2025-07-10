#ifndef RESP_H
#define RESP_H

#include <string>
#include <vector>
#include <sstream>

constexpr std::string CRLF = "\r\n";
constexpr std::string OK_simple = "+OK\r\n";
constexpr std::string null_bulk_string = "$-1\r\n";
constexpr std::string null_array = "*-1\r\n";
constexpr std::string null = "_\r\n";

std::string simple_string(const std::string& content);
std::string simple_error(const std::string& error);
std::string integer(long long int val);
std::string bulk_string(const std::string& content);
std::string array(const std::vector<std::string>& elements);
std::string boolean(bool val);

enum RESP_type
{
    Simple_string = '+',
    Simple_error = '-',
    Integer = ':',
    Bulk_string = '$',
    Array = '*',
    Null = '_',
    Boolean = '#',
    Double = ',',
    Big_number = '(',
    Bulk_error = '!',
    Verbatim_string = '=',
    Map = '%',
    Attribute = '|',
    Set = '~',
    Push = '>'
};

struct RESP_data
{
    RESP_type type;
    union
    {
        long long int integer{};
        double real;
        bool boolean;
        std::string string;
        std::vector<RESP_data> array;
    };

    RESP_data();
    RESP_data(const RESP_data& data);
    RESP_data(RESP_data&& data) noexcept;

    ~RESP_data();

    RESP_data& operator=(const RESP_data& other);
};

RESP_data parse(std::stringstream& input);

#endif //RESP_H
