

#include "link.hpp"

#include <stdexcept>

Link::Link(
    unsigned id, 
    unsigned width_bits, 
    double transfers_per_ns, 
    std::size_t fifo_depth
)
    : id_(id),
      width_bits_(width_bits),
      transfers_per_ns_(transfers_per_ns),
      fifo_(fifo_depth),
      active_request_(std::nullopt) {
    if (width_bits_ == 0 || transfers_per_ns_ <= 0.0) {
        throw std::invalid_argument(
            "Link width and transfer rate must be positive"
        );
    }
}

bool Link::Accept(const LinkRequest& request) {
    if (!IsBusy()) {
        active_request_ = CreateActiveTransfer(request, request.ready_time_ns);
        return true;
    } return fifo_.TryPush(request);
}

ActiveTransfer Link::CompleteTransfer() {
    ActiveTransfer completed_transfer = active_request_.value();
    if (HasWaitingRequest()) {
        active_request_ = CreateActiveTransfer(
            fifo_.Pop(),
            completed_transfer.finish_time_ns
        );
    } else {
        active_request_ = std::nullopt;
    }
    return completed_transfer;
}

bool Link::IsBusy() const {
    return active_request_.has_value();
}

bool Link::HasWaitingRequest() const {
    return !fifo_.IsEmpty();
}

std::size_t Link::WaitingRequestCount() const noexcept {
    return fifo_.Size();
}

double Link::TransferTimeNs(unsigned bits) const {
    return static_cast<double>(bits) /
        static_cast<double>(width_bits_) /
        transfers_per_ns_;
}

std::optional<double> Link::CompletionTimeNs() const noexcept {
    if (!active_request_) {
        return std::nullopt;
    } else {
        return active_request_->finish_time_ns;
    }
}

ActiveTransfer Link::CreateActiveTransfer(
    const LinkRequest& request,
    double start_time_ns
) {
    return ActiveTransfer{
        .request = request,
        .link_id = id_,
        .start_time_ns = start_time_ns,
        .finish_time_ns 
            = start_time_ns + TransferTimeNs(request.transfer_bits)
    };
}
