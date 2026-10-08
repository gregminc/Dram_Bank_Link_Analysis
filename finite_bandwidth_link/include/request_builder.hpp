

#pragma once

#include "address_mapper.hpp"
#include "simulation_config.hpp"
#include "simulation_types.hpp"
#include "trace_reader.hpp"

class RequestBuilder {
public:
    explicit RequestBuilder(const SimulationConfig& config);

    LinkRequest Build(const TraceRecord& record);
private:
    AddressMapper address_mapper_;
    unsigned ranks_;
    unsigned bank_groups_;
    unsigned banks_per_group_;
    unsigned transfer_bits_;
    std::uint64_t next_request_id_ = 0;
};
