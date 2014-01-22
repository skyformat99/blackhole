#pragma once

#include "socket/udp.hpp"
#include "socket/tcp.hpp"

namespace blackhole {

namespace sink {

namespace socket {

struct config_t {
    std::string host;
    std::uint16_t port;
};

} // namespace socket

template<typename Protocol, typename Backend = socket::boost_backend_t<Protocol> >
class socket_t {
    Backend m_backend;

public:
    typedef socket::config_t config_type;

    static const char* name() {
        return Backend::name();
    }

    static std::string cfgname() {
        return name();
    }

    static std::string parse(const boost::any&) {
        return name();
    }

    socket_t(const config_type& config) :
        m_backend(config.host, config.port)
    {}

    socket_t(const std::string& host, std::uint16_t port) :
        m_backend(host, port)
    {}

    void consume(const std::string& message) {
        m_backend.write(message);
    }

    Backend& backend() {
        return m_backend;
    }
};

} // namespace sink

template<typename Protocol>
struct factory_traits<sink::socket_t<Protocol>> {
    typedef typename sink::socket_t<Protocol>::config_type config_type;

    static config_type map_config(const boost::any& config) {
        config_type cfg;
        aux::vector_to(config, cfg.host, cfg.port);
        return cfg;
    }
};

} // namespace blackhole
