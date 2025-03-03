#define _POSIX_C_SOURCE 199309L
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

struct Object_s {
  float color[3];
  int type; //0 : sphere | 1 : torus | 2 : cylinder | 3 : box | 4 : plan
  float pos[3];
  float radius;
  float rot[3]; //normale for plans
  float thickness; //thickness for torus and rounding for cylinder and box
  float size[3];
  int id;
};

typedef struct Object_s Object_t;

// Déclaration de la structure KDNode
struct KDNode {
    float split_pos;
    int split_axis;
    int left_child_index;
    int right_child_index;
    int start_index;
    int end_index;
};

typedef struct KDNode KDNode_t;

// Déclaration de la structure KDNodeGPU
struct KDNodeGPU {
    float split_pos;
    int split_axis;
    int left_child_index;
    int right_child_index;
    int start_index;
    int end_index;
};

typedef struct KDNodeGPU KDNodeGPU_t;

// Structure pour stocker les nœuds alloués
struct KDTreeInfo {
    KDNode_t* root;
    KDNode_t** all_nodes;
    int node_count;
    int max_nodes;
};

typedef struct KDTreeInfo KDTreeInfo_t;

struct Scene_s {
  Object_t* obj;
  int nb;
  KDTreeInfo_t* kdtree_info;
};

typedef struct Scene_s Scene_t;

static double geometry[4] = { 0, };
static double mouse[4] = { 0, };

static GLint prog = 0;
static GLuint scene_data_ssbo = 0;
static Scene_t* scene = NULL;
int view_perf;
int output;

// Déclarations de fonction
KDTreeInfo_t* init_kdtree_info(int max_nodes);
void add_node_to_kdtree_info(KDTreeInfo_t* info, KDNode_t* node);
void free_kdtree_info(KDTreeInfo_t* info);
KDNode_t* build_kdtree(Object_t** objects, int nb_objects, int depth, KDTreeInfo_t* info);
void send_kdtree_to_shader(KDNodeGPU_t* kdtree_data, int num_nodes);
void free_kdtree(KDNode_t* nodes, int root_index);

// Prototype de la fonction display_kdtree
void display_kdtree(Scene_t* scene);

Object_t init_object(int type,
                      float r, float g, float b,
                      float x, float y, float z,
                      float alpha, float beta, float gamma,
                      float size_x, float size_y, float size_z,
                      float radius,
                      float thickness,
                      int id) {

  Object_t o = {
    .color = {r, g, b},
    .pos = {x, y, z},
    .rot = {alpha, beta, gamma},
    .size = {size_x, size_y, size_z},
    .radius = radius,
    .thickness = thickness,
    .type = type,
    .id = id
  };
  
  return o;
}

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
    display_kdtree(scene);

    // Convertir le KD-tree en format GPU
    KDNodeGPU_t* kdtree_data = malloc(sizeof(KDNodeGPU_t) * scene->kdtree_info->node_count);
    // Remplir kdtree_data avec les données du KD-tree
    // (Assurez-vous de copier les données correctement)

    // Envoyer le KD-tree au GPU
    send_kdtree_to_shader(kdtree_data, scene->kdtree_info->node_count);

    free(obj_ptrs);
    free(kdtree_data);
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
  scancode = scancode;
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

// Structure pour représenter une boîte englobante alignée sur les axes (AABB)
typedef struct {
    float min[3]; // Coordonnées minimales (x, y, z)
    float max[3]; // Coordonnées maximales (x, y, z)
} BoundingBox_t;

