/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/interrupts.h>
#include <kernel/arch/pc/pit.h>
#include <kernel/debug.h>
#include <kernel/tasking/scheduler.h>
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

static void _timer_irq()
{
    _tick_count++;

    timer_block_t *current = _base;
    timer_block_t *prev = NULL;

    while (current != NULL) {
        if (current->countdown > 0)
            current->countdown--;

        if (current->countdown == 0) {
            timer_block_t *next = current->next;
            if (prev == NULL) {
                _base = next;
            } else {
                prev->next = next;
            }
            current = next;
        } else {
            prev = current;
            current = current->next;
        }
    }

    scheduler_tick();
}

void timer_init()
{
    debug_log("Initializing the timer\n");
    pit_set_frequency(1000);
    irq_register_handler(0, _timer_irq);
    debug_log("Initialized the timer\n");
    interrupts_enable();
}

void sleep(uint64_t ms)
{
    timer_block_t block;
    block.countdown = ms;

    /* Mark task as sleeping */
    interrupts_disable();

    task_t *current = task_get_current();
    if (current) {
        current->quantum = 0;
        current->quantum_remaining = 0;
    }

    block.next = _base;
    _base = &block;

    interrupts_enable();

    /* Wait until wake time */
    while (block.countdown > 0)
        asm_hlt();

    current->quantum = DEFAULT_QUANTUM;
}

uint64_t timer_get_ticks()
{
    return _tick_count;
}
