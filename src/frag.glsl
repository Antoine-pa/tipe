#version 450 core
#extension GL_ARB_shading_language_420pack : require

struct Object {
    vec3 color;
    int type; //0 : sphere | 1 : torus | 2 : cylender | 3 : box | 4 : plan
    vec3 pos;
    float radius;
    vec3 rot; //normale for plans
    float thickness; //thickness for torus and rounding for cylender and box
    vec3 size;
    int id;
};

struct KDNodeGPU {
    float split_pos;
    int split_axis;
    int left_child_index;
    int right_child_index;
    int start_index;
    int end_index;
};

const int nbObjects = 5;

uniform vec4                iResolution_;           // viewport resolution (in pixels)
uniform float               iGlobalTime;           // shader playback time (in seconds)
uniform vec4                iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
int iViewPerf = int(iResolution_.a);
vec3 iResolution = iResolution_.rgb;

layout(std430, binding = 0) buffer Objects {
    Object iObjects[];
};

layout(std430, binding = 1) buffer KDTree {
    KDNodeGPU kdtree[];
};

out vec4 FragColor;

struct Data
{
  float d;
  vec3 color;
  int steps;
};

// Structure pour stocker les intersections
struct Intersections {
    int count;        // Nombre d'intersections (0, 1 ou 2)
    float t[2];       // Distances d'intersection le long du rayon
};

//raymarcher
const float MAX_DIST = 20.0;
const int MAX_STEPS = 100;
const float EPSILON = 0.001;

//background
const vec3 BACKGROUND = vec3(0.15, 0.35, 1.0);
const float BACKGROUND_LIGHT_INTENSITY = 0.1;

//light
const float RADIUS_OF_LIGHT = 3.0;

//mouse
const float RADIUS_MOUSE = 2.0;

//other data
const float PI = 3.141592;
const float ZOOM = 1.0;

// Ajouter cet uniform avec les autres au début du fichier
uniform int iAlgo;           // 0 : algo classique | 1 : algo avec KD-tree | 2 : algo avec raytracing

// Ajoutez ces constantes au début du fichier
const vec3 KD_PLANE_COLOR_X = vec3(1.0, 0.3, 0.3); // Rouge pour l'axe X
const vec3 KD_PLANE_COLOR_Y = vec3(0.3, 1.0, 0.3); // Vert pour l'axe Y
const vec3 KD_PLANE_COLOR_Z = vec3(0.3, 0.3, 1.0); // Bleu pour l'axe Z
const float KD_PLANE_OPACITY = 0.35;              // Transparence des plans

// Ajouter cette déclaration avec les autres uniformes au début du fichier
uniform int iDisplayKDTree;

//fonction de rotation
vec3 rotateX(vec3 p, float a){
  float sa = sin(a), ca = cos(a);
  return vec3(p.x, ca * p.y - sa * p.z, sa * p.y + ca * p.z);
}
vec3 rotateY(vec3 p, float a){
  float sa = sin(a),   ca = cos(a);
  return vec3(ca * p.x + sa * p.z, p.y, -sa * p.x + ca * p.z);
}
vec3 rotateZ(vec3 p, float a){
  float sa = sin(a),   ca = cos(a);
  return vec3(ca * p.x - sa * p.y, sa * p.x + ca * p.y, p.z);
}

vec3 rotate(vec3 p, vec3 a){
    p = rotateX(p, a.x);
    p = rotateY(p, a.y);
    p = rotateZ(p, a.z);
    return p;
}

//signed function field
vec4 Plane(vec3 p, vec3 n, vec3 c){
    return vec4(c, dot(p, n));
}

// Ajouter la fonction Sphere manquante
vec4 Sphere(vec3 p, float r, vec3 c){
    return vec4(c, length(p) - r);
}

vec4 Torus(vec3 p, vec3 rot, float r, float t, vec3 c){
    p = rotate(p, rot);
    return vec4(c, length(vec2(length(p.xz)-r, p.y)) - t);
}

vec4 Cylender(vec3 p, vec3 rot, vec2 size, float smoo, vec3 c){
    p = rotate(p, rot);

    float dX = length(p.xz) - size.x;
    float dY = abs(p.y) - size.y;
    
    float dE = length(vec2(max(dX, 0.0), max(dY, 0.0)));
    float dI = min(max(dX, dY), 0.0);
    
    float d = dE + dI;

    return vec4(c, d - smoo * 0.1);
}

vec4 Box(vec3 p, vec3 rot, vec3 size, float smoo, vec3 c){
    p = rotate(p, rot);
    
    vec3 diff = abs(p) - size;
    
    float dE = length(max(diff, 0.0));
    float dI = min(max(diff.x, max(diff.y, diff.z)), 0.0);
    
    float d = dE + dI;

    return vec4(c, d - smoo * 0.1);
}

vec4 substract(vec4 obj1, vec4 obj2){
    return max(obj1, -obj2);
}

vec4 intersection(vec4 obj1, vec4 obj2){
    return max(obj1, obj2);
}

