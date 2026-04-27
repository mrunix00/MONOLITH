/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/libui.h>
#include <stdlib.h>

int main()
{
    ui_ctx_t ctx = {0};
    if (!ui_init_context(&ctx))
        goto terminate;

    ui_wctx_t *window = ui_new_window(&ctx, "UIDemo", 300, 110, (window_flags_t){.resizable = true});
    if (window == NULL)
        goto terminate;

    char textbox_buffer[256] = {0};
    ui_textbox_state_t textbox_state = ui_new_textbox_state(textbox_buffer, sizeof(textbox_buffer));
    while (ui_pump_events(&ctx)) {
        ui_begin_window(window);

        ui_begin_column(window, (ui_rtlb_t){.r = 5, .t = 5, .l = 5, .b = 0});
        ui_label(window, "Hello, World!");
        ui_label(window, "This is libui");
        ui_begin_row(window, (ui_rtlb_t){0});
        if (ui_button(window, "Exit")) {
            exit();
        }
        ui_textbox(window, &textbox_state);
        ui_end_row(window);
        ui_end_column(window);

        ui_end_window(window);
    }

terminate:
    ui_deinit_context(&ctx);
    exit();
}
