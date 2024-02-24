#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <malloc.h>
#include "io.h"
#include <linmath.h>
#include "hash.h"
#include "hash_table.h"
#include "opengl.h"
#include "ui.h"
#include "util.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <SDL.h>

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
	//float text_color_hover[4];
	//float background_color_hover[4];
} UIStyleProps;

typedef struct
{
	//UIStyleProps selectors[...];
	UIStyleProps initial; // Default
	UIStyleProps hovered;
	UIStyleProps active;
	UIStyleProps focused;
} UIStyle;

enum
{
	k_EUIStyleSelectorDefault,
	k_EUIStyleSelectorInput,
	k_EUIStyleSelectorMax
} k_EUIStyleSelector;

typedef struct UIFont_s
{
	char path[256];
	int font_size;
	unsigned char *ttf_buffer;
	GLuint gl_texture;
	stbtt_bakedchar cdata[96];
	stbtt_fontinfo font_info;
	float height;
} UIFont;

typedef struct
{
	bool pressed;
} UIButtonElement;

typedef struct
{
	bool *state;
} UICheckboxElement;

typedef struct
{
	char *out_string;
	size_t out_string_length;
} UITextElement;

typedef enum
{
	k_EUIElementTypeNone,
	k_EUIElementTypeButton,
	k_EUIElementTypeCheckbox,
	k_EUIElementTypeLabel,
	k_EUIElementTypeText,
	k_EUIElementTypePane,
	k_EUIElementTypeFrame,
	k_EUIElementTypeMax
} k_EUIElementType;

typedef struct
{
	float x, y, w, h;
} UIRectangle;

typedef struct
{
	size_t index;
	k_EUIElementType type;
	// uint64_t id;
	char label[256];
	union
	{
		UIButtonElement button;
		UITextElement text;
		UICheckboxElement checkbox;
	} u;
	UIRectangle rect;
	UIStyleProps style;
	float content_width, content_height;
} UIElement;

typedef struct
{
	int buttons[3]; // Left, Middle, Right
	int x, y;
} UIMouseState;

typedef struct
{
	float x, y;
	UIFont *default_font;
	GLuint gl_program;
	UIElement *elements;
	size_t numelements;
	size_t maxelements;
	int width, height;
	GLuint white_texture;
	UIMouseState mouse, mouse_prev_frame;
	bool scan_code_state[SDL_NUM_SCANCODES];
	bool interact_active;

	UIStyle styles[k_EUIStyleSelectorMax];

	char *active_text_input;
	size_t max_active_text_input_length;
	bool text_input_changed;
	int selection_beg, selection_end;
	int caret_pos;
} UIContext;
static UIContext ui_ctx;

static const char *vertex_shader_source = "#version 300 es\n\
layout(location = 0) in vec2 position;\n\
layout(location = 1) in vec2 texCoord;\n\
out vec2 v_texCoord;\n\
uniform mat4 projection;\n\
uniform mat4 model;\n\
void main()\n\
{\n\
    gl_Position = projection * model * vec4(position, 0.0, 1.0);\n\
    v_texCoord = texCoord;\n\
}\n";
static const char *fragment_shader_source = "#version 300 es\nprecision mediump float;\n\
in vec2 v_texCoord;\n\
uniform sampler2D s_texture;\n\
uniform vec4 textColor;\n\
void main() {\n\
    float alpha = texture2D(s_texture, v_texCoord).r;\n\
    gl_FragColor = textColor * alpha;\n\
}";

