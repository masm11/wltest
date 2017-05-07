#define WL_EGL_PLATFORM
#include <stdio.h>
#include <gtk/gtk.h>
#include <EGL/egl.h>
#include <gdk/gdkwayland.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <cairo/cairo-gl.h>
#include <GLES2/gl2.h>
#include <assert.h>
#include <stdlib.h>

static GtkWidget *drawable;

static EGLDisplay dpy = 0;
static EGLSurface sfc;
static EGLContext ctxt;

#define CHECK_GL_ERROR() check_gl_error(__FILE__, __LINE__)
static void check_gl_error(const char *file, int lineno)
{
    int err = glGetError();
    if (err == GL_NO_ERROR)
	return;
    printf("%s:%d: err=0x%08x.\n", file, lineno, err);
    exit(1);
}

#define CHECK_EGL_ERROR() check_egl_error(__FILE__, __LINE__)
static void check_egl_error(const char *file, int lineno)
{
    int err = eglGetError();
    if (err == EGL_SUCCESS)
	return;
    printf("%s:%d: err=0x%08x.\n", file, lineno, err);
    exit(1);
}

static void init_egl(void)
{
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    CHECK_EGL_ERROR();
    
    int major, minor;
    if (!eglInitialize(dpy, &major, &minor)) {
	printf("eglInitialize() failed.\n");
	exit(1);
    }
    CHECK_EGL_ERROR();
    printf("%d.%d\n", major, minor);
    
    int attribs[] = {
	EGL_RED_SIZE,
	8,
	EGL_GREEN_SIZE,
	8,
	EGL_BLUE_SIZE,
	8,
	EGL_ALPHA_SIZE,
	8,
	EGL_DEPTH_SIZE,
	8,
	EGL_SURFACE_TYPE,
	EGL_WINDOW_BIT,
	EGL_CONFORMANT,
	EGL_OPENGL_ES2_BIT,
	EGL_NONE,
    };
    EGLConfig config;
    int num_configs;
    if (!eglChooseConfig(dpy, attribs, &config, 1, &num_configs)) {
	printf("eglChooseConfig() failed.\n");
	exit(1);
    }
    CHECK_EGL_ERROR();
    
    struct wl_surface *native_window =
	    gdk_wayland_window_get_wl_surface(gtk_widget_get_window(drawable));
    sfc = eglCreateWindowSurface(dpy, config,
	    wl_egl_window_create(native_window, 400, 400),
	    NULL);
    CHECK_EGL_ERROR();  // EGL_BAD_ALLOC??
    
    ctxt = eglCreateContext(dpy, config, NULL, NULL);
    if (ctxt == EGL_NO_CONTEXT) {
	printf("eglCreateContext() failed.\n");
	exit(1);
    }
    CHECK_EGL_ERROR();
    
    
    const GLchar *vertex_shader_source = 
	    "attribute mediump vec4 attr_pos;"
	    "void main() {"
	    "    gl_Position = attr_pos;"
	    "}";
    // シェーダオブジェクトの作成
    int vert_shader = glCreateShader(GL_VERTEX_SHADER);
    CHECK_GL_ERROR();
    assert(vert_shader != 0);
    
    glShaderSource(vert_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vert_shader);
    
    // コンパイルエラーをチェックする
    {
	GLint compileSuccess = 0;
	glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &compileSuccess);
	if (compileSuccess == GL_FALSE) {
	    // エラーが発生した
	    GLint infoLen = 0;
	    // エラーメッセージを取得
	    glGetShaderiv(vert_shader, GL_INFO_LOG_LENGTH, &infoLen);
	    if (infoLen > 1) {
		GLchar *message = (GLchar*) calloc(infoLen, sizeof(GLchar));
		glGetShaderInfoLog(vert_shader, infoLen, NULL, message);
		printf("%s\n", message);
		free((void*) message);
	    } else {
		printf("comple error not info...\n");
	    }
	}
	assert(compileSuccess == GL_TRUE);
    }

    const GLchar *fragment_shader_source = 
	    "void main() {"
	    "    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);"
	    "}";
    // シェーダーオブジェクトを作成する
    int frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    assert(frag_shader != 0);
    glShaderSource(frag_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(frag_shader);
    
    // コンパイルエラーをチェックする
    {
	GLint compileSuccess = 0;
	glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &compileSuccess);
	if (compileSuccess == GL_FALSE) {
	    // エラーが発生した
	    GLint infoLen = 0;
	    // エラーメッセージを取得
	    glGetShaderiv(frag_shader, GL_INFO_LOG_LENGTH, &infoLen);
	    if (infoLen > 1) {
		GLchar *message = (GLchar*) calloc(infoLen, sizeof(GLchar));
		glGetShaderInfoLog(frag_shader, infoLen, NULL, message);
		printf("%s\n", message);
		free((void*) message);
	    } else {
		printf("comple error not info...\n");
	    }
	}
	assert(compileSuccess == GL_TRUE);
    }
    
    int shader_program = glCreateProgram();
    assert(shader_program != 0);
    
    // 頂点シェーダーとプログラムを関連付ける
    glAttachShader(shader_program, vert_shader);
    assert(glGetError() == GL_NO_ERROR);
    // フラグメントシェーダーとプログラムを関連付ける
    glAttachShader(shader_program, frag_shader);
    assert(glGetError() == GL_NO_ERROR);
    
    // リンク処理を行う
    glLinkProgram(shader_program);
    
    // リンクエラーをチェックする
    {
	GLint linkSuccess = 0;
	glGetProgramiv(shader_program, GL_LINK_STATUS, &linkSuccess);
	
	if (linkSuccess == GL_FALSE) {
	    // エラーが発生したため、状態をチェックする
	    GLint infoLen = 0;
	    // エラーメッセージを取得
	    glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &infoLen);
	    if (infoLen > 1) {
		GLchar *message = (GLchar*) calloc(infoLen, sizeof(GLchar));
		glGetProgramInfoLog(shader_program, infoLen, NULL, message);
		printf("%s\n", message);
		free((void*) message);
	    }
	}
	
	// GL_NO_ERRORであることを検証する
	assert(linkSuccess == GL_TRUE);
    }
    
    int attr_pos = glGetAttribLocation(shader_program, "attr_pos");
    assert(attr_pos >= 0);
    
    glUseProgram(shader_program);
    
    
    eglMakeCurrent(dpy, sfc, sfc, ctxt);
}

static void draw(void)
{
    if (dpy == 0)
	init_egl();
    
    cairo_device_t *device = cairo_egl_device_create(dpy, ctxt);
    cairo_surface_t *surface = cairo_gl_surface_create_for_egl(device, sfc, 500, 500);
    cairo_t *cr = cairo_create(surface);
    
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    
    eglSwapBuffers(dpy, sfc);
    printf("%ld: draw\n", time(NULL));
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    cairo_device_destroy(device);
}

static gboolean draw_cb(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
/*
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 40.0);
    
    cairo_move_to(cr, 10.0, 50.0);
    cairo_show_text(cr, "test");
*/
    draw();
    return FALSE;
}

static gboolean timeout_cb(gpointer user_data)
{
    draw();
    return G_SOURCE_CONTINUE;
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    
    GtkWidget *toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_show(toplevel);
    
    drawable = gtk_drawing_area_new();
    gtk_widget_show(drawable);
    gtk_widget_set_size_request(drawable, 500 ,500);
    gtk_container_add(GTK_CONTAINER(toplevel), drawable);
    
    g_signal_connect(drawable, "draw", G_CALLBACK(draw_cb), NULL);
    g_timeout_add(100, timeout_cb, NULL);
    
    gtk_main();
    
    return 0;
}
