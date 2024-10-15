#pragma once
#include "Log.h"
#include "Stream.h"
#include <fstream>
#include <string>

class FileStream : public Stream {
public:
    FileStream(const std::string& filename, std::ios_base::openmode mode) {
        file.open(filename, mode | std::ios_base::binary);

        if (!file.is_open()) {
            CORE_ERROR("Unable to open file.");
        }
    }

    ~FileStream() override {
        if (file.is_open()) {
            file.close();
        }
    }

    void readBytes(char* buffer, std::size_t size) override {
        if (!file.read(buffer, size)) {
            CORE_ERROR("Failed to read from file.");
        }
    }

    void writeBytes(const char* buffer, std::size_t size) override {
        if (!file.write(buffer, size)) {
            CORE_ERROR("Failed to write to file.");
        }
    }
private:
    std::fstream file;
};