//#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
UIFont *ui_load_font(const char *path)
{
	UIFont *font = calloc(1, sizeof(UIFont));
	font->font_size = 18;
	snprintf(font->path, sizeof(font->path), path);

	if(k_EIOResultOk != io_read_binary_file(font->path, &font->ttf_buffer, NULL, NULL))
	{
		printf("Can't load font '%s'\n", font->path);
		return NULL;
	}
	stbtt_InitFont(&font->font_info, font->ttf_buffer, 0);

	int ascent, descent, line_gap;
	stbtt_GetFontVMetrics(&font->font_info, &ascent, &descent, &line_gap);
	float scale = stbtt_ScaleForPixelHeight(&font->font_info, font->font_size);
	font->height = (ascent - descent + line_gap) * scale;

	unsigned char image[512 * 512];
	stbtt_BakeFontBitmap(font->ttf_buffer, 0, font->font_size, image, 512, 512, 32, 96, font->cdata); // no guarantee this fits!

	#if 0
	if(!stbi_write_png("bitmap_font.png", 512, 512, 1, image, 0))
	{
		printf("Error writing PNG file.\n");
	}
	#endif
	glGenTextures(1, &font->gl_texture);
	glBindTexture(GL_TEXTURE_2D, font->gl_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, image);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	return font;
}
void ui_font_measure_text(UIFont *font, const char *beg, const char *end, float *width, float *height)
{
	if(width)
	{
		*width = 0.f;
	}
	#if 0
	if(height)
	{
		*height = 0.f;
	}
	#endif

	float scale = stbtt_ScaleForPixelHeight(&font->font_info, font->font_size);
	#if 0
	int ascent, descent, lineGap;
	stbtt_GetFontVMetrics(&font->font_info, &ascent, &descent, &lineGap);
	if(height)
	{
		*height = (ascent - descent + lineGap) * scale;
	}
	#endif
	float x, y;
	while(1)
	{
		if(end)
		{
			if(beg == end)
				break;
		}
		else
		{
			if(!*beg)
				break;
		}
		if(*beg >= 32 && *beg < 128)
		{
			int advance, lsb, x0, y0, x1, y1;
			stbtt_GetCodepointHMetrics(&font->font_info, *beg, &advance, &lsb);
			stbtt_GetCodepointBitmapBox(&font->font_info, *beg, 1, 1, &x0, &y0, &x1, &y1);
			if(width)
			{
				*width += advance * scale;
			}

			stbtt_aligned_quad q;
			stbtt_GetBakedQuad(font->cdata, 512, 512, *beg - 32, &x, &y, &q, 1);
#if 0
			if(width)
			{
				*width += (q.x1 - q.x0);
			}
#endif
			if(height)
			{
				float h = (q.y1 - q.y0);
				if(*height == 0.f || h > *height)
				{
					*height = h;
				}
			}
		}
		++beg;
	}
}
static UIElement *ui_new_element_(k_EUIElementType type)
{
	if(ui_ctx.numelements >= ui_ctx.maxelements)
	{
		size_t n = ui_ctx.maxelements * 2;
		if(n == 0)
			n = 7;
		ui_ctx.elements = realloc(ui_ctx.elements, sizeof(UIElement) * n);
		ui_ctx.maxelements = n;
	}
	UIElement *e = &ui_ctx.elements[ui_ctx.numelements++];
	memset(e, 0, sizeof(UIElement));
	e->type = type;
	e->index = ui_ctx.numelements - 1;
	//e->x = ui_ctx.x;
	//e->y = ui_ctx.y;
	return e;
}

bool ui_input_selection_valid()
{
	size_t n = strlen(ui_ctx.active_text_input);
	return ui_ctx.selection_beg >= 0 && ui_ctx.selection_end <= n && ui_ctx.selection_beg < ui_ctx.selection_end;
}
void ui_input_clear_selection()
{
	ui_ctx.selection_beg = -1;
	ui_ctx.selection_end = -1;
}
void ui_input_remove_selection()
{
	size_t n = strlen(ui_ctx.active_text_input);
	memmove(&ui_ctx.active_text_input[ui_ctx.selection_beg],
			&ui_ctx.active_text_input[ui_ctx.selection_end + 1],
			n + 1 - ui_ctx.selection_end);
	ui_ctx.caret_pos = 0;
	ui_input_clear_selection();
}