vec4 minDist(vec4 a, vec4 b){
    return a.a < b.a ? a : b;
}

vec4 colDist(vec3 p, Object obj) {
    if(obj.type == 0) {
        return Sphere(p - obj.pos.xyz, obj.radius, obj.color.xyz);
    }
    if(obj.type == 1) {
        return Torus(p - obj.pos.xyz, obj.rot.xyz, obj.radius, obj.thickness, obj.color.xyz);
    }
    if(obj.type == 2) {
        return Cylender(p - obj.pos.xyz, obj.rot.xyz, obj.size.xy, obj.thickness, obj.color.xyz);
    }
    if(obj.type == 3) {
        return Box(p - obj.pos.xyz, obj.rot.xyz, obj.size.xyz, obj.thickness, obj.color.xyz);
    }
    else {
        return Plane(p - obj.pos.xyz, obj.rot.xyz, obj.color.xyz);
    }
}

vec3 color_steps(int nb_steps, int max_steps) {
    return vec3(float(nb_steps)/(float(max_steps)));
}

// Fonction pour calculer une sphère englobante d'un objet
vec4 compute_bounding_sphere(Object obj) {
    vec4 result;
    
    // Le centre est toujours la position de l'objet
    result.rgb = obj.pos;
    
    // Calcul du rayon selon le type d'objet
    switch (obj.type) {
        case 0: // Sphère
            // Le rayon de la sphère englobante est simplement le rayon de la sphère
            result.a = obj.radius;
            break;
            
        case 1: // Tore
            // Pour un tore, le rayon est la somme du rayon principal et de l'épaisseur
            result.a = obj.radius + obj.thickness;
            break;
            
        case 2: // Cylindre
            {
                // Pour un cylindre, le rayon de la sphère englobante est la racine carrée de
                // (rayon du cylindre)² + (hauteur/2)²
                float height = obj.size.x;
                float radius = obj.size.y;
                result.a = sqrt((height * 0.5) * (height * 0.5) + radius * radius);
                
                // Ajouter l'arrondi si présent
                if (obj.thickness > 0.0) {
                    result.a += obj.thickness;
                }
            }
            break;
            
        case 3: // Box
            {
                // Pour une boîte, le rayon de la sphère englobante est la longueur de la diagonale
                // Si obj.size est la demi-taille de la boîte, alors la diagonale complète est 
                // simplement la longueur du vecteur obj.size
                result.a = length(obj.size);
                
                // Ajouter l'arrondi si présent
                if (obj.thickness > 0.0) {
                    result.a += obj.thickness * 0.1; // L'arrondi est appliqué comme obj.thickness * 0.1 dans Box()
                }
            }
            break;
            
        case 4: // Plan
        default:
            // Pour un plan infini, on ne peut pas vraiment définir une sphère englobante finie
            // On va donc utiliser une très grande valeur
            result.a = 1000.0;
            break;
    }
    
    return result;
}

// Fonction pour calculer les intersections entre un rayon et une sphère
Intersections raytracing_sphere(vec3 sphereCenter, float sphereRadius, vec3 rayOrigin, vec3 rayDirection) {
    Intersections result;
    result.count = 0;
    
    // Normaliser la direction du rayon pour s'assurer qu'elle est unitaire
    vec3 rayDirNormalized = normalize(rayDirection);
    
    // Vecteur reliant l'origine du rayon au centre de la sphère
    vec3 oc = rayOrigin - sphereCenter;
    
    // Coefficients de l'équation quadratique at² + bt + c = 0
    float a = dot(rayDirNormalized, rayDirNormalized);  // Toujours 1 si la direction est normalisée
    float b = 2.0 * dot(oc, rayDirNormalized);
    float c = dot(oc, oc) - sphereRadius * sphereRadius;
    
    // Calculer le discriminant
    float discriminant = b * b - 4.0 * a * c;
    
    if (discriminant >= 0.0) { // Intersection réelle ou tangente
        float sqrtDiscriminant = sqrt(discriminant);
        
        float t0 = (-b - sqrtDiscriminant) / (2.0 * a);
        float t1 = (-b + sqrtDiscriminant) / (2.0 * a);

        // Stocker les intersections valides (t > 0) et distinctes
        if (t0 > 0.0001) { // Utiliser un petit epsilon pour s'assurer que c'est devant
            result.t[result.count++] = t0;
        }
        
        if (t1 > 0.0001) {
            bool add_t1 = true;
            if (result.count > 0) { // Vérifier si t1 est différent de t0 déjà ajouté
                if (abs(t1 - result.t[0]) < EPSILON) {
                    add_t1 = false;
                }
            }
            if (add_t1 && result.count < 2) {
                 result.t[result.count++] = t1;
            }
        }

        // S'assurer que t[0] est la plus petite intersection si deux sont trouvées
        if (result.count == 2 && result.t[0] > result.t[1]) {
            float temp = result.t[0];
            result.t[0] = result.t[1];
            result.t[1] = temp;
        }
    }
    
    return result;
}

