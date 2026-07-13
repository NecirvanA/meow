#include "terminal.hpp"

#include <cstdio>
#include <cstring>
#include <csignal>

#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace {

termios g_orig_termios{};
bool g_raw_mode = false;
volatile sig_atomic_t g_resized = 0;

void handle_sigwinch(int) { g_resized = 1; }

} // namespace

namespace term {

std::pair<int, int> size() {
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        return {ws.ws_col, ws.ws_row};
    }
    return {80, 24};
}

void enable_raw_mode() {
    if (g_raw_mode) return;
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) != 0) return;
    termios raw = g_orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    g_raw_mode = true;
}

void disable_raw_mode() {
    if (!g_raw_mode) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
    g_raw_mode = false;
}

void enter_alt_screen() { std::fputs("\x1b[?1049h", stdout); std::fflush(stdout); }
void leave_alt_screen() { std::fputs("\x1b[?1049l", stdout); std::fflush(stdout); }
void hide_cursor() { std::fputs("\x1b[?25l", stdout); std::fflush(stdout); }
void show_cursor() { std::fputs("\x1b[?25h", stdout); std::fflush(stdout); }

void move_to(int col, int row) {
    std::printf("\x1b[%d;%dH", row + 1, col + 1);
    std::fflush(stdout);
}

void move_to_column(int col) {
    std::printf("\x1b[%dG", col + 1);
    std::fflush(stdout);
}

void clear_all() { std::fputs("\x1b[2J", stdout); std::fflush(stdout); }
void clear_current_line() { std::fputs("\x1b[2K", stdout); std::fflush(stdout); }
void clear_from_cursor_up() { std::fputs("\x1b[1J", stdout); std::fflush(stdout); }

void install_resize_handler() {
    struct sigaction sa{};
    sa.sa_handler = handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, nullptr);
}

Event poll_event(int timeout_ms) {
    if (g_resized) {
        g_resized = 0;
        return {EventType::Resize, 0};
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
        char c = 0;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == 1) {
            return {EventType::Key, c};
        }
        if (n == 0) {
            // stdin closed/EOF: treat as a quit request instead of busy-looping.
            return {EventType::Key, 'q'};
        }
    }
    if (g_resized) {
        g_resized = 0;
        return {EventType::Resize, 0};
    }
    return {EventType::None, 0};
}

} // namespace term