bool ui_event(SDL_Event *ev)
{
	if(SDL_GetRelativeMouseMode())
		return false;
	bool ctrl = ui_ctx.scan_code_state[SDL_SCANCODE_LCTRL] || ui_ctx.scan_code_state[SDL_SCANCODE_RCTRL];
	bool shift = ui_ctx.scan_code_state[SDL_SCANCODE_LSHIFT] || ui_ctx.scan_code_state[SDL_SCANCODE_RSHIFT];
	switch(ev->type)
	{
		case SDL_MOUSEMOTION:
			ui_ctx.mouse.x = ev->motion.x;
			ui_ctx.mouse.y = ev->motion.y;
			break;

		case SDL_MOUSEBUTTONDOWN: ui_ctx.mouse.buttons[0] = true; break;
		case SDL_MOUSEBUTTONUP: ui_ctx.mouse.buttons[0] = false; break;

		case SDL_KEYUP:
		{
			ui_ctx.scan_code_state[ev->key.keysym.scancode] = false;
		}
		break;
		case SDL_KEYDOWN:
		{
			ui_ctx.scan_code_state[ev->key.keysym.scancode] = true;
			switch(ev->key.keysym.sym)
			{
#if 0
				case SDLK_LEFT:
				case SDLK_RIGHT:
				{
					if(ui_ctx.active_text_input)
					{
						size_t maxchars = ui_ctx.max_active_text_input_length;
						size_t n = strlen(ui_ctx.active_text_input);
						if(n > 0)
						{
							if(shift)
							{
								if(ui_input_selection_valid())
								{
									if(ev->key.keysym.sym == SDLK_LEFT)
									{
										ui_ctx.selection_end = max(ui_ctx.selection_end + 1, n);
									}
								}
							}
							else
							{
								if(ev->key.keysym.sym == SDLK_LEFT)
								{
									ui_ctx.caret_pos = max(ui_ctx.caret_pos - 1, 0);
								}
								else
								{
									ui_ctx.caret_pos = min(ui_ctx.caret_pos + 1, n);
								}
							}
						}
					}
				}
				break;
#endif
				case SDLK_BACKSPACE:
				{
					if(ui_ctx.active_text_input)
					{
						size_t maxchars = ui_ctx.max_active_text_input_length;
						size_t n = strlen(ui_ctx.active_text_input);
						if(n > 0)
						{
							if(ui_input_selection_valid())
							{
								ui_input_remove_selection();
							}
							else
							{
								ui_ctx.active_text_input[n - 1] = 0;
							}
						}
						ui_ctx.text_input_changed = true;
					}
				}
				break;
				#if 0
				case SDLK_a:
				{
					if(ctrl && ui_ctx.active_text_input)
					{
						size_t maxchars = ui_ctx.max_active_text_input_length;
						size_t n = strlen(ui_ctx.active_text_input);
						if(n > 0)
						{
							ui_ctx.selection_beg = 0;
							ui_ctx.selection_end = n;
							ui_ctx.caret_pos = n;
						}
					}
				}
				break;
				#endif
			}
		}
		break;
		case SDL_TEXTINPUT:
		{
			if(!ctrl && ui_ctx.active_text_input)
			{
				size_t maxchars = ui_ctx.max_active_text_input_length;
				size_t n = strlen(ui_ctx.active_text_input);
				if(ui_input_selection_valid())
				{
					ui_input_remove_selection();
					n = strlen(ui_ctx.active_text_input);
				}
				strncat(ui_ctx.active_text_input, ev->text.text, maxchars - n - 1);
				ui_ctx.caret_pos = n;
			}
		}
		break;
	}
	return true;
}

