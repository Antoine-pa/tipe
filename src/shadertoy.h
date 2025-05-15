#ifndef SHADERTOY_H
#define SHADERTOY_H

#include <stdio.h>
#include <GLFW/glfw3.h>
#include "objects.h"

// Déclarations des variables globales
extern double geometry[4];
extern uint64_t render_start;
extern float mouse[4];
extern GLuint prog;
extern int view_perf;
extern int use_kdtree;
extern Scene_t* scene;
extern GLuint scene_data_ssbo;

// Prototypes des fonctions principales
void init_scene(FILE* obj_file, int nb_objects, const char* base_filename);
void display(GLFWwindow* window);
void keyboard_handler(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_pos_callback(GLFWwindow* window, double x, double y);
void scroll_callback(GLFWwindow* window, double x, double y);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void save_scene(const char* filename);
void load_scene(const char* filename);
GLuint setup_scene_data_buffer(Scene_t* scene);
void update_scene_data_buffer(Scene_t* scene);
void cleanup_scene();

// Point d'entrée du programme
int main(int argc, char *argv[]);

#endif // SHADERTOY_H 