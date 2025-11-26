// puzzles.cpp
#include "imgui.h"
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <GL/glut.h>
#include <random> 


// ========================================================
//  Estructuras de datos básicas
// ========================================================

struct Block
{
    int         id;        // índice dentro del vector blocks
    const char* label;     // texto del bloque
    bool        used;      // true si está en algún slot
};

struct Slot
{
    int id;                // identificador único del slot
    int expectedBlockId;   // índice de bloque correcto
    int currentBlockId;    // -1 si vacío, si no índice en blocks
};

struct Puzzle
{
    const char* title;
    const char* description;
    std::vector<Block>     blocks;
    std::vector<Slot>      slots;
};

// ========================================================
//  Estado global de puzzles
// ========================================================

static std::vector<Puzzle> g_Puzzles;
static int  g_ActivePuzzle = -1;
static bool g_IsOpen = false;
extern bool g_PrismIsRed;
// Estado para gestionar mensajes y cierre diferido
static bool g_WaitingAutoClose = false;
static int  g_AutoCloseStartMs = 0;
static bool g_PendingFailPopup = false;

// API de world.cpp
extern void World_DisablePrism(int index);
extern int  World_OnPuzzleFailed();   // devuelve vidas restantes
extern void World_OnPuzzleSolved(int puzzleIndex);
// Misma definición que en world.cpp
enum class LevelDifficulty {
    EASY,
    MEDIUM,
    HARD
};

extern LevelDifficulty g_CurrentLevel;


// RNG para frases del narrador SRX
static std::mt19937 g_SrxRngPuzzle{ std::random_device{}() };

extern void World_SetNarratorLine(const char* text, int durationMs);
extern const char* SrxGetSuccessLine();
extern const char* SrxGetFailureLineForPuzzle(int idx);
extern const char* SrxGetGiveUpLine();

static bool g_HasQueuedNarrator = false;
static std::string g_QueuedNarratorLine;




static bool g_LastCheckWasOk = false;
static bool g_HasCheckResult = false;

static std::string g_CurrentPopupInsult;


// ========================================================
//  Utilidades comunes
// ========================================================
static int SrxRandInt(int maxExclusive)
{
    std::uniform_int_distribution<int> dist(0, maxExclusive - 1);
    return dist(g_SrxRngPuzzle);
}
static const char* SrxGetFailureLineForPuzzle(int idx)
{
    switch (idx)
    {
    case 0: // Suma de pares
    {
        static const char* lines[] = {
            "¿Sumar pares? Tranquilo, entiendo que dos mas dos te sobrepasa."
        };
        return lines[SrxRandInt(1)];
    }
    case 1: // Búsqueda binaria
    {
        static const char* lines[] = {
            "Hasta un interruptor de luz tiene mas criterio que tu."
        };
        return lines[SrxRandInt(1)];
    }
    case 2: // Euclides / gcd
    {
        static const char* lines[] = {
            "Euclides murio hace siglos, pero tu implementacion lo habria matado otra vez.",
            "Tu logica hace que la aritmetica parezca un deporte extremo."
        };
        return lines[SrxRandInt(2)];
    }
    case 3: // Quicksort
    {
        static const char* lines[] = {
            "Quicksort se llama ‘quick’ por algo. Lo tuyo fue… lento.",
            "Clasificar era facil. Clasificarte a ti es mucho mas sencillo: deficiente."
        };
        return lines[SrxRandInt(2)];
    }
    case 4: // DFS
    {
        static const char* lines[] = {
            "No te preocupes, perderte parece ser tu unico talento.",
            "Un grafo tiene caminos... Tu elegiste tropezarte en cada uno."
        };
        return lines[SrxRandInt(2)];
    }
    case 5: // LIS
    {
        static const char* lines[] = {
            "Curioso: cada error tuyo sí forma una serie interminable. Ahora tengo algo personal con el que sigue... no lo arruines"
        };
        return lines[SrxRandInt(1)];
    }
    case 6: // Dijkstra (Puzzle 7) - banco especial
    {
        static const char* lines[] = {
            "Vaya… Dijkstra te aplasto tambien. Al menos no soy el unico mediocre que no pudo con el holandes errante.",
            "Tranquilo, este puzzle y yo tenemos historia. Tu solo anadiste otro capitulo de verguenza.",
            "Pense que yo era el unico con cuentas pendientes con este algoritmo, pero gracias por competir conmigo en mediocridad... Se nota que me superaste"
        };
        return lines[SrxRandInt(3)];
    }
    case 7: // LRU cache
    {
        static const char* lines[] = {
            "LRU: el menos recientemente usado. Como tus neuronas."
        };
        return lines[SrxRandInt(1)];
    }
    default:
        // Fallback genérico
        return "Asombroso. Has arruinado algo que venia arruinado por diseno.";
    }
}
static const char* SrxGetSuccessLine()
{
    static const char* lines[] = {
        "Correcto… supongo. Tampoco es que ahora seas alguien brillante.",
        "Mira tu. No tan mediocre como parecia… solo un poco.",
        "Correcto. El universo esta tan confundido como yo.",
        "Lo lograste. El azar a veces se aburre y te favorece."
    };
    return lines[SrxRandInt(4)];
}
static const char* SrxGetGiveUpLine()
{
    static const char* lines[] = {
        "Ni el puzzle te quiso. Y tu tampoco te quisiste.",
        "Rendirse: lo unico que ejecutas sin errores."
    };
    return lines[SrxRandInt(2)];
}


static void ResetPuzzleState(Puzzle& p)
{
    for (auto& s : p.slots)
        s.currentBlockId = -1;
    for (auto& b : p.blocks)
        b.used = false;
}

static bool AllSlotsCorrect(const Puzzle& p)
{
    for (const auto& s : p.slots)
        if (s.currentBlockId != s.expectedBlockId)
            return false;
    return true;
}

static Block* FindBlockById(Puzzle& p, int id)
{
    for (auto& b : p.blocks)
        if (b.id == id) return &b;
    return nullptr;
}

// Renderiza un slot como botón + destino de drag&drop
static void RenderSlotButton(Puzzle& p, int slotIndex, float width = 150.0f)
{
    Slot& slot = p.slots[slotIndex];

    std::string slotLabel;
    if (slot.currentBlockId == -1)
        slotLabel = "____";
    else
    {
        Block* b = FindBlockById(p, slot.currentBlockId);
        slotLabel = b ? b->label : "????";
    }

    ImGui::PushID(slot.id);
    ImGui::Button(slotLabel.c_str(), ImVec2(width, 0.0f));

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BLOCK_ID"))
        {
            int blockId = *(const int*)payload->Data;

            // Liberar bloque anterior, si lo había
            if (slot.currentBlockId != -1)
            {
                Block* oldBlock = FindBlockById(p, slot.currentBlockId);
                if (oldBlock) oldBlock->used = false;
            }

            slot.currentBlockId = blockId;
            Block* newBlock = FindBlockById(p, blockId);
            if (newBlock) newBlock->used = true;
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::PopID();
}

// Paleta de bloques no usados
static void RenderBlockPalette(Puzzle& p)
{
    ImGui::Text("Bloques disponibles (arrastra hacia los huecos):");
    ImGui::Spacing();

    for (auto& b : p.blocks)
    {
        if (b.used)
            continue;

        ImGui::PushID(b.id);
        ImGui::Button(b.label, ImVec2(260, 0));

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("BLOCK_ID", &b.id, sizeof(int));
            ImGui::Text("%s", b.label);
            ImGui::EndDragDropSource();
        }

        ImGui::PopID();
    }
}
static void InitEasyPuzzle0(Puzzle& p)
{
    p.title = "Puzzle Fácil 1/5 - ¿Qué es un algoritmo?";
    p.description = "Selecciona la definición correcta de algoritmo.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    // Correcta
    int correct = add("Un conjunto de pasos ordenados para resolver un problema.");
    // Incorrectas
    add("Un error de programación.");
    add("Un tipo de computadora.");
    add("Un archivo que se ejecuta solo.");

    p.slots.push_back({ 0, correct, -1 });

    ResetPuzzleState(p);
}

