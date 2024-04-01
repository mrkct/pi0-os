#pragma once

#include <stdint.h>
#include <stddef.h>
#include <api/syscalls.h>
#include <kernel/error.h>


void wm_task_entry();

kernel::Error wm_create_window(api::PID task, uint32_t width, uint32_t height);

kernel::Error wm_update_window(api::PID, uint32_t *framebuffer);

bool wm_read_keyevent(api::PID, api::KeyEvent&);
