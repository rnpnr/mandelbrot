/* see LICENSE for licensing details */
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <GL/gl.h>
#include <GLES3/gl32.h>
#include <GLFW/glfw3.h>

typedef float     f32;
typedef uint8_t   u8;
typedef int32_t   i32;
typedef uint32_t  u32;
typedef ptrdiff_t size;

typedef struct { size len; u8 *data; } s8;

typedef union {
	struct { u8 a, b, g, r; };
	u32 rgba;
} Colour;

static struct {
	GLFWwindow *window;
	u32 vao, vbo;
	i32 pid;
	i32 height, width;
	i32 u_screen_dim;
	Colour clear_colour;
} g_glctx;

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
	if (g_glctx.u_screen_dim != -1)
		glUniform2ui(g_glctx.u_screen_dim, w, h);
}

static void
key_callback(GLFWwindow *win, i32 key, i32 sc, i32 action, i32 mod)
{
	(void)sc; (void)mod;
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(win, GL_TRUE);
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
	glfwSwapInterval(0);
	
	glfwSetFramebufferSizeCallback(g_glctx.window, fb_callback);
	glfwSetKeyCallback(g_glctx.window, key_callback);
	//glfwSetScrollCallback(glctx.window, scroll_callback);

	g_glctx.clear_colour = (Colour){ .r = 64, .b = 64, .g = 64, .a = 255 };
	clear_colour(g_glctx.clear_colour);

	init_renderer();

	return 0;
}

static s8
read_whole_file(char *file)
{
	s8 ret = {0};
	i32 fd = open(file, O_RDONLY);
	if (fd < 0)
		return ret;
	ret.len  = lseek(fd, 0L, SEEK_END);
	ret.data = malloc(ret.len);
	lseek(fd, 0L, SEEK_SET);
	if (ret.data == NULL) {
		ret.len = 0;
		close(fd);
		return ret;
	}
	if (ret.len != read(fd, ret.data, ret.len)) {
		ret.len = 0;
		free(ret.data);
	}
	close(fd);
	return ret;
}

static u32
compile_shader(u32 type, s8 s)
{
	u32 sid = glCreateShader(type);
	glShaderSource(sid, 1, (const char **)&s.data, (int *)&s.len);
	glCompileShader(sid);

	i32 res = 0;
	glGetShaderiv(sid, GL_COMPILE_STATUS, &res);
	if (res != GL_TRUE) {
		i32 len, len2;
		glGetShaderiv(sid, GL_INFO_LOG_LENGTH, &len);
		char *data = malloc(len);
		glGetShaderInfoLog(sid, len, &len2, data);
		fputs("compile_shader: ", stderr);
		fwrite(data, 1, len2, stderr);
		fputc('\n', stderr);
		free(data);
		glDeleteShader(sid);
		return 0;
	}

	return sid;
}

static i32
program_from_files(char *vert, char *frag)
{
	s8 vertex   = read_whole_file(vert);
	s8 fragment = read_whole_file(frag);
	if (vertex.len == 0 || fragment.len == 0)
		return -1;
	i32 pid = glCreateProgram();
	u32 vid = compile_shader(GL_VERTEX_SHADER,   vertex);
	u32 fid = compile_shader(GL_FRAGMENT_SHADER, fragment);

	if (fid == 0 || vid == 0)
		return -1;

	glAttachShader(pid, vid);
	glAttachShader(pid, fid);
	glLinkProgram(pid);
	glValidateProgram(pid);
	glDeleteShader(vid);
	glDeleteShader(fid);
	glUseProgram(pid);

	g_glctx.u_screen_dim = glGetUniformLocation(pid, "u_screen_dim");
	if (g_glctx.u_screen_dim != -1)
		glUniform2ui(g_glctx.u_screen_dim, g_glctx.width, g_glctx.height);

	free(vertex.data);
	free(fragment.data);

	return pid;
}

int
main(void)
{
	if (!glfwInit())
		return -1;
	glfwSetErrorCallback(error_callback);

	spawn_window();

	if (g_glctx.window == NULL) {
		glfwTerminate();
		return -1;
	}

	g_glctx.pid = program_from_files("vert.glsl", "frag.glsl");
	if (g_glctx.pid == -1) {
		glfwTerminate();
		return -1;
	}

	u32 fcount = 0;
	f32 last_time = 0;
	while (!glfwWindowShouldClose(g_glctx.window)) {
		glfwPollEvents();

		f32 current_time = glfwGetTime();
		f32 dt = current_time - last_time;
		last_time = current_time;
		if (++fcount > 1000) {
			printf("FPS: %0.03f | dt = %0.03f [ms]\n",
			       1 / dt, dt * 1e3);
			fcount = 0;
		}
		
		clear_colour(g_glctx.clear_colour);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glfwSwapBuffers(g_glctx.window);
	}

	return 0;
}