// Fonction pour déterminer si un rayon intersecte une boîte englobante (version plus robuste)
bool rayIntersectsAABB(vec3 rayOrigin, vec3 rayDir, vec3 boxMin, vec3 boxMax, float maxDist) {
    // Ajouter une petite marge pour la stabilité numérique
    boxMin -= vec3(0.001);
    boxMax += vec3(0.001);
    
    // Éviter les divisions par zéro en ajoutant un petit epsilon
    vec3 invDir = 1.0 / (rayDir + vec3(0.0000001));
    
    // Calculer les distances t aux intersections avec les plans de la boîte
    vec3 t1 = (boxMin - rayOrigin) * invDir;
    vec3 t2 = (boxMax - rayOrigin) * invDir;
    
    // Trouver les distances min et max sur chaque axe
    vec3 tmin = min(t1, t2);
    vec3 tmax = max(t1, t2);
    
    // Trouver les distances d'entrée et de sortie
    float tNear = max(max(tmin.x, tmin.y), tmin.z);
    float tFar = min(min(tmax.x, tmax.y), tmax.z);
    
    // Être plus permissif pour les objets proches
    bool isClose = distance(rayOrigin, (boxMin + boxMax) * 0.5) < 2.0;
    
    // Vérifier si l'intersection est valide
    return (tFar > 0.0 && tNear < tFar && tNear < maxDist) || isClose;
}

