#pragma once

#ifndef LYRA_ENGINE_SCENE_OCT_TREE_H
#define LYRA_ENGINE_SCENE_OCT_TREE_H

#include <algorithm>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/Bounds.h>
#include <Lyra/Utilities/Geometry.h>
#include <Lyra/Utilities/Intersect.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/ECS.h>
#include <Lyra/Scene/Camera.h>
#include <Lyra/Scene/Transform.h>

namespace lyra
{
    /**
     * @brief 3d spatial partitioning tree for fast scene range, point, sphere, ray, and frustum culling queries.
     *
     * uses a contiguous node pool with block-allocated children (8 per node) for cache locality.
     * tracks element locations via a hash map for o(1) removals and fast in-place updates.
     */
    template <typename T = Entity>
    class OctTree
    {
    public:
        using NodeIndex                         = uint;
        static constexpr NodeIndex INVALID_NODE = NodeIndex(-1);

        struct ElementEntry
        {
            T        element;
            Bounds3D bounds;
        };

        struct RayHit
        {
            T        element;
            float    distance = 0.0f;
            Bounds3D bounds;

            bool operator<(const RayHit& other) const
            {
                return distance < other.distance;
            }
        };

        struct Node
        {
            Bounds3D             bounds;
            NodeIndex            parent      = INVALID_NODE;
            NodeIndex            first_child = INVALID_NODE; // contiguous block of 8 children
            Vector<ElementEntry> elements;
            uint                 depth = 0;

            FORCE_INLINE bool is_leaf() const { return first_child == INVALID_NODE; }
        };

        struct ElementLocation
        {
            NodeIndex node_index    = INVALID_NODE;
            uint      element_index = 0;
        };

        explicit OctTree(
            Bounds3D root_bounds           = {{-1000.0f, -1000.0f, -1000.0f}, {1000.0f, 1000.0f, 1000.0f}},
            uint     max_depth             = 8,
            uint     max_elements_per_node = 16)
            : root_bounds(root_bounds), max_depth_limit(max_depth), node_capacity(max_elements_per_node)
        {
            clear();
        }

        /**
         * @brief insert an element with given 3d bounding box into the tree.
         * @return true if successfully inserted, false if element already exists or is out of bounds.
         */
        bool insert(const T& element, const Bounds3D& bounds)
        {
            if (element_map.find(element) != element_map.end()) {
                return false;
            }

            if (!nodes[0].bounds.contains(bounds)) {
                return false;
            }

            insert_recursive(0, element, bounds);
            ++element_count;
            return true;
        }

        /**
         * @brief remove an element from the tree in o(1) time.
         * @return true if found and removed, false otherwise.
         */
        bool remove(const T& element)
        {
            auto it = element_map.find(element);
            if (it == element_map.end()) {
                return false;
            }

            NodeIndex node_idx = it->second.node_index;
            uint      elem_idx = it->second.element_index;
            auto&     node     = nodes[node_idx];

            uint last_idx = static_cast<uint>(node.elements.size() - 1);
            if (elem_idx != last_idx) {
                // swap with last element
                node.elements[elem_idx]                                    = std::move(node.elements[last_idx]);
                element_map[node.elements[elem_idx].element].element_index = elem_idx;
            }
            node.elements.pop_back();
            element_map.erase(it);
            --element_count;
            return true;
        }

        /**
         * @brief update an element's bounding box.
         *
         * checks if element still fits in its current node for fast in-place update.
         */
        bool update(const T& element, const Bounds3D& new_bounds)
        {
            auto it = element_map.find(element);
            if (it == element_map.end()) {
                return false;
            }

            NodeIndex current_node_idx = it->second.node_index;
            uint      elem_idx         = it->second.element_index;
            auto&     current_node     = nodes[current_node_idx];

            // fast path: still fits in current node
            if (current_node.bounds.contains(new_bounds)) {
                // if it has children, check if it now fits entirely inside a child
                if (!current_node.is_leaf()) {
                    int child_sub = get_child_index(current_node.bounds, new_bounds);
                    if (child_sub == -1) {
                        // stays in current node
                        current_node.elements[elem_idx].bounds = new_bounds;
                        return true;
                    }
                } else {
                    current_node.elements[elem_idx].bounds = new_bounds;
                    return true;
                }
            }

            // re-insert path
            remove(element);
            return insert(element, new_bounds);
        }

