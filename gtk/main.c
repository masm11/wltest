#include <stdio.h>
#include <gtk/gtk.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHECK_GL_ERROR() check_gl_error(__FILE__, __LINE__)
static void check_gl_error(const char *file, int lineno)
{
    int err = glGetError();
    if (err == GL_NO_ERROR)
	return;
    printf("%s:%d: err=0x%08x.\n", file, lineno, err);
    exit(1);
}

static const char *vertex_shader_source1 =
	"attribute vec4 position0;\n"
	"attribute vec3 normal0;\n"
	"attribute vec3 color0;\n"
	"attribute vec2 tex0;\n"
	"varying vec4 vsout_color0;\n"
	"varying vec2 vsout_uv;\n"
	"varying float vsout_shade;\n"
	"uniform mat4 matPVW;\n"
	"uniform mat4 matRot;\n"
	"void main() {\n"
	"  gl_Position = matPVW * position0;\n"
	"  vec4 norm = matRot * vec4(normal0.xyz, 0.0);\n"
	"  float shade = clamp(dot(normalize(vec3(1.0, 1.0, 1.0)), normalize(norm.xyz)), 0.0, 1.0);\n"
	"  vsout_shade = shade * 0.7 + 0.3;\n"
	"  vsout_color0.rgb = color0;\n"
	"  vsout_color0.a = 1.0;\n"
	"  vsout_uv = tex0;\n"
	"}";

static const char *fragment_shader_source1 =
	// "precision mediump float;\n"
	"uniform sampler2D texture;\n"
	"varying vec4 vsout_color0;\n"
	"varying vec2 vsout_uv;\n"
	"varying float vsout_shade;\n"
	"void main() {\n"
	"  gl_FragColor = vec4(vsout_color0.rgb * texture2D(texture, vsout_uv).rgb * vsout_shade, 1.0);\n"
	"}";

static const char *vertex_shader_source2 =
	"attribute vec4 position2;\n"
	"attribute vec3 color2;\n"
	"varying vec4 vsout_color2;\n"
	"void main() {\n"
	"  gl_Position = position2;\n"
	"  vsout_color2.rgb = color2;\n"
	"  vsout_color2.a = 1.0;\n"
	"}";

static const char *fragment_shader_source2 =
	"varying vec4 vsout_color2;\n"
	"void main() {\n"
	"  gl_FragColor = vsout_color2;\n"
	"}";

static void check_compiled(int shader)
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

static int create_shader_program(const char *vs_src, const char *fs_src)
{
    int vs = glCreateShader(GL_VERTEX_SHADER);
    int fs = glCreateShader(GL_FRAGMENT_SHADER);
    
    fprintf(stderr, "vertex shader.\n");
    glShaderSource(vs, 1, &vs_src, 0);
    CHECK_GL_ERROR();
    
    glCompileShader(vs);
    check_compiled(vs);
    
    fprintf(stderr, "fragment shader.\n");
    glShaderSource(fs, 1, &fs_src, NULL);
    glCompileShader(fs);
    check_compiled(fs);
    
    int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    
    glLinkProgram(prog);
    
    return prog;
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

struct vertex_t {
    struct {
	float x, y, z;
    } position;
    struct {
	float nx, ny, nz;
    } normal;
    struct {
	float u, v;
    } texture;
    struct {
	float r, g, b;
    } color;
};

#define TORUS_N 100
#define RADIUS (3.0f)
#define MINOR_RADIUS (1.0f)

static void create_torus(uint16_t *indices, struct vertex_t *vertices)
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
	    
	    vertices[idx++] = (struct vertex_t) {
		.position = {
		    .x = v.v[0],
		    .y = v.v[1],
		    .z = v.v[2],
		},
		.normal = {
		    .nx = n.v[0],
		    .ny = n.v[1],
		    .nz = n.v[2],
		},
		.texture = {
		    .u = (float) i / TORUS_N,
		    .v = (float) j / TORUS_N,
		},
		.color = {
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
	    indices[idx++] = i2;
	    indices[idx++] = i1;
	    
	    indices[idx++] = i2;
	    indices[idx++] = i3;
	    indices[idx++] = i1;
	}
    }
}