// Fonction pour calculer la boîte englobante d'un objet en fonction de son type
BoundingBox_t calculate_bounding_box(Object_t* obj) {
    BoundingBox_t bbox;
    
    // Initialiser les valeurs min/max avec la position de l'objet
    for (int i = 0; i < 3; i++) {
        bbox.min[i] = obj->pos[i];
        bbox.max[i] = obj->pos[i];
    }
    
    // Ajuster en fonction du type d'objet
    switch (obj->type) {
        case 0: // Sphère
            // Étendre la boîte de "radius" dans toutes les directions
            for (int i = 0; i < 3; i++) {
                bbox.min[i] -= obj->radius;
                bbox.max[i] += obj->radius;
            }
            break;
            
        case 1: // Tore
            {
                // Pour un tore, l'extension maximale est le rayon principal + rayon interne
                float total_radius = obj->radius + obj->thickness;
                
                // Si le tore est aligné avec les axes par défaut (XZ)
                if (fabs(obj->rot[0]) < 0.001 && fabs(obj->rot[1]) < 0.001 && fabs(obj->rot[2]) < 0.001) {
                    // Extension dans le plan XZ
                    bbox.min[0] -= total_radius;
                    bbox.min[2] -= total_radius;
                    bbox.max[0] += total_radius;
                    bbox.max[2] += total_radius;
                    
                    // Extension en Y limitée au rayon interne
                    bbox.min[1] -= obj->thickness;
                    bbox.max[1] += obj->thickness;
                } else {
                    // Pour une rotation arbitraire, utiliser une approche conservatrice
                    for (int i = 0; i < 3; i++) {
                        bbox.min[i] -= total_radius;
                        bbox.max[i] += total_radius;
                    }
                }
            }
            break;
            
        case 2: // Cylindre
            {
                // Extraire la hauteur et le rayon du cylindre
                float height = obj->size[0];
                float radius = obj->size[1];
                
                // Si le cylindre est aligné avec l'axe Y par défaut
                if (fabs(obj->rot[0]) < 0.001 && fabs(obj->rot[2]) < 0.001) {
                    // Extension en Y basée sur la hauteur
                    bbox.min[1] -= height / 2.0f;
                    bbox.max[1] += height / 2.0f;
                    
                    // Extension en X et Z basée sur le rayon
                    bbox.min[0] -= radius;
                    bbox.min[2] -= radius;
                    bbox.max[0] += radius;
                    bbox.max[2] += radius;
                } else {
                    // Pour une rotation arbitraire, utiliser une approche conservatrice
                    float max_extent = fmax(height / 2.0f, radius);
                    for (int i = 0; i < 3; i++) {
                        bbox.min[i] -= max_extent;
                        bbox.max[i] += max_extent;
                    }
                }
            }
            break;
            
        case 3: // Pavé droit
            {
                // Extraire les dimensions du pavé
                float x_size = obj->size[0];
                float y_size = obj->size[1];
                float z_size = obj->size[2];
                
                // Si le pavé est aligné avec les axes (pas de rotation)
                if (fabs(obj->rot[0]) < 0.001 && fabs(obj->rot[1]) < 0.001 && fabs(obj->rot[2]) < 0.001) {
                    bbox.min[0] -= x_size / 2.0f;
                    bbox.min[1] -= y_size / 2.0f;
                    bbox.min[2] -= z_size / 2.0f;
                    bbox.max[0] += x_size / 2.0f;
                    bbox.max[1] += y_size / 2.0f;
                    bbox.max[2] += z_size / 2.0f;
                } else {
                    // Pour une rotation arbitraire, utiliser une approche conservatrice
                    float max_extent = sqrt(x_size*x_size + y_size*y_size + z_size*z_size) / 2.0f;
                    for (int i = 0; i < 3; i++) {
                        bbox.min[i] -= max_extent;
                        bbox.max[i] += max_extent;
                    }
                }
                
                // Ajouter le facteur de lissage/arrondi
                if (obj->thickness > 0) {
                    for (int i = 0; i < 3; i++) {
                        bbox.min[i] -= obj->thickness;
                        bbox.max[i] += obj->thickness;
                    }
                }
            }
            break;
            
        case 4: // Plan infini
            {
                // Pour un plan, utiliser une boîte très grande dans la direction du plan
                // et fine dans la direction de la normale
                
                // Normaliser le vecteur normal
                float norm = sqrt(obj->rot[0]*obj->rot[0] + obj->rot[1]*obj->rot[1] + obj->rot[2]*obj->rot[2]);
                float nx = obj->rot[0] / norm;
                float ny = obj->rot[1] / norm;
                float nz = obj->rot[2] / norm;
                
                // Définir une grande valeur pour l'extension du plan
                float large_value = 1000.0f; // Ajuster selon vos besoins
                
                // Étendre légèrement dans la direction de la normale
                float small_value = 0.01f;
                
                for (int i = 0; i < 3; i++) {
                    if (i == 0) {
                        bbox.min[i] = fabs(nx) > 0.9f ? obj->pos[i] - small_value : obj->pos[i] - large_value;
                        bbox.max[i] = fabs(nx) > 0.9f ? obj->pos[i] + small_value : obj->pos[i] + large_value;
                    } else if (i == 1) {
                        bbox.min[i] = fabs(ny) > 0.9f ? obj->pos[i] - small_value : obj->pos[i] - large_value;
                        bbox.max[i] = fabs(ny) > 0.9f ? obj->pos[i] + small_value : obj->pos[i] + large_value;
                    } else {
                        bbox.min[i] = fabs(nz) > 0.9f ? obj->pos[i] - small_value : obj->pos[i] - large_value;
                        bbox.max[i] = fabs(nz) > 0.9f ? obj->pos[i] + small_value : obj->pos[i] + large_value;
                    }
                }
            }
            break;
            
        default:
            // Pour les autres types non spécifiés, utiliser une marge par défaut
            for (int i = 0; i < 3; i++) {
                bbox.min[i] -= 1.0f;
                bbox.max[i] += 1.0f;
            }
            break;
    }
    
    return bbox;
}

