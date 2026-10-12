// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "compat/mfc_time.h"

namespace {

tm localBreakdown(time_t t)
{
    tm result = {};
#ifdef _WIN32
    localtime_s(&result, &t);
#else
    localtime_r(&t, &result);
#endif
    return result;
}

}

int CTime::GetYear() const { return localBreakdown(m_time).tm_year + 1900; }
int CTime::GetMonth() const { return localBreakdown(m_time).tm_mon + 1; }
int CTime::GetDay() const { return localBreakdown(m_time).tm_mday; }
int CTime::GetHour() const { return localBreakdown(m_time).tm_hour; }
int CTime::GetMinute() const { return localBreakdown(m_time).tm_min; }
int CTime::GetSecond() const { return localBreakdown(m_time).tm_sec; }
int CTime::GetDayOfWeek() const { return localBreakdown(m_time).tm_wday + 1; }

CString CTime::Format(const char* format) const
{
    if (!format)
        return CString();
    const tm local = localBreakdown(m_time);
    char buffer[256];
    const size_t length = std::strftime(buffer, sizeof(buffer), format, &local);
    return CString(buffer, static_cast<int>(length));
}
