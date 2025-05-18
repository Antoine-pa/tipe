#define _POSIX_C_SOURCE 199309L
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <unistd.h> //getcwd
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <time.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "kdtree.h"
#include <GL/glu.h>  // Pour gluSphere, gluCylinder, etc.
#include "objects.h"  // Inclure le nouveau fichier objects.h

#ifdef DEBUG_KD_TREE
#define KD_DEBUG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define KD_DEBUG(fmt, ...) /* rien */
#endif

static double geometry[4] = { 0, };
static double mouse[4] = { 0, };

static GLint prog = 0;
static GLuint scene_data_ssbo = 0;
static Scene_t* scene = NULL;
int view_perf;
int output;
int algo;
int display_kdtree_shader;
static int show_kdtree_3d = 0;  // État pour afficher ou non le KD-tree en 3D
static GLuint kdtree_ssbo = 0;

void print_object(Object_t o) {
  printf("type : %d\ncolor : %f, %f, %f\n", o.id, o.color[0], o.color[1], o.color[2]);
  printf("position : %f, %f, %f\n", o.pos[0], o.pos[1], o.pos[2]);
  printf("rotation : %f, %f, %f\n", o.rot[0], o.rot[1], o.rot[2]);
  printf("size : %f, %f, %f\n", o.size[0], o.size[1], o.size[2]);
  printf("radius : %f\nthickness : %f\n=======\n", o.radius, o.thickness);
}

Scene_t* init_scene() {
    int nb_objects = 5;
    Scene_t* scene = malloc(sizeof(Scene_t));
    Object_t* objs = malloc(sizeof(Object_t) * nb_objects);
    objs[0] = init_object(0, 0.06, 0.04, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0);
    objs[1] = init_object(0, 0.3, 0.04, 0.06, 0.1, 0.45, -0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.2, 0.0, 1);
    objs[2] = init_object(1, 1.0, 0.4, 0.0, 0.0, 0.2, 0.1, 0.0, 0.37, 0.93, 0.0, 0.0, 0.0, 0.8, 0.1, 2);
    objs[3] = init_object(2, 1.0, 0.2, 0.1, 0.6, 0.3, 0.4, 0.1, 0.4, 0.3, 0.12, 0.4, 0.0, 0.0, 0.3, 3);
    objs[4] = init_object(3, 0.96, 0.41, 0.6, -1.25, 0.5, 0.0, 0.0, 0.5, 0.2, 0.2, 0.2, 0.2, 0.0, 0.05, 4);
    //objs[5] = init_object(4, 0.25, 0.45, 0.5, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 5);
    scene->obj = objs;
    scene->nb = nb_objects;

    // Construire le KD-tree
    Object_t** obj_ptrs = malloc(sizeof(Object_t*) * nb_objects);
    for (int i = 0; i < nb_objects; i++) {
        obj_ptrs[i] = &objs[i];
    }
    
    // Initialiser l'info du KD-tree
    scene->kdtree_info = init_kdtree_info(100);
    scene->kdtree_info->root = build_kdtree(obj_ptrs, nb_objects, 0, scene->kdtree_info);
    
    // Afficher le KD-tree
    display_kdtree(scene->kdtree_info);

    // Convertir le KD-tree en format GPU
    KDNodeGPU_t* kdtree_data = convert_kdtree_to_gpu_format(scene->kdtree_info);
    
    // Envoyer les données au shader
    send_kdtree_to_shader(kdtree_data, scene->kdtree_info->node_count);
    free(kdtree_data);

    free(obj_ptrs);
    return scene;
}

