#pragma once

#include <concepts>

#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <stdexcept>

#include <utility>



// parses different string time formats into epoch_ms (the standard assumed by time util convertors)
/*
 * Parses a timestamp string into epoch milliseconds.
 *
 * Handled formats:
 *   "2024-06-05"                  → date only
 *   "2024-06-05 14:30:00"         → datetime with space
 *   "2024-06-05T14:30:00"         → ISO 8601
 *   "2024-06-05T14:30:00.123"     → ISO 8601 with milliseconds
 *   "2024/06/05 14:30:00"         → slash separated
 *   "05-06-2024" / "06-05-2024"   → ambiguous, use format param (default: DDMMYYYY)
 *
 * Returns: epoch milliseconds as long long
*/

enum class DATE_FORMAT { DDMMYYYY, MMDDYYYY };

// string version
inline long long to_epoch_ms(const std::string& ts, DATE_FORMAT format = DATE_FORMAT::DDMMYYYY) {
    // keep ms here becaue mktime() converts to seconds precision
    int ms = 0;

    std::string s = ts;

    // handle weird formats
    std::replace(s.begin(), s.end(), '/', '-');
    std::replace(s.begin(), s.end(), 'T', ' ');

    // extract ms if present
    auto dot = s.find('.');
    if (dot != std::string::npos) {
        std::string ms_str = s.substr(dot + 1);
        ms_str.resize(3, '0');
        ms = std::stoi(ms_str);
        s  = s.substr(0, dot);
    }

    // check if it has time and if the first 4 chars are digits (year_first)
    bool has_time   = s.size() > 10;
    bool year_first = std::isdigit(s[0]) && std::isdigit(s[1]) &&
                      std::isdigit(s[2]) && std::isdigit(s[3]) &&
                      s[4] == '-';


    std::istringstream ss(std::move(s));
    std::tm t = {};

    // parse the str into std::tm format inside ss
    if (year_first) {
        ss >> std::get_time(&t, has_time ? "%Y-%m-%d %H:%M:%S" : "%Y-%m-%d");
    } else {
        if (format == DATE_FORMAT::DDMMYYYY) {
            ss >> std::get_time(&t, has_time ? "%d-%m-%Y %H:%M:%S" : "%d-%m-%Y");
        } else {
            ss >> std::get_time(&t, has_time ? "%m-%d-%Y %H:%M:%S" : "%m-%d-%Y");
        }
    }

    if (ss.fail()) {
        throw std::invalid_argument("parse_str_to_timestamp: unrecognized format: " + ts);
    }


    std::time_t epoch = std::mktime(&t);
    return static_cast<long long>(epoch) * 1000 + ms;
}

enum class TS_UNIT { SECONDS, MILLISECONDS };

// numeric version
inline long long to_epoch_ms(long long ts, TS_UNIT unit = TS_UNIT::MILLISECONDS) {
    switch (unit) {
        case (TS_UNIT::SECONDS): 
            return ts * 1000;
        case (TS_UNIT::MILLISECONDS):
            return ts;
        default:
            throw std::invalid_argument("to_epoch_ms: unknown TS_UNIT");
    }
}



/*
 * Time utilities — input is always epoch milliseconds:
 *
 * to_date(ts_ms)        → YYYYMMDD          as long long  (e.g. 20240605)
 * to_time(ts_ms)        → HHMMSS.mmm        as double     (e.g. 143000.123)
 * to_datetime(ts_ms)    → YYYYMMDDHHMMSSMMM as long long  (e.g. 20240605143000123)
 * to_day_of_week(ts_ms) → 1-7               as int        (0=Sun, 1=Mon ... 6=Sat)
*/

// portable thread-safe localtime
inline std::tm localtime_safe(std::time_t t) {
    std::tm result;
#ifdef _WIN32
    localtime_s(&result, &t);
#else
    localtime_r(&t, &result);
#endif
    return result;
}


inline long long to_date(long long ts) {
    std::time_t t = static_cast<std::time_t>(ts / 1000);
    auto tm = localtime_safe(t);

    int year  = tm.tm_year + 1900;
    int month = tm.tm_mon  + 1;
    int day   = tm.tm_mday;

    return year * 10000 + month * 100 + day;
}


inline double to_time(long long ts) {
    int ms = ts % 1000;

    std::time_t t = static_cast<std::time_t>(ts / 1000);
    auto tm = localtime_safe(t);

    int hour = tm.tm_hour;
    int min  = tm.tm_min;
    int sec  = tm.tm_sec;

    return hour * 10000 + min * 100 + sec + ms / 1000.0;
}


inline long long to_datetime(long long ts) {
    int ms = ts % 1000;

    std::time_t t = static_cast<std::time_t>(ts / 1000);
    auto tm = localtime_safe(t);

    long long year  = tm.tm_year + 1900;
    long long month = tm.tm_mon  + 1;
    long long day   = tm.tm_mday;
    long long hour  = tm.tm_hour;
    long long min   = tm.tm_min;
    long long sec   = tm.tm_sec;

    // YYYYMMDDHHMMSSMMM
    return year  * 10000000000000LL
         + month * 100000000000LL
         + day   * 1000000000LL
         + hour  * 10000000LL
         + min   * 100000LL
         + sec   * 1000LL
         + ms;
}


inline int to_day_of_week(long long ts) {
    std::time_t t = static_cast<std::time_t>(ts / 1000);
    auto tm = localtime_safe(t);

    // 0=Sunday, 1=Monday, ..., 6=Saturday
    return tm.tm_wday;
}


// created this concept so when calling get_timestamp()
// if the type does not contain a timestamp the error is readable and at compile time
template <typename T>
concept HasTimestamp = 
    requires (T t) { t.ts; }        ||
    requires (T t) { t.timestamp; } ||
    requires (T t) { t.time; }      ||
    requires (T t) { t.date; }      ||
    requires (T t) { t.datetime; }  ||
    requires (T t) { t.epoch; };


// NOTE: if you add a field to HasTimestamp, add it here too (and vice versa)
template <HasTimestamp T>
auto get_timestamp(const T& t) {
    if constexpr      (requires { t.ts; })        return t.ts;
    else if constexpr (requires { t.timestamp; }) return t.timestamp;
    else if constexpr (requires { t.time; })      return t.time;
    else if constexpr (requires { t.date; })      return t.date;
    else if constexpr (requires { t.datetime; })  return t.datetime;
    else if constexpr (requires { t.epoch; })     return t.epoch;
    else static_assert(
        false, 
        "unreachable: HasTimestamp passed but no field matched —"
        "did you add a field to the concept but not the extractor (get_timestamp()) ?"
    );
}