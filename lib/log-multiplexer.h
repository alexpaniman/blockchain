#pragma once


#include <csignal>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <map>
#include <string>

class log_multiplexer;

inline log_multiplexer *global_mux = nullptr;

class log_multiplexer {
public:

    log_multiplexer() {
        global_mux = this;
        redraw();

        {
            struct sigaction sa;
            sa.sa_handler = redraw;
            sa.sa_flags = 0;
            sigemptyset(&sa.sa_mask);
            sigaction(SIGWINCH, &sa, NULL);
        }

        {
            struct sigaction sa;
            sa.sa_handler = handle_sigint;
            sigaction(SIGINT, &sa, NULL);
        }
    }

    void run() {
        enable_raw_mode();
        disable_cursor();

        while (true) {
            read(STDIN_FILENO, &current_, 1);

            if (current_ == 'q')
                break;

            if (logs.find(current_) == logs.end())
                continue;

            redraw();
        }

        enable_cursor();
        disable_raw_mode();
    }

    static void redraw(int sig = 0) {
        global_mux->clear_screen();
        printf("%s\n", global_mux->logs[global_mux->current_].c_str());

        int rows, cols;
        global_mux->get_terminal_size(&rows, &cols);

        global_mux->move_cursor(rows, 0);
        global_mux->draw_horizontal_line(rows, 0, cols - 1, ("[" + std::string{global_mux->current_} + "]").c_str()); 

        fflush(stdout);
    }

    void append(char symbol, std::string log) {
        logs[symbol].append(log);
    }


private:
    std::map<char, std::string> logs;
    char current_;

    static void handle_sigint(int sig = 0) {
        enable_cursor();
        disable_raw_mode();

        exit(0);
    }

    void draw_horizontal_line(int row, int start_col, int end_col, const char* text = "") {
        printf("\033[%d;%dH", row, start_col); // Move to starting position
        printf("\033[30m\033[42m"); // TODO: green color

        int i = start_col;
        for (; i <= end_col; i++) {
            if (text[i - start_col] == '\0')
                break;
            printf("%c", text[i - start_col]);
        }
        for (; i <= end_col; i++) {
            printf(" "); // Unicode line character
        }

        fflush(stdout);
    }

    static void enable_raw_mode() {
        struct termios raw;
        tcgetattr(STDIN_FILENO, &raw);
        raw.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    static void disable_raw_mode() {
        struct termios cooked;
        tcgetattr(STDIN_FILENO, &cooked);
        cooked.c_lflag |= (ICANON | ECHO); // Re-enable canonical mode and echo
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &cooked);
    }

    void move_cursor(int row, int col) {
        printf("\033[%d;%dH", row, col); // ANSI escape sequence
        fflush(stdout);
    }

    void get_terminal_size(int *rows, int *cols) {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
            *rows = ws.ws_row;
            *cols = ws.ws_col;
        } else {
            *rows = *cols = -1; // Error case
        }
    }

    void clear_screen() {
        printf("\033[0m\033[2J\033[H"); // Clear screen and move cursor to top-left
        fflush(stdout);
    }

    // Function to disable the cursor
    static void disable_cursor() {
        printf("\033[?25l");
        fflush(stdout);
    }

    // Function to enable the cursor
    static void enable_cursor() {
        printf("\033[?25h");
        fflush(stdout);
    }
};

