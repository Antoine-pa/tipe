#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

struct Object_s {
  int id; //0 : sphere | 1 : torus | 2 : cylinder | 3 : box | 4 : plan
  float color[4];
  float pos[4];
  float rot[4]; //normale for plans
  float size[4];
  float radius;
  float thickness; //thickness for torus and rounding for cylinder and box
};

typedef struct Object_s Object_t;

const int obj_size = 96;

struct Scene_s {
  Object_t** obj;
  int nb;
};

typedef struct Scene_s Scene_t;

Object_t* init_object(int id,
                      float r, float g, float b,
                      float x, float y, float z,
                      float alpha, float beta, float gamma,
                      float size_x, float size_y, float size_z,
                      float radius,
                      float thickness) {

  Object_t* o = malloc(sizeof(Object_t));
  
  o->id = id;

  o->color[0] = r;
  o->color[1] = g;
  o->color[2] = b;
  o->color[3] = 2.0;

  o->pos[0] = x;
  o->pos[1] = y;
  o->pos[2] = z;
  o->pos[3] = 0.0;

  o->rot[0] = alpha;
  o->rot[1] = beta;
  o->rot[2] = gamma;
  o->rot[3] = 0.0;

  o->size[0] = size_x;
  o->size[1] = size_y;
  o->size[2] = size_z;
  o->size[3] = 0.0;

  o->radius = radius;

  o->thickness = thickness;
  return o;
}

void print_object(Object_t* o) {
  printf("type : %d\ncolor : %f, %f, %f\n", o->id, o->color[0], o->color[1], o->color[2]);
  printf("position : %f, %f, %f\n", o->pos[0], o->pos[1], o->pos[2]);
  printf("rotation : %f, %f, %f\n", o->rot[0], o->rot[1], o->rot[2]);
  printf("size : %f, %f, %f\n", o->size[0], o->size[1], o->size[2]);
  printf("radius : %f\nthickness : %f\n=======\n", o->radius, o->thickness);
}

Scene_t* init_scene() {
  int nb_objects = 6;
  Scene_t* scene = malloc(sizeof(Scene_t));
  Object_t** objs = malloc(sizeof(Object_t*)*nb_objects);
  objs[0] = init_object(0,   0.06, 0.04, 1.0,   0.0, 0.0, 0.0,    0.0, 0.0, 0.0,     0.0, 0.0, 0.0,   0.5,  0.0);
  objs[1] = init_object(0,   0.3, 0.04, 0.06,   0.1, 0.45, -0.1,  0.0, 0.0, 0.0,     0.0, 0.0, 0.0,   0.2,  0.0);
  objs[2] = init_object(1,   1.0, 0.4, 0.0,     0.0, 0.2, 0.1,    0.0, 0.37, 0.93,   0.0, 0.0, 0.0,   0.8,  0.1);
  objs[3] = init_object(2,   1.0, 0.2, 0.1,     0.6, 0.3, 0.4,    0.1, 0.4, 0.3,     0.12, 0.4, 0.0,  0.0,  0.3);
  objs[4] = init_object(3,   0.96, 0.41, 0.6,   -0.65, 0.5, 0.0,  0.0, 0.5, 0.2,     0.2, 0.2, 0.2,   0.0,  0.05);
  objs[5] = init_object(4,   0.25, 0.45, 0.5,   0.0, 0.0, 0.0,    0.0, 1.0, 0.0,     0.0, 0.0, 0.0,   0.0,  0.0);
  scene->obj = objs;
  scene->nb = nb_objects;
  return scene;
}

void free_scene(Scene_t* scene) {
  for(int i = 0; i<scene->nb; i++) {
    free(scene->obj[i]);
  }
  free(scene->obj);
  free(scene);
}

static double geometry[4] = { 0, };
static double mouse[4] = { 0, };

