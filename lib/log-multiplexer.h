#pragma once

#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <functional>


struct pane {
    pane(std::string name);

    std::string name;
    std::vector<std::string> lines;
    int vscroll, hscroll;
    std::map<char, std::function<int ()>> bindings;

    // TODO: enum its_kind { LOG, IMAGE };
};


class log_multiplexer {
public:
    log_multiplexer();
    ~log_multiplexer();

    log_multiplexer(const log_multiplexer &other) = delete;
    log_multiplexer& operator=(const log_multiplexer &other) = delete;

    log_multiplexer(const log_multiplexer &&other) = delete;
    log_multiplexer& operator=(const log_multiplexer &&other) = delete;


    void create_pane(int pane_id, std::string name);
    void connect_panes(int parent, int child);

    void run();
    void assign(int pane_id, const std::string &message);
    void append(int pane_id, const std::string &message);

    void redraw();

private:
    std::map<int, pane> panes_;
    std::map<int, std::vector<int>> tree_;

    int current_;
    static constexpr int ROOT_PANE_ID = 0;

    std::mutex mutex_;
};


log_multiplexer* get_global_log_multiplexer();

