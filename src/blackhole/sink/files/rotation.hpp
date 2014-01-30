#pragma once

#include <cstdint>
#include <string>

#include <boost/algorithm/string.hpp>

#include "blackhole/sink/files/rotation/config.hpp"
#include "blackhole/sink/files/rotation/counter.hpp"
#include "blackhole/sink/files/rotation/timer.hpp"

namespace blackhole {

namespace sink {

//! Tag for file sinks with no rotation.
template<class Backend, class Timer = timer_t> class NoRotation;

template<class Backend, class Timer = timer_t>
class rotator_t {
    rotation::config_t config;
    Backend& backend;
    Timer m_timer;
    basename::generator_t generator;
    rotation::counter_t counter;
public:
    static const char* name() {
        return "rotate";
    }

    rotator_t(const rotation::config_t& config, Backend& backend) :
        config(config),
        backend(backend),
        generator(config.pattern),
        counter(rotation::counter_t::from_string(config.pattern))
    {}

    Timer& timer() {
        return m_timer;
    }

    bool necessary() const {
        // In case of size based rotation everything is simple: file.size() + message.size() >= size;
        // In case of datetime based rotation that's seems to be a bit difficult: get_current_time() >= current_fence().
        return false;
    }

    void rotate() const {
        backend.flush();
        backend.close();
        rollover();
        backend.open();
    }

private:
    struct rollover_pair_t {
        std::string current;
        std::string renamed;
    };

    void rollover() const {
        const std::string& filename = backend.filename();
        const std::string& basename = generator.basename(filename);

        if (counter.valid()) {
            rollover(backend.listdir(), basename);
        }

        if (backend.exists(filename)) {
            backend.rename(filename, backup_filename(basename));
        }
    }

    void rollover(std::vector<std::string> filenames, const std::string& pattern) const {
        filenames.erase(std::remove_if(filenames.begin(), filenames.end(), matching::datetime_t(pattern)), filenames.end());
        std::sort(filenames.begin(), filenames.end(), comparator::time::ascending<Backend>(backend));
        for (auto it = filenames.begin(); it != filenames.end(); ++it) {
            const std::string& filename = *it;
            if (backend.exists(filename)) {
                backend.rename(filename, counter.next(filename));
            }
        }
    }

    std::string backup_filename(const std::string& pattern) const {
        std::string filename = pattern;
        boost::algorithm::replace_all(filename, "%N", "1");
        std::time_t time = m_timer.current();
        char buf[128];
        if (strftime(buf, 128, filename.data(), std::gmtime(&time)) == 0) {
            // Do nothing.
        }

        return std::string(buf);
    }
};

} // namespace sink

} // namespace blackhole