static void InitEasyPuzzle1(Puzzle& p)
{
    p.title = "Puzzle Fácil 2/5 - ¿Qué es una variable?";
    p.description = "Elige la descripción correcta de una variable.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    int correct = add("Un espacio en memoria donde se guarda un valor.");
    add("Un virus del sistema.");
    add("Un archivo de texto.");
    add("Un tipo de teclado.");

    p.slots.push_back({ 0, correct, -1 });

    ResetPuzzleState(p);
}

static void InitEasyPuzzle2(Puzzle& p)
{
    p.title = "Puzzle Fácil 3/5 - Ordenar tres números";
    p.description = "Completa el código para ordenar 3 números pequeños.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    int b_if1 = add("if (a > b) std::swap(a, b);");
    int b_if2 = add("if (b > c) std::swap(b, c);");
    int b_if3 = add("if (a > b) std::swap(a, b);");

    p.slots.push_back({ 0, b_if1, -1 });
    p.slots.push_back({ 1, b_if2, -1 });
    p.slots.push_back({ 2, b_if3, -1 });

    ResetPuzzleState(p);
}
static void InitEasyPuzzle3(Puzzle& p)
{
    p.title = "Puzzle Fácil 4/5 - Incremento";
    p.description = "Completa el código para incrementar un número.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    int correct = add("x = x + 1;");
    add("x = x - 1;");
    add("x = x * 0;");
    add("x = 10;");

    p.slots.push_back({ 0, correct, -1 });

    ResetPuzzleState(p);
}
static void InitEasyPuzzle4(Puzzle& p)
{
    p.title = "Puzzle Fácil 5/5 - ¿Es par?";
    p.description = "Completa el código para comprobar si un número es par.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    int correct = add("return (x % 2 == 0);");

    add("return true;");
    add("return false;");
    add("return x;");

    p.slots.push_back({ 0, correct, -1 });

    ResetPuzzleState(p);
}

// ========================================================
//  Puzzles MEDIUM (7 en total)
//  - mismo modo UI que EASY
//  - bucles, condiciones y funciones
// ========================================================

static void InitMediumPuzzle0(Puzzle& p)
{
    p.title = "Puzzle Medio 1/7 - Bucle simple";
    p.description =
        "Observa el siguiente codigo:\n"
        "int suma = 0;\n"
        "for (int i = 1; i <= 10; ++i)\n"
        "    suma += i;\n\n"
        "Selecciona la descripcion que mejor explica que hace el bucle.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    int correct = add("Suma los numeros del 1 al 10 y guarda el resultado en 'suma'.");
    add("Multiplica los numeros del 1 al 10 y guarda el resultado en 'suma'.");
    add("Cuenta cuantos numeros pares hay entre 1 y 10.");
    add("Resta los numeros del 10 al 1 y guarda el resultado en 'suma'.");

    p.slots.push_back({ 0, correct, -1 });

    ResetPuzzleState(p);
}

static void InitMediumPuzzle1(Puzzle& p)
{
    p.title = "Puzzle Medio 2/7 - Recorrer un vector";
    p.description =
        "Queremos recorrer un vector 'v' de tamano 'n' y mostrar sus elementos\n"
        "en orden de indice, desde 0 hasta n-1.\n\n"
        "Elige la cabecera correcta del bucle for.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    int correct = add("for (int i = 0; i < n; ++i)");
    add("for (int i = 1; i <= n; ++i)");
    add("for (int i = 0; i <= n; ++i)");
    add("for (int i = n - 1; i >= 0; --i)");

    p.slots.push_back({ 0, correct, -1 });

    ResetPuzzleState(p);
}

static void InitMediumPuzzle2(Puzzle& p)
{
    p.title = "Puzzle Medio 3/7 - Maximo de tres numeros";
    p.description =
        "Queremos completar la funcion max3 para devolver el mayor de tres enteros:\n\n"
        "int max3(int a, int b, int c) {\n"
        "    int m = a;\n"
        "    // linea 0\n"
        "    // linea 1\n"
        "    return m;\n"
        "}\n\n"
        "Coloca las dos lineas correctas en orden.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    int b_if1 = add("if (b > m) m = b;");
    int b_if2 = add("if (c > m) m = c;");
    int b_wrong1 = add("if (b < m) m = b;");
    int b_wrong2 = add("if (c < m) m = c;");

    (void)b_wrong1;
    (void)b_wrong2;

    p.slots.push_back({ 0, b_if1, -1 });
    p.slots.push_back({ 1, b_if2, -1 });

    ResetPuzzleState(p);
}

static void InitMediumPuzzle3(Puzzle& p)
{
    p.title = "Puzzle Medio 4/7 - Contar positivos";
    p.description =
        "Tenemos un vector v de enteros y queremos contar cuantos elementos son\n"
        "estrictamente positivos.\n\n"
        "Dentro del bucle:\n"
        "for (std::size_t i = 0; i < v.size(); ++i) {\n"
        "    // linea 0\n"
        "}\n\n"
        "Elige la condicion correcta para aumentar el contador.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    int correct = add("if (v[i] > 0) ++count;");
    add("if (v[i] >= 0) ++count;");
    add("if (v[i] < 0) ++count;");
    add("if (v[i] == 0) ++count;");

    p.slots.push_back({ 0, correct, -1 });

    ResetPuzzleState(p);
}

