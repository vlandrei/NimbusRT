#pragma once
#include "Logger.hpp"
#include <chrono>

namespace VCT
{
    class ScopedProfiler
    {
    public:
        using Clock = std::chrono::high_resolution_clock;

        ScopedProfiler(const std::string_view& message, int line) : m_Start(Clock::now()), m_Message(message), m_Line(line) {}
        ~ScopedProfiler()
        {
            auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - m_Start).count();
            LOG("%s line: %d execution time: %f milliseconds", m_Message.data(), m_Line, static_cast<float>(delta) / 1000000.0f);
        }
    private:
        std::chrono::high_resolution_clock::time_point m_Start;
        std::string_view m_Message;
        int m_Line;
    };
}

#ifdef LOGGING_ENABLED
#define MERGE_NAME2(x, y) x ## y
#define MERGE_NAME(x, y) MERGE_NAME2(x, y)
#define PROFILE_SCOPE() VCT::ScopedProfiler MERGE_NAME(profiler, __LINE__) = VCT::ScopedProfiler(__func__, __LINE__)
#define PROFILE_SCOPE_MSG(Message) VCT::ScopedProfiler MERGE_NAME(profiler, __LINE__) = VCT::ScopedProfiler(__func__ " " Message, __LINE__)
#else
    #define PROFILE_SCOPE()
    #define PROFILE_SCOPE_MSG(Message)
#endif