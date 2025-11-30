// Reference: https://github.com/skaarj1989/
#pragma once

#ifndef LYRA_LIBRARY_COMMON_BLAKBOARD_H
#define LYRA_LIBRARY_COMMON_BLAKBOARD_H

#include <any>
#include <cassert>
#include <typeindex>
#include <unordered_map>

namespace lyra::detail
{

    struct Blackboard
    {
    public:
        Blackboard()                      = default;
        Blackboard(const Blackboard&)     = default;
        Blackboard(Blackboard&&) noexcept = default;
        ~Blackboard()                     = default;

        Blackboard& operator=(const Blackboard&)     = default;
        Blackboard& operator=(Blackboard&&) noexcept = default;

        template <typename T, typename... Args>
        T& add(Args&&... args);

        template <typename T>
        [[nodiscard]] const T& get() const;
        template <typename T>
        [[nodiscard]] const T* try_get() const;

        template <typename T>
        [[nodiscard]] T& get();
        template <typename T>
        [[nodiscard]] T* try_get();

        template <typename T>
        [[nodiscard]] bool has() const;

    private:
        std::unordered_map<std::type_index, std::any> m_storage;
    };

    template <typename T, typename... Args>
    inline T& Blackboard::add(Args&&... args)
    {
        assert(!has<T>());
        return m_storage[typeid(T)].emplace<T>(T{std::forward<Args>(args)...});
    }

    template <typename T>
    const T& Blackboard::get() const
    {
        assert(has<T>());
        return std::any_cast<const T&>(m_storage.at(typeid(T)));
    }
    template <typename T>
    const T* Blackboard::try_get() const
    {
        auto it = m_storage.find(typeid(T));
        return it != m_storage.cend() ? std::any_cast<const T>(&it->second) : nullptr;
    }

    template <typename T>
    inline T& Blackboard::get()
    {
        return const_cast<T&>(
            const_cast<const Blackboard*>(this)->get<T>());
    }
    template <typename T>
    inline T* Blackboard::try_get()
    {
        return const_cast<T*>(
            const_cast<const Blackboard*>(this)->try_get<T>());
    }

    template <typename T>
    inline bool Blackboard::has() const
    {
#if __cplusplus >= 202002L
        return m_storage.contains(typeid(T));
#else
        return m_storage.find(typeid(T)) != m_storage.cend();
#endif
    }

} // namespace lyra::detail
#endif // LYRA_LIBRARY_COMMON_BLAKBOARD_H