        /**
         * @brief check if an element is in the tree.
         */
        FORCE_INLINE bool contains(const T& element) const
        {
            return element_map.find(element) != element_map.end();
        }

        /**
         * @brief get the current bounding box of an element.
         */
        Optional<Bounds3D> get_bounds(const T& element) const
        {
            auto it = element_map.find(element);
            if (it == element_map.end()) {
                return std::nullopt;
            }
            const auto& node = nodes[it->second.node_index];
            return node.elements[it->second.element_index].bounds;
        }

        /**
         * @brief clear all nodes and elements, resetting tree to initial root bounds.
         */
        void clear()
        {
            nodes.clear();
            element_map.clear();
            element_count = 0;

            Node root;
            root.bounds = root_bounds;
            root.depth  = 0;
            nodes.push_back(std::move(root));
        }

        /**
         * @brief rebuild tree with new root bounds.
         */
        void rebuild(const Bounds3D& new_bounds)
        {
            Vector<ElementEntry> all_elements;
            all_elements.reserve(element_count);
            for (const auto& node : nodes) {
                for (const auto& e : node.elements) {
                    all_elements.push_back(e);
                }
            }

            root_bounds = new_bounds;
            clear();

            for (const auto& entry : all_elements) {
                insert(entry.element, entry.bounds);
            }
        }

        /**
         * @brief grow root bounds to encompass target bounds if needed.
         */
        void grow_to_fit(const Bounds3D& target_bounds)
        {
            if (nodes.empty() || !nodes[0].bounds.contains(target_bounds)) {
                Bounds3D expanded = nodes.empty() ? target_bounds : nodes[0].bounds.merged(target_bounds);
                // expand margin
                Vector3 sz = expanded.size();
                expanded.min -= sz * 0.1f;
                expanded.max += sz * 0.1f;
                rebuild(expanded);
            }
        }

        // =====================================================================
        // Queries
        // =====================================================================

        /**
         * @brief query elements overlapping a given bounding box with callback.
         *
         * returning false from callback halts traversal early.
         */
        void query(
            const Bounds3D&                                     box,
            const FunctionRef<bool(const T&, const Bounds3D&)>& callback) const
        {
            query_box_recursive(0, box, callback);
        }

        /**
         * @brief query all elements overlapping a given bounding box.
         */
        Vector<T> query(const Bounds3D& box) const
        {
            Vector<T> results;
            query(box, [&](const T& elem, const Bounds3D&) {
                results.push_back(elem);
                return true;
            });
            return results;
        }

        /**
         * @brief query elements containing a point with callback.
         */
        void query(
            Vector3                                             point,
            const FunctionRef<bool(const T&, const Bounds3D&)>& callback) const
        {
            query_point_recursive(0, point, callback);
        }

        /**
         * @brief query all elements containing a point.
         */
        Vector<T> query(Vector3 point) const
        {
            Vector<T> results;
            query(point, [&](const T& elem, const Bounds3D&) {
                results.push_back(elem);
                return true;
            });
            return results;
        }

        /**
         * @brief query elements overlapping a sphere with callback.
         */
        void query(
            const Sphere&                                       sphere,
            const FunctionRef<bool(const T&, const Bounds3D&)>& callback) const
        {
            query_sphere_recursive(0, sphere, callback);
        }

        /**
         * @brief query all elements overlapping a sphere.
         */
        Vector<T> query(const Sphere& sphere) const
        {
            Vector<T> results;
            query(sphere, [&](const T& elem, const Bounds3D&) {
                results.push_back(elem);
                return true;
            });
            return results;
        }

        /**
         * @brief query elements intersecting a ray with callback.
         */
        void query(
            const Ray3D&                                               ray,
            const FunctionRef<bool(const T&, const Bounds3D&, float)>& callback,
            float                                                      max_distance = 1e9f) const
        {
            query_ray_recursive(0, ray, callback, max_distance);
        }

        /**
         * @brief query all elements intersecting a ray, sorted by hit distance.
         */
        Vector<RayHit> query_ray(const Ray3D& ray, float max_distance = 1e9f) const
        {
            Vector<RayHit> hits;
            query(
                ray, [&](const T& elem, const Bounds3D& b, float dist) {
                hits.push_back({elem, dist, b});
                return true;
            }, max_distance);
            std::sort(hits.begin(), hits.end());
            return hits;
        }

