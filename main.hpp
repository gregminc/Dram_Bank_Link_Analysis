

#pragma once

// Matches DRAMsim3 configs/LPDDR4_8Gb_x16_2400.ini.
constexpr unsigned CHANNELS = 1;
constexpr unsigned RANKS = 2;
constexpr unsigned BANK_GROUPS = 2;
constexpr unsigned BANKS_PER_GROUP = 4;
constexpr unsigned ROWS = 65536;
constexpr unsigned COLUMNS = 1024;
constexpr unsigned BUS_WIDTH_BYTES = 8;
constexpr unsigned BURST_LENGTH = 16;
