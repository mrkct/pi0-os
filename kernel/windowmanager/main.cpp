#include <kernel/base.h>
#include <api/process.h>
#include <kernel/task/scheduler.h>
#include <kernel/input/keyboard.h>
#include <kernel/device/framebuffer.h>
#include <kernel/lib/circular_queue.h>
#include <kernel/lib/linkedlist.h>
#include "windowmanager.h"


using namespace kernel;
using namespace klib;

struct Window {
    api::PID owner;
    uint32_t x, y, width, height;
    uint32_t *framebuffer;

    bool was_updated = false;
    CircularQueue<api::KeyEvent, 32> keyevents;
};

static LinkedList<Window> g_open_windows;
static bool g_screen_requires_update = true;
static bool g_screen_requires_full_redraw = false;


static void draw_background()
{
    static constexpr uint32_t BACKGROUND_COLOR = 0xffa87332;
    Framebuffer &fb = get_main_framebuffer();

    uint32_t *addr = fb.address;
    uint32_t pixelcount = fb.pitch * fb.height / sizeof(uint32_t);

    for (; pixelcount >= 4; pixelcount -= 4) {
        *addr++ = BACKGROUND_COLOR;
        *addr++ = BACKGROUND_COLOR;
        *addr++ = BACKGROUND_COLOR;
        *addr++ = BACKGROUND_COLOR;
    }

    for (; pixelcount; pixelcount--) {
        *addr++ = BACKGROUND_COLOR;
    }
}

static Window *find_window(api::PID pid)
{
    auto *node = g_open_windows.find([&](auto *w) { return w->value.owner == pid; });
    return node == nullptr ? nullptr : &node->value;
}

void wm_task_entry()
{
    set_keyboard_event_listener([](api::KeyEvent const& event) {
        auto *active_window = g_open_windows.first();
        if (active_window == nullptr)
            return;
        
        active_window->value.keyevents.push(event);
    });

    draw_background();
    while (true) {
        interrupt_disable();
        g_open_windows.foreach([](ListNode<Window> *window) {
            if (find_task_by_pid(window->value.owner) == nullptr) {
                kprintf("[WM]: Removing window %d\n", window->value.owner);
                g_open_windows.remove(window);
                kfree(window);
                g_screen_requires_update = true;
                g_screen_requires_full_redraw = true;
            }
        });

        if (g_screen_requires_update) {
            // We don't need to redraw a window if all the ones below it also weren't updated
            bool has_refreshed_windows_below = false;
            if (g_screen_requires_full_redraw) {
                g_screen_requires_full_redraw = false;
                has_refreshed_windows_below = true;
                draw_background();
            }

            g_open_windows.foreach_reverse([&](ListNode<Window> *window) {
                if (window->value.was_updated) {
                    window->value.was_updated = false;
                    has_refreshed_windows_below = true;
                }

                if (has_refreshed_windows_below) {
                    blit_to_main_framebuffer(
                        window->value.framebuffer,
                        window->value.x,
                        window->value.y,
                        window->value.width,
                        window->value.height);
                }
            });
            g_screen_requires_update = false;
        }
        interrupt_enable();
        api::sys_yield();
    }
}

bool wm_read_keyevent(api::PID pid, api::KeyEvent &out_event)
{
    auto *wnode = find_window(pid);
    if (wnode == nullptr)
        return false;
    return wnode->keyevents.pop(out_event);
}

Error wm_create_window(api::PID task, uint32_t width, uint32_t height)
{
    Window window = {};
    window.owner = task;

    // TODO: A smarter algorithm for the initial positioning of a window
    static uint32_t positions[2*4] = {
        120, 120,
        1280 - 640 - 120, 120,
        120, 360,
        1280 - 120 - 640, 120
    };
    static uint32_t next_pos = 0;
    window.x = positions[next_pos];
    window.y = positions[next_pos+1];
    window.was_updated = true;
    next_pos = (next_pos + 2) % sizeof(positions);

    window.width = width;
    window.height = height;
    
    uint32_t *fb;
    TRY(kmalloc(width*height*sizeof(uint32_t), fb));
    window.framebuffer = fb;
    window.keyevents = {};

    auto result = g_open_windows.add(window);
    if (!result.is_success()) {
        kfree(fb);
        return result;
    }

    g_screen_requires_update = true;
    return Success;
}

Error wm_update_window(api::PID pid, uint32_t *framebuffer)
{
    Window *w = find_window(pid);
    if (w == nullptr) {
        kprintf("[WM]: Received request to update window %d, but it does not exist\n", pid);
        return NotFound;
    }
    
    memcpy(w->framebuffer, framebuffer, w->width * w->height * sizeof(uint32_t));
    w->was_updated = true;
    g_screen_requires_update = true;
    return Success;
}
