#include "time_util.h"

namespace common{


int64_t now_in_s() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t now_in_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t now_in_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string date_format(time_t ts_ms, const char *format){
    struct tm  result;
    struct tm *ltm = localtime_r(&ts_ms, &result);
    if (nullptr == ltm) return "";
    char buffer[64];
    if (snprintf(buffer, sizeof(buffer), format, 1900 + ltm->tm_year, 1 + ltm->tm_mon, ltm->tm_mday) < 0) {
        return "";
    }
    return std::string {buffer};
}

std::string date_time_format(time_t ts_ms, const char *format){
    struct tm  result;
    struct tm *ltm = localtime_r(&ts_ms, &result);
    if (nullptr == ltm) return "";
    char buffer[64];
    if (snprintf(buffer, sizeof(buffer), format, 1900 + ltm->tm_year, 1 + ltm->tm_mon, ltm->tm_mday, 
        ltm->tm_hour, ltm->tm_min, ltm->tm_sec) < 0) {
        return "";
    }
    return std::string {buffer};
}

std::string date_format_from_now(time_t time_offset_ms, const char *format){
    time_t     now = time(nullptr);
    now += time_offset_ms;
    return date_format(now, format);
}

std::string date_time_format_from_now(time_t time_offset_ms, const char *format){
    time_t     now = time(nullptr);
    now += time_offset_ms;
    return date_time_format(now, format);
}

};

