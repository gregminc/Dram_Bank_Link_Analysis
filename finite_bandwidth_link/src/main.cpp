#include "request_builder.hpp"
#include "simulation_config.hpp"
#include "simulator.hpp"
#include "statistics.hpp"
#include "trace_reader.hpp"

#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

struct CommandLineOptions {
    std::string trace_path;
    std::optional<std::string> summary_output_path;
    std::optional<std::string> link_output_path;
    std::optional<std::string> bank_output_path;
    SimulationConfig config = kDefaultSimulationConfig;
};

void PrintUsage(std::ostream& output, const char* program) {
    output
        << "Usage: " << program << " --trace PATH [options]\n\n"
        << "Link options:\n"
        << "  --banks-per-link N       Banks sharing each link (default 2)\n"
        << "  --link-width-bits N      Link width in bits (default 16)\n"
        << "  --transfers-per-ns X     Link transfer rate (default 4.266)\n"
        << "  --fifo-depth N           Waiting entries per link (default 4)\n"
        << "  --transfer-bits N        Bits per request (default 256)\n\n"
        << "Address mapping options:\n"
        << "  --bank-groups N          Bank groups per rank (default 2)\n"
        << "  --banks-per-group N      Banks per group (default 4)\n"
        << "  --channels N             Channels (default 1)\n"
        << "  --ranks N                Ranks (default 1)\n"
        << "  --rows N                 Rows (default 65536)\n"
        << "  --columns N              Columns (default 1024)\n"
        << "  --bus-width-bytes N      Address-mapping bus width (default 8)\n"
        << "  --burst-length N         Address-mapping burst length (default 16)\n\n"
        << "Output options:\n"
        << "  --summary-output PATH    Write summary CSV instead of stdout\n"
        << "  --link-output PATH       Write per-link CSV\n"
        << "  --bank-output PATH       Write per-bank CSV\n"
        << "  --help                   Show this message\n";
}

std::string RequireValue(int argc, char** argv, int& index) {
    if (index + 1 >= argc) {
        throw std::invalid_argument(
            std::string("Missing value for option ") + argv[index]
        );
    }
    return argv[++index];
}

unsigned ParseUnsigned(const std::string& text, const std::string& option) {
    std::size_t consumed = 0;
    const unsigned long value = std::stoul(text, &consumed);
    if (consumed != text.size() || value == 0 ||
        value > std::numeric_limits<unsigned>::max()) {
        throw std::invalid_argument(option + " requires a positive integer");
    }
    return static_cast<unsigned>(value);
}

std::size_t ParseSize(const std::string& text, const std::string& option) {
    return static_cast<std::size_t>(ParseUnsigned(text, option));
}

double ParsePositiveDouble(
    const std::string& text,
    const std::string& option
) {
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value) || value <= 0.0) {
        throw std::invalid_argument(option + " requires a positive number");
    }
    return value;
}

CommandLineOptions ParseCommandLine(int argc, char** argv) {
    CommandLineOptions options;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help") {
            PrintUsage(std::cout, argv[0]);
            std::exit(0);
        } else if (argument == "--trace") {
            options.trace_path = RequireValue(argc, argv, index);
        } else if (argument == "--summary-output") {
            options.summary_output_path = RequireValue(argc, argv, index);
        } else if (argument == "--link-output") {
            options.link_output_path = RequireValue(argc, argv, index);
        } else if (argument == "--bank-output") {
            options.bank_output_path = RequireValue(argc, argv, index);
        } else if (argument == "--banks-per-link") {
            options.config.link.banks_per_link = ParseUnsigned(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--link-width-bits") {
            options.config.link.width_bits = ParseUnsigned(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--transfers-per-ns") {
            options.config.link.transfers_per_ns = ParsePositiveDouble(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--fifo-depth") {
            options.config.link.fifo_depth = ParseSize(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--transfer-bits") {
            options.config.transfer_bits = ParseUnsigned(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--bank-groups") {
            options.config.address_mapping.bank_groups = ParseUnsigned(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--banks-per-group") {
            options.config.address_mapping.banks_per_group = ParseUnsigned(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--channels") {
            options.config.address_mapping.channels = ParseUnsigned(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--ranks") {
            options.config.address_mapping.ranks = ParseUnsigned(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--rows") {
            options.config.address_mapping.rows = ParseUnsigned(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--columns") {
            options.config.address_mapping.columns = ParseUnsigned(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--bus-width-bytes") {
            options.config.address_mapping.bus_width_bytes = ParseUnsigned(
                RequireValue(argc, argv, index), argument);
        } else if (argument == "--burst-length") {
            options.config.address_mapping.burst_length = ParseUnsigned(
                RequireValue(argc, argv, index), argument);
        } else {
            throw std::invalid_argument("Unknown option: " + argument);
        }
    }

    if (options.trace_path.empty()) {
        throw std::invalid_argument("--trace is required");
    }
    return options;
}

template <typename Writer>
void WriteOptionalFile(
    const std::optional<std::string>& path,
    Writer writer
) {
    if (!path) {
        return;
    }
    std::ofstream output(*path);
    if (!output) {
        throw std::runtime_error("Unable to open output file: " + *path);
    }
    writer(output);
}

}

int main(int argc, char** argv) {
    try {
        const CommandLineOptions options = ParseCommandLine(argc, argv);
        TraceReader trace_reader(options.trace_path);
        if (!trace_reader.IsOpen()) {
            throw std::runtime_error(
                "Unable to open trace file: " + options.trace_path
            );
        }

        RequestBuilder request_builder(options.config);
        Simulator simulator(options.config);

        bool overflowed = false;
        try {
            simulator.Run(trace_reader, request_builder);
        } catch (const QueueOverflowError& error) {
            overflowed = true;
            std::cerr << error.what() << '\n';
        }

        const SimulationStatistics& statistics = simulator.Statistics();
        if (options.summary_output_path) {
            WriteOptionalFile(
                options.summary_output_path,
                [&](std::ostream& output) {
                    WriteSummaryCsv(
                        statistics,
                        options.config,
                        options.trace_path,
                        output
                    );
                }
            );
        } else {
            WriteSummaryCsv(
                statistics,
                options.config,
                options.trace_path,
                std::cout
            );
        }

        WriteOptionalFile(
            options.link_output_path,
            [&](std::ostream& output) {
                WriteLinkStatisticsCsv(statistics, output);
            }
        );
        WriteOptionalFile(
            options.bank_output_path,
            [&](std::ostream& output) {
                WriteBankStatisticsCsv(statistics, output);
            }
        );

        return overflowed ? 2 : 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
