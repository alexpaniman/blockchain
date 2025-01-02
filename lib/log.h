#pragma once

#include "log-multiplexer.h"

#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

#include <format>
#include <stdio.h>


#ifndef LOG_ID
#define LOG_ID -1 // <= just print to stdout
#endif


inline std::string get_current_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm *local_time = std::localtime(&now_time);

    std::stringstream time_stream;
    time_stream << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");

    return time_stream.str();
}

template <typename... arg_types>
void log_print(ssize_t id, const std::format_string<arg_types...>& fmt, arg_types&&... args) {
    std::string message = std::format(fmt, std::forward<arg_types>(args)...);
    if (id == -1) {
        puts(message.c_str());
        return;
    }

    message.push_back('\n');
    get_global_log_multiplexer()->append(id, message);
}


#ifndef NOLOG
#define LOG(fmt, ...) log_print(LOG_ID, "LOG [{}]: " fmt, get_current_time() __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG(fmt, ...) ((void) 0)
#endif
