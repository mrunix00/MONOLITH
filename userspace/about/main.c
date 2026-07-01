/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/libui.h>
#include <libui/widgets/column.h>
#include <libui/widgets/spacer.h>
#include <libui/widgets/text.h>

int main()
{
    ui_ctx_t ctx;
    ui_wctx_t *w;

    if (!ui_init_context(&ctx))
        return 1;

    w = ui_new_window(&ctx, "About MONOLITH", 360, 190, (window_flags_t){.resizable = true});
    if (w == NULL) {
        ui_deinit_context(&ctx);
        return 1;
    }

    while (ui_pump_events(&ctx)) {
        UI_WINDOW(w, {
            UI_COLUMN(w, UINT32_MAX, UINT32_MAX, {
                ui_spacer(w, UI_SPACER_VERTICAL);
                ui_text(w, "MONOLITH Project", UI_TEXT_CENTER | UI_TEXT_FILL_HORIZONTAL);
                ui_text(w, "A hobby operating system", UI_TEXT_CENTER | UI_TEXT_FILL_HORIZONTAL);
                ui_spacer(w, UI_SPACER_VERTICAL);
            });
        });
    }

    ui_deinit_context(&ctx);
    return 0;
}
