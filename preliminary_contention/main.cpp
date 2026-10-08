

#include "main.hpp"
#include "trace_reader.hpp"
#include "address_mapper.hpp"
#include "statistics.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct RuntimeOptions {
    std::string trace_filename;
    double window_size_ns = 0.0;
    unsigned banks_per_link_group = 0;
    std::filesystem::path output_directory;
};

void PrintUsage(const char* program_name) {
    std::cerr
        << "Usage: " << program_name << " <trace_file>"
        << " --window-ns <positive_number>"
        << " --banks-per-link-group <positive_integer>"
        << " --output-dir <directory>\n";
}

RuntimeOptions ParseArguments(int argc, char** argv) {
    if (argc < 2) {
        throw std::invalid_argument("Missing trace filename");
    }

    RuntimeOptions options;
    options.trace_filename = argv[1];

    for (int index = 2; index < argc; ++index) {
        const std::string argument = argv[index];
        if (index + 1 >= argc) {
            throw std::invalid_argument("Missing value for " + argument);
        }

        const std::string value = argv[++index];
        if (argument == "--window-ns") {
            options.window_size_ns = std::stod(value);
        } else if (argument == "--banks-per-link-group") {
            options.banks_per_link_group =
                static_cast<unsigned>(std::stoul(value));
        } else if (argument == "--output-dir") {
            options.output_directory = value;
        } else {
            throw std::invalid_argument("Unknown argument: " + argument);
        }
    }

    if (options.window_size_ns <= 0.0 ||
        options.banks_per_link_group == 0 ||
        options.output_directory.empty()) {
        throw std::invalid_argument("All runtime options are required");
    }

    constexpr unsigned banks_per_rank = BANK_GROUPS * BANKS_PER_GROUP;
    if (options.banks_per_link_group > banks_per_rank ||
        banks_per_rank % options.banks_per_link_group != 0) {
        throw std::invalid_argument(
            "banks-per-link-group must divide the eight banks in each rank");
    }

    return options;
}

}

int main(int argc, char** argv) {
    try {
        const RuntimeOptions options = ParseArguments(argc, argv);
        TraceReader reader(options.trace_filename);
        if (!reader.IsOpen()) {
            throw std::runtime_error(
                "Could not open trace file: " + options.trace_filename);
        }

        AddressMappingConfig mapping_config = {
            .channels = CHANNELS,
            .ranks = RANKS,
            .bank_groups = BANK_GROUPS,
            .banks_per_group = BANKS_PER_GROUP,
            .rows = ROWS,
            .columns = COLUMNS,
            .bus_width_bytes = BUS_WIDTH_BYTES,
            .burst_length = BURST_LENGTH
        };

        AnalysisConfig analysis_config = {
            .channels = CHANNELS,
            .ranks_per_channel = RANKS,
            .bank_groups_per_rank = BANK_GROUPS,
            .banks_per_group = BANKS_PER_GROUP,
            .banks_per_link_group = options.banks_per_link_group,
            .window_size_ns = options.window_size_ns
        };

        AddressMapper mapper(mapping_config);
        Statistics statistics(analysis_config);

        TraceRecord record;
        while (reader.Next(record)) {
            const DramAddress dram_address = mapper.Decode(record.address);
            statistics.Observe(record, dram_address);
        }

        std::filesystem::create_directories(options.output_directory);
        statistics.WriteBankStatistics(
            (options.output_directory / "bank_statistics.csv").string());
        statistics.WriteActiveBankHistogram(
            (options.output_directory / "active_bank_histogram.csv").string());
        statistics.WriteGroupStatistics(
            (options.output_directory / "group_statistics.csv").string());

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        PrintUsage(argv[0]);
        return 1;
    }
}