        /**
         * @brief view frustum culling query with callback.
         *
         * leverages 3-state hierarchical pruning (disjoint / contains / intersects).
         */
        void query(
            const Frustum&                                      frustum,
            const FunctionRef<bool(const T&, const Bounds3D&)>& callback) const
        {
            query_frustum_recursive(0, frustum, callback);
        }

        /**
         * @brief view frustum culling query returning all visible elements.
         */
        Vector<T> query(const Frustum& frustum) const
        {
            Vector<T> results;
            query(frustum, [&](const T& elem, const Bounds3D&) {
                results.push_back(elem);
                return true;
            });
            return results;
        }

        /**
         * @brief view frustum culling query for a scene camera and world transform.
         */
        Vector<T> query(const Camera& camera, const TransformWorld& xform) const
        {
            Frustum frustum = extract_camera_frustum(camera, xform);
            return query(frustum);
        }

        /**
         * @brief custom hierarchy traversal with node predicate and element visitor.
         */
        template <typename NodePredicate, typename ElementCallback>
        void traverse(NodePredicate&& node_filter, ElementCallback&& element_callback) const
        {
            traverse_recursive(0, node_filter, element_callback);
        }

        // =====================================================================
        // Inspection & Statistics
        // =====================================================================

        FORCE_INLINE size_t          size() const { return element_count; }
        FORCE_INLINE size_t          node_count() const { return nodes.size(); }
        FORCE_INLINE bool            empty() const { return element_count == 0; }
        FORCE_INLINE const Bounds3D& bounds() const { return nodes.empty() ? root_bounds : nodes[0].bounds; }
        FORCE_INLINE uint            max_depth() const { return max_depth_limit; }
        FORCE_INLINE uint            max_elements_per_node() const { return node_capacity; }

    public:
        /**
         * @brief extract view frustum from a camera component and its world transform.
         */
        static inline Frustum extract_camera_frustum(const Camera& camera, const TransformWorld& xform)
        {
            Matrix4x4 view = glm::inverse(xform.xform);
            Matrix4x4 vp   = camera.projection * view;
            return Frustum::from_view_projection(vp);
        }

    private:
        /**
         * @brief determine which child octant completely contains the given bounds.
         * @return child octant index (0..7) or -1 if the bounds straddles split axes.
         */
        static int get_child_index(const Bounds3D& node_bounds, const Bounds3D& elem_bounds)
        {
            Vector3 mid = node_bounds.center();

            int x_bit = -1;
            if (elem_bounds.max.x <= mid.x) {
                x_bit = 0;
            } else if (elem_bounds.min.x >= mid.x) {
                x_bit = 1;
            }

            int y_bit = -1;
            if (elem_bounds.max.y <= mid.y) {
                y_bit = 0;
            } else if (elem_bounds.min.y >= mid.y) {
                y_bit = 1;
            }

            int z_bit = -1;
            if (elem_bounds.max.z <= mid.z) {
                z_bit = 0;
            } else if (elem_bounds.min.z >= mid.z) {
                z_bit = 1;
            }

            if (x_bit != -1 && y_bit != -1 && z_bit != -1) {
                return x_bit | (y_bit << 1) | (z_bit << 2);
            }
            return -1;
        }

        static Bounds3D compute_child_bounds(const Bounds3D& parent_bounds, int child_idx)
        {
            Vector3 min = parent_bounds.min;
            Vector3 max = parent_bounds.max;
            Vector3 mid = parent_bounds.center();

            float c_min_x = (child_idx & 1) ? mid.x : min.x;
            float c_max_x = (child_idx & 1) ? max.x : mid.x;
            float c_min_y = (child_idx & 2) ? mid.y : min.y;
            float c_max_y = (child_idx & 2) ? max.y : mid.y;
            float c_min_z = (child_idx & 4) ? mid.z : min.z;
            float c_max_z = (child_idx & 4) ? max.z : mid.z;

            return Bounds3D(c_min_x, c_min_y, c_min_z, c_max_x, c_max_y, c_max_z);
        }

