#ifndef KDTREE_H
#define KDTREE_H

#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "objects.h"  // Inclure le nouveau fichier objects.h


// Structure pour les nœuds du KD-tree
typedef struct {
    float split_pos;
    int split_axis;
    int left_child_index;
    int right_child_index;
    int start_index;
    int end_index;
} KDNode_t;

// Structure pour les nœuds du KD-tree à envoyer au GPU
typedef struct {
    float split_pos;
    int split_axis;
    int left_child_index;
    int right_child_index;
    int start_index;
    int end_index;
} KDNodeGPU_t;

// Structure pour stocker les nœuds alloués
typedef struct KDTreeInfo_s {
    KDNode_t* root;
    KDNode_t** all_nodes;
    int node_count;
    int max_nodes;
} KDTreeInfo_t;

// Fonctions d'initialisation et de gestion du KD-tree
KDTreeInfo_t* init_kdtree_info(int max_nodes);
void add_node_to_kdtree_info(KDTreeInfo_t* info, KDNode_t* node);
void free_kdtree_info(KDTreeInfo_t* info);
KDNode_t* build_kdtree(Object_t** objects, int nb_objects, int depth, KDTreeInfo_t* info);
extern void send_kdtree_to_shader(KDNodeGPU_t* kdtree_data, int num_nodes);
void free_kdtree(KDNode_t* nodes, int root_index);
void display_kdtree(KDTreeInfo_t* kdtree_info);
void print_kdtree(KDTreeInfo_t* tree_info, int node_index, int depth, Scene_t* scene);

// Fonction récursive pour dessiner un nœud du KD-tree et ses enfants
void render_kdtree_node(KDTreeInfo_t* tree_info, int node_index, float min[3], float max[3]);

// Fonction de conversion pour l'envoi au GPU
KDNodeGPU_t* convert_kdtree_to_gpu_format(KDTreeInfo_t* kdtree_info);

// Nouvelles fonctions déplacées de shadertoy.c
BoundingBox_t calculate_bounding_box(Object_t* obj);
void print_object_details(Object_t* obj);

#endif // KDTREE_H 