// Fonction pour construire le KD-tree avec des objets volumétriques
KDNode_t* build_kdtree(Object_t** objects, int nb_objects, int depth, KDTreeInfo_t* info) {
    if (nb_objects == 0 || depth >= MAX_DEPTH) return NULL;

    KDNode_t* node = malloc(sizeof(KDNode_t));
    add_node_to_kdtree_info(info, node);
    
    current_axis = depth % 3; // Mettre à jour l'axe de tri
    node->split_axis = current_axis;

    // Si peu d'objets, ne pas subdiviser davantage
    if (nb_objects <= 4) { // Seuil arbitraire, ajustez selon vos besoins
        node->split_pos = 0.0f; // Valeur arbitraire
        node->start_index = 0;
        node->end_index = nb_objects;
        node->left_child_index = -1;
        node->right_child_index = -1;
        return node;
    }

    // Calculer les boîtes englobantes pour tous les objets
    BoundingBox_t* bboxes = malloc(sizeof(BoundingBox_t) * nb_objects);
    for (int i = 0; i < nb_objects; i++) {
        bboxes[i] = calculate_bounding_box(objects[i]);
    }

    // Trier les objets en fonction de la position centrale sur l'axe actuel
    current_axis = node->split_axis;
    qsort(objects, nb_objects, sizeof(Object_t*), compare_objects);
    
    // Calculer la médiane des positions centrales
    int median_index = nb_objects / 2;
    node->split_pos = objects[median_index]->pos[current_axis];
    
    // Créer des listes pour les objets à gauche, à droite et chevauchant le plan
    Object_t** left_objects = malloc(sizeof(Object_t*) * nb_objects);
    Object_t** right_objects = malloc(sizeof(Object_t*) * nb_objects);
    Object_t** overlap_objects = malloc(sizeof(Object_t*) * nb_objects);
    
    int left_count = 0;
    int right_count = 0;
    int overlap_count = 0;
    
    // Répartir les objets selon leur position par rapport au plan de séparation
    for (int i = 0; i < nb_objects; i++) {
        BoundingBox_t bbox = bboxes[i];
        if (bbox.max[current_axis] <= node->split_pos) {
            // Objet entièrement à gauche
            left_objects[left_count++] = objects[i];
        } else if (bbox.min[current_axis] >= node->split_pos) {
            // Objet entièrement à droite
            right_objects[right_count++] = objects[i];
        } else {
            // Objet chevauchant le plan
            overlap_objects[overlap_count++] = objects[i];
        }
    }
    
    // Si la distribution est très déséquilibrée, ajuster
    if (left_count == 0 || right_count == 0) {
        // Répartir les objets de chevauchement
        for (int i = 0; i < overlap_count; i++) {
            if (i < overlap_count / 2) {
                left_objects[left_count++] = overlap_objects[i];
            } else {
                right_objects[right_count++] = overlap_objects[i];
            }
        }
        overlap_count = 0;
    }
    
    // Libérer les boîtes englobantes
    free(bboxes);
    
    // Traiter les objets chevauchants en les ajoutant aux deux sous-arbres
    for (int i = 0; i < overlap_count; i++) {
        left_objects[left_count++] = overlap_objects[i];
        right_objects[right_count++] = overlap_objects[i];
    }
    
    // Construire récursivement les sous-arbres
    KDNode_t* left_child = NULL;
    KDNode_t* right_child = NULL;
    
    if (left_count > 0) {
        left_child = build_kdtree(left_objects, left_count, depth + 1, info);
    }
    
    if (right_count > 0) {
        right_child = build_kdtree(right_objects, right_count, depth + 1, info);
    }
    
    // Assigner les indices des enfants
    if (left_child) {
        node->left_child_index = info->node_count - 1; // Indice relatif
    } else {
        node->left_child_index = -1;
    }
    
    if (right_child) {
        node->right_child_index = info->node_count - 1; // Indice relatif
    } else {
        node->right_child_index = -1;
    }
    
    // Stocker les informations des objets
    node->start_index = 0;
    node->end_index = nb_objects;
    
    // Libérer les listes temporaires
    free(left_objects);
    free(right_objects);
    free(overlap_objects);
    
    return node;
}