void inherit_style(UIStyle *dst, UIStyle *src)
{
	if(dst == src)
	{
		dst->hovered = dst->initial;
		dst->active = dst->initial;
		dst->focused = dst->initial;
	}
	else
	{
		memcpy(dst, src, sizeof(UIStyle));
	}
}
SDL_Cursor *default_cursor;
SDL_Cursor *hand_cursor;
SDL_Cursor *text_cursor;
bool ui_init(int width, int height)
{
	default_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
	hand_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
	text_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);

	memset(&ui_ctx, 0, sizeof(UIContext));
	ui_ctx.default_font = ui_load_font("C:/Windows/Fonts/arial.ttf", ui_ctx.default_font);
	GLuint create_program(const char *path, const char *vs_source, const char *fs_source);
	ui_ctx.gl_program = create_program("#ui", vertex_shader_source, fragment_shader_source);
	ui_ctx.width = width;
	ui_ctx.height = height;
	glGenTextures(1, &ui_ctx.white_texture);
	glBindTexture(GL_TEXTURE_2D, ui_ctx.white_texture);
	static const unsigned char image[] = { 255, 255, 255, 255, 255, 255 };
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 2, 2, 0, GL_RED, GL_UNSIGNED_BYTE, image);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	{
		UIStyleProps *style = &ui_ctx.styles[k_EUIStyleSelectorDefault].initial;
		style->background_color[0] = 1.f;
		style->background_color[1] = 1.f;
		style->background_color[2] = 1.f;
		style->background_color[3] = 0.f;
		style->text_color[0] = 0.f;
		style->text_color[1] = 0.f;
		style->text_color[2] = 0.f;
		style->text_color[3] = 1.f;
		style->border_color[0] = 0.f;
		style->border_color[1] = 0.f;
		style->border_color[2] = 0.f;
		style->border_color[3] = 0.f;
		style->margin = 5.f;
		style->padding_y = 2.5f;
		style->padding_x = 10.f;
		style->border_thickness = 1.f;
		ui_ctx.styles[k_EUIStyleSelectorDefault].hovered = ui_ctx.styles[k_EUIStyleSelectorDefault].initial;
		ui_ctx.styles[k_EUIStyleSelectorDefault].active = ui_ctx.styles[k_EUIStyleSelectorDefault].initial;
		ui_ctx.styles[k_EUIStyleSelectorDefault].focused = ui_ctx.styles[k_EUIStyleSelectorDefault].initial;
		for(size_t i = 1; i < k_EUIStyleSelectorMax; ++i)
		{
			ui_ctx.styles[i] = ui_ctx.styles[k_EUIStyleSelectorDefault];
		}
	}

	{
		UIStyleProps *style = &ui_ctx.styles[k_EUIStyleSelectorInput].initial;
		style->background_color[3] = 1.f;
		style->border_color[0] = 143.f / 255.f;
		style->border_color[1] = 143.f / 255.f;
		style->border_color[2] = 157.f / 255.f;
		style->border_color[3] = 1.f;
		style->background_color[0] = 233.f / 255.f;
		style->background_color[1] = 233.f / 255.f;
		style->background_color[2] = 237.f / 255.f;
		style->background_color[3] = 1.f;
		style->text_color[0] = 0.f;
		style->text_color[1] = 0.f;
		style->text_color[2] = 0.f;
		style->text_color[3] = 1.f;
		ui_ctx.styles[k_EUIStyleSelectorInput].hovered = ui_ctx.styles[k_EUIStyleSelectorInput].initial;
		ui_ctx.styles[k_EUIStyleSelectorInput].active = ui_ctx.styles[k_EUIStyleSelectorInput].initial;
		ui_ctx.styles[k_EUIStyleSelectorInput].focused = ui_ctx.styles[k_EUIStyleSelectorInput].initial;
	}
	{
		UIStyleProps *style = &ui_ctx.styles[k_EUIStyleSelectorInput].hovered;
		style->background_color[0] = 180.f / 255.f;
		style->background_color[1] = 180.f / 255.f;
		style->background_color[2] = 184.f / 255.f;
	}
	{
		UIStyleProps *style = &ui_ctx.styles[k_EUIStyleSelectorInput].focused;
		style->border_color[0] = 0.f;
		style->border_color[1] = 0.f;
		style->border_color[2] = 1.f;
	}

	return true;
}

typedef struct
{
	float position[2];
	float texCoord[2];
} UIGLVertex;
static GLuint ui_vao = 0, ui_vbo = 0;

//TODO: text overflow?