// Fonction optimisée de traversée du KD-tree avec réduction des appels à colDist
Data traverseKDTree(vec3 rayOrigin, vec3 rayDirection) {
    Data data = Data(MAX_DIST, BACKGROUND, 0);
    int colDistCalls = 0;
    
    // Pile pour stocker les nœuds à visiter et leurs distances
    int stack[32];  // Nœuds
    float dist[32]; // Distances au nœud
    int stackPtr = 0;
    
    // Commencer par la racine
    stack[stackPtr] = 0;
    dist[stackPtr] = 0.0;
    stackPtr++;
    
    // Array pour marquer quels objets ont été testés
    bool tested[nbObjects]; 
    for (int i = 0; i < nbObjects; i++) {
        tested[i] = false;
    }
    
    // Variables pour suivre l'état de la recherche
    bool objectFound = false;
    float minFoundDist = MAX_DIST;
    
    // Test rapide global avant de commencer la traversée
    vec3 globalMin = vec3(-20.0, -20.0, -20.0); // Plus conservative
    vec3 globalMax = vec3(20.0, 20.0, 20.0);    // Plus conservative
    
    // Supprimer ce test pour toujours parcourir le KD-tree
    // if (!rayIntersectsAABB(rayOrigin, rayDirection, globalMin, globalMax, MAX_DIST)) {
    //     data.steps = 0;
    //     return data;
    // }
    
    while (stackPtr > 0) {
        // Récupérer le nœud le plus proche (selon la distance)
        int nearestIdx = 0;
        float nearestDist = dist[0];
        
        for (int i = 1; i < stackPtr; i++) {
            if (dist[i] < nearestDist) {
                nearestDist = dist[i];
                nearestIdx = i;
            }
        }
        
        // Si la distance minimale dépasse la distance de l'objet le plus proche déjà trouvé,
        // on peut arrêter la recherche seulement si on a déjà trouvé un objet
        if (objectFound && nearestDist >= minFoundDist) {
            break;
        }
        
        // Récupérer le nœud et le retirer de la pile
        int nodeIndex = stack[nearestIdx];
        float nodeDist = dist[nearestIdx];
        
        // Déplacer le dernier élément à la place de celui qu'on retire
        stack[nearestIdx] = stack[stackPtr-1];
        dist[nearestIdx] = dist[stackPtr-1];
        stackPtr--;
        
        KDNodeGPU node = kdtree[nodeIndex];
        
        // Si c'est une feuille, tester tous les objets de ce nœud
        if (node.split_axis == -1) {
            for (int i = node.start_index; i < node.end_index && i < nbObjects; i++) {
                if (i < 0 || i >= nbObjects) continue; // Protection supplémentaire
                
                // Marquer l'objet comme "potentiellement traité par le KD-tree"
                // Cela sert pour la passe de sécurité finale.
                if (iObjects[i].id < nbObjects) {
                    tested[iObjects[i].id] = true;
                }
                
                // Estimer une boîte englobante très conservative pour l'objet
                vec3 objMin, objMax;
                
                switch (iObjects[i].type) {
                    case 0: // Sphère
                        // Élargir un peu la boîte pour les sphères
                        objMin = iObjects[i].pos - vec3(iObjects[i].radius * 1.1);
                        objMax = iObjects[i].pos + vec3(iObjects[i].radius * 1.1);
                        break;
                    case 1: // Tore
                        {
                            // Rendre la boîte plus conservative pour les tores
                            float totalRadius = iObjects[i].radius + iObjects[i].thickness;
                            totalRadius *= 1.5; // Marge de sécurité plus importante
                            objMin = iObjects[i].pos - vec3(totalRadius);
                            objMax = iObjects[i].pos + vec3(totalRadius);
                        }
                        break;
                    case 2: // Cylindre
                    case 3: // Box
                    default:
                        // Pour les formes complexes, utiliser une estimation beaucoup plus large
                        float maxExtent = max(max(iObjects[i].size.x, iObjects[i].size.y), iObjects[i].size.z);
                        if (maxExtent < iObjects[i].radius) maxExtent = iObjects[i].radius;
                        maxExtent *= 2.0; // Doubler la taille pour être sûr
                        objMin = iObjects[i].pos - vec3(maxExtent);
                        objMax = iObjects[i].pos + vec3(maxExtent);
                        break;
                }
                
                // Les objets proches de la caméra sont toujours testés
                float distToObject = distance(rayOrigin, iObjects[i].pos);
                bool isClose = distToObject < 3.0;
                
                // Vérifier si le rayon intersecte la boîte englobante de l'objet
                if (isClose || rayIntersectsAABB(rayOrigin, rayDirection, objMin, objMax, data.d)) {
                    // Maintenant faire le test de collision précis
            vec4 o = colDist(rayOrigin, iObjects[i]);
                    colDistCalls++;
                    
                    if (o.a < data.d) {
                        data.d = o.a;
                        data.color = o.rgb;
                        objectFound = true;
                        minFoundDist = o.a;
                    }
                }
            }
            continue;
        }
        
        // Calculer l'intersection avec le plan de séparation
        float dirComponent = rayDirection[node.split_axis];
        float origComponent = rayOrigin[node.split_axis];
        
        // Gérer le cas où le rayon est presque parallèle au plan (augmenter le seuil)
        if (abs(dirComponent) < 0.001) {
            // Rayon presque parallèle au plan, visiter les deux enfants selon la position
            if (origComponent <= node.split_pos) {
                // L'origine est du côté gauche
                if (node.left_child_index != -1) {
                    stack[stackPtr] = node.left_child_index;
                    dist[stackPtr] = nodeDist;
                    stackPtr++;
                }
                if (node.right_child_index != -1) {
                    stack[stackPtr] = node.right_child_index;
                    dist[stackPtr] = nodeDist;
                    stackPtr++;
                }
            } else {
                // L'origine est du côté droit
                if (node.right_child_index != -1) {
                    stack[stackPtr] = node.right_child_index;
                    dist[stackPtr] = nodeDist;
                    stackPtr++;
                }
                if (node.left_child_index != -1) {
                    stack[stackPtr] = node.left_child_index;
                    dist[stackPtr] = nodeDist;
                    stackPtr++;
                }
            }
            continue;
        }
        
        // Calculer le point d'intersection avec le plan de séparation
        float t = (node.split_pos - origComponent) / dirComponent;
        
        // Déterminer de quel côté du plan se trouve l'origine du rayon
        bool originIsLeft = origComponent < node.split_pos;
        
        // Déterminer où va le rayon
        bool rayGoingRight = dirComponent > 0.0;
        
        // Nœud près (contenant l'origine)
        int nearNode = originIsLeft ? node.left_child_index : node.right_child_index;
        
        // Nœud loin (de l'autre côté du plan)
        int farNode = originIsLeft ? node.right_child_index : node.left_child_index;
        
        // Distance au nœud loin (distance au plan)
        float farDist = max(0.0, t);
        
        // Toujours visiter les deux nœuds si on est proche du plan de séparation
        bool closeToPlane = abs(origComponent - node.split_pos) < 0.2;
        
        // Visitez toujours les deux nœuds s'ils existent
        if (closeToPlane || t <= 0.0 || t >= data.d) {
            // Visiter d'abord le nœud près
            if (nearNode != -1) {
                stack[stackPtr] = nearNode;
                dist[stackPtr] = nodeDist;
                stackPtr++;
            }
            
            // Puis le nœud loin si on est proche du plan
            if (closeToPlane && farNode != -1) {
                stack[stackPtr] = farNode;
                dist[stackPtr] = farDist;
                stackPtr++;
            }
        } else {
            // Sinon, visiter les deux nœuds, d'abord le plus proche
            if (nearNode != -1) {
                stack[stackPtr] = nearNode;
                dist[stackPtr] = nodeDist;
                stackPtr++;
            }
            
            if (farNode != -1) {
                stack[stackPtr] = farNode;
                dist[stackPtr] = farDist;
                stackPtr++;
            }
        }
    }
    
    // Deuxième passe: vérifier les objets qui n'ont pas été testés
    // Et parfois même tester à nouveau certains objets pour être sûr
    for (int i = 0; i < nbObjects; i++) {
        // Forcer le test de tous les objets proches même s'ils ont déjà été testés
        float distToObject = distance(rayOrigin, iObjects[i].pos);
        bool forceTest = distToObject < 3.0;
        
        // Si l'objet n'a pas été testé dans la première passe ou est forcé
        if (forceTest || (iObjects[i].id < nbObjects && !tested[iObjects[i].id])) {
            vec4 o = colDist(rayOrigin, iObjects[i]);
            colDistCalls++;
            if (o.a < data.d) {
                data.d = o.a;
                data.color = o.rgb;
            }
        }
    }
    
    data.steps = colDistCalls;
    return data;
}

