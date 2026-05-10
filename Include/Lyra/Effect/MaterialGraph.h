#ifndef LYRA_LYRA_EFFECT_MATERIALGRAPH_H
#define LYRA_LYRA_EFFECT_MATERIALGRAPH_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/Config.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Collections.h>

namespace lyra
{
    /**
     * @brief Supported data types for pins and properties in the material graph.
     */
    enum struct MaterialGraphValueType : uint
    {
        NONE,
        BOOL,
        INT,
        UINT,
        FLOAT,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        TEXTURE2D,
        TEXTURE_CUBE,
        SAMPLER
    };

    /**
     * @brief A variant holding a typed value for node properties or pin defaults.
     */
    struct MaterialGraphProperty
    {
        Variant<bool, int, uint, float, Vector2, Vector3, Vector4, String> value;

        MaterialGraphProperty() : value(0.0f) {}
        MaterialGraphProperty(bool v) : value(v) {}
        MaterialGraphProperty(int v) : value(v) {}
        MaterialGraphProperty(uint v) : value(v) {}
        MaterialGraphProperty(float v) : value(v) {}
        MaterialGraphProperty(const Vector2& v) : value(v) {}
        MaterialGraphProperty(const Vector3& v) : value(v) {}
        MaterialGraphProperty(const Vector4& v) : value(v) {}
        MaterialGraphProperty(const String& v) : value(v) {}
    };

    /**
     * @brief A pin on a material graph node.
     */
    struct MaterialGraphPin
    {
        uint                   id = 0;
        String                 name;
        MaterialGraphValueType type          = MaterialGraphValueType::NONE;
        MaterialGraphProperty  default_value = {};
    };

    /**
     * @brief A node in the material graph.
     */
    struct MaterialGraphNode
    {
        uint                                   id = 0;
        String                                 type; ///< e.g., "Add", "TextureSample", "CustomSlang"
        Vector<MaterialGraphPin>               inputs;
        Vector<MaterialGraphPin>               outputs;
        HashMap<String, MaterialGraphProperty> properties;
    };

    /**
     * @brief A connection between two pins in the material graph.
     */
    struct MaterialGraphLink
    {
        uint id        = 0;
        uint from_node = 0;
        uint from_pin  = 0;
        uint to_node   = 0;
        uint to_pin    = 0;
    };

    /**
     * @brief Describes the shading algorithm/graph for a material.
     */
    struct MaterialGraph
    {
        Vector<MaterialGraphNode> nodes;
        Vector<MaterialGraphLink> links;

        /**
         * @brief Loads a material graph from a JSON object.
         * @param json The JSON object containing the graph data.
         */
        void load(const JSON& json);

        /**
         * @brief Saves the material graph to a JSON object.
         * @return The JSON object representing the graph.
         */
        JSON save() const;
    };

} // namespace lyra

#endif // LYRA_LYRA_EFFECT_MATERIALGRAPH_H