bool ui_render_text_(UIFont *font, float *x, float *y, float x_max, const char *text, float *textcolor)
{
	glBindTexture(GL_TEXTURE_2D, font->gl_texture);

	mat4x4 proj;
	float aspect = (float)ui_ctx.width / (float)ui_ctx.height;

	mat4x4_identity(proj);
	mat4x4_ortho(proj, 0.f, (float)ui_ctx.width, (float)ui_ctx.height, 0.f, -(1 << 16), (1 << 16));
	//mat4x4_ortho(proj, -aspect, aspect, -1.f, 1.f, 1.f, -1.f);
	glUniformMatrix4fv(glGetUniformLocation(ui_ctx.gl_program, "projection"), 1, GL_FALSE, proj);

	mat4x4 identity;
	mat4x4_identity(identity);
	#if 0
	mat4x4 translation;
	mat4x4_translate(translation, x, y, 0.0f);

	mat4x4 scale;
	mat4x4_identity(scale);
	mat4x4_scale_aniso(scale, identity, 100.0f, 100.0f, 1.0f);

	mat4x4 model;
	mat4x4_mul(model, translation, scale);
	#endif


	glUniformMatrix4fv(glGetUniformLocation(ui_ctx.gl_program, "model"), 1, GL_FALSE, identity);
	glUniform4fv(glGetUniformLocation(ui_ctx.gl_program, "textColor"), 1, textcolor);
	float w = (float)ui_ctx.width;
	float h = (float)ui_ctx.height;
	bool overflow = false;
	while(*text)
	{
		if(*text >= 32 && *text < 128)
		{
			stbtt_aligned_quad q;
			stbtt_GetBakedQuad(font->cdata, 512, 512, *text - 32, x, y, &q, 1); // 1=opengl & d3d10+,0=d3d9
			if(x_max > 0.f && *x >= x_max)
			{
				overflow = true;
				break;
			}
			UIGLVertex vertices[] = { { q.x0, q.y0, q.s0, q.t0 }, { q.x1, q.y0, q.s1, q.t0 },
									  { q.x0, q.y1, q.s0, q.t1 },

									  { q.x1, q.y0, q.s1, q.t0 }, { q.x1, q.y1, q.s1, q.t1 },
									  { q.x0, q.y1, q.s0, q.t1 } };
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

			glDrawArrays(GL_TRIANGLES, 0, 6);
			// https://stackoverflow.com/questions/50185488/why-can-i-use-gl-quads-with-gldrawarrays
			// GL_INVALID_ENUM, newer OpenGL API doesn't support GL_QUADS anymore.
			// glDrawArrays(GL_QUADS, 0, 4);
		}
		++text;
	}

	//glDeleteBuffers(1, &vbo);
	//glDeleteVertexArrays(1, &vao);
	CHECK_GL_ERROR();
	return overflow;
}

bool ui_clicked()
{
	return !ui_ctx.mouse_prev_frame.buttons[0] && ui_ctx.mouse.buttons[0];
}

/*
bool ui_hovering_element()
{
	return false;
}*/
bool ui_mouse_test_rectangle(UIRectangle *rect)
{
	return (ui_ctx.mouse.x >= rect->x && ui_ctx.mouse.x <= rect->x + rect->w)
		   && (ui_ctx.mouse.y >= rect->y && ui_ctx.mouse.y <= rect->y + rect->h);
}

