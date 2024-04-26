/* Standalone Shadertoy
 * Copyright (C) 2014 Simon Budig <simon@budig.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

//#include <sys/types.h>

#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut_ext.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

struct Object_s {
  int id; //0 : sphere | 1 : torus | 2 : cylinder | 3 : box | 4 : plan
  float* color;
  float* pos;
  float* rot; //normale for plans
  float* size;
  float radius;
  float thickness; //thickness for torus and rounding for cylinder and box
};

typedef struct Object_s Object_t;

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

  o->color = malloc(sizeof(float)*3);
  o->color[0] = r;
  o->color[1] = g;
  o->color[2] = b;

  o->pos = malloc(sizeof(float)*3);
  o->pos[0] = x;
  o->pos[1] = y;
  o->pos[2] = z;

  o->rot = malloc(sizeof(float)*3);
  o->rot[0] = alpha;
  o->rot[1] = beta;
  o->rot[2] = gamma;

  o->size = malloc(sizeof(float)*3);
  o->size[0] = size_x;
  o->size[1] = size_y;
  o->size[2] = size_z;

  o->radius = radius;

  o->thickness = thickness;
  return o;
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
    free(scene->obj[i]->color);
    free(scene->obj[i]->pos);
    free(scene->obj[i]->rot);
    free(scene->obj[i]->size);
  }
  free(scene);
}

/* width, height, x0, y0 (top left) */
static double geometry[4] = { 0, };

/* x, y, x_press, y_press  (in target coords) */
static double mouse[4] = { 0, };

static GLint prog = 0;
static GLenum tex[4];

void
mouse_press_handler (int button, int state, int x, int y)
{
  char msg[1000];
  int x0, y0, height;

  if (button != GLUT_LEFT_BUTTON)
    return;

  if (state == GLUT_DOWN)
    {
      x0     = glutGet (GLUT_WINDOW_X);
      y0     = glutGet (GLUT_WINDOW_Y);
      height = glutGet (GLUT_WINDOW_HEIGHT);

      if (geometry[0] > 0.1 && geometry[1] > 0.1)
        {
          mouse[2] = mouse[0] =               geometry[2] + x0 + x;
          mouse[3] = mouse[1] = geometry[1] - geometry[3] - y0 - y;
        }
      else
        {
          mouse[2] = mouse[0] = x;
          mouse[3] = mouse[1] = height - y;
        }
    }
  else
    {
      mouse[2] = -1;
      mouse[3] = -1;
    }

  snprintf (msg, sizeof (msg), "iMouse:%.0f,%.0f,%.0f,%.0f",
            mouse[0], mouse[1], mouse[2], mouse[3]);
}


void
mouse_move_handler (int x, int y)
{
  char msg[1000];
  int x0, y0, height;

      x0     = glutGet (GLUT_WINDOW_X);
      y0     = glutGet (GLUT_WINDOW_Y);
      height = glutGet (GLUT_WINDOW_HEIGHT);

      if (geometry[0] > 0.1 && geometry[1] > 0.1)
        {
          mouse[0] =               geometry[2] + x0 + x;
          mouse[1] = geometry[1] - geometry[3] - y0 - y;
        }
      else
        {
          mouse[0] = x;
          mouse[1] = height - y;
        }

  snprintf (msg, sizeof (msg), "iMouse:%.0f,%.0f,%.0f,%.0f",
            mouse[0], mouse[1], mouse[2], mouse[3]);
}


void
keyboard_handler (unsigned char key, int x, int y)
{
  switch (key)
    {
      case '\x1b':  /* Escape */
      case 'q':
      case 'Q':
        glutLeaveMainLoop ();
        break;

      case 'f': /* fullscreen */
      case 'F':
        glutFullScreenToggle ();
        break;

      default:
        break;
    }
}


void
redisplay (int value)
{
  glutPostRedisplay ();
  glutTimerFunc (value, redisplay, value);
}

