#include "key.h"

#include <cstdio>
#include <unistd.h>


namespace {

keybinding from_char(char symbol) {
    if ('A' <= symbol && symbol <= 'Z')
        return (key::of('a') + (symbol - 'A')) | mod::SHIFT;

    if (symbol == '\x1b')
        return key::NONE;

    if (1 <= symbol && symbol <= 26)
        return key::of(symbol + 96) | mod::CTRL;

    return key::of(symbol);
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
    if (modifier == 0) return 0;
    -- modifier;

    keybinding mask = 0;

    if (modifier & 1) mask |= mod::SHIFT;
    if (modifier & 2) mask |= mod::ALT;
    if (modifier & 4) mask |= mod::CTRL;
    if (modifier & 8) mask |= mod::META;

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
    for (; *input != '\0' && '0' <= *input && *input <= '9'; ++ input) {
        number *= 10;
        number += *input - '0';
    }

    return number;
}

} // end anonymous namespace


keybinding read_keybinding() {
    char sequence[9] = {};
    int num_read = read(STDIN_FILENO, sequence, 8);
    if (num_read == 0)
        return key::NONE;

    // Handle: <char> = <char>
    if (keybinding simple = from_char(sequence[0]))
        return simple;

    // All escape sequences start from escape (surprise!)
    if (sequence[0] != '\x1b')
        return key::NONE; // unparsable

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

        // Handle: <esc> '[' = Alt+[
        if (sequence[1] == '\x1b')
            return key::of('[') | mod::ALT;

        return key::NONE; // unparsable
    }

    // Handle: <esc> '[' <keycode> (';' <modifier>) '~'
    if (sequence[num_read - 1] == '~') {
        const char* parsed = sequence + 2;

        int keycode = parse_uint(parsed);
        if (*parsed != '~' && *parsed != ';')
            return key::NONE; // unparsable

        int modifier = 0;
        if (*parsed == ';') {
            ++ parsed;

            modifier = parse_uint(parsed);
        }

        if (*parsed != '~')
            return key::NONE; // unparsable

        ++ parsed;

        if (keybinding simple = from_keycode(keycode))
            return simple | from_ansi_modifier(modifier);

        return key::NONE; // unparsable
    }

    const char* parsed = sequence + 2;
    /* second param */ parse_uint(parsed);

    if (*parsed == ';' || *parsed == 'O' /* FN1-FN4 keys for whatever reason */)
        ++ parsed;

    int modifier = parse_uint(parsed);

    if (keybinding xterm = from_xterm_letter(*parsed))
        return xterm | from_ansi_modifier(modifier);

    return key::NONE; // unparsable
}

std::string describe_key(keybinding bind) {
    bind &= key::MASK;

    switch (bind) {
    case key::NONE:      return "<none>";
    
    // == Functional
    case key::F0:        return "<f0>";
    case key::F1:        return "<f1>";
    case key::F2:        return "<f2>";
    case key::F3:        return "<f3>";
    case key::F4:        return "<f4>";
    case key::F5:        return "<f5>";
    case key::F6:        return "<f6>";
    case key::F7:        return "<f7>";
    case key::F8:        return "<f8>";
    case key::F9:        return "<f9>";
    case key::F10:       return "<f10>";
    case key::F11:       return "<f11>";
    case key::F12:       return "<f12>";
    case key::F13:       return "<f13>";
    case key::F14:       return "<f14>";
    case key::F15:       return "<f15>";
    case key::F16:       return "<f16>";
    case key::F17:       return "<f17>";
    case key::F18:       return "<f18>";
    case key::F19:       return "<f19>";
    case key::F20:       return "<f20>";

    // == Navigation
    case key::UP:        return "<up>";
    case key::DOWN:      return "<down>";
    case key::RIGHT:     return "<right>";
    case key::LEFT:      return "<left>";
    case key::HOME:      return "<home>";
    case key::END:       return "<end>";
    case key::PAGE_UP:   return "<page_up>";
    case key::PAGE_DOWN: return "<page_down>";

    // == Spacing
    case key::TAB:       return "<tab>";
    case key::SPACE:     return "<space>";
    case key::ENTER:     return "<enter>";

    // == Deleting
    case key::BACKSPACE: return "<backspace>";
    case key::DELETE:    return "<delete>";

    // == Special
    case key::ESCAPE:    return "<escape>";
    case key::INSERT:    return "<insert>";

    }

    return std::string(1, key::from(bind));
}

std::string describe_mod(keybinding modifier) {
    modifier &= mod::MASK;

    std::string mods;

    if (modifier & mod::SHIFT) mods += "<shift>+";
    if (modifier & mod::ALT)   mods += "<alt>+";
    if (modifier & mod::CTRL)  mods += "<ctrl>+";
    if (modifier & mod::META)  mods += "<meta>+";

    return mods;
}

std::string describe_keybinding(keybinding bind) {
    return describe_mod(bind) + describe_key(bind);
}

