Example:


https://github.com/riicchhaarrd/cimui/assets/5922568/e0465134-9721-49b4-814c-f0132986eef5


```c

// Initialize
ui_init(800, 600);

// Core loop
ui_begin_frame();

ui_image_from_path("test.png", &img, (UIVec2) { 800.f / 2.f, 640.f / 2.f });
ui_translate(30.f, 30.f);
ui_label("Example label");
ui_sameline();
ui_label("FPS: %f, deltaTime: %f", fps, dt);
static float framerate = 30.f;
ui_float("Framerate", &framerate);
static float frame = 0.f;
frame += framerate * dt;
static bool view_camera_settings = false;
ui_checkbox("camera settings", &view_camera_settings);
if(view_camera_settings)
{
	ui_float("camera x", &camera.position[0]);
	ui_float("camera y", &camera.position[1]);
	ui_float("camera z", &camera.position[2]);
}
ui_float("frame", &frame);
static char text[64] = { 0 };
ui_text("Example text input", text, sizeof(text));
if(ui_button("Exit"))
{
	exit(0);
}

ui_end_frame();

// OpenGL rendering
ui_render();

// Uninitialize
ui_cleanup();

```
