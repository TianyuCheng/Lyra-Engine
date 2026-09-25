#pragma once

#ifndef LYRA_REFLECT_MODEL_H
#define LYRA_REFLECT_MODEL_H

#include <map>
#include <vector>
#include <string>

namespace lyra::reflect
{
    /**
     * @brief Represents a parsed C++ attribute, e.g. [[lyra::edit(euler, order = YXZ)]].
     */
    struct Attribute
    {
        std::string                        name;
        std::vector<std::string>           positional_args;
        std::map<std::string, std::string> named_args;

        bool has_arg(const std::string& key) const
        {
            return named_args.find(key) != named_args.end();
        }

        auto get_arg(const std::string& key, const std::string& default_val = "") const -> std::string
        {
            auto it = named_args.find(key);
            if (it == named_args.end()) {
                return default_val;
            }
            return it->second;
        }

        auto get_pos(size_t index, const std::string& default_val = "") const -> std::string
        {
            if (index >= positional_args.size()) {
                return default_val;
            }
            return positional_args[index];
        }
    };

    /**
     * @brief Reflected struct field metadata.
     */
    struct FieldMeta
    {
        std::string name;
        std::string type;
        std::string default_value;

        std::string drawer;
        std::string drawer_options;

        std::string min_val;
        std::string max_val;
        std::string speed;
        std::string reset_val;

        std::string tooltip;
        std::string label;

        bool hidden = false;
        int  line   = 0;
    };

    /**
     * @brief Reflected component struct metadata.
     */
    struct ComponentMeta
    {
        std::string            name;
        std::string            category = "General";
        std::string            tooltip;
        std::vector<FieldMeta> fields;

        std::string header_file;
        std::string relative_include;
        int         line = 0;
    };

    /**
     * @brief Parameter metadata for an ECS system function.
     */
    struct ParamMeta
    {
        std::string raw_type;
        std::string type;
        bool        is_const = false;
        bool        is_ref   = false;
        bool        is_query = false;

        std::vector<ParamMeta> query_subtypes;
    };

    /**
     * @brief Reflected free-function system metadata.
     */
    struct SystemMeta
    {
        std::string name;
        std::string stage = "UPDATE";
        std::string group;
        std::string header_file;
        std::string relative_include;
        int         line          = 0;
        bool        run_in_editor = false;

        std::vector<ParamMeta> params;
    };

    /**
     * @brief Aggregated reflection module container.
     */
    struct ModuleReflection
    {
        std::string                module_name;
        std::vector<ComponentMeta> components;
        std::vector<SystemMeta>    systems;
        std::vector<std::string>   included_headers;
    };

} // namespace lyra::reflect

#endif // LYRA_REFLECT_MODEL_H