void ui_render_quad_(float x, float y, float width, float height, float *bgcolor)
{
	glBindTexture(GL_TEXTURE_2D, ui_ctx.white_texture);

	mat4x4 proj;
	float aspect = (float)ui_ctx.width / (float)ui_ctx.height;

	mat4x4_identity(proj);
	mat4x4_ortho(proj, 0.f, (float)ui_ctx.width, (float)ui_ctx.height, 0.f, -(1 << 16), (1 << 16));
	// mat4x4_ortho(proj, -aspect, aspect, -1.f, 1.f, 1.f, -1.f);
	glUniformMatrix4fv(glGetUniformLocation(ui_ctx.gl_program, "projection"), 1, GL_FALSE, proj);

	mat4x4 identity;
	mat4x4_identity(identity);
#if 1
	mat4x4 translation;
	mat4x4_translate(translation, x, y, 0.0f);

	mat4x4 scale;
	mat4x4_identity(scale);
	mat4x4_scale_aniso(scale, identity, width, height, 1.0f);

	mat4x4 model;
	mat4x4_mul(model, translation, scale);
#endif

	glUniformMatrix4fv(glGetUniformLocation(ui_ctx.gl_program, "model"), 1, GL_FALSE, model);
	glUniform4fv(glGetUniformLocation(ui_ctx.gl_program, "textColor"), 1, bgcolor);
	float w = (float)ui_ctx.width;
	float h = (float)ui_ctx.height;

	UIGLVertex vertices[] = { { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f },
							  { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 0.0f } };
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	// https://stackoverflow.com/questions/50185488/why-can-i-use-gl-quads-with-gldrawarrays
	// GL_INVALID_ENUM, newer OpenGL API doesn't support GL_QUADS anymore.
	// glDrawArrays(GL_QUADS, 0, 4);
	CHECK_GL_ERROR();
}
void ui_render_element_(UIElement *e)
{
	UIFont *font = ui_ctx.default_font;
	//bool hovering = ui_mouse_test_rectangle(&e->rect);
	static unsigned int blink_time = 0;
	unsigned int now = ticks();
	static bool draw_caret = false;
	if(now - blink_time >= 600)
	{
		draw_caret ^= 1;
		blink_time += 600;
	}
	
	UIStyleProps *props = &e->style;

	float x = e->rect.x;
	float y = e->rect.y;
	float w = e->rect.w;
	float h = e->rect.h;
	float content_x = x + props->border_thickness + props->padding_x / 2.f + props->margin / 2.f;
	float content_y = y + props->border_thickness + props->padding_y / 2.f + props->margin / 2.f;
	ui_render_quad_(x, y, w, h, props->border_color);

	ui_render_quad_(x + props->border_thickness,
					y + props->border_thickness,
					w - 2.0f * props->border_thickness,
					h - 2.0f * props->border_thickness,
					props->background_color);

	switch(e->type)
	{
		case k_EUIElementTypeButton:
		case k_EUIElementTypeLabel:
			content_y += e->content_height;
			ui_render_text_(font, &content_x, &content_y, 0.f, e->label, props->text_color);
			break;
		case k_EUIElementTypeText:
		{
			float value_y = content_y;
			content_y += e->content_height;
			ui_render_text_(font, &content_x, &content_y, 0.f, e->label, props->text_color);
			ui_render_text_(font, &content_x, &content_y, 0.f, ": ", props->text_color);
			if(e->u.text.out_string)
			{
				float value_x = content_x;
				#if 0
				float text_width = 0.f;
				ui_font_measure_text(ui_ctx.default_font,
									 e->u.text.out_string,
									 NULL,
									 &text_width,
									 NULL);
#endif
				if(!ui_render_text_(font,
									&content_x,
									&content_y,
									props->width + e->rect.x,
									e->u.text.out_string,
									props->text_color))
				{
					if(ui_ctx.active_text_input == e->u.text.out_string)
					{
						if(draw_caret && ui_ctx.caret_pos >= 0 && ui_ctx.caret_pos <= strlen(e->u.text.out_string))
						{
							float caret_x_offset = 0.f;
							ui_font_measure_text(ui_ctx.default_font,
												 e->u.text.out_string,
												 e->u.text.out_string + ui_ctx.caret_pos,
												 &caret_x_offset,
												 NULL);
							caret_x_offset += value_x - 1.f;
							ui_render_text_(font, &caret_x_offset, &content_y, 0.f, "|", props->text_color);
						}
					}
				}
				if(ui_ctx.selection_beg >= 0 && ui_ctx.selection_end <= strlen(e->u.text.out_string) && ui_ctx.selection_beg < ui_ctx.selection_end)
				{
					size_t n = ui_ctx.selection_end - ui_ctx.selection_beg;
					float selx = 0.f, sely = 0.f;
					ui_font_measure_text(ui_ctx.default_font,
										 e->u.text.out_string,
										 e->u.text.out_string + n,
										 &selx,
										 &sely);
					static const float selcol[] = { 0.f, 0.f, 1.f, 0.5f };
					ui_render_quad_(value_x, value_y, selx, e->content_height, selcol);

				}
			}
		} break;
		case k_EUIElementTypeCheckbox:
		{
			content_y += e->content_height;
			ui_render_text_(font, &content_x, &content_y, 0.f, e->label, props->text_color);
			ui_render_text_(font, &content_x, &content_y, 0.f, ": ", props->text_color);
			float sz = e->content_height;
			ui_render_quad_(content_x,
							content_y - sz,
							sz,
							sz,
							props->text_color);
			if(*e->u.checkbox.state)
			{
				ui_render_quad_(content_x + sz / 4.f,
								content_y - e->content_height + sz / 4.f,
								sz / 2.f,
								sz / 2.f,
								props->background_color);
			}
		} break;
	}
}