void send_kdtree_to_shader(KDNodeGPU_t* kdtree_data, int num_nodes) {
    GLuint kdtree_ssbo;
    glGenBuffers(1, &kdtree_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, kdtree_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(KDNodeGPU_t) * num_nodes, kdtree_data, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, kdtree_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void cleanup_buffers() {
    if (scene_data_ssbo != 0) {
        glDeleteBuffers(1, &scene_data_ssbo);
    }
    // Ajoutez d'autres buffers à supprimer si nécessaire
}

void free_kdtree(KDNode_t* nodes, int root_index) {
    // Fonction simplifiée: ne fait rien pour éviter la boucle infinie
    (void)nodes; // Pour éviter l'avertissement de paramètre non utilisé
    (void)root_index; // Pour éviter l'avertissement de paramètre non utilisé
    return;
}

// Fonction pour obtenir le nom du type d'objet
const char* get_object_type_name(int type) {
    switch (type) {
        case 0: return "Sphère";
        case 1: return "Tore";
        case 2: return "Cylindre";
        case 3: return "Pavé droit";
        case 4: return "Plan infini";
        default: return "Type inconnu";
    }
}

// Fonction pour afficher les caractéristiques d'un objet selon son type
void print_object_details(Object_t* obj) {
    printf("      %s (ID: %d): ", get_object_type_name(obj->type), obj->id);
    
    switch (obj->type) {
        case 0: // Sphère
            printf("Pos(%.2f, %.2f, %.2f), Rayon=%.2f", 
                   obj->pos[0], obj->pos[1], obj->pos[2], obj->radius);
            break;
        case 1: // Tore
            printf("Pos(%.2f, %.2f, %.2f), Rayon=%.2f, Épaisseur=%.2f, Normal(%.2f, %.2f, %.2f)", 
                   obj->pos[0], obj->pos[1], obj->pos[2], 
                   obj->radius, obj->thickness, 
                   obj->rot[0], obj->rot[1], obj->rot[2]);
            break;
        case 2: // Cylindre
            printf("Pos(%.2f, %.2f, %.2f), Hauteur=%.2f, Rayon=%.2f, Orient(%.2f, %.2f, %.2f)", 
                   obj->pos[0], obj->pos[1], obj->pos[2], 
                   obj->size[0], obj->size[1], 
                   obj->rot[0], obj->rot[1], obj->rot[2]);
            break;
        case 3: // Pavé droit
            printf("Pos(%.2f, %.2f, %.2f), Dim(%.2f, %.2f, %.2f), Orient(%.2f, %.2f, %.2f)", 
                   obj->pos[0], obj->pos[1], obj->pos[2], 
                   obj->size[0], obj->size[1], obj->size[2], 
                   obj->rot[0], obj->rot[1], obj->rot[2]);
            break;
        case 4: // Plan infini
            printf("Pos(%.2f, %.2f, %.2f), Normal(%.2f, %.2f, %.2f)", 
                   obj->pos[0], obj->pos[1], obj->pos[2], 
                   obj->rot[0], obj->rot[1], obj->rot[2]);
            break;
        default:
            printf("Caractéristiques inconnues");
            break;
    }
    printf("\n");
}

// Fonction pour afficher le contenu du KD-tree avec indentation
void print_kdtree(KDTreeInfo_t* tree_info, int node_index, int depth, Scene_t* scene) {
    if (node_index < 0 || node_index >= tree_info->node_count) {
        return;
    }

    KDNode_t* node = tree_info->all_nodes[node_index];
    
    // Créer l'indentation en fonction de la profondeur
    char indent[100] = "";
    for (int i = 0; i < depth; i++) {
        strcat(indent, "  ");
    }
    
    // Déterminer la lettre de l'axe (x, y, z)
    char axis_letter = 'x';
    if (node->split_axis == 1) {
        axis_letter = 'y';
    } else if (node->split_axis == 2) {
        axis_letter = 'z';
    }
    
    // Afficher les informations du nœud avec l'axe en lettre
    printf("%sNœud[%d]: axe=%c, pos=%.2f, objets=%d-%d\n", 
           indent, node_index, axis_letter, node->split_pos, 
           node->start_index, node->end_index);
    
    // Afficher les objets contenus dans ce nœud
    printf("%s  Objets dans ce nœud:\n", indent);
    
    // Parcourir les objets associés à ce nœud
    // Note: Cette partie dépend de comment les objets sont associés aux nœuds dans votre KD-tree
    // Si le nœud contient des indices directs, utilisez ces indices
    // Sinon, parcourez tous les objets et vérifiez s'ils appartiennent à ce nœud
    
    for (int i = 0; i < scene->nb; i++) {
        // Vérifier si l'objet appartient à ce nœud en utilisant sa boîte englobante
        Object_t* obj = &scene->obj[i];
        BoundingBox_t bbox = calculate_bounding_box(obj);
        
        // Si l'objet chevauche le plan de séparation, il est dans ce nœud
        if (node->split_axis == 0) {
            if (bbox.min[0] <= node->split_pos && bbox.max[0] >= node->split_pos) {
                print_object_details(obj);
            }
        } else if (node->split_axis == 1) {
            if (bbox.min[1] <= node->split_pos && bbox.max[1] >= node->split_pos) {
                print_object_details(obj);
            }
        } else {
            if (bbox.min[2] <= node->split_pos && bbox.max[2] >= node->split_pos) {
                print_object_details(obj);
            }
        }
    }
    
    // Calculer les indices des enfants
    int left_index = -1, right_index = -1;
    
    if (node->left_child_index >= 0) {
        left_index = node_index + node->left_child_index;
        printf("%s  Gauche -> Nœud[%d]\n", indent, left_index);
    } else {
        printf("%s  Gauche -> NULL\n", indent);
    }
    
    if (node->right_child_index >= 0) {
        right_index = node_index + node->right_child_index;
        printf("%s  Droite -> Nœud[%d]\n", indent, right_index);
    } else {
        printf("%s  Droite -> NULL\n", indent);
    }
    
    // Afficher récursivement les enfants
    if (left_index >= 0) {
        print_kdtree(tree_info, left_index, depth + 1, scene);
    }
    if (right_index >= 0) {
        print_kdtree(tree_info, right_index, depth + 1, scene);
    }
}

// Fonction principale pour afficher le KD-tree (mise à jour pour passer scene)
void display_kdtree(Scene_t* scene) {
    if (!scene || !scene->kdtree_info || !scene->kdtree_info->root) {
        printf("KD-tree vide ou non initialisé\n");
        return;
    }
    
    printf("=== Structure du KD-tree ===\n");
    printf("Nombre total de nœuds: %d\n", scene->kdtree_info->node_count);
    
    // Trouver l'index du nœud racine
    int root_index = 0;  // Généralement, la racine est le premier nœud
    
    print_kdtree(scene->kdtree_info, root_index, 0, scene);
    printf("===========================\n");
}

// Fonction pour initialiser l'info du KD-tree
KDTreeInfo_t* init_kdtree_info(int max_nodes) {
    KDTreeInfo_t* info = malloc(sizeof(KDTreeInfo_t));
    info->root = NULL;
    info->all_nodes = malloc(sizeof(KDNode_t*) * max_nodes);
    info->node_count = 0;
    info->max_nodes = max_nodes;
    return info;
}

// Fonction pour ajouter un nœud à l'info du KD-tree
void add_node_to_kdtree_info(KDTreeInfo_t* info, KDNode_t* node) {
    if (info->node_count < info->max_nodes) {
        info->all_nodes[info->node_count++] = node;
    }
}

// Fonction pour libérer l'info du KD-tree
void free_kdtree_info(KDTreeInfo_t* info) {
    if (info) {
        for (int i = 0; i < info->node_count; i++) {
            free(info->all_nodes[i]);
        }
        free(info->all_nodes);
        free(info);
    }
}

int main (int argc, char* argv[]) {
  if(argc == 3) {
    view_perf = atoi(argv[1]);
    output = atoi(argv[2]);
  }
  else {
    printf("erreur :\nLa commande doit etre de la forme :\n./shadertoy.bin [n1] [n2]\n avec n1 et n2 = 0 ou 1. n1 signifiant la vue des performances et n2 si on doit enregistrer une image.\n");
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
  cleanup_buffers();
  return 0;
}