static void create_flat(uint16_t *indices, struct vertex_t *vertices)
{
    int idx;
    
    idx = 0;
    vertices[idx++] = (struct vertex_t) { {   0,   0, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 1 } };
    vertices[idx++] = (struct vertex_t) { { 100,   0, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 1 } };
    vertices[idx++] = (struct vertex_t) { {   0, 100, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 1 } };
    vertices[idx++] = (struct vertex_t) { { 100, 100, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 1 } };
    vertices[idx++] = (struct vertex_t) { {   0,   0, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 } };
    vertices[idx++] = (struct vertex_t) { {   0, 100, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 } };
    vertices[idx++] = (struct vertex_t) { {-100,   0, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 } };
    vertices[idx++] = (struct vertex_t) { {-100, 100, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 } };
    vertices[idx++] = (struct vertex_t) { {   0,   0, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 1, 1 } };
    vertices[idx++] = (struct vertex_t) { {-100,   0, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 1, 1 } };
    vertices[idx++] = (struct vertex_t) { {   0,-100, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 1, 1 } };
    vertices[idx++] = (struct vertex_t) { {-100,-100, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 1, 1 } };
    vertices[idx++] = (struct vertex_t) { {   0,   0, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 } };
    vertices[idx++] = (struct vertex_t) { {   0,-100, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 } };
    vertices[idx++] = (struct vertex_t) { { 100,   0, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 } };
    vertices[idx++] = (struct vertex_t) { { 100,-100, 0 }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 } };
    
    idx = 0;
    indices[idx++] = 0; indices[idx++] = 1; indices[idx++] = 2;
    indices[idx++] = 2; indices[idx++] = 1; indices[idx++] = 3;
    indices[idx++] = 4; indices[idx++] = 5; indices[idx++] = 6;
    indices[idx++] = 6; indices[idx++] = 5; indices[idx++] = 7;
    indices[idx++] = 8; indices[idx++] = 9; indices[idx++] =10;
    indices[idx++] =10; indices[idx++] = 9; indices[idx++] =11;
    indices[idx++] =12; indices[idx++] =13; indices[idx++] =14;
    indices[idx++] =14; indices[idx++] =13; indices[idx++] =15;
}

struct work_t {
    int inited;
    
    struct torus_t {
	double angle;
	
	int shader;
	GLuint vertex_buffer, index_buffer;
	int nr_indices;
	
	GLuint tex;
    } torus;
    
    struct background_t {
	int shader;
	GLuint vertex_buffer, index_buffer;
	int nr_indices;
    } bg;
};

static void create_texture(struct torus_t *tw)
{
    FILE *fp;
    if ((fp = popen("png2pnm < test.png", "r")) == NULL) {
	perror("popen");
	exit(1);
    }
    
    char magic[3];
    int width, height;
    int max;
    if (fscanf(fp, "%2s %d %d %d\n", magic, &width, &height, &max) != 4) {
	printf("bad header.\n");
	exit(1);
    }
    
    unsigned char *data;
    if ((data = malloc(width * height * 4)) == NULL) {
	printf("out of memory.\n");
	exit(1);
    }
    memset(data, 0, width * height * 4);
    
    for (int y = 0; y < height; y++) {
	for (int x = 0; x < width; x++) {
	    int r = fgetc(fp);
	    int g = fgetc(fp);
	    int b = fgetc(fp);
	    if (b == EOF) {
		printf("unexpected eof.\n");
		exit(1);
	    }
	    data[(y * width + x) * 4 + 0] = r;
	    data[(y * width + x) * 4 + 1] = g;
	    data[(y * width + x) * 4 + 2] = b;
	    data[(y * width + x) * 4 + 3] = 255;
	}
    }
    
    pclose(fp);
    
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // glEnable(GL_SCISSOR_TEST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    tw->tex = tex;
}

static void create_torus_model(struct torus_t *tw)
{
    tw->shader = create_shader_program(vertex_shader_source1, fragment_shader_source1);
    CHECK_GL_ERROR();
    
    uint16_t indices_torus[TORUS_N * TORUS_N * 6];
    struct vertex_t vertices_torus[TORUS_N * TORUS_N];
    create_torus(indices_torus, vertices_torus);
    glGenBuffers(1, &tw->vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, tw->vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices_torus, vertices_torus, GL_STATIC_DRAW);
    glGenBuffers(1, &tw->index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tw->index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices_torus, indices_torus, GL_STATIC_DRAW);
    tw->nr_indices = TORUS_N * TORUS_N * 6;
    CHECK_GL_ERROR();
    
    create_texture(tw);
}

