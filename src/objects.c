#include "objects.h"
#include <math.h>

// Fonction pour calculer la boîte englobante d'un objet
BoundingBox_t compute_object_bbox(Object_t* obj) {
    BoundingBox_t bbox;
    float radius = obj->radius;
    
    // Initialiser les min/max avec la position de l'objet
    for (int i = 0; i < 3; i++) {
        bbox.min[i] = obj->pos[i] - radius;
        bbox.max[i] = obj->pos[i] + radius;
    }
    
    // Pour les objets plus complexes (boîtes, etc.), il faudrait adapter cette fonction
    // en fonction du type d'objet
    
    return bbox;
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

Object_t init_object(int type, float r, float g, float b, float px, float py, float pz, 
                   float rx, float ry, float rz, float sx, float sy, float sz, 
                   float radius, float thickness, int id) {
    Object_t obj;
    obj.type = type;
    obj.color[0] = r;
    obj.color[1] = g;
    obj.color[2] = b;
    obj.pos[0] = px;
    obj.pos[1] = py;
    obj.pos[2] = pz;
    obj.rot[0] = rx;
    obj.rot[1] = ry;
    obj.rot[2] = rz;
    obj.size[0] = sx;
    obj.size[1] = sy;
    obj.size[2] = sz;
    obj.radius = radius;
    obj.thickness = thickness;
    obj.id = id;
    return obj;
} 