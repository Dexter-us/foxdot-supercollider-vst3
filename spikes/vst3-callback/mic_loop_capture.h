#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dexter {

enum class MicLoopState : std::uint8_t {
    Empty,
    Recording,
    Ready,
    Playing
};

// Fixed-capacity stereo looper. The audio thread is the only thread that
// touches sample storage; UI actions are queued atomically and the UI only
// reads published state/peak values.
class MicLoopCapture {
public:
    static constexpr std::size_t kMaxSeconds = 8;
    static constexpr std::size_t kMaxSampleRate = 192000;
    static constexpr std::size_t kMaxFrames = kMaxSeconds * kMaxSampleRate;

    MicLoopCapture() { prepare(48000.0); }

    // Call only while the processor is stopped; this sizes storage once for
    // the host sample rate, never from the realtime callback.
    void prepare(double sampleRate) {
        if (!std::isfinite(sampleRate))
            sampleRate = 48000.0;
        const auto safeRate = static_cast<std::size_t>(
            std::clamp(sampleRate, 8000.0,
                       static_cast<double>(kMaxSampleRate)));
        capacityFrames_ = std::min(kMaxFrames, kMaxSeconds * safeRate);
        samples_.assign(capacityFrames_ * 2, 0.0f);
        writeFrame_ = 0;
        playFrame_ = 0;
        publishedFrames_.store(0, std::memory_order_release);
        state_.store(MicLoopState::Empty, std::memory_order_release);
    }

    void requestRecordToggle() noexcept {
        command_.store(Command::ToggleRecord, std::memory_order_release);
    }

    void requestPlay() noexcept {
        command_.store(Command::Play, std::memory_order_release);
    }

    void requestStop() noexcept {
        command_.store(Command::Stop, std::memory_order_release);
    }

    MicLoopState state() const noexcept {
        return state_.load(std::memory_order_acquire);
    }

    float inputPeak() const noexcept {
        return inputPeak_.load(std::memory_order_acquire);
    }

    std::size_t recordedFrames() const noexcept {
        return publishedFrames_.load(std::memory_order_acquire);
    }

    // Called once per output frame from the VST audio callback. Missing host
    // inputs are silence. Recording is opt-in and never passes the live mic
    // through; only a completed loop is heard when playback is requested.
    void processFrame(float inputLeft, float inputRight,
                      float& outputLeft, float& outputRight) noexcept {
        const auto command = command_.exchange(Command::None,
                                               std::memory_order_acq_rel);
        if (command == Command::ToggleRecord) {
            if (state_.load(std::memory_order_relaxed) ==
                MicLoopState::Recording) {
                finishRecording();
            } else {
                startRecording();
            }
        } else if (command == Command::Play) {
            if (writeFrame_ > 0) {
                playFrame_ = 0;
                state_.store(MicLoopState::Playing,
                             std::memory_order_release);
            }
        } else if (command == Command::Stop) {
            if (state_.load(std::memory_order_relaxed) ==
                MicLoopState::Recording) {
                finishRecording();
            } else {
                state_.store(writeFrame_ > 0 ? MicLoopState::Ready
                                             : MicLoopState::Empty,
                             std::memory_order_release);
            }
        }

        outputLeft = 0.0f;
        outputRight = 0.0f;
        const auto current = state_.load(std::memory_order_relaxed);
        if (current == MicLoopState::Recording) {
            if (writeFrame_ < capacityFrames_) {
                const auto offset = writeFrame_ * 2;
                const float left = finiteOrZero(inputLeft);
                const float right = finiteOrZero(inputRight);
                samples_[offset] = left;
                samples_[offset + 1] = right;
                blockPeak_ = std::max(blockPeak_,
                                      std::max(std::abs(left), std::abs(right)));
                ++writeFrame_;
                publishedFrames_.store(writeFrame_,
                                       std::memory_order_release);
            }
            if (writeFrame_ >= capacityFrames_)
                finishRecording();
            return;
        }

        if (current == MicLoopState::Playing && writeFrame_ > 0) {
            const auto offset = playFrame_ * 2;
            outputLeft = samples_[offset];
            outputRight = samples_[offset + 1];
            playFrame_ = (playFrame_ + 1) % writeFrame_;
        }
    }

    // Publish one peak value per host block so the UI can tell whether the
    // selected input is actually routed, without reading audio buffers.
    void finishBlock() noexcept {
        inputPeak_.store(blockPeak_, std::memory_order_release);
        blockPeak_ = 0.0f;
    }

private:
    enum class Command : std::uint8_t {
        None,
        ToggleRecord,
        Play,
        Stop
    };

    static float finiteOrZero(float value) noexcept {
        return std::isfinite(value) ? value : 0.0f;
    }

    void startRecording() noexcept {
        writeFrame_ = 0;
        playFrame_ = 0;
        publishedFrames_.store(0, std::memory_order_release);
        state_.store(MicLoopState::Recording, std::memory_order_release);
    }

    void finishRecording() noexcept {
        state_.store(writeFrame_ > 0 ? MicLoopState::Ready
                                     : MicLoopState::Empty,
                     std::memory_order_release);
    }

    std::vector<float> samples_;
    std::atomic<Command> command_{Command::None};
    std::atomic<MicLoopState> state_{MicLoopState::Empty};
    std::atomic<std::size_t> publishedFrames_{0};
    std::atomic<float> inputPeak_{0.0f};
    std::size_t capacityFrames_ = 0;
    std::size_t writeFrame_ = 0;
    std::size_t playFrame_ = 0;
    float blockPeak_ = 0.0f;
};

} // namespace dexter
