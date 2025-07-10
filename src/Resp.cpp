#include "Resp.h"

#include <memory>
#include <utility>
#include <vector>

std::string simple_string(const std::string& content)
{
    return '+' + content + CRLF;
}

std::string simple_error(const std::string& error)
{
    return '-' + error + CRLF;
}

std::string integer(const long long int val)
{
    return ':' + std::to_string(val) + CRLF;
}

std::string bulk_string(const std::string& content)
{
    return '$' + std::to_string(content.length()) + CRLF + content + CRLF;
}

std::string array(const std::vector<std::string>& elements)
{
    std::string res = "*" + std::to_string(elements.size()) + CRLF;
    for (const auto& element : elements)
    {
        res += element + CRLF;
    }
    return res;
}

std::string boolean(const bool val)
{
    return std::to_string('#') + (val ? 't' : 'f') + CRLF;
}

void copy_data(const RESP_data& src, RESP_data& dest)
{
    switch (src.type)
    {
    case Null:
    case Integer:
        dest.integer = src.integer;
        break;
    case Double:
        dest.real = src.real;
        break;
    case Boolean:
        dest.boolean = src.boolean;
        break;
    case Simple_string:
    case Simple_error:
    case Bulk_string:
    case Bulk_error:
    case Big_number:
    case Verbatim_string:
        dest.string = src.string;
        break;
    case Array:
    case Map:
    case Attribute:
    case Set:
    case Push:
        dest.array = src.array;
        break;
    }
}

RESP_data::RESP_data(): type(Null)
{
}

RESP_data::RESP_data(const RESP_data& data): type(data.type)
{
    copy_data(data, *this);
}

RESP_data::RESP_data(RESP_data&& data) noexcept : type(std::exchange(data.type, Null))
{
    switch (type)
    {
    case Null:
    case Integer:
        integer = std::exchange(data.integer, 0);
        break;
    case Double:
        real = std::exchange(data.real, 0.0);
        break;
    case Boolean:
        boolean = std::exchange(data.boolean, false);
        break;
    case Simple_string:
    case Simple_error:
    case Bulk_string:
    case Bulk_error:
    case Big_number:
    case Verbatim_string:
        new (&string) std::string;  // kinda defeats the point of a move constructor, but it is required
        string = std::move(data.string);
        break;
    case Array:
    case Map:
    case Attribute:
    case Set:
    case Push:
        new (&array) std::vector<RESP_data>;
        array = std::move(data.array);
        break;
    }
}

RESP_data::~RESP_data()
{
    switch (type)
    {
    case Simple_string:
    case Simple_error:
    case Bulk_string:
    case Bulk_error:
    case Big_number:
    case Verbatim_string:
        string.~basic_string();
        break;
    case Array:
    case Map:
    case Attribute:
    case Set:
    case Push:
        array.~vector();
        break;
    default:
        break;
    }
}

RESP_data& RESP_data::operator=(const RESP_data& other)
{
    if (this == &other)
    {
        return *this;
    }
    type = other.type;
    copy_data(other, *this);
    return *this;
}

RESP_data parse(std::stringstream& input)
{
    auto next_line = [&input]
    {
        input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    };

    RESP_data data;
    data.type = static_cast<RESP_type>(input.get());

    size_t n;
    switch (data.type)
    {
    case Array:
        input >> n;
        new (&data.array) std::vector<RESP_data>;
        data.array.reserve(n);

        next_line();
        for (size_t i = 0; i < n; i++)
        {
            data.array.push_back(std::move(parse(input)));
        }
        break;
    case Bulk_string:
        input >> n;
        new (&data.string) std::string;
        data.string.reserve(n);

        next_line();
        std::getline(input, data.string, '\r');
        break;
    default:
        break;
    }
    next_line();
    return data;
}