void free_scene(Scene_t* scene) {
    if (scene) {
        if (scene->obj) {
            free(scene->obj);
        }
        
        if (scene->kdtree_info) {
            free_kdtree_info(scene->kdtree_info);
        }
        
        free(scene);
    }
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
    glUniform1f (uindex, 0.0);
    //glUniform1f (uindex, ((float) ticks) / 1000.0);

  uindex = glGetUniformLocation (prog, "iResolution_");
  if (uindex >= 0)
    {
      if (geometry[0] > 0.1 && geometry[1] > 0.1)
        glUniform4f (uindex, geometry[0], geometry[1], 1.0, (float)view_perf);
      else
        glUniform4f (uindex, width, height, 1.0, view_perf);
    }

  GLint useKDTreeLoc = glGetUniformLocation(prog, "iAlgo");
  if (useKDTreeLoc >= 0) {
    glUniform1i(useKDTreeLoc, algo);
    KD_DEBUG("iAlgo uniform transmis: %d\n", algo);
  } else {
    printf("ERREUR: L'uniform iAlgo n'a pas été trouvé dans le shader!\n");
  }

  GLint displayKDTreeLoc = glGetUniformLocation(prog, "iDisplayKDTree");
  if (displayKDTreeLoc >= 0) {
    glUniform1i(displayKDTreeLoc, display_kdtree_shader);
    KD_DEBUG("iDisplayKDTree uniform transmis: %d\n", display_kdtree_shader);
  } else {
    printf("ERREUR: L'uniform iDisplayKDTree n'a pas été trouvé dans le shader!\n");
  }

  // Si l'utilisateur a activé l'affichage 3D du KD-tree
  if (show_kdtree_3d && scene && scene->kdtree_info) {
    // Désactiver le shader
    glUseProgram(0);
    
    // Configurer la projection pour la 3D
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)width / (double)height, 0.1, 100.0);
    
    // Configurer la vue
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(5.0, 5.0, 5.0,  // Position de la caméra
              0.0, 0.0, 0.0,  // Point regardé
              0.0, 1.0, 0.0); // Vecteur "up"
    
    // Rotation de la scène pour pouvoir la voir sous différents angles
    static float rotation = 0.0f;
    rotation += 0.5f;  // Rotation lente
    glRotatef(rotation, 0.0f, 1.0f, 0.0f);
  
    
    // Revenir au shader pour le rendu normal
    glUseProgram(prog);
  }

  // Réafficher le nœud racine du KD-tree pour vérifier les données
  if (algo == 1 && kdtree_ssbo != 0) {
    // Afficher toutes les données du KD-tree pour le débogage
    int num_nodes = 3; // D'après vos logs précédents
    KDNodeGPU_t* nodes = malloc(sizeof(KDNodeGPU_t) * num_nodes);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, kdtree_ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(KDNodeGPU_t) * num_nodes, nodes);
    
    KD_DEBUG("=== KD-tree complet envoyé au shader ===\n");
    for (int i = 0; i < num_nodes; i++) {
        KD_DEBUG("Nœud %d: axe=%d, pos=%.2f, gauche=%d, droite=%d\n",
               i, nodes[i].split_axis, nodes[i].split_pos, 
               nodes[i].left_child_index, nodes[i].right_child_index);
    }
    KD_DEBUG("=====================================\n");
    
    free(nodes);
  }

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, kdtree_ssbo);

  //uindex = glGetUniformLocation (prog, "iMouse");
  //if (uindex >= 0)
    //glUniform4f (uindex, mouse[0],  mouse[1], mouse[2], mouse[3]);
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

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
  mods = mods;
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
  (void)scancode;
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
  } else if ((key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
  } else if ((key == GLFW_KEY_F || key == GLFW_KEY_ENTER) && action == GLFW_PRESS && (mods & GLFW_MOD_ALT)) {
      if (glfwGetWindowMonitor(window) == NULL)
          glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, 800, 600, GLFW_DONT_CARE);
      else
          glfwSetWindowMonitor(window, NULL, 0, 0, 800, 600, GLFW_DONT_CARE);
  } else if (key == GLFW_KEY_K && action == GLFW_PRESS) {
      // Activer/désactiver l'affichage 3D du KD-tree
      show_kdtree_3d = !show_kdtree_3d;
      printf("Affichage 3D du KD-tree: %s\n", show_kdtree_3d ? "activé" : "désactivé");
  }
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
  fprintf(stderr, "%d : Erreur GLFW: %s\n", error, description);
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

