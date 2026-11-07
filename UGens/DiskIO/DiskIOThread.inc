// SPDX-License-Identifier: GPL-2.0-or-later
// Replaces only the upstream worker/queue, not the three UGens' DSP routines.
// SC's stock scsynth queue and wake counter assume one audio producer. A VST
// process can contain several independent Worlds/audio producers.
struct DiskIOThread {
    boost::lockfree::queue<DiskIOMsg, boost::lockfree::capacity<1024>> mDiskFifo;
    std::atomic<bool> mRunning{true};
    std::atomic<unsigned> mPending{0};
    std::atomic<std::uint64_t> mDrops{0};
    std::mutex mWakeMutex;
    std::condition_variable mWake;
    std::thread mThread;

    void launchThread() { mThread = std::thread(&DiskIOThread::ioThreadFunc, this); }
    ~DiskIOThread() {
        mRunning.store(false, std::memory_order_release);
        mWake.notify_one();
        if (mThread.joinable()) mThread.join();
    }
    bool Write(DiskIOMsg& message) {
        mPending.fetch_add(1, std::memory_order_release);
        if (!mDiskFifo.bounded_push(message)) {
            mPending.fetch_sub(1, std::memory_order_release);
            mDrops.fetch_add(1, std::memory_order_relaxed);
            return false; // never allocate, wait, or do file I/O on the caller
        }
        mWake.notify_one();
        return true;
    }
    void ioThreadFunc() {
        for (;;) {
            DiskIOMsg message;
            while (mDiskFifo.pop(message)) {
                message.Perform();
                mPending.fetch_sub(1, std::memory_order_release);
            }
            if (!mRunning.load(std::memory_order_acquire) &&
                mPending.load(std::memory_order_acquire) == 0) return;
            // Timed wait covers a notify racing the transition into wait.
            std::unique_lock<std::mutex> lock(mWakeMutex);
            mWake.wait_for(lock, std::chrono::milliseconds(2));
        }
    }
};
