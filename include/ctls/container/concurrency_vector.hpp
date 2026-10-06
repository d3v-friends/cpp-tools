#pragma once

#include <mutex>
#include <vector>

namespace ctls {
    template <class T> class concurrency_vector {
    public:
        explicit concurrency_vector() = default;
        concurrency_vector(const concurrency_vector&) = delete;
        concurrency_vector& operator=(const concurrency_vector&) = delete;
        concurrency_vector(concurrency_vector&&) = delete;
        concurrency_vector& operator=(concurrency_vector&&) = delete;
        ~concurrency_vector() = default;

        void reserve(std::size_t size) {
            std::scoped_lock lock(m_mutex);
            m_data.reserve(size);
        }

        void push_back(T value) {
            std::scoped_lock lock(m_mutex);
            m_data.push_back(std::move(value));
        }

        std::vector<T> get_data() const {
            std::scoped_lock lock(m_mutex);
            return m_data;
        }

    private:
        mutable std::mutex m_mutex;
        std::vector<T> m_data;
    };
} // namespace ctls
