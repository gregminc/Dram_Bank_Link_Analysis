

#pragma once

class BankLinkMapper {
public: 
    BankLinkMapper(unsigned number_of_banks, unsigned banks_per_link);

    [[nodiscard]] unsigned LinkForBank(unsigned bank) const;
    [[nodiscard]] unsigned NumberOfLinks() const;

private:
    unsigned number_of_banks_;
    unsigned banks_per_link;
    unsigned number_of_links_;
};