#pragma once

#include <atomic>

class spinlock_mutex final {
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
    template<typename WhileIdleFunc = void()>
    void lock(WhileIdleFunc idle_work = [] {}) {
        while (flag.test_and_set(std::memory_order_acquire)) {
            idle_work();
        }
    }
    bool try_lock() {
        return !flag.test_and_set(std::memory_order_acquire);
    }
    void unlock() {
        flag.clear(std::memory_order_release);
    }

    spinlock_mutex() = default;

    spinlock_mutex(const spinlock_mutex &) = delete;

    spinlock_mutex &operator=(const spinlock_mutex &) = delete;
};

class spinlock_rw_mutex {
private:
    std::atomic<int> readers_count{ 0 };
    std::atomic<bool> writer_lock{ false };

public:
    void lock_read() {
        while (true) {
            // Spin until there are no writers
            while (writer_lock) {}

            // Increment the readers count
            readers_count.fetch_add(1, std::memory_order_acquire);

            // Check if a writer has acquired the lock
            if (writer_lock) {
                readers_count.fetch_sub(1, std::memory_order_release);
            } else {
                break;  // Lock acquired successfully for reading
            }
        }
    }

    bool try_lock_read() {
        // Try to acquire the lock for reading
        if (!writer_lock) {
            readers_count.fetch_add(1, std::memory_order_acquire);
            return true;
        }
        return false;
    }

    void unlock_read() {
        readers_count.fetch_sub(1, std::memory_order_release);
    }

    void lock_write() {
        while (true) {
            // Spin until there are no readers or writers
            while (readers_count != 0 || writer_lock) {}

            // Try to acquire the lock for writing
            bool expected = false;
            if (writer_lock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                break;  // Lock acquired successfully for writing
            }
        }
    }

    bool try_lock_write() {
        // Try to acquire the lock for writing
        bool expected = false;
        return writer_lock.compare_exchange_strong(expected, true, std::memory_order_acquire);
    }

    void unlock_write() {
        writer_lock.store(false, std::memory_order_release);
    }
};