// world.cpp
// Lógica del laberinto 3D: generación, físicas, cámara, portal, prismas, etc.

#include <GL/glut.h>
#include <GL/glu.h>

#include <vector>
#include <array>
#include <stack>
#include <random>
#include <cmath>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <cstdlib>

#include <string>   

// Comunicación con el sistema de puzzles (definido en puzzles.cpp)
extern void Puzzles_OpenForPrism(int prismIndex);
extern bool Puzzles_IsOpen();
extern void Puzzles_Init(int numPrisms);
// -----------------------------------------------------------------------------
// Parámetros globales de cámara / movimiento
// -----------------------------------------------------------------------------

static float camX = 1.5f, camY = 1.62f, camZ = 1.5f;
static float yaw = 0.0f, pitch = 0.0f;
static float baseSpeed = 3.0f, mouseSens = 0.0028f;
static float gravity = 18.0f, jumpVel = 6.8f, velY = 0.0f;
static bool onGround = true;
static bool keys[256] = { false };
static bool mouseCaptured = true;

static bool  sprint = false;
static bool  wIsDown = false;
static int   lastWTapMs = -100000;
static const int   SPRINT_DOUBLE_TAP_MS = 250;
static const float SPRINT_MULT = 2.8f;

static int  winW = 1600, winH = 900;

static bool isFullscreen = false;
// Flag de pausa global
static bool g_Paused = false;
// Numero de vidas
static int g_PlayerLives = 3;

// Bloquear sprint (doble W) despues de fallar un puzzle
static bool g_SprintBlocked = false;

// Controles invertidos (W/S, A/D) al perder la 2da vida
static bool g_InvertControls = false;

// Narrador
static std::string g_SrxFullLine;     // texto completo: "[SRX]: lo que sea"
static int g_SrxStartMs = 0;  // momento en el que empezó a mostrarse

// Velocidad de escritura y tiempos del efecto
static const float SRX_CHARS_PER_SEC = 25.0f;  // más lento que antes
static const float SRX_HOLD_SEC = 3.5f;   // tiempo con texto completo visible
static const float SRX_FADE_SEC = 3.0f;   // tiempo del desvanecido (fade lento)

enum class LevelDifficulty {
    EASY,
    MEDIUM,
    HARD
};
LevelDifficulty g_CurrentLevel = LevelDifficulty::EASY;
// Rombos rojos en el nivel facil/medio
bool g_PrismIsRed = false;
enum class TransitionState {
    NONE,
    FADING_OUT,
    FADING_IN
};

static TransitionState g_TransitionState = TransitionState::NONE;
static float g_TransitionTime = 0.0f;
static const float TRANSITION_TOTAL = 2.0f; // 2 segundo para cada fase

static LevelDifficulty g_TransitionTargetLevel = LevelDifficulty::EASY;








// RNG para frases random
static std::mt19937 g_SrxRng{ std::random_device{}() };

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// -----------------------------------------------------------------------------
// Mundo / laberinto
// -----------------------------------------------------------------------------

static const float CELL = 2.7f;
static const float wallH = 8.0f;

// Sky
static float skyYawDeg = 0.0f;
static float skyPitchDeg = 0.0f;
static float skyRollDeg = 0.0f;
static bool  skyFlipV = true;
static const float SKYDOME_RAD = 250.0f;

// Texturas
GLuint texFloor = 0;
static const float FLOOR_UV_PER_UNIT = 0.5f;

GLuint texSkyEquirect = 0;
GLuint texWall = 0;
static const float UV_SCALE = 1.0f;

// 0 = normal
// 1 = muros sin textura, grises
// 2 = cielo rojo, muros y suelo negros
// 3 = cielo sin textura, azul muy oscuro (noche); resto sigue negro
// 4 = solo queda el portal flotando en el vacio negro
static int g_WorldStage = 0;

// Quadrics reutilizables
GLUquadric* gQuadricSky = nullptr;
GLUquadric* gQuadricSphere = nullptr;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline float clampf(float v, float a, float b) {
    return v < a ? a : (v > b ? b : v);
}

// -----------------------------------------------------------------------------
// Mapa del laberinto
// -----------------------------------------------------------------------------

#define USE_EXAMPLE_A

#ifdef USE_EXAMPLE_A
static const int MAP_W = 7;
static const int MAP_H = 35;
static int maze[MAP_H][MAP_W];

// 1 = muro, 0 = espacio (pasillo central en la columna 3)
static int mazeHard[MAP_H][MAP_W] = {
    {1,1,1,0,1,1,1}, // 0
    {1,1,1,0,1,1,1}, // 1
    {1,1,1,0,1,1,1}, // 2
    {1,1,1,0,1,1,1}, // 3
    {1,1,1,0,1,1,1}, // 4
    {1,1,1,0,1,1,1}, // 5
    {1,1,1,0,1,1,1}, // 6
    {1,1,1,0,1,1,1}, // 7
    {1,1,1,0,1,1,1}, // 8
    {1,1,1,0,1,1,1}, // 9
    {1,1,1,0,1,1,1}, // 10
    {1,1,1,0,1,1,1}, // 11
    {1,1,1,0,1,1,1}, // 12
    {1,1,1,0,1,1,1}, // 13
    {1,1,1,0,1,1,1}, // 14
    {1,1,1,0,1,1,1}, // 15
    {1,1,1,0,1,1,1}, // 16
    {1,1,1,0,1,1,1}, // 17
    {1,1,1,0,1,1,1}, // 18
    {1,1,1,0,1,1,1}, // 19
    {1,1,1,0,1,1,1}, // 20
    {1,1,1,0,1,1,1}, // 21
    {1,1,1,0,1,1,1}, // 22
    {1,1,1,0,1,1,1}, // 23
    {1,1,1,0,1,1,1}, // 24
    {1,1,1,0,1,1,1}, // 25
    {1,1,1,0,1,1,1}, // 26
    {1,1,1,0,1,1,1}, // 27
    {1,1,1,0,1,1,1}, // 28
    {1,1,1,0,1,1,1}, // 29
    {1,1,1,0,1,1,1}, // 30
    {1,1,1,0,1,1,1}, // 31
    {1,1,1,0,1,1,1}, // 32
    {1,1,1,0,1,1,1}, // 33
    {1,1,1,0,1,1,1}  // 34
};
#endif
static const int mazeEasy[MAP_H][MAP_W] = {
 //  0 1 2 3 4 5 6
    {1,1,1,0,1,1,1}, // 0
    {1,1,1,0,1,1,1}, // 1
    {1,0,0,0,0,0,1}, // 2
    {1,0,1,0,1,0,1}, // 3
    {1,0,0,0,0,0,1}, // 4
    {1,1,1,0,1,1,1}, // 5
    {1,0,0,0,0,0,1}, // 6
    {1,0,1,1,1,0,1}, // 7
    {1,0,0,0,1,0,1}, // 8
    {1,1,1,0,1,0,1}, // 9
    {1,0,0,0,0,0,1}, // 10
    {1,0,1,1,1,1,1}, // 11
    {1,0,1,0,0,0,1}, // 12
    {1,0,1,0,1,0,1}, // 13
    {1,0,0,0,1,0,1}, // 14
    {1,1,1,1,1,0,1}, // 15
    {1,0,0,0,0,0,1}, // 16
    {1,0,1,1,1,1,1}, // 17
    {1,0,0,0,1,1,1}, // 18
    {1,1,1,0,1,1,1}, // 19
    {1,1,1,0,0,0,1}, // 20
    {1,1,1,1,1,0,1}, // 21
    {1,1,1,1,1,0,1}, // 22
    {1,1,1,1,1,0,1}, // 23
    {1,1,1,1,1,0,1}, // 24
    {1,0,0,0,0,0,1}, // 25
    {1,0,1,1,1,1,1}, // 26
    {1,0,0,0,1,1,1}, // 27
    {1,1,1,0,1,1,1}, // 28
    {1,0,0,0,1,1,1}, // 29
    {1,0,1,1,1,1,1}, // 30
    {1,0,1,1,1,1,1}, // 31
    {1,0,0,0,1,1,1}, // 32 
    {1,1,1,0,1,1,1}, // 33 
    {1,1,1,0,1,1,1}  // 34
};

