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
#include <math.h>

static GtkWidget *drawable;

#define CHECK_GL_ERROR() check_gl_error(__FILE__, __LINE__)
static void check_gl_error(const char *file, int lineno)
{
    int err = glGetError();
    if (err == GL_NO_ERROR)
	return;
    printf("%s:%d: err=0x%08x.\n", file, lineno, err);
    exit(1);
}

#if 0
#define CHECK_EGL_ERROR() check_egl_error(__FILE__, __LINE__)
static void check_egl_error(const char *file, int lineno)
{
    int err = eglGetError();
    if (err == EGL_SUCCESS)
	return;
    printf("%s:%d: err=0x%08x.\n", file, lineno, err);
    exit(1);
}
#endif

static const char *srcVertexShader =
	"attribute vec4 position0;\n"
	"attribute vec3 normal0;\n"
	"attribute vec3 color0;\n"
	"varying vec4 vsout_color0;\n"
	"uniform mat4 matPVW;\n"
	"void main() {\n"
	"  gl_Position = matPVW * position0;\n"
	"  float lmb = clamp(dot(vec3(0.0, 0.5, 0.5), normalize(normal0.xyz)), 0.0f, 1.0f);\n"
	"  lmb = lmb * 0.5 + 0.5;\n"
	"  vsout_color0.rgb = color0;\n"
	"  vsout_color0.a = lmb;\n"
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

struct vec3 {
    float v[3];
};

struct vec4 {
    float v[4];
};

struct mat4 {
    float v[4][4];
};

static struct mat4 mat4_mul(struct mat4 s0, struct mat4 s1)
{
    struct mat4 d;
    
    for (int y = 0; y < 4; y++) {
	for (int x = 0; x < 4; x++) {
	    float sum = 0;
	    for (int i = 0; i < 4; i++)
		sum += s0.v[y][i] * s1.v[i][x];
	    d.v[y][x] = sum;
	}
    }
    
    return d;
}

static struct vec4 mat4_mul_vec4(struct mat4 m, struct vec4 v)
{
    struct vec4 r;
    for (int y = 0; y < 4; y++) {
	float sum = 0;
	for (int x = 0; x < 4; x++)
	    sum += m.v[y][x] * v.v[x];
	r.v[y] = sum;
    }
    return r;
}

static struct mat4 mat4_tr(struct mat4 s)
{
    struct mat4 d;
    for (int y = 0; y < 4; y++) {
	for (int x = 0; x < 4; x++)
	    d.v[y][x] = s.v[x][y];
    }
    return d;
}

struct VertexPosition {
    float x, y, z;
};
struct VertexNormal {
    float nx, ny, nz;
};
struct Color {
    float r, g, b;
};

struct VertexPN {
    struct VertexPosition Position;
    struct VertexNormal Normal;
    struct Color Color;
};

#define TORUS_N 20
#define RADIUS (3.0f)
#define MINOR_RADIUS (1.0f)