static void create_background_model(struct background_t *bw)
{
    bw->shader = create_shader_program(vertex_shader_source2, fragment_shader_source2);
    CHECK_GL_ERROR();
    
    uint16_t indices[24];
    struct vertex_t vertices[16];
    create_flat(indices, vertices);
    glGenBuffers(1, &bw->vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, bw->vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
    glGenBuffers(1, &bw->index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bw->index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices, indices, GL_STATIC_DRAW);
    bw->nr_indices = 24;
    CHECK_GL_ERROR();
}

static void create_resources(struct work_t *w)
{
    create_torus_model(&w->torus);
    create_background_model(&w->bg);
}

static void draw_background(struct background_t *bw)
{
    CHECK_GL_ERROR();
    glEnable(GL_DEPTH_TEST);
    CHECK_GL_ERROR();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CHECK_GL_ERROR();
    
    int stride = sizeof(struct vertex_t);
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    
    glUseProgram(bw->shader);
    CHECK_GL_ERROR();
    
    glBindBuffer(GL_ARRAY_BUFFER, bw->vertex_buffer);
    CHECK_GL_ERROR();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bw->index_buffer);
    CHECK_GL_ERROR();
    
    GLint locPos = glGetAttribLocation(bw->shader, "position2");
    GLint locCol = glGetAttribLocation(bw->shader, "color2");
    
    CHECK_GL_ERROR();
    glVertexAttribPointer(locPos, 3, GL_FLOAT, GL_FALSE, stride, &((struct vertex_t *) NULL)->position);
    CHECK_GL_ERROR();
    glVertexAttribPointer(locCol, 3, GL_FLOAT, GL_FALSE, stride, &((struct vertex_t *) NULL)->color);
    CHECK_GL_ERROR();
    glEnableVertexAttribArray(locPos);
    glEnableVertexAttribArray(locCol);
    CHECK_GL_ERROR();
    
    glDrawElements(GL_TRIANGLES, bw->nr_indices, GL_UNSIGNED_SHORT, NULL);
}

