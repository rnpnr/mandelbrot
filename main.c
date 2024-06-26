/* see LICENSE for licensing details */
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include <GL/gl.h>
#include <GLES3/gl32.h>
#include <GLFW/glfw3.h>

typedef float     f32;
typedef double    f64;
typedef uint8_t   u8;
typedef int32_t   i32;
typedef uint32_t  u32;
typedef ptrdiff_t size;

typedef struct { f32 x, y; } v2;
typedef struct { v2 top_left, bottom_right; } Rect;

typedef struct { size len; u8 *data; } s8;

typedef struct { u8 *beg, *end; } Arena;

#ifdef __clang__
#define ASSERT(c) if (!(c)) __builtin_debugtrap()
#else
#define ASSERT(c) if (!(c)) asm("int3; nop");
#endif

#define ABS(x)           ((x) <= 0 ? -(x) : (x))
#define BETWEEN(x, l, h) ((x) >= (l) && (x) <= (h))
#define ARRAY_COUNT(a)   (sizeof(a) / sizeof(*a))

#define EPS 0.00001f

#define MAX_ITERATIONS 300

#include "util.c"

#ifdef __unix__
#include "os_unix.c"
#else
#error Unsupported Platform!
#endif

typedef union {
	struct { u8 a, b, g, r; };
	u32 rgba;
} Colour;

static struct {
	GLFWwindow *window;
	u32 vao, vbo;
	i32 pid;
	i32 height, width;
	union {
		struct {
			i32 screen_dim, z_n;
			i32 top_left, bottom_right;
			i32 use_approx;
		};
		i32 E[5];
	} uniforms;
	v2  dP;
	Rect boundary;
	f32 zoom;
	v2 *z_n;
	Colour clear_colour;
} g_glctx;

static const char *uniform_names[] = {
	"u_screen_dim",
	"u_z_n",
	"u_top_left",
	"u_bottom_right",
	"u_use_approx",
};

static Rect default_boundary = {
	.top_left     = (v2){ .x = -2.5, .y =  1.5 },
	.bottom_right = (v2){ .x =  1.0, .y = -1.5 },
};

static Rect
move_rect(Rect r, v2 delta)
{
	r.top_left.x     += delta.x;
	r.top_left.y     += delta.y;
	r.bottom_right.x += delta.x;
	r.bottom_right.y += delta.y;
	return r;
}

static v2
rect_center(Rect r)
{
	return (v2) {
		.x = r.bottom_right.x - r.top_left.x,
		.y = r.top_left.y - r.bottom_right.y,
	};
}

static f32
magnitude_v2(v2 v)
{
	return v.x * v.x + v.y * v.y;
}

static v2
sub_v2(v2 a, v2 b)
{
	return (v2){ .x = a.x - b.x, .y = a.y - b.y };
}

static void
debug_logger(u32 src, u32 type, u32 id, u32 lvl, i32 len, const char *msg, const void *data)
{
	(void)src; (void)type; (void)id; (void)data;
	fputs("[gl error ", stderr);
	switch(lvl) {
	case GL_DEBUG_SEVERITY_HIGH:   fputs("HIGH]: ",      stderr); break;
	case GL_DEBUG_SEVERITY_MEDIUM: fputs("MEDIUM]: ",    stderr); break;
	case GL_DEBUG_SEVERITY_LOW:    fputs("LOW]: ",       stderr); break;
	default:                       fputs("(default)]: ", stderr); break;
	}
	fwrite(msg, 1, len, stderr);
	fputc('\n', stderr);
}

static void
error_callback(int code, const char *desc)
{
	fprintf(stderr, "GLFW Error (0x%04X): %s\n", code, desc);
}

static void
fb_callback(GLFWwindow *win, i32 w, i32 h)
{
	g_glctx.height = h;
	g_glctx.width  = w;
	glViewport(0, 0, w, h);
}