static void create_torus(uint16_t *indices, struct VertexPN *vertices)
{
    int idx;
    
    idx = 0;
    for (int i = 0; i < TORUS_N; i++) {
	float c = cos(2 * M_PI * i / TORUS_N);
	float s = sin(2 * M_PI * i / TORUS_N);
	struct mat4 rot_y = {
	    {
		{ c, 0, s, 0 },
		{ 0, 1, 0, 0 },
		{-s, 0, c, 0 },
		{ 0, 0, 0, 1 },
	    },
	};
	struct mat4 tra_x = {
	    {
		{ 1, 0, 0, RADIUS },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 },
	    },
	};
	struct mat4 m = mat4_mul(rot_y, tra_x);
	
	for (int j = 0; j < TORUS_N; j++) {
	    float c0 = cos(2 * M_PI * j / TORUS_N);
	    float s0 = sin(2 * M_PI * j / TORUS_N);
	    struct vec4 vtx = {
		{ MINOR_RADIUS * c0, MINOR_RADIUS * s0, 0, 1 },
	    };
	    struct vec4 v = mat4_mul_vec4(m, vtx);
	    struct vec4 norm = {
		{ c0, s0, 0, 1 },
	    };
	    struct vec4 n = mat4_mul_vec4(rot_y, norm);
	    
	    vertices[idx++] = (struct VertexPN) {
		.Position = {
		    .x = v.v[0],
		    .y = v.v[1],
		    .z = v.v[2],
		},
		.Normal = {
		    .nx = n.v[0],
		    .ny = n.v[1],
		    .nz = n.v[2],
		},
		.Color = {
		    .r = (float) i / TORUS_N,
		    .g = (float) j / TORUS_N,
		    .b = 0,
		},
	    };
	}
    }
    
    idx = 0;
    for (int i = 0; i < TORUS_N; i++) {
	for (int j = 0; j < TORUS_N; j++) {
	    int i0 = i * TORUS_N + j;
	    int i1 = i * TORUS_N + (j + 1) % TORUS_N;
	    int i2 = (i + 1) % TORUS_N * TORUS_N + j;
	    int i3 = (i + 1) % TORUS_N * TORUS_N + (j + 1) % TORUS_N;
	    
	    indices[idx++] = i0;
	    indices[idx++] = i1;
	    indices[idx++] = i2;
	    
	    indices[idx++] = i2;
	    indices[idx++] = i1;
	    indices[idx++] = i3;
	}
    }
}

static struct {
    GLuint vb, ib;
    int shader;
    int indexCount;
} drawObj;

static GLint locPVW;

static void CreateResource(void)
{
    drawObj.shader = createShaderProgram(srcVertexShader, srcFragmentShader);
    GLint locPos = glGetAttribLocation(drawObj.shader, "position0");
    GLint locNrm = glGetAttribLocation(drawObj.shader, "normal0");
    GLint locCol = glGetAttribLocation(drawObj.shader, "color0");
    CHECK_GL_ERROR();
    
    locPVW = glGetUniformLocation(drawObj.shader, "matPVW");
    CHECK_GL_ERROR();
    
    uint16_t indices_torus[TORUS_N * TORUS_N * 6];
    struct VertexPN vertices_torus[TORUS_N * TORUS_N];
    create_torus(indices_torus, vertices_torus);
    glGenBuffers(1, &drawObj.vb);
    glBindBuffer(GL_ARRAY_BUFFER, drawObj.vb);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices_torus, vertices_torus, GL_STATIC_DRAW);
    glGenBuffers(1, &drawObj.ib);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawObj.ib);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices_torus, indices_torus, GL_STATIC_DRAW);
    drawObj.indexCount = TORUS_N * TORUS_N * 6;
    CHECK_GL_ERROR();
    
    int stride = sizeof(struct VertexPN);
    glVertexAttribPointer(locPos, 3, GL_FLOAT, GL_FALSE, stride, &((struct VertexPN *) NULL)->Position);
    CHECK_GL_ERROR();
    glVertexAttribPointer(locNrm, 3, GL_FLOAT, GL_FALSE, stride, &((struct VertexPN *) NULL)->Normal);
    CHECK_GL_ERROR();
    glVertexAttribPointer(locCol, 3, GL_FLOAT, GL_FALSE, stride, &((struct VertexPN *) NULL)->Color);
    CHECK_GL_ERROR();
    
    glEnableVertexAttribArray(locPos);
    glEnableVertexAttribArray(locNrm);
    glEnableVertexAttribArray(locCol);
    CHECK_GL_ERROR();
}

#if 0
static void DestroyResource(void)
{
    glDeleteBuffers(1, &drawObj.vb);
    glDeleteBuffers(1, &drawObj.ib);
    glDeleteProgram(drawObj.shader);
}
#endif

