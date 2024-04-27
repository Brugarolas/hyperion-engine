/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_UTIL_LOGGER_HPP
#define HYPERION_UTIL_LOGGER_HPP

#include <core/Name.hpp>

#include <core/system/Debug.hpp>

namespace hyperion {

struct LogChannel
{
    uint32  id;
    Name    name;

    LogChannel(Name name);
};

class Logger
{
public:
    Logger();
    Logger(Name context_name);
    Logger(const Logger &other)                 = default;
    Logger &operator=(const Logger &other)      = default;
    Logger(Logger &&other) noexcept             = default;
    Logger &operator=(Logger &&other) noexcept  = default;
    ~Logger()                                   = default;

    HYP_FORCE_INLINE
    uint64 GetLogMask() const
        { return m_log_mask; }

    HYP_FORCE_INLINE
    void SetLogMask(uint64 mask)
        { m_log_mask = mask; }

    HYP_FORCE_INLINE
    bool IsEnabled(uint32 channel_id) const
        { return (m_log_mask & (1ull << uint64(channel_id))) != 0; }

    template <class ... Args>
    void Log(LogChannel channel, const char *format, Args &&...args) const
    {
        if (!IsEnabled(channel.id)) {
            return;
        }

        std::printf("%s\n", format, std::forward<Args>(args)...);
    }

private:
    Name    m_context_name;
    uint64  m_log_mask;
};

} // namespace hyperion

#endif