static void
key_callback(GLFWwindow *win, i32 key, i32 sc, i32 action, i32 mod)
{
	(void)sc;

	f32 scale = (mod & GLFW_MOD_SHIFT) ? 1 : 0.5;

	v2 dP = sub_v2(g_glctx.boundary.top_left, g_glctx.boundary.bottom_right);
	dP.x *= -scale; //g_glctx.zoom;
	dP.y *=  scale; //g_glctx.zoom;

	switch (key) {
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_PRESS)
			glfwSetWindowShouldClose(win, GL_TRUE);
		break;
	case GLFW_KEY_W:
		if (action == GLFW_PRESS || action == GLFW_REPEAT)
			g_glctx.dP.y = dP.y;
		else if (action == GLFW_RELEASE)
			g_glctx.dP.y = 0;
		break;
	case GLFW_KEY_A:
		if (action == GLFW_PRESS || action == GLFW_REPEAT)
			g_glctx.dP.x = -dP.x;
		else if (action == GLFW_RELEASE)
			g_glctx.dP.x = 0;
		break;
	case GLFW_KEY_S:
		if (action == GLFW_PRESS || action == GLFW_REPEAT)
			g_glctx.dP.y = -dP.y;
		else if (action == GLFW_RELEASE)
			g_glctx.dP.y = 0;
		break;
	case GLFW_KEY_D:
		if (action == GLFW_PRESS || action == GLFW_REPEAT)
			g_glctx.dP.x = dP.x;
		else if (action == GLFW_RELEASE)
			g_glctx.dP.x = 0;
		break;
	}
}

static void
scroll_callback(GLFWwindow *win, f64 xdelta, f64 ydelta)
{
	v2 delta = sub_v2(g_glctx.boundary.top_left, g_glctx.boundary.bottom_right);

	f32 scale = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) ? 0.2 : 0.05;

	g_glctx.zoom += ydelta * 1/scale;
	if (g_glctx.zoom < 1) g_glctx.zoom = 1;

	delta.x = ABS(delta.x) * scale * 0.5 * ydelta;
	delta.y = ABS(delta.y) * scale * 0.5 * ydelta;
	g_glctx.boundary.top_left.x += delta.x;
	g_glctx.boundary.top_left.y -= delta.y;
	g_glctx.boundary.bottom_right.x -= delta.x;
	g_glctx.boundary.bottom_right.y += delta.y;
}

static void
recalculate_z_n(void)
{
	v2 *z_n = g_glctx.z_n;
	z_n[0] = rect_center(g_glctx.boundary);
	for (u32 i = 1; i < MAX_ITERATIONS; i++) {
		f32 xx = z_n[i - 1].x * z_n[i - 1].x;
		f32 yy = z_n[i - 1].y * z_n[i - 1].y;
		z_n[i].x = xx - yy + z_n[0].x;
		z_n[i].y = 2 * z_n[i - 1].x * z_n[i - 1].y + z_n[0].y;
	}
}

static void
mouse_button_callback(GLFWwindow *win, i32 btn, i32 act, i32 mod)
{
#if 0
	if (btn == GLFW_MOUSE_BUTTON_LEFT && act == GLFW_PRESS) {
		f64 xpos, ypos;
		glfwGetCursorPos(g_glctx.window, &xpos, &ypos);
		f32 delta_x = 2 * xpos / g_glctx.width - 1;
		f32 delta_y = 2 * ypos / g_glctx.height - 1;
		f32 scale = 1; //g_glctx.zoom;
		g_glctx.cursor.x -= scale * delta_x;
		g_glctx.cursor.y -= scale * delta_y;
	}
#endif

	if (btn == GLFW_MOUSE_BUTTON_RIGHT && act == GLFW_PRESS) {
		g_glctx.boundary = default_boundary;
		g_glctx.dP = (v2){0};
		g_glctx.zoom = 1.0;
		recalculate_z_n();
		glUniform2fv(g_glctx.uniforms.z_n, MAX_ITERATIONS,
		             (f32 *)g_glctx.z_n);
	}
}