// Définir une structure pour stocker les plans du KD-tree
struct KDPlane {
    vec3 position;    // Position du plan (point de référence)
    vec3 normal;      // Normale du plan
    int axis;         // 0=X, 1=Y, 2=Z
    int depth;        // Profondeur dans l'arbre KD
};

// Définir une constante pour le nombre maximum de plans à visualiser
const int MAX_PLANES = nbObjects;

// Remplacer la fonction récursive par une version itérative
void collectKDPlanes(inout KDPlane planes[MAX_PLANES], inout int planeCount) {
    // Structure pour simuler une pile
    struct StackItem {
        int nodeIndex;
        int depth;
    };
    const int MAX_STACK = 32;
    StackItem stack[MAX_STACK];
    int stackPos = 0;
    
    // Commencer par le nœud racine
    if (kdtree.length() > 0) {
        stack[0].nodeIndex = 0;
        stack[0].depth = 0;
        stackPos = 1;
    }
    
    // Parcourir l'arbre avec notre pile simulée
    while (stackPos > 0 && planeCount < MAX_PLANES) {
        // Prendre le nœud en haut de la pile
        stackPos--;
        int nodeIndex = stack[stackPos].nodeIndex;
        int depth = stack[stackPos].depth;
        
        // Si l'index est invalide, passer au suivant
        if (nodeIndex < 0 || nodeIndex >= kdtree.length()) continue;
        
        KDNodeGPU node = kdtree[nodeIndex];
        
        // Si ce n'est pas une feuille (a un axe de division valide)
        if (node.split_axis >= 0 && node.split_axis <= 2) {
            // Ajouter ce plan à notre collection
            vec3 position = vec3(0.0);
            vec3 normal = vec3(0.0);
            
            // Définir la position et la normale selon l'axe de division
            if (node.split_axis == 0) {
                position = vec3(node.split_pos, 0.0, 0.0);
                normal = vec3(1.0, 0.0, 0.0);
            } else if (node.split_axis == 1) {
                position = vec3(0.0, node.split_pos, 0.0);
                normal = vec3(0.0, 1.0, 0.0);
            } else if (node.split_axis == 2) {
                position = vec3(0.0, 0.0, node.split_pos);
                normal = vec3(0.0, 0.0, 1.0);
            }
            
            planes[planeCount].position = position;
            planes[planeCount].normal = normal;
            planes[planeCount].axis = node.split_axis;
            planes[planeCount].depth = depth;
            planeCount++;
            
            // Ajouter les enfants à la pile (si valides)
            if (node.right_child_index >= 0 && stackPos < MAX_STACK) {
                stack[stackPos].nodeIndex = node.right_child_index;
                stack[stackPos].depth = depth + 1;
                stackPos++;
            }
            
            if (node.left_child_index >= 0 && stackPos < MAX_STACK) {
                stack[stackPos].nodeIndex = node.left_child_index;
                stack[stackPos].depth = depth + 1;
                stackPos++;
            }
        }
    }
}

// Ajouter cette fonction après collectKDPlanes
void addDebugPlanes(inout KDPlane planes[MAX_PLANES], inout int planeCount) {
    // Vérifier les logs - le KD-tree n'a que 3 nœuds et seulement un avec un axe valide
    // Ajouter manuellement des plans pour les axes Y et Z pour la visualisation
    
    // Si on n'a pas encore de plan pour l'axe Y, en ajouter un
    bool hasYPlane = false;
    bool hasZPlane = false;
    
    for (int i = 0; i < planeCount; i++) {
        if (planes[i].axis == 1) hasYPlane = true;
        if (planes[i].axis == 2) hasZPlane = true;
    }
    
    // Ajouter un plan Y si nécessaire
    if (!hasYPlane && planeCount < MAX_PLANES) {
        planes[planeCount].position = vec3(0.0, 0.0, 0.0);
        planes[planeCount].normal = vec3(0.0, 1.0, 0.0);
        planes[planeCount].axis = 1;  // Y
        planes[planeCount].depth = 1;  // Profondeur 1 
        planeCount++;
    }
    
    // Ajouter un plan Z si nécessaire
    if (!hasZPlane && planeCount < MAX_PLANES) {
        planes[planeCount].position = vec3(0.0, 0.0, 0.0);
        planes[planeCount].normal = vec3(0.0, 0.0, 1.0);
        planes[planeCount].axis = 2;  // Z
        planes[planeCount].depth = 1;  // Profondeur 1
        planeCount++;
    }
}

