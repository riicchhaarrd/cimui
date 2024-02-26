#pragma once

typedef struct
{
	float x, y;
} UIVec2;

typedef struct
{
	float border_thickness;
	float border_color[4];
	float margin;
	float width;
	float height;
	float max_width;
	float max_height;
	float padding_x;
	float padding_y;
	float background_color[4];
	float text_color[4];
	// float text_color_hover[4];
	// float background_color_hover[4];
} UIStyleProps;

typedef struct
{
	// UIStyleProps selectors[...];
	UIStyleProps initial; // Default
	UIStyleProps hovered;
	UIStyleProps active;
	UIStyleProps focused;
} UIStyle;

typedef struct
{
	float translation[2];
	float rotation;
	float scale[2];
} UITransform;

// Leave push/pop stack implementations up to caller
void ui_save_transform(UITransform*);
void ui_restore_transform(UITransform*);

void ui_translate(float x, float y);
void ui_begin_frame();
void ui_end_frame();
bool ui_init(int width, int height);
void ui_render();
void ui_update();
void ui_cleanup();

void ui_sameline();
void ui_label(const char *fmt, ...);

bool ui_button_ex(const char *label, UIVec2 size);
static bool ui_button(const char *label)
{
	return ui_button_ex(label, (UIVec2) { 0.f, 0.f });
}
bool ui_checkbox_ex(const char *label, bool *out_cond, UIVec2 size);
static bool ui_checkbox(const char *label, bool *out_cond)
{
	return ui_checkbox_ex(label, out_cond, (UIVec2) { 0.f, 0.f });
}
bool ui_text_ex(const char *label, char *out_text, size_t out_text_length, UIVec2 size);
static bool ui_text(const char *label, char *out_text, size_t out_text_length)
{
	return ui_text_ex(label, out_text, out_text_length, (UIVec2) { 0.f, 0.f });
}
bool ui_integer_ex(const char *label, int *, UIVec2 size);
static bool ui_integer(const char *label, int *out_value)
{
	return ui_integer_ex(label, out_value, (UIVec2) { 0.f, 0.f });
}
bool ui_float_ex(const char *label, float *, UIVec2 size);
static bool ui_float(const char *label, float *out_value)
{
	return ui_float_ex(label, out_value, (UIVec2) { 0.f, 0.f });
}
bool ui_image(unsigned int image_id, UIVec2 size);
unsigned int ui_load_image(const char *path);
bool ui_image_from_path(const char *path, unsigned int *image_id, UIVec2 size);