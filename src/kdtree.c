#define _USE_MATH_DEFINES
#include "kdtree.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Définir une macro de debug proprement
#ifdef DEBUG_KD_TREE
#define KD_DEBUG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define KD_DEBUG(fmt, ...) /* rien */
#endif

// Fonction pour initialiser la structure d'information du KD-tree
KDTreeInfo_t* init_kdtree_info(int max_nodes) {
    KDTreeInfo_t* info = malloc(sizeof(KDTreeInfo_t));
    if (!info) {
        fprintf(stderr, "Failed to allocate KDTreeInfo\n");
        return NULL;
    }
    
    info->all_nodes = malloc(sizeof(KDNode_t*) * max_nodes);
    if (!info->all_nodes) {
        fprintf(stderr, "Failed to allocate KDTreeInfo nodes array\n");
        free(info);
        return NULL;
    }
    
    info->node_count = 0;
    info->max_nodes = max_nodes;
    info->root = NULL;
    
    return info;
}

// Fonction pour ajouter un nœud à la structure d'information
void add_node_to_kdtree_info(KDTreeInfo_t* info, KDNode_t* node) {
    if (info->node_count < info->max_nodes) {
        info->all_nodes[info->node_count] = node;
        info->node_count++;
    } else {
        fprintf(stderr, "KDTreeInfo nodes array is full\n");
    }
}

// Fonction pour construire le KD-tree récursivement
KDNode_t* build_kdtree(Object_t** objects, int nb_objects, int depth, KDTreeInfo_t* info) {
    if (nb_objects <= 0) {
        return NULL;
    }
    
    // Créer un nouveau nœud
    KDNode_t* node = malloc(sizeof(KDNode_t));
    if (!node) {
        fprintf(stderr, "Failed to allocate KDNode\n");
        return NULL;
    }
    
    // Ajouter le nœud à la liste des nœuds
    add_node_to_kdtree_info(info, node);
    
    // Si c'est la racine, mettre à jour le pointeur de la racine
    if (info->node_count == 1) {
        info->root = node;
    }
    
    // Si peu d'objets ou profondeur maximale atteinte, créer une feuille
    if (nb_objects <= 4 || depth >= 10) {
        node->split_axis = -1; // Marquer comme feuille
        node->left_child_index = -1;
        node->right_child_index = -1;
        node->start_index = 0;
        node->end_index = nb_objects;
        
        KD_DEBUG("Feuille créée avec %d objets: [", nb_objects);
        #ifdef DEBUG_KD_TREE
        for (int i = 0; i < nb_objects; i++) {
            printf("%d", objects[i]->id);
            if (i < nb_objects - 1) printf(", ");
        }
        #endif
        KD_DEBUG("]\n");
        
        return node;
    }
    
    // Calculer les boîtes englobantes pour tous les objets
    BoundingBox_t* bboxes = malloc(sizeof(BoundingBox_t) * nb_objects);
    for (int i = 0; i < nb_objects; i++) {
        bboxes[i] = compute_object_bbox(objects[i]);
    }
    
    // Déterminer l'axe de séparation (alterné en fonction de la profondeur)
    int current_axis = depth % 3;
    node->split_axis = current_axis;
    
    // Calculer la position médiane sur cet axe
    float min_val = INFINITY;
    float max_val = -INFINITY;
    
    for (int i = 0; i < nb_objects; i++) {
        float center = objects[i]->pos[current_axis];
        if (center < min_val) min_val = center;
        if (center > max_val) max_val = center;
    }
    
    node->split_pos = (min_val + max_val) / 2.0f;
    
    // Séparer les objets en deux groupes
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

// Fonction pour convertir le KD-tree en format GPU
KDNodeGPU_t* convert_kdtree_to_gpu_format(KDTreeInfo_t* kdtree_info) {
    KDNodeGPU_t* kdtree_data = malloc(sizeof(KDNodeGPU_t) * kdtree_info->node_count);
    if (!kdtree_data) {
        fprintf(stderr, "Failed to allocate GPU KD-tree data\n");
        return NULL;
    }
    
    for (int i = 0; i < kdtree_info->node_count; i++) {
        kdtree_data[i].split_axis = kdtree_info->all_nodes[i]->split_axis;
        kdtree_data[i].split_pos = kdtree_info->all_nodes[i]->split_pos;
        kdtree_data[i].left_child_index = kdtree_info->all_nodes[i]->left_child_index;
        kdtree_data[i].right_child_index = kdtree_info->all_nodes[i]->right_child_index;
        kdtree_data[i].start_index = kdtree_info->all_nodes[i]->start_index;
        kdtree_data[i].end_index = kdtree_info->all_nodes[i]->end_index;
    }
    
    return kdtree_data;
}

// Fonction pour afficher le KD-tree (pour débogage)
void display_kdtree(KDTreeInfo_t* kdtree_info) {
    KD_DEBUG("KD-tree contient %d nœuds\n", kdtree_info->node_count);
    for (int i = 0; i < kdtree_info->node_count; i++) {
        KDNode_t* node = kdtree_info->all_nodes[i];
        #ifndef DEBUG_KD_TREE
        (void)node; // Évite le warning de variable non utilisée
        #endif
        KD_DEBUG("Nœud %d: axe=%d, pos=%.2f, gauche=%d, droite=%d, début=%d, fin=%d\n",
               i, node->split_axis, node->split_pos, 
               node->left_child_index, node->right_child_index,
               node->start_index, node->end_index);
    }
}

// Fonction pour libérer la mémoire du KD-tree
void free_kdtree_info(KDTreeInfo_t* info) {
    if (info) {
        for (int i = 0; i < info->node_count; i++) {
            free(info->all_nodes[i]);
        }
        free(info->all_nodes);
        free(info);
    }
}

// Fonction pour libérer un nœud du KD-tree (à ne pas utiliser directement)
void free_kdtree(KDNode_t* nodes, int root_index) {
    // Cette fonction n'est pas utilisée car free_kdtree_info libère tous les nœuds
    (void)nodes;
    (void)root_index;
}


// Assurez-vous que print_kdtree est implémentée
void print_kdtree(KDTreeInfo_t* tree_info, int node_index, int depth, Scene_t* scene) {
    if (node_index < 0 || node_index >= tree_info->node_count) {
        return;
    }
    
    KDNode_t* node = tree_info->all_nodes[node_index];
    
    // Indentation pour montrer la hiérarchie
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
    
    if (node->split_axis >= 0) {
        // Nœud interne
        printf("Nœud [%d]: Axe %d, Position %.2f\n", 
               node_index, node->split_axis, node->split_pos);
        
        // Afficher les enfants
        print_kdtree(tree_info, node_index + node->left_child_index, depth + 1, scene);
        print_kdtree(tree_info, node_index + node->right_child_index, depth + 1, scene);
    } else {
        // Feuille
        printf("Feuille [%d]: %d objets\n", node_index, node->end_index - node->start_index);
        
        // Si la scène est disponible, afficher les informations sur les objets
        if (scene) {
            int start = node->start_index;
            int end = node->end_index;
            
            for (int i = start; i < end; i++) {
                if (i < scene->nb) {
                    Object_t* obj = &scene->obj[i];
                    for (int j = 0; j < depth + 1; j++) {
                        printf("  ");
                    }
                    printf("Objet %d: Type %s, Position (%.2f, %.2f, %.2f)\n", 
                           obj->id, 
                           get_object_type_name(obj->type),
                           obj->pos[0], obj->pos[1], obj->pos[2]);
                }
            }
        }
    }
} 