#include "bank_link_mapper.hpp"

#include <stdexcept>

BankLinkMapper::BankLinkMapper(
    unsigned number_of_banks,
    unsigned banks_per_link_value
)
    : number_of_banks_(number_of_banks),
      banks_per_link(banks_per_link_value),
      number_of_links_(0) {
    if (number_of_banks_ == 0 || banks_per_link == 0) {
        throw std::invalid_argument(
            "Bank count and banks per link must be nonzero"
        );
    }

    number_of_links_ =
        (number_of_banks_ + banks_per_link - 1) / banks_per_link;
}

unsigned BankLinkMapper::LinkForBank(unsigned bank) const {
    if (bank >= number_of_banks_) {
        throw std::out_of_range("Bank index is outside the configured range");
    }
    return bank / banks_per_link;
}

unsigned BankLinkMapper::NumberOfLinks() const {
    return number_of_links_;
}
