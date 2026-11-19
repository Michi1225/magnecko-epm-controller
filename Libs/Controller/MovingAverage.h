#pragma once
#include <array>
#include <cstddef>

template <std::size_t Capacity> class MovingAverage {
    static_assert(Capacity > 0, "Moving average needs storage");
    std::array<float, Capacity> samples_{};
    std::size_t next_ = 0, count_ = 0;
public:
    float add(float sample) {
        samples_[next_] = sample;
        next_ = (next_ + 1) % Capacity;
        if (count_ < Capacity) ++count_;
        float sum = 0;
        for (std::size_t i = 0; i < count_; ++i) sum += samples_[i];
        return sum / static_cast<float>(count_);
    }
};
