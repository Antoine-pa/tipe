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
const int nbObjects = 6;

uniform vec4                iResolution_;           // viewport resolution (in pixels)
uniform float               iGlobalTime;           // shader playback time (in seconds)
uniform vec4                iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
int iViewPerf = int(iResolution_.a);
vec3 iResolution = iResolution_.rgb;
//uniform Object iObjects[6];
/*
layout (std140) uniform Objects
{
    Object[nbObjects]   iObjects;
};
*/

layout(std430, binding = 0) buffer Objects {
    Object iObjects[];
};

out vec4 FragColor;

struct Data
{
  float d;
  vec3 color;
  int steps;
};

/*
const Object iObjects[nbObjects] = {
    Object(vec3(0.06, 0.04, 1.0), 0, vec3(0.0, 0.0, 0.0), 0.5, vec3(0.0, 0.0, 0.0), 0.0, vec3(0.0, 0.0, 0.0), 0),
    Object(vec3(0.3, 0.04, 0.06), 0, vec3(0.1, 0.45, -0.1), 0.2, vec3(0.0, 0.0, 0.0), 0.0, vec3(0.0, 0.0, 0.0), 1),
    Object(vec3(1.0, 0.4, 0.0), 1, vec3(0.0, 0.2, 0.1), 0.8, vec3(0.0, 0.37, 0.93), 0.1, vec3(0.0, 0.0, 0.0), 2),
    Object(vec3(1.0, 0.2, 0.1), 2, vec3(0.6, 0.3, 0.4), 0.0, vec3(0.1, 0.4, 0.3), 0.3, vec3(0.12, 0.4, 0.0), 3),
    Object(vec3(0.96, 0.41, 0.6), 3, vec3(-0.65, 0.5, 0.0), 0.0, vec3(0.0, 0.5, 0.2), 0.05, vec3(0.2, 0.2, 0.2), 4),
    Object(vec3(0.25, 0.45, 0.5), 4, vec3(0.0, 0.0, 0.0), 0.0, vec3(0.0, 1.0, 0.0), 0.0, vec3(0.0, 0.0, 0.0), 5),
};
*/
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

vec4 Sphere(vec3 p, float r, vec3 c){
    return vec4(c, length(p) - r); //rgb=>color, a=>distance
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

//calculate the min distance object
vec4 scene(vec3 p){
    vec4 obj = colDist(p, iObjects[0]);
    for(int i = 1; i<nbObjects; i++) {
        vec4 o = colDist(p, iObjects[i]);
        obj = minDist(obj, o);
    }
    return obj;
}

//raymarching
Data march(vec3 rayOrigin, vec3 rayDirection){
    Data data = Data(0., BACKGROUND, 0);
    vec3 currentPoint = rayOrigin; //current point
    float d = 0.;
    vec4 info_scene = vec4(0.0);
    
    for(int i = 0; i < MAX_STEPS; i++){
        currentPoint = rayOrigin + rayDirection * d;
        info_scene = scene(currentPoint);
        d += info_scene.a;
        if(info_scene.a < EPSILON){
            data.d = d;
            data.color = info_scene.rgb;
            data.steps = i;
            return data;
        }
        if(d > MAX_DIST){
            data.d = d;
            data.steps = i;
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

void mainImage( out vec4 frag_Color, in vec2 fragCoord )
{
    vec2 uv = (fragCoord - (iResolution.xy * 0.5)) / iResolution.y;
    
    vec2 mouse = iMouse.xy / iResolution.xy;
    
    float initAngle = -(PI * 0.5);
    
    vec3 rayOrigin = vec3(cos(mouse.x * 3.0 * PI + initAngle) * RADIUS_MOUSE, tan(mouse.y * PI * 0.4 + 0.1), sin(mouse.x * 3.0 * PI + initAngle) * RADIUS_MOUSE);
    vec3 target = vec3(0, 0, 0);
    
    vec3 fwd = normalize(target - rayOrigin); //vector foreward
    vec3 side = normalize(cross(vec3(0, 1.0, 0), fwd)); //cross product => return a vector perpendicular at two vectors
    vec3 up = normalize(cross(fwd, side));
    
    vec3 rayDirection = normalize(ZOOM * fwd + side * uv.x + up * uv.y); //vector where the camera is pointing
    
    
    Data data_scene = march(rayOrigin, rayDirection); //raymarching, rgb=>color, a=> distance
    float d = data_scene.d; //distance
    vec3 col = mix(vec3(1.0), BACKGROUND, pow(uv.y + 0.5, 0.6 + mouse.y)); //linear interpollation between white and the background color in terms of uv.y
    //col = color_steps(data_scene.steps, MAX_STEPS);
    if(d < MAX_DIST){
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
    }
    else if(iViewPerf > 0) {
        //black and white
        col = color_steps(data_scene.steps, MAX_STEPS);
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