void ui_begin_frame()
{
	ui_ctx.interact_active = SDL_GetRelativeMouseMode();
	ui_ctx.x = 0;
	ui_ctx.y = 0;
	ui_ctx.numelements = 0;
}
void ui_end_frame()
{
	ui_ctx.text_input_changed = false;
	ui_ctx.mouse_prev_frame = ui_ctx.mouse;
	memset(ui_ctx.mouse.buttons, 0, sizeof(ui_ctx.mouse.buttons));
}

void ui_translate(float x, float y)
{
	ui_ctx.x = x;
	ui_ctx.y = y;
}

void ui_render()
{
	glUseProgram(ui_ctx.gl_program);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUniform1i(glGetUniformLocation(ui_ctx.gl_program, "s_texture"), 0);
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_DEPTH_TEST);
	if(ui_vao == 0)
	{
		glGenVertexArrays(1, &ui_vao);
		glBindVertexArray(ui_vao);
		if(ui_vbo == 0)
		{
			glGenBuffers(1, &ui_vbo);
		}
		glBindBuffer(GL_ARRAY_BUFFER, ui_vbo);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIGLVertex), (void *)offsetof(UIGLVertex, position));
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIGLVertex), (void *)offsetof(UIGLVertex, texCoord));
		glEnableVertexAttribArray(1);
	}
	glBindVertexArray(ui_vao);
	glBindBuffer(GL_ARRAY_BUFFER, ui_vbo);
	//float color[] = { 1.f, 0.f, 0.f, 1.f };
	//ui_render_quad_(ui_ctx.mouse.x, ui_ctx.mouse.y, 8.f, 8.f, color);
	char *active_text_input = ui_ctx.active_text_input;
	UIElement *active_element = NULL;
	UIElement *hovered_element = NULL;
	for(size_t i = 0; i < ui_ctx.numelements; ++i)
	{
		UIElement *e = &ui_ctx.elements[i];
		bool hovered = ui_mouse_test_rectangle(&e->rect);
		if(hovered)
		{
			hovered_element = e;
			if(ui_clicked())
			{
				active_element = e;
				if(e->type = k_EUIElementTypeText)
				{
					ui_ctx.active_text_input = e->u.text.out_string;
					ui_ctx.max_active_text_input_length = e->u.text.out_string_length;
					ui_ctx.caret_pos = 0;
					ui_ctx.text_input_changed = false;
					void ui_input_clear_selection();
					ui_input_clear_selection();
				}
			}
		}
		ui_render_element_(e);
	}
	if(hovered_element)
	{
		//TODO: add cursor to style props
		if(hovered_element->type == k_EUIElementTypeButton || hovered_element->type == k_EUIElementTypeCheckbox)
		{
			SDL_SetCursor(hand_cursor);
		}
		else if(hovered_element->type == k_EUIElementTypeText)
		{
			SDL_SetCursor(text_cursor);
		}
		else
		{
			SDL_SetCursor(default_cursor);
		}
	}
	else
	{
		SDL_SetCursor(default_cursor);
	}
	if(ui_clicked() && !active_element)
	{
		ui_ctx.active_text_input = NULL;
	}
	if(!active_text_input)
	{
		if(ui_ctx.active_text_input)
		{
			SDL_StartTextInput();
		}
		else
		{
			SDL_StopTextInput();
		}
	}
}
/*
void ui_update(UIContext *ctx)
{
}*/

void ui_cleanup()
{
	free(ui_ctx.default_font);
	ui_ctx.default_font = NULL;
}

