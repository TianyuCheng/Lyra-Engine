#pragma once

#ifndef LYRA_LYRA_COMMON_HANDLE_H
#define LYRA_LYRA_COMMON_HANDLE_H

#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Conversion.h>

// for conversion conversion betwen slot_map_handle and our handles
#include <apus/slot_map.hpp>

namespace lyra
{

    template <typename E, E TYPE, typename THandle = uint32_t>
    struct TypedEnumHandle
    {
        static constexpr E type = TYPE;

        THandle value;

        TypedEnumHandle() : value(THandle(-1)) {}

        explicit TypedEnumHandle(THandle value) : value(value) {}

        FORCE_INLINE bool valid() const { return value != THandle(-1); }

        FORCE_INLINE void reset() { value = THandle(-1); }

        template <typename SlotType>
        FORCE_INLINE static TypedEnumHandle create(apus::slot_map_handle<SlotType> handle)
        {
            static_assert(sizeof(THandle) == sizeof(apus::slot_map_handle<SlotType>));
            return TypedEnumHandle(as_type<THandle>(handle));
        }

        template <typename SlotType>
        FORCE_INLINE apus::slot_map_handle<SlotType> to_slotmap_handle() const
        {
            static_assert(sizeof(THandle) == sizeof(apus::slot_map_handle<SlotType>));
            return as_type<apus::slot_map_handle<SlotType>>(value);
        }

        constexpr static auto type_name() -> CString
        {
            return to_string(TYPE);
        }
    };

    template <typename T, typename THandle = uint32_t>
    struct TypedIntegerHandle
    {
        THandle value = nullptr;

        TypedIntegerHandle() : value(THandle(-1)) {}

        explicit TypedIntegerHandle(THandle value) : value(value) {}

        FORCE_INLINE bool valid() const { return value != nullptr; }

        FORCE_INLINE void reset() { value = nullptr; }
    };

    template <typename T>
    struct TypedPointerHandle
    {
        void* pointer = nullptr;

        TypedPointerHandle() : pointer(nullptr) {}

        explicit TypedPointerHandle(void* pointer) : pointer(pointer) {}

        FORCE_INLINE bool valid() const { return pointer != nullptr; }

        FORCE_INLINE void reset() { pointer = nullptr; }

        template <typename U>
        FORCE_INLINE U* as_type() { return reinterpret_cast<U*>(pointer); }

        template <typename U>
        FORCE_INLINE U* as_type() const { return reinterpret_cast<U*>(pointer); }
    };

    template <typename E, E TYPE, typename THandle = uint32_t>
    FORCE_INLINE bool operator==(const TypedEnumHandle<E, TYPE, THandle>& lhs, const TypedEnumHandle<E, TYPE, THandle>& rhs)
    {
        return lhs.value == rhs.value;
    }

    template <typename E, E TYPE, typename THandle = uint32_t>
    FORCE_INLINE bool operator!=(const TypedEnumHandle<E, TYPE, THandle>& lhs, const TypedEnumHandle<E, TYPE, THandle>& rhs)
    {
        return lhs.value != rhs.value;
    }

    template <typename T, typename THandle = uint32_t>
    FORCE_INLINE bool operator==(const TypedIntegerHandle<T, THandle>& lhs, const TypedIntegerHandle<T, THandle>& rhs)
    {
        return lhs.value == rhs.value;
    }

    template <typename T, typename THandle = uint32_t>
    FORCE_INLINE bool operator!=(const TypedIntegerHandle<T, THandle>& lhs, const TypedIntegerHandle<T, THandle>& rhs)
    {
        return lhs.value != rhs.value;
    }

    template <typename T>
    FORCE_INLINE bool operator==(const TypedPointerHandle<T>& lhs, const TypedPointerHandle<T>& rhs)
    {
        return lhs.pointer == rhs.pointer;
    }

    template <typename T>
    FORCE_INLINE bool operator!=(const TypedPointerHandle<T>& lhs, const TypedPointerHandle<T>& rhs)
    {
        return lhs.pointer != rhs.pointer;
    }

} // namespace lyra

#endif // LYRA_LYRA_COMMON_HANDLE_H
