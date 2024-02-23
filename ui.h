#pragma once

void ui_translate(float x, float y);
void ui_begin_frame();
void ui_end_frame();
bool ui_init(int width, int height);
void ui_render();
void ui_update();
void ui_cleanup();

bool ui_label(const char *fmt, ...);
bool ui_button(const char *label);
bool ui_checkbox(const char *label, bool *out_cond);
bool ui_text(const char *label, char *out_text, size_t out_text_length);