void load_scene(Scene_t* scene) {
  GLint uindex;
  char address[1000];
  
  for(int i = 0; i<scene->nb; i++) {
    sprintf(address, "iObjects[%d].id", i);
    uindex = glGetUniformLocation (prog, address);
    glUniform1i (uindex, (scene->obj[i])->id);

    sprintf(address, "iObjects[%d].color", i);
    uindex = glGetUniformLocation (prog, address);
    glUniform3f (uindex, (scene->obj[i])->color[0], (scene->obj[i])->color[1], (scene->obj[i])->color[2]);

    sprintf(address, "iObjects[%d].pos", i);
    uindex = glGetUniformLocation (prog, address);
    glUniform3f (uindex,  (scene->obj[i])->pos[0], (scene->obj[i])->pos[1], (scene->obj[i])->pos[2]);

    sprintf(address, "iObjects[%d].rot", i);
    uindex = glGetUniformLocation (prog, address);
    glUniform3f (uindex,  (scene->obj[i])->rot[0], (scene->obj[i])->rot[1], (scene->obj[i])->rot[2]);

    sprintf(address, "iObjects[%d].size", i);
    uindex = glGetUniformLocation (prog, address);
    glUniform3f (uindex, (scene->obj[i])->size[0], (scene->obj[i])->size[1], (scene->obj[i])->size[2]);

    sprintf(address, "iObjects[%d].radius", i);
    uindex = glGetUniformLocation (prog, address);
    glUniform1f (uindex, (scene->obj[i])->radius);

    sprintf(address, "iObjects[%d].thickness", i);
    uindex = glGetUniformLocation (prog, address);
    glUniform1f (uindex, (scene->obj[i])->thickness);
  }
}


void
display (void)
{
  static int frames, last_time;
  Scene_t* scene = init_scene();
  int x0, y0, width, height, ticks;
  GLint uindex;
  struct timespec ts;

  glUseProgram (prog);

  x0     = glutGet (GLUT_WINDOW_X);
  y0     = glutGet (GLUT_WINDOW_Y);
  width  = glutGet (GLUT_WINDOW_WIDTH);
  height = glutGet (GLUT_WINDOW_HEIGHT);
  clock_gettime (CLOCK_MONOTONIC_RAW, &ts);
  ticks  = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

  if (frames == 0)
    last_time = ticks;

  frames++;

  if (ticks - last_time >= 5000)
    {
      fprintf (stderr, "FPS: %.2f\n", 1000.0 * frames / (ticks - last_time));
      frames = 0;
    }

  load_scene(scene);

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

  glClear (GL_COLOR_BUFFER_BIT);
  glRectf (-1.0, -1.0, 1.0, 1.0);

  glutSwapBuffers ();

  free_scene(scene);
}