// Modifier visualizeKDPlanes pour appeler cette fonction
vec4 visualizeKDPlanes(vec3 rayOrigin, vec3 rayDirection, float maxDist, float objectDist) {
    // Tableau pour stocker tous les plans
    KDPlane planes[MAX_PLANES];
    int planeCount = 0;
    
    // Initialiser le tableau
    for (int i = 0; i < MAX_PLANES; i++) {
        planes[i].position = vec3(0.0);
        planes[i].normal = vec3(0.0);
        planes[i].axis = -1;
        planes[i].depth = -1;
    }
    
    // Collecter tous les plans de l'arbre KD de façon non récursive
    collectKDPlanes(planes, planeCount);
    
    // Ajouter des plans de debug si nécessaire 
    addDebugPlanes(planes, planeCount);
    
    // Structure pour stocker les intersections
    const int MAX_INTERSECTIONS = MAX_PLANES;
    struct Intersection {
        float t;           // Distance de l'intersection
        vec4 color;        // Couleur et alpha
        int planeIndex;    // Index du plan correspondant
    };
    
    // Tableau pour stocker les intersections valides
    Intersection intersections[MAX_INTERSECTIONS];
    int intersectionCount = 0;
    
    // Calculer toutes les intersections avec les plans collectés
    for (int i = 0; i < planeCount; i++) {
        vec3 normal = planes[i].normal;
        vec3 position = planes[i].position;
        float denom = dot(rayDirection, normal);
        
        if (abs(denom) > 0.0001) {
            float t = dot(position - rayOrigin, normal) / denom;
            
            // Vérifier si le plan est entre la caméra et l'objet le plus proche
            if (t > 0.01 && t < objectDist) {
                // Calculer la couleur selon l'axe
                vec3 color;
                if (planes[i].axis == 0) color = KD_PLANE_COLOR_X;
                else if (planes[i].axis == 1) color = KD_PLANE_COLOR_Y;
                else color = KD_PLANE_COLOR_Z;
                
                // Calculer l'alpha selon la profondeur
                float baseAlpha = 0.7;
                float depthFactor = 1.0 - float(planes[i].depth) * 0.15; // Réduire l'opacité avec la profondeur
                float alpha = baseAlpha * max(0.3, depthFactor);
                
                // Atténuation avec la distance
                float fadeWithDist = 1.0 - clamp(t/maxDist, 0.0, 0.9);
                alpha *= fadeWithDist;
                
                // Enregistrer cette intersection
                if (intersectionCount < MAX_INTERSECTIONS) {
                    intersections[intersectionCount].t = t;
                    intersections[intersectionCount].color = vec4(color, alpha);
                    intersections[intersectionCount].planeIndex = i;
                    intersectionCount++;
                }
            }
        }
    }
    
    // Si aucune intersection, retourner transparence complète
    if (intersectionCount == 0) {
        return vec4(0.0);
    }
    
    // Trier les intersections par distance (tri simple à bulles)
    for (int i = 0; i < intersectionCount - 1; i++) {
        for (int j = 0; j < intersectionCount - i - 1; j++) {
            if (intersections[j].t > intersections[j+1].t) {
                // Échanger les éléments
                Intersection temp = intersections[j];
                intersections[j] = intersections[j+1];
                intersections[j+1] = temp;
            }
        }
    }
    
    // Trouver les intersections proches pour ajouter des contours
    for (int i = 0; i < intersectionCount; i++) {
        for (int j = i + 1; j < intersectionCount; j++) {
            float dist = abs(intersections[i].t - intersections[j].t);
            
            // Si deux plans sont très proches, c'est une intersection de plans
            if (dist < 0.1) {
                // Vérifier que les plans ne sont pas parallèles (axes différents)
                int axis1 = planes[intersections[i].planeIndex].axis;
                int axis2 = planes[intersections[j].planeIndex].axis;
                
                if (axis1 != axis2) {
                    // Renforcer les contours aux intersections en augmentant l'alpha
                    float contourStrength = 1.0 - dist * 10.0; // Plus fort quand plus proche
                    intersections[i].color.a = min(1.0, intersections[i].color.a + 0.2 * contourStrength);
                    intersections[j].color.a = min(1.0, intersections[j].color.a + 0.2 * contourStrength);
                    
                    // Assombrir légèrement la couleur pour marquer l'intersection
                    intersections[i].color.rgb *= mix(1.0, 0.7, contourStrength * 0.5);
                    intersections[j].color.rgb *= mix(1.0, 0.7, contourStrength * 0.5);
                }
            }
        }
    }
    
    // Combiner toutes les intersections, de la plus proche à la plus éloignée
    vec4 result = vec4(0.0);
    
    // Prendre l'intersection la plus proche (déjà trié)
    if (intersectionCount > 0) {
        result = intersections[0].color;
        
        // Ajouter l'effet de superposition pour les autres plans
        for (int i = 1; i < intersectionCount; i++) {
            vec4 nextColor = intersections[i].color;
            // Mélanger avec le résultat actuel
            result.rgb = mix(result.rgb, nextColor.rgb, nextColor.a * 0.3);
            // Augmenter légèrement l'opacité aux superpositions
            result.a = min(1.0, result.a + nextColor.a * 0.2);
        }
    }
    
    return result;
}