        void subdivide(NodeIndex node_idx)
        {
            NodeIndex first_child       = static_cast<NodeIndex>(nodes.size());
            nodes[node_idx].first_child = first_child;
            uint     child_depth        = nodes[node_idx].depth + 1;
            Bounds3D p_bounds           = nodes[node_idx].bounds;

            nodes.resize(nodes.size() + 8);
            for (int i = 0; i < 8; ++i) {
                Node& child       = nodes[first_child + i];
                child.bounds      = compute_child_bounds(p_bounds, i);
                child.parent      = node_idx;
                child.depth       = child_depth;
                child.first_child = INVALID_NODE;
            }

            // re-distribute existing elements that cleanly fit into child octants
            Vector<ElementEntry> remaining;
            remaining.reserve(nodes[node_idx].elements.size());

            for (auto& entry : nodes[node_idx].elements) {
                int child_idx = get_child_index(p_bounds, entry.bounds);
                if (child_idx != -1) {
                    NodeIndex c_idx    = first_child + child_idx;
                    uint      elem_pos = static_cast<uint>(nodes[c_idx].elements.size());
                    nodes[c_idx].elements.push_back(std::move(entry));
                    element_map[nodes[c_idx].elements.back().element] = {c_idx, elem_pos};
                } else {
                    remaining.push_back(std::move(entry));
                }
            }

            nodes[node_idx].elements = std::move(remaining);
            // update element_map indices for remaining elements
            for (uint i = 0; i < nodes[node_idx].elements.size(); ++i) {
                element_map[nodes[node_idx].elements[i].element] = {node_idx, i};
            }
        }

        void insert_recursive(NodeIndex node_idx, const T& element, const Bounds3D& bounds)
        {
            auto& node = nodes[node_idx];

            if (!node.is_leaf()) {
                int child_idx = get_child_index(node.bounds, bounds);
                if (child_idx != -1) {
                    insert_recursive(node.first_child + child_idx, element, bounds);
                    return;
                }
            } else if (node.elements.size() >= node_capacity && node.depth < max_depth_limit) {
                subdivide(node_idx);
                int child_idx = get_child_index(nodes[node_idx].bounds, bounds);
                if (child_idx != -1) {
                    insert_recursive(nodes[node_idx].first_child + child_idx, element, bounds);
                    return;
                }
            }

            // store in current node
            uint elem_pos = static_cast<uint>(nodes[node_idx].elements.size());
            nodes[node_idx].elements.push_back({element, bounds});
            element_map[element] = {node_idx, elem_pos};
        }

        bool query_box_recursive(
            NodeIndex                                           node_idx,
            const Bounds3D&                                     box,
            const FunctionRef<bool(const T&, const Bounds3D&)>& callback) const
        {
            const auto& node = nodes[node_idx];
            if (!node.bounds.overlaps(box)) {
                return true;
            }

            for (const auto& entry : node.elements) {
                if (entry.bounds.overlaps(box)) {
                    if (!callback(entry.element, entry.bounds)) {
                        return false;
                    }
                }
            }

            if (!node.is_leaf()) {
                for (int i = 0; i < 8; ++i) {
                    if (!query_box_recursive(node.first_child + i, box, callback)) {
                        return false;
                    }
                }
            }

            return true;
        }

        bool query_point_recursive(
            NodeIndex                                           node_idx,
            Vector3                                             point,
            const FunctionRef<bool(const T&, const Bounds3D&)>& callback) const
        {
            const auto& node = nodes[node_idx];
            if (!node.bounds.contains(point)) {
                return true;
            }

            for (const auto& entry : node.elements) {
                if (entry.bounds.contains(point)) {
                    if (!callback(entry.element, entry.bounds)) {
                        return false;
                    }
                }
            }

            if (!node.is_leaf()) {
                for (int i = 0; i < 8; ++i) {
                    if (!query_point_recursive(node.first_child + i, point, callback)) {
                        return false;
                    }
                }
            }

            return true;
        }

        bool query_sphere_recursive(
            NodeIndex                                           node_idx,
            const Sphere&                                       sphere,
            const FunctionRef<bool(const T&, const Bounds3D&)>& callback) const
        {
            const auto& node = nodes[node_idx];
            if (!lyra::overlaps(sphere, node.bounds)) {
                return true;
            }

            for (const auto& entry : node.elements) {
                if (lyra::overlaps(sphere, entry.bounds)) {
                    if (!callback(entry.element, entry.bounds)) {
                        return false;
                    }
                }
            }

            if (!node.is_leaf()) {
                for (int i = 0; i < 8; ++i) {
                    if (!query_sphere_recursive(node.first_child + i, sphere, callback)) {
                        return false;
                    }
                }
            }

            return true;
        }