int
load_texture (char    *filename,
              GLenum   type,
              GLenum  *tex_id,
              char     nearest,
              char     repeat)
{
  GdkPixbuf *pixbuf;
  int width, height;
  uint8_t *data;
  GLfloat *tex_data;
  int rowstride;
  int cpp, bps;
  int x, y, c;

  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  data = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  bps = gdk_pixbuf_get_bits_per_sample (pixbuf);
  cpp = gdk_pixbuf_get_n_channels (pixbuf);

  if (bps != 8 && bps != 16)
    {
      fprintf (stderr, "unexpected bits per sample: %d\n", bps);
      return 0;
    }


  if (cpp != 3 && cpp != 4)
    {
      fprintf (stderr, "unexpected n_channels: %d\n", cpp);
      return 0;
    }

  tex_data = malloc (width * height * cpp * sizeof (GLfloat));
  for (y = 0; y < height; y++)
    {
      uint8_t  *cur_row8  = (uint8_t *)  (data + y * rowstride);
      uint16_t *cur_row16 = (uint16_t *) (data + y * rowstride);

      for (x = 0; x < width; x++)
        {
          for (c = 0; c < cpp; c++)
            {
              if (bps == 8)
                tex_data[(y * width + x) * cpp + c] = ((GLfloat) cur_row8[x * cpp + c]) / 255.0;
              else
                tex_data[(y * width + x) * cpp + c] = ((GLfloat) cur_row16[x * cpp + c]) / 65535.0;
            }
        }
    }

  glGenTextures (1, tex_id);
  glBindTexture (type, *tex_id);
  glTexImage2D (type, 0, GL_RGBA,
                width, height,
                0, cpp == 3 ? GL_RGB : GL_RGBA,
                GL_FLOAT,
                tex_data);
  if (nearest)
    {
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
  else
    {
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glGenerateMipmap (GL_TEXTURE_2D);
    }

  if (repeat)
    {
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
  else
    {
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    }

  free (tex_data);
  g_object_unref (pixbuf);

  fprintf (stderr, "texture: %s, %dx%d, %d (%d) --> id %d\n",
           filename, width, height, cpp, bps, *tex_id);

  return 1;
}


GLint
compile_shader (const GLenum  shader_type,
                const GLchar *shader_source)
{
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


GLint
link_program (const GLchar *shader_source)
{
  GLint frag, program;
  GLint status = GL_FALSE;
  GLint loglen, n_uniforms;
  GLchar *error_message;
  GLint i;

  GLchar name[80];
  GLsizei namelen;

  frag = compile_shader (GL_FRAGMENT_SHADER, shader_source);
  if (frag < 0)
    return -1;

  program = glCreateProgram ();

  glAttachShader (program, frag);
  glLinkProgram (program);
  // glDeleteShader (frag);

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

void
init_glew (void)
{
  GLenum status;

  status = glewInit ();

  if (status != GLEW_OK)
    {
      fprintf (stderr, "glewInit error: %s\n", glewGetErrorString (status));
      exit (-1);
    }

  fprintf (stderr,
           "GL_VERSION   : %s\nGL_VENDOR    : %s\nGL_RENDERER  : %s\n"
           "GLEW_VERSION : %s\nGLSL VERSION : %s\n\n",
           glGetString (GL_VERSION), glGetString (GL_VENDOR),
           glGetString (GL_RENDERER), glewGetString (GLEW_VERSION),
           glGetString (GL_SHADING_LANGUAGE_VERSION));

  if (!GLEW_VERSION_2_1)
    {
      fprintf (stderr, "OpenGL 2.1 or better required for GLSL support.");
      exit (-1);
    }
}


char *
load_file (char *filename)
{
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


int main (int argc, char *argv[]) {
  char *frag_code = NULL;
  glutInit (&argc, argv);

  static struct option long_options[] = {
      { "texture",  required_argument, NULL,  't' },
      { "geometry", required_argument, NULL,  'g' },
      { "help",     no_argument, NULL,        'h' },
      { 0,          0,                 NULL,  0   }
  };

  glutInitWindowSize (800, 600);
  glutInitDisplayMode (GLUT_RGBA | GLUT_DOUBLE);
  glutCreateWindow ("Shadertoy");

  init_glew ();

  /* option parsing */
  while (1)
    {
      int c, slot, i;
      char nearest, repeat;

      c = getopt_long (argc, argv, ":t:g:?", long_options, NULL);
      if (c == -1)
        break;

      switch (c)
        {
          case 'g':
            for (i = 0; i < 4; i++)
              {
                char *token = strsep (&optarg, "x+");
                if (!token)
                  break;
                geometry[i] = atof (token);
              }

            fprintf (stderr, "geometry: %.0fx%.0f+%.0f+%.0f\n",
                     geometry[0], geometry[1], geometry[2], geometry[3]);
            break;

          case 't':
            if (optarg[0] <  '0' || optarg[0] >  '3' || strchr (optarg, ':') == NULL)
              {
                fprintf (stderr, "Argument for texture file needs a slot from 0 to 3\n");
                exit (1);
              }

            slot = optarg[0] - '0';

            repeat = 1;
            nearest = 0;

            for (c = 1; optarg[c] != ':' && optarg[c] != '\0'; c++)
              {
                switch (optarg[c])
                  {
                    case 'r':
                      repeat = 1;
                      break;
                    case 'o':
                      repeat = 0;
                      break;
                    case 'i':
                      nearest = 0;
                      break;
                    case 'n':
                      nearest = 1;
                      break;
                    default:
                      break;
                  }
              }

            if (optarg[c] != ':' ||
                !load_texture (optarg + c + 1, GL_TEXTURE_2D, &tex[slot], nearest, repeat))
              {
                fprintf (stderr, "Failed to load texture. Aborting.\n");
                exit (1);
              }
            break;

          case 'h':
          case ':':
          default:
            fprintf (stderr, "Usage:\n  %s [options] <shaderfile>\n", argv[0]);
            fprintf (stderr, "Options:    --help\n");
            fprintf (stderr, "            --texture [0-3]:<textureimage>\n");
            exit (c == ':' ? 1 : 0);
            break;
        }
    }


  if (optind != argc - 1)
    {
      fprintf (stderr, "No shaderfile specified. Aborting.\n");
      exit (-1);
    }

  frag_code = load_file (argv[optind]);
  if (!frag_code)
    {
      fprintf (stderr, "Failed to load Shaderfile. Aborting.\n");
      exit (-1);
    }

  prog = link_program (frag_code);
  if (prog < 0)
    {
      fprintf (stderr, "Failed to link shader program. Aborting\n");
      exit (-1);
    }

  glutDisplayFunc  (display);
  glutMouseFunc    (mouse_press_handler);
  glutMotionFunc   (mouse_move_handler);
  glutKeyboardFunc (keyboard_handler);

  redisplay (1000/60);

  glutMainLoop ();

  return 0;
}

