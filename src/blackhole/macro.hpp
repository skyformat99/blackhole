#pragma once

#include "blackhole/logger.hpp"
#include "blackhole/detail/logger/pusher.hpp"
#include "blackhole/detail/syntax.hpp"

#define BH_LOG(__log__, __level__, ...) \
    if (auto record = (__log__).open_record((__level__))) \
        if (blackhole::aux::syntax_check(__VA_ARGS__)) \
            blackhole::aux::logger::make_pusher((__log__), record, __VA_ARGS__)
