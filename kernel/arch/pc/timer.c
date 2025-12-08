/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/idt.h>
#include <kernel/arch/pc/pit.h>
#include <kernel/debug.h>
#include <kernel/memory/heap.h>
#include <kernel/timer.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct timer_block timer_block_t;
struct timer_block
{
    struct timer_block *next;
    uint64_t countdown;
};

static timer_block_t *_base;
static volatile uint64_t _tick_count = 0;

static timer_block_t *_new_time_block(uint64_t duration)
{
    timer_block_t *new_block = kmalloc(sizeof(timer_block_t));
    new_block->countdown = duration;
    if (_base == NULL) {
        _base = new_block;
        new_block->next = NULL;
    } else {
        new_block->next = _base;
        _base = new_block;
    }
    return new_block;
}

static void _timer_irq()
{
    _tick_count++;

    timer_block_t *current = _base;
    timer_block_t *prev = NULL;

    while (current != NULL) {
        if (current->countdown == 0) {
            timer_block_t *temp = current;
            if (prev == NULL) {
                _base = current->next;
            } else {
                prev->next = current->next;
            }
            current = current->next;
            kfree(temp);
        } else {
            current->countdown--;
            prev = current;
            current = current->next;
        }
    }
}

void timer_init()
{
    debug_log("[*] Initializing the timer\n");
    pit_set_frequency(1000);
    irq_register_handler(0, _timer_irq);
    debug_log("[+] Initialized the timer\n");
}

void sleep(uint64_t ms)
{
    timer_block_t *block = _new_time_block(ms);
    while (block->countdown > 0)
        __asm__("hlt");
}

uint64_t timer_get_ticks()
{
    return _tick_count;
}
