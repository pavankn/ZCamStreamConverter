#pragma once
#include <chrono>

template<typename T>
class VFrameQueue
{
public:

    using CallbackFunc = std::function<void(T*)>;

    struct Frame
    {
        T data;
        uint64_t time;
        bool noDrop;
    };

    VFrameQueue() : running(false), maxTime(0) {}

    void start()
    {
        running = true;
        worker = std::thread(&VFrameQueue::run, this);
    }

    void stop()
    {
        running = false;
        cv.notify_all();

        if (worker.joinable())
            worker.join();
    }

    void setFrameCallback(CallbackFunc cb)
    {
        callback = std::move(cb);
    }

    void setFrameTime(uint64_t time_us)
    {
        maxTime = time_us;
    }

    void enqueue(T data, uint64_t time_us, bool noDrop)
    {
        {
            std::lock_guard<std::mutex> lock(queueLock);
            if (frameQueue.size() > 120)
            {
                frameQueue.pop(); // drop oldest
            }
            frameQueue.push({ data, time_us, noDrop });
        }
        cv.notify_one();
    }

private:

    static uint64_t os_gettime_ns()
    {
        auto now = std::chrono::steady_clock::now();

        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()
        ).count();
    }


    void run()
    {
        Frame current;

        uint64_t lastFrameTime = 0;
        uint64_t lastStartTime = 0;
        uint64_t processingTime = 0;

        while (running)
        {
            {
                std::unique_lock<std::mutex> lock(queueLock);

                cv.wait(lock, [&] {
                    return !frameQueue.empty() || !running;
                    });

                if (!running)
                    break;

                current = frameQueue.front();
                frameQueue.pop();
            }

            if (current.time < lastFrameTime)
                continue;

            if (current.noDrop)
            {
                lastStartTime = os_gettime_ns() / 1000;

                if (callback)
                    callback(&current.data);

                lastFrameTime = current.time;

                processingTime =
                    os_gettime_ns() / 1000 - lastStartTime;
            }
            else if (current.time - lastFrameTime + 15000 >
                processingTime)
            {
                lastStartTime = os_gettime_ns() / 1000;

                if (callback)
                    callback(&current.data);

                lastFrameTime = current.time;

                processingTime =
                    os_gettime_ns() / 1000 - lastStartTime;
            }
        }
    }

private:

    std::queue<Frame> frameQueue;
    std::mutex queueLock;
    std::condition_variable cv;

    std::thread worker;
    std::atomic<bool> running;

    CallbackFunc callback;

    uint64_t maxTime;
};