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
	i32 u_screen_dim, u_zoom, u_top_left, u_bottom_right;
	f32 zoom;
	Rect boundary;
	Colour clear_colour;
} g_glctx;


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

	f32 delta = (mod & GLFW_MOD_SHIFT) ? 0.25 : 0.05;
	delta /= g_glctx.zoom;

	switch (key) {
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_PRESS)
			glfwSetWindowShouldClose(win, GL_TRUE);
		break;
	case GLFW_KEY_W:
		if (action == GLFW_PRESS || action == GLFW_REPEAT)
			g_glctx.boundary = move_rect(g_glctx.boundary, (v2){.y = delta});
		break;
	case GLFW_KEY_A:
		if (action == GLFW_PRESS || action == GLFW_REPEAT)
			g_glctx.boundary = move_rect(g_glctx.boundary, (v2){.x = -delta});
		break;
	case GLFW_KEY_S:
		if (action == GLFW_PRESS || action == GLFW_REPEAT)
			g_glctx.boundary = move_rect(g_glctx.boundary, (v2){.y = -delta});
		break;
	case GLFW_KEY_D:
		if (action == GLFW_PRESS || action == GLFW_REPEAT)
			g_glctx.boundary = move_rect(g_glctx.boundary, (v2){.x = delta});
		break;
	}
}

static void
scroll_callback(GLFWwindow *win, f64 xdelta, f64 ydelta)
{
	f32 scale = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) ? 5 : 1;
	g_glctx.zoom += scale * ydelta;
	if (g_glctx.zoom < 1.0) g_glctx.zoom = 1.0;
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
		g_glctx.zoom = 1;
		g_glctx.boundary = default_boundary;
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

	g_glctx.u_screen_dim = -1;
	g_glctx.window = glfwCreateWindow(g_glctx.width, g_glctx.height,
	                                  "Mandelbrot Viewer", NULL, NULL);
	if (g_glctx.window == NULL)
		return -1;

	glfwMakeContextCurrent(g_glctx.window);

	/* disable vsync */
	glfwSwapInterval(0);

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
	g_glctx.u_screen_dim = glGetUniformLocation(g_glctx.pid, "u_screen_dim");
	g_glctx.u_zoom       = glGetUniformLocation(g_glctx.pid, "u_zoom");
	g_glctx.u_top_left   = glGetUniformLocation(g_glctx.pid, "u_top_left");
	g_glctx.u_bottom_right   = glGetUniformLocation(g_glctx.pid, "u_bottom_right");
}

int
main(void)
{
	Arena memory = get_arena();

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
	g_glctx.zoom = 1.0;
	g_glctx.boundary = default_boundary;

	u32 fcount = 0;
	f32 last_time = 0;
	while (!glfwWindowShouldClose(g_glctx.window)) {
		glfwPollEvents();

		f32 current_time = glfwGetTime();
		f32 dt = current_time - last_time;
		last_time = current_time;
		if (++fcount > 1000) {
			printf("FPS: %0.03f | dt = %0.03f [ms] | zoom = %0.01f\n"
			       "bounds = { { %0.04f, %0.04f }, { %0.04f, %0.04f } }\n",
			       1 / dt, dt * 1e3, g_glctx.boundary.top_left.x,
			       g_glctx.boundary.top_left.y, g_glctx.boundary.bottom_right.x,
			       g_glctx.boundary.bottom_right.y, g_glctx.zoom);
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

		glUniform2fv(g_glctx.u_top_left, 1, (f32 *)&g_glctx.boundary.top_left);
		glUniform2fv(g_glctx.u_bottom_right, 1, (f32 *)&g_glctx.boundary.bottom_right);
		glUniform2ui(g_glctx.u_screen_dim, g_glctx.width, g_glctx.height);
		glUniform1f(g_glctx.u_zoom, g_glctx.zoom);
		clear_colour(g_glctx.clear_colour);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glfwSwapBuffers(g_glctx.window);
	}

	return 0;
}
