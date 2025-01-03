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

    bool shift = follow && lines.size() >= rows;
    for (int i = vscroll + shift; i < rows + vscroll; ++ i) {
        if (i + 1 > lines.size())
            break;

        bool has_newline = false;
        for (int j = hscroll; j < cols + hscroll; ++ j) {
            if (j + 1 > lines[i].size())
                break;

            putchar(lines[i][j]);
            if (lines[i][j] == '\n')
                has_newline = true;
        }

        // TODO: what if last line of the log is written partially without newline?
        if (!has_newline)
            putchar('\n');
    }

    if (follow && hscroll == 0)
        printf("\x1b[47m \x1b[0m\n");
}


log_multiplexer *global_mux = nullptr; // <= stores global multiplexer for handler

void handle_sigwinch(int signo) {
    global_mux->redraw();
}

void handle_sigint(int signo) {
    global_mux->~log_multiplexer(); // TODO: don't use destructor
    exit(0);
}



std::string make_modeline(int cols, const std::string &left, const std::string &right) {
    std::string modeline = left;

    for (int i = 0; i < cols - (int) left.size() - (int) right.size(); ++ i)
        modeline.push_back(' ');

    modeline += right;

    assert(modeline.size() == cols);
    return modeline;
}

} // end anonymous namespace


log_multiplexer* get_global_log_multiplexer() { return global_mux; }


pane::pane(std::string name):
    name(std::move(name)),
    vscroll(VSCROLL_FOLLOW),
    hscroll(0) {

}


log_multiplexer::log_multiplexer():
    current_(-1) {
    assert(!global_mux && "There can only be one log multiplexer at a time!");

    create_pane(-1, "-");

    printf("\x1b[?1049h");

    global_mux = this;
    register_simple_signal(SIGWINCH, handle_sigwinch);
    register_simple_signal(SIGINT,   handle_sigint);

    enable_raw_mode();
    printf(DISABLE_CURSOR);
    redraw();
}

log_multiplexer::~log_multiplexer() {
    printf(ENABLE_CURSOR);
    disable_raw_mode();

    printf("\x1b[?1049l");

    register_simple_signal(SIGWINCH, SIG_DFL);
    register_simple_signal(SIGINT,  SIG_DFL);

    global_mux = nullptr;
}


void log_multiplexer::create_pane(int log_id, std::string name) {
    panes_.emplace(log_id, std::move(name));
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

        pane &current = panes_.at(current_);
        int &hscroll = current.hscroll, &vscroll = current.vscroll;

        switch (symbols[0]) {
            case    'q': this->~log_multiplexer(); exit(0); break; // TODO: don't use exit

            case    '<': if (panes_.count(current_ - 1)) current_ --;              break;
            case    '>': if (panes_.count(current_ + 1)) current_ ++;              break;

            case    'h': if (hscroll != 0)               hscroll --;               break;
            case    'l':                                 hscroll ++;               break;

            case    'g':                                 vscroll = 0;              break;
            case    'G':                                 vscroll = VSCROLL_FOLLOW; break;

            case    'j': if (vscroll != VSCROLL_FOLLOW)  vscroll ++;               break;
            case '\x04': if (vscroll != VSCROLL_FOLLOW)  vscroll += rows / 2;      break;

            case    'k': if (vscroll != 0)               vscroll --;               break;
            case '\x15':
                if (vscroll == VSCROLL_FOLLOW)
                    vscroll -= rows / 2;
                else {
                    vscroll -= rows / 2;
                    if (vscroll < 0)
                        vscroll = 0;
                }
                break;
        }

        if (vscroll < VSCROLL_FOLLOW) {
            vscroll = current.lines.size() - rows;
            vscroll = vscroll < 0 ? 0 : vscroll;
        }

        if (vscroll > static_cast<int>(current.lines.size()) - rows)
            vscroll = VSCROLL_FOLLOW;

        redraw();
    }
}

void log_multiplexer::assign(int log_id, const std::string &message) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        pane &current = panes_.at(log_id);

        current.lines.clear();
        split_line(current.lines, message);
    }

    if (log_id == current_)
        redraw();
}

void log_multiplexer::append(int log_id, const std::string &message) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        split_line(panes_.at(log_id).lines, message);
    }

    if (log_id == current_)
        redraw();
}

void log_multiplexer::redraw() {
    int rows, cols;
    get_terminal_size(&rows, &cols);

    if (rows == 0 || cols == 0)
        return;

    pane &current = panes_.at(current_);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        printf(RESET_SCREEN);
        print_page(current.lines, rows - 1, cols, current.vscroll, current.hscroll);
    }

    {
        printf(MOVE_CURSOR(%d, 0), rows);

        std::string location;
        switch (current.vscroll) {
            case VSCROLL_FOLLOW: location += "BOT";                                  break;
            case              0: location += "TOP";                                  break;
            default:             location += "+" + std::to_string(current.vscroll);  break;
        }

        switch (current.hscroll) {
            case              0:                                                     break;
            default:             location += " +" + std::to_string(current.hscroll); break;
        }

        location += " ";

        std::string marker = "[" + current.name + "]";

        std::string modeline = make_modeline(cols, marker, location);
        draw_horizontal_line(modeline, cols);
    }

    fflush(stdout);
}

