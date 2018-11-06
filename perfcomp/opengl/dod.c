#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <GL/glew.h>

#define WIN_WIDTH     (1280)
#define WIN_HEIGHT    (720)
#define MAX_VBO_BYTES (1024ll * 1024ll * 8ll) // 8M de VRAM
#define RECT_SIZE     ((long)(sizeof(struct vertex) * 4ll))
#define MAX_RECTS     (1000000ll)

struct color {
	GLfloat r, g, b;
};

struct vec2f {
	GLfloat x, y;
};

struct vertex {
	struct vec2f pos;
	struct color color;
};


static struct vertex vertexs[MAX_RECTS * 4];
static struct vec2f vels[MAX_RECTS];
static struct vec2f poss[MAX_RECTS];
static GLfloat sizes[MAX_RECTS];
static long long nrects = 0;

static SDL_Window* window = NULL;
static SDL_GLContext glcontext = NULL;
static GLuint vao = 0, vbo = 0;
static GLuint sp_id = 0, vs_id = 0, fs_id = 0;


static GLfloat randf(GLfloat min, GLfloat max)
{
	GLfloat retval;
	do {
		retval = 1.0f*rand()/RAND_MAX*(max -  min) + min;
	} while (!(retval > min && retval < max));
	return retval;
}

static void randf_arr(const GLfloat* const intervals, GLfloat* const result, const int count)
{
	for (int i = 0; i < count; ++i)
		result[i] = randf(intervals[i * 2], intervals[i * 2 + 1]);
}

static void init_random_engine(void)
{
	srand(time(NULL));
}


static void terminate_system(void)
{
	if (fs_id != 0) {
		glDetachShader(sp_id, fs_id);
		glDeleteShader(fs_id);
	}

	if (vs_id != 0) {
		glDetachShader(sp_id, vs_id);
		glDeleteShader(vs_id);
	}

	if (sp_id != 0)
		glDeleteProgram(sp_id);
	
	if (vbo != 0)
		glDeleteBuffers(1, &vbo);

	if (vao != 0)
		glDeleteVertexArrays(1, &vao);

	if (glcontext != NULL)
		SDL_GL_DeleteContext(glcontext);

	if (window != NULL)
		SDL_DestroyWindow(window);

	SDL_Quit();
}


static bool initialize_system(void)
{
	const GLchar* const vs_src =
	"#version 130\n"
	"in vec2 pos;\n"
	"in vec3 rgb;\n"
	"out vec4 frag_color;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = vec4(pos, 0.0, 1.0);\n"
	"	frag_color = vec4(rgb, 1.0);\n"
	"}\n";
	const GLchar* const fs_src =
	"#version 130\n"
	"in vec4 frag_color;\n"
	"out vec4 outcolor;\n"
	"void main()\n"
	"{\n"
	"	outcolor = frag_color;\n"
	"}\n";

	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
		return false;
	}

	window = SDL_CreateWindow("DOD DEMO",
	                          SDL_WINDOWPOS_CENTERED,
	                          SDL_WINDOWPOS_CENTERED,
	                          WIN_WIDTH, WIN_HEIGHT,
	                          SDL_WINDOW_OPENGL);
	if (window == NULL) {
		fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
		terminate_system();
		return false;
	}

	if (SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) < 0) {
		fprintf(stderr, "Couldn't set GL DOUBLE BUFFER\n");
		terminate_system();
		return false;
	}

	glcontext = SDL_GL_CreateContext(window);
	if (glcontext == NULL) {
		fprintf(stderr, "Couldn't create GL Context: %s\n", SDL_GetError());
		terminate_system();
		return false;
	}

	GLenum err;
	if ((err = glewInit()) != GLEW_OK) {
		fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
		terminate_system();
		return false;
	}

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, MAX_VBO_BYTES,
	             NULL, GL_DYNAMIC_DRAW);

	sp_id = glCreateProgram();
	if (sp_id == 0) {
		fprintf(stderr, "Couldn't create GL Program\n");
		terminate_system();
		return false;
	}

	vs_id = glCreateShader(GL_VERTEX_SHADER);
	if (vs_id == 0) {
		fprintf(stderr, "Couldn't create Vertex Shader\n");
		terminate_system();
		return false;
	}

	fs_id = glCreateShader(GL_FRAGMENT_SHADER);
	if (fs_id == 0) {
		fprintf(stderr, "Couldn't create Fragment Shader\n");
		terminate_system();
		return false;
	}

	// compile vertex shader
	glShaderSource(vs_id, 1, &vs_src, NULL);
	glCompileShader(vs_id);
	
	GLint shader_success;

	glGetShaderiv(vs_id, GL_COMPILE_STATUS, &shader_success);
	if (shader_success == GL_FALSE) {
		fprintf(stderr, "Couldn't compile Vertex Shader\n");
		terminate_system();
		return false;
	}


	// compile fragment shader
	glShaderSource(fs_id, 1, &fs_src, NULL);
	glCompileShader(fs_id);
	
	glGetShaderiv(fs_id, GL_COMPILE_STATUS, &shader_success);
	if (shader_success == GL_FALSE) {
		fprintf(stderr, "Couldn't compile Fragment Shader\n");
		terminate_system();
		return false;
	}


	glAttachShader(sp_id, vs_id);
	glAttachShader(sp_id, fs_id);
	glLinkProgram(sp_id);
	glUseProgram(sp_id);

	const GLint pos_attrib = glGetAttribLocation(sp_id, "pos");
	const GLint rgb_attrib = glGetAttribLocation(sp_id, "rgb");

	glEnableVertexAttribArray(pos_attrib);
	glEnableVertexAttribArray(rgb_attrib);
	glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_TRUE,
	                      sizeof(struct vertex), NULL);
	glVertexAttribPointer(rgb_attrib, 3, GL_FLOAT, GL_TRUE,
	                      sizeof(struct vertex),
	                      (void*)(sizeof(GLfloat) * 2));

	SDL_GL_SetSwapInterval(0);
	init_random_engine();
	return true;
}

