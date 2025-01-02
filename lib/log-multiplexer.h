#pragma once

#include <map>
#include <string>
#include <mutex>


class log_multiplexer {
public:
    log_multiplexer();
    ~log_multiplexer();

    log_multiplexer(const log_multiplexer &other) = delete;
    log_multiplexer& operator=(const log_multiplexer &other) = delete;

    log_multiplexer(const log_multiplexer &&other) = delete;
    log_multiplexer& operator=(const log_multiplexer &&other) = delete;


    void run();
    void append(int log_id, const std::string &message);

    void redraw();

private:
    std::map<int, std::string> logs_;
    int current_;

    std::mutex mutex_;
};

log_multiplexer* get_global_log_multiplexer();

