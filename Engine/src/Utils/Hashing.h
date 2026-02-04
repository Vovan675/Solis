#pragma once
#include "cstdint"

// https://stackoverflow.com/questions/28675727/using-crc32-algorithm-to-hash-string-at-compile-time
namespace crc
{
    // Generate CRC lookup table
    template <unsigned c, int k = 8>
    struct f : f<((c & 1) ? 0xedb88320 : 0) ^ (c >> 1), k - 1> {};
    template <unsigned c> struct f<c, 0>{enum {value = c};};

    #define A(x) B(x) B(x + 128)
    #define B(x) C(x) C(x +  64)
    #define C(x) D(x) D(x +  32)
    #define D(x) E(x) E(x +  16)
    #define E(x) F(x) F(x +   8)
    #define F(x) G(x) G(x +   4)
    #define G(x) H(x) H(x +   2)
    #define H(x) I(x) I(x +   1)
    #define I(x) f<x>::value ,

    constexpr unsigned crc_table[] = { A(0) };

    constexpr uint32_t crc32_impl(const uint8_t *p, size_t len, uint32_t crc)
    {
        return len ?
            crc32_impl(p+1,len-1,(crc>>8)^crc_table[(crc&0xFF)^*p])
            : crc;
    }

    constexpr uint32_t crc32_impl(const uint8_t *data, size_t length)
    {
        uint32_t result = 0xFFFFFFFFu;

        for (size_t byte = 0; byte < length; byte++)
        {
            result = (result >> 8) ^ crc_table[(result ^ data[byte]) & 0xFF];
        }
        return ~result;
    }

    static uint32_t crc32(const uint8_t *data, size_t length)
    {
        return crc32_impl(data, length);
    }

    template <uint32_t len>
    static constexpr uint32_t crc32(const char (&data)[len])
    {
        return crc32_impl((uint8_t *)data, len);
    }
}