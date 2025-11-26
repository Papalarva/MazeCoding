// main.cpp
#include <GL/glut.h>
#include <GL/glu.h>

#include "imgui.h"
#include "imgui_impl_glut.h"
#include "imgui_impl_opengl2.h"

// ------------------- Mundo (world.cpp) -------------------
extern void World_Init();
extern void World_Update(int ms);
extern void World_Render();
extern void World_OnResize(int w, int h);
extern void World_OnKeyDown(unsigned char k, int x, int y);
extern void World_OnKeyUp(unsigned char k, int x, int y);
extern void World_OnSpecialKey(int key, int x, int y);
extern void World_OnMouseButton(int b, int s, int x, int y);
extern void World_OnMouseMotion(int x, int y);

// ------------------- Puzzles (puzzles.cpp) -------------------
extern void Puzzles_Init(int numPrisms);
extern void Puzzles_DrawImGui();
extern bool Puzzles_IsOpen();

// ---------------------------------------------------------
// Tamaño inicial ventana
// ---------------------------------------------------------
int winW = 1600, winH = 900;

// ---------------------------------------------------------
// display
// ---------------------------------------------------------
void display()
{
    if (!Puzzles_IsOpen())
    {
        // Escena 3D normal
        World_Render();
    }
    else
    {
        // Pantalla limpia y color sólido cuando hay puzzle
        glClearColor(0.02f, 0.02f, 0.05f, 1.0f); // el color que quieras
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // --------- ImGui por encima ----------
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGLUT_NewFrame();
    ImGui::NewFrame();

    // Dibuja puzzle (si hay alguno abierto)
    Puzzles_DrawImGui();

    ImGui::Render();
    glDisable(GL_DEPTH_TEST);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    glEnable(GL_DEPTH_TEST);

    glutSwapBuffers();
}


// ---------------------------------------------------------
// reshape
// ---------------------------------------------------------
void reshape(int w, int h)
{
    winW = w;
    winH = h;
    if (h == 0) h = 1;

    // Avisamos al mundo para que actualice su proyección / winW / winH internos
    World_OnResize(w, h);

    // Y también a ImGui
    ImGui_ImplGLUT_ReshapeFunc(w, h);
}

// ---------------------------------------------------------
// timer (llamado cada ~16 ms)
// ---------------------------------------------------------
void timer(int ms)
{
    // Integra físicas / movimiento del mundo
    World_Update(ms);

    glutPostRedisplay();
    glutTimerFunc(16, timer, 16);
}

// ---------------------------------------------------------
// Input: teclado y ratón
// ---------------------------------------------------------

void keyboardDown(unsigned char k, int x, int y)
{
    // Primero ImGui (por si quiere capturar teclas)
    ImGui_ImplGLUT_KeyboardFunc(k, x, y);

    // Luego la lógica del mundo (world.cpp ya consulta Puzzles_IsOpen())
    World_OnKeyDown(k, x, y);
}

void keyboardUp(unsigned char k, int x, int y)
{
    ImGui_ImplGLUT_KeyboardUpFunc(k, x, y);
    World_OnKeyUp(k, x, y);
}

void specialKeys(int key, int x, int y)
{
    // F11, etc.
    World_OnSpecialKey(key, x, y);
}

void mouse(int b, int s, int x, int y)
{
    // Primero alimentamos a ImGui con el evento de ratón normal
    ImGui_ImplGLUT_MouseFunc(b, s, x, y);

    // Interpretar la rueda del ratón como botones 3 y 4
    // (convención de muchas implementaciones de GLUT / freeglut)
    if (s == GLUT_DOWN)
    {
        ImGuiIO& io = ImGui::GetIO();

        if (b == 3)        // rueda hacia arriba
            io.MouseWheel += 1.0f;
        else if (b == 4)   // rueda hacia abajo
            io.MouseWheel -= 1.0f;
    }

    // Después, tu lógica de mundo/FPS
    World_OnMouseButton(b, s, x, y);
}


void motion(int x, int y)
{
    // Movimiento con botón pulsado (drag) → ImGui
    ImGui_ImplGLUT_MotionFunc(x, y);

    // El mundo solo rota cámara si el ratón está capturado y
    // *no* hay puzzle abierto (eso ya se comprueba dentro).
    World_OnMouseMotion(x, y);
}

void passiveMotion(int x, int y)
{
    // Si quieres que el mundo también use passive motion:
    World_OnMouseMotion(x, y);
}

// ---------------------------------------------------------
// main
// ---------------------------------------------------------
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Laberinto con puzzles");

    // Inicializa el mundo (OpenGL, texturas, laberinto, cámara…)
    World_Init();

    // --------- Inicializar ImGui ----------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGLUT_Init();
    ImGui_ImplOpenGL2_Init();
    // --------------------------------------

    // Supón que tienes 8 prismas (como en world.cpp)
    Puzzles_Init(8);

    // Callbacks GLUT
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKeys);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(passiveMotion);
    glutTimerFunc(16, timer, 16);

    glutMainLoop();
    return 0;
}