static const int mazeMedium[MAP_H][MAP_W] = {
 //  0 1 2 3 4 5 6
    {1,1,1,0,1,1,1}, // 0
    {1,0,0,0,0,0,1}, // 1
    {1,0,1,1,1,0,1}, // 2
    {1,0,1,1,0,0,1}, // 3
    {1,0,0,0,1,1,1}, // 4
    {1,1,1,0,1,1,1}, // 5
    {1,0,0,0,0,0,1}, // 6
    {1,0,1,1,1,0,1}, // 7
    {1,0,0,0,1,0,1}, // 8
    {1,1,1,1,1,0,1}, // 9
    {1,0,0,0,0,0,1}, // 10
    {1,0,1,0,1,1,1}, // 11
    {1,0,1,0,0,0,1}, // 12
    {1,0,1,1,1,0,1}, // 13
    {1,0,0,0,1,0,1}, // 14
    {1,1,1,0,1,1,1}, // 15
    {1,0,0,0,0,0,1}, // 16
    {1,0,1,1,1,0,1}, // 17
    {1,0,0,0,1,0,1}, // 18
    {1,1,1,1,1,0,1}, // 19
    {1,0,0,0,0,0,1}, // 20
    {1,0,1,0,1,1,1}, // 21
    {1,0,1,0,1,0,1}, // 22
    {1,0,1,0,0,0,1}, // 23
    {1,1,1,1,1,0,1}, // 24
    {1,0,0,0,0,0,1}, // 25
    {1,0,1,1,1,0,1}, // 26
    {1,0,1,0,1,0,1}, // 27
    {1,0,1,0,1,0,1}, // 28
    {1,0,1,0,0,0,1}, // 29
    {1,0,1,1,1,1,1}, // 30
    {1,0,0,0,0,0,1}, // 31
    {1,1,1,1,1,0,1}, // 32 
    {1,1,1,0,0,0,1}, // 33 
    {1,1,1,0,1,1,1}  // 34
};
struct CellCoord {
    int x; // columna
    int z; // fila
};

struct AABB {
    float minx, miny, minz;
    float maxx, maxy, maxz;
};

struct Rect {
    int x, z, w, l;
};

std::vector<AABB> decorWalls;
std::vector<AABB> hiddenWalls;
std::vector<AABB> walls;
std::vector<Rect> wallRects;
std::vector<AABB> extraWalls;

// prismas verdes (triggers de puzzles)
static std::vector<CellCoord> greenPrismsHard = {
    {3,  0},
    {3,  4},
    {3,  9},
    {3, 14},
    {3, 19},
    {3, 24},
    {3, 29},
    {3, 34}
};
// Nivel facil
static std::vector<CellCoord> greenPrismsEasy = {
    {3, 4},
    {2, 10},
    {5, 16},
    {5, 22},
    {3, 29}
};
// prismas verdes (triggers de puzzles)
static std::vector<CellCoord> greenPrismsMedium = {
    {3,  1},
    {3,  6},
    {3, 10},
    {3, 14},
    {3, 20},
    {5, 25},
    {2, 31}
};

// Conjunto ACTIVO de rombos para el nivel actual
static std::vector<CellCoord> greenPrisms;
static std::vector<bool> greenPrismActive;
bool gHasSkyTexture = false;

// “foyer” + pasillo
static const float START_W = 6.0f * CELL;
static const float START_D = 4.0f * CELL;
static const float CORRIDOR_W = 1.5f * CELL;

static inline float ENTRANCE_CX() { return ((MAP_W / 2) + 0.5f) * CELL; }
static inline float START_CX() { return ENTRANCE_CX(); }
static inline float START_CZ() { return -8.0f * CELL; }

static const float PLAYER_Y_EYE = 1.62f;
static float PLAYER_SPAWN_X = 0.0f;
static float PLAYER_SPAWN_Z = 0.0f;

static inline AABB MakeAABB(float x0, float y0, float z0,
    float x1, float y1, float z1)
{
    AABB b;
    b.minx = std::min(x0, x1); b.maxx = std::max(x0, x1);
    b.miny = std::min(y0, y1); b.maxy = std::max(y0, y1);
    b.minz = std::min(z0, z1); b.maxz = std::max(z0, z1);
    return b;
}
void World_SetNarratorLine(const char* text, int /*durationMs*/)
{
    if (!text || !*text)
    {
        g_SrxFullLine.clear();
        g_SrxStartMs = 0;
        return;
    }

    g_SrxFullLine = text;
    g_SrxStartMs = glutGet(GLUT_ELAPSED_TIME);
}
static void SetupSrxWelcomeForCurrentLevel()
{
    // Frases de bienvenida (las mismas que ya tienes)
    static const char* s_WelcomeHard[] = {
        "[SRX]: Bienvenido… o lo que sea. No esperaba mucho de ti, pero adelante, sorprendeme con tu mediocre desempeno.",
        "[SRX]: Has entrado. Depresion cronica te dio acceso anticipado.",
        "[SRX]: SRX presente. Tu tambien, por desgracia.",
        "[SRX]: Llegaste. El juego ya bajo sus estandares para recibirte."
    };

    static const char* s_WelcomeEasy[] = {
        "[SRX]: Bienvenido al nivel facil. Si esto ya te cuesta no me quiero ni imaginar el resto. Y no preguntes por las vidas... Lo entenderas luego",
        "[SRX]: Empezamos suave, no porque lo merezcas, es mas bien para que no te pierdas. Y no preguntes por las vidas... Lo entenderas luego",
        "[SRX]: Nivel facil. Consideralo un tutorial para personas con capacidades limitadas... como tu. Y no preguntes por las vidas... Lo entenderas luego",
        "[SRX]: Esto es lo mas sencillo que vas a ver por aqui. De todas formas no tengo nada de fe en ti. Y no preguntes por las vidas... Lo entenderas luego"
       
    };

    // Nivel medio: explícitamente sin texto al inicio
    if (g_CurrentLevel == LevelDifficulty::MEDIUM) {
        World_SetNarratorLine("", 0);
        return;
    }

    const char** lines = nullptr;
    int count = 0;

    if (g_CurrentLevel == LevelDifficulty::EASY) {
        lines = s_WelcomeEasy;
        count = (int)std::size(s_WelcomeEasy);
    }
    else if (g_CurrentLevel == LevelDifficulty::HARD) {
        lines = s_WelcomeHard;
        count = (int)std::size(s_WelcomeHard);
    }
    else {
        // Por seguridad, si hubiera otra dificultad futura
        World_SetNarratorLine("", 0);
        return;
    }

    std::uniform_int_distribution<int> dist(0, count - 1);
    const char* line = lines[dist(g_SrxRng)];
    World_SetNarratorLine(line, 7000);
}



// -----------------------------------------------------------------------------
// Carga de texturas
// -----------------------------------------------------------------------------

GLuint loadTextureSTB(const char* filename) {
    int w, h, ch;
    unsigned char* data = stbi_load(filename, &w, &h, &ch, 3);
    if (!data) {
        std::cerr << "Error cargando textura: " << filename << std::endl;
        return 0;
    }
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return id;
}

GLuint loadTextureEquirect(const char* filename) {
    int w, h, ch;
    unsigned char* data = stbi_load(filename, &w, &h, &ch, 3);
    if (!data) {
        std::cerr << "Error cargando panorama: " << filename << std::endl;
        gHasSkyTexture = false;
        return 0;
    }

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);        // 360°
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // polos
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    gHasSkyTexture = true;
    return id;
}

// -----------------------------------------------------------------------------
// Construcción de la sala final, foyer, etc.
// -----------------------------------------------------------------------------

void buildEndRoom() {
    const float centerX = ENTRANCE_CX();
    const float labEndZ = MAP_H * CELL;

    const float roomW = 4.0f * CELL;
    const float roomD = 3.0f * CELL;
    const float halfW = 0.5f * roomW;

    const float rx0 = centerX - halfW;
    const float rx1 = centerX + halfW;
    const float rz0 = labEndZ;
    const float rz1 = labEndZ + roomD;

    const float doorW = CORRIDOR_W + 0.5f * CELL;
    const float halfDoor = 0.5f * doorW;
    const float doorX0 = centerX - halfDoor;
    const float doorX1 = centerX + halfDoor;

    const float jambThk = 0.12f * CELL;
    const float eps = 0.05f;

    auto addWall = [&](float x0, float y0, float z0,
        float x1, float y1, float z1) {
            AABB a = MakeAABB(x0, y0, z0, x1, y1, z1);
            walls.emplace_back(a);       // colisión
            extraWalls.emplace_back(a);  // render
        };

    // columnas de puerta
    addWall(doorX0 - jambThk, 0.0f, rz0 - eps, doorX0, wallH, rz0 + eps);
    addWall(doorX1, 0.0f, rz0 - eps, doorX1 + jambThk, wallH, rz0 + eps);

    // Laterales y fondo
    addWall(rx0 - eps, 0.0f, rz0, rx0 + eps, wallH, rz1); // lateral izq
    addWall(rx1 - eps, 0.0f, rz0, rx1 + eps, wallH, rz1); // lateral der
    addWall(rx0, 0.0f, rz1 - eps, rx1, wallH, rz1 + eps); // fondo
}