void ui_element_content_measurements_(UIElement *e, float *w, float *h)
{
	*w = *h = 0.f;
	switch(e->type)
	{
		case k_EUIElementTypeButton:
		case k_EUIElementTypeLabel:
		{
			ui_font_measure_text(ui_ctx.default_font, e->label, NULL, w, h);
		} break;
		case k_EUIElementTypeCheckbox:
		{
			ui_font_measure_text(ui_ctx.default_font, e->label, NULL, w, h);
			*w += *h * 2.f;
		} break;
		case k_EUIElementTypeText:
		{
			float x;
			ui_font_measure_text(ui_ctx.default_font, e->label, NULL, &x, h);
			*w = x;
			ui_font_measure_text(ui_ctx.default_font, ": |", NULL, &x, h);
			*w += x;
			ui_font_measure_text(ui_ctx.default_font, e->u.text.out_string, NULL, &x, h);
			*w += x;
		} break;
	}
}

void ui_element_bounds_(UIElement *e)
{
	UIStyleProps *props = &e->style;
	if(props->width == 0.f)
	{
		props->width = e->content_width;
	}
	if(props->height == 0.f)
	{
		props->height = e->content_height;
	}
	e->rect.x = ui_ctx.x; // TODO?: margin-left: -10px
	e->rect.y = ui_ctx.y;
	e->rect.w = props->border_thickness * 2.f + props->padding_x + props->margin + props->width;
	e->rect.h = props->border_thickness * 2.f + props->padding_y + props->margin + props->height;
}

void ui_element_layout_next_(UIElement *e)
{
	ui_ctx.y += e->rect.h;
}

void ui_element_style_(UIElement *e, UIStyle *style)
{
	ui_element_content_measurements_(e, &e->content_width, &e->content_height);
	e->style = style->initial;
	ui_element_bounds_(e);
	if(ui_mouse_test_rectangle(&e->rect))
	{
		e->style = style->hovered;
		ui_element_bounds_(e);
	}
	if(e->type == k_EUIElementTypeText)
	{
		if(ui_ctx.active_text_input == e->u.text.out_string)
		{
			e->style = style->focused;
			ui_element_bounds_(e);
		}
	}
}

void ui_label(const char *fmt, ...)
{
	char text[256] = { 0 };
	if(fmt)
	{
		va_list va;
		va_start(va, fmt);
		vsnprintf(text, sizeof(text), fmt, va);
		va_end(va);
	}
	UIElement *e = ui_new_element_(k_EUIElementTypeLabel);
	snprintf(e->label, sizeof(e->label), "%s", text);
	UIStyle *style = &ui_ctx.styles[k_EUIStyleSelectorDefault];
	ui_element_style_(e, style);

	ui_element_layout_next_(e);
	return ui_clicked() && ui_mouse_test_rectangle(&e->rect);
}

bool ui_button(const char *label)
{
	//TODO: store label in hashmap
	//TODO: check previous frame input state for this element and return true if it was pressed
	UIElement *e = ui_new_element_(k_EUIElementTypeButton);
	snprintf(e->label, sizeof(e->label), "%s", label);
	UIStyle *style = &ui_ctx.styles[k_EUIStyleSelectorInput];
	ui_element_style_(e, style);

	ui_element_layout_next_(e);
	return ui_clicked() && ui_mouse_test_rectangle(&e->rect);
}

bool ui_text(const char *label, char *out_text, size_t out_text_length)
{
	UIElement *e = ui_new_element_(k_EUIElementTypeText);
	snprintf(e->label, sizeof(e->label), "%s", label);
	e->u.text.out_string = out_text;
	e->u.text.out_string_length = out_text_length;
	UIStyle *style = &ui_ctx.styles[k_EUIStyleSelectorInput];
	ui_element_style_(e, style);
	e->style.width = 100.f;
	ui_element_bounds_(e);

	ui_element_layout_next_(e);
	if(ui_ctx.active_text_input == out_text)
	{
		return ui_ctx.text_input_changed;
	}
	return false;
}

bool ui_checkbox(const char *label, bool *out_cond)
{
	UIElement *e = ui_new_element_(k_EUIElementTypeCheckbox);
	snprintf(e->label, sizeof(e->label), "%s", label);
	e->u.checkbox.state = out_cond;
	UIStyle *style = &ui_ctx.styles[k_EUIStyleSelectorInput];
	ui_element_style_(e, style);

	ui_element_layout_next_(e);
	bool pressed = ui_clicked() && ui_mouse_test_rectangle(&e->rect);
	if(pressed)
	{
		*out_cond ^= 1;
	}
	return pressed;
}