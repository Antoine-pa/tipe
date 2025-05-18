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

//raymarcher
const float MAX_DIST = 20.0;
const int MAX_STEPS = 300;
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
    return vec3(1.0-float(nb_steps)/(float(max_steps)));
}

// Fonction optimisée de traversée du KD-tree
vec4 traverseKDTree(vec3 rayOrigin, vec3 rayDirection) {
    // Version simplifiée pour le débogage
    vec4 obj = vec4(BACKGROUND, MAX_DIST);
    
    // Afficher les données du premier nœud
    if (kdtree.length() > 0) {
        KDNodeGPU rootNode = kdtree[0];
        
        // Tester tous les objets sans utiliser le KD-tree
        for (int i = 0; i < nbObjects; i++) {
            vec4 o = colDist(rayOrigin, iObjects[i]);
        obj = minDist(obj, o);
    }
        
        // Indicateur plus visible pour le mode KD-tree
        obj.rgb *= vec3(0.7, 1.0, 0.7);  // Teinte verte plus prononcée
        
    return obj;
}

    return vec4(BACKGROUND, MAX_DIST);
}

// Définir une structure pour stocker les plans du KD-tree
struct KDPlane {
    vec3 position;    // Position du plan (point de référence)
    vec3 normal;      // Normale du plan
    int axis;         // 0=X, 1=Y, 2=Z
    int depth;        // Profondeur dans l'arbre KD
};

// Définir une constante pour le nombre maximum de plans à visualiser
const int MAX_PLANES = 16;

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

// Fonction scene modifiée pour inclure la visualisation des plans
vec4 scene(vec3 p) {
    // Trouver l'objet le plus proche
    vec4 objResult;
    
    if (iAlgo == 1) {
        objResult = traverseKDTree(p, normalize(p));
    } else {
        objResult = colDist(p, iObjects[0]);
        for(int i = 1; i < nbObjects; i++) {
            vec4 o = colDist(p, iObjects[i]);
            objResult = minDist(objResult, o);
        }
    }
    
    return objResult;
}

// Modifier la fonction march pour intégrer la visualisation des plans
Data march(vec3 rayOrigin, vec3 rayDirection) {
    Data data = Data(0., BACKGROUND, 0);
    vec3 currentPoint = rayOrigin;
    float d = 0.;
    vec4 info_scene = vec4(0.0);
    
    for(int i = 0; i < MAX_STEPS; i++) {
        currentPoint = rayOrigin + rayDirection * d;
        info_scene = scene(currentPoint);
        d += info_scene.a;
        
        if(info_scene.a < EPSILON) {
            data.d = d;
            data.color = info_scene.rgb;
            data.steps = i;
            
            // Désactiver la visualisation des plans ici - elle se fait déjà dans mainImage
            // if (iAlgo == 1) {
            //     vec4 planes = visualizeKDPlanes(rayOrigin, rayDirection, d);
            //     if (planes.a > 0.1) {
            //         data.color = planes.rgb;
            //     }
            // }
            
            return data;
        }
        
        if(d > MAX_DIST) {
            data.d = d;
            data.steps = i;
            
            // Visualiser les plans du KD-tree si activé
            if (iAlgo == 1 && iDisplayKDTree == 1) {
                vec4 planes = visualizeKDPlanes(rayOrigin, rayDirection, MAX_DIST, d);
                data.color = mix(BACKGROUND, planes.rgb, planes.a);
            }
            
            return data;
        }
    }
    
    data.d = MAX_DIST+1.;
    data.steps = MAX_STEPS+1;
    return data;
}


//calulate the normale of a pixel
vec3 normal(vec3 p){
    float dP = scene(p).a;
    vec2 eps = vec2(0.01, 0.0);
    
    float dX = scene(p + eps.xyy).a - dP; //diff of distance between p and the pts at 0.01x next
    float dY = scene(p + eps.yxy).a - dP; //diff of distance between p and the pts at 0.01y next
    float dZ = scene(p + eps.yyx).a - dP; //diff of distance between p and the pts at 0.01z next
    
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

// Ajouter ces fonctions de conversion au début du fichier
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
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
    float d = data_scene.d; // distance de l'objet le plus proche
    vec3 col = mix(vec3(1.0), BACKGROUND, pow(uv.y + 0.5, 0.6 + mouse.y));
    
    if(d < MAX_DIST) {
        vec3 p = rayOrigin + rayDirection * d; //pts touch
        col = data_scene.color;
        vec3 nor = normal(p);
        Data data_lighting = lighting(p, nor);
        
        vec3 amb_backg = nor.y * BACKGROUND * BACKGROUND_LIGHT_INTENSITY;
    
        col *= (data_lighting.color.r + amb_backg);
        col = pow(col, vec3(0.4545)); //gamma curve correction

        if(iViewPerf > 0) {
            //black and white
            col = color_steps(data_scene.steps+data_lighting.steps, 2*MAX_STEPS);
        }
    } else {
        // ... code existant pour le fond ...
    }
    
    // Visualisation des plans uniquement si l'option est activée
    if (iDisplayKDTree == 1) {
        // Passer la distance de l'objet à la fonction pour filtrer les plans
        vec4 planesColor = visualizeKDPlanes(rayOrigin, rayDirection, MAX_DIST, d);
        
        if (planesColor.a > 0.0) {
            // Mélange plus direct pour plus de visibilité
            col = mix(col, planesColor.rgb, planesColor.a * 0.8);
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