        bool query_ray_recursive(
            NodeIndex                                                  node_idx,
            const Ray3D&                                               ray,
            const FunctionRef<bool(const T&, const Bounds3D&, float)>& callback,
            float                                                      max_distance) const
        {
            const auto& node  = nodes[node_idx];
            float       t_min = 0.0f;
            float       t_max = 0.0f;
            if (!lyra::intersects(ray, node.bounds, t_min, t_max) || t_min > max_distance) {
                return true;
            }

            for (const auto& entry : node.elements) {
                float e_t_min = 0.0f;
                float e_t_max = 0.0f;
                if (lyra::intersects(ray, entry.bounds, e_t_min, e_t_max)) {
                    float hit_dist = (e_t_min >= 0.0f) ? e_t_min : 0.0f;
                    if (hit_dist <= max_distance) {
                        if (!callback(entry.element, entry.bounds, hit_dist)) {
                            return false;
                        }
                    }
                }
            }

            if (!node.is_leaf()) {
                for (int i = 0; i < 8; ++i) {
                    if (!query_ray_recursive(node.first_child + i, ray, callback, max_distance)) {
                        return false;
                    }
                }
            }

            return true;
        }

        bool collect_all_recursive(
            NodeIndex                                           node_idx,
            const FunctionRef<bool(const T&, const Bounds3D&)>& callback) const
        {
            const auto& node = nodes[node_idx];
            for (const auto& entry : node.elements) {
                if (!callback(entry.element, entry.bounds)) {
                    return false;
                }
            }

            if (!node.is_leaf()) {
                for (int i = 0; i < 8; ++i) {
                    if (!collect_all_recursive(node.first_child + i, callback)) {
                        return false;
                    }
                }
            }
            return true;
        }

        bool query_frustum_recursive(
            NodeIndex                                           node_idx,
            const Frustum&                                      frustum,
            const FunctionRef<bool(const T&, const Bounds3D&)>& callback) const
        {
            const auto&     node        = nodes[node_idx];
            ContainmentType containment = lyra::test_containment(frustum, node.bounds);

            if (containment == ContainmentType::DISJOINT) {
                return true;
            }

            // fast path: entire node is inside frustum, all sub-elements are guaranteed visible
            if (containment == ContainmentType::CONTAINS) {
                return collect_all_recursive(node_idx, callback);
            }

            // node intersects boundary: check individual elements
            for (const auto& entry : node.elements) {
                if (lyra::intersects(frustum, entry.bounds)) {
                    if (!callback(entry.element, entry.bounds)) {
                        return false;
                    }
                }
            }

            if (!node.is_leaf()) {
                for (int i = 0; i < 8; ++i) {
                    if (!query_frustum_recursive(node.first_child + i, frustum, callback)) {
                        return false;
                    }
                }
            }

            return true;
        }

        template <typename NodePredicate, typename ElementCallback>
        bool traverse_recursive(
            NodeIndex         node_idx,
            NodePredicate&&   node_filter,
            ElementCallback&& element_callback) const
        {
            const auto& node = nodes[node_idx];
            if (!node_filter(node.bounds)) {
                return true;
            }

            for (const auto& entry : node.elements) {
                if (!element_callback(entry.element, entry.bounds)) {
                    return false;
                }
            }

            if (!node.is_leaf()) {
                for (int i = 0; i < 8; ++i) {
                    if (!traverse_recursive(node.first_child + i, node_filter, element_callback)) {
                        return false;
                    }
                }
            }

            return true;
        }

    private:
        Bounds3D                    root_bounds;
        uint                        max_depth_limit = 8;
        uint                        node_capacity   = 16;
        size_t                      element_count   = 0;
        Vector<Node>                nodes;
        HashMap<T, ElementLocation> element_map;
    };

    using SceneOctTree = OctTree<Entity>;

} // namespace lyra

#endif // LYRA_ENGINE_SCENE_OCT_TREE_H