void load_scene() {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene_data_ssbo);

    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, scene->nb * sizeof(Object_t), scene->obj);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}


void glSendData() {
    if (scene_data_ssbo == 0) {  
        glGenBuffers(1, &scene_data_ssbo);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene_data_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, scene->nb * sizeof(Object_t), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, scene_data_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    load_scene();
}

void saveFramebuffer(const char* filename, int width, int height) {
  // Allouer de la mémoire pour les pixels
  unsigned char* pixels = (unsigned char*)malloc(3 * width * height);

  // Lire les pixels depuis le framebuffer
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

  // Allouer de la mémoire pour l'image retournée
  unsigned char* flippedPixels = (unsigned char*)malloc(3 * width * height);

  // Retourner l'image verticalement
  for (int y = 0; y < height; y++) {
    memcpy(
      flippedPixels + (height - 1 - y) * 3 * width,  // Ligne destination (inversée)
      pixels + y * 3 * width,                        // Ligne source
      3 * width                                     // Taille d'une ligne
    );
  }

  // Sauvegarder l'image retournée au format PNG
  if (stbi_write_png(filename, width, height, 3, flippedPixels, width * 3)) {
    printf("Image sauvegardée dans '%s'.\n", filename);
  } else {
    fprintf(stderr, "Erreur lors de la sauvegarde de l'image.\n");
  }

  // Libérer la mémoire
  free(pixels);
  free(flippedPixels);
}

int current_axis; // Variable globale pour stocker l'axe de tri

// Fonction de comparaison pour qsort
int compare_objects(const void* a, const void* b) {
    const Object_t* objA = *(const Object_t**)a;
    const Object_t* objB = *(const Object_t**)b;
    if (objA->pos[current_axis] < objB->pos[current_axis]) return -1;
    if (objA->pos[current_axis] > objB->pos[current_axis]) return 1;
    return 0;
}

#define MAX_DEPTH 20



void send_kdtree_to_shader(KDNodeGPU_t* kdtree_data, int num_nodes) {
    if (kdtree_ssbo == 0) {
        glGenBuffers(1, &kdtree_ssbo);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, kdtree_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(KDNodeGPU_t) * num_nodes, kdtree_data, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, kdtree_ssbo);
}

int main (int argc, char* argv[]) {
  if(argc == 5) {
    view_perf = atoi(argv[1]);
    output = atoi(argv[2]);
    algo = atoi(argv[3]);
    display_kdtree_shader = atoi(argv[4]);
  }
  else {
    printf("erreur :\nLa commande doit etre de la forme :\n./shadertoy.bin [n1] [n2] [n3]\n avec n1, n2 et n3 = 0 ou 1. n1 signifiant la vue des performances, n2 si on doit enregistrer une image, et n3 si on utilise les KD-trees.\n");
    return 0;
  }
  
  GLFWwindow* window = init_glfw_window();
  init_glew();
  scene = init_scene();
  
  int buffer_size = 100;
  char directory[buffer_size];
  getcwd(directory, buffer_size);

  // Construire le chemin complet du fichier
  char vertex_path[buffer_size + 50];
  snprintf(vertex_path, sizeof(vertex_path), "%s/src/vertex.glsl", directory);
  char fragment_path[buffer_size + 50];
  snprintf(fragment_path, sizeof(fragment_path), "%s/src/frag.glsl", directory);

  // Charger le fichier
  char* vert_code = load_file(vertex_path);
  char* frag_code = load_file(fragment_path);
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

  glSendData();
  int width, height;
  glfwGetWindowSize(window, &width, &height);
  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    display(window);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4); // Dessiner un carré avec 4 vertices
    glBindVertexArray(0);
    if (output > 0) {
      saveFramebuffer("output.png", width, height);
      break;
    }
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  glfwDestroyWindow(window);
  glfwTerminate();
  free_scene(scene);
  return 0;
}