static void drawCube(int width, int height)
{
    CHECK_GL_ERROR();
    glEnable(GL_DEPTH_TEST);
    CHECK_GL_ERROR();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CHECK_GL_ERROR();
    glViewport(0, 0, width, height);
    CHECK_GL_ERROR();
    
    static double angle = 0;
    if ((angle += 0.1) >= 12 * M_PI)
	angle -= 12 * M_PI;
    
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
    CHECK_GL_ERROR();
    glUseProgram(drawObj.shader);
    CHECK_GL_ERROR();
    
    struct mat4 r1 = {
	{
	    { cos(angle), -sin(angle), 0, 0 },
	    { sin(angle),  cos(angle), 0, 0 },
	    { 0,                    0, 1, 0 },
	    { 0,                    0, 0, 1 },
	},
    };
    struct mat4 r2 = {
	{
	    { 1,            0,             0, 0 },
	    { 0, cos(angle/2), -sin(angle/2), 0 },
	    { 0, sin(angle/2),  cos(angle/2), 0 },
	    { 0,            0,             0, 1 },
	},
    };
    struct mat4 r3 = {
	{
	    {  cos(angle/3), 0, sin(angle/3), 0 },
	    {             0, 1,            0, 0 },
	    { -sin(angle/3), 0, cos(angle/3), 0 },
	    {             0, 0,            0, 1 },
	},
    };
    const float scale = 1.0;
    struct mat4 s1 = {
	{
	    { scale,     0,     0, 0 },
	    {     0, scale,     0, 0 },
	    {     0,     0, scale, 0 },
	    {     0,     0,     0, 1 },
	},
    };
    struct mat4 t1 = {
	{
	    { 1, 0, 0, 0 },
	    { 0, 1, 0, 0 },
	    { 0, 0, 1, -100 },
	    { 0, 0, 0, 1 },
	},
    };
    const float near = 80;
    const float far = 120;
    const float right = 10;
    const float top = 10;
    struct mat4 proj = {
	{
	    { near / right, 0, 0, 0 },
	    { 0, near / top, 0, 0 },
	    { 0, 0, -(far+near)/(far-near), -2*far*near/(far-near) },
	    { 0, 0, -1, 0 },
	},
    };
    struct mat4 m = {
	{
	    { 1, 0, 0, 0 },
	    { 0, 1, 0, 0 },
	    { 0, 0, 1, 0 },
	    { 0, 0, 0, 1 },
	},
    };
    m = mat4_mul(r1, m);
    m = mat4_mul(r2, m);
    m = mat4_mul(r3, m);
    m = mat4_mul(s1, m);
    m = mat4_mul(t1, m);
    m = mat4_mul(proj, m);
    
    printf("----\n");
    for (int y = 0; y < 4; y++) {
	for (int x = 0; x < 4; x++)
	    printf("%7.3f ", m.v[y][x]);
	printf("\n");
    }
    
    // glm::mat4 pvw = proj * view * world;
    
    CHECK_GL_ERROR();
    glUniformMatrix4fv(locPVW, 1, GL_TRUE, (float *) &m);
    CHECK_GL_ERROR();
    
    glBindBuffer(GL_ARRAY_BUFFER, drawObj.vb);
    CHECK_GL_ERROR();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawObj.ib);
    CHECK_GL_ERROR();
    
    glDrawElements(GL_TRIANGLES, drawObj.indexCount, GL_UNSIGNED_SHORT, NULL);
}


static int create_texture(void)
{
    static unsigned char data[16 * 16 * 4];
    GLuint tex;

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

static gboolean render(GtkWidget *area, GdkGLContext *context)
{
    CHECK_GL_ERROR();
    static int tex = 0;
    if (tex == 0) {
	tex = create_texture();
	CreateResource();
    }
    
    static float v = 0.0f;
    static float diff = 0.01f;
    CHECK_GL_ERROR();
    glClearColor(v, v, v, 1);
    CHECK_GL_ERROR();
    if (diff > 0) {
	if ((v += diff) >= 0.3f) {
	    v = 0.3f;
	    diff = -diff;
	}
    } else {
	if ((v += diff) < 0) {
	    v = 0;
	    diff = -diff;
	}
    }
    
    drawCube(gdk_window_get_width(gtk_widget_get_window(area)),
	    gdk_window_get_height(gtk_widget_get_window(area)));
    
    CHECK_GL_ERROR();
    
    return TRUE;
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    
    GtkWidget *toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_show(toplevel);
    
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
