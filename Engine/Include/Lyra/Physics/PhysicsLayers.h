#pragma once

#ifndef LYRA_ENGINE_PHYSICS_PHYSICSLAYERS_H
#define LYRA_ENGINE_PHYSICS_PHYSICSLAYERS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Physics/PhysicsEnums.h>

namespace lyra
{
    /**
     * @brief 32-channel collision and trigger interaction matrix.
     */
    class CollisionMatrix
    {
    public:
        static constexpr uint LAYER_COUNT = 32;

        CollisionMatrix()
        {
            // default: all layers collide with all layers except triggers which generate trigger events
            m_collision_mask.fill(0xFFFFFFFF);
            m_trigger_mask.fill(0x00000000);

            // trigger layer (index 2) does not collide physically, but triggers against Moving and Character
            uint trigger_idx = static_cast<uint>(CollisionLayer::Trigger);
            uint moving_bit  = 1u << static_cast<uint>(CollisionLayer::Moving);
            uint char_bit    = 1u << static_cast<uint>(CollisionLayer::Character);

            m_collision_mask[trigger_idx] = 0;
            for (uint i = 0; i < LAYER_COUNT; ++i) {
                m_collision_mask[i] &= ~(1u << trigger_idx);
            }

            m_trigger_mask[trigger_idx] = moving_bit | char_bit;
            m_trigger_mask[static_cast<uint>(CollisionLayer::Moving)] |= (1u << trigger_idx);
            m_trigger_mask[static_cast<uint>(CollisionLayer::Character)] |= (1u << trigger_idx);
        }

        void set_collision(ushort layer_a, ushort layer_b, bool enabled)
        {
            if (layer_a >= LAYER_COUNT || layer_b >= LAYER_COUNT) return;
            if (enabled) {
                m_collision_mask[layer_a] |= (1u << layer_b);
                m_collision_mask[layer_b] |= (1u << layer_a);
            } else {
                m_collision_mask[layer_a] &= ~(1u << layer_b);
                m_collision_mask[layer_b] &= ~(1u << layer_a);
            }
        }

        bool can_collide(ushort layer_a, ushort layer_b) const
        {
            if (layer_a >= LAYER_COUNT || layer_b >= LAYER_COUNT) return false;
            return (m_collision_mask[layer_a] & (1u << layer_b)) != 0;
        }

        void set_trigger(ushort layer_a, ushort layer_b, bool enabled)
        {
            if (layer_a >= LAYER_COUNT || layer_b >= LAYER_COUNT) return;
            if (enabled) {
                m_trigger_mask[layer_a] |= (1u << layer_b);
                m_trigger_mask[layer_b] |= (1u << layer_a);
            } else {
                m_trigger_mask[layer_a] &= ~(1u << layer_b);
                m_trigger_mask[layer_b] &= ~(1u << layer_a);
            }
        }

        bool can_trigger(ushort layer_a, ushort layer_b) const
        {
            if (layer_a >= LAYER_COUNT || layer_b >= LAYER_COUNT) return false;
            return (m_trigger_mask[layer_a] & (1u << layer_b)) != 0;
        }

        auto get_collision_mask(ushort layer) const -> uint
        {
            return (layer < LAYER_COUNT) ? m_collision_mask[layer] : 0u;
        }

        auto get_trigger_mask(ushort layer) const -> uint
        {
            return (layer < LAYER_COUNT) ? m_trigger_mask[layer] : 0u;
        }

    private:
        Array<uint, LAYER_COUNT> m_collision_mask;
        Array<uint, LAYER_COUNT> m_trigger_mask;
    };

} // namespace lyra

#endif // LYRA_ENGINE_PHYSICS_PHYSICSLAYERS_H
