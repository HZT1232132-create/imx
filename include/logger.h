#pragma once

#include <string>
#include <fstream>
#include "process_result.h"

class EventLogger {
public:
    explicit EventLogger(const std::string& logPath);
    ~EventLogger();

    void write(const ProcessResult& result);
    bool isOpen() const;

private:
    std::ofstream m_file;
    void writeHeader();
};
