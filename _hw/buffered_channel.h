#pragma once

#include <optional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

#include <optional>

template <class T>
class BufferedChannel {
public:
    explicit BufferedChannel(int size) : capacity_(size) {}

    void Send(const T& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_send_.wait(lock, [this] {
            return closed_ || static_cast<int>(queue_.size()) < capacity_;
        });
        if (closed_) {
            throw std::runtime_error("Send on closed channel");
        }
        queue_.push(value);
        cv_recv_.notify_one();
    }

    std::optional<T> Recv() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_recv_.wait(lock, [this] {
            return !queue_.empty() || closed_;
        });
        if (!queue_.empty()) {
            T value = std::move(queue_.front());
            queue_.pop();
            cv_send_.notify_one();
            return value;
        }
        return std::nullopt;
    }

    void Close() {
        std::unique_lock<std::mutex> lock(mtx_);
        closed_ = true;
        cv_send_.notify_all();
        cv_recv_.notify_all();
    }

    private:
        int capacity_;
        std::queue<T> queue_;
        std::mutex mtx_;
        std::condition_variable cv_send_;
        std::condition_variable cv_recv_;
        bool closed_ = false;
};