static void
clear_colour(Colour c)
{
	glClearColor(c.r / 255.0f, c.b / 255.0f, c.g / 255.0f, c.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

static void
init_renderer(void)
{
	glDebugMessageCallback(debug_logger, NULL);
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
	                      GL_DEBUG_SEVERITY_NOTIFICATION,
	                      0, 0, GL_FALSE);

	glGenVertexArrays(1, &g_glctx.vao);
	glBindVertexArray(g_glctx.vao);

	f32 vertices[] = {
		-1.0f,  1.0f,
		-1.0f, -1.0f,
		 1.0f,  1.0f,
		 1.0f, -1.0f,
	};
	glGenBuffers(1, &g_glctx.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_glctx.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
}

static i32
spawn_window(void)
{
	GLFWmonitor *mon = glfwGetPrimaryMonitor();
	if (!mon)
		return -1;
	glfwGetMonitorWorkarea(mon, NULL, NULL, &g_glctx.width, &g_glctx.height);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	g_glctx.window = glfwCreateWindow(g_glctx.width, g_glctx.height,
	                                  "Mandelbrot Viewer", NULL, NULL);
	if (g_glctx.window == NULL)
		return -1;

	glfwMakeContextCurrent(g_glctx.window);

	/* disable vsync */
	//glfwSwapInterval(0);

	glfwSetFramebufferSizeCallback(g_glctx.window, fb_callback);
	glfwSetKeyCallback(g_glctx.window, key_callback);
	glfwSetScrollCallback(g_glctx.window, scroll_callback);
	glfwSetMouseButtonCallback(g_glctx.window, mouse_button_callback);

	g_glctx.clear_colour = (Colour){ .r = 64, .b = 64, .g = 64, .a = 255 };
	clear_colour(g_glctx.clear_colour);

	init_renderer();

	return 0;
}

static u32
compile_shader(Arena a, u32 type, s8 s)
{
	u32 sid = glCreateShader(type);
	glShaderSource(sid, 1, (const char **)&s.data, (int *)&s.len);
	glCompileShader(sid);

	i32 res = 0;
	glGetShaderiv(sid, GL_COMPILE_STATUS, &res);
	if (res != GL_TRUE) {
		i32 len, len2;
		glGetShaderiv(sid, GL_INFO_LOG_LENGTH, &len);
		char *data = alloc(&a, char, len);
		glGetShaderInfoLog(sid, len, &len2, data);
		fputs("compile_shader: ", stderr);
		fwrite(data, 1, len2, stderr);
		fputc('\n', stderr);
		glDeleteShader(sid);
		return 0;
	}

	return sid;
}

static i32
program_from_files(Arena a, char *vert, size vfilesize, char *frag, size ffilesize)
{
	s8 vertex   = os_read_file(&a, vert, vfilesize);
	s8 fragment = os_read_file(&a, frag, ffilesize);
	if (vertex.len == 0 || fragment.len == 0)
		return -1;
	i32 pid = glCreateProgram();
	u32 vid = compile_shader(a, GL_VERTEX_SHADER,   vertex);
	u32 fid = compile_shader(a, GL_FRAGMENT_SHADER, fragment);

	if (fid == 0 || vid == 0)
		return -1;

	glAttachShader(pid, vid);
	glAttachShader(pid, fid);
	glLinkProgram(pid);
	glValidateProgram(pid);
	glDeleteShader(vid);
	glDeleteShader(fid);

	return pid;
}

static Arena
get_arena(void)
{
	static u8 *data[32 * 1024];
	Arena a = {0};
	a.beg = (u8 *)data;
	/* cleanup up your dirty laundry boy!! */
	asm("" : "+r"(a.beg));
	a.end = a.beg + 32 * 1024;
	return a;
}

static void
validate_uniforms(void)
{
	for (u32 i = 0; i < ARRAY_COUNT(g_glctx.uniforms.E); i++) {
		i32 uid = glGetUniformLocation(g_glctx.pid, uniform_names[i]);
		g_glctx.uniforms.E[i] = uid;
	}
}

#if 0
static v2
map_screen_to_rect(Rect bounds, v2 screen_pos)
{
	ASSERT(BETWEEN(screen_pos.x, 0, 1.0) && BETWEEN(screen_pos.y, 0, 1.0));

	v2 size   = {
		.x = ABS(bounds.bottom_right.x - bounds.top_left.x),
		.y = ABS(bounds.top_left.y - bounds.bottom_right.y),
	};

	return (v2){
		.x = bounds.top_left.x + size.x * screen_pos.x,
		.y = bounds.bottom_right.y + size.y * screen_pos.y,
	};
}

static void
print_motion(void)
{
	v2 dP  = g_glctx.dP;
	v2 ddP = g_glctx.ddP;
	printf("dP  = { .x = %0.02f, .y = %0.02f }\n", dP.x, dP.y);
	printf("ddP = { .x = %0.02f, .y = %0.02f }\n", ddP.x, ddP.y);
}
#endif

int
main(void)
{
	Arena memory = get_arena();
	g_glctx.z_n = alloc(&memory, v2, MAX_ITERATIONS);
	g_glctx.zoom = 1.0;

	g_glctx.boundary = default_boundary;

	if (!glfwInit())
		return -1;
	glfwSetErrorCallback(error_callback);

	spawn_window();

	if (g_glctx.window == NULL) {
		glfwTerminate();
		return -1;
	}

	os_file_stats vert_stats = os_get_file_stats("vert.glsl");
	os_file_stats frag_stats = os_get_file_stats("frag.glsl");
	g_glctx.pid = program_from_files(memory,
	                                 "vert.glsl", vert_stats.filesize,
	                                 "frag.glsl", frag_stats.filesize);
	if (g_glctx.pid == -1) {
		glfwTerminate();
		return -1;
	}
	glUseProgram(g_glctx.pid);
	validate_uniforms();

	u32 fcount = 0;
	f32 last_time = 0;
	while (!glfwWindowShouldClose(g_glctx.window)) {
		glfwPollEvents();

		f32 current_time = glfwGetTime();
		f32 dt = current_time - last_time;
		last_time = current_time;

		i32 ua = 1.0e-8 > magnitude_v2(sub_v2(g_glctx.boundary.top_left,
		                                      g_glctx.boundary.bottom_right));

		if (++fcount > 300) {
			v2 bound_cent = rect_center(g_glctx.boundary);
			printf("FPS: %0.03f | dt = %0.03f [ms] | approx = %d\n"
			       "Center: <%f, %f>\n",
			       1 / dt, dt * 1e3, ua, bound_cent.x, bound_cent.y);
			fcount = 0;
		}

		os_file_stats new_vert = os_get_file_stats("vert.glsl");
		os_file_stats new_frag = os_get_file_stats("frag.glsl");
		if (os_compare_filetime(vert_stats.timestamp, new_vert.timestamp) ||
		    os_compare_filetime(frag_stats.timestamp, new_frag.timestamp)) {
			i32 pid = program_from_files(memory,
			                             "vert.glsl", new_vert.filesize,
			                             "frag.glsl", new_frag.filesize);
			if (pid > 0) {
				frag_stats = new_frag;
				vert_stats = new_vert;
				glDeleteProgram(g_glctx.pid);
				g_glctx.pid = pid;
				glUseProgram(g_glctx.pid);
				validate_uniforms();
			}
		}

		v2 v = g_glctx.dP;
		v2 delta;
		delta.x = v.x * dt;
		delta.y = v.y * dt;
		g_glctx.boundary = move_rect(g_glctx.boundary, delta);

		if (ua) {
			recalculate_z_n();
			glUniform2fv(g_glctx.uniforms.z_n, MAX_ITERATIONS,
			             (f32 *)g_glctx.z_n);
		}

		glUniform2fv(g_glctx.uniforms.top_left, 1,
		             (f32 *)&g_glctx.boundary.top_left);
		glUniform2fv(g_glctx.uniforms.bottom_right, 1,
		             (f32 *)&g_glctx.boundary.bottom_right);
		glUniform2ui(g_glctx.uniforms.screen_dim,
		             g_glctx.width, g_glctx.height);

		glUniform1i(g_glctx.uniforms.use_approx, ua);

		clear_colour(g_glctx.clear_colour);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glfwSwapBuffers(g_glctx.window);
	}

	return 0;
}
