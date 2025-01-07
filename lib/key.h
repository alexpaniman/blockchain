#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <cassert>


using keybinding = uint16_t;

struct key {
    static inline constexpr keybinding NONE      = 0;
    
    // == Functional
    static inline constexpr keybinding F0        = 1;
    static inline constexpr keybinding F1        = 2;
    static inline constexpr keybinding F2        = 3;
    static inline constexpr keybinding F3        = 4;
    static inline constexpr keybinding F4        = 5;
    static inline constexpr keybinding F5        = 6;
    static inline constexpr keybinding F6        = 7;
    static inline constexpr keybinding F7        = 8;
    static inline constexpr keybinding F8        = 9;
    static inline constexpr keybinding F9        = 10;
    static inline constexpr keybinding F10       = 11;
    static inline constexpr keybinding F11       = 12;
    static inline constexpr keybinding F12       = 13;
    static inline constexpr keybinding F13       = 14;
    static inline constexpr keybinding F14       = 15;
    static inline constexpr keybinding F15       = 16;
    static inline constexpr keybinding F16       = 17;
    static inline constexpr keybinding F17       = 18;
    static inline constexpr keybinding F18       = 19;
    static inline constexpr keybinding F19       = 20;
    static inline constexpr keybinding F20       = 21;

    // == Deleting
    static inline constexpr keybinding DELETE    = 22;

    // == Special
    static inline constexpr keybinding INSERT    = 23;

    // == Navigation
    static inline constexpr keybinding UP        = 24;
    static inline constexpr keybinding DOWN      = 25;
    static inline constexpr keybinding RIGHT     = 26;
    static inline constexpr keybinding LEFT      = 27;

    static inline constexpr keybinding HOME      = 28;
    static inline constexpr keybinding END       = 29;
    static inline constexpr keybinding PAGE_UP   = 30;
    static inline constexpr keybinding PAGE_DOWN = 31;

    // Number of special keys that don't have a corresponding char,
    // so all of the above, including PgDn, Home and such.
    static inline constexpr keybinding NUM_EXTRA = 32;

    template <unsigned char symbol>
    static inline constexpr keybinding from_char = NUM_EXTRA + symbol;

    // == Spacing
    static inline constexpr keybinding TAB       = from_char<'\t'>;
    static inline constexpr keybinding SPACE     = from_char<' '>;
    static inline constexpr keybinding ENTER     = from_char<'\n'>;

    // == Deleting
    static inline constexpr keybinding BACKSPACE = from_char<127>;

    // == Special
    static inline constexpr keybinding ESCAPE    = from_char<'\x1b'>;

    // == Alphabet
    static inline constexpr keybinding A         = from_char<'a'>;
    static inline constexpr keybinding B         = from_char<'b'>;
    static inline constexpr keybinding C         = from_char<'c'>;
    static inline constexpr keybinding D         = from_char<'d'>;
    static inline constexpr keybinding E         = from_char<'e'>;
    static inline constexpr keybinding F         = from_char<'f'>;
    static inline constexpr keybinding G         = from_char<'g'>;
    static inline constexpr keybinding H         = from_char<'h'>;
    static inline constexpr keybinding I         = from_char<'i'>;
    static inline constexpr keybinding J         = from_char<'j'>;
    static inline constexpr keybinding K         = from_char<'k'>;
    static inline constexpr keybinding L         = from_char<'l'>;
    static inline constexpr keybinding M         = from_char<'m'>;
    static inline constexpr keybinding N         = from_char<'n'>;
    static inline constexpr keybinding O         = from_char<'o'>;
    static inline constexpr keybinding P         = from_char<'p'>;
    static inline constexpr keybinding Q         = from_char<'q'>;
    static inline constexpr keybinding R         = from_char<'r'>;
    static inline constexpr keybinding S         = from_char<'s'>;
    static inline constexpr keybinding T         = from_char<'t'>;
    static inline constexpr keybinding U         = from_char<'u'>;
    static inline constexpr keybinding V         = from_char<'v'>;
    static inline constexpr keybinding W         = from_char<'w'>;
    static inline constexpr keybinding X         = from_char<'x'>;
    static inline constexpr keybinding Y         = from_char<'y'>;
    static inline constexpr keybinding Z         = from_char<'z'>;

    // == Numbers
    static inline constexpr keybinding N0        = from_char<'0'>;
    static inline constexpr keybinding N1        = from_char<'1'>;
    static inline constexpr keybinding N2        = from_char<'2'>;
    static inline constexpr keybinding N3        = from_char<'3'>;
    static inline constexpr keybinding N4        = from_char<'4'>;
    static inline constexpr keybinding N5        = from_char<'5'>;
    static inline constexpr keybinding N6        = from_char<'6'>;
    static inline constexpr keybinding N7        = from_char<'7'>;
    static inline constexpr keybinding N8        = from_char<'8'>;
    static inline constexpr keybinding N9        = from_char<'9'>;

    static inline constexpr keybinding of(unsigned char symbol) {
        return NUM_EXTRA + symbol;
    }

    inline static constexpr char from(keybinding key) {
        return (key & MASK) - NUM_EXTRA;
    }

    static inline constexpr keybinding MASK = 0xFFF;
};

struct mod {
    static inline constexpr keybinding CTRL  = 0x1000;
    static inline constexpr keybinding ALT   = 0x2000;
    static inline constexpr keybinding SHIFT = 0x4000;
    static inline constexpr keybinding META  = 0x8000;

    static inline constexpr keybinding MASK = 0xF000;
};

// Parse emacs-like keybinding:
inline consteval keybinding kbd(const char* key) {
    keybinding mods = 0;

    // TODO: handle corner cases

    const char* parsed = key;
    while (*parsed != '\0') {
        if (*parsed == '-' && parsed != key) {
            switch (*(parsed - 1)) {
            case 'C': mods |= mod::CTRL;  break;
            case 'M': mods |= mod::ALT;   break;
            case 'S': mods |= mod::SHIFT; break;
            }
        }

        ++ parsed;
    }

    -- parsed;
    if ('A' <= *parsed && *parsed <= 'Z')
        return key::of(*parsed - 'A' + 'a') | mod::SHIFT | mods;

    return key::of(*parsed) | mods;
}

keybinding read_keybinding();
std::string describe_keybinding(keybinding key);

