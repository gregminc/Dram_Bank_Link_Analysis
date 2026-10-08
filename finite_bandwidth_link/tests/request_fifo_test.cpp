#include "request_fifo.hpp"

#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

int main() {
    RequestFifo fifo(3);
    Require(fifo.IsEmpty(), "new FIFO should be empty");
    Require(fifo.Capacity() == 3, "FIFO capacity should be three");

    Require(fifo.TryPush(LinkRequest{.id = 1}), "push one failed");
    Require(fifo.TryPush(LinkRequest{.id = 2}), "push two failed");
    Require(fifo.TryPush(LinkRequest{.id = 3}), "push three failed");
    Require(fifo.IsFull(), "FIFO should be full");
    Require(!fifo.TryPush(LinkRequest{.id = 4}), "full FIFO accepted data");

    Require(fifo.Front().id == 1, "front request should be one");
    Require(fifo.Pop().id == 1, "first pop should be one");
    Require(fifo.TryPush(LinkRequest{.id = 4}), "wraparound push failed");
    Require(fifo.Pop().id == 2, "second pop should be two");
    Require(fifo.Pop().id == 3, "third pop should be three");
    Require(fifo.Pop().id == 4, "fourth pop should be four");
    Require(fifo.IsEmpty(), "FIFO should be empty after pops");

    bool underflowed = false;
    try {
        static_cast<void>(fifo.Pop());
    } catch (const std::underflow_error&) {
        underflowed = true;
    }
    Require(underflowed, "empty pop should throw underflow_error");
}
