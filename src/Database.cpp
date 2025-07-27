#include "Database.h"

#include <unordered_map>
#include <string>
#include <fstream>
#include <iostream>
#include <mutex>
#include <utility>
#include "Resp.h"

std::unordered_map<std::string, std::string> key_vals_map;
std::unordered_map<std::string, Timestamp> key_expiry_map;
std::unordered_map<std::string, std::string> config_key_vals_map = {
    {"port", "6379"}
};

std::mutex key_vals_lock;
std::mutex key_expiry_lock;
std::mutex config_key_vals_lock;

std::unordered_map<std::string, std::vector<Stream_entry>> streams;

std::mutex streams_lock;

std::unordered_map<std::string, std::list<std::string>> lists;
std::mutex lists_lock;

enum Special_type
{
    None,
    Byte,
    TwoByte,
    FourByte,
    Compressed
};

Special_type read_length(std::basic_istream<char>& file, unsigned int& val)
{
    char byte;
    file.read(&byte, 1);
    switch (byte & 0xC0)
    {
    case 0x00:
        val = byte & 0x3F;
        return None;
    case 0x80:
        unsigned int ret;
        file.read(reinterpret_cast<std::istream::char_type*>(&ret), 4);
        val = __builtin_bswap32(ret);
        return None;
    case 0x40:
        unsigned char byte2;
        file.read(reinterpret_cast<std::istream::char_type*>(&byte2), 1);
        val = byte2 + ((byte & 0x3F) << 8);
        return None;
    case 0xC0:
        switch (byte & 0x3F)
        {
        case 0:
            val = 1;
            return Byte;
        case 1:
            val = 2;
            return TwoByte;
        case 2:
            val = 4;
            return FourByte;
        case 3:
            read_length(file, val);
            return Compressed;
        default:
            return None;
        }
    default:
        return None;
    }
}

std::string read_string(std::basic_istream<char>& file)
{
    std::string str;
    switch (unsigned int len; read_length(file, len))
    {
    case None:
        str = std::string(len, '\0');
        file.read(&str[0], len);
        return str;
    case Byte:
    case TwoByte:
    case FourByte:
        unsigned int fourByte;
        file.read(reinterpret_cast<std::istream::char_type*>(&fourByte), len);
        return std::to_string(fourByte);
    case Compressed:
        unsigned int compressedLen;
        read_length(file, compressedLen);
        unsigned int uncompressedLen;
        read_length(file, uncompressedLen);
        // we don't deal with compressed strings yet
        file.ignore(compressedLen);
        return str;
    }
    return str;
}

std::string read_key_val(std::basic_istream<char>& file, const unsigned char byte)
{
    std::string key = read_string(file);
    switch (byte)
    {
    case 0:
        key_vals()[key] = read_string(file);
        break;
    default:
        std::cerr << "Value type not supported" << std::endl;
        break;
    }
    return key;
}

std::unordered_map<std::string, std::string>& key_vals()
{
    const std::lock_guard lock(key_vals_lock);
    return key_vals_map;
}

std::unordered_map<std::string, Timestamp>& key_expiry()
{
    const std::lock_guard lock(key_expiry_lock);
    return key_expiry_map;
}

std::unordered_map<std::string, std::string>& config_key_vals()
{
    const std::lock_guard lock(config_key_vals_lock);
    return config_key_vals_map;
}

std::string Stream_entry::id_bulk() const
{
    return bulk_string(std::to_string(milliseconds_time) + "-" + std::to_string(sequence_number));
}

void stream_add(const std::string& stream_key, const Stream_entry& se)
{
    const std::lock_guard lock(streams_lock);
    streams[stream_key].push_back(se);
}

bool stream_exists(const std::string& stream_key)
{
    const std::lock_guard lock(streams_lock);
    return streams.contains(stream_key);
}

const Stream_entry& stream_top(const std::string& stream_key)
{
    const std::lock_guard lock(streams_lock);
    return streams[stream_key].back();
}

bool stream_empty(const std::string& stream_key)
{
    const std::lock_guard lock(streams_lock);
    return streams[stream_key].empty();
}

void read_rdb(std::basic_istream<char>* s)
{
    std::ifstream* file = nullptr;
    if (s == nullptr)
    {
        if (!config_key_vals().contains("dir") || !config_key_vals().contains("dbfilename"))
        {
            std::cout << "Database file not loaded\n";
            return;
        }
        file = new std::ifstream(config_key_vals()["dir"] + "/" + config_key_vals()["dbfilename"], std::ios::binary);

        if (not file->is_open()) {
            std::cerr << "Unable to open rdb file\n";
            delete file;
            return;
        }
        s = file;
    }

    char read_buffer[6];

    s->read(read_buffer, 5);
    read_buffer[5] = '\0';
    if (std::string(read_buffer) != "REDIS")
    {
        std::cerr << "Supplied file is not an RDB file\n";
        if (file != nullptr)
        {
            file->close();
            delete file;
        }
        return;
    }

    s->read(read_buffer, 4);
    read_buffer[4] = '\0';
    const int version = std::stoi(read_buffer);

    std::string aux_key;
    std::string aux_val;
    while (not s->eof())
    {
        unsigned char byte;
        s->read(reinterpret_cast<std::istream::char_type*>(&byte), 1);

        switch (byte)
        {
        case 0xFF:
            unsigned long long crc64;
            s->read(reinterpret_cast<std::istream::char_type*>(&crc64), 8);
            // won't bother with checking yet
            if (file != nullptr)
            {
                file->close();
                delete file;
            }
            return;
        case 0xFE:
            unsigned int db_sel;
            read_length(*s, db_sel);
            // we currently ignore this as there is only one database present
            break;
        case 0xFD:
            unsigned int expire_sec;
            s->read(reinterpret_cast<std::istream::char_type*>(&expire_sec), 4);
            s->read(reinterpret_cast<std::istream::char_type*>(&byte), 1);
            key_expiry()[read_key_val(*s, byte)] = Timestamp(std::chrono::seconds(expire_sec));
            break;
        case 0xFC:
            unsigned long long expire_msec;
            s->read(reinterpret_cast<std::istream::char_type*>(&expire_msec), 8);
            s->read(reinterpret_cast<std::istream::char_type*>(&byte), 1);
            key_expiry()[read_key_val(*s, byte)] = Timestamp(std::chrono::milliseconds(expire_msec));
            break;
        case 0xFB:
            unsigned int key_val_size;
            read_length(*s, key_val_size);
            unsigned int expiry_size;
            read_length(*s, expiry_size);
            // could use these to reserve space in the db; not used yet
            break;
        case 0xFA:
            aux_key = read_string(*s);
            aux_val = read_string(*s);
            // currently do nothing with this
            break;
        default:
            read_key_val(*s, byte);
            break;
        }
    }

    std::cerr << "Supplied file is broken\n";
    if (file != nullptr)
    {
        file->close();
        delete file;
    }
}
