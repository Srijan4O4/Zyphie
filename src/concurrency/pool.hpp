#pragma once
#include "zyphie/error.hpp"
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace zyphie {
// The coordinator admits at most capacity futures, including completed results.
// Workers never touch archive files, callbacks, or caller-owned buffers.
class Pool {
public:
    explicit Pool(unsigned count) {
        try {
            for (unsigned i=0;i<count;++i) workers_.emplace_back([this](std::stop_token stop) {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mutex_);
                        if (!ready_.wait(lock,stop,[&]{return !tasks_.empty();})) return;
                        if (stop.stop_requested()) return;
                        task=std::move(tasks_.front()); tasks_.pop_front();
                    }
                    task(); // packaged_task transports exceptions to the coordinator.
                }
            });
        } catch (...) { shutdown(); throw; }
    }
    ~Pool() { shutdown(); }
    Pool(const Pool&)=delete;
    template<class F> auto submit(F&& function) {
        using Result=std::invoke_result_t<F>;
        auto task=std::make_shared<std::packaged_task<Result()>>(std::forward<F>(function));
        auto future=task->get_future();
        { std::lock_guard lock(mutex_); tasks_.emplace_back([task]{(*task)();}); }
        ready_.notify_one(); return future;
    }
private:
    void shutdown() noexcept {
        for(auto& worker:workers_) worker.request_stop();
        ready_.notify_all(); workers_.clear();
        tasks_.clear();
    }
    std::mutex mutex_;
    std::condition_variable_any ready_;
    std::deque<std::function<void()>> tasks_;
    std::vector<std::jthread> workers_;
};
}