// -----------------------------------------------------------------------------
// Dibujar prisma (rombo) azul oscuro en el pasillo
// -----------------------------------------------------------------------------
void greedyMerge();
void buildFoyerAndCorridor();
void buildEndRoom();

static void LoadLevelData()
{
    // 1) Elegir matriz y rombos según dificultad
    const int (*srcMaze)[MAP_W] = nullptr;

    if (g_CurrentLevel == LevelDifficulty::EASY) {
        srcMaze = mazeEasy;
        greenPrisms = greenPrismsEasy;
        g_PrismIsRed = true;   // modo "amable"
    }
    else if (g_CurrentLevel == LevelDifficulty::MEDIUM) {
        srcMaze = mazeMedium;
        greenPrisms = greenPrismsMedium;
        g_PrismIsRed = true;   // mismas mecánicas que easy (sin SRX / sin insultos)
    }
    else { // HARD
        srcMaze = mazeHard;
        greenPrisms = greenPrismsHard;
        g_PrismIsRed = false;  // modo cruel
    }

    // Copiar la matriz elegida al buffer 'maze' usado por todo el código
    std::memcpy(maze, srcMaze, sizeof(maze));

    // Reset de rombos activos
    greenPrismActive.assign(greenPrisms.size(), true);

    // 2) Cargar texturas segun nivel
    if (g_CurrentLevel == LevelDifficulty::EASY) {
        texWall = loadTextureSTB("textures/wall.jpg");
        texSkyEquirect = loadTextureEquirect("textures/panorama.jpg");
    }
    else if (g_CurrentLevel == LevelDifficulty::MEDIUM) {
        texWall = loadTextureSTB("textures/wall2.jpg");
        texSkyEquirect = loadTextureEquirect("textures/panorama3.jpg");
    }
    else { // HARD
        texWall = loadTextureSTB("textures/wall5.jpg");
        texSkyEquirect = loadTextureEquirect("textures/panorama2.jpg");
    }

    // 3) Reconstruir geometría de muros y habitaciones
    greedyMerge();
    buildFoyerAndCorridor();
    buildEndRoom();

    // 4) Inicializar puzzles para este nivel
    Puzzles_Init((int)greenPrisms.size());
}

void drawGreenDiamond() {
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LIGHTING_BIT | GL_TEXTURE_BIT);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);

    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4] = { 0.0f,  0.0f,  0.0f,  1.0f };

    // Color según dificultad / nivel
    if (g_PrismIsRed) {
        // Nivel fácil: rombo rojo
        ambient[0] = 0.15f; ambient[1] = 0.02f; ambient[2] = 0.02f; ambient[3] = 1.0f;
        diffuse[0] = 0.90f; diffuse[1] = 0.10f; diffuse[2] = 0.10f; diffuse[3] = 1.0f;
    }
    else {
        // Nivel difícil: color original “azul oscuro”
        ambient[0] = 0.05f; ambient[1] = 0.08f; ambient[2] = 0.20f; ambient[3] = 1.0f;
        diffuse[0] = 0.10f; diffuse[1] = 0.12f; diffuse[2] = 0.28f; diffuse[3] = 1.0f;
    }

    glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT, GL_SHININESS, 0.0f);

    const float h = 1.0f;
    const float r = 0.5f;

    const GLfloat top[3] = { 0.0f,  h,  0.0f };
    const GLfloat bottom[3] = { 0.0f, -h,  0.0f };
    const GLfloat e1[3] = { r,   0.0f,  0.0f };
    const GLfloat e2[3] = { 0.0f, 0.0f,  r };
    const GLfloat e3[3] = { -r,   0.0f,  0.0f };
    const GLfloat e4[3] = { 0.0f, 0.0f, -r };

    glBegin(GL_TRIANGLES);

    glNormal3f(0.0f, 1.0f, 0.0f);
    // caras superiores
    glVertex3fv(top); glVertex3fv(e1); glVertex3fv(e2);
    glVertex3fv(top); glVertex3fv(e2); glVertex3fv(e3);
    glVertex3fv(top); glVertex3fv(e3); glVertex3fv(e4);
    glVertex3fv(top); glVertex3fv(e4); glVertex3fv(e1);

    glNormal3f(0.0f, -1.0f, 0.0f);
    // caras inferiores
    glVertex3fv(bottom); glVertex3fv(e2); glVertex3fv(e1);
    glVertex3fv(bottom); glVertex3fv(e3); glVertex3fv(e2);
    glVertex3fv(bottom); glVertex3fv(e4); glVertex3fv(e3);
    glVertex3fv(bottom); glVertex3fv(e1); glVertex3fv(e4);

    glEnd();

    glPopAttrib();
}


void drawGreenDiamondsInCorridor() {
    glPushMatrix();

    for (int i = 0; i < (int)greenPrisms.size(); ++i) {
        if (i < (int)greenPrismActive.size() && !greenPrismActive[i])
            continue;

        const auto& c = greenPrisms[i];

        float worldX = (c.x + 0.5f) * CELL;
        float worldZ = (c.z + 0.5f) * CELL;

        glPushMatrix();
        glTranslatef(worldX, 1.0f, worldZ);
        drawGreenDiamond();
        glPopMatrix();
    }

    glPopMatrix();
}


// -----------------------------------------------------------------------------
// Suelos oscuros
// -----------------------------------------------------------------------------
static inline void drawDarkFloorArea(float x0, float z0, float x1, float z1) {
    const float y = -0.001f; // evita z-fighting

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);

    GLfloat adf[4];
    const GLfloat noSpec[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    if (g_WorldStage >= 2 && g_WorldStage < 4) {
        // Mundo ya en fase roja/negra: suelo casi negro
        adf[0] = adf[1] = adf[2] = 0.02f;
        adf[3] = 1.0f;
    }
    else {
        // Suelo oscuro normal
        adf[0] = adf[1] = adf[2] = 0.10f;
        adf[3] = 1.0f;
    }

    glMaterialfv(GL_FRONT, GL_AMBIENT, adf);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, adf);
    glMaterialfv(GL_FRONT, GL_SPECULAR, noSpec);
    glMaterialf(GL_FRONT, GL_SHININESS, 0.0f);

    glBegin(GL_QUADS);
    glNormal3f(0.f, 1.f, 0.f);
    glVertex3f(x0, y, z0);
    glVertex3f(x1, y, z0);
    glVertex3f(x1, y, z1);
    glVertex3f(x0, y, z1);
    glEnd();
}


void drawEndRoomFloor() {
    const float centerX = ENTRANCE_CX();
    const float labEndZ = MAP_H * CELL;
    const float roomW = 4.0f * CELL;
    const float roomD = 3.0f * CELL;
    const float halfW = 0.5f * roomW;

    const float rx0 = centerX - halfW;
    const float rx1 = centerX + halfW;
    const float rz0 = labEndZ;
    const float rz1 = labEndZ + roomD;

    drawDarkFloorArea(rx0, rz0, rx1, rz1);
}

// -----------------------------------------------------------------------------
// Portal
// -----------------------------------------------------------------------------

