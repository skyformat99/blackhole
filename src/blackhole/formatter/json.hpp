#pragma once

#include <list>
#include <string>
#include <vector>
#include <unordered_map>

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "blackhole/error.hpp"
#include "blackhole/factory.hpp"
#include "blackhole/formatter/base.hpp"
#include "blackhole/formatter/json/config.hpp"
#include "blackhole/formatter/map/value.hpp"
#include "blackhole/record.hpp"
#include "blackhole/utils/actions/empty.hpp"
#include "blackhole/utils/nullptr.hpp"
#include "blackhole/utils/split.hpp"

namespace blackhole {

namespace formatter {

namespace aux {
template<typename Visitor, typename T>
void apply_visitor(Visitor&, const std::string&, const T&);
} // namespace aux

//! This class looks creppy, because of inconvenient rapidjson interface. Hope someone could refactor it.
class json_visitor_t : public boost::static_visitor<> {
    rapidjson::Document* root;
    const json::config_t& config;
    const mapping::value_t& mapper;

    // Mapped attribute values cache helps to keep them alive.
    std::list<std::string> cache;

    // There is no other way to pass additional argument when invoking `apply_visitor` except
    // explicit setting it every iteration. To do this we grant `name` member access to setter
    // function.
    template<typename Visitor, typename T>
    friend void aux::apply_visitor(Visitor&, const std::string&, const T&);

    const std::string* name;
public:
    json_visitor_t(rapidjson::Document* root, const json::config_t& config, const mapping::value_t& mapper) :
        root(root),
        config(config),
        mapper(mapper),
        name(nullptr)
    {}

    template<typename T>
    void operator ()(const T& value) {
        auto it = config.routing.specified.find(*name);
        if (it != config.routing.specified.end()) {
            const json::map::routing_t::routes_t& positions = it->second;
            if (positions.size() > 0) {
                add_positional(positions, *name, value);
            } else {
                map_and_add_member(root, *name, value);
            }
        } else if (!config.routing.unspecified.empty()) {
            add_positional(config.routing.unspecified, *name, value);
        } else {
            map_and_add_member(root, *name, value);
        }
    }

private:
    template<typename T>
    void add_positional(const json::map::routing_t::routes_t& positions, const std::string& name, const T& value) {
        rapidjson::Value* current = root;
        for (auto it = positions.begin(); it != positions.end(); ++it) {
            const std::string& position = *it;
            if (!current->HasMember(position.c_str())) {
                current = add_node(current, position);
            } else {
                current = get_child(current, position);
            }
        }

        map_and_add_member(current, name, value);
    }

    rapidjson::Value* add_node(rapidjson::Value* parent, const std::string& name) {
        rapidjson::Value current;
        current.SetObject();
        add_member_impl(parent, name, current);
        return get_child(parent, name);
    }

    rapidjson::Value* get_child(rapidjson::Value* node, const std::string& name) const {
        return &(*node)[name.c_str()];
    }

    template<typename T>
    void map_and_add_member(rapidjson::Value* node, const std::string& name, const T& value) {
        auto result = mapper.execute(name, value);
        if (result.is_initialized()) {
            cache.push_back(result.get());
            add_member(node, name, cache.back());
        } else {
            add_member(node, name, value);
        }
    }

    template<typename T, class = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    void add_member(rapidjson::Value* node, const std::string& name, T value) {
        add_member_impl(node, name, value);
    }

    void add_member(rapidjson::Value* node, const std::string& name, std::time_t value) {
        add_member_impl(node, name, static_cast<int64_t>(value));
    }

    void add_member(rapidjson::Value* node, const std::string& name, const std::string& value) {
        add_member_impl(node, name, value.c_str());
    }

    template<typename T>
    void add_member_impl(rapidjson::Value* node, const std::string& name, T&& value) {
        node->AddMember(mapped(name).c_str(), std::forward<T>(value), root->GetAllocator());
    }

    const std::string& mapped(const std::string& name) const {
        auto it = config.naming.find(name);
        if (it != config.naming.end()) {
            return it->second;
        }
        return name;
    }
};

namespace aux {

template<typename Visitor, typename T>
void apply_visitor(Visitor& visitor, const std::string& name, const T& value) {
    visitor.name = &name;
    boost::apply_visitor(visitor, value);
}

} // namespace aux

class json_t : public base_t {
    const json::config_t config;

public:
    typedef json::config_t config_type;

    static const char* name() {
        return "json";
    }

    json_t(const json::config_t& config = json::config_t()) :
        config(config)
    {}

    std::string format(const log::record_t& record) const {
        rapidjson::Document root;
        root.SetObject();

        json_visitor_t visitor(&root, config, mapper);
        for (auto it = record.attributes.begin(); it != record.attributes.end(); ++it) {
            const std::string& name = it->first;
            const log::attribute_t& attribute = it->second;
            aux::apply_visitor(visitor, name, attribute.value);
        }

        rapidjson::GenericStringBuffer<rapidjson::UTF8<>> buffer;
        rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF8<>>> writer(buffer);

        root.Accept(writer);
        std::string message = std::string(buffer.GetString(), buffer.Size());
        if (config.newline) {
            message.push_back('\n');
        }
        return message;
    }
};

} // namespace formatter

template<>
struct factory_traits<formatter::json_t> {
    typedef formatter::json_t formatter_type;
    typedef formatter::json_t::config_type config_type;

    static void map_config(const aux::extractor<formatter_type>& ex, config_type& cfg) {
        using namespace formatter::json::map;

        ex["newline"].to(cfg.newline);

        auto mapping = ex["mapping"].get<std::map<std::string, boost::any>>();
        for (auto it = mapping.begin(); it != mapping.end(); ++it) {
            cfg.naming[it->first] = boost::any_cast<std::string>(it->second);
        }

        auto routing = ex["routing"].get<std::map<std::string, boost::any>>();
        for (auto it = routing.begin(); it != routing.end(); ++it) {
            const std::string& name = it->first;
            const boost::any& value = it->second;

            if (value.type() == typeid(std::string)) {
                handle_unspecified(name, value, cfg);
            } else if (value.type() == typeid(std::vector<std::string>)) {
                handle_specified(name, value, cfg);
            } else {
                throw blackhole::error_t("wrong configuration");
            }
        }
    }

    static void handle_specified(const std::string& name, const boost::any& value, config_type& cfg) {
        std::vector<std::string> positions = aux::split(name, "/");
        std::vector<std::string> keys;
        aux::any_to(value, keys);
        for (auto it = keys.begin(); it != keys.end(); ++it) {
            const std::string& key = *it;
            cfg.routing.specified[key] = positions;
        }
    }

    static void handle_unspecified(const std::string& name, const boost::any& value, config_type& cfg) {
        if (boost::any_cast<std::string>(value) == "*") {
            cfg.routing.unspecified = aux::split(name, "/");
        } else {
            throw blackhole::error_t("wrong configuration");
        }
    }
};

} // namespace blackhole
