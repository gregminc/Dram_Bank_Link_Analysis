#include "link.hpp"

#include <cmath>
#include <stdexcept>

namespace {

bool NearlyEqual(double left, double right) {
    return std::abs(left - right) < 1e-9;
}

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

int main() {
    Link link(0, 16, 4.0, 1);
    const LinkRequest first{
        .id = 1,
        .ready_time_ns = 0.0,
        .bank = 0,
        .transfer_bits = 256,
    };
    const LinkRequest second{
        .id = 2,
        .ready_time_ns = 1.0,
        .bank = 0,
        .transfer_bits = 256,
    };
    const LinkRequest third{
        .id = 3,
        .ready_time_ns = 2.0,
        .bank = 0,
        .transfer_bits = 256,
    };

    Require(link.Accept(first), "idle link rejected first request");
    Require(link.IsBusy(), "link should be busy");
    Require(
        link.CompletionTimeNs() &&
            NearlyEqual(*link.CompletionTimeNs(), 4.0),
        "first completion time should be four"
    );
    Require(link.Accept(second), "link failed to queue second request");
    Require(link.WaitingRequestCount() == 1, "FIFO should contain one request");
    Require(!link.Accept(third), "full link FIFO accepted third request");

    const ActiveTransfer completed_first = link.CompleteTransfer();
    Require(completed_first.request.id == 1, "wrong first completed request");
    Require(
        NearlyEqual(completed_first.start_time_ns, 0.0),
        "wrong first start time"
    );
    Require(
        NearlyEqual(completed_first.finish_time_ns, 4.0),
        "wrong first finish time"
    );
    Require(
        link.CompletionTimeNs() &&
            NearlyEqual(*link.CompletionTimeNs(), 8.0),
        "second completion time should be eight"
    );

    const ActiveTransfer completed_second = link.CompleteTransfer();
    Require(completed_second.request.id == 2, "wrong second completed request");
    Require(
        NearlyEqual(completed_second.start_time_ns, 4.0),
        "wrong second start time"
    );
    Require(
        NearlyEqual(completed_second.finish_time_ns, 8.0),
        "wrong second finish time"
    );
    Require(!link.IsBusy(), "link should be idle after second completion");
    Require(!link.CompletionTimeNs(), "idle link should not have completion time");
}
