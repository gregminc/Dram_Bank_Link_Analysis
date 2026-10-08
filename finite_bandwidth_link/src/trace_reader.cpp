

#include "trace_reader.hpp"

#include <stdexcept>
#include <sstream>

TraceReader::TraceReader(const std::string& filename) : input_(filename) {}

bool TraceReader::IsOpen() const {
    return input_.is_open();
}

bool TraceReader::Next(TraceRecord& record) {
    std::string line;
    while (getline(input_, line)) {
        line_number_++;

        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream is(line);

        std::string operation;
        std::string address;

        if (!(is >> record.ready_time_ns >> operation >> address)) {
            throw std::runtime_error("Malformed trace line: " + std::to_string(line_number_));
        }

        if (operation == "Read") {
            record.operation = Operation::Read;
        } else if (operation == "Write") {
            record.operation = Operation::Write;
        } else {
            throw std::runtime_error(
                "Unknown operation on trace line " +
                std::to_string(line_number_) + ": " + operation
            );
        }

        record.address = std::stoull(address, nullptr, 0); 
        return true;
    }

    return false;
}
