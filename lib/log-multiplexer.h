#pragma once

#include "key.h"

#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <optional>
#include <memory>


using pane_id = int;


struct location { int x, y; };

struct pane_data {
    std::optional<location> cursor;
    std::vector<std::string> lines;
};

struct pane_action {
    enum its_kind { SUBSTITUTE, REDRAW, IGNORE } kind = IGNORE;
    pane_id substitute;
};

class pane_controller {
public:
    pane_data *pane = nullptr;

    virtual pane_action update(keybinding key) = 0;
    virtual ~pane_controller() = default;
};

struct pane {
    std::string name;
    int vscroll, hscroll;

    std::vector<std::string> lines;
    std::map<keybinding, int> next;

    enum its_mode { LOG, IMAGE } mode;
    std::unique_ptr<pane_controller> controller;

    pane(std::string name, pane::its_mode mode,
         std::unique_ptr<pane_controller> controller = nullptr);

};


class log_multiplexer {
public:
    log_multiplexer();
    ~log_multiplexer();

    log_multiplexer(const log_multiplexer &other) = delete;
    log_multiplexer& operator=(const log_multiplexer &other) = delete;

    log_multiplexer(const log_multiplexer &&other) = delete;
    log_multiplexer& operator=(const log_multiplexer &&other) = delete;


    void create_pane(int pane_id, std::string name, pane::its_mode mode,
                     std::unique_ptr<pane_controller> controller = nullptr);

    void run();
    void assign(int pane_id, const std::string &message);
    void append(int pane_id, const std::string &message);

    void redraw();

private:
    std::map<int, pane> panes_;

    int current_;
    static constexpr int ROOT_PANE_ID = 0;

    std::mutex mutex_;
};


log_multiplexer* get_global_log_multiplexer();

