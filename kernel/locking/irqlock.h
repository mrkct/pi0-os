#pragma once

#include <kernel/base.h>


typedef bool IrqLock;

IrqLock irq_lock();

void release(IrqLock);