static void draw_torus(struct torus_t *tw, int width, int height)
{
    glClear(GL_DEPTH_BUFFER_BIT);
    
    if ((tw->angle += 0.01) >= 12 * M_PI)
	tw->angle -= 12 * M_PI;
    
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    CHECK_GL_ERROR();
    glUseProgram(tw->shader);
    CHECK_GL_ERROR();
    
    glBindBuffer(GL_ARRAY_BUFFER, tw->vertex_buffer);
    CHECK_GL_ERROR();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tw->index_buffer);
    CHECK_GL_ERROR();
    
    GLint locPos = glGetAttribLocation(tw->shader, "position0");
    GLint locNrm = glGetAttribLocation(tw->shader, "normal0");
    GLint locCol = glGetAttribLocation(tw->shader, "color0");
    GLint locTex = glGetAttribLocation(tw->shader, "tex0");
    
    int stride = sizeof(struct vertex_t);
    glVertexAttribPointer(locPos, 3, GL_FLOAT, GL_FALSE, stride, &((struct vertex_t *) NULL)->position);
    CHECK_GL_ERROR();
    glVertexAttribPointer(locNrm, 3, GL_FLOAT, GL_FALSE, stride, &((struct vertex_t *) NULL)->normal);
    CHECK_GL_ERROR();
    glVertexAttribPointer(locCol, 3, GL_FLOAT, GL_FALSE, stride, &((struct vertex_t *) NULL)->color);
    CHECK_GL_ERROR();
    glVertexAttribPointer(locTex, 3, GL_FLOAT, GL_FALSE, stride, &((struct vertex_t *) NULL)->texture);
    CHECK_GL_ERROR();
    glEnableVertexAttribArray(locPos);
    glEnableVertexAttribArray(locNrm);
    glEnableVertexAttribArray(locCol);
    glEnableVertexAttribArray(locTex);
    CHECK_GL_ERROR();
    
    struct mat4 r1 = {
	{
	    { cos(tw->angle), -sin(tw->angle), 0, 0 },
	    { sin(tw->angle),  cos(tw->angle), 0, 0 },
	    { 0,                    0, 1, 0 },
	    { 0,                    0, 0, 1 },
	},
    };
    struct mat4 r2 = {
	{
	    { 1,            0,             0, 0 },
	    { 0, cos(tw->angle/2), -sin(tw->angle/2), 0 },
	    { 0, sin(tw->angle/2),  cos(tw->angle/2), 0 },
	    { 0,            0,             0, 1 },
	},
    };
    struct mat4 r3 = {
	{
	    {  cos(tw->angle/3), 0, sin(tw->angle/3), 0 },
	    {             0, 1,            0, 0 },
	    { -sin(tw->angle/3), 0, cos(tw->angle/3), 0 },
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
    float right = 10;
    float top = 10;
    if (width > height) {
	right *= (float) width / height;
    } else {
	top *= (float) height / width;
    }
    struct mat4 proj = {
	{
	    { near/right, 0, 0, 0 },
	    { 0, near/top, 0, 0 },
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
    struct mat4 rot = m;
    m = mat4_mul(t1, m);
    m = mat4_mul(proj, m);
    
    GLint locPVW = glGetUniformLocation(tw->shader, "matPVW");
    GLint locRot = glGetUniformLocation(tw->shader, "matRot");
    GLint locTexture = glGetUniformLocation(tw->shader, "texture");
    
    CHECK_GL_ERROR();
    glUniformMatrix4fv(locPVW, 1, GL_TRUE, (float *) &m);
    CHECK_GL_ERROR();
    glUniformMatrix4fv(locRot, 1, GL_TRUE, (float *) &rot);
    CHECK_GL_ERROR();
    glUniform1i(locTexture, 0);
    CHECK_GL_ERROR();
    glBindTexture(GL_TEXTURE_2D, tw->tex);
    CHECK_GL_ERROR();
    
    glBindBuffer(GL_ARRAY_BUFFER, tw->vertex_buffer);
    CHECK_GL_ERROR();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tw->index_buffer);
    CHECK_GL_ERROR();
    
    glDrawElements(GL_TRIANGLES, tw->nr_indices, GL_UNSIGNED_SHORT, NULL);
}

static void draw(struct work_t *w, int width, int height)
{
    draw_background(&w->bg);
    draw_torus(&w->torus, width, height);
}

static gboolean render(GtkWidget *area, GdkGLContext *context, gpointer user_data)
{
    struct work_t *w = user_data;
    CHECK_GL_ERROR();
    if (!w->inited) {
	create_resources(w);
	CHECK_GL_ERROR();
	w->inited = 1;
    }
    
    glClearColor(0, 0, 0, 1);
    CHECK_GL_ERROR();
    
    draw(w, gdk_window_get_width(gtk_widget_get_window(area)),
	    gdk_window_get_height(gtk_widget_get_window(area)));
    
    CHECK_GL_ERROR();
    
    return TRUE;
}

static gboolean timeout_cb(gpointer user_data)
{
    gtk_gl_area_queue_render(GTK_GL_AREA(user_data));
    return G_SOURCE_CONTINUE;
}

int main(int argc, char **argv)
{
    struct work_t w;
    memset(&w, 0, sizeof w);
    
    gtk_init(&argc, &argv);
    
    GtkWidget *toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_show(toplevel);
    
    GtkWidget *drawable = gtk_gl_area_new();
    gtk_gl_area_set_use_es(GTK_GL_AREA(drawable), TRUE);
    gtk_gl_area_set_required_version(GTK_GL_AREA(drawable), 2, 0);
    gtk_gl_area_set_has_alpha(GTK_GL_AREA(drawable), TRUE);
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(drawable), TRUE);
    gtk_gl_area_set_auto_render(GTK_GL_AREA(drawable), FALSE);
    g_signal_connect(drawable, "render", G_CALLBACK(render), &w);
    gtk_widget_show(drawable);
    gtk_widget_set_size_request(drawable, 500, 500);
    gtk_container_add(GTK_CONTAINER(toplevel), drawable);
    
    g_timeout_add(17, timeout_cb, drawable);
    
    gtk_main();
    
    return 0;
}