void drawPortal(float x, float y, float z) {
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT);
    glPushMatrix();

    glTranslatef(x, y, z);

    float dx = camX - x;
    float dz = camZ - z;

    float yawToCam = atan2f(dx, dz) * 180.0f / M_PI;
    glRotatef(yawToCam, 0.0f, 1.0f, 0.0f);

    float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
    glRotatef(60.0f * t, 0.0f, 0.0f, 1.0f);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    const int   SEG = 72;
    const float innerR = 0.90f;
    const float outerR = 1.20f;

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.15f, 0.8f, 1.0f, 0.95f);
    glVertex3f(0.0f, 0.0f, 0.0f);

    for (int i = 0; i <= SEG; ++i) {
        float ang = (2.0f * (float)M_PI * i) / SEG;

        float r = innerR + 0.08f * sinf(3.0f * ang + 2.5f * t);

        float g = 0.6f + 0.3f * sinf(ang * 2.0f + t);
        float b = 1.0f;
        float a = 0.65f + 0.25f * sinf(ang * 4.0f + 1.5f * t);

        glColor4f(0.0f, g, b, a);
        glVertex3f(r * cosf(ang), r * sinf(ang), 0.0f);
    }
    glEnd();

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= SEG; ++i) {
        float ang = (2.0f * (float)M_PI * i) / SEG;

        float pulse = 0.5f + 0.5f * sinf(4.0f * t + 3.0f * ang);

        float r0 = innerR;
        float r1 = outerR + 0.05f * pulse;

        glColor4f(0.1f, 0.9f, 1.0f, 1.0f);
        glVertex3f(r1 * cosf(ang), r1 * sinf(ang), 0.0f);

        glColor4f(0.0f, 0.6f, 1.0f, 0.35f);
        glVertex3f(r0 * cosf(ang), r0 * sinf(ang), 0.0f);
    }
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(0.1f, 0.55f, 1.0f, 0.18f);
    GLUquadric* q = gluNewQuadric();
    gluSphere(q, outerR * 1.05f, 32, 16);
    gluDeleteQuadric(q);

    glDepthMask(GL_TRUE);
    glPopMatrix();
    glPopAttrib();
}

// -----------------------------------------------------------------------------
// Greedy merge de paredes
// -----------------------------------------------------------------------------

void greedyMerge() {
    wallRects.clear();

    bool used[MAP_H][MAP_W];
    std::memset(used, 0, sizeof(used));

    for (int z = 0; z < MAP_H; ++z) {
        for (int x = 0; x < MAP_W; ++x) {
            if (maze[z][x] != 1 || used[z][x]) continue;
            int w = 1;
            while (x + w < MAP_W && maze[z][x + w] == 1 && !used[z][x + w]) ++w;

            int  l = 1;
            bool expand = true;
            while (z + l < MAP_H && expand) {
                for (int i = 0; i < w; ++i) {
                    if (maze[z + l][x + i] != 1 || used[z + l][x + i]) {
                        expand = false;
                        break;
                    }
                }
                if (expand) ++l;
            }
            for (int dz = 0; dz < l; ++dz)
                for (int dx = 0; dx < w; ++dx)
                    used[z + dz][x + dx] = true;

            wallRects.push_back({ x, z, w, l });
        }
    }

    walls.clear();
    for (const auto& r : wallRects) {
        float x0 = r.x * CELL;
        float z0 = r.z * CELL;
        float x1 = (r.x + r.w) * CELL;
        float z1 = (r.z + r.l) * CELL;
        walls.push_back({ x0, 0.0f, z0, x1, wallH, z1 });
    }
}

// -----------------------------------------------------------------------------
// Muros biselados
// -----------------------------------------------------------------------------

