#pragma once
#include <vector>
#include <glm/glm.hpp>

class Stream
{
public:
    virtual ~Stream() = default;

    virtual void readBytes(char *buffer, std::size_t size) = 0;
    virtual void writeBytes(const char *buffer, std::size_t size) = 0;

    template<typename T>
    inline void read(T &value)
    {
        readBytes(reinterpret_cast<char *>(&value), sizeof(T));
    }

    template<typename T>
    inline void write(const T &value)
    {
        writeBytes(reinterpret_cast<const char *>(&value), sizeof(T));
    }

    template<typename T>
    inline void read(std::vector<T> &vec, bool bulk_memory = false)
    {
        std::size_t size;
        read(size);

        vec.resize(size);

        if (size <= 0)
            return;

        if (bulk_memory)
        {
            readBytes(reinterpret_cast<char *>(vec.data()), size * sizeof(T));
        } else
        {
            for (std::size_t i = 0; i < size; ++i)
                read(vec[i]);
        }
    }

    template<typename T>
    void write(const std::vector<T> &vec, bool bulk_memory = false)
    {
        std::size_t size = vec.size();
        write(size);

        if (bulk_memory)
        {
            writeBytes(reinterpret_cast<const char *>(vec.data()), size * sizeof(T));
        } else
        {
            for (const auto &element : vec)
                write(element);
        }
    }

    void read(std::string &str)
    {
        std::size_t size;
        read(size);

        if (size == 0)
        {
            str.clear();
            return;
        }

        str.resize(size);
        readBytes(&str[0], size);
    }

    void write(const std::string &str)
    {
        write(str.size());
        if (str.size() > 0)
        {
            writeBytes(str.data(), str.size());
        }
    }

    template<typename T>
    inline void read(glm::vec2 &vec)
    {
        read(vec.x);
        read(vec.y);
    }

    template<typename T>
    inline void write(glm::vec2 &vec)
    {
        write(vec.x);
        write(vec.y);
    }

    template<typename T>
    inline void read(glm::vec3 &vec)
    {
        read(vec.x);
        read(vec.y);
        read(vec.z);
    }

    template<typename T>
    inline void write(glm::vec3 &vec)
    {
        write(vec.x);
        write(vec.y);
        write(vec.z);
    }
};