// Modifier la fonction scene pour utiliser le type Data
Data scene(vec3 p, bool initialRayHits[nbObjects]) {
    Data result;
    
    if (iAlgo == 1) {
        // Utiliser le KD-tree
        result = traverseKDTree(p, normalize(p));
    } else if (iAlgo == 2) {
        // Algorithme hybride : raymarching uniquement sur les objets présélectionnés
        result = Data(MAX_DIST, BACKGROUND, 0);
        int colDistCalls = 0;

        float minDistToObj = MAX_DIST;
        vec3 closestColor = BACKGROUND;

        for (int i = 0; i < nbObjects; i++) {
            // Ne tester que les objets touchés par le raytracing initial
            if (initialRayHits[i]) { 
                vec4 o = colDist(p, iObjects[i]);
                colDistCalls++;
                if (o.a < minDistToObj) {
                    minDistToObj = o.a;
                    closestColor = o.rgb;
                }
            }
        }
        result.d = minDistToObj;
        result.color = closestColor;
        result.steps = colDistCalls;
    } else {
        // Algorithme classique
        result = Data(MAX_DIST, BACKGROUND, 0);
        int colDistCalls = 0;
        
        vec4 firstObj = colDist(p, iObjects[0]);
        colDistCalls++;
        result.d = firstObj.a;
        result.color = firstObj.rgb;
        
        for(int i = 1; i < nbObjects; i++) {
            vec4 o = colDist(p, iObjects[i]);
            colDistCalls++;
            if (o.a < result.d) {
                result.d = o.a;
                result.color = o.rgb;
            }
        }
        
        result.steps = colDistCalls;
    }
    
    return result;
}

// Modifier la fonction march pour accumuler tous les appels à colDist
Data march(vec3 rayOrigin, vec3 rayDirection) {
    Data data = Data(0., BACKGROUND, 0); // Initialize color to BACKGROUND by default
    // vec3 currentPoint = rayOrigin; // currentPoint will be calculated inside loop
    // float d = 0.; // Will be replaced by accumulated_d
    Data info_scene;
    int totalColDistCalls = 0; 

    bool initialRayHitsBoundingSphere[nbObjects];
    float overallMinT = MAX_DIST;
    float overallMaxT = 0.0; // Correctly 0.0 for max search
    int boundingSpheresHitCount = 0;

    float accumulated_d;
    float loopMaxDist;

    if (iAlgo == 2) {
        for (int i = 0; i < nbObjects; i++) {
            initialRayHitsBoundingSphere[i] = false;
            vec4 boundingSphere = compute_bounding_sphere(iObjects[i]);
            Intersections isects = raytracing_sphere(boundingSphere.rgb, boundingSphere.a, rayOrigin, rayDirection);
            
            if (isects.count > 0) {
                initialRayHitsBoundingSphere[i] = true;
                boundingSpheresHitCount++;
                for (int k = 0; k < isects.count; k++) {
                    if (isects.t[k] > 0.0) { // Consider only intersections in front
                        if (isects.t[k] < overallMinT) overallMinT = isects.t[k];
                        if (isects.t[k] > overallMaxT) overallMaxT = isects.t[k];
                    }
                }
            }
        }

        if (boundingSpheresHitCount == 0) {
            data.d = MAX_DIST + 1.0; 
            data.color = BACKGROUND; // Explicitly BACKGROUND
            data.steps = 0;
            return data;
        }
        // Start raymarching slightly before the first bounding sphere hit
        accumulated_d = max(0.0, overallMinT - EPSILON * 10.0); 
        // Limit raymarching to slightly after the last bounding sphere hit or MAX_DIST
        loopMaxDist = min(overallMaxT + EPSILON * 10.0, MAX_DIST);
        if (overallMinT > overallMaxT) { // No valid positive t values found in isects
             data.d = MAX_DIST + 1.0; 
            data.color = BACKGROUND;
            data.steps = 0;
            return data;
        }
        
    } else {
        for (int i = 0; i < nbObjects; i++) {
            initialRayHitsBoundingSphere[i] = true; 
        }
        accumulated_d = 0.0;
        loopMaxDist = MAX_DIST;
    }
    
    for(int i = 0; i < MAX_STEPS; i++) {
        vec3 currentPoint = rayOrigin + rayDirection * accumulated_d;
        info_scene = scene(currentPoint, initialRayHitsBoundingSphere);
        totalColDistCalls += info_scene.steps;
        
        if(info_scene.d < EPSILON) { // HIT
            data.d = accumulated_d; // Distance to currentPoint which is on the surface
            data.color = info_scene.color;
            data.steps = totalColDistCalls;
            return data;
        }
        
        // Advance ray by the SDF distance (ensure it's positive to make progress)
        accumulated_d += max(info_scene.d, EPSILON * 0.1); 
        
        if(accumulated_d > loopMaxDist) {
            break; // Exceeded allowed distance for this raymarch (overallMaxT or MAX_DIST)
        }
    }
    
    // If loop finishes (MAX_STEPS or accumulated_d > loopMaxDist) without an EPSILON hit
    data.d = MAX_DIST + 1.0; // Signal miss
    data.color = BACKGROUND;   // Ensure background color
    data.steps = totalColDistCalls;
    return data;
}