static void InitMediumPuzzle4(Puzzle& p)
{
    p.title = "Puzzle Medio 5/7 - Intercambio ordenado";
    p.description =
        "Queremos que los enteros a y b queden en orden ascendente.\n"
        "Si a es mayor que b, los intercambiamos; si no, los dejamos igual.\n\n"
        "Completa el bloque de codigo en el orden correcto.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    int l0 = add("if (a > b) {");
    int l1 = add("    std::swap(a, b);");
    int l2 = add("}");

    // Distractores
    add("if (a < b) {");
    add("std::swap(a, b);");

    p.slots.push_back({ 0, l0, -1 });
    p.slots.push_back({ 1, l1, -1 });
    p.slots.push_back({ 2, l2, -1 });

    ResetPuzzleState(p);
}

static void InitMediumPuzzle5(Puzzle& p)
{
    p.title = "Puzzle Medio 6/7 - Condicion booleana";
    p.description =
        "Queremos una expresion booleana que sea verdadera solo cuando x es un\n"
        "numero entero IMPAR y POSITIVO.\n\n"
        "Elige la condicion correcta.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    int correct = add("x > 0 && (x % 2 != 0)");
    add("x >= 0 && (x % 2 == 0)");
    add("x < 0 && (x % 2 != 0)");
    add("(x % 2 != 0)");

    p.slots.push_back({ 0, correct, -1 });

    ResetPuzzleState(p);
}

static void InitMediumPuzzle6(Puzzle& p)
{
    p.title = "Puzzle Medio 7/7 - Busqueda lineal";
    p.description =
        "Queremos completar una funcion que comprueba si un vector contiene\n"
        "un valor objetivo:\n\n"
        "bool contiene(const std::vector<int>& v, int objetivo) {\n"
        "    for (std::size_t i = 0; i < v.size(); ++i) {\n"
        "        // linea 0\n"
        "    }\n"
        "    return false;\n"
        "}\n\n"
        "Elige la linea correcta para que la funcion devuelva true cuando\n"
        "encuentra el objetivo.";

    p.blocks.clear();
    p.slots.clear();

    auto add = [&](const char* txt) {
        int id = (int)p.blocks.size();
        p.blocks.push_back({ id, txt, false });
        return id;
        };

    int correct = add("if (v[i] == objetivo) return true;");
    add("if (v[i] == objetivo) return false;");
    add("if (v[i] != objetivo) return true;");
    add("return true;");

    p.slots.push_back({ 0, correct, -1 });

    ResetPuzzleState(p);
}


// ========================================================
//  Construcción de Puzzles

// --------------------------------------------------------
// Puzzle 0: sumar elementos pares de un vector sin desbordar
// int sum_even(const std::vector<int>& v);
// --------------------------------------------------------
static void InitPuzzle0(Puzzle& p)
{
    p.title = "Puzzle 1/8 - Suma de pares segura";
    p.description =
        "Dado un vector de enteros, define una funcion que calcule la suma de "
        "todos los elementos pares sin leer posiciones que no existan.";

    p.blocks.clear();
    p.slots.clear();

    auto addBlock = [&](const char* txt) -> int
        {
            int id = (int)p.blocks.size();
            p.blocks.push_back({ id, txt, false });
            return id;
        };

    // Bloques correctos
    int b_init = addBlock("std::size_t i = 0");
    int b_cond = addBlock("i < v.size()");
    int b_inc = addBlock("++i");
    int b_body = addBlock("if (v[i] % 2 == 0) acc += v[i];");

    // Distractores
    addBlock("int i = v.size()");
    addBlock("i <= v.size()");
    addBlock("--i");
    addBlock("acc += v[i];");
    addBlock("if (v[i] % 2 != 0) acc += v[i];");

    // Slots
    p.slots.push_back({ 0, b_init, -1 });
    p.slots.push_back({ 1, b_cond, -1 });
    p.slots.push_back({ 2, b_inc,  -1 });
    p.slots.push_back({ 3, b_body, -1 });

    ResetPuzzleState(p);
}

// --------------------------------------------------------
// Puzzle 1: búsqueda binaria - primer elemento >= target
// int first_ge(const std::vector<int>& v, int target);
// --------------------------------------------------------
static void InitPuzzle1(Puzzle& p)
{
    p.title = "Puzzle 2/8 - Busqueda binaria";
    p.description =
        "Dispones de un vector de enteros ordenado y un valor objetivo. "
        "La funcion debe devolver el indice del primer elemento mayor o igual "
        "que ese valor, o -1 si no existe ninguno.";

    p.blocks.clear();
    p.slots.clear();

    auto addBlock = [&](const char* txt) -> int
        {
            int id = (int)p.blocks.size();
            p.blocks.push_back({ id, txt, false });
            return id;
        };

    int b_lo0 = addBlock("int lo = 0;");
    int b_hi0 = addBlock("int hi = (int)v.size();");
    int b_mid = addBlock("int mid = lo + (hi - lo) / 2;");
    int b_left = addBlock("if (v[mid] >= target) hi = mid;");
    int b_right = addBlock("else lo = mid + 1;");
    int b_ret = addBlock("return (lo < (int)v.size() && v[lo] >= target) ? lo : -1;");

    // Distractores
    addBlock("int hi = (int)v.size() - 1;");
    addBlock("if (v[mid] > target) hi = mid - 1;");
    addBlock("else lo = mid;");
    addBlock("return lo;");

    p.slots.push_back({ 0, b_lo0,   -1 });
    p.slots.push_back({ 1, b_hi0,   -1 });
    p.slots.push_back({ 2, b_mid,   -1 });
    p.slots.push_back({ 3, b_left,  -1 });
    p.slots.push_back({ 4, b_right, -1 });
    p.slots.push_back({ 5, b_ret,   -1 });

    ResetPuzzleState(p);
}

// --------------------------------------------------------
// Puzzle 2: máximo común divisor recursivo (Euclides)
// int gcd(int a, int b);
// --------------------------------------------------------
static void InitPuzzle2(Puzzle& p)
{
    p.title = "Puzzle 3/8 - Algoritmo de Euclides";
    p.description =
        "Construye una funcion que calcule el maximo comun divisor de dos "
        "enteros, manejando correctamente signos y casos limite.";

    p.blocks.clear();
    p.slots.clear();

    auto addBlock = [&](const char* txt) -> int
        {
            int id = (int)p.blocks.size();
            p.blocks.push_back({ id, txt, false });
            return id;
        };

    int b_abs_a = addBlock("a = std::abs(a);");
    int b_abs_b = addBlock("b = std::abs(b);");
    int b_base = addBlock("if (b == 0) return a;");
    int b_recurse = addBlock("return gcd(b, a % b);");

    // Distractores
    addBlock("if (a == 0) return b;");
    addBlock("return gcd(a % b, b);");
    addBlock("if (b == 1) return 1;");
    addBlock("return a * b;");

    p.slots.push_back({ 0, b_abs_a,   -1 });
    p.slots.push_back({ 1, b_abs_b,   -1 });
    p.slots.push_back({ 2, b_base,    -1 });
    p.slots.push_back({ 3, b_recurse, -1 });

    ResetPuzzleState(p);
}

// --------------------------------------------------------
// Puzzle 3: partición en quicksort (in-place)
// void partition(std::vector<int>& v, int lo, int hi);
// --------------------------------------------------------
static void InitPuzzle3(Puzzle& p)
{
    p.title = "Puzzle 4/8 - Particion de quicksort";
    p.description =
        "A partir de un segmento de un vector y de un pivote, define la "
        "operacion que reorganiza los elementos de forma que queden a un lado "
        "los menores o iguales al pivote y al otro lado el resto.";

    p.blocks.clear();
    p.slots.clear();

    auto addBlock = [&](const char* txt) -> int
        {
            int id = (int)p.blocks.size();
            p.blocks.push_back({ id, txt, false });
            return id;
        };

    int b_pivot_val = addBlock("int pivot = v[hi];");
    int b_i_init = addBlock("int i = lo - 1;");
    int b_for_init = addBlock("int j = lo;");
    int b_for_cond = addBlock("j < hi;");
    int b_for_inc = addBlock("++j;");
    int b_if_cmp = addBlock("if (v[j] <= pivot) { std::swap(v[++i], v[j]); }");
    int b_swap_end = addBlock("std::swap(v[i + 1], v[hi]);");

    // Distractores
    addBlock("if (v[j] >= pivot) { std::swap(v[++i], v[j]); }");
    addBlock("int i = lo;");
    addBlock("j <= hi;");
    addBlock("std::swap(v[i], v[hi]);");

    p.slots.push_back({ 0, b_pivot_val, -1 });
    p.slots.push_back({ 1, b_i_init,    -1 });
    p.slots.push_back({ 2, b_for_init,  -1 });
    p.slots.push_back({ 3, b_for_cond,  -1 });
    p.slots.push_back({ 4, b_for_inc,   -1 });
    p.slots.push_back({ 5, b_if_cmp,    -1 });
    p.slots.push_back({ 6, b_swap_end,  -1 });

    ResetPuzzleState(p);
}


// --------------------------------------------------------
// Puzzle 4: DFS en grafo para contar alcance
// int reachable(const std::vector<std::vector<int>>& g, int s);
// --------------------------------------------------------
static void InitPuzzle4(Puzzle& p)
{
    p.title = "Puzzle 5/8 - DFS iterativo";
    p.description =
        "En un grafo dirigido representado con listas de adyacencia, cuenta "
        "cuantos nodos pueden alcanzarse desde un nodo inicial dado.";

    p.blocks.clear();
    p.slots.clear();

    auto addBlock = [&](const char* txt) -> int
        {
            int id = (int)p.blocks.size();
            p.blocks.push_back({ id, txt, false });
            return id;
        };

    int b_stack_init = addBlock("std::vector<int> st{ s };");
    int b_visited = addBlock("std::vector<bool> vis(g.size(), false);");
    int b_count_init = addBlock("int count = 0;");
    int b_while_cond = addBlock("!st.empty()");
    int b_pop = addBlock("int u = st.back(); st.pop_back();");
    int b_skip_vis = addBlock("if (vis[u]) continue;");
    int b_mark = addBlock("vis[u] = true; ++count;");
    int b_push_adj = addBlock("for (int v : g[u]) if (!vis[v]) st.push_back(v);");

    // Distractores
    addBlock("std::queue<int> st;");
    addBlock("if (!vis[u]) continue;");
    addBlock("vis[u] = false;");
    addBlock("count--;");

    p.slots.push_back({ 0, b_stack_init, -1 });
    p.slots.push_back({ 1, b_visited,    -1 });
    p.slots.push_back({ 2, b_count_init, -1 });
    p.slots.push_back({ 3, b_while_cond, -1 });
    p.slots.push_back({ 4, b_pop,        -1 });
    p.slots.push_back({ 5, b_skip_vis,   -1 });
    p.slots.push_back({ 6, b_mark,       -1 });
    p.slots.push_back({ 7, b_push_adj,   -1 });

    ResetPuzzleState(p);
}


// --------------------------------------------------------
// Puzzle 5: LIS O(n log n) - actualización de tails
// int lis_length(const std::vector<int>& a);
// --------------------------------------------------------
static void InitPuzzle5(Puzzle& p)
{
    p.title = "Puzzle 6/8 - Subsecuencia creciente maxima (LIS)";
    p.description =
        "Dada una secuencia de enteros, determina la longitud de una "
        "subsecuencia estrictamente creciente lo mas larga posible.";

    p.blocks.clear();
    p.slots.clear();

    auto addBlock = [&](const char* txt) -> int
        {
            int id = (int)p.blocks.size();
            p.blocks.push_back({ id, txt, false });
            return id;
        };

    int b_tails_init = addBlock("std::vector<int> tails;");
    int b_for_x = addBlock("for (int x : a)");
    int b_lb = addBlock("auto it = std::lower_bound(tails.begin(), tails.end(), x);");
    int b_append = addBlock("if (it == tails.end()) tails.push_back(x);");
    int b_replace = addBlock("else *it = x;");

    // Distractores
    addBlock("auto it = std::upper_bound(tails.begin(), tails.end(), x);");
    addBlock("if (it != tails.end()) tails.push_back(x);");
    addBlock("*tails.begin() = x;");

    p.slots.push_back({ 0, b_tails_init, -1 });
    p.slots.push_back({ 1, b_for_x,      -1 });
    p.slots.push_back({ 2, b_lb,         -1 });
    p.slots.push_back({ 3, b_append,     -1 });
    p.slots.push_back({ 4, b_replace,    -1 });

    ResetPuzzleState(p);
}

// --------------------------------------------------------
// Puzzle 6: Dijkstra clásico con priority_queue
// std::vector<int> dijkstra(const Graph& g, int s);
// --------------------------------------------------------
static void InitPuzzle6(Puzzle& p)
{
    p.title = "Puzzle 7/8 - Dijkstra";
    p.description =
        "En un grafo ponderado sin pesos negativos, calcula la distancia "
        "minima desde un nodo origen hasta todos los demas nodos del grafo.";

    p.blocks.clear();
    p.slots.clear();

    auto addBlock = [&](const char* txt) -> int
        {
            int id = (int)p.blocks.size();
            p.blocks.push_back({ id, txt, false });
            return id;
        };

    int b_dist_init = addBlock("std::vector<int> dist(g.size(), INF); dist[s] = 0;");
    int b_pq_init = addBlock("using Node = std::pair<int,int>; std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;");
    int b_push_s = addBlock("pq.push({0, s});");
    int b_while = addBlock("while (!pq.empty())");
    int b_pop = addBlock("auto [d, u] = pq.top(); pq.pop();");
    int b_skip = addBlock("if (d != dist[u]) continue;");
    int b_relax = addBlock("for (auto [v, w] : g[u]) if (dist[v] > d + w) { dist[v] = d + w; pq.push({dist[v], v}); }");

    // Distractores
    addBlock("if (d == dist[u]) continue;");
    addBlock("for (auto [v, w] : g[u]) dist[v] = d + w;");
    addBlock("pq.push({d, u});");

    p.slots.push_back({ 0, b_dist_init, -1 });
    p.slots.push_back({ 1, b_pq_init,   -1 });
    p.slots.push_back({ 2, b_push_s,    -1 });
    p.slots.push_back({ 3, b_while,     -1 });
    p.slots.push_back({ 4, b_pop,       -1 });
    p.slots.push_back({ 5, b_skip,      -1 });
    p.slots.push_back({ 6, b_relax,     -1 });

    ResetPuzzleState(p);
}


// --------------------------------------------------------
// Puzzle 7: LRU cache mínima
//
// Plantilla conceptual:
//
// template<typename Key, typename Value>
// class LRUCache {
// public:
//     Value* get(const Key& k) {
//         auto it = map.find(k);
//         if (it == map.end()) return nullptr;
//         // mover nodo a front
//         ...
//     }
//
//     void put(const Key& k, const Value& v) {
//         auto it = map.find(k);
//         if (it != map.end()) { ... } else if (list.size() == capacity) { ... }
//         ...
//     }
// private:
//     std::size_t capacity;
//     std::list<std::pair<Key,Value>> list;
//     std::unordered_map<Key, typename std::list<std::pair<Key,Value>>::iterator> map;
// };
// --------------------------------------------------------
static void InitPuzzle7(Puzzle& p)
{
    p.title = "Puzzle 8/8 - Cache LRU";
    p.description =
        "Implementa el comportamiento basico de una cache LRU: al acceder a "
        "una clave se marca como la mas reciente, y cuando la capacidad se "
        "llena se expulsa la clave menos utilizada recientemente.";

    p.blocks.clear();
    p.slots.clear();

    auto addBlock = [&](const char* txt) -> int
        {
            int id = (int)p.blocks.size();
            p.blocks.push_back({ id, txt, false });
            return id;
        };

    int b_get_splice = addBlock("list.splice(list.begin(), list, it->second);");
    int b_get_ret = addBlock("return &it->second->second;");

    int b_put_found_move = addBlock("list.splice(list.begin(), list, it->second); it->second->second = v;");
    int b_put_full = addBlock("if (list.size() == capacity) { auto last = std::prev(list.end()); map.erase(last->first); list.pop_back(); }");
    int b_put_insert = addBlock("list.emplace_front(k, v); map[k] = list.begin();");

    // Distractores
    addBlock("list.push_back({k, v});");
    addBlock("map.erase(k);");
    addBlock("if (list.size() > capacity) list.clear();");
    addBlock("return nullptr;");

    p.slots.push_back({ 0, b_get_splice,      -1 });
    p.slots.push_back({ 1, b_get_ret,         -1 });
    p.slots.push_back({ 2, b_put_found_move,  -1 });
    p.slots.push_back({ 3, b_put_full,        -1 });
    p.slots.push_back({ 4, b_put_insert,      -1 });

    ResetPuzzleState(p);
}

// ========================================================
//  Inicialización global
// ========================================================

extern bool g_PrismIsRed;   // desde World.cpp (true en nivel fácil)

void Puzzles_Init(int numPrisms)
{
    g_Puzzles.clear();

    // g_PrismIsRed controla el "modo amable" (sin SRX / sin insultos).
    // Vamos a usar numPrisms para distinguir EASY (5), MEDIUM (7) y HARD (8).

    if (!g_PrismIsRed)
    {
        // MODO HARD → 8 puzzles existentes
        g_Puzzles.resize(8);
        InitPuzzle0(g_Puzzles[0]);
        InitPuzzle1(g_Puzzles[1]);
        InitPuzzle2(g_Puzzles[2]);
        InitPuzzle3(g_Puzzles[3]);
        InitPuzzle4(g_Puzzles[4]);
        InitPuzzle5(g_Puzzles[5]);
        InitPuzzle6(g_Puzzles[6]);
        InitPuzzle7(g_Puzzles[7]);
    }
    else
    {
        if (numPrisms <= 5)
        {
            // EASY → 5 puzzles muy sencillos
            g_Puzzles.resize(5);
            InitEasyPuzzle0(g_Puzzles[0]);
            InitEasyPuzzle1(g_Puzzles[1]);
            InitEasyPuzzle2(g_Puzzles[2]);
            InitEasyPuzzle3(g_Puzzles[3]);
            InitEasyPuzzle4(g_Puzzles[4]);
        }
        else if (numPrisms == 7)
        {
            // MEDIUM → 7 puzzles ligeramente más dificiles
            g_Puzzles.resize(7);
            InitMediumPuzzle0(g_Puzzles[0]);
            InitMediumPuzzle1(g_Puzzles[1]);
            InitMediumPuzzle2(g_Puzzles[2]);
            InitMediumPuzzle3(g_Puzzles[3]);
            InitMediumPuzzle4(g_Puzzles[4]);
            InitMediumPuzzle5(g_Puzzles[5]);
            InitMediumPuzzle6(g_Puzzles[6]);
        }
        else
        {
            // Fallback por si en el futuro cambias el número:
            // reutilizamos al menos los puzzles easy.
            g_Puzzles.resize(5);
            InitEasyPuzzle0(g_Puzzles[0]);
            InitEasyPuzzle1(g_Puzzles[1]);
            InitEasyPuzzle2(g_Puzzles[2]);
            InitEasyPuzzle3(g_Puzzles[3]);
            InitEasyPuzzle4(g_Puzzles[4]);
        }
    }

    g_ActivePuzzle = -1;
    g_IsOpen = false;
}



void Puzzles_OpenForPrism(int index)
{
    if (index < 0 || g_Puzzles.empty())
        return;

    // Si hay más prismas que puzzles (caso nivel medio),
    // reutilizamos puzzles de forma circular.
    int idx = index % (int)g_Puzzles.size();

    g_ActivePuzzle = idx;
    g_IsOpen = true;

    ResetPuzzleState(g_Puzzles[g_ActivePuzzle]);

    // Resetear estados de verificación/cierre
    g_WaitingAutoClose = false;
    g_PendingFailPopup = false;

    // Reiniciamos el estado de verificación
    g_LastCheckWasOk = false;
    g_HasCheckResult = false;
}


bool Puzzles_IsOpen()
{
    return g_IsOpen;
}

// ========================================================
//  Dibujo de cada puzzle (UI con ImGui)
// ========================================================

static void DrawPuzzleCode(int idx, Puzzle& p)
{
    // -----------------------------------------
    // MODO FÁCIL / MEDIO (prismas rojos)
    // -----------------------------------------
    if (g_PrismIsRed)
    {
        // -------------------------
        // NIVEL FÁCIL
        // -------------------------
        if (g_CurrentLevel == LevelDifficulty::EASY)
        {
            switch (idx)
            {
            case 0: // Easy 0 – ¿Qué es un algoritmo?
                ImGui::Text("Pregunta:");
                ImGui::TextWrapped("¿Qué es un algoritmo?");
                ImGui::Spacing();
                ImGui::Text("Elige la respuesta correcta arrastrando un bloque a este hueco:");
                ImGui::Spacing();
                ImGui::Text("Respuesta:");
                ImGui::SameLine();
                RenderSlotButton(p, 0, 400.0f);
                break;

            case 1: // Easy 1 – ¿Qué es una variable?
                ImGui::Text("Pregunta:");
                ImGui::TextWrapped("¿Qué es una variable?");
                ImGui::Spacing();
                ImGui::Text("Elige la respuesta correcta arrastrando un bloque a este hueco:");
                ImGui::Spacing();
                ImGui::Text("Respuesta:");
                ImGui::SameLine();
                RenderSlotButton(p, 0, 400.0f);
                break;

            case 2: // Easy 2 – ordenar 3 números
                ImGui::Text("void ordenar3(int& a, int& b, int& c) {");
                ImGui::Text("    // Completa los pasos para ordenar a, b, c de menor a mayor.");
                ImGui::Text("    ");
                RenderSlotButton(p, 0, 320.0f);
                ImGui::Text("    ");
                RenderSlotButton(p, 1, 320.0f);
                ImGui::Text("    ");
                RenderSlotButton(p, 2, 320.0f);
                ImGui::Text("}");
                break;

            case 3: // Easy 3 – incrementar
                ImGui::Text("int incrementar(int x) {");
                ImGui::Text("    ");
                RenderSlotButton(p, 0, 220.0f);
                ImGui::Text("    return x;");
                ImGui::Text("}");
                break;

            case 4: // Easy 4 – es par
                ImGui::Text("bool es_par(int x) {");
                ImGui::Text("    ");
                RenderSlotButton(p, 0, 260.0f);
                ImGui::Text("}");
                break;

            default:
                ImGui::Text("Puzzle facil no definido.");
                break;
            }

            return; // no caer en el modo hard
        }

        // -------------------------
        // NIVEL MEDIO
        // -------------------------
        if (g_CurrentLevel == LevelDifficulty::MEDIUM)
        {
            switch (idx)
            {
            case 0: // Medio 1 – bucle suma 1..10
                ImGui::Text("Codigo:");
                ImGui::Text("int suma = 0;");
                ImGui::Text("for (int i = 1; i <= 10; ++i)");
                ImGui::Text("    suma += i;");
                ImGui::Spacing();
                ImGui::Text("Descripcion correcta del bucle:");
                ImGui::Spacing();
                RenderSlotButton(p, 0, 600.0f);
                break;

            case 1: // Medio 2 – recorrer vector
                ImGui::Text("Queremos mostrar todos los elementos de 'v' de 0 a n-1.");
                ImGui::Spacing();
                ImGui::Text("Cabecera correcta del bucle for:");
                ImGui::Spacing();
                RenderSlotButton(p, 0, 420.0f);
                break;

            case 2: // Medio 3 – maximo de tres
                ImGui::Text("int max3(int a, int b, int c) {");
                ImGui::Text("    int m = a;");
                ImGui::Text("    ");
                RenderSlotButton(p, 0, 260.0f); // if (b > m) m = b;
                ImGui::Text("    ");
                RenderSlotButton(p, 1, 260.0f); // if (c > m) m = c;
                ImGui::Text("    return m;");
                ImGui::Text("}");
                break;

            case 3: // Medio 4 – contar positivos
                ImGui::Text("for (std::size_t i = 0; i < v.size(); ++i) {");
                ImGui::Text("    ");
                RenderSlotButton(p, 0, 320.0f); // condicion sobre v[i]
                ImGui::Text("}");
                break;

            case 4: // Medio 5 – intercambio ordenado
                ImGui::Text("void ordenar(int& a, int& b) {");
                ImGui::Text("    ");
                RenderSlotButton(p, 0, 200.0f); // if (a > b) {
                ImGui::Text("        ");
                RenderSlotButton(p, 1, 260.0f); // std::swap(a, b);
                ImGui::Text("    ");
                RenderSlotButton(p, 2, 60.0f);  // }
                ImGui::Text("}");
                break;

            case 5: // Medio 6 – condicion booleana impar y positivo
                ImGui::Text("Queremos una condicion que sea TRUE solo si x es impar y positivo.");
                ImGui::Spacing();
                ImGui::Text("Condicion:");
                ImGui::SameLine();
                RenderSlotButton(p, 0, 340.0f);
                break;

            case 6: // Medio 7 – busqueda lineal
                ImGui::Text("bool contiene(const std::vector<int>& v, int objetivo) {");
                ImGui::Text("    for (std::size_t i = 0; i < v.size(); ++i) {");
                ImGui::Text("        ");
                RenderSlotButton(p, 0, 360.0f); // if (v[i] == objetivo) return true;
                ImGui::Text("    }");
                ImGui::Text("    return false;");
                ImGui::Text("}");
                break;

            default:
                ImGui::Text("Puzzle medio no definido.");
                break;
            }

            return; // tampoco caer en modo hard
        }

        // Si por alguna razon estamos en otro estado con prismas rojos
        ImGui::Text("Puzzle (modo rojo) no definido para este nivel.");
        return;
    }

    // -----------------------------------------
    // MODO HARD (puzzles originales)
    // -----------------------------------------
    switch (idx)
    {
    case 0: // sum_even
        ImGui::Text("int sum_even(const std::vector<int>& v) {");
        ImGui::Text("    int acc = 0;");
        ImGui::Text("    for (");
        ImGui::SameLine();
        RenderSlotButton(p, 0);
        ImGui::SameLine();
        ImGui::Text(";");
        ImGui::SameLine();
        RenderSlotButton(p, 1);
        ImGui::SameLine();
        ImGui::Text(";");
        ImGui::SameLine();
        RenderSlotButton(p, 2);
        ImGui::Text(") {");
        ImGui::Text("        ");
        ImGui::SameLine();
        RenderSlotButton(p, 3, 400.0f);
        ImGui::Text("    }");
        ImGui::Text("    return acc;");
        ImGui::Text("}");
        break;

    case 1: // binary search
        ImGui::Text("int first_ge(const std::vector<int>& v, int target) {");
        ImGui::Text("    ");
        RenderSlotButton(p, 0, 280.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 1, 280.0f);
        ImGui::Text("    while (lo < hi) {");
        ImGui::Text("        ");
        RenderSlotButton(p, 2, 280.0f);
        ImGui::Text("        ");
        RenderSlotButton(p, 3, 360.0f);
        ImGui::Text("        ");
        RenderSlotButton(p, 4, 260.0f);
        ImGui::Text("    }");
        ImGui::Text("    ");
        RenderSlotButton(p, 5, 420.0f);
        ImGui::Text("}");
        break;

    case 2: // gcd
        ImGui::Text("#include <cmath>");
        ImGui::Text("int gcd(int a, int b) {");
        ImGui::Text("    ");
        RenderSlotButton(p, 0, 220.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 1, 220.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 2, 220.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 3, 220.0f);
        ImGui::Text("}");
        break;

    case 3: // quicksort partition
        ImGui::Text("int partition(std::vector<int>& v, int lo, int hi) {");
        ImGui::Text("    ");
        RenderSlotButton(p, 0, 220.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 1, 220.0f);
        ImGui::Text("    for (");
        ImGui::SameLine();
        RenderSlotButton(p, 2, 140.0f);
        ImGui::SameLine(); ImGui::Text(";");
        ImGui::SameLine(); RenderSlotButton(p, 3, 140.0f);
        ImGui::SameLine(); ImGui::Text(";");
        ImGui::SameLine(); RenderSlotButton(p, 4, 140.0f);
        ImGui::Text("    ) {");
        ImGui::Text("        ");
        RenderSlotButton(p, 5, 420.0f);
        ImGui::Text("    }");
        ImGui::Text("    ");
        RenderSlotButton(p, 6, 260.0f);
        ImGui::Text("    return i + 1;");
        ImGui::Text("}");
        break;

    case 4: // DFS
        ImGui::Text("int reachable(const std::vector<std::vector<int>>& g, int s) {");
        ImGui::Text("    ");
        RenderSlotButton(p, 0, 260.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 1, 280.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 2, 180.0f);
        ImGui::Text("    while (");
        ImGui::SameLine(); RenderSlotButton(p, 3, 140.0f); ImGui::Text(") {");
        ImGui::Text("        ");
        RenderSlotButton(p, 4, 260.0f);
        ImGui::Text("        ");
        RenderSlotButton(p, 5, 200.0f);
        ImGui::Text("        ");
        RenderSlotButton(p, 6, 220.0f);
        ImGui::Text("        ");
        RenderSlotButton(p, 7, 360.0f);
        ImGui::Text("    }");
        ImGui::Text("    return count;");
        ImGui::Text("}");
        break;

    case 5: // LIS
        ImGui::Text("int lis_length(const std::vector<int>& a) {");
        ImGui::Text("    ");
        RenderSlotButton(p, 0, 240.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 1, 220.0f);
        ImGui::Text("    {");
        ImGui::Text("        ");
        RenderSlotButton(p, 2, 360.0f);
        ImGui::Text("        ");
        RenderSlotButton(p, 3, 360.0f);
        ImGui::Text("        ");
        RenderSlotButton(p, 4, 180.0f);
        ImGui::Text("    }");
        ImGui::Text("    return (int)tails.size();");
        ImGui::Text("}");
        break;

    case 6: // Dijkstra
        ImGui::Text("std::vector<int> dijkstra(const Graph& g, int s) {");
        ImGui::Text("    const int INF = 1e9;");
        ImGui::Text("    ");
        RenderSlotButton(p, 0, 380.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 1, 520.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 2, 200.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 3, 160.0f);
        ImGui::Text("    {");
        ImGui::Text("        ");
        RenderSlotButton(p, 4, 320.0f);
        ImGui::Text("        ");
        RenderSlotButton(p, 5, 220.0f);
        ImGui::Text("        ");
        RenderSlotButton(p, 6, 520.0f);
        ImGui::Text("    }");
        ImGui::Text("    return dist;");
        ImGui::Text("}");
        break;

    case 7: // LRU cache
        ImGui::Text("// Fragmentos clave de una caché LRU");
        ImGui::Separator();
        ImGui::Text("Value* get(const Key& k) {");
        ImGui::Text("    auto it = map.find(k);");
        ImGui::Text("    if (it == map.end()) return nullptr;");
        ImGui::Text("    ");
        RenderSlotButton(p, 0, 360.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 1, 260.0f);
        ImGui::Text("}");
        ImGui::Spacing();
        ImGui::Text("void put(const Key& k, const Value& v) {");
        ImGui::Text("    auto it = map.find(k);");
        ImGui::Text("    if (it != map.end()) {");
        ImGui::Text("        ");
        RenderSlotButton(p, 2, 440.0f);
        ImGui::Text("        return;");
        ImGui::Text("    }");
        ImGui::Text("    ");
        RenderSlotButton(p, 3, 520.0f);
        ImGui::Text("    ");
        RenderSlotButton(p, 4, 360.0f);
        ImGui::Text("}");
        break;

    default:
        ImGui::Text("Puzzle no definido.");
        break;
    }
}


// ========================================================
//  API pública
// ========================================================

void Puzzles_DrawImGui()
{
    if (!g_IsOpen || g_ActivePuzzle < 0 || g_ActivePuzzle >= (int)g_Puzzles.size())
        return;

    Puzzle& p = g_Puzzles[g_ActivePuzzle];
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 winSize(900.0f, 560.0f);
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (!ImGui::Begin(p.title, nullptr, flags))
    {
        ImGui::End();
        return;
    }

    // -------------------------------------------------
    // Recuadro de descripción del problema
    // -------------------------------------------------
    ImGui::TextUnformatted("Descripción del problema:");
    ImGui::Spacing();

    ImGui::PushID(g_ActivePuzzle);
    ImGui::BeginChild(
        "Descripcion",
        ImVec2(0.0f, 90.0f),
        true,
        ImGuiWindowFlags_NoScrollbar
    );
    ImGui::TextWrapped("%s", p.description);
    ImGui::EndChild();
    ImGui::PopID();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // -------------------------------------------------
    // Código del puzzle
    // -------------------------------------------------
    DrawPuzzleCode(g_ActivePuzzle, p);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // -------------------------------------------------
    // Paleta de bloques
    // -------------------------------------------------
    RenderBlockPalette(p);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // =================================================
    //  MODO FÁCIL (prismas rojos)
    //  - SIN SRX
    //  - SIN VIDAS NI CASTIGOS
    // =================================================
    if (g_PrismIsRed)
    {
        // Botón "Verificar solución"
        if (ImGui::Button("Verificar solución"))
        {
            bool ok = AllSlotsCorrect(p);

            if (ok)
            {
                // Solo desactivamos el prisma y cerramos el puzzle.
                World_DisablePrism(g_ActivePuzzle);

                g_IsOpen = false;
                g_ActivePuzzle = -1;

                ImGui::End();
                return;
            }
            else
            {
                // Simplemente marcamos que estuvo mal para mostrar un mensaje.
                g_LastCheckWasOk = false;
                g_HasCheckResult = true;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Reiniciar puzzle"))
        {
            ResetPuzzleState(p);
            g_HasCheckResult = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Rendirte"))
        {
            // En modo fácil NO hay castigos ni SRX.
            // Solo cerramos el puzzle y dejamos el prisma tal cual.
            g_IsOpen = false;
            g_ActivePuzzle = -1;
            ImGui::End();
            return;
        }

        ImGui::Spacing();

        if (g_HasCheckResult)
        {
            if (!g_LastCheckWasOk)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                    "Respuesta incorrecta. Intenta de nuevo.");
            }
        }

        ImGui::End();
        return; // IMPORTANTÍSIMO: no ejecutar lógica de SRX / vidas
    }

    // =================================================
    //  MODO HARD (puzzles originales con SRX + castigos)
    // =================================================

    if (!g_WaitingAutoClose && !g_PendingFailPopup)
    {
        // Botón "Verificar solución"
        if (ImGui::Button("Verificar solución"))
        {
            bool ok = AllSlotsCorrect(p);

            if (ok)
            {
                // Éxito: desactivar el prisma correspondiente
                World_DisablePrism(g_ActivePuzzle);

                // Avisar al mundo para que avance la degradación visual
                World_OnPuzzleSolved(g_ActivePuzzle);

                // Preparar la frase de SRX, pero NO mostrarla aún.
                {
                    std::string msg;

                    if (g_ActivePuzzle == 5)
                    {
                        // Puzzle 6 (LIS, índice 5): línea especial antes de Dijkstra
                        msg = "[SRX]: Bien. Se acabo el calentamiento. Tengo algo personal con Dijkstra, asi que no arruines lo que viene.";
                    }
                    else if (g_ActivePuzzle == 7)
                    {
                        // Último puzzle (LRU, índice 7): cierre base
                        msg = "[SRX]: Bueno... supongo que se acabo.";
                    }
                    else
                    {
                        const char* raw = SrxGetSuccessLine();
                        msg = std::string("[SRX]: ") + raw;
                    }

                    // Comentarios extra según el cambio en el mundo
                    switch (g_ActivePuzzle)
                    {
                    case 2: // Puzzle 3/8
                        msg += "  Y por si tu percepcion es tan torpe como tu codigo: los muros empiezan a borrarse. Ni el escenario quiere seguir viendote.";
                        break;

                    case 4: // Puzzle 5/8
                        msg += "  Mira arriba: el cielo se puso rojo. El resto del mundo se apago para no compartir paleta contigo.";
                        break;

                    case 6: // Puzzle 7/8
                        msg += "  Dijkstra sobrevivio, el decorado no: el cielo perdio hasta la textura. Este lugar se esta desrenderizando mas rapido que tu autoestima.";
                        break;

                    case 7: // Puzzle 8/8
                        msg += "  Observa bien: ya no queda nada. Solo ese portal y tu capacidad infinita de tomar malas decisiones.";
                        break;

                    default:
                        break;
                    }

                    g_QueuedNarratorLine = msg;
                    g_HasQueuedNarrator = true;
                }

                // Mensaje y cierre diferido
                g_WaitingAutoClose = true;
                g_AutoCloseStartMs = glutGet(GLUT_ELAPSED_TIME);
            }
            else
            {
                // Fallo con castigos
                World_DisablePrism(g_ActivePuzzle);
                int lives = World_OnPuzzleFailed();   // vidas tras el castigo

                // Avanzar la degradación del mundo también al fallar
                if (g_ActivePuzzle == 2 || g_ActivePuzzle == 4 ||
                    g_ActivePuzzle == 6 || g_ActivePuzzle == 7)
                {
                    World_OnPuzzleSolved(g_ActivePuzzle);
                }

                {
                    const char* raw = SrxGetFailureLineForPuzzle(g_ActivePuzzle);
                    std::string msg = std::string("[SRX]: ") + raw;

                    if (lives == 2)
                        msg += "\n Una vida menos. No es como si la estuvieras usando.";
                    else if (lives == 1)
                        msg += "\n Por cierto, el mundo gira… o quiza solo tu incompetencia.";

                    switch (g_ActivePuzzle)
                    {
                    case 2:
                        msg += "  Y por si no notas nada con esos ojos de compilador ciego: "
                            "los muros empiezan a borrarse. Ni el propio laberinto quiere seguir viendote.";
                        break;
                    case 4:
                        msg += "  Fijate: el cielo se puso rojo y todo lo demas se apago. "
                            "Hasta el escenario prefiere desaparecer antes que seguir alojando tus errores.";
                        break;
                    case 6:
                        msg += "  Dijkstra te gano y de paso se llevo el decorado: "
                            "solo queda una noche vacia y tu historial de fallos.";
                        break;
                    case 7:
                        msg += "  Cada intento tuyo borra mas mundo. Al final solo quedara ese portal "
                            "preguntandose por que fuiste precisamente tu quien llego hasta aqui.";
                        break;
                    default:
                        break;
                    }

                    g_QueuedNarratorLine = msg;
                    g_HasQueuedNarrator = true;
                }

                g_PendingFailPopup = true;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Reiniciar puzzle"))
        {
            ResetPuzzleState(p);
        }

        ImGui::SameLine();
        if (ImGui::Button("Rendirte"))
        {
            World_DisablePrism(g_ActivePuzzle);
            int lives = World_OnPuzzleFailed();

            if (g_ActivePuzzle == 2 || g_ActivePuzzle == 4 ||
                g_ActivePuzzle == 6 || g_ActivePuzzle == 7)
            {
                World_OnPuzzleSolved(g_ActivePuzzle);
            }

            const char* raw = SrxGetGiveUpLine();
            std::string msg = std::string("[SRX]: ") + raw;

            if (lives == 2)
                msg += "\n Una vida menos. No es como si la estuvieras usando.";
            else if (lives == 1)
                msg += "\n El mundo gira… o quiza solo tu incompetencia.";

            switch (g_ActivePuzzle)
            {
            case 2:
                msg += "  Y de paso, los muros empiezan a borrarse. "
                    "Ni siquiera el entorno quiere seguir cargando contigo.";
                break;
            case 4:
                msg += "  El cielo se volvio rojo y todo lo demas se fue a negro. "
                    "Eso no es ambientacion, es el universo intentando desconectarte.";
                break;
            case 6:
                msg += "  El cielo perdio la textura y se quedo en una noche profunda. "
                    "Ideal para esconder tu rendimiento.";
                break;
            case 7:
                msg += "  Ya casi no queda nada: solo el portal y tu persistencia en fracasar con estilo.";
                break;
            default:
                break;
            }

            g_IsOpen = false;
            g_ActivePuzzle = -1;
            ImGui::End();

            World_SetNarratorLine(msg.c_str(), 7000);
            return;
        }
    }
    else if (g_WaitingAutoClose)
    {
        ImGui::TextColored(ImVec4(0.2f, 0.3f, 1.0f, 1.0f),
            "Correcto: la solución es coherente. Cerrando puzzle.");

        int now = glutGet(GLUT_ELAPSED_TIME);
        if (now - g_AutoCloseStartMs >= 2500)
        {
            g_IsOpen = false;
            g_ActivePuzzle = -1;
            g_WaitingAutoClose = false;

            ImGui::End();

            if (g_HasQueuedNarrator && !g_QueuedNarratorLine.empty())
            {
                World_SetNarratorLine(g_QueuedNarratorLine.c_str(), 7000);
                g_QueuedNarratorLine.clear();
                g_HasQueuedNarrator = false;
            }
            return;
        }
    }

    // Popup de fallo (solo modo hard)
    if (g_PendingFailPopup)
    {
        static const char* s_PopupInsults[] = {
            "Asombroso. Has logrado decepcionar incluso mis expectativas y eso que no tengo.",
            "Eres la demostración viviente de que la incompetencia también escala.",
            "Notable. Transformaste algo simple en un fracaso monumental.",
            "Impresionante. Ni esforzándote podrías haberlo hecho peor.",
            "Fascinante. La precisión con la que fallas roza lo artístico.",
            "Vaya. Dominas la incompetencia con la serenidad de un experto.",
            "Tu capacidad de equivocarte es lo único verdaderamente consistente en ti.",
            "Sorprendente: cada decisión tuya es un recordatorio de por qué existo.",
            "Tu mediocridad es tan estable que casi inspira confianza.",
            "Me pregunto si fue la falta de habilidad o la ausencia de lógica… Difícil distinguir.",
            "Eres feo como una piedra y tonto como un zapato",
            "Si fueras más incompetente, necesitarías un tutor para respirar.",
            "Tu desempeño es tan bajo que redefine el concepto de límite.",
            "Tu sufrimiento será… educativo.",
            "Tu incompetencia es tan constante que debería tener su propio número primo.",
            "¿Sabes qué es lo gracioso ? Tú no.",
            "Se supone que soy el bufón. ¿Cuál es tu excusa?"
        };

        const int insultCount =
            (int)(sizeof(s_PopupInsults) / sizeof(s_PopupInsults[0]));

        g_CurrentPopupInsult = s_PopupInsults[SrxRandInt(insultCount)];

        ImGui::OpenPopup("Puzzle incorrecto");
        g_PendingFailPopup = false;
    }

    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.5f)
    );

    if (ImGui::BeginPopupModal("Puzzle incorrecto", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s", g_CurrentPopupInsult.c_str());
        ImGui::Spacing();

        if (ImGui::Button("Aceptar"))
        {
            g_IsOpen = false;
            g_ActivePuzzle = -1;
            ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
            ImGui::End();

            if (g_HasQueuedNarrator && !g_QueuedNarratorLine.empty())
            {
                World_SetNarratorLine(g_QueuedNarratorLine.c_str(), 7000);
                g_QueuedNarratorLine.clear();
                g_HasQueuedNarrator = false;
            }
            return;
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}
