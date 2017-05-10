#define WL_EGL_PLATFORM 1
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
static int shader_program;

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

static void init_shaders(void)
{
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
    
    shader_program = glCreateProgram();
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
}

static void init_egl(void)
{
    struct wl_display *native_dpy
	    = gdk_wayland_display_get_wl_display(
		    gdk_window_get_display(
			    gtk_widget_get_window(drawable)
			));
    
    dpy = eglGetDisplay(native_dpy);
    if (dpy == EGL_NO_DISPLAY) {
	printf("eglGetDisplay() failed.\n");
	exit(1);
    }
    CHECK_EGL_ERROR();
    
    int major, minor;
    if (!eglInitialize(dpy, &major, &minor)) {
	CHECK_EGL_ERROR();
	printf("eglInitialize() failed.\n");
	exit(1);
    }
    printf("%d.%d\n", major, minor);
    
    int attribs[] = {
	EGL_RED_SIZE,		8,
	EGL_GREEN_SIZE,		8,
	EGL_BLUE_SIZE,		8,
	EGL_ALPHA_SIZE,		8,
	EGL_DEPTH_SIZE,		16,
	EGL_SURFACE_TYPE,	EGL_WINDOW_BIT,
	EGL_RENDERABLE_TYPE,	EGL_OPENGL_ES2_BIT,
	EGL_NONE,
    };
    EGLConfig config;
    int num_configs = -1;
    if (!eglChooseConfig(dpy, attribs, &config, 1, &num_configs)) {
	CHECK_EGL_ERROR();
	printf("eglChooseConfig() failed.\n");
	exit(1);
    }
    printf("num_configs=%d.\n", num_configs);
    int v = -1;
    if (!eglGetConfigAttrib(dpy, config, EGL_BUFFER_SIZE, &v)) {
	CHECK_EGL_ERROR();
	printf("eglGetConfigAttrib(buffer_size) failed.\n");
	exit(1);
    }
    printf("buffer_size=%d.\n", v);
    v = -1;
    if (!eglGetConfigAttrib(dpy, config, EGL_DEPTH_SIZE, &v)) {
	CHECK_EGL_ERROR();
	printf("eglGetConfigAttrib(depth_size) failed.\n");
	exit(1);
    }
    printf("depth_size=%d.\n", v);
    
    int context_attrib[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE,
    };
    ctxt = eglCreateContext(dpy, config, EGL_NO_CONTEXT, context_attrib);
    if (ctxt == EGL_NO_CONTEXT) {
	CHECK_EGL_ERROR();
	printf("eglCreateContext() failed.\n");
	exit(1);
    }
    
    struct wl_surface *native_window =
	    gdk_wayland_window_get_wl_surface(gtk_widget_get_window(drawable));
    printf("native_window=%p\n", native_window);
    struct wl_egl_window *win = wl_egl_window_create(native_window, 400, 400);
    printf("win=%p\n", win);
    sfc = eglCreateWindowSurface(dpy, config, win, NULL);
    if (sfc == EGL_NO_SURFACE) {
	CHECK_EGL_ERROR();
	printf("eglCreateWindowSurface() failed.\n");
	exit(1);
    }
    
    eglMakeCurrent(dpy, sfc, sfc, ctxt);
    
    init_shaders();
}

#if 0
typedef struct { float v[3]; } vec3;
typedef struct { float v[4][4]; } mat4;

static mat4 rotate(mat4 cur, float angle, vec3 axis)
{
    return cur;
}

#if 0
template<typename T>
GLuint createBufferObject( std::vector<T>& src, GLuint type ){
    GLuint vbo;
    glGenBuffers( 1, &vbo );
    glBindBuffer( type, vbo );
    glBufferData( type, sizeof(T) * src.size(), src.data(), GL_STATIC_DRAW );
    return vbo;
}

