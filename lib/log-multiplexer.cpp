#pragma once

#include "log-multiplexer.h"

#include <cassert>
#include <cctype>
#include <climits>
#include <csignal>
#include <mutex>
#include <stdio.h>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


#define MOVE_CURSOR(X, Y) "\033[" #X ";" #Y "H"
#define RESET             "\033[0m"
#define DISABLE_CURSOR    "\033[?25l"
#define ENABLE_CURSOR     "\033[?25h"
#define CLEAR_SCREEN      "\033[2J"
#define RESET_SCREEN      RESET CLEAR_SCREEN MOVE_CURSOR(0, 0)

#define FOREGROUND_BLACK  "\033[30m"
#define BACKGROUND_GREEN  "\033[46m"


namespace {


    void draw_horizontal_line(const std::string &text, int num_cols) {
        printf(FOREGROUND_BLACK BACKGROUND_GREEN);

        for (int i = 0; i < num_cols; ++ i)
        printf("%c", i < text.size() ? text[i] : ' ');

    fflush(stdout);
}

void enable_raw_mode() {
    termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode() {
    termios cooked;
    tcgetattr(STDIN_FILENO, &cooked);
    cooked.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &cooked);
}

void get_terminal_size(int *rows, int *cols) {
    winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    } else {
        *rows = *cols = -1; // TODO: better error handling
    }
}

void register_simple_signal(int signal, void (*handler)(int signo)) {
    struct sigaction sa = {};
    sa.sa_handler = handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(signal, &sa, NULL);
}


log_multiplexer *global_mux = nullptr; // <= stores global multiplexer for handler

void handle_sigwinch(int signo) {
    global_mux->redraw();
}

void handle_sigint(int signo) {
    printf(ENABLE_CURSOR);
    disable_raw_mode();

    exit(0);
}


} // end anonymous namespace


log_multiplexer* get_global_log_multiplexer() { return global_mux; }



log_multiplexer::log_multiplexer():
    current_(0) {

    assert(!global_mux && "There can only be one log multiplexer at a time!");

    global_mux = this;
    redraw();

    register_simple_signal(SIGWINCH, handle_sigwinch);
    register_simple_signal(SIGINT,   handle_sigint);

    enable_raw_mode();
    printf(DISABLE_CURSOR);
}

log_multiplexer::~log_multiplexer() {
    printf(ENABLE_CURSOR);
    disable_raw_mode();

    register_simple_signal(SIGWINCH, SIG_DFL);
    register_simple_signal(SIGINT,  SIG_DFL);

    global_mux = nullptr;
}

void log_multiplexer::run() {
    while (true) {
        char symbols[3];
        int num_read = read(STDIN_FILENO, &symbols, 3);

        if (num_read == 3 && symbols[0] == '\x1b' && symbols[1] == '[') {
            if (symbols[2] == 'D')
                symbols[0] = 'h'; // <= handle <left arrow> the same as <h>

            if (symbols[2] == 'C')
                symbols[0] = 'l'; // <= handle <right arrow> the same as <l>

            if (symbols[0] != '\x1b')
                num_read = 1;
        }

        switch (std::tolower(symbols[0])) {
            case '\x1b': return;
            case    'q': return;

            case    'h': if (current_ >       0) current_ --; break;
            case    'l': if (current_ < INT_MAX) current_ ++; break;
        }

        redraw();
    }
}

void log_multiplexer::append(int log_id, const std::string &message) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logs_[log_id].append(message);
    }

    redraw();
}

void log_multiplexer::redraw() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        printf(RESET_SCREEN "%s", logs_[global_mux->current_].c_str());
    }

    int rows, cols;
    get_terminal_size(&rows, &cols);

    printf(MOVE_CURSOR(%d, 0), rows);
    draw_horizontal_line("[" + std::to_string(current_) + "]", cols);

    fflush(stdout);
}