static GLint prog = 0;
static GLenum tex[4];
static GLuint ubo = 0;
static Scene_t* scene = NULL;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    double xpos, ypos;
    int x0, y0, width, height;

    if (button != GLFW_MOUSE_BUTTON_LEFT)
        return;

    if (action == GLFW_PRESS) {
        glfwGetWindowPos(window, &x0, &y0);
        glfwGetWindowSize(window, &width, &height);
        glfwGetCursorPos(window, &xpos, &ypos);

        if (geometry[0] > 0.1 && geometry[1] > 0.1) {
            mouse[2] = mouse[0] = geometry[2] + x0 + xpos;
            mouse[3] = mouse[1] = geometry[1] - geometry[3] - y0 - ypos;
        } else {
            mouse[2] = mouse[0] = xpos;
            mouse[3] = mouse[1] = height - ypos;
        }
    } else {
        mouse[2] = -1;
        mouse[3] = -1;
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    int x0, y0, height;
    char msg[1000];

    glfwGetWindowPos(window, &x0, &y0);
    glfwGetWindowSize(window, NULL, &height);

    if (geometry[0] > 0.1 && geometry[1] > 0.1) {
        mouse[0] = geometry[2] + x0 + xpos;
        mouse[1] = geometry[1] - geometry[3] - y0 - ypos;
    } else {
        mouse[0] = xpos;
        mouse[1] = height - ypos;
    }

    snprintf(msg, sizeof(msg), "iMouse:%.0f,%.0f,%.0f,%.0f", mouse[0], mouse[1], mouse[2], mouse[3]);
}


