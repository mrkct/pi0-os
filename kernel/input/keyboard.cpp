#include <kernel/input/keyboard.h>
#include <api/input.h>
#include <kernel/lib/string.h>
#include <kernel/lib/math.h>
#include <kernel/lib/memory.h>


namespace kernel {

static uint32_t g_keystate[256 / (8 * sizeof(uint32_t))];
static bool g_caps_lock_active = false;
static void (*g_cb)(api::KeyEvent const&) = nullptr;

static unsigned char g_normal_keymap[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    '*', // Some keyboards have a dedicated, duplicate key for the '*' key
    ' '
};
static unsigned char g_shift_keymap[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    '*', // Some keyboards have a dedicated, duplicate key for the '*' key
    ' '
};

static bool is_key_pressed(uint32_t keycode);

void notify_key_event(api::KeyEvent update)
{
    auto idx = update.keycode / (8 * sizeof(uint32_t));
    auto bit = update.keycode % (8 * sizeof(uint32_t));
    if (update.pressed) {
        g_keystate[idx] |= (1 << bit);
    } else {
        g_keystate[idx] &= ~(1 << bit);
    }

    // NOTE: QEMU doesn't report the CAPS LOCK key release event 
    if (update.keycode == api::KEYCODE_CAPS_LOCK && update.pressed) {
        g_caps_lock_active = !g_caps_lock_active;
    }

    if (!update.raw_character && update.keycode < array_size(g_normal_keymap)) {
        bool use_shift_keymap = g_caps_lock_active;
        if (is_key_pressed(api::KEYCODE_SHIFT))
            use_shift_keymap = !use_shift_keymap;

        unsigned char *keymap = use_shift_keymap ? g_shift_keymap : g_normal_keymap;
        update.character = keymap[update.keycode];
    }

    if (g_cb) {
        g_cb(update);
    }
}

static bool is_key_pressed(uint32_t keycode)
{
    auto idx = keycode / (8 * sizeof(uint32_t));
    auto bit = keycode % (8 * sizeof(uint32_t));
    return (g_keystate[idx] & (1 << bit)) != 0;
}

void set_keyboard_event_listener(void (*cb)(api::KeyEvent const&))
{
    g_cb = cb;
}

uint32_t char_to_keycode(unsigned char c)
{
    // WARNING: This is probably incomplete
    switch (tolower(c)) {
    case 'a': return api::KEYCODE_A;
    case 'b': return api::KEYCODE_B;
    case 'c': return api::KEYCODE_C;
    case 'd': return api::KEYCODE_D;
    case 'e': return api::KEYCODE_E;
    case 'f': return api::KEYCODE_F;
    case 'g': return api::KEYCODE_G;
    case 'h': return api::KEYCODE_H;
    case 'i': return api::KEYCODE_I;
    case 'j': return api::KEYCODE_J;
    case 'k': return api::KEYCODE_K;
    case 'l': return api::KEYCODE_L;
    case 'm': return api::KEYCODE_M;
    case 'n': return api::KEYCODE_N;
    case 'o': return api::KEYCODE_O;
    case 'p': return api::KEYCODE_P;
    case 'q': return api::KEYCODE_Q;
    case 'r': return api::KEYCODE_R;
    case 's': return api::KEYCODE_S;
    case 't': return api::KEYCODE_T;
    case 'u': return api::KEYCODE_U;
    case 'v': return api::KEYCODE_V;
    case 'w': return api::KEYCODE_W;
    case 'x': return api::KEYCODE_X;
    case 'y': return api::KEYCODE_Y;
    case 'z': return api::KEYCODE_Z;
    case '1': return api::KEYCODE_1;
    case '2': return api::KEYCODE_2;
    case '3': return api::KEYCODE_3;
    case '4': return api::KEYCODE_4;
    case '5': return api::KEYCODE_5;
    case '6': return api::KEYCODE_6;
    case '7': return api::KEYCODE_7;
    case '8': return api::KEYCODE_8;
    case '9': return api::KEYCODE_9;
    case '0': return api::KEYCODE_0;
    case '-': return api::KEYCODE_MINUS;
    case '=': return api::KEYCODE_EQUAL;
    case '[': return api::KEYCODE_BRACKET_LEFT;
    case ']': return api::KEYCODE_BRACKET_RIGHT;
    case '\\': return api::KEYCODE_BACKSLASH;
    case ';': return api::KEYCODE_SEMICOLON;
    case '\'': return api::KEYCODE_APOSTROPHE;
    case ',': return api::KEYCODE_COMMA;
    case '.': return api::KEYCODE_DOT;
    case '/': return api::KEYCODE_SLASH;
    case '`': return api::KEYCODE_GRAVE_ACCENT;
    case ' ': return api::KEYCODE_SPC;
    default: return 0;
    }
}

}