#pragma once

#include <functional>
#include <string>

#include <boost/format.hpp>

#include "blackhole/attribute.hpp"
#include "blackhole/utils/unique.hpp"
#include "blackhole/utils/underlying.hpp"

namespace blackhole {

namespace mapping {

template<typename T>
struct extracter {
    std::function<std::string(const T&)> func;

    extracter(std::function<std::string(const T&)> func) :
        func(func)
    {}

    std::string operator()(const T& value) const {
        return func(value);
    }

    std::string operator()(T value) const {
        return func(value);
    }

    std::string operator()(const log::attribute_value_t& value) const {
        typedef typename aux::underlying_type<T>::type underlying_type;
        return func(static_cast<T>(boost::get<underlying_type>(value)));
    }
};

class mapper_t {
    typedef std::function<std::string(const log::attribute_value_t&)> mapping_t;
    std::unordered_map<std::string, mapping_t> m_mappings;

public:
    template<typename T>
    void add(const std::string& key, std::function<std::string(const T&)> handler) {
        m_mappings[key] = extracter<T>(handler);
    }

    template<typename T>
    std::tuple<std::string, bool> execute(const std::string& key, T&& value) const {
        auto it = m_mappings.find(key);
        if (it != m_mappings.end()) {
            const mapping_t& action = it->second;
            return std::make_tuple(action(std::forward<T>(value)), true);
        }
        return std::make_tuple(std::string(""), false);
    }
};

inline void apply(const mapper_t& mapper, const std::string& key, const log::attribute_t& attribute, boost::format* format) {
    bool ok;
    std::string result;
    std::tie(result, ok) = mapper.execute(key, attribute.value);
    if (ok) {
        (*format) % result;
    } else {
        (*format) % attribute.value;
    }
}

} // namespace mapping

} // namespace blackhole
