#pragma once

#include <sstream>
#include <type_traits>

#include "blackhole/attribute.hpp"
#include "blackhole/detail/traits/supports/stream_push.hpp"
#include "blackhole/detail/util/lazy.hpp"

namespace blackhole {

namespace aux {

/*!
 * Converts various types to log event attributes.
 *
 * Helper hierarchy of template classes that allows to pass objects of user
 * defined classes as attribute in tha main logging macro.
 * To make this possible, user defined class must have fully defined stream
 * push `operator<<`.
 * The logic is as follows: if the object can be implicitly converted to
 * the `attribute::value_t` object, then that convertion is used.
 * Otherwise the library would check via SFINAE if the custom class has defined
 * stream push `operator<<` and, if yes - uses it.
 * Otherwise static assert with human-readable message is triggered.
 */
template<typename T, class = void>
struct conv;

template<typename T>
struct conv<T, typename std::enable_if<
        attribute::is_constructible<T>::value>::type
    >
{
    static attribute::value_t from(T&& value) {
        return attribute::value_t(std::forward<T>(value));
    }
};

template<typename T>
struct conv<T, typename std::enable_if<
        !attribute::is_constructible<T>::value &&
        traits::supports::stream_push<T>::value>::type
    >
{
    static attribute::value_t from(T&& value) {
        std::ostringstream stream;
        stream << value;
        return attribute::value_t(stream.str());
    }
};

template<typename T>
struct conv<T, typename std::enable_if<
        !attribute::is_constructible<T>::value &&
        !traits::supports::stream_push<T>::value>::type
    >
{
    static attribute::value_t from(T&&) {
        static_assert(
            aux::util::lazy_false<T>::value,
            "stream operator<< is not defined for type `T`"
        );
        return attribute::value_t();
    }
};

} // namespace aux

} // namespace blackhole
