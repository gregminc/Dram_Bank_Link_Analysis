

#pragma once

#include "simulation_types.hpp"
#include "request_fifo.hpp"

#include <cstddef>
#include <optional>

struct ActiveTransfer {
    LinkRequest request;
    unsigned link_id;
    double start_time_ns;
    double finish_time_ns;
};

class Link {
public: 
    Link(
        unsigned id, 
        unsigned width_bits,
        double transfers_per_ns,
        std::size_t fifo_depth
    );

    bool Accept(const LinkRequest& request);
    ActiveTransfer CompleteTransfer();

    [[nodiscard]] bool IsBusy() const;
    [[nodiscard]] bool HasWaitingRequest() const;
    [[nodiscard]] std::size_t WaitingRequestCount() const noexcept;
    [[nodiscard]] double TransferTimeNs(unsigned bits) const;

    [[nodiscard]]
    std::optional<double> CompletionTimeNs() const noexcept;

private:
    unsigned id_;
    unsigned width_bits_;
    double transfers_per_ns_;

    ActiveTransfer CreateActiveTransfer(
        const LinkRequest&,
        double start_time_ns
    );

    RequestFifo fifo_;
    std::optional<ActiveTransfer> active_request_;
};
