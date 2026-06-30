#pragma once

/**
 * @file arena.hpp
 * @brief Arena allocator for efficient memory management in the Precious compiler.
 *
 * Arena allocators allocate memory from a pre-allocated block (the "arena") and
 * release all memory at once when the arena is destroyed. This avoids the overhead
 * of individual malloc/free calls and is ideal for compiler internals where many
 * small allocations are made during a single compilation phase.
 *
 * Key characteristics:
 * - No individual deallocation (bulk free on destruction)
 * - Pointer stability: allocated objects remain valid until arena destruction
 * - O(1) allocation time
 * - Suitable for AST nodes and parser internals
 */

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <utility>

/**
 * @class ArenaAllocator
 * @brief A simple arena allocator that manages a contiguous block of memory.
 *
 * The allocator maintains a linear buffer and advances an offset pointer on each
 * allocation. It handles alignment automatically via std::align. Once the arena
 * is destroyed, all allocated memory is released in a single deallocation.
 *
 * Usage:
 * @code
 *   ArenaAllocator arena(1024 * 1024); // 1 MB arena
 *   NodeTerm* term = arena.emplace<NodeTermIntLit>(token);
 * @endcode
 */
class ArenaAllocator final {
    public:
        /**
         * @brief Constructs an arena allocator with the specified maximum size.
         * @param max_num_bytes Maximum number of bytes the arena can allocate.
         *
         * Allocates a contiguous buffer of the requested size. The arena will
         * throw std::bad_alloc if an allocation exceeds the remaining capacity.
         */
        explicit ArenaAllocator(const std::size_t max_num_bytes)
            : m_size { max_num_bytes }
            , m_buffer { new std::byte[max_num_bytes] }
            , m_offset { m_buffer }
        {
        }

        ArenaAllocator(const ArenaAllocator&) = delete;
        ArenaAllocator& operator=(const ArenaAllocator&) = delete;

        /**
         * @brief Move constructor. Transfers ownership of the arena buffer.
         * @param other The arena to move from. Left in a valid empty state.
         */
        ArenaAllocator(ArenaAllocator&& other) noexcept
            : m_size { std::exchange(other.m_size, 0) }
            , m_buffer { std::exchange(other.m_buffer, nullptr) }
            , m_offset { std::exchange(other.m_offset, nullptr) }
        {
        }

        /**
         * @brief Move assignment operator. Swaps state with another arena.
         * @param other The arena to swap with.
         * @return Reference to this arena.
         */
        ArenaAllocator& operator=(ArenaAllocator&& other) noexcept
        {
            std::swap(m_size, other.m_size);
            std::swap(m_buffer, other.m_buffer);
            std::swap(m_offset, other.m_offset);
            return *this;
        }

        /**
         * @brief Allocates raw, uninitialized memory for a single object of type T.
         * @tparam T The type to allocate memory for.
         * @return Pointer to the allocated memory, properly aligned for T.
         * @throws std::bad_alloc If there is insufficient space in the arena.
         *
         * Uses std::align to ensure proper alignment for the requested type.
         * The returned memory is NOT initialized — use emplace() for constructed objects.
         */
        template <typename T>
        [[nodiscard]] T* alloc()
        {
            std::size_t remaining_num_bytes = m_size - static_cast<std::size_t>(m_offset - m_buffer);
            auto pointer = static_cast<void*>(m_offset);
            const auto aligned_address = std::align(alignof(T), sizeof(T), pointer, remaining_num_bytes);
            if (aligned_address == nullptr) {
                throw std::bad_alloc {};
            }
            m_offset = static_cast<std::byte*>(aligned_address) + sizeof(T);
            return static_cast<T*>(aligned_address);
        }

        /**
         * @brief Allocates memory and constructs an object of type T in-place.
         * @tparam T The type to construct.
         * @tparam Args Constructor argument types.
         * @param args Arguments forwarded to T's constructor.
         * @return Pointer to the constructed object.
         * @throws std::bad_alloc If there is insufficient space in the arena.
         *
         * This is the preferred way to create objects in the arena. It combines
         * allocation (alloc<T>()) with construction via placement new.
         */
        template <typename T, typename... Args>
        [[nodiscard]] T* emplace(Args&&... args)
        {
            const auto allocated_memory = alloc<T>();
            return new (allocated_memory) T { std::forward<Args>(args)... };
        }

        /**
         * @brief Destructor. Releases the arena's memory buffer.
         *
         * All objects allocated by this arena become invalid after destruction.
         * This does NOT call destructors on allocated objects — ensure any
         * necessary cleanup is done before arena destruction.
         */
        ~ArenaAllocator()
        {
            delete[] m_buffer;
        }

    private:
    std::size_t m_size;     ///< Total capacity of the arena buffer in bytes.
    std::byte* m_buffer;    ///< Pointer to the start of the allocated memory buffer.
    std::byte* m_offset;    ///< Current write position (next free byte) in the buffer.
};