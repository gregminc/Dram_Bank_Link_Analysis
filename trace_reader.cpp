

#include "trace_reader.hpp"

#include <sstream>
#include <stdexcept>

TraceReader::TraceReader(const std::string& filename)
    : input_(filename) {}

bool TraceReader::IsOpen() const {
    return input_.is_open();
}

bool TraceReader::Next(TraceRecord& record) {
    std::string line;
    while (std::getline(input_, line)) {
        ++line_number_;

        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream stream(line);

        std::string operation;
        std::string address;

        if (!(stream >> record.time_stamp_ns >> operation >> address)) {
            throw std::runtime_error(
                "Malformed trace line: " + std::to_string(line_number_)
            );
        }

        record.operation =
            operation == "READ" ? Operation::Read : Operation::Write;

        record.address = std::stoull(address, nullptr, 0);
        return true;
    }

    return false;
}