static void CreateResource() {
    drawObj.shader = createShaderProgram( srcVertexShader, srcFragmentShader );
    GLint locPos = glGetAttribLocation( drawObj.shader, "position0" );
    GLint locNrm = glGetAttribLocation( drawObj.shader, "normal0" );
    
    locPVW = glGetUniformLocation( drawObj.shader, "matPVW" );
    
    std::vector<uint16_t> indicesTorus;
    std::vector<VertexPN> verticesTorus;
    createTorus( indicesTorus, verticesTorus );
    drawObj.vb = createBufferObject( verticesTorus, GL_ARRAY_BUFFER );
    drawObj.ib = createBufferObject( indicesTorus, GL_ELEMENT_ARRAY_BUFFER );
    drawObj.indexCount = indicesTorus.size();
    
    char* offset = NULL;
    int stride = sizeof(VertexPN);
    glVertexAttribPointer( locPos, 3, GL_FLOAT, GL_FALSE, stride, offset );
    offset += sizeof(VertexPosition);
    glVertexAttribPointer( locNrm, 3, GL_FLOAT, GL_FALSE, stride, offset );
    offset += sizeof(VertexNormal);
    
    glEnableVertexAttribArray( locPos );
    glEnableVertexAttribArray( locNrm );
}
#endif

static void draw_cube(void)
{
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    static double angle = 0.0f;
    angle += .01f;
    if (angle > 3600.0)
	angle -= 3600.0;
    
    vec3 cameraPos = { { 0.0, 0.0f, 10.0f } };
    // mat4 proj = glm::perspective<float>(30.0f, (float) width / (float) height, 1.0f, 100.0f);
    // glm::mat4 view = glm::lookAt<float>(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    mat4 world = { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } } };
    vec3 axis_x = { { 1, 0, 0 } };
    vec3 axis_y = { { 0, 1, 0 } };
    vec3 axis_z = { { 0, 0, 1 } };
    world = rotate(world, (float) angle, axis_y);
    world = rotate(world, (float) angle * 0.5f, axis_z);
    world = rotate(world, (float) angle * 0.5f, axis_x);
    glUseProgram(shader_program);
    
    mat4 pvw = world;
    glUniform4fv(locPVW, 4, pvw.v[0]);
    
    glBindBuffer(GL_ARRAY_BUFFER, drawObj.vb);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawObj.ib);
    
    glDrawElements(GL_TRIANGLES, drawObj.indexCount, GL_UNSIGNED_SHORT, NULL);
}
#endif

static void draw(void)
{
    if (dpy == 0)
	init_egl();
    
/*
    cairo_device_t *device = cairo_egl_device_create(dpy, ctxt);
    cairo_surface_t *surface = cairo_gl_surface_create_for_egl(device, sfc, 500, 500);
    cairo_t *cr = cairo_create(surface);
*/
    
    glClearColor(0, 0, 0, 1);
    CHECK_GL_ERROR();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    CHECK_GL_ERROR();
    // glViewport(0, 0, 400, 400);
    CHECK_GL_ERROR();
    
    eglSwapBuffers(dpy, sfc);
    CHECK_GL_ERROR();
    printf("%ld: draw, ", time(NULL));
    fflush(stdout);
    
/*
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    cairo_device_destroy(device);
*/
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
    gtk_gl_area_queue_render(GTK_GL_AREA(drawable));
    return G_SOURCE_CONTINUE;
}

static gboolean render(GtkGLArea *area, GdkGLContext *context)
{
    static float v = 0.0f;
    glClearColor(v, v, v, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if ((v += 0.01f) >= 1.0f)
	v -= 1.0f;
    
    return TRUE;
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    
    GtkWidget *toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_show(toplevel);
    
#if 0
    drawable = gtk_drawing_area_new();
    gtk_widget_show(drawable);
    gtk_widget_set_size_request(drawable, 500 ,500);
    gtk_container_add(GTK_CONTAINER(toplevel), drawable);
    
    g_signal_connect(drawable, "draw", G_CALLBACK(draw_cb), NULL);
    g_timeout_add(100, timeout_cb, NULL);
#endif
    
    drawable = gtk_gl_area_new();
    gtk_gl_area_set_use_es(GTK_GL_AREA(drawable), TRUE);
    gtk_gl_area_set_required_version(GTK_GL_AREA(drawable), 2, 0);
    gtk_gl_area_set_has_alpha(GTK_GL_AREA(drawable), TRUE);
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(drawable), TRUE);
    gtk_gl_area_set_auto_render(GTK_GL_AREA(drawable), FALSE);
    g_signal_connect(drawable, "render", G_CALLBACK(render), NULL);
    gtk_widget_show(drawable);
    gtk_widget_set_size_request(drawable, 500 ,500);
    gtk_container_add(GTK_CONTAINER(toplevel), drawable);

    g_timeout_add(100, timeout_cb, NULL);
    
    gtk_main();
    
    return 0;
}
