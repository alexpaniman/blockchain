#include "log-multiplexer.h"
#include "log.h"

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

void split_line(std::vector<std::string> &lines, const std::string &text) {
    for (auto i = text.begin(); i != text.end(); ++ i) {
        if (lines.empty() || lines.back().back() == '\n')
            lines.emplace_back();

        lines.back().push_back(*i);
    }
}


inline constexpr int VSCROLL_FOLLOW = -1;

void print_page(const std::vector<std::string> &lines, int rows, int cols, int vscroll, int hscroll) {
    bool follow = vscroll == VSCROLL_FOLLOW;
    if (follow)
        vscroll = rows > lines.size() ? 0 : lines.size() - rows;

    for (int i = vscroll + follow; i < rows + vscroll; ++ i) {
        if (i + 1 > lines.size())
            break;

        for (int j = hscroll; j < cols + hscroll; ++ j) {
            if (j + 1 > lines[i].size())
                break;

            putchar(lines[i][j]);
        }
    }

    if (follow)
        printf("\x1b[47m \x1b[0m\n");
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
    current_(0),
    vscroll_(VSCROLL_FOLLOW),
    hscroll_(0) {

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
            if (symbols[2] == 'D') symbols[0] = '<'; // <= handle the same as '<'
            if (symbols[2] == 'C') symbols[0] = '>'; // <= handle the same as '>'
        }

        int rows, cols;
        get_terminal_size(&rows, &cols);

        -- rows;

        switch (symbols[0]) {
            case    'q': this->~log_multiplexer(); exit(0); break; // TODO: don't use exit

            case    '<': if (current_ != 0)              current_ --;               break;
            case    '>':                                 current_ ++;               break;

            case    'h': if (hscroll_ != 0)              hscroll_ --;               break;
            case    'l':                                 hscroll_ ++;               break;

            case    'g':                                 vscroll_ = 0;              break;
            case    'G':                                 vscroll_ = VSCROLL_FOLLOW; break;

            case    'j': if (vscroll_ != VSCROLL_FOLLOW) vscroll_ ++;               break;
            case '\x04': if (vscroll_ != VSCROLL_FOLLOW) vscroll_ += rows / 2;      break;

            case    'k': if (vscroll_ != 0)              vscroll_ --;               break;
            case '\x15':
                if (vscroll_ == VSCROLL_FOLLOW)
                    vscroll_ -= rows / 2;
                else {
                    vscroll_ -= rows / 2;
                    if (vscroll_ < 0)
                        vscroll_ = 0;
                }
                break;
        }

        if (vscroll_ < VSCROLL_FOLLOW) {
            vscroll_ = logs_[current_].size() - rows;
            vscroll_ = vscroll_ < 0 ? 0 : vscroll_;
        }

        if (vscroll_ > static_cast<int>(logs_[current_].size()) - rows)
            vscroll_ = VSCROLL_FOLLOW;

        redraw();
    }
}

void log_multiplexer::append(int log_id, const std::string &message) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        split_line(logs_[log_id], message);
    }

    redraw();
}

void log_multiplexer::redraw() {
    int rows, cols;
    get_terminal_size(&rows, &cols);

    if (rows == 0 || cols == 0)
        return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        printf(RESET_SCREEN);
        print_page(logs_[current_], rows - 1, cols, vscroll_, hscroll_);
    }

    {
        printf(MOVE_CURSOR(%d, 0), rows);

        std::string text = "[" + std::to_string(current_) + "] ";
        switch (vscroll_) {
            case VSCROLL_FOLLOW: text += "BOT";                          break;
            case              0: text += "TOP";                          break;
            default:             text += "+" + std::to_string(vscroll_); break;
        }

        switch (hscroll_) {
            case              0:                                         break;
            default:             text += "+" + std::to_string(hscroll_); break;
        }

        draw_horizontal_line(text, cols);
    }

    fflush(stdout);
}

