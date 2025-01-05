#pragma once

#include <cstdint>
#include <cstdio>

enum key: uint16_t {
    NONE      = 0x00,

    // == Alphabet
    A         = 0x01,
    B         = 0x02,
    C         = 0x03,
    D         = 0x04,
    E         = 0x05,
    F         = 0x06,
    G         = 0x07,
    H         = 0x08,
    I         = 0x09,
    J         = 0x0A,
    K         = 0x0B,
    L         = 0x0C,
    M         = 0x0D,
    N         = 0x0E,
    O         = 0x0F,
    P         = 0x10,
    Q         = 0x11,
    R         = 0x12,
    S         = 0x13,
    T         = 0x14,
    U         = 0x15,
    V         = 0x16,
    W         = 0x17,
    X         = 0x18,
    Y         = 0x19,
    Z         = 0x1A,

    // == Numbers
    N0        = 0x1B,
    N1        = 0x1C,
    N2        = 0x1D,
    N3        = 0x1E,
    N4        = 0x1F,
    N5        = 0x20,
    N6        = 0x21,
    N7        = 0x22,
    N8        = 0x23,
    N9        = 0x24,

    // == Functional
    F0        = 0x25,
    F1        = 0x26,
    F2        = 0x27,
    F3        = 0x28,
    F4        = 0x29,
    F5        = 0x2A,
    F6        = 0x2B,
    F7        = 0x2C,
    F8        = 0x2D,
    F9        = 0x2E,
    F10       = 0x2F,
    F11       = 0x30,
    F12       = 0x31,
    F13       = 0x32,
    F14       = 0x33,
    F15       = 0x34,
    F16       = 0x35,
    F17       = 0x36,
    F18       = 0x37,
    F19       = 0x38,
    F20       = 0x39,

    // == Spacing
    TAB       = 0x3A,
    SPACE     = 0x3B,
    ENTER     = 0x3C,

    // == Deleting
    BACKSPACE = 0x3D,
    DELETE    = 0x3E,

    // == Special
    ESCAPE    = 0x3F,
    INSERT    = 0x40,

    // == Navigation
    UP        = 0x41,
    DOWN      = 0x42,
    RIGHT     = 0x43,
    LEFT      = 0x44,
    HOME      = 0x45,
    END       = 0x46,
    PAGE_UP   = 0x47,
    PAGE_DOWN = 0x48,
};

struct mod {
    static inline constexpr uint32_t CTRL  = 0x100;
    static inline constexpr uint32_t ALT   = 0x200;
    static inline constexpr uint32_t SHIFT = 0x400;
    static inline constexpr uint32_t META  = 0x800;
};

inline constexpr int KEY_MASK = 0xFF;

using keybinding = uint32_t;

