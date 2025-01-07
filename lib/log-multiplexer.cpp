#include "log-multiplexer.h"
#include "key.h"
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

void clamp_nonnegative(int &value) {
    value = value < 0? 0 : value;
}

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

void print_page(const std::vector<std::string> &lines, int rows, int cols, int vscroll, int hscroll, bool follow) {
    if (follow)
        vscroll = rows > lines.size() ? 0 : lines.size() - rows;

    bool shift = follow && lines.size() >= rows;
    for (int i = vscroll + shift; i < rows + vscroll; ++ i) {
        if (i + 1 > static_cast<int>(lines.size()))
            break;

        if (i < 0) {
            putchar('\n');
            continue;
        }

        bool has_newline = false;
        for (int j = hscroll; j < cols + hscroll; ++ j) {
            if (j + 1 > static_cast<int>(lines[i].size()))
                break;

            if (j < 0) {
                putchar(' ');
                continue;
            }

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


pane::pane(std::string name, pane::its_mode mode,
           std::unique_ptr<pane_controller> controller):
    name(std::move(name)),
    vscroll(VSCROLL_FOLLOW),
    hscroll(0),
    mode(mode),
    controller(std::move(controller)) {
}


log_multiplexer::log_multiplexer():
    current_(-1) {
    assert(!global_mux && "There can only be one log multiplexer at a time!");

    create_pane(-1, "-", pane::LOG);

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


void log_multiplexer::create_pane(int pane_id, std::string name, pane::its_mode mode,
                                  std::unique_ptr<pane_controller> controller) {
    panes_.emplace(pane_id, pane{std::move(name), mode, std::move(controller)});
}

void log_multiplexer::run() {
    while (true) {
        int rows, cols;
        get_terminal_size(&rows, &cols);

        -- rows;

        pane &current = panes_.at(current_);
        int &hscroll = current.hscroll, &vscroll = current.vscroll;

        int prev_vscroll = vscroll;
        bool prev_follow = vscroll == VSCROLL_FOLLOW;

        int num_lines = current.lines.size();
        int max_vscroll = num_lines - rows;
        clamp_nonnegative(max_vscroll);

        switch (read_keybinding()) {
            case key::LEFT     : current_ --;                            break;
            case key::RIGHT    : current_ ++;                            break;

            case kbd(      "h"): hscroll --;                             break;
            case kbd(      "l"): hscroll ++;                             break;

            case key::HOME     : vscroll = hscroll = prev_follow = 0;    break;
            case key::END      : vscroll = max_vscroll + 1, hscroll = 0; break;

            case kbd(      "g"): vscroll = prev_follow = 0;              break;
            case kbd(      "G"): vscroll = max_vscroll + 1;              break;

            case kbd(      "j"): vscroll ++;                             break;
            case kbd(    "C-d"): vscroll += rows / 2;                    break;
            case key::PAGE_DOWN: vscroll += rows;                        break;

            case kbd(      "k"): vscroll --;                             break;
            case kbd(    "C-u"): vscroll -= rows / 2;                    break;
            case key::PAGE_UP  : vscroll -= rows;                        break;
        }

        // VSCROLL & HSCROLL are restricted in LOG mode, take care of that:
        if (current.mode == pane::LOG) {
            // ============= HSCROLL =============
            clamp_nonnegative(hscroll);

            // ============= VSCROLL =============
            if (prev_follow) {
                // We were in follow mode and broke out by scrolling up
                if (vscroll < VSCROLL_FOLLOW)
                    vscroll += max_vscroll - VSCROLL_FOLLOW + 1;
                // Follow is the bottom state, can't scroll down from it 
                else
                if (vscroll > VSCROLL_FOLLOW)
                    vscroll = VSCROLL_FOLLOW;
            } else {
                // Can't scroll past the beginning of the log, reset scroll to 0
                clamp_nonnegative(vscroll);

                // Scrolling past the last line activates follow mode
                if (vscroll > max_vscroll)
                    vscroll = VSCROLL_FOLLOW;
            }
        }

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

        bool follow = current.vscroll == VSCROLL_FOLLOW && current.mode == pane::LOG;
        print_page(current.lines, rows - 1, cols, current.vscroll, current.hscroll, follow);
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

        std::string left = marker + " ";
        for (int i = 0; i < current.next.size(); ++ i)
            left += "(" + std::to_string(i) + ")" + panes_.at(current.next[i]).name + " ";

        std::string modeline = make_modeline(cols, left, location);
        draw_horizontal_line(modeline, cols);
    }

    fflush(stdout);
}

