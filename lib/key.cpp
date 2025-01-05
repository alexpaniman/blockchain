#include "key.h"

#include <cstdio>
#include <unistd.h>


namespace {

keybinding from_char(char symbol) {
    if ('a' <= symbol && symbol <= 'z')
        return key::A + (symbol - 'a');

    if ('A' <= symbol && symbol <= 'Z')
        return (key::A + (symbol - 'A')) | mod::SHIFT;

    if ('0' <= symbol && symbol <= '9')
        return key::N0 + (symbol - '0');

    return key::NONE;
}

keybinding from_keycode(int keycode) {
    switch (keycode) {
    case  1: return key::HOME;
    case  2: return key::INSERT;
    case  3: return key::DELETE;
    case  4: return key::END;
    case  5: return key::PAGE_UP;
    case  6: return key::PAGE_DOWN;
    case  7: return key::HOME;
    case  8: return key::END;

    case 10: return key::F0;
    case 11: return key::F1;
    case 12: return key::F2;
    case 13: return key::F3;
    case 14: return key::F4;
    case 15: return key::F5;

    case 17: return key::F6;
    case 18: return key::F7;
    case 19: return key::F8;
    case 20: return key::F9;
    case 21: return key::F10;

    case 23: return key::F11;
    case 24: return key::F12;
    case 25: return key::F13;
    case 26: return key::F14;

    case 28: return key::F15;
    case 29: return key::F16;

    case 31: return key::F17;
    case 32: return key::F18;
    case 33: return key::F19;
    case 34: return key::F20;
    }

    return key::NONE;
}

keybinding from_ansi_modifier(int modifier) {
    keybinding mask = 0;

    -- modifier;

    if (modifier & 1)
        mask |= mod::SHIFT;

    if (modifier & 2)
        mask |= mod::ALT;

    if (modifier & 4)
        mask |= mod::CTRL;

    if (modifier & 8)
        mask |= mod::META;

    return mask;
}

keybinding from_xterm_letter(char letter) {
    switch (letter) {
    case 'A': return key::UP;
    case 'B': return key::DOWN;
    case 'C': return key::RIGHT;
    case 'D': return key::LEFT;

    case 'F': return key::END;
    case 'G': return key::NONE; // it's actually "keypad 5", whatever that means
    case 'H': return key::HOME;

    case 'P': return key::F1;
    case 'Q': return key::F2;
    case 'R': return key::F3;
    case 'S': return key::F4;
    }

    return key::NONE;
}

int parse_uint(const char *&input) {
    int number = 0;
    while (*input != '\0' && '0' <= *input && *input <= '9') {
        number *= 10;
        number += *input - '0';
    }

    return number;
}

} // end anonymous namespace


keybinding read() {
    char sequence[9];
    int num_read = 0;

    num_read = read(STDIN_FILENO, sequence, 1);
    if (num_read == 0)
        return key::NONE;

    // Handle: <char> = <char>
    if (keybinding simple = from_char(sequence[0]))
        return simple;

    // All escape sequences start from escape (surprise!)
    if (sequence[0] != '\x1b')
        return key::NONE; // TODO: handle more keys

    num_read += read(STDIN_FILENO, sequence + 1, 7);

    // Handle: <esc>
    if (num_read == 1)
        return key::ESCAPE;

    if (num_read == 2) {
        // Handle: <esc> <char> = Alt+<char>
        if (keybinding simple = from_char(sequence[1]))
            return simple | mod::ALT;

        // Handle: <esc> <esc> = <esc>
        if (sequence[1] == '\x1b')
            return key::ESCAPE;

        return key::NONE; // unparsable
    }

    // Handle: <esc> '[' = Alt+[
    if (sequence[1] != '[')
        return key::NONE; // TODO: handle more keys

    // Handle: <esc> '[' <keycode> (';' <modifier>) '~'
    if (sequence[num_read - 1] == '~') {
        const char* parsed = sequence + 2;

        int keycode = parse_uint(parsed);
        if (*parsed != '~' && *parsed != ';')
            return key::NONE; // unparsable

        int modifier = 0;
        if (*parsed ++ == ';')
            modifier = parse_uint(parsed);

        if (*parsed != '~')
            return key::NONE; // unparsable

        if (keybinding simple = from_keycode(keycode))
            return simple | from_ansi_modifier(modifier);

        return key::NONE; // unparsable
    }

    const char* parsed = sequence + 2;
    int modifier = parse_uint(parsed);

    if (modifier == 0)
        modifier = 1;

    if (keybinding xterm = from_xterm_letter(*parsed))
        return xterm | from_ansi_modifier(modifier);

    return key::NONE; // unparsable
}

