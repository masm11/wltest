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


static const char *srcVertexShader =
	"attribute vec4 position0;\n"
	"attribute vec3 normal0;\n"
	"varying vec4 vsout_color0;\n"
	"uniform vec4 matPVW[4];\n"
	"void main() {\n"
	"  vec4 pos;\n"
	"  pos = matPVW[0] * position0.xxxx;\n"
	"  pos += matPVW[1]* position0.yyyy;\n"
	"  pos += matPVW[2]* position0.zzzz;\n"
	"  pos += matPVW[3]* position0.wwww;\n"
	"  gl_Position = pos;\n"
	"  float lmb = clamp( dot( vec3(0.0, 0.5, 0.5), normalize(normal0.xyz)), 0.f, 1.f );\n"
	"  lmb = lmb * 0.5 + 0.5;\n"
	"  vsout_color0.rgb = vec3(lmb,lmb,lmb);\n"
	"  vsout_color0.a = 1.0;\n"
	"}";

static const char *srcFragmentShader =
	// "precision mediump float;\n"
	"varying vec4 vsout_color0;\n"
	"void main() {\n"
	"  gl_FragColor = vsout_color0;\n"
	"}";

static void checkCompiled(int shader)
{
    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
	GLint length;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
	if (length) {
	    char *buf = malloc(length);
	    glGetShaderInfoLog(shader, length, NULL, buf);
	    fprintf(stderr, "CompileLog: %s\n", buf);
	}
	exit(EXIT_FAILURE);
    }
    fprintf(stdout, "Compile Succeed.\n");
}

static int createShaderProgram(const char *srcVS, const char *srcFS)
{
    int shaderVS = glCreateShader(GL_VERTEX_SHADER);
    int shaderFS = glCreateShader(GL_FRAGMENT_SHADER);
    
    fprintf(stderr, "vertex shader.\n");
    glShaderSource(shaderVS, 1, &srcVS, 0);
    int errCode = glGetError();
    if (errCode != GL_NO_ERROR) {
	fprintf(stderr, "GLErr.  %X\n", errCode);
	exit(1);
    }
    
    glCompileShader(shaderVS);
    checkCompiled(shaderVS);
    
    fprintf(stderr, "fragment shader.\n");
    glShaderSource(shaderFS, 1, &srcFS, NULL);
    glCompileShader(shaderFS);
    checkCompiled(shaderFS);
    
    int program = glCreateProgram();
    glAttachShader(program, shaderVS);
    glAttachShader(program, shaderFS);
    
    glLinkProgram(program);
    
    return program;
}

struct VertexPosition {
    float x, y, z;
};
struct VertexNormal {
    float nx, ny, nz;
};

struct VertexPN {
    struct VertexPosition Position;
    struct VertexNormal Normal;
};

static const double PI = 3.14159265;

#define TORUS_N 20

static void create_torus(uint16_t *indices, struct VertexPN *vertices)
{
    float radius = 3.0f;
    float minorRadius = 1.0f;
    int n = TORUS_N;
    
    int idx = 0;
    for (int i = 0; i <= n; i++) {
	double ph = PI * 2.0 * i / n;
	double r = cos(ph) * minorRadius;
	double y = sin(ph) * minorRadius;
	
	for (int j = 0; j <= n; j++) {
	    double th = 2.0 * PI * j / n;
	    float x = (r + radius) * cos(th);
	    float z = (r + radius) * sin(th);
	    
	    struct VertexPN v;
	    v.Position.x = x;
	    v.Position.y = (float) y;
	    v.Position.z = z;
	    
	    float nx = r * cos(th);
	    float ny = y;
	    float nz = r * sin(th);
	    v.Normal.nx = nx;
	    v.Normal.ny = ny;
	    v.Normal.nz = nz;
	    
	    vertices[idx++] = v;
	}
    }
    
    idx = 0;
    for(int i = 0; i < n; i++) {
	for(int j = 0; j < n; j++) {
	    int index = (n + 1) * j + i;
	    indices[idx++] = index;
	    indices[idx++] = index + n + 2;
	    indices[idx++] = index + 1;
	    
	    indices[idx++] = index;
	    indices[idx++] = index + n + 1;
	    indices[idx++] = index + n + 2;
	}
    }
}

static struct {
    int vb, ib;
    int shader;
    int indexCount;
} drawObj;

static GLint locPVW;

