#include <mutex>
#include <queue>
#include <condition_variable>
#include <optional>

#ifndef FLASHLIGHT_CONCURRENT_QUEUE_H
#define FLASHLIGHT_CONCURRENT_QUEUE_H

template<typename T>
class ConcurrentQueue final {
public:
    ConcurrentQueue() = default;

    ConcurrentQueue(const ConcurrentQueue &) = delete;

    ConcurrentQueue &operator=(const ConcurrentQueue &) = delete;

    void push(T &&value) {
        {
            std::unique_lock lock(mMutex);
            mQueue.push(value);
            mIsInterrupted = false;
        }
        mCV.notify_one();
    }

    std::optional<T> waitAndPop() {
        std::unique_lock lock(mMutex);
        mCV.wait(lock, [this] {
            return !mQueue.empty() || mIsInterrupted;
        });
        std::optional<T> result;
        if (!mQueue.empty()) {
            result = mQueue.front();
            mQueue.pop();
        }
        return result;
    }

    bool isEmpty() const {
        std::lock_guard lock(mMutex);
        return mQueue.empty();
    }

    void interruptWaiting() {
        {
            std::unique_lock lock(mMutex);
            if (mIsInterrupted) {
                return;
            }
            mIsInterrupted = true;
        }
        mCV.notify_one();
    }

private:
    std::queue<T> mQueue;
    mutable std::mutex mMutex;
    std::condition_variable mCV;
    bool mIsInterrupted = false;
};

#endif //FLASHLIGHT_CONCURRENT_QUEUE_H