void drawBeveledBox(float sx, float h, float sz, float bevel) {
    float bMax = 0.2f * std::fmin(sx, sz);
    float b = clampf(bevel, 0.0f, bMax);
    float x0 = 0, x1 = sx, z0 = 0, z1 = sz, y0 = 0, y1 = h;
    float xl = b, xr = sx - b, zf = b, zb = sz - b;

    glColor4f(1, 1, 1, 1);

    glBegin(GL_QUADS);
    // derecha
    {
        float u0 = 0, u1 = (zb - zf) * UV_SCALE, v0 = 0, v1 = (y1 - y0) * UV_SCALE;
        glNormal3f(1, 0, 0);
        glTexCoord2f(u0, v0); glVertex3f(xr, y0, zf);
        glTexCoord2f(u0, v1); glVertex3f(xr, y1, zf);
        glTexCoord2f(u1, v1); glVertex3f(xr, y1, zb);
        glTexCoord2f(u1, v0); glVertex3f(xr, y0, zb);
    }
    // izquierda
    {
        float u0 = 0, u1 = (zb - zf) * UV_SCALE, v0 = 0, v1 = (y1 - y0) * UV_SCALE;
        glNormal3f(-1, 0, 0);
        glTexCoord2f(u0, v0); glVertex3f(xl, y0, zb);
        glTexCoord2f(u0, v1); glVertex3f(xl, y1, zb);
        glTexCoord2f(u1, v1); glVertex3f(xl, y1, zf);
        glTexCoord2f(u1, v0); glVertex3f(xl, y0, zf);
    }
    // fondo
    {
        float u0 = 0, u1 = (xr - xl) * UV_SCALE, v0 = 0, v1 = (y1 - y0) * UV_SCALE;
        glNormal3f(0, 0, 1);
        glTexCoord2f(u0, v0); glVertex3f(xl, y0, zb);
        glTexCoord2f(u0, v1); glVertex3f(xl, y1, zb);
        glTexCoord2f(u1, v1); glVertex3f(xr, y1, zb);
        glTexCoord2f(u1, v0); glVertex3f(xr, y0, zb);
    }
    // frente
    {
        float u0 = 0, u1 = (xr - xl) * UV_SCALE, v0 = 0, v1 = (y1 - y0) * UV_SCALE;
        glNormal3f(0, 0, -1);
        glTexCoord2f(u0, v0); glVertex3f(xr, y0, zf);
        glTexCoord2f(u0, v1); glVertex3f(xr, y1, zf);
        glTexCoord2f(u1, v1); glVertex3f(xl, y1, zf);
        glTexCoord2f(u1, v0); glVertex3f(xl, y0, zf);
    }
    // techo
    {
        float u0 = 0, u1 = (xr - xl) * UV_SCALE, v0 = 0, v1 = (zb - zf) * UV_SCALE;
        glNormal3f(0, 1, 0);
        glTexCoord2f(u0, v0); glVertex3f(xl, y1, zf);
        glTexCoord2f(u0, v1); glVertex3f(xl, y1, zb);
        glTexCoord2f(u1, v1); glVertex3f(xr, y1, zb);
        glTexCoord2f(u1, v0); glVertex3f(xr, y1, zf);
    }
    // base
    {
        glNormal3f(0, -1, 0);
        glTexCoord2f(0, 0); glVertex3f(x0, y0, z0);
        glTexCoord2f(0, 1); glVertex3f(x0, y0, z1);
        glTexCoord2f(1, 1); glVertex3f(x1, y0, z1);
        glTexCoord2f(1, 0); glVertex3f(x1, y0, z0);
    }
    glEnd();

    const float diag = b * 1.41421356f;
    const float uDiag = diag * UV_SCALE;
    const float vY = (h)*UV_SCALE;

    glBegin(GL_QUADS);
    // biseles
    glNormal3f(0.707f, 0, 0.707f);
    glTexCoord2f(0, 0);   glVertex3f(xr, 0, zb);
    glTexCoord2f(0, vY);  glVertex3f(xr, h, zb);
    glTexCoord2f(uDiag, vY); glVertex3f(x1, h, z1);
    glTexCoord2f(uDiag, 0);  glVertex3f(x1, 0, z1);

    glNormal3f(0.707f, 0, -0.707f);
    glTexCoord2f(0, 0);   glVertex3f(xr, 0, zf);
    glTexCoord2f(0, vY);  glVertex3f(xr, h, zf);
    glTexCoord2f(uDiag, vY); glVertex3f(x1, h, 0);
    glTexCoord2f(uDiag, 0);  glVertex3f(x1, 0, 0);

    glNormal3f(-0.707f, 0, 0.707f);
    glTexCoord2f(0, 0);   glVertex3f(xl, 0, zb);
    glTexCoord2f(0, vY);  glVertex3f(xl, h, zb);
    glTexCoord2f(uDiag, vY); glVertex3f(0, h, z1);
    glTexCoord2f(uDiag, 0);  glVertex3f(0, 0, z1);

    glNormal3f(-0.707f, 0, -0.707f);
    glTexCoord2f(0, 0);   glVertex3f(xl, 0, zf);
    glTexCoord2f(0, vY);  glVertex3f(xl, h, zf);
    glTexCoord2f(uDiag, vY); glVertex3f(0, h, 0);
    glTexCoord2f(uDiag, 0);  glVertex3f(0, 0, 0);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glColor3f(0.18f, 0.18f, 0.22f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(xl, h, zf); glVertex3f(xl, h, zb);
    glVertex3f(xr, h, zb); glVertex3f(xr, h, zf);
    glEnd();
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
}

void drawAABB_AsBeveled(const AABB& b, float bevel) {
    glPushMatrix();
    glTranslatef(b.minx, 0.0f, b.minz);
    drawBeveledBox(b.maxx - b.minx, wallH, b.maxz - b.minz, bevel);
    glPopMatrix();
}

// -----------------------------------------------------------------------------
// Sky equirect
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Sky equirect con degradacion del mundo
// -----------------------------------------------------------------------------
void drawSkyEquirect(float radius) {
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    bool useTexture = (g_WorldStage <= 2 && gHasSkyTexture);
    if (useTexture)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    if (skyFlipV) {
        glScalef(1.0f, -1.0f, 1.0f);
        glTranslatef(0.0f, -1.0f, 0.0f);
    }
    glMatrixMode(GL_MODELVIEW);

    glPushMatrix();
    glTranslatef(camX, camY, camZ);

    glRotatef(skyYawDeg, 0, 1, 0);
    glRotatef(skyPitchDeg, 1, 0, 0);
    glRotatef(skyRollDeg, 0, 0, 1);

    // Color del cielo segun nivel
    if (g_WorldStage == 0 || g_WorldStage == 1) {
        // Normal (con textura)
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
    else if (g_WorldStage == 2) {
        // Cielo rojo inquietante
        glColor4f(0.75f, 0.05f, 0.05f, 1.0f);
    }
    else { // g_WorldStage >= 3
        // Sin textura, azul muy oscuro tipo noche
        glColor4f(0.02f, 0.04f, 0.12f, 1.0f);
    }

    if (useTexture)
        glBindTexture(GL_TEXTURE_2D, texSkyEquirect);
    else
        glBindTexture(GL_TEXTURE_2D, 0);

    const int slices = 64, stacks = 48;
    gluSphere(gQuadricSky, radius, slices, stacks);

    glPopMatrix();

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glBindTexture(GL_TEXTURE_2D, 0);
}


// -----------------------------------------------------------------------------
// Suelos foyer/pasillo
// -----------------------------------------------------------------------------

void buildFoyerAndCorridor() {
    extraWalls.clear();

    const float sx0 = START_CX() - 0.5f * START_W;
    const float sx1 = START_CX() + 0.5f * START_W;
    const float sz0 = START_CZ() - 0.5f * START_D;
    const float sz1 = START_CZ() + 0.5f * START_D;

    const float corridorHalf = 0.5f * CORRIDOR_W;
    const float cx0 = START_CX() - corridorHalf;
    const float cx1 = START_CX() + corridorHalf;
    const float cz0 = sz1;
    const float cz1 = 0.0f;

    const float eps = 0.05f;

    // cinco muros del foyer
    extraWalls.emplace_back(MakeAABB(sx0, 0.0f, sz1 - eps, cx0, wallH, sz1 + eps));
    extraWalls.emplace_back(MakeAABB(cx1, 0.0f, sz1 - eps, sx1, wallH, sz1 + eps));
    extraWalls.emplace_back(MakeAABB(sx0 - eps, 0.0f, sz0, sx0 + eps, wallH, sz1));
    extraWalls.emplace_back(MakeAABB(sx1 - eps, 0.0f, sz0, sx1 + eps, wallH, sz1));
    extraWalls.emplace_back(MakeAABB(sx0, 0.0f, sz0 - eps, sx1, wallH, sz0 + eps));

    // paredes del pasillo
    extraWalls.emplace_back(MakeAABB(cx0 - eps, 0.0f, cz0, cx0 + eps, wallH, cz1));
    extraWalls.emplace_back(MakeAABB(cx1 - eps, 0.0f, cz0, cx1 + eps, wallH, cz1));

    walls.insert(walls.end(), extraWalls.begin(), extraWalls.end());
}

void drawFoyerCorridorFloors() {
    const float sx0 = START_CX() - 0.5f * START_W;
    const float sx1 = START_CX() + 0.5f * START_W;
    const float sz0 = START_CZ() - 0.5f * START_D;
    const float sz1 = START_CZ() + 0.5f * START_D;

    const float corridorHalf = 0.5f * CORRIDOR_W;
    const float cx0 = START_CX() - corridorHalf;
    const float cx1 = START_CX() + corridorHalf;
    const float cz0 = sz1;
    const float cz1 = 0.0f;

    drawDarkFloorArea(sx0, sz0, sx1, sz1); // foyer
    drawDarkFloorArea(cx0, cz0, cx1, cz1); // pasillo
}

// -----------------------------------------------------------------------------
// Colisiones
// -----------------------------------------------------------------------------

bool collideXZ(float nx, float nz, float radius) {
    for (const auto& w : walls) {
        float cx = clampf(nx, w.minx, w.maxx);
        float cz = clampf(nz, w.minz, w.maxz);
        float dx = nx - cx, dz = nz - cz;
        if (dx * dx + dz * dz < radius * radius) return true;
    }
    return false;
}

bool collideY(float x, float y, float z, float radius, float height) {
    AABB p{ x - radius, y - 0.1f, z - radius,
            x + radius, y + height, z + radius };
    for (const auto& w : walls) {
        bool o = !(p.maxx <= w.minx || p.minx >= w.maxx ||
            p.maxy <= w.miny || p.miny >= w.maxy ||
            p.maxz <= w.minz || p.minz >= w.maxz);
        if (o) return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Cámara y mira
// -----------------------------------------------------------------------------

void setCamera() {
    float cosP = cosf(pitch), sinP = sinf(pitch);
    float cosY = cosf(yaw), sinY = sinf(yaw);
    float dirX = cosP * cosY;
    float dirY = sinP;
    float dirZ = cosP * sinY;

    glLoadIdentity();
    gluLookAt(camX, camY, camZ,
        camX + dirX, camY + dirY, camZ + dirZ,
        0, 1, 0);

    GLfloat lightPos[] = { 0.0f, 10.0f, 0.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
}

void drawCircleReticle(float radiusPx, int seg) {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    float cx = winW * 0.5f;
    float cy = winH * 0.5f;

    glLineWidth(2.0f);
    glColor3f(0.95f, 0.95f, 0.95f);

    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; ++i) {
        float t = i * (2.0f * (float)M_PI / seg);
        glVertex2f(cx + radiusPx * cosf(t),
            cy + radiusPx * sinf(t));
    }
    glEnd();

    glPointSize(4.0f);
    glBegin(GL_POINTS);
    glVertex2f(cx, cy);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

// -----------------------------------------------------------------------------
// HUD de vidas (corazones)
// -----------------------------------------------------------------------------

// corazón simple en espacio 2D
static void DrawHeart(float x, float y, float size)
{
    float s = size;
    float half = s * 0.5f;
    float quarter = s * 0.25f;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glColor3f(0.9f, 0.15f, 0.2f);

    glBegin(GL_QUADS);
    // Bloques superiores
    glVertex2f(x + quarter, y + s);
    glVertex2f(x + half, y + s);
    glVertex2f(x + half, y + s - quarter);
    glVertex2f(x + quarter, y + s - quarter);

    glVertex2f(x + half, y + s);
    glVertex2f(x + s - quarter, y + s);
    glVertex2f(x + s - quarter, y + s - quarter);
    glVertex2f(x + half, y + s - quarter);

    // Bloque central grande
    glVertex2f(x, y + s - quarter);
    glVertex2f(x + s, y + s - quarter);
    glVertex2f(x + s, y + quarter);
    glVertex2f(x, y + quarter);
    glEnd();

    // Triángulo inferior
    glBegin(GL_TRIANGLES);
    glVertex2f(x, y + quarter);
    glVertex2f(x + s, y + quarter);
    glVertex2f(x + half, y - quarter);
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
}

static void DrawLivesHUD()
{
    if (g_PlayerLives <= 0) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    float size = 32.0f;
    float margin = 10.0f;
    float gap = 8.0f;

    float y = winH - margin - size;

    for (int i = 0; i < g_PlayerLives; ++i)
    {
        float x = margin + i * (size + gap);
        DrawHeart(x, y, size);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
// NARRADOR HUD
static void DrawNarratorHUD()
{
    if (g_SrxFullLine.empty())
        return;

    int now = glutGet(GLUT_ELAPSED_TIME);
    if (g_SrxStartMs == 0)
        g_SrxStartMs = now;

    int   elapsedMs = now - g_SrxStartMs;
    float elapsed = elapsedMs / 1000.0f;

    const std::size_t totalChars = g_SrxFullLine.size();
    const float timeToType = totalChars / SRX_CHARS_PER_SEC;

    std::size_t visibleChars = 0;
    float alpha = 1.0f;

    if (elapsed <= timeToType)
    {
        visibleChars = static_cast<std::size_t>(elapsed * SRX_CHARS_PER_SEC);
        if (visibleChars > totalChars) visibleChars = totalChars;
        if (visibleChars == 0 && totalChars > 0) visibleChars = 1;
    }
    else
    {
        float t = elapsed - timeToType;

        if (t <= SRX_HOLD_SEC)
        {
            visibleChars = totalChars;
        }
        else if (t <= SRX_HOLD_SEC + SRX_FADE_SEC)
        {
            visibleChars = totalChars;
            float fadeT = (t - SRX_HOLD_SEC) / SRX_FADE_SEC;
            if (fadeT > 1.0f) fadeT = 1.0f;
            alpha = 1.0f - fadeT;
        }
        else
        {
            g_SrxFullLine.clear();
            g_SrxStartMs = 0;
            return;
        }
    }

    std::string toDraw = g_SrxFullLine.substr(0, visibleChars);
    if (toDraw.empty())
        return;

    // ---- Preparar proyección 2D ----
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const void* font = GLUT_BITMAP_TIMES_ROMAN_24;

    auto lineWidth = [&](const std::string& s) -> int {
        int w = 0;
        for (char c : s)
            w += glutBitmapWidth((void*)font, c);
        return w;
        };

    float maxWidth = winW * 0.80f;
    std::vector<std::string> lines;
    std::string current;
    std::string word;
    bool truncated = false;

    // --- Partir en palabras y envolver en max 2 líneas ---
    for (size_t i = 0; i <= toDraw.size(); ++i)
    {
        char c = (i < toDraw.size()) ? toDraw[i] : ' ';
        if (c == ' ' || c == '\n' || i == toDraw.size())
        {
            if (!word.empty())
            {
                std::string candidate = current.empty()
                    ? word
                    : current + " " + word;

                if ((float)lineWidth(candidate) <= maxWidth || current.empty())
                {
                    current = candidate;
                }
                else
                {
                    if (!current.empty())
                        lines.push_back(current);
                    current = word;

                    if (lines.size() >= 2)
                    {
                        truncated = true;
                        break;
                    }
                }
                word.clear();
            }

            if (c == '\n')
            {
                if (!current.empty())
                {
                    lines.push_back(current);
                    current.clear();
                    if (lines.size() >= 2)
                    {
                        truncated = true;
                        break;
                    }
                }
            }
        }
        else
        {
            word.push_back(c);
        }
    }

    if (!current.empty() && lines.size() < 2)
        lines.push_back(current);

    if (lines.size() > 2)
    {
        lines.resize(2);
        truncated = true;
    }

    if (truncated && !lines.empty())
        lines.back() += "...";

    if (lines.empty())
    {
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_LIGHTING);

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        return;
    }

    // ---- Posición: centrado horizontal, segunda linea MAS ABAJO que la primera ----
    float firstLineY = winH * 0.25f;  // altura de la PRIMERA linea medida desde abajo
    float lineHeight = 28.0f;

    glColor4f(0.9f, 0.9f, 0.9f, alpha);

    for (size_t i = 0; i < lines.size(); ++i)
    {
        const std::string& line = lines[i];
        int w = lineWidth(line);

        float x = (winW - w) * 0.5f;
        // i = 0 -> primera linea en firstLineY
        // i = 1 -> segunda linea MAS ABAJO (más cerca del borde inferior)
        float y = firstLineY - (float)i * lineHeight;

        glRasterPos2f(x, y);
        for (char c : line)
            glutBitmapCharacter((void*)font, c);
    }


    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

static void DrawScreenFade()
{
    if (g_TransitionState == TransitionState::NONE)
        return;

    float t = clampf(g_TransitionTime / TRANSITION_TOTAL, 0.0f, 1.0f);
    float alpha = 0.0f;

    if (g_TransitionState == TransitionState::FADING_OUT) {
        alpha = t;               // negro de 0 → 1
    }
    else if (g_TransitionState == TransitionState::FADING_IN) {
        alpha = 1.0f - t;        // negro de 1 → 0
    }

    if (alpha <= 0.0f)
        return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor4f(0.0f, 0.0f, 0.0f, alpha);

    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f((float)winW, 0.0f);
    glVertex2f((float)winW, (float)winH);
    glVertex2f(0.0f, (float)winH);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}


static void DrawPauseOverlay()
{
    if (!g_Paused)
        return;

    // Configurar proyección en 2D
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Nada de profundidad ni texturas
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    // =============================
    // 1) FONDO NEGRO SOLIDO
    // =============================
    glDisable(GL_BLEND); // sin transparencia
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(winW, 0);
    glVertex2f(winW, winH);
    glVertex2f(0, winH);
    glEnd();

    // =============================
    // 2) TEXTO "PAUSA" GIGANTE — BLANCO PURO
    // =============================
    const char* title = "PAUSA";
    void* font = GLUT_STROKE_ROMAN;

    float w = 0;
    for (const char* p = title; *p; ++p)
        w += glutStrokeWidth(font, *p);

    float scale = (winW * 0.7f) / w;
    float baseHeight = 119.05f;
    float titleHeight = baseHeight * scale;

    float centerX = winW * 0.5f;
    float centerY = winH * 0.60f;

    // Sombra (negra, opcional)
    glPushMatrix();
    glTranslatef(centerX - (w * scale) / 2 + 4.0f,
        centerY - titleHeight / 2 - 4.0f,
        0.0f);
    glScalef(scale, scale, 1.0f);
    glLineWidth(10.0f);
    glColor3f(0.0f, 0.0f, 0.0f);
    for (const char* p = title; *p; ++p)
        glutStrokeCharacter(font, *p);
    glPopMatrix();

    // Texto blanco puro
    glPushMatrix();
    glTranslatef(centerX - (w * scale) / 2,
        centerY - titleHeight / 2,
        0.0f);
    glScalef(scale, scale, 1.0f);
    glLineWidth(10.0f);
    glColor3f(1.0f, 1.0f, 1.0f);   // BLANCO
    for (const char* p = title; *p; ++p)
        glutStrokeCharacter(font, *p);
    glPopMatrix();

    // =============================
    // 3) TEXTO INFERIOR
    // =============================
    const char* hint = "Presiona ESC para volver";
    void* bfont = GLUT_BITMAP_HELVETICA_18;

    int hintW = 0;
    for (const char* p = hint; *p; ++p)
        hintW += glutBitmapWidth(bfont, *p);

    float hx = (winW - hintW) * 0.5f;
    float hy = centerY - titleHeight * 1.15f;

    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(hx, hy);
    for (const char* p = hint; *p; ++p)
        glutBitmapCharacter(bfont, *p);

    // Restaurar estado mínimo
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}




// -----------------------------------------------------------------------------
// Dibujo global del laberinto
// -----------------------------------------------------------------------------

void drawMaze() {
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glColor4f(1, 1, 1, 1);

    // Nivel 4: el mundo ya no existe, solo queda el portal en el vacio
    if (g_WorldStage >= 4)
    {
        const float centerX = ENTRANCE_CX();
        const float labEndZ = MAP_H * CELL;
        const float roomD = 3.0f * CELL;

        const float portalZ = labEndZ + 0.5f * roomD;
        const float portalY = 1.2f;

        drawPortal(centerX, portalY, portalZ);
        return;
    }

    // --- Suelos ---
    drawDarkFloorArea(0.0f, 0.0f, MAP_W * CELL, MAP_H * CELL);
    drawFoyerCorridorFloors();
    drawEndRoomFloor();

    // --- Muros ---
    const GLfloat noSpec[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    if (g_WorldStage == 0)
    {
        // Mundo normal: muros con textura
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texWall);
    }
    else if (g_WorldStage == 1)
    {
        glDisable(GL_TEXTURE_2D);

        GLfloat wallCol[4] = {
            0.10f, 0.10f, 0.10f, 1.0f
        };

        glMaterialfv(GL_FRONT, GL_AMBIENT, wallCol);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, wallCol);
        glMaterialfv(GL_FRONT, GL_SPECULAR, noSpec);
        glMaterialf(GL_FRONT, GL_SHININESS, 0.0f);
    }
    else
    {
        // A partir de nivel 2: muros casi negros
        glDisable(GL_TEXTURE_2D);

        GLfloat wallCol[4] = {
            0.03f, 0.03f, 0.03f, 1.0f
        };

        glMaterialfv(GL_FRONT, GL_AMBIENT, wallCol);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, wallCol);
        glMaterialfv(GL_FRONT, GL_SPECULAR, noSpec);
        glMaterialf(GL_FRONT, GL_SHININESS, 0.0f);
    }

    // Muros del laberinto
    for (auto& r : wallRects) {
        glPushMatrix();
        glTranslatef(r.x * CELL, 0.0f, r.z * CELL);
        drawBeveledBox(r.w * CELL, wallH, r.l * CELL, 0.12f);
        glPopMatrix();
    }

    // Muros extra
    for (auto& w : extraWalls)
        drawAABB_AsBeveled(w, 0.12f);
    for (auto& w : decorWalls)
        drawAABB_AsBeveled(w, 0.12f);

    // Prismas verdes (solo mientras existan muros/suelo)
    drawGreenDiamondsInCorridor();

    // Portal en la sala final
    {
        const float centerX = ENTRANCE_CX();
        const float labEndZ = MAP_H * CELL;
        const float roomD = 3.0f * CELL;

        const float portalZ = labEndZ + 0.5f * roomD;
        const float portalY = 1.2f;

        drawPortal(centerX, portalY, portalZ);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}


// -----------------------------------------------------------------------------
// Detección de prismas (para puzzles)
// -----------------------------------------------------------------------------

// devuelve índice del prisma tocado, o -1
int World_GetTouchedPrismIndex()
{
    const float triggerRadius = 1.2f;

    for (int i = 0; i < (int)greenPrisms.size(); ++i)
    {
        if (i < (int)greenPrismActive.size() && !greenPrismActive[i])
            continue;

        const auto& c = greenPrisms[i];

        float prismX = (c.x + 0.5f) * CELL;
        float prismZ = (c.z + 0.5f) * CELL;

        float dx = camX - prismX;
        float dz = camZ - prismZ;

        if (dx * dx + dz * dz <= triggerRadius * triggerRadius)
            return i;
    }
    return -1;
}



// Desactiva visual y lógicamente un prisma por índice (0..7)
void World_DisablePrism(int index)
{
    if (index < 0 || index >= (int)greenPrisms.size())
        return;

    if (index >= (int)greenPrismActive.size())
        greenPrismActive.resize(greenPrisms.size(), true);

    greenPrismActive[index] = false;
}

// Llamado por puzzles cuando el jugador falla una verificación
// Llamado por puzzles cuando el jugador falla una verificación.
// Devuelve el número de vidas restantes tras aplicar el castigo.
int World_OnPuzzleFailed()
{
    if (g_PlayerLives > 0)
        --g_PlayerLives;   // quitar un corazón

    // Bloquear sprint de forma permanente
    g_SprintBlocked = true;
    sprint = false;

    if (g_PlayerLives == 1) {
        g_InvertControls = true;
    }

    return g_PlayerLives;
}


// Llamado por puzzles cuando el jugador RESUELVE un puzzle
// Cambia el nivel de degradacion del mundo segun el indice del puzzle.
void World_OnPuzzleSolved(int puzzleIndex)
{
    int desiredStage = g_WorldStage;

    // Puzzle 3/8 (indice 2) -> muros grises, sin textura
    if (puzzleIndex == 2) {
        desiredStage = std::max(desiredStage, 1);
    }
    // Puzzle 5/8 (indice 4) -> cielo rojo, muros y suelo negros
    else if (puzzleIndex == 4) {
        desiredStage = std::max(desiredStage, 2);
    }
    // Puzzle 7/8 (indice 6, Dijkstra) -> cielo sin textura, azul oscuro
    else if (puzzleIndex == 6) {
        desiredStage = std::max(desiredStage, 3);
    }
    // Puzzle 8/8 (indice 7, LRU) -> solo portal en vacio negro
    else if (puzzleIndex == 7) {
        desiredStage = std::max(desiredStage, 4);
    }

    if (desiredStage != g_WorldStage)
    {
        g_WorldStage = desiredStage;

        // Al ultimo nivel: fondo completamente negro
        if (g_WorldStage == 4)
        {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}



// -----------------------------------------------------------------------------
// Init de OpenGL + mundo
// -----------------------------------------------------------------------------

void World_Init()
{
    // ----------------------------------------
    // OpenGL base
    // ----------------------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDepthMask(GL_TRUE);

    // Iluminación global
    GLfloat globalAmbient[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    GLfloat lightAmbient[] = { 0.9f, 0.9f, 0.9f, 1.0f };
    GLfloat lightDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat lightSpecular[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat lightPos[] = { 0.0f, 10.0f, 0.0f, 1.0f };

    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    GLfloat matAmbient[] = { 0.18f, 0.18f, 0.18f, 1.0f };
    GLfloat matDiffuse[] = { 0.22f, 0.22f, 0.22f, 1.0f };
    GLfloat matSpecular[] = { 0.0f,  0.0f,  0.0f,  1.0f };

    glMaterialfv(GL_FRONT, GL_AMBIENT, matAmbient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, matDiffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matSpecular);
    glMaterialf(GL_FRONT, GL_SHININESS, 0.0f);

    glClearColor(0.07f, 0.07f, 0.10f, 1.0f);

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // ----------------------------------------
    // Crear quadrics necesarios
    // ----------------------------------------
    if (!gQuadricSky) {
        gQuadricSky = gluNewQuadric();
        gluQuadricTexture(gQuadricSky, GL_TRUE);
        gluQuadricNormals(gQuadricSky, GLU_NONE);
    }

    if (!gQuadricSphere) {
        gQuadricSphere = gluNewQuadric();
        gluQuadricTexture(gQuadricSphere, GL_FALSE);
        gluQuadricNormals(gQuadricSphere, GLU_SMOOTH);
    }

    // Orientación del panorama equirectangular
    skyFlipV = true;
    skyYawDeg = 90.0f;
    skyPitchDeg = 270.0f;
    skyRollDeg = 90.0f;

    // ----------------------------------------
    // Cargar el nivel actual (EASY por defecto)
    // ----------------------------------------
    LoadLevelData();
    // Esto ya:
    // - copia mazeEasy → maze
    // - pone rombos rojos
    // - carga wall1 + panoramaEasy
    // - reconstruye paredes + foyer + endroom

    // ----------------------------------------
    // Spawn del jugador
    // ----------------------------------------
    PLAYER_SPAWN_X = START_CX();
    PLAYER_SPAWN_Z = START_CZ() + 0.25f * START_D;

    camX = PLAYER_SPAWN_X;
    camZ = PLAYER_SPAWN_Z;
    camY = PLAYER_Y_EYE;

    yaw = 0.0f;
    pitch = 0.0f;
    velY = 0.0f;

    if (mouseCaptured)
        glutSetCursor(GLUT_CURSOR_NONE);

    // ----------------------------------------
// Líneas SRX según dificultad
// ----------------------------------------
    SetupSrxWelcomeForCurrentLevel();

}





// -----------------------------------------------------------------------------
// Resize desde GLUT
// -----------------------------------------------------------------------------

void World_OnResize(int w, int h)
{
    winW = w;
    winH = h;
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70.0, (double)w / (double)h, 0.05, 400.0);
    glMatrixMode(GL_MODELVIEW);
}

// -----------------------------------------------------------------------------
// Input: teclado y ratón (llamado desde main.cpp)
// -----------------------------------------------------------------------------

void World_OnKeyDown(unsigned char k, int, int)
{
    // Si un puzzle está abierto, ignoramos controles del jugador
    if (Puzzles_IsOpen())
        return;

    if (k == 27) {
        g_Paused = !g_Paused;
        return;
    }

    keys[k] = true;

    keys[k] = true;

    if (k == 'w' || k == 'W') {
        int now = glutGet(GLUT_ELAPSED_TIME);
        if (!wIsDown) {
            if (!g_SprintBlocked && (now - lastWTapMs <= SPRINT_DOUBLE_TAP_MS))
                sprint = true;
            lastWTapMs = now;
            wIsDown = true;
        }
    }


    if (k == ' ' && onGround) {
        velY = jumpVel;
        onGround = false;
    }
}

void World_OnKeyUp(unsigned char k, int, int)
{
    if (Puzzles_IsOpen())
        return;

    keys[k] = false;
    if (k == 'w' || k == 'W') {
        wIsDown = false;
        sprint = false;
    }
}

void World_OnSpecialKey(int key, int, int)
{
    if (key == GLUT_KEY_F11) {
        if (!isFullscreen) {
            isFullscreen = true;
            glutFullScreen();
        }
        else {
            isFullscreen = false;
            int screenW = glutGet(GLUT_SCREEN_WIDTH);
            int screenH = glutGet(GLUT_SCREEN_HEIGHT);
            glutReshapeWindow(winW, winH);
            glutPositionWindow((screenW - winW) / 2, (screenH - winH) / 2);
        }
    }
    else if (key == GLUT_KEY_F1) {
        g_CurrentLevel = LevelDifficulty::EASY;
        LoadLevelData();
    }
    else if (key == GLUT_KEY_F3) {
        g_CurrentLevel = LevelDifficulty::HARD;
        LoadLevelData();
    }
    else if (key == GLUT_KEY_F2) {
        g_CurrentLevel = LevelDifficulty::MEDIUM;
        LoadLevelData();

        // En nivel medio no queremos texto del narrador SRX
        World_SetNarratorLine("", 0);
    }
}


void World_OnMouseButton(int b, int s, int x, int y)
{
    (void)x; (void)y;

    // Si un puzzle está abierto, no capturamos ratón
    if (Puzzles_IsOpen())
        return;

    if (b == GLUT_LEFT_BUTTON && s == GLUT_DOWN && !mouseCaptured) {
        mouseCaptured = true;
        glutSetCursor(GLUT_CURSOR_NONE);
        glutWarpPointer(winW / 2, winH / 2);
    }
}

void World_OnMouseMotion(int x, int y)
{
    if (!mouseCaptured) return;
    if (Puzzles_IsOpen()) return;
    if (g_Paused) return;

    int cx = winW / 2;
    int cy = winH / 2;

    int dx = x - cx;
    int dy = y - cy;
    if (dx == 0 && dy == 0) return;

    yaw += dx * mouseSens;
    pitch -= dy * mouseSens;

    const float maxP = (float)(M_PI / 2.0 - 0.01);
    if (pitch > maxP) pitch = maxP;
    if (pitch < -maxP) pitch = -maxP;

    if (yaw > M_PI) yaw -= (float)(2 * M_PI);
    if (yaw < -M_PI) yaw += (float)(2 * M_PI);

    glutWarpPointer(cx, cy);
    glutPostRedisplay();
}

// -----------------------------------------------------------------------------
// Update del mundo (llamado desde timer en main.cpp)
// -----------------------------------------------------------------------------

void World_Update(int ms)
{
    // --- manejar transición global (fade + cambio de nivel) ---
    if (g_TransitionState != TransitionState::NONE)
    {
        float dt = ms / 1000.0f;
        g_TransitionTime += dt;

        if (g_TransitionState == TransitionState::FADING_OUT) {
            if (g_TransitionTime >= TRANSITION_TOTAL) {
                // Cambiamos al nivel objetivo (MEDIUM o HARD)
                g_CurrentLevel = g_TransitionTargetLevel;
                LoadLevelData();

                // Respawn al inicio del laberinto correspondiente
                PLAYER_SPAWN_X = START_CX();
                PLAYER_SPAWN_Z = START_CZ() + 0.25f * START_D;

                camX = PLAYER_SPAWN_X;
                camZ = PLAYER_SPAWN_Z;
                camY = PLAYER_Y_EYE;

                yaw = 0.0f;
                pitch = 0.0f;
                velY = 0.0f;
                onGround = true;

                // limpiar input
                std::memset(keys, 0, sizeof(keys));
                sprint = false;
                wIsDown = false;

                // Volvemos a configurar la línea de SRX según el nuevo nivel
                SetupSrxWelcomeForCurrentLevel();

                // Pasamos a FADING_IN
                g_TransitionState = TransitionState::FADING_IN;
                g_TransitionTime = 0.0f;
            }
        }


        else if (g_TransitionState == TransitionState::FADING_IN) {
            if (g_TransitionTime >= TRANSITION_TOTAL) {
                g_TransitionState = TransitionState::NONE;
                g_TransitionTime = 0.0f;
            }
        }

        // Mientras hay transición, bloqueamos movimiento/jugador
        return;
    }
    // Si el puzzle está abierto, no integramos físicas ni movimiento
    if (Puzzles_IsOpen())
        return;
    if (g_Paused)
        return;
    float dt = ms / 1000.0f;

    if (!(keys['w'] || keys['W']))
        sprint = false;

    // El sprint solo tiene efecto si no está bloqueado
    float speed = baseSpeed * ((sprint && !g_SprintBlocked) ? SPRINT_MULT : 1.0f);

    float fwdX = cosf(yaw), fwdZ = sinf(yaw);
    float rightX = -sinf(yaw), rightZ = cosf(yaw);

    // Si g_InvertControls es true, movemos en la dirección opuesta
    int dir = g_InvertControls ? -1 : 1;

    float ax = 0.0f, az = 0.0f;
    if (keys['w'] || keys['W']) { ax += dir * fwdX;   az += dir * fwdZ; }
    if (keys['s'] || keys['S']) { ax -= dir * fwdX;   az -= dir * fwdZ; }
    if (keys['d'] || keys['D']) { ax += dir * rightX; az += dir * rightZ; }
    if (keys['a'] || keys['A']) { ax -= dir * rightX; az -= dir * rightZ; }

    float len = std::sqrt(ax * ax + az * az);
    if (len > 0.0001f) {
        ax /= len;
        az /= len;
    }

    float nx = camX + ax * speed * dt;
    float nz = camZ + az * speed * dt;

    float radius = 0.25f;
    float bodyH = 1.6f;

    if (!collideXZ(nx, camZ, radius)) camX = nx;
    if (!collideXZ(camX, nz, radius)) camZ = nz;

    // Física vertical
    velY -= gravity * dt;
    float ny = camY + velY * dt;

    const float eyeH = PLAYER_Y_EYE;
    if (ny < eyeH) {
        ny = eyeH;
        velY = 0.0f;
        onGround = true;
    }
    else {
        onGround = false;
    }

    if (!collideY(camX, ny - eyeH, camZ, radius, bodyH))
        camY = ny;

    // --- disparo del puzzle al entrar en un prisma ---
    static bool wasTouching = false;
    int  prismIndex = World_GetTouchedPrismIndex();
    bool touching = (prismIndex >= 0);

    if (touching && !wasTouching) {
        // Abrir puzzle asociado a ese prisma
        Puzzles_OpenForPrism(prismIndex);

        // soltar ratón
        mouseCaptured = false;
        glutSetCursor(GLUT_CURSOR_LEFT_ARROW);

        // limpiar entrada
        std::memset(keys, 0, sizeof(keys));
        sprint = false;
        wIsDown = false;
        velY = 0.0f;
    }

    wasTouching = touching;

    if (g_TransitionState == TransitionState::NONE)
    {
        const float centerX = ENTRANCE_CX();
        const float labEndZ = MAP_H * CELL;
        const float roomD = 3.0f * CELL;

        const float portalX = centerX;
        const float portalZ = labEndZ + 0.5f * roomD;
        const float triggerRadius = 1.0f;

        float dx = camX - portalX;
        float dz = camZ - portalZ;

        if (dx * dx + dz * dz <= triggerRadius * triggerRadius)
        {
            // Según en qué dificultad estés, decides a cuál saltar
            if (g_CurrentLevel == LevelDifficulty::EASY) {
                g_TransitionTargetLevel = LevelDifficulty::MEDIUM;
            }
            else if (g_CurrentLevel == LevelDifficulty::MEDIUM) {
                g_TransitionTargetLevel = LevelDifficulty::HARD; // depresión crónica
            }
            else {
                // En HARD ya no hacemos nada especial con el portal (por ahora)
                return;
            }

            g_TransitionState = TransitionState::FADING_OUT;
            g_TransitionTime = 0.0f;

            // Limpiar entrada / estados de movimiento
            std::memset(keys, 0, sizeof(keys));
            sprint = false;
            wIsDown = false;
            velY = 0.0f;
        }
    }
}

// -----------------------------------------------------------------------------
// Render del mundo (llamado desde display en main.cpp)
// -----------------------------------------------------------------------------

void World_Render()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    setCamera();

    if (g_WorldStage < 4 && gHasSkyTexture) {
        drawSkyEquirect(SKYDOME_RAD);
    }

    drawMaze();
    drawCircleReticle(8.0f, 48);

    DrawLivesHUD();
    DrawNarratorHUD();
    DrawPauseOverlay();

    DrawScreenFade();
}