static void CreateResource(void)
{
    drawObj.shader = createShaderProgram(srcVertexShader, srcFragmentShader);
    GLint locPos = glGetAttribLocation(drawObj.shader, "position0");
    GLint locNrm = glGetAttribLocation(drawObj.shader, "normal0");
    
    locPVW = glGetUniformLocation(drawObj.shader, "matPVW");
    
    uint16_t indices_torus[TORUS_N * TORUS_N * 6];
    struct VertexPN vertices_torus[(TORUS_N + 1) * (TORUS_N + 1)];
    create_torus(indices_torus, vertices_torus);
    glGenBuffers(1, &drawObj.vb);
    glBindBuffer(GL_ARRAY_BUFFER, drawObj.vb);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices_torus, vertices_torus, GL_STATIC_DRAW);
    glGenBuffers(1, &drawObj.ib);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawObj.ib);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices_torus, indices_torus, GL_STATIC_DRAW);
    drawObj.indexCount = TORUS_N * TORUS_N * 6;
    
    char *offset = NULL;
    int stride = sizeof(struct VertexPN);
    glVertexAttribPointer(locPos, 3, GL_FLOAT, GL_FALSE, stride, offset);
    offset += sizeof(struct VertexPosition);
    glVertexAttribPointer(locNrm, 3, GL_FLOAT, GL_FALSE, stride, offset);
    offset += sizeof(struct VertexNormal);
    
    glEnableVertexAttribArray(locPos);
    glEnableVertexAttribArray(locNrm);
}

static void DestroyResource(void)
{
    glDeleteBuffers(1, &drawObj.vb);
    glDeleteBuffers(1, &drawObj.ib);
    glDeleteProgram(drawObj.shader);
}

struct vec3 {
    float v[3];
};

struct mat4 {
    float v[4][4];
};

static void drawCube(int width, int height)
{
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    static double angle = 0;
    if ((angle += 0.01) > 3600)
	angle -= 3600;
    
#if 0
    struct vec3 cameraPos = {
	{ 0.0, 0.0f, 10.0f },
    };
    glm::mat4 proj = glm::perspective<float>( 30.0f, float(width)/float(height), 1.0f, 100.0f );
    glm::mat4 view = glm::lookAt<float>( cameraPos, glm::vec3(0.0f,0.0f,0.0f), glm::vec3(0.0f,1.0f,0.0f) );
    glm::mat4 world= glm::rotate( glm::mat4(1.0f), (float)angle, glm::vec3(0.0f,0.0f,1.0f) );
    world= glm::rotate( world, (float) angle * 0.5f, glm::vec3( 0.0f, 0.0f, 1.0f ) );
    world= glm::rotate( world, (float) angle * 0.5f, glm::vec3( 1.0f, 0.0f, 0.0f ));
#endif
    glUseProgram(drawObj.shader);
    
    // glm::mat4 pvw = proj * view * world;
    struct mat4 pvw = {
	{
	    { 1, 0, 0, 0 },
	    { 0, 1, 0, 0 },
	    { 0, 0, 1, -10 },
	    { 0, 0, 0, 1 },
	},
    };
    glUniform4fv(locPVW, 4, (float *) &pvw);
    
    glBindBuffer(GL_ARRAY_BUFFER, drawObj.vb);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawObj.ib);
    
    glDrawElements(GL_TRIANGLES, drawObj.indexCount, GL_UNSIGNED_SHORT, NULL);
}


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

static int create_texture(void)
{
    static unsigned char data[16 * 16 * 4];
    int tex;

    for (int y = 0; y < 16; y++) {
	for (int x = 0; x < 16; x++) {
	    data[(y * 16 + x) * 4 + 0] = rand();
	    data[(y * 16 + x) * 4 + 1] = rand();
	    data[(y * 16 + x) * 4 + 2] = rand();
	    data[(y * 16 + x) * 4 + 3] = rand();
	}
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // glEnable(GL_SCISSOR_TEST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    return tex;
}

static gboolean timeout_cb(gpointer user_data)
{
    gtk_gl_area_queue_render(GTK_GL_AREA(drawable));
    return G_SOURCE_CONTINUE;
}

static gboolean render(GtkGLArea *area, GdkGLContext *context)
{
    static int tex = 0;
    if (tex == 0) {
	tex = create_texture();
	CreateResource();
    }
    
    static float v = 0.0f;
    glClearColor(v, v, v, 1);
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if ((v += 0.01f) >= 1.0f)
	v -= 1.0f;
    
    drawCube(400, 400);
    
    CHECK_GL_ERROR();
    
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
