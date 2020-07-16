#pragma once

#include <chrono>

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Milliseconds = std::chrono::milliseconds;
