

#include "request_builder.hpp"
#include "address_mapper.hpp"

#include <cstdint>

RequestBuilder::RequestBuilder(const SimulationConfig& config)
    : address_mapper_(config.address_mapping),
      ranks_(config.address_mapping.ranks),
      bank_groups_(config.address_mapping.bank_groups),
      banks_per_group_(config.address_mapping.banks_per_group),
      transfer_bits_(config.transfer_bits) {}

LinkRequest RequestBuilder::Build(const TraceRecord& record) {
    const DramAddress dram_address = address_mapper_.Decode(record.address);
    const unsigned absolute_bank =
        (((dram_address.channel * ranks_) + dram_address.rank) *
            bank_groups_ + dram_address.bank_group) *
        banks_per_group_ + dram_address.bank;
    return LinkRequest {
        .id = next_request_id_++,
        .ready_time_ns = record.ready_time_ns,
        .bank = absolute_bank,
        .operation = record.operation,
        .transfer_bits = transfer_bits_
    };
} 