static bool handle_events(void)
{
	static SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT)
			return false;
	}
	return true;
}


static void push_rect(void)
{
	if (nrects >= MAX_RECTS) {
		printf("MAX RECTS LIMIT\n");
		return;
	}

	static const GLfloat intervals[] = {
		-0.00005, 0.00005, // posx
		-0.00005, 0.00005, // posy
		-0.0015, 0.0015,   // velx
		-0.0015, 0.0015,   // vely
		-0.1, 1.0,         // r
		-0.1, 1.0,         // g
		-0.1, 1.0,         // b
		0.0009, 0.0022     // size
	};

	static GLfloat result[(sizeof(intervals) / sizeof(GLfloat)) / 2];

	randf_arr(&intervals[0], &result[0], sizeof(result) / sizeof(GLfloat));

	const GLfloat posx = result[0];
	const GLfloat posy = result[1];
	const GLfloat velx = result[2];
	const GLfloat vely = result[3];
	const GLfloat r = result[4];
	const GLfloat g = result[5];
	const GLfloat b = result[6];
	const GLfloat size = result[7];

	poss[nrects].x = posx;
	poss[nrects].y = posy;
	vels[nrects].x = velx;
	vels[nrects].y = vely;
	sizes[nrects] = size;

	vertexs[nrects * 4].pos.x = posx - size;
	vertexs[nrects * 4].pos.y = posy - size;
	vertexs[nrects * 4].color.r = r;
	vertexs[nrects * 4].color.g = g;
	vertexs[nrects * 4].color.b = b;

	vertexs[nrects * 4 + 1].pos.x = posx + size;
	vertexs[nrects * 4 + 1].pos.y = posy - size;
	vertexs[nrects * 4 + 1].color.r = r;
	vertexs[nrects * 4 + 1].color.g = g;
	vertexs[nrects * 4 + 1].color.b = b;

	vertexs[nrects * 4 + 2].pos.x = posx + size;
	vertexs[nrects * 4 + 2].pos.y = posy + size;
	vertexs[nrects * 4 + 2].color.r = r;
	vertexs[nrects * 4 + 2].color.g = g;
	vertexs[nrects * 4 + 2].color.b = b;

	vertexs[nrects * 4 + 3].pos.x = posx - size;
	vertexs[nrects * 4 + 3].pos.y = posy + size;
	vertexs[nrects * 4 + 3].color.r = r;
	vertexs[nrects * 4 + 3].color.g = g;
	vertexs[nrects * 4 + 3].color.b = b;

	++nrects;
}



int main(int argc, char** argv)
{
	((void)argc);
	((void)argv);

	if (!initialize_system())
		return EXIT_FAILURE;

	while (handle_events()) {
		const Uint32 start_ticks = SDL_GetTicks();
		
		glClearColor(0x00, 0x00, 0x00, 0xFF);
		glClear(GL_COLOR_BUFFER_BIT);

		for (long long i = 0; i < nrects; ++i) {
			if (poss[i].x < -1.0 || poss[i].x > 1.0)
				vels[i].x = -vels[i].x;
			if (poss[i].y < -1.0 || poss[i].y > 1.0)
				vels[i].y = -vels[i].y;

			poss[i].x += vels[i].x;
			poss[i].y += vels[i].y;

			const GLfloat posx = poss[i].x;
			const GLfloat posy = poss[i].y;

			vertexs[i * 4].pos.x = posx - sizes[i];
			vertexs[i * 4].pos.y = posy - sizes[i];

			vertexs[i * 4 + 1].pos.x = posx + sizes[i];
			vertexs[i * 4 + 1].pos.y = posy - sizes[i];

			vertexs[i * 4 + 2].pos.x = posx + sizes[i];
			vertexs[i * 4 + 2].pos.y = posy + sizes[i];

			vertexs[i * 4 + 3].pos.x = posx - sizes[i];
			vertexs[i * 4 + 3].pos.y = posy + sizes[i];
		}

		const long long max_rects_per_pack = MAX_VBO_BYTES / (RECT_SIZE);

		if (nrects < max_rects_per_pack) {
			glBufferSubData(GL_ARRAY_BUFFER, 0, RECT_SIZE * nrects, vertexs);
			glDrawArrays(GL_QUADS, 0, nrects * 4);
		} else {
			const long long packs = nrects / max_rects_per_pack;
			for (long long i = 0; i < packs; ++i) {
				glBufferSubData(GL_ARRAY_BUFFER, 0,
				                RECT_SIZE * max_rects_per_pack,
				                &vertexs[i * 4 * max_rects_per_pack]);
				glDrawArrays(GL_QUADS, 0, max_rects_per_pack * 4);
			}
			const long long remaining = nrects - (packs * max_rects_per_pack);
			glBufferSubData(GL_ARRAY_BUFFER, 0,
			                RECT_SIZE * remaining,
			                &vertexs[packs * max_rects_per_pack * 4]);
			glDrawArrays(GL_QUADS, 0, remaining * 4);
		}

		SDL_GL_SwapWindow(window);

		const Uint32 end_ticks = SDL_GetTicks();
		if ((end_ticks - start_ticks) < 16) {
			for (int i = 0; i < 50; ++i)
				push_rect();
		}

		printf("FPS: %.2lf\nRECTS: %lld\n", 1000.0 / (end_ticks - start_ticks), nrects);
	}

	terminate_system();
	return EXIT_SUCCESS;
}