//calulate the normale of a pixel
vec3 normal(vec3 p){
    // Pour normal, on doit tester tous les objets autour du point p,
    // donc on passe un tableau où tout est à true.
    bool allObjects[nbObjects];
    for(int i=0; i < nbObjects; ++i) allObjects[i] = true;
    
    Data sceneData = scene(p, allObjects);
    float dP = sceneData.d;
    vec2 eps = vec2(0.01, 0.0);
    
    float dX = scene(p + eps.xyy, allObjects).d - dP;
    float dY = scene(p + eps.yxy, allObjects).d - dP;
    float dZ = scene(p + eps.yyx, allObjects).d - dP;
    
    return normalize(vec3(dX, dY, dZ));
}

Data lighting(vec3 p, vec3 n){
    vec3 lP = vec3(cos(iGlobalTime) * RADIUS_OF_LIGHT, 1.0, sin(iGlobalTime) * RADIUS_OF_LIGHT); //position de la lumiere
    vec3 lD = lP - p; //direction of the light
    vec3 lN = normalize(lD); //direction normalize
    Data lighting = march(p + n * 0.01, lN);
    float l = 0.;
    if(lighting.d >= length(lD)) // if the distance is not bigger than the norme of the direction (if it's not the background)
        l = max(0.0, dot(n, lN)); //no lighting
    
    lighting.color = vec3(l);
    return lighting; //else we return the dot product between the normale and the direction of the light form of nb positive
}

void mainImage(out vec4 frag_Color, in vec2 fragCoord) {
    vec2 uv = (fragCoord - (iResolution.xy * 0.5)) / iResolution.y;
    
    vec2 mouse = iMouse.xy / iResolution.xy;
    
    float initAngle = -(PI * 0.5);
    
    vec3 rayOrigin = vec3(cos(mouse.x * 3.0 * PI + initAngle) * RADIUS_MOUSE, tan(mouse.y * PI * 0.4 + 0.1), sin(mouse.x * 3.0 * PI + initAngle) * RADIUS_MOUSE);
    vec3 target = vec3(0, 0, 0);
    
    vec3 fwd = normalize(target - rayOrigin); //vector foreward
    vec3 side = normalize(cross(vec3(0, 1.0, 0), fwd)); //cross product => return a vector perpendicular at two vectors
    vec3 up = normalize(cross(fwd, side));
    
    vec3 rayDirection = normalize(ZOOM * fwd + side * uv.x + up * uv.y); //vector where the camera is pointing
    
    
    Data data_scene = march(rayOrigin, rayDirection);
    //float d = data_scene.d; // d is part of data_scene
    vec3 col; // Declare col here
    
    if(data_scene.d < MAX_DIST) { // HIT
        if(iViewPerf > 0) {
            col = color_steps(data_scene.steps, nbObjects*MAX_STEPS); // Or some other appropriate max
        } else {
            vec3 p = rayOrigin + rayDirection * data_scene.d; //pts touch
        col = data_scene.color;
        vec3 nor = normal(p);
        Data data_lighting = lighting(p, nor);
        
        vec3 amb_backg = nor.y * BACKGROUND * BACKGROUND_LIGHT_INTENSITY;
    
        col *= (data_lighting.color.r + amb_backg);
        col = pow(col, vec3(0.4545)); //gamma curve correction
        }
    } else { // MISS (data_scene.d >= MAX_DIST)
        if(iViewPerf > 0) {
            col = color_steps(data_scene.steps, nbObjects*MAX_STEPS); // Or 1,1 if steps is high for misses
        } else {
            col = BACKGROUND; // Explicitly set to background for misses
        }
    }

    
    // Visualisation des plans KD-tree si activé, et seulement si ce n'est pas un miss complet.
    // Ou si on veut les voir même sur le fond.
    if (iAlgo == 1 && iDisplayKDTree == 1) { // KD-Tree plane visualization specifically for iAlgo 1
        // Pass data_scene.d to visualizeKDPlanes. If it was a miss, d will be large.
        vec4 planesColor = visualizeKDPlanes(rayOrigin, rayDirection, MAX_DIST, data_scene.d);
        if (planesColor.a > 0.0) {
            col = mix(col, planesColor.rgb, planesColor.a); // Adjusted mixing for better visibility
        }
    }
    
    frag_Color = vec4(col, 1.0);
}

void main (void)
{
  vec4 color = vec4 (0.0, 0.0, 0.0, 1.0);
  mainImage (color, gl_FragCoord.xy);
  color.w = 1.0;
  FragColor = color;
}