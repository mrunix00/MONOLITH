/*
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/libui.h>
#include <libui/widgets/button.h>
#include <libui/widgets/column.h>
#include <libui/widgets/container.h>
#include <libui/widgets/row.h>
#include <libui/widgets/text.h>
#include <libui/widgets/textbox.h>
#include <string.h>

int main()
{
    ui_ctx_t ctx = {0};
    if (!ui_init_context(&ctx))
        return 1;

    static char list[256][256] = {0};
    size_t list_count = 0;

    ui_wctx_t *w = ui_new_window(&ctx, "UIDemo", 300, 200, (window_flags_t){.resizable = true});
    if (w == NULL) {
        ui_deinit_context(&ctx);
        return 1;
    }

    char textbox_buffer[256] = {0};
    while (ui_pump_events(&ctx)) {
        UI_WINDOW(w, {
            UI_COLUMN(w, -1, -1, {
                ui_text(w, "Hello, World!", UI_TEXT_LEFT | UI_TEXT_WRAP);
                ui_text(w, "This is libui on MONOLITH", UI_TEXT_LEFT | UI_TEXT_WRAP);

                UI_CONTAINER(w, "uidemo.list", -1, -1, {
                    for (size_t i = 0; i < list_count; i++)
                        ui_text(w, list[i], UI_TEXT_LEFT);
                });

                bool submit;
                ui_widget_event_t button_state;
                UI_ROW(w, -1, 0, {
                    submit = ui_textbox(w, "Text Input", textbox_buffer, sizeof(textbox_buffer));
                    button_state = ui_button(w, "Submit");
                });

                if (submit || (button_state & UI_WIDGET_EVENT_LCLICKED)) {
                    strncpy(list[list_count], textbox_buffer, sizeof(list[list_count]) - 1);
                    list_count++;
                    textbox_buffer[0] = '\0';
                    ui_set_container_scroll(w, "uidemo.list", UI_AXIS_Y, -1);
                }
            });
        });
    }

    ui_deinit_context(&ctx);
    return 0;
}
