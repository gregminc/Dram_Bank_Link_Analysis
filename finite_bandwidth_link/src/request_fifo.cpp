

#include "request_fifo.hpp"

#include <stdexcept>

RequestFifo::RequestFifo(std::size_t capacity) 
    : buffer_(capacity) {
    if (capacity == 0) {
        throw std::invalid_argument("Fifo capacity must be nonzero");
    } 
}

bool RequestFifo::TryPush(const LinkRequest& request) {
    if (IsFull()) {
        return false;
    } else {
        buffer_[tail_] = request;
        tail_ = NextIndex(tail_);
        ++size_;
        return true;
    }
}

const LinkRequest& RequestFifo::Front() const {
    if (IsEmpty()) {
        throw std::underflow_error("Fifo underflow");
    } else {
        return buffer_[head_];
    }
}

LinkRequest RequestFifo::Pop() {
    if (IsEmpty()) {
        throw std::underflow_error("Fifo underflow");
    } else {
        LinkRequest temp = buffer_[head_];
        --size_;
        head_ = NextIndex(head_);
        return temp;
    }
}

void RequestFifo::Clear() noexcept {
    head_ = 0;
    tail_ = 0;
    size_ = 0;
}

bool RequestFifo::IsEmpty() const noexcept {
    return !size_;
}

bool RequestFifo::IsFull() const noexcept {
    return size_ == buffer_.size();
}

std::size_t RequestFifo::Size() const noexcept {
    return size_;
}

std::size_t RequestFifo::Capacity() const noexcept {
    return buffer_.size();
}

std::size_t RequestFifo::NextIndex(std::size_t index) const noexcept {
    if (index < buffer_.size() - 1) {
        return ++index;
    } else return 0;
}