void keyboard_handler(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if ((key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if ((key == GLFW_KEY_F || key == GLFW_KEY_ENTER) && action == GLFW_PRESS && (mods & GLFW_MOD_ALT)) {
        if (glfwGetWindowMonitor(window) == NULL)
            glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, 800, 600, GLFW_DONT_CARE);
        else
            glfwSetWindowMonitor(window, NULL, 0, 0, 800, 600, GLFW_DONT_CARE);
    }
}

void load_scene(Scene_t* scene) {
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    GLvoid* bufferData = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
    if(bufferData) {
        for(int i = 0; i < scene->nb; i++) {
            Object_t* obj = scene->obj[i];
            memcpy(bufferData + obj_size * i, &obj->id, sizeof(int));
            memcpy(bufferData + obj_size * i + 16, obj->color, sizeof(float) * 4);
            memcpy(bufferData + obj_size * i + 32, obj->pos, sizeof(float) * 4);
            memcpy(bufferData + obj_size * i + 48, obj->rot, sizeof(float) * 4);
            memcpy(bufferData + obj_size * i + 64, obj->size, sizeof(float) * 4);
            memcpy(bufferData + obj_size * i + 80, &obj->radius, sizeof(float));
            memcpy(bufferData + obj_size * i + 84, &obj->thickness, sizeof(float));
        }
        glUnmapBuffer(GL_UNIFORM_BUFFER);
    } else {
        fprintf(stderr, "Failed to map uniform buffer.\n");
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}


void display(GLFWwindow* window) {
  
  static int frames, last_time;
  int x0, y0, width, height, ticks;
  GLint uindex;

  glUseProgram(prog);
  
  glfwGetWindowPos(window, &x0, &y0);
  glfwGetWindowSize(window, &width, &height);

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  ticks = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

  if (frames == 0)
    last_time = ticks;

  frames++;

  if (ticks - last_time >= 5000)
    {
      fprintf (stderr, "FPS: %.2f\n", 1000.0 * frames / (ticks - last_time));
      frames = 0;
    }

  uindex = glGetUniformLocation (prog, "iGlobalTime");
  if (uindex >= 0)
    glUniform1f (uindex, ((float) ticks) / 1000.0);

  uindex = glGetUniformLocation (prog, "iTime");
  if (uindex >= 0)
    glUniform1f (uindex, ((float) ticks) / 1000.0);

  uindex = glGetUniformLocation (prog, "iResolution");
  if (uindex >= 0)
    {
      if (geometry[0] > 0.1 && geometry[1] > 0.1)
        glUniform3f (uindex, geometry[0], geometry[1], 1.0);
      else
        glUniform3f (uindex, width, height, 1.0);
    }

  uindex = glGetUniformLocation (prog, "iOffset");
  if (uindex >= 0)
    {
      if (geometry[0] > 0.1 && geometry[1] > 0.1)
        {
          glUniform2f (uindex,
                       x0 + geometry[2],
                       geometry[1] - (y0 + height) - geometry[3]);
        }
      else
        {
          glUniform2f (uindex, 0.0, 0.0);
        }
    }

  uindex = glGetUniformLocation (prog, "iMouse");
  if (uindex >= 0)
    glUniform4f (uindex, mouse[0],  mouse[1], mouse[2], mouse[3]);


  uindex = glGetUniformLocation (prog, "iChannel0");
  if (uindex >= 0)
    {
      glActiveTexture (GL_TEXTURE0 + 0);
      glBindTexture (GL_TEXTURE_2D, tex[0]);
      glUniform1i (uindex, 0);
    }

  uindex = glGetUniformLocation (prog, "iChannel1");
  if (uindex >= 0)
    {
      glActiveTexture (GL_TEXTURE0 + 1);
      glBindTexture (GL_TEXTURE_2D, tex[1]);
      glUniform1i (uindex, 1);
    }

  uindex = glGetUniformLocation (prog, "iChannel2");
  if (uindex >= 0)
    {
      glActiveTexture (GL_TEXTURE0 + 2);
      glBindTexture (GL_TEXTURE_2D, tex[2]);
      glUniform1i (uindex, 2);
    }

  uindex = glGetUniformLocation (prog, "iChannel3");
  if (uindex >= 0)
    {
      glActiveTexture (GL_TEXTURE0 + 3);
      glBindTexture (GL_TEXTURE_2D, tex[3]);
      glUniform1i (uindex, 3);
    }

  uindex = glGetUniformLocation (prog, "resolution");
  if (uindex >= 0)
    {
      if (geometry[0] > 0.1 && geometry[1] > 0.1)
        glUniform2f (uindex, geometry[0], geometry[1]);
      else
        glUniform2f (uindex, width, height);
    }

  uindex = glGetUniformLocation (prog, "led_color");
  if (uindex >= 0)
    glUniform3f (uindex, 0.5, 0.3, 0.8);
  
}


GLint compile_shader (const GLenum  shader_type,
                      const GLchar *shader_source) {
  GLuint shader = glCreateShader (shader_type);
  GLint status = GL_FALSE;
  GLint loglen;
  GLchar *error_message;

  glShaderSource (shader, 1, &shader_source, NULL);
  glCompileShader (shader);

  glGetShaderiv (shader, GL_COMPILE_STATUS, &status);
  if (status == GL_TRUE)
    return shader;

  glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &loglen);
  error_message = calloc (loglen, sizeof (GLchar));
  glGetShaderInfoLog (shader, loglen, NULL, error_message);
  fprintf (stderr, "shader failed to compile:\n   %s\n", error_message);
  free (error_message);

  return -1;
}


GLint link_program(const GLchar* vert_code, const GLchar* frag_code) {
  GLint frag, vert, program;
  GLint status = GL_FALSE;
  GLint loglen, n_uniforms;
  GLchar *error_message;
  GLint i;

  GLchar name[80];
  GLsizei namelen;

  vert = compile_shader(GL_VERTEX_SHADER, vert_code);
  frag = compile_shader(GL_FRAGMENT_SHADER, frag_code);
  if (frag < 0 || vert < 0)
    return -1;

  program = glCreateProgram ();

  glAttachShader(program, vert);
  glAttachShader(program, frag);
  glLinkProgram(program);

  glGetProgramiv (program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE)
    {
      glGetProgramiv (program, GL_INFO_LOG_LENGTH, &loglen);
      error_message = calloc (loglen, sizeof (GLchar));
      glGetProgramInfoLog (program, loglen, NULL, error_message);
      fprintf (stderr, "program failed to link:\n   %s\n", error_message);
      free (error_message);
      return -1;
    }

  /* diagnostics */
  glGetProgramiv (program, GL_ACTIVE_UNIFORMS, &n_uniforms);
  fprintf (stderr, "%d uniforms:\n", n_uniforms);

  for (i = 0; i < n_uniforms; i++)
    {
      GLint size;
      GLenum type;

      glGetActiveUniform (program, i, 79, &namelen, &size, &type, name);
      name[namelen] = '\0';
      fprintf (stderr, "  %2d: %-20s (type: 0x%04x, size: %d)\n", i, name, type, size);
    }

  return program;
}

void init_glew (void) {
  GLenum status;
  
  status = glewInit ();

  if (status != GLEW_OK)
    {
      fprintf (stderr, "glewInit error: %s\n", glewGetErrorString (status));
      glfwTerminate();
      exit(-1);
    }

  fprintf (stderr,
           "GL_VERSION   : %s\nGL_VENDOR    : %s\nGL_RENDERER  : %s\n"
           "GLEW_VERSION : %s\nGLSL VERSION : %s\n\n",
           glGetString (GL_VERSION), glGetString (GL_VENDOR),
           glGetString (GL_RENDERER), glewGetString (GLEW_VERSION),
           glGetString (GL_SHADING_LANGUAGE_VERSION));

  if (!GLEW_VERSION_3_2) {
        fprintf(stderr, "OpenGL 3.2 n'est pas disponible.\n");
        glfwTerminate();
        exit(-1);
    }
}


char * load_file(char *filename) {
  FILE *f;
  int size;
  char *data;

  f = fopen (filename, "rb");
  if (f == NULL)
    {
      perror ("error opening file");
      return NULL;
    }

  fseek (f, 0, SEEK_END);
  size = ftell (f);
  fseek (f, 0, SEEK_SET);

  data = malloc (size + 1);
  if (fread (data, size, 1, f) < 1)
    {
      fprintf (stderr, "problem reading file %s\n", filename);
      free (data);
      return NULL;
    }
  fclose(f);

  data[size] = '\0';

  return data;
}

void error_callback(int error, const char* description) {
  fprintf(stderr, "Erreur GLFW: %s\n", description);
}

GLFWwindow* init_glfw_window() {
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    exit(-1);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow* window = glfwCreateWindow(800, 600, "Raymarcher", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to create GLFW window\n");
    glfwTerminate();
    exit(-1);
  }

  glfwMakeContextCurrent(window);

  glfwSetKeyCallback(window, keyboard_handler);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetCursorPosCallback(window, cursor_position_callback);
  return window;
}



int main () {

  GLFWwindow* window = init_glfw_window();
  init_glew();
  scene = init_scene();

  char* vert_code = load_file("/home/antoine/Documents/prepa/tipe/vertex.glsl");
  char* frag_code = load_file("/home/antoine/Documents/prepa/tipe/frag.glsl");
  if (!vert_code || !frag_code) {
    fprintf(stderr, "Failed to load shader code\n");
    return -1;
  }

  prog = link_program((const GLchar*)vert_code, (const GLchar*)frag_code);

  free(vert_code);
  free(frag_code);

  if (prog < 0){
    fprintf (stderr, "Failed to link shaders program. Aborting\n");
    exit (-1);
  }

  GLfloat vertices[] = {
    -1.0f,  1.0f,
    -1.0f, -1.0f,
    1.0f, -1.0f,
    1.0f,  1.0f
  };

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  
  glGenBuffers(1, &ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
  glBufferData(GL_UNIFORM_BUFFER, obj_size*scene->nb, NULL, GL_STATIC_DRAW); // allocate 96*scene->nb bytes of memory
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  
  
  load_scene(scene);
  
  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    display(window);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4); // Dessiner un carré avec 4 vertices
    glBindVertexArray(0);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  free_scene(scene);
  return 0;
}