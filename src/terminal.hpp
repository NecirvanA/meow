#pragma once

#include <utility>

namespace term {

enum class EventType { None, Key, Resize };

struct Event {
    EventType type = EventType::None;
    char key = 0;
};

// (cols, rows)
std::pair<int, int> size();

void enable_raw_mode();
void disable_raw_mode();

void enter_alt_screen();
void leave_alt_screen();
void hide_cursor();
void show_cursor();

// 0-indexed column/row.
void move_to(int col, int row);
void move_to_column(int col);

void clear_all();
void clear_current_line();
void clear_from_cursor_up();

// Installs the SIGWINCH handler used by poll_event to detect resizes.
void install_resize_handler();

// Waits up to timeout_ms for a key press or terminal resize.
Event poll_event(int timeout_ms);

} // namespace term
