#ifndef OBJECTS_H
#define OBJECTS_H

#include <GL/glew.h>

// Structure pour stocker les informations d'un objet
// Cette structure représente tous les types d'objets 3D de la scène
typedef struct {
    float color[3];   // Couleur RGB de l'objet (valeurs entre 0 et 1)
    int type;         // Type d'objet: 0=Sphère, 1=Tore, 2=Cylindre, 3=Pavé droit, 4=Plan infini
    float pos[3];     // Position (x, y, z) du centre de l'objet
    float radius;     // Rayon pour les sphères, tores, cylindres
    float rot[3];     // Rotation en radians (x, y, z) pour orienter l'objet
    float thickness;  // Épaisseur du tore ou diamètre du tube
    float size[3];    // Dimensions (largeur, hauteur, profondeur) pour pavés droits
    int id;           // Identifiant unique de l'objet
} Object_t;

// Structure de boîte englobante pour les objets
typedef struct {
    float min[3];  // Coordonnées minimales (x, y, z)
    float max[3];  // Coordonnées maximales (x, y, z)
} BoundingBox_t;


typedef struct KDTreeInfo_s KDTreeInfo_t;

// Structure pour stocker la scène
typedef struct {
    Object_t* obj;
    int nb;
    KDTreeInfo_t* kdtree_info;
} Scene_t;

// Ajouter le prototype de init_object
Object_t init_object(int type, float r, float g, float b, float px, float py, float pz, 
                    float rx, float ry, float rz, float sx, float sy, float sz, 
                    float radius, float thickness, int id);


BoundingBox_t compute_object_bbox(Object_t* obj);

const char* get_object_type_name(int type);

#endif // OBJECTS_H 