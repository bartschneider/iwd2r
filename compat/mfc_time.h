// iwd2r. Distributed under LICENSE.md.
#pragma once

#include <ctime>

class CTimeSpan {
public:
    CTimeSpan()
        : m_time(0)
    {
    }
    explicit CTimeSpan(time_t seconds)
        : m_time(seconds)
    {
    }

    long long GetTotalSeconds() const { return m_time; }
    long long GetDays() const { return m_time / 86400; }
    int GetHours() const { return static_cast<int>((m_time % 86400) / 3600); }
    int GetMinutes() const { return static_cast<int>((m_time % 3600) / 60); }
    int GetSeconds() const { return static_cast<int>(m_time % 60); }

private:
    time_t m_time;
};

class CTime {
public:
    CTime()
        : m_time(0)
    {
    }
    CTime(time_t time)
        : m_time(time)
    {
    }

    static CTime GetCurrentTime() { return CTime(std::time(nullptr)); }
    // ATL's CTime::GetTickCount, which the resource cache calls: the current
    // time, the same as GetCurrentTime.
    static CTime GetTickCount() { return GetCurrentTime(); }

    time_t GetTime() const { return m_time; }
    int GetYear() const;
    int GetMonth() const;
    int GetDay() const;
    int GetHour() const;
    int GetMinute() const;
    int GetSecond() const;
    // 1 = Sunday.
    int GetDayOfWeek() const;

    CTimeSpan operator-(const CTime& other) const { return CTimeSpan(m_time - other.m_time); }
    bool operator==(const CTime& other) const { return m_time == other.m_time; }
    bool operator!=(const CTime& other) const { return m_time != other.m_time; }
    bool operator<(const CTime& other) const { return m_time < other.m_time; }
    bool operator>(const CTime& other) const { return m_time > other.m_time; }
    bool operator<=(const CTime& other) const { return m_time <= other.m_time; }
    bool operator>=(const CTime& other) const { return m_time >= other.m_time; }

private:
    time_t m_time;
};
