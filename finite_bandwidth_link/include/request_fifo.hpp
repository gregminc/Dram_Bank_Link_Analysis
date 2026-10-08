

#pragma once

#include "simulation_types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

class RequestFifo {
public:
    explicit RequestFifo(std::size_t capacity);

    bool TryPush(const LinkRequest& request);
    const LinkRequest& Front() const;
    LinkRequest Pop();

    void Clear() noexcept;

    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] bool IsFull() const noexcept;
    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] std::size_t Capacity() const noexcept;

private:
    [[nodiscard]] std::size_t NextIndex(std::size_t index) const noexcept;

    std::vector<LinkRequest> buffer_;
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t size_ = 0;
};
