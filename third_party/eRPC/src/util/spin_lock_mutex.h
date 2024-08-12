#pragma once
#include <atomic>
namespace erpc
{
    class spin_lock_mutex final
    {
    private:
        std::atomic_flag flag = ATOMIC_FLAG_INIT;

    public:
        template <typename WhileIdleFunc = void()>
        void lock(WhileIdleFunc idle_work = [] {})
        {
            while (flag.test_and_set(std::memory_order_acquire))
            {
                idle_work();
            }
        }

        void unlock()
        {
            flag.clear(std::memory_order_release);
        }

        spin_lock_mutex() = default;
        spin_lock_mutex(const spin_lock_mutex &) = delete;
        spin_lock_mutex &operator=(const spin_lock_mutex &) = delete;
    };
}
