#pragma once

#ifndef LYRA_ENGINE_UTILITIES_DETAIL_TOOLBOARD_H
#define LYRA_ENGINE_UTILITIES_DETAIL_TOOLBOARD_H

#include <cassert>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <absl/container/flat_hash_map.h>

namespace lyra::detail
{
    /**
     * @brief A type-indexed registry for borrowing external subsystems, devices, and tools.
     *
     * Unlike Blackboard (which owns dynamic value types via type erasure), Toolboard holds
     * non-owning pointers to external hardware and engine infrastructure (e.g. Window,
     * GPUDevice, Compiler).
     */
    struct Toolboard
    {
    private:
        template <typename T>
        using clean_element_t = std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;

        template <typename T>
        using get_return_t = std::conditional_t<std::is_pointer_v<T>, clean_element_t<T>*, clean_element_t<T>&>;

        template <typename T>
        using const_get_return_t = std::conditional_t<std::is_pointer_v<T>, const clean_element_t<T>*, const clean_element_t<T>&>;

    public:
        Toolboard()                      = default;
        Toolboard(const Toolboard&)     = default;
        Toolboard(Toolboard&&) noexcept = default;
        ~Toolboard()                     = default;

        Toolboard& operator=(const Toolboard&)     = default;
        Toolboard& operator=(Toolboard&&) noexcept = default;

        /**
         * @brief Register a pointer to an external tool/subsystem.
         */
        template <typename T = void, typename U>
        clean_element_t<std::conditional_t<std::is_void_v<T>, U, T>>* add(U* instance)
        {
            using Element = clean_element_t<std::conditional_t<std::is_void_v<T>, U, T>>;
            assert(instance != nullptr);
            assert(!has<Element>());
            m_storage[typeid(Element)] = static_cast<void*>(static_cast<Element*>(instance));
            return static_cast<Element*>(instance);
        }

        /**
         * @brief Register a reference to an external tool/subsystem.
         */
        template <typename T = void, typename U>
        clean_element_t<std::conditional_t<std::is_void_v<T>, U, T>>& add(U& instance)
        {
            using Element = clean_element_t<std::conditional_t<std::is_void_v<T>, U, T>>;
            assert(!has<Element>());
            m_storage[typeid(Element)] = static_cast<void*>(static_cast<Element*>(std::addressof(instance)));
            return static_cast<Element&>(instance);
        }

        /**
         * @brief Retrieve a reference or pointer to a registered tool (asserts presence).
         */
        template <typename T>
        [[nodiscard]] const_get_return_t<T> get() const
        {
            using Element = clean_element_t<T>;
            assert(has<Element>());
            if constexpr (std::is_pointer_v<T>)
            {
                return static_cast<const Element*>(m_storage.at(typeid(Element)));
            }
            else
            {
                return *static_cast<const Element*>(m_storage.at(typeid(Element)));
            }
        }

        template <typename T>
        [[nodiscard]] get_return_t<T> get()
        {
            using Element = clean_element_t<T>;
            assert(has<Element>());
            if constexpr (std::is_pointer_v<T>)
            {
                return static_cast<Element*>(m_storage.at(typeid(Element)));
            }
            else
            {
                return *static_cast<Element*>(m_storage.at(typeid(Element)));
            }
        }

        /**
         * @brief Query a tool by type. Returns nullptr if not registered. Never returns a double pointer.
         */
        template <typename T>
        [[nodiscard]] const clean_element_t<T>* try_get() const
        {
            using Element = clean_element_t<T>;
            auto it = m_storage.find(typeid(Element));
            return it != m_storage.cend() ? static_cast<const Element*>(it->second) : nullptr;
        }

        template <typename T>
        [[nodiscard]] clean_element_t<T>* try_get()
        {
            using Element = clean_element_t<T>;
            auto it = m_storage.find(typeid(Element));
            return it != m_storage.cend() ? static_cast<Element*>(it->second) : nullptr;
        }

        /**
         * @brief Check whether a tool is registered.
         */
        template <typename T>
        [[nodiscard]] bool has() const
        {
            using Element = clean_element_t<T>;
#if __cplusplus >= 202002L
            return m_storage.contains(typeid(Element));
#else
            return m_storage.find(typeid(Element)) != m_storage.cend();
#endif
        }

        /**
         * @brief Remove a tool by type.
         */
        template <typename T>
        bool remove()
        {
            using Element = clean_element_t<T>;
            return m_storage.erase(typeid(Element)) > 0;
        }

        /**
         * @brief Clear all registered tools.
         */
        void clear() noexcept
        {
            m_storage.clear();
        }

        /**
         * @brief Number of registered tools.
         */
        [[nodiscard]] std::size_t size() const noexcept
        {
            return m_storage.size();
        }

        /**
         * @brief Whether the toolboard has any tools registered.
         */
        [[nodiscard]] bool empty() const noexcept
        {
            return m_storage.empty();
        }

    private:
        absl::flat_hash_map<std::type_index, void*> m_storage;
    };

} // namespace lyra::detail

#endif // LYRA_ENGINE_UTILITIES_DETAIL_TOOLBOARD_H
