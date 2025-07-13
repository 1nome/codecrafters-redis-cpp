#include "Database.h"

#include <unordered_map>
#include <string>
#include <fstream>
#include <iostream>

std::unordered_map<std::string, std::string> key_vals;
std::unordered_map<std::string, Timestamp> key_expiry;
std::unordered_map<std::string, std::string> config_key_vals;

enum Special_type
{
    None,
    Byte,
    TwoByte,
    FourByte,
    Compressed
};

Special_type read_length(std::ifstream& file, unsigned int& val)
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

std::string read_string(std::ifstream& file)
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

std::string read_key_val(std::ifstream& file, unsigned char byte)
{
    std::string key = read_string(file);
    switch (byte)
    {
    case 0:
        key_vals[key] = read_string(file);
        break;
    default:
        std::cerr << "Value type not supported" << std::endl;
        break;
    }
    return key;
}

void read_rdb()
{
    if (!config_key_vals.contains("dir") || !config_key_vals.contains("dbfilename"))
    {
        std::cout << "Database file not loaded\n";
        return;
    }
    std::ifstream file(config_key_vals["dir"] + "/" + config_key_vals["dbfilename"], std::ios::binary);

    if (not file.is_open()) {
        std::cerr << "Unable to open rdb file\n";
        return;
    }

    char read_buffer[6];

    file.read(read_buffer, 5);
    read_buffer[5] = '\0';
    if (std::string(read_buffer) != "REDIS")
    {
        std::cerr << "Supplied file is not an RDB file\n";
        file.close();
        return;
    }

    file.read(read_buffer, 4);
    read_buffer[4] = '\0';
    const int version = std::stoi(read_buffer);

    std::string aux_key;
    std::string aux_val;
    while (not file.eof())
    {
        unsigned char byte;
        file.read(reinterpret_cast<std::istream::char_type*>(&byte), 1);

        switch (byte)
        {
        case 0xFF:
            unsigned long long crc64;
            file.read(reinterpret_cast<std::istream::char_type*>(&crc64), 8);
            // won't bother with checking yet
            file.close();
            return;
        case 0xFE:
            unsigned int db_sel;
            read_length(file, db_sel);
            // we currently ignore this as there is only one database present
            break;
        case 0xFD:
            unsigned int expire_sec;
            file.read(reinterpret_cast<std::istream::char_type*>(&expire_sec), 4);
            file.read(reinterpret_cast<std::istream::char_type*>(&byte), 1);
            key_expiry[read_key_val(file, byte)] = Timestamp(std::chrono::seconds(expire_sec));
            break;
        case 0xFC:
            unsigned long long expire_msec;
            file.read(reinterpret_cast<std::istream::char_type*>(&expire_msec), 8);
            file.read(reinterpret_cast<std::istream::char_type*>(&byte), 1);
            key_expiry[read_key_val(file, byte)] = Timestamp(std::chrono::milliseconds(expire_msec));
            break;
        case 0xFB:
            unsigned int key_val_size;
            read_length(file, key_val_size);
            unsigned int expiry_size;
            read_length(file, expiry_size);
            // could use these to reserve space in the db; not used yet
            break;
        case 0xFA:
            aux_key = read_string(file);
            aux_val = read_string(file);
            // currently do nothing with this
            break;
        default:
            read_key_val(file, byte);
            break;
        }
    }

    std::cerr << "Supplied file is broken\n";
    file.close();
}
