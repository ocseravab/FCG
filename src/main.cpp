//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//    INF01047 Fundamentos de Computação Gráfica
//               Prof. Eduardo Gastal

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <set>
#include <map>
#include <unordered_map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <tiny_obj_loader.h>

#include <stb_image.h>

#include "utils.h"
#include "matrices.h"

#define M_PI 3.141592f

// Estrutura que representa um modelo geométrico carregado a partir de um arquivo ".obj".
struct ObjModel
{
    tinyobj::attrib_t                 attrib;
    std::vector<tinyobj::shape_t>     shapes;
    std::vector<tinyobj::material_t>  materials;

    // Este construtor lê o modelo de um arquivo utilizando a biblioteca tinyobjloader.
    // Veja: https://github.com/syoyo/tinyobjloader
    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true)
    {
        printf("Carregando objetos do arquivo \"%s\"...\n", filename);

        // Se basepath == NULL, então setamos basepath como o dirname do
        // filename, para que os arquivos MTL sejam corretamente carregados caso
        // estejam no mesmo diretório dos arquivos OBJ.
        std::string fullpath(filename);
        std::string dirname;
        if (basepath == NULL)
        {
            auto i = fullpath.find_last_of("/");
            if (i != std::string::npos)
            {
                dirname = fullpath.substr(0, i+1);
                basepath = dirname.c_str();
            }
        }

        std::string warn;
        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);

        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());

        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");

        for (size_t shape = 0; shape < shapes.size(); ++shape)
        {
            if (shapes[shape].name.empty())
            {
                fprintf(stderr,
                        "*********************************************\n"
                        "Erro: Objeto sem nome dentro do arquivo '%s'.\n"
                        "Veja https://www.inf.ufrgs.br/~eslgastal/fcg-faq-etc.html#Modelos-3D-no-formato-OBJ .\n"
                        "*********************************************\n",
                    filename);
                throw std::runtime_error("Objeto sem nome.");
            }
            printf("- Objeto '%s'\n", shapes[shape].name.c_str());
        }

        printf("OK.\n");
    }
};


// Declaração de funções utilizadas para pilha de matrizes de modelagem.
void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4& M);

// Declaração de várias funções utilizadas em main().  Essas estão definidas
// logo após a definição de main() neste arquivo.
void BuildTrianglesAndAddToVirtualScene(ObjModel*); // Constrói representação de um ObjModel como malha de triângulos para renderização
void ComputeNormals(ObjModel* model); // Computa normais de um ObjModel, caso não existam.
void LoadShadersFromFiles(); // Carrega os shaders de vértice e fragmento, criando um programa de GPU
GLuint LoadTextureImage(const char* filename); // Função que carrega imagens de textura
void DrawVirtualObject(const char* object_name); // Desenha um objeto armazenado em g_VirtualScene
void DrawDirectionIndicator(glm::vec4 position, glm::vec4 forward, float length, glm::mat4 view, glm::mat4 projection, bool is_player = false); // Desenha indicador de direção
void DrawCrosshair(GLFWwindow* window); // Desenha crosshair no centro da tela
void DrawHealthBar(GLFWwindow* window, glm::vec4 world_position, float health, float max_health, glm::mat4 view, glm::mat4 projection); // Desenha barra de vida acima do inimigo
void DrawHUD(GLFWwindow* window); // Desenha HUD com HP e munição do jogador
void CameraRaycast(glm::vec4 camera_position, glm::vec4 ray_direction); // Realiza raycast e verifica interseções
void PlayerRaycast(); // Realiza raycast a partir do centro do jogador na direção que ele está olhando
void EnemyToPlayerRaycast(size_t enemy_index); // Realiza raycast de um inimigo específico em direção ao jogador
void DrawRaycastLine(glm::vec4 start, glm::vec4 end, glm::mat4 view, glm::mat4 projection); // Desenha linha amarela para visualizar raycast
int SpawnWave(const std::vector<glm::vec4>& spawn_positions); // Spawna uma wave de monstros nas posições especificadas, retorna o ID da wave
bool IsWaveComplete(int wave_id); // Verifica se todos os monstros de uma wave estão mortos
void UpdateWaves(); // Atualiza o status de todas as waves
std::vector<int> GetActiveWaves(); // Retorna os IDs de todas as waves ativas
std::vector<int> GetCompleteWaves(); // Retorna os IDs de todas as waves completas
GLuint LoadShader_Vertex(const char* filename);   // Carrega um vertex shader
GLuint LoadShader_Fragment(const char* filename); // Carrega um fragment shader
void LoadShader(const char* filename, GLuint shader_id); // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); // Cria um programa de GPU
void PrintObjModelInfo(ObjModel*); // Função para debugging

// Declaração de funções auxiliares para renderizar texto dentro da janela
// OpenGL. Estas funções estão definidas no arquivo "textrendering.cpp".
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow* window);
float TextRendering_CharWidth(GLFWwindow* window);
void TextRendering_PrintString(GLFWwindow* window, const std::string &str, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrix(GLFWwindow* window, glm::mat4 M, float x, float y, float scale = 1.0f);
void TextRendering_PrintVector(GLFWwindow* window, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProduct(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductMoreDigits(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductDivW(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);

// Funções abaixo renderizam como texto na janela OpenGL algumas matrizes e
// outras informações do programa. Definidas após main().
void TextRendering_ShowModelViewProjection(GLFWwindow* window, glm::mat4 projection, glm::mat4 view, glm::mat4 model, glm::vec4 p_model);
void TextRendering_ShowProjection(GLFWwindow* window);
void TextRendering_ShowFramesPerSecond(GLFWwindow* window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ErrorCallback(int error, const char* description);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

// Definimos uma estrutura que armazenará dados necessários para renderizar
// cada objeto da cena virtual.
struct SceneObject
{
    std::string  name;        // Nome do objeto
    std::string  material_name;
    size_t       first_index; // Índice do primeiro vértice dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    size_t       num_indices; // Número de índices do objeto dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    GLenum       rendering_mode; // Modo de rasterização (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
    GLuint       vertex_array_object_id; // ID do VAO onde estão armazenados os atributos do modelo
    glm::vec3    bbox_min; // Axis-Aligned Bounding Box do objeto
    glm::vec3    bbox_max;
};

// Estrutura que representa o jogador
struct Player
{
    // Posição e orientação
    glm::vec4 position;           // Posição do jogador no mundo (x, y, z, 1.0)
    float rotation_y;            // Rotação em torno do eixo Y (em radianos) - direção que o jogador está olhando
    glm::vec4 forward_vector;    // Vetor direção para frente do jogador (normalizado)
    glm::vec4 right_vector;      // Vetor direção para direita do jogador (normalizado)
    glm::vec3 model_center; 	// centro do modelo em coordenadas de modelo

    // Estados de movimento
    enum MovementState {
        IDLE,
        WALKING,
        RUNNING
    };
    MovementState movement_state;

    // Velocidades
    float walk_speed;            // Velocidade de caminhada
    float run_speed;             // Velocidade de corrida
    float current_speed;         // Velocidade atual (calculada baseada no estado)

    // Controles de movimento
    bool moving_forward;         // Tecla para frente pressionada
    bool moving_backward;        // Tecla para trás pressionada
    bool moving_left;            // Tecla para esquerda pressionada
    bool moving_right;           // Tecla para direita pressionada
    bool is_running;             // Shift pressionado para correr

    // Status do jogador
    float health;                // Vida do jogador (0.0 a 100.0)
    float max_health;            // Vida máxima

    // Sistema de tiro
    int magazine_ammo;           // Munição atual no carregador
    int magazine_size;           // Tamanho do carregador (6 balas)
    float shoot_cooldown;        // Tempo restante do cooldown de tiro (em segundos)
    float shoot_cooldown_time;   // Tempo total do cooldown de tiro (em segundos)
    float reload_time;           // Tempo restante do reload (em segundos)
    float reload_time_total;     // Tempo total do reload (em segundos)
    bool is_reloading;           // Se está recarregando

    // Câmera em terceira pessoa
    float camera_distance;       // Distância da câmera ao jogador
    float camera_height;         // Altura da câmera em relação ao jogador
    float camera_angle_horizontal; // Ângulo horizontal da câmera (em radianos)
    float camera_angle_vertical;   // Ângulo vertical da câmera (em radianos)

    // Construtor
    Player()
        : position(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f))
        , rotation_y(0.0f)
        , forward_vector(glm::vec4(0.0f, 0.0f, -1.0f, 0.0f))
        , right_vector(glm::vec4(1.0f, 0.0f, 0.0f, 0.0f))
        , movement_state(IDLE)
        , walk_speed(2.0f)
        , run_speed(5.0f)
        , current_speed(0.0f)
        , moving_forward(false)
        , moving_backward(false)
        , moving_left(false)
        , moving_right(false)
        , max_health(100.0f)
        , health(100.0f)  // Initialize with literal value since max_health isn't initialized yet
        , magazine_size(6)
        , magazine_ammo(6)  // Initialize with literal value since magazine_size isn't initialized yet
        , shoot_cooldown(0.0f)
        , shoot_cooldown_time(0.5f)  // 0.5 segundos de cooldown entre tiros
        , reload_time(0.0f)
        , reload_time_total(2.0f)     // 2 segundos para recarregar
        , is_reloading(false)
        , camera_distance(3.0f)
        , camera_height(1.5f)
        , camera_angle_horizontal(0.0f)
        , camera_angle_vertical(0.3f)  // Câmera ligeiramente acima
    {
    }

    // Atualiza os vetores de direção baseado na rotação Y
    void UpdateDirectionVectors()
    {
        forward_vector = glm::vec4(
            sin(rotation_y),
            0.0f,
            -cos(rotation_y),
            0.0f
        );
        right_vector = glm::vec4(
            cos(rotation_y),
            0.0f,
            sin(rotation_y),
            0.0f
        );
    }

    // Atualiza o estado de movimento baseado nas teclas pressionadas
    void UpdateMovementState()
    {
        bool is_moving = moving_forward || moving_backward || moving_left || moving_right;

        if (is_moving)
        {
            movement_state = is_running ? RUNNING : WALKING;
            current_speed = is_running ? run_speed : walk_speed;
        }
        else
        {
            movement_state = IDLE;
            current_speed = 0.0f;
        }
    }

    // Calcula a posição da câmera em terceira pessoa
    glm::vec4 GetThirdPersonCameraPosition()
    {
        float x = position.x + camera_distance * cos(camera_angle_vertical) * sin(camera_angle_horizontal);
        float y = position.y + camera_height + camera_distance * sin(camera_angle_vertical);
        float z = position.z + camera_distance * cos(camera_angle_vertical) * cos(camera_angle_horizontal);
        return glm::vec4(x, y, z, 1.0f);
    }

    glm::vec4 GetCameraLookAt()
    {
        // Compensa o fato de que o modelo foi escalado com 0.3f
        float scale = 0.3f;

        // Calcula o ponto de referência do jogador (centro do modelo)
        // Olha diretamente para o centro do cowboy para mantê-lo centralizado
        glm::vec4 player_reference = glm::vec4(
            position.x - model_center.x * scale,
            position.y - model_center.y * scale + camera_height,
            position.z - model_center.z * scale,
            1.0f
        );

        // Retorna o centro do jogador para manter o cowboy centralizado
        // O crosshair ainda funciona porque é desenhado no centro da tela
        // e o raycast usa o vetor de visualização da câmera
        return player_reference;
    }

    // Atualiza a posição do jogador baseado no movimento e no tempo decorrido
    // O movimento é relativo à direção da câmera (terceira pessoa)
    void UpdatePosition(float delta_time)
    {
        // Atualiza o estado de movimento
        UpdateMovementState();

        // Calcula a direção da câmera (forward da câmera)
        glm::vec4 camera_forward = glm::vec4(
            GetCameraLookAt().x - GetThirdPersonCameraPosition().x,
            GetCameraLookAt().y - GetThirdPersonCameraPosition().y,
            GetCameraLookAt().z - GetThirdPersonCameraPosition().z,
            0.0f // ← ESSENCIAL!!!
        );

        camera_forward.y = 0.0f; // Mantém o movimento apenas no plano horizontal

        float camera_forward_length = sqrt(camera_forward.x * camera_forward.x + camera_forward.z * camera_forward.z);

        // Atualiza a rotação do jogador para sempre corresponder à direção forward da câmera
        if (camera_forward_length > 0.001f)
        {
            camera_forward.x /= camera_forward_length;
            camera_forward.z /= camera_forward_length;

            // A rotação do jogador sempre corresponde à direção forward da câmera
            rotation_y = atan2(camera_forward.x, -camera_forward.z);
        }

        // Calcula a direção direita da câmera (perpendicular ao forward no plano horizontal)
        glm::vec4 camera_right = crossproduct(camera_forward, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
        float camera_right_length = sqrt(camera_right.x * camera_right.x + camera_right.z * camera_right.z);
        if (camera_right_length > 0.001f)
        {
            camera_right.x /= camera_right_length;
            camera_right.z /= camera_right_length;
        }

        // Calcula o vetor de movimento baseado nas teclas pressionadas
        glm::vec4 movement_direction = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

        if (moving_forward)
            movement_direction = movement_direction + camera_forward;
        if (moving_backward)
            movement_direction = movement_direction - camera_forward;
        if (moving_right)
            movement_direction = movement_direction + camera_right;
        if (moving_left)
            movement_direction = movement_direction - camera_right;

        // Normaliza o vetor de movimento se não for zero
        float movement_length = sqrt(movement_direction.x * movement_direction.x + movement_direction.z * movement_direction.z);

        // Atualiza a posição apenas se estiver se movendo
        if (movement_length > 0.001f && current_speed > 0.0f)
        {
            movement_direction.x /= movement_length;
            movement_direction.z /= movement_length;

            float move_distance = current_speed * delta_time;
            position.x += movement_direction.x * move_distance;
            position.z += movement_direction.z * move_distance;
        }
    }
};

// Estrutura que representa um inimigo
struct Enemy
{
    // Posição e orientação
    glm::vec4 position;           // Posição atual do inimigo no mundo (x, y, z, 1.0)
    glm::vec4 spawn_position;     // Posição inicial de spawn do inimigo
    float rotation_y;            // Rotação em torno do eixo Y (em radianos) - direção que o inimigo está olhando
    glm::vec4 forward_vector;    // Vetor direção para frente do inimigo (normalizado)
    glm::vec4 right_vector;      // Vetor direção para direita do inimigo (normalizado)

    // Estados de movimento (monstros não podem correr, apenas caminhar, agachar ou ficar em pé)
    enum MovementState {
        IDLE,
        WALKING,
        CROUCHING,
        STANDING
    };
    MovementState movement_state;

    // Velocidades
    float walk_speed;            // Velocidade de caminhada
    float current_speed;         // Velocidade atual (calculada baseada no estado)

    // Status do inimigo
    float max_health;            // Vida máxima
    float health;                // Vida do inimigo (0.0 a 100.0)

    // Construtor
    Enemy(glm::vec4 spawn_pos, int wave = -1)
        : position(spawn_pos)
        , spawn_position(spawn_pos)
        , rotation_y(0.0f)
        , forward_vector(glm::vec4(0.0f, 0.0f, -1.0f, 0.0f))
        , right_vector(glm::vec4(1.0f, 0.0f, 0.0f, 0.0f))
        , movement_state(IDLE)
        , walk_speed(1.5f)
        , current_speed(0.0f)
        , max_health(100.0f)
        , health(max_health)
        , wave_id(wave)
    {
    }

    // Atualiza os vetores de direção baseado na rotação Y
    void UpdateDirectionVectors()
    {
        forward_vector = glm::vec4(
            sin(rotation_y),
            0.0f,
            -cos(rotation_y),
            0.0f
        );
        right_vector = glm::vec4(
            cos(rotation_y),
            0.0f,
            sin(rotation_y),
            0.0f
        );
    }

    // Atualiza o estado de movimento (inimigos ficam parados por enquanto)
    void UpdateMovementState()
    {
        // Inimigos ficam em estado IDLE por padrão
        movement_state = IDLE;
        current_speed = 0.0f;
    }

    // Atualiza a posição do inimigo (inimigos não se movem por enquanto)
    void UpdatePosition(float delta_time)
    {
        // Atualiza o estado de movimento
        UpdateMovementState();

        // Inimigos permanecem na posição de spawn
        // (sem movimento por enquanto)
    }

    // Aplica dano ao inimigo
    void TakeDamage(float damage)
    {
        if (health <= 0.0f)
            return; // Já está morto

        health -= damage;
        if (health < 0.0f)
            health = 0.0f;
    }

    // Verifica se o inimigo está morto
    bool IsDead() const
    {
        return health <= 0.0f;
    }

    // ID da wave à qual este inimigo pertence (-1 se não pertence a nenhuma wave)
    int wave_id;
};

// Estrutura que representa uma caixa ou barril no mundo
struct Box
{
    glm::vec4 position;    // Posição da caixa no mundo (x, y, z, 1.0)
    float rotation_y;      // Rotação em torno do eixo Y (em radianos)
    glm::vec3 scale;       // Escala da caixa (largura, altura, profundidade)

    Box(glm::vec4 pos, float rot_y = 0.0f, glm::vec3 scl = glm::vec3(0.5f, 0.5f, 0.5f))
        : position(pos), rotation_y(rot_y), scale(scl)
    {
    }
};

// Estrutura que representa uma wave de monstros
struct Wave
{
    int wave_id;                    // ID único da wave
    std::vector<size_t> enemy_indices; // Índices dos inimigos desta wave no vetor g_Enemies
    bool is_active;                  // Se a wave está ativa (ainda tem inimigos vivos)
    bool is_complete;                // Se todos os inimigos da wave foram derrotados

    Wave(int id) : wave_id(id), is_active(true), is_complete(false)
    {
    }

    // Verifica se todos os inimigos da wave estão mortos
    bool CheckCompletion(const std::vector<Enemy>& enemies)
    {
        if (is_complete)
            return true;

        // Verifica se todos os inimigos da wave estão mortos
        bool all_dead = true;
        for (size_t idx : enemy_indices)
        {
            if (idx < enemies.size() && !enemies[idx].IsDead())
            {
                all_dead = false;
                break;
            }
        }

        if (all_dead)
        {
            is_complete = true;
            is_active = false;
            return true;
        }

        return false;
    }
};

// # ------------------------------------------------------------------------------- #
// ##### -------------------------- VARIÁVEIS GLOBAIS -------------------------- #####
// # ------------------------------------------------------------------------------- #

// A cena virtual é uma lista de objetos nomeados, guardados em um dicionário (map).
// Veja dentro da função BuildTrianglesAndAddToVirtualScene() como que são incluídos objetos dentro da variável g_VirtualScene
// Veja na função main() como estes são acessados.

std::map<std::string, SceneObject> g_VirtualScene;
float g_CowboyMinY = 0.0f; // menor y do cowboy em coordenadas de modelo

// Pilha que guardará as matrizes de modelagem.
std::stack<glm::mat4>  g_MatrixStack;

// Razão de proporção da janela (largura/altura)
float g_ScreenRatio = 1.0f;

int g_windowWidth = 1280;
int g_windowHeight = 960;

// "g_LeftMouseButtonPressed = true" se o usuário está com o botão esquerdo do mouse
// pressionado no momento atual. Veja função MouseButtonCallback().
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false;
bool g_MiddleMouseButtonPressed = false;

// Variáveis que definem a câmera em coordenadas esféricas, controladas pelo usuário.
float g_CameraDistance = 3.5f; // Distância da câmera para a origem

// Instância do jogador
Player g_Player;

// Lista de inimigos
std::vector<Enemy> g_Enemies;

// Lista de waves de monstros
std::vector<Wave> g_Waves;
int g_NextWaveID = 0; // Contador para gerar IDs únicos de waves

// Lista de caixas/barrils no mundo
std::vector<Box> g_Boxes;

// Variáveis para visualização de raycast de inimigo
bool g_DrawEnemyRaycast = false;
glm::vec4 g_EnemyRaycastStart;
glm::vec4 g_EnemyRaycastEnd;
float g_EnemyRaycastTime = 0.0f; // Tempo quando o raycast foi realizado
const float g_EnemyRaycastDuration = 3.0f; // Duração em segundos que a linha fica visível

// Variável que controla o tipo de projeção utilizada: perspectiva ou ortográfica.
bool g_UsePerspectiveProjection = true;

// Variável que controla se o texto informativo será mostrado na tela.
bool g_ShowInfoText = true;

// Variáveis para controle de tempo (delta time)
float g_LastFrameTime = 0.0f;

// Variáveis globais que armazenam a última posição do cursor do mouse.
// Usado para calcular quanto que o mouse se movimentou entre dois instantes de tempo.
// Utilizadas no callback CursorPosCallback().
double g_LastCursorPosX, g_LastCursorPosY;

std::unordered_map<std::string, GLuint> g_textureID;

// Variáveis que definem um programa de GPU (shaders). Veja função LoadShadersFromFiles().
GLuint g_GpuProgramID = 0;
GLint g_model_uniform;
GLint g_view_uniform;
GLint g_projection_uniform;
GLint g_object_id_uniform;
GLint g_bbox_min_uniform;
GLint g_bbox_max_uniform;

// Número de texturas carregadas pela função LoadTextureImage()
GLuint g_NumLoadedTextures = 0;

// VAO e VBO para renderização de linhas (indicadores de direção)
GLuint g_LineVAO = 0;
GLuint g_LineVBO = 0;

// ======================================================
// CARREGA TODAS AS TEXTURAS CORRETAS DO COWBOY
// ======================================================
void LoadAllCowboyTextures(ObjModel& model)
{
    for (const auto &mat : model.materials)
    {
        if (!mat.diffuse_texname.empty())
        {
            std::string texname = mat.diffuse_texname;

            // Se já começa com "model/", remover!
            if (texname.rfind("model/", 0) == 0)
            {
                texname = texname.substr(6); // remove "model/"
            }

            std::string path = "../../data/model/" + texname;
            GLuint tex = LoadTextureImage(path.c_str());
            g_textureID[mat.name] = tex;

            printf("✔ MATERIAL '%s'  →  textura '%s'  →  ID %u\n",
                mat.name.c_str(), texname.c_str(), tex);
        }
        else
        {
            printf("⚠ MATERIAL '%s' NÃO TEM textura difusa!\n", mat.name.c_str());
        }
    }
}



int main(int argc, char* argv[])
{
    int success = glfwInit();

    if (!success)
    {
        fprintf(stderr, "ERROR: glfwInit() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    glfwSetErrorCallback(ErrorCallback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    GLFWwindow* window;
    window = glfwCreateWindow(g_windowWidth, g_windowHeight, "Sunset Riders", NULL, NULL);

    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Centralizamos a janela na tela
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    int x = (mode->width - g_windowWidth) / 2;
    int y = (mode->height - g_windowHeight) / 2;
    glfwSetWindowPos(window, x, y);

    // Definimos a função de callback que será chamada sempre que o usuário
    // pressionar alguma tecla do teclado ...
    glfwSetKeyCallback(window, KeyCallback);
    // ... ou clicar os botões do mouse ...
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    // ... ou movimentar o cursor do mouse em cima da janela ...
    glfwSetCursorPosCallback(window, CursorPosCallback);
    // ... ou rolar a "rodinha" do mouse.
    glfwSetScrollCallback(window, ScrollCallback);

    // Indicamos que as chamadas OpenGL deverão renderizar nesta janela
    glfwMakeContextCurrent(window);

    // Carregamento de todas funções definidas por OpenGL 3.3, utilizando a
    // biblioteca GLAD.
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    // Configuramos o cursor para ficar desabilitado (escondido e travado na janela)
    // Isso permite movimento ilimitado do mouse para controle da câmera
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Inicializamos a posição do cursor no centro da janela
    glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);

    // Definimos a função de callback que será chamada sempre que a janela for
    // redimensionada, por consequência alterando o tamanho do "framebuffer"
    // (região de memória onde são armazenados os pixels da imagem).
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, g_windowWidth, g_windowHeight); // Forçamos a chamada do callback acima, para definir g_ScreenRatio.

    // Imprimimos no terminal informações sobre a GPU do sistema
    const GLubyte *vendor      = glGetString(GL_VENDOR);
    const GLubyte *renderer    = glGetString(GL_RENDERER);
    const GLubyte *glversion   = glGetString(GL_VERSION);
    const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion, glslversion);

    // Carregamos os shaders de vértices e de fragmentos que serão utilizados
    // para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
    //
    LoadShadersFromFiles();

    // Construímos a representação de objetos geométricos através de malhas de triângulos

    ObjModel planemodel("../../data/plane.obj");
    ComputeNormals(&planemodel);
    BuildTrianglesAndAddToVirtualScene(&planemodel);

    // Carregamos o modelo do jogador (cowboy)...
    ObjModel cowboymodel("../../data/cowboy.obj");
    ComputeNormals(&cowboymodel);
    BuildTrianglesAndAddToVirtualScene(&cowboymodel);
    LoadAllCowboyTextures(cowboymodel);

    const float player_scale = 0.3f;
    const float ground_y = -1.1f;
    g_CowboyMinY = std::numeric_limits<float>::max();

    for (const auto &obj : g_VirtualScene)
    {
        // Ignora chão e cubo pra focar no cowboy
        if (obj.first == "the_plane" || obj.first == "the_cube")
            continue;

        printf("OBJETO \"%s\" usa MATERIAL \"%s\"\n",
               obj.first.c_str(), obj.second.material_name.c_str());

        g_CowboyMinY = std::min(g_CowboyMinY, obj.second.bbox_min.y);
    }

    SceneObject cowboy_obj = g_VirtualScene["cowboy"];

    glm::vec3 g_CowboyCenterModel = (cowboy_obj.bbox_min + cowboy_obj.bbox_max) * 0.5f;

    g_Player.model_center = g_CowboyCenterModel;   // agora o player sabe o centro real do modelo!

    float center_x = (cowboy_obj.bbox_min.x + cowboy_obj.bbox_max.x) * 0.5f;
    float center_z = (cowboy_obj.bbox_min.z + cowboy_obj.bbox_max.z) * 0.5f;

    float player_y = ground_y - g_CowboyMinY * player_scale;

    g_Player.position = glm::vec4(
        -center_x * player_scale,
        player_y,
        -center_z * player_scale,
        1.0f
    );
    g_Player.UpdateDirectionVectors();

    printf(">>> Player pos = (%f, %f, %f)\n",
           g_Player.position.x, g_Player.position.y, g_Player.position.z);

    //... e o modelo dos inimigos (the_cube)...
    ObjModel enemymodel("../../data/cube.obj");
    ComputeNormals(&enemymodel);
    BuildTrianglesAndAddToVirtualScene(&enemymodel);

    // Inicializamos algumas caixas/barrils no mundo
    const float box_y = ground_y + 0.25f; // Caixas ficam meio acima do chão

    // Posições dos inimigos (para evitar colisão)
    // Inimigos spawnam a 5.0f de distância do jogador nas 4 direções cardinais
    // Vamos posicionar caixas em outras áreas, evitando essas posições
    float player_x = g_Player.position.x;
    float player_z = g_Player.position.z;
    float spawn_distance = 5.0f;
    float min_distance_from_spawn = 2.0f; // Distância mínima das posições de spawn dos inimigos

    // Adiciona várias caixas em posições que não colidem com spawns de inimigos
    // Posições diagonais e mais distantes
    g_Boxes.push_back(Box(glm::vec4(player_x + 7.0f, box_y, player_z + 7.0f, 1.0f), 0.0f, glm::vec3(0.4f, 0.4f, 0.4f)));
    g_Boxes.push_back(Box(glm::vec4(player_x - 7.0f, box_y, player_z + 7.0f, 1.0f), 0.5f, glm::vec3(0.5f, 0.5f, 0.5f)));
    g_Boxes.push_back(Box(glm::vec4(player_x + 7.0f, box_y, player_z - 7.0f, 1.0f), 1.0f, glm::vec3(0.3f, 0.6f, 0.3f))); // Barril mais alto
    g_Boxes.push_back(Box(glm::vec4(player_x - 7.0f, box_y, player_z - 7.0f, 1.0f), 1.5f, glm::vec3(0.4f, 0.4f, 0.4f)));

    // Posições laterais (evitando as 4 direções cardinais onde inimigos spawnam)
    g_Boxes.push_back(Box(glm::vec4(player_x + 8.0f, box_y, player_z + 2.0f, 1.0f), 0.0f, glm::vec3(0.5f, 0.5f, 0.5f)));
    g_Boxes.push_back(Box(glm::vec4(player_x - 8.0f, box_y, player_z - 2.0f, 1.0f), 0.0f, glm::vec3(0.4f, 0.4f, 0.4f)));
    g_Boxes.push_back(Box(glm::vec4(player_x + 2.0f, box_y, player_z + 8.0f, 1.0f), 0.0f, glm::vec3(0.3f, 0.6f, 0.3f))); // Barril mais alto
    g_Boxes.push_back(Box(glm::vec4(player_x - 2.0f, box_y, player_z - 8.0f, 1.0f), 0.0f, glm::vec3(0.5f, 0.5f, 0.5f)));

    // Mais algumas caixas em posições variadas
    g_Boxes.push_back(Box(glm::vec4(player_x + 6.0f, box_y, player_z + 3.0f, 1.0f), 0.8f, glm::vec3(0.4f, 0.4f, 0.4f)));
    g_Boxes.push_back(Box(glm::vec4(player_x - 6.0f, box_y, player_z - 3.0f, 1.0f), 1.2f, glm::vec3(0.3f, 0.6f, 0.3f))); // Barril mais alto
    g_Boxes.push_back(Box(glm::vec4(player_x + 3.0f, box_y, player_z + 6.0f, 1.0f), 0.3f, glm::vec3(0.4f, 0.4f, 0.4f)));
    g_Boxes.push_back(Box(glm::vec4(player_x - 3.0f, box_y, player_z - 6.0f, 1.0f), 0.7f, glm::vec3(0.5f, 0.5f, 0.5f)));

    // Inicializamos a câmera para começar olhando para o jogador
    g_Player.camera_angle_horizontal = 0.0f;
    g_Player.camera_angle_vertical = 0.3f;

    // Spawna uma wave de 4 inimigos ao redor do jogador
    glm::vec4 player_pos = g_Player.position;
    
    // Calcula a posição Y correta para os inimigos
    // O cubo vai de -1 a +1, e é escalado por 0.3, então vai de -0.3 a +0.3
    // Para que o fundo do inimigo fique no chão, precisamos posicionar em ground_y + 0.3
    const float enemy_scale = 0.3f;
    const float enemy_y = ground_y + enemy_scale; // Posiciona o inimigo para que o fundo fique no chão

    std::vector<glm::vec4> wave_spawn_positions = {
        glm::vec4(player_pos.x + spawn_distance, enemy_y, player_pos.z, 1.0f),           // Direita
        glm::vec4(player_pos.x - spawn_distance, enemy_y, player_pos.z, 1.0f),           // Esquerda
        glm::vec4(player_pos.x, enemy_y, player_pos.z + spawn_distance, 1.0f),           // Frente
        glm::vec4(player_pos.x, enemy_y, player_pos.z - spawn_distance, 1.0f)            // Atrás
    };
    SpawnWave(wave_spawn_positions);

    if ( argc > 1 )
    {
        ObjModel model(argv[1]);
        BuildTrianglesAndAddToVirtualScene(&model);
    }

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();

    // Habilitamos o Z-buffer. Veja slides 104-116 do documento Aula_09_Projecoes.pdf.
    glEnable(GL_DEPTH_TEST);

    // Habilitamos o Backface Culling. Veja slides 8-13 do documento Aula_02_Fundamentos_Matematicos.pdf, slides 23-34 do documento Aula_13_Clipping_and_Culling.pdf e slides 112-123 do documento Aula_14_Laboratorio_3_Revisao.pdf.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Inicializamos o tempo do último frame
    g_LastFrameTime = (float)glfwGetTime();

    // Ficamos em um loop infinito, renderizando, até que o usuário feche a janela
    while (!glfwWindowShouldClose(window))
    {
        // Calculamos o delta time (tempo decorrido desde o último frame)
        float current_time = (float)glfwGetTime();
        float delta_time = current_time - g_LastFrameTime;
        g_LastFrameTime = current_time;

        // Limita o delta time para evitar problemas com frames muito longos
        if (delta_time > 0.1f)
            delta_time = 0.1f;

        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como branco.
        //           R     G     B     A
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima, e também resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo os shaders de vértice e fragmentos).
        glUseProgram(g_GpuProgramID);

        // Atualizamos a posição do jogador baseado no movimento
        g_Player.UpdatePosition(delta_time);

        // Atualizamos os vetores de direção do jogador
        g_Player.UpdateDirectionVectors();

        // Atualizamos o cooldown de tiro
        if (g_Player.shoot_cooldown > 0.0f)
        {
            g_Player.shoot_cooldown -= delta_time;
            if (g_Player.shoot_cooldown < 0.0f)
                g_Player.shoot_cooldown = 0.0f;
        }

        // Atualizamos o tempo de reload
        if (g_Player.is_reloading)
        {
            g_Player.reload_time -= delta_time;
            if (g_Player.reload_time <= 0.0f)
            {
                // Recarregamento completo - recarrega o carregador
                g_Player.magazine_ammo = g_Player.magazine_size;
                g_Player.is_reloading = false;
                g_Player.reload_time = 0.0f;
            }
        }

        // Atualizamos todos os inimigos (apenas os vivos)
        for (auto& enemy : g_Enemies)
        {
            if (enemy.IsDead())
                continue;
            enemy.UpdatePosition(delta_time);
            enemy.UpdateDirectionVectors();
        }

        // Atualizamos o status das waves (verifica se estão completas)
        UpdateWaves();

        // Câmera em terceira pessoa seguindo o jogador
        // A câmera é posicionada usando o sistema de terceira pessoa do jogador
        glm::vec4 camera_position_c  = g_Player.GetThirdPersonCameraPosition(); // Posição da câmera em terceira pessoa
        glm::vec4 camera_lookat_l    = g_Player.GetCameraLookAt();
        glm::vec4 camera_view_vector = camera_lookat_l - camera_position_c; // Vetor "view", sentido para onde a câmera está virada
        glm::vec4 camera_up_vector   = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f); // Vetor "up" fixado para apontar para o "céu" (eito Y global)

        // Computamos a matriz "View" utilizando os parâmetros da câmera para
        // definir o sistema de coordenadas da câmera.  Veja slides 2-14, 184-190 e 236-242 do documento Aula_08_Sistemas_de_Coordenadas.pdf.
        glm::mat4 view = Matrix_Camera_View(camera_position_c, camera_view_vector, camera_up_vector);

        // Agora computamos a matriz de Projeção.
        glm::mat4 projection;

        // Veja slides 176-204 do documento Aula_09_Projecoes.pdf.
        float nearplane = -0.1f;
        float farplane  = -10.0f;

        if (g_UsePerspectiveProjection)
        {
            float field_of_view = 3.141592 / 3.0f;
            projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);
        }
        else
        {
            float t = 1.5f * g_CameraDistance / 2.5f;
            float b = -t;
            float r = t * g_ScreenRatio;
            float l = -r;
            projection = Matrix_Orthographic(l, r, b, t, nearplane, farplane);
        }

        glm::mat4 model = Matrix_Identity(); // Transformação identidade de modelagem

        // Enviamos as matrizes "view" e "projection" para a placa de vídeo
        glUniformMatrix4fv(g_view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));

        #define PLANE  0
        #define PLAYER 1
        #define ENEMY  2
        #define BOX    10

        // Desenhamos o plano do chão
        model = Matrix_Translate(0.0f,-1.1f,0.0f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, PLANE);
        DrawVirtualObject("the_plane");

        // Desenhamos as caixas/barrils
        for (const auto& box : g_Boxes)
        {
            model = Matrix_Translate(box.position.x, box.position.y, box.position.z);
            model = model * Matrix_Rotate_Y(box.rotation_y);
            model = model * Matrix_Scale(box.scale.x, box.scale.y, box.scale.z);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, BOX); // Usa shader específico para caixas (cor marrom)
            DrawVirtualObject("the_cube");
        }

        // Desenhamos o jogador
        // Primeiro transladamos para a posição do jogador, depois rotacionamos em torno do eixo Y
        model = Matrix_Translate(g_Player.position.x, g_Player.position.y, g_Player.position.z);
        model = model * Matrix_Rotate_Y(g_Player.rotation_y);
        // Escalamos um pouco para que o jogador seja visível
        model = model * Matrix_Scale(0.3f, 0.3f, 0.3f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, PLAYER);
        for (const auto& obj : g_VirtualScene)
        {
            // Ignorar os objetos anteriores (chão e cubo)
            if (obj.first == "the_plane" || obj.first == "the_cube")
                 continue;

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            DrawVirtualObject(obj.first.c_str());
        }

        // Desenhamos todos os inimigos (apenas os vivos)
        for (const auto& enemy : g_Enemies)
        {
            // Pula inimigos mortos - eles não devem ser renderizados
            if (enemy.IsDead())
                continue;

            // Ajusta a escala baseada no estado (agachado é menor)
            float scale_y = 0.3f;
            if (enemy.movement_state == Enemy::CROUCHING)
            {
                scale_y = 0.2f; // Inimigo agachado é mais baixo
            }
            else if (enemy.movement_state == Enemy::STANDING)
            {
                scale_y = 0.3f; // Inimigo em pé é normal
            }
            model = Matrix_Translate(enemy.position.x, enemy.position.y, enemy.position.z);
            model = model * Matrix_Rotate_Y(-enemy.rotation_y);
            // Escala diferente para Y quando agachado
            model = model * Matrix_Scale(0.3f, scale_y, 0.3f);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, ENEMY);
            DrawVirtualObject("the_cube");
        }

        // Desenhamos indicadores de direção para o jogador e inimigos
        // Indicador do jogador (verde)
        // DrawDirectionIndicator(g_Player.position, g_Player.forward_vector, 0.8f, view, projection, true);

        // Indicadores dos inimigos (vermelho) - apenas para inimigos vivos
        for (const auto& enemy : g_Enemies)
        {
            if (enemy.IsDead())
                continue;
            DrawDirectionIndicator(enemy.position, enemy.forward_vector, 0.8f, view, projection, false);
        }

        // Desenha linha amarela do raycast de inimigo se ativada e ainda dentro do tempo
        if (g_DrawEnemyRaycast)
        {
            float current_time = (float)glfwGetTime();
            float elapsed_time = current_time - g_EnemyRaycastTime;
            
            if (elapsed_time < g_EnemyRaycastDuration)
            {
                DrawRaycastLine(g_EnemyRaycastStart, g_EnemyRaycastEnd, view, projection);
            }
            else
            {
                // Desativa o desenho após 3 segundos
                g_DrawEnemyRaycast = false;
            }
        }

        // Desenhamos as barras de vida dos inimigos (apenas para inimigos vivos)
        for (const auto& enemy : g_Enemies)
        {
            if (enemy.IsDead())
                continue;
            DrawHealthBar(window, enemy.position, enemy.health, enemy.max_health, view, projection);
        }

        // Desenhamos o crosshair no centro da tela
        DrawCrosshair(window);

        // Desenhamos o HUD com HP e munição
        DrawHUD(window);

        // Imprimimos na informação sobre a matriz de projeção sendo utilizada.
        TextRendering_ShowProjection(window);

        // Imprimimos na tela informação sobre o número de quadros renderizados
        // por segundo (frames per second).
        TextRendering_ShowFramesPerSecond(window);

        // O framebuffer onde OpenGL executa as operações de renderização não
        // é o mesmo que está sendo mostrado para o usuário, caso contrário
        // seria possível ver artefatos conhecidos como "screen tearing". A
        // chamada abaixo faz a troca dos buffers, mostrando para o usuário
        // tudo que foi renderizado pelas funções acima.
        // Veja o link: https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics
        glfwSwapBuffers(window);

        // Verificamos com o sistema operacional se houve alguma interação do
        // usuário (teclado, mouse, ...). Caso positivo, as funções de callback
        // definidas anteriormente usando glfwSet*Callback() serão chamadas
        // pela biblioteca GLFW.
        glfwPollEvents();
    }

    // Finalizamos o uso dos recursos do sistema operacional
    glfwTerminate();

    // Fim do programa
    return 0;
}

// Função que carrega uma imagem para ser utilizada como textura
GLuint LoadTextureImage(const char* filename)
{
    printf("Carregando imagem \"%s\"... ", filename);

    // Primeiro fazemos a leitura da imagem do disco
    stbi_set_flip_vertically_on_load(true);
    int width;
    int height;
    int channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);

    if ( data == NULL )
    {
        fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }

    printf("OK (%dx%d).\n", width, height);

    // Agora criamos objetos na GPU com OpenGL para armazenar a textura
    GLuint texture_id;
    GLuint sampler_id;
    glGenTextures(1, &texture_id);
    glGenSamplers(1, &sampler_id);

    // Veja slides 95-96 do documento Aula_20_Mapeamento_de_Texturas.pdf
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Parâmetros de amostragem da textura.
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Agora enviamos a imagem lida do disco para a GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    GLuint textureunit = g_NumLoadedTextures;
    glActiveTexture(GL_TEXTURE0 + textureunit);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindSampler(textureunit, sampler_id);

    stbi_image_free(data);

    g_NumLoadedTextures += 1;

    return texture_id;
}

// Função que desenha um objeto armazenado em g_VirtualScene. Veja definição
// dos objetos na função BuildTrianglesAndAddToVirtualScene().
void DrawVirtualObject(const char* object_name)
{
    SceneObject obj = g_VirtualScene[object_name];
    auto it = g_textureID.find(obj.material_name);
    if (it != g_textureID.end())
    {
        glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_texture"), 1);
        glActiveTexture(GL_TEXTURE0);  // ativa slot 0
        glBindTexture(GL_TEXTURE_2D, it->second);  // textura correta
        glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);  // avisa ao shader
    }
    else
    {
        glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_texture"), 0);
    }

    // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
    // vértices apontados pelo VAO criado pela função BuildTrianglesAndAddToVirtualScene(). Veja
    // comentários detalhados dentro da definição de BuildTrianglesAndAddToVirtualScene().
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);

    // Setamos as variáveis "bbox_min" e "bbox_max" do fragment shader
    // com os parâmetros da axis-aligned bounding box (AABB) do modelo.
    glm::vec3 bbox_min = g_VirtualScene[object_name].bbox_min;
    glm::vec3 bbox_max = g_VirtualScene[object_name].bbox_max;
    glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

    // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
    // apontados pelo VAO como linhas. Veja a definição de
    // g_VirtualScene[""] dentro da função BuildTrianglesAndAddToVirtualScene(), e veja
    // a documentação da função glDrawElements() em
    // http://docs.gl/gl3/glDrawElements.
    glDrawElements(
        g_VirtualScene[object_name].rendering_mode,
        g_VirtualScene[object_name].num_indices,
        GL_UNSIGNED_INT,
        (void*)(g_VirtualScene[object_name].first_index * sizeof(GLuint))
    );

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Função que carrega os shaders de vértices e de fragmentos que serão
// utilizados para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
//
void LoadShadersFromFiles()
{
    // Note que o caminho para os arquivos "shader_vertex.glsl" e
    // "shader_fragment.glsl" estão fixados, sendo que assumimos a existência
    // da seguinte estrutura no sistema de arquivos:
    //
    //    + FCG_Lab_01/
    //    |
    //    +--+ bin/
    //    |  |
    //    |  +--+ Release/  (ou Debug/ ou Linux/)
    //    |     |
    //    |     o-- main.exe
    //    |
    //    +--+ src/
    //       |
    //       o-- shader_vertex.glsl
    //       |
    //       o-- shader_fragment.glsl
    //
    GLuint vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    GLuint fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");

    // Deletamos o programa de GPU anterior, caso ele exista.
    if ( g_GpuProgramID != 0 )
        glDeleteProgram(g_GpuProgramID);

    // Criamos um programa de GPU utilizando os shaders carregados acima.
    g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
    // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
    // (GPU)! Veja arquivo "shader_vertex.glsl" e "shader_fragment.glsl".
    g_model_uniform      = glGetUniformLocation(g_GpuProgramID, "model"); // Variável da matriz "model"
    g_view_uniform       = glGetUniformLocation(g_GpuProgramID, "view"); // Variável da matriz "view" em shader_vertex.glsl
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection"); // Variável da matriz "projection" em shader_vertex.glsl
    g_object_id_uniform  = glGetUniformLocation(g_GpuProgramID, "object_id"); // Variável "object_id" em shader_fragment.glsl
    g_bbox_min_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_min");
    g_bbox_max_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_max");

    // Variáveis em "shader_fragment.glsl" para acesso das imagens de textura
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage2"), 2);
    glUseProgram(0);
}

// Função que pega a matriz M e guarda a mesma no topo da pilha
void PushMatrix(glm::mat4 M)
{
    g_MatrixStack.push(M);
}

// Função que remove a matriz atualmente no topo da pilha e armazena a mesma na variável M
void PopMatrix(glm::mat4& M)
{
    if ( g_MatrixStack.empty() )
    {
        M = Matrix_Identity();
    }
    else
    {
        M = g_MatrixStack.top();
        g_MatrixStack.pop();
    }
}

// Função que computa as normais de um ObjModel, caso elas não tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel* model)
{
    if ( !model->attrib.normals.empty() )
        return;

    // Primeiro computamos as normais para todos os TRIÂNGULOS.
    // Segundo, computamos as normais dos VÉRTICES através do método proposto
    // por Gouraud, onde a normal de cada vértice vai ser a média das normais de
    // todas as faces que compartilham este vértice e que pertencem ao mesmo "smoothing group".

    // Obtemos a lista dos smoothing groups que existem no objeto
    std::set<unsigned int> sgroup_ids;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        assert(model->shapes[shape].mesh.smoothing_group_ids.size() == num_triangles);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);
            unsigned int sgroup = model->shapes[shape].mesh.smoothing_group_ids[triangle];
            assert(sgroup >= 0);
            sgroup_ids.insert(sgroup);
        }
    }

    size_t num_vertices = model->attrib.vertices.size() / 3;
    model->attrib.normals.reserve( 3*num_vertices );

    // Processamos um smoothing group por vez
    for (const unsigned int & sgroup : sgroup_ids)
    {
        std::vector<int> num_triangles_per_vertex(num_vertices, 0);
        std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f,0.0f,0.0f,0.0f));

        // Acumulamos as normais dos vértices de todos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                glm::vec4  vertices[3];
                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                    const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                    const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                    vertices[vertex] = glm::vec4(vx,vy,vz,1.0);
                }

                const glm::vec4  a = vertices[0];
                const glm::vec4  b = vertices[1];
                const glm::vec4  c = vertices[2];

                const glm::vec4  n = crossproduct(b-a, c-a);

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    num_triangles_per_vertex[idx.vertex_index] += 1;
                    vertex_normals[idx.vertex_index] += n;
                }
            }
        }

        // Computamos a média das normais acumuladas
        std::vector<size_t> normal_indices(num_vertices, 0);

        for (size_t vertex_index = 0; vertex_index < vertex_normals.size(); ++vertex_index)
        {
            if (num_triangles_per_vertex[vertex_index] == 0)
                continue;

            glm::vec4 n = vertex_normals[vertex_index] / (float)num_triangles_per_vertex[vertex_index];
            n /= norm(n);

            model->attrib.normals.push_back( n.x );
            model->attrib.normals.push_back( n.y );
            model->attrib.normals.push_back( n.z );

            size_t normal_index = (model->attrib.normals.size() / 3) - 1;
            normal_indices[vertex_index] = normal_index;
        }

        // Escrevemos os índices das normais para os vértices dos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index =
                        normal_indices[ idx.vertex_index ];
                }
            }
        }

    }
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel* model)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float>  model_coefficients;
    std::vector<float>  normal_coefficients;
    std::vector<float>  texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        const float minval = std::numeric_limits<float>::min();
        const float maxval = std::numeric_limits<float>::max();

        glm::vec3 bbox_min = glm::vec3(maxval,maxval,maxval);
        glm::vec3 bbox_max = glm::vec3(minval,minval,minval);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];

                indices.push_back(first_index + 3*triangle + vertex);

                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                //printf("tri %d vert %d = (%.2f, %.2f, %.2f)\n", (int)triangle, (int)vertex, vx, vy, vz);
                model_coefficients.push_back( vx ); // X
                model_coefficients.push_back( vy ); // Y
                model_coefficients.push_back( vz ); // Z
                model_coefficients.push_back( 1.0f ); // W

                bbox_min.x = std::min(bbox_min.x, vx);
                bbox_min.y = std::min(bbox_min.y, vy);
                bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx);
                bbox_max.y = std::max(bbox_max.y, vy);
                bbox_max.z = std::max(bbox_max.z, vz);

                // Inspecionando o código da tinyobjloader, o aluno Bernardo
                // Sulzbach (2017/1) apontou que a maneira correta de testar se
                // existem normais e coordenadas de textura no ObjModel é
                // comparando se o índice retornado é -1. Fazemos isso abaixo.

                if ( idx.normal_index != -1 )
                {
                    const float nx = model->attrib.normals[3*idx.normal_index + 0];
                    const float ny = model->attrib.normals[3*idx.normal_index + 1];
                    const float nz = model->attrib.normals[3*idx.normal_index + 2];
                    normal_coefficients.push_back( nx ); // X
                    normal_coefficients.push_back( ny ); // Y
                    normal_coefficients.push_back( nz ); // Z
                    normal_coefficients.push_back( 0.0f ); // W
                }

                if ( idx.texcoord_index != -1 )
                {
                    const float u = model->attrib.texcoords[2*idx.texcoord_index + 0];
                    const float v = model->attrib.texcoords[2*idx.texcoord_index + 1];
                    texture_coefficients.push_back( u );
                    texture_coefficients.push_back( v );
                }
            }
        }

        size_t last_index = indices.size() - 1;

        SceneObject theobject;
        theobject.name           = model->shapes[shape].name;
        theobject.first_index    = first_index; // Primeiro índice
        theobject.num_indices    = last_index - first_index + 1; // Número de indices
        theobject.rendering_mode = GL_TRIANGLES;       // Índices correspondem ao tipo de rasterização GL_TRIANGLES.
        theobject.vertex_array_object_id = vertex_array_object_id;

	// TINYOBJLOADER – PEGANDO NOME DO MATERIAL CORRETAMENTE
	int material_index = model->shapes[shape].mesh.material_ids.size() > 0 ?
	                     model->shapes[shape].mesh.material_ids[0] : -1;

	if (material_index >= 0 && material_index < (int)model->materials.size())
	{
	    theobject.material_name = model->materials[material_index].name;
	}
	else
	{
	    theobject.material_name = "NO_MATERIAL";
	}


        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;

        g_VirtualScene[model->shapes[shape].name] = theobject;
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0; // "(location = 0)" em "shader_vertex.glsl"
    GLint  number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if ( !normal_coefficients.empty() )
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        location = 1; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if ( !texture_coefficients.empty() )
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        location = 2; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint indices_id;
    glGenBuffers(1, &indices_id);

    // "Ligamos" o buffer. Note que o tipo agora é GL_ELEMENT_ARRAY_BUFFER.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!
    //

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Carrega um Vertex Shader de um arquivo GLSL. Veja definição de LoadShader() abaixo.
GLuint LoadShader_Vertex(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos vértices.
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, vertex_shader_id);

    // Retorna o ID gerado acima
    return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo GLSL . Veja definição de LoadShader() abaixo.
GLuint LoadShader_Fragment(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos fragmentos.
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, fragment_shader_id);

    // Retorna o ID gerado acima
    return fragment_shader_id;
}

// Função auxilar, utilizada pelas duas funções acima. Carrega código de GPU de
// um arquivo GLSL e faz sua compilação.
void LoadShader(const char* filename, GLuint shader_id)
{
    // Lemos o arquivo de texto indicado pela variável "filename"
    // e colocamos seu conteúdo em memória, apontado pela variável
    // "shader_string".
    std::ifstream file;
    try {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    } catch ( std::exception& e ) {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar* shader_string = str.c_str();
    const GLint   shader_string_length = static_cast<GLint>( str.length() );

    // Define o código do shader GLSL, contido na string "shader_string"
    glShaderSource(shader_id, 1, &shader_string, &shader_string_length);

    // Compila o código do shader GLSL (em tempo de execução)
    glCompileShader(shader_id);

    // Verificamos se ocorreu algum erro ou "warning" durante a compilação
    GLint compiled_ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled_ok);

    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    // Alocamos memória para guardar o log de compilação.
    // A chamada "new" em C++ é equivalente ao "malloc()" do C.
    GLchar* log = new GLchar[log_length];
    glGetShaderInfoLog(shader_id, log_length, &log_length, log);

    // Imprime no terminal qualquer erro ou "warning" de compilação
    if ( log_length != 0 )
    {
        std::string  output;

        if ( !compiled_ok )
        {
            output += "ERROR: OpenGL compilation of \"";
            output += filename;
            output += "\" failed.\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }
        else
        {
            output += "WARNING: OpenGL compilation of \"";
            output += filename;
            output += "\".\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }

        fprintf(stderr, "%s", output.c_str());
    }

    // A chamada "delete" em C++ é equivalente ao "free()" do C
    delete [] log;
}

// Esta função cria um programa de GPU, o qual contém obrigatoriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
    // Criamos um identificador (ID) para este programa de GPU
    GLuint program_id = glCreateProgram();

    // Definição dos dois shaders GLSL que devem ser executados pelo programa
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    // Linkagem dos shaders acima ao programa
    glLinkProgram(program_id);

    // Verificamos se ocorreu algum erro durante a linkagem
    GLint linked_ok = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked_ok);

    // Imprime no terminal qualquer erro de linkagem
    if ( linked_ok == GL_FALSE )
    {
        GLint log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

        // Alocamos memória para guardar o log de compilação.
        // A chamada "new" em C++ é equivalente ao "malloc()" do C.
        GLchar* log = new GLchar[log_length];

        glGetProgramInfoLog(program_id, log_length, &log_length, log);

        std::string output;

        output += "ERROR: OpenGL linking of program failed.\n";
        output += "== Start of link log\n";
        output += log;
        output += "\n== End of link log\n";

        // A chamada "delete" em C++ é equivalente ao "free()" do C
        delete [] log;

        fprintf(stderr, "%s", output.c_str());
    }

    // Os "Shader Objects" podem ser marcados para deleção após serem linkados
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    // Retornamos o ID gerado acima
    return program_id;
}

// Definição da função que será chamada sempre que a janela do sistema
// operacional for redimensionada, por consequência alterando o tamanho do
// "framebuffer" (região de memória onde são armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    // Indicamos que queremos renderizar em toda região do framebuffer. A
    // função "glViewport" define o mapeamento das "normalized device
    // coordinates" (NDC) para "pixel coordinates".  Essa é a operação de
    // "Screen Mapping" ou "Viewport Mapping" vista em aula ({+ViewportMapping2+}).
    glViewport(0, 0, width, height);

    // Atualizamos também a razão que define a proporção da janela (largura /
    // altura), a qual será utilizada na definição das matrizes de projeção,
    // tal que não ocorra distorções durante o processo de "Screen Mapping"
    // acima, quando NDC é mapeado para coordenadas de pixels. Veja slides 205-215 do documento Aula_09_Projecoes.pdf.
    //
    // O cast para float é necessário pois números inteiros são arredondados ao
    // serem divididos!
    g_ScreenRatio = (float)width / height;
}

// Função callback chamada sempre que o usuário aperta algum dos botões do mouse
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_LeftMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_LeftMouseButtonPressed = true;

        // Debug: Print all player relevant fields
        // printf("Player Magazine Ammo: %d\n", g_Player.magazine_ammo);
        // printf("Player Shoot Cooldown: %f\n", g_Player.shoot_cooldown);
        // printf("Player Is Reloading: %d\n", g_Player.is_reloading);
        // printf("Player Reload Time: %f\n", g_Player.reload_time);
        // printf("Player Reload Time Total: %f\n", g_Player.reload_time_total);
        // printf("Player Health: %f\n", g_Player.health);
        // printf("Player Max Health: %f\n", g_Player.max_health);
        // printf("Player Magazine Size: %d\n", g_Player.magazine_size);

        // Verifica se pode atirar (tem munição no carregador, não está em cooldown e não está recarregando)
        if (g_Player.magazine_ammo > 0 && g_Player.shoot_cooldown <= 0.0f && !g_Player.is_reloading)
        {
            // Realiza raycast do centro da tela
            glm::vec4 camera_position = g_Player.GetThirdPersonCameraPosition();
            glm::vec4 camera_lookat = g_Player.GetCameraLookAt();
            glm::vec4 camera_view_vector = camera_lookat - camera_position;

            // Normaliza o vetor de direção da câmera
            float view_length = sqrt(camera_view_vector.x * camera_view_vector.x +
                                     camera_view_vector.y * camera_view_vector.y +
                                     camera_view_vector.z * camera_view_vector.z);
            if (view_length > 0.001f)
            {
                camera_view_vector.x /= view_length;
                camera_view_vector.y /= view_length;
                camera_view_vector.z /= view_length;
                CameraRaycast(camera_position, camera_view_vector);

                // Consome munição do carregador e inicia cooldown
                g_Player.magazine_ammo--;
                g_Player.shoot_cooldown = g_Player.shoot_cooldown_time;
            }
        }
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_LeftMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_RightMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_RightMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_RightMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_MiddleMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_MiddleMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_MiddleMouseButtonPressed = false;
    }
}

// Função callback chamada sempre que o usuário movimentar o cursor do mouse em
// cima da janela OpenGL.
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    // Computamos quanto que o mouse se movimentou desde o último instante de tempo
    // e usamos esta movimentação para atualizar os parâmetros que definem a
    // posição da câmera dentro da cena virtual. Com GLFW_CURSOR_DISABLED, o mouse
    // está sempre ativo e não precisa de clique.

    // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
    float dx = xpos - g_LastCursorPosX;
    float dy = ypos - g_LastCursorPosY;

    // Atualizamos os ângulos da câmera em terceira pessoa do jogador
    // Sensibilidade reduzida da câmera
    float camera_sensitivity = 0.003f;
    g_Player.camera_angle_horizontal -= camera_sensitivity * dx;
    g_Player.camera_angle_vertical   += camera_sensitivity * dy;

    // Limita o ângulo vertical para evitar que a câmera vá muito acima ou abaixo
    float vmax = 3.141592f/3.0f;  // Limite superior (câmera não vai muito acima)
    float vmin = -0.15; // Limite inferior (câmera não vai muito abaixo)

    if (g_Player.camera_angle_vertical > vmax)
        g_Player.camera_angle_vertical = vmax;

    if (g_Player.camera_angle_vertical < vmin)
        g_Player.camera_angle_vertical = vmin;

    // Atualizamos as variáveis globais para armazenar a posição atual do
    // cursor como sendo a última posição conhecida do cursor.
    g_LastCursorPosX = xpos;
    g_LastCursorPosY = ypos;

    if (g_RightMouseButtonPressed)
    {
        // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;

        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
    }

    if (g_MiddleMouseButtonPressed)
    {
        // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;

        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
    }
}

// Função callback chamada sempre que o usuário movimenta a "rodinha" do mouse.
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Atualizamos a distância da câmera em terceira pessoa do jogador utilizando a
    // movimentação da "rodinha", simulando um ZOOM.
    g_Player.camera_distance -= 0.1f*yoffset;

    // Uma câmera look-at nunca pode estar exatamente "em cima" do ponto para
    // onde ela está olhando, pois isto gera problemas de divisão por zero na
    // definição do sistema de coordenadas da câmera. Isto é, a variável abaixo
    // nunca pode ser zero. Versões anteriores deste código possuíam este bug,
    // o qual foi detectado pelo aluno Vinicius Fraga (2017/2).
    const float verysmallnumber = std::numeric_limits<float>::epsilon();
    const float maxdistance = 20.0f; // Distância máxima
    const float mindistance = 1.0f;  // Distância mínima

    if (g_Player.camera_distance < mindistance)
        g_Player.camera_distance = mindistance;

    if (g_Player.camera_distance > maxdistance)
        g_Player.camera_distance = maxdistance;
}

// Definição da função que será chamada sempre que o usuário pressionar alguma
// tecla do teclado. Veja http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mod)
{
    // =======================
    // Não modifique este loop! Ele é utilizando para correção automatizada dos
    // laboratórios. Deve ser sempre o primeiro comando desta função KeyCallback().
    for (int i = 0; i < 10; ++i)
        if (key == GLFW_KEY_0 + i && action == GLFW_PRESS && mod == GLFW_MOD_SHIFT)
            std::exit(100 + i);
    // =======================

    // Se o usuário pressionar a tecla ESC, fechamos a janela.
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);


    float delta = 3.141592 / 16; // 22.5 graus, em radianos.

    if (key == GLFW_KEY_X && action == GLFW_PRESS)
    {
        // PASS
    }

    if (key == GLFW_KEY_Y && action == GLFW_PRESS)
    {
        // PASS
    }
    if (key == GLFW_KEY_Z && action == GLFW_PRESS)
    {
        // PASS
    }

    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        // PASS
    }

    // Test keys for raycast functions
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        // P key: Test PlayerRaycast (from player center)
        PlayerRaycast();
    }

    if (key == GLFW_KEY_E && action == GLFW_PRESS)
    {
        // E key: Test EnemyToPlayerRaycast (from first alive enemy to player)
        if (!g_Enemies.empty())
        {
            // Find first alive enemy
            for (size_t i = 0; i < g_Enemies.size(); ++i)
            {
                if (!g_Enemies[i].IsDead())
                {
                    EnemyToPlayerRaycast(i);
                    break;
                }
            }
        }
        else
        {
            printf("No enemies available for EnemyToPlayerRaycast test\n");
        }
    }

    // Controles WASD para movimento do jogador
    if (key == GLFW_KEY_W)
    {
        if (action == GLFW_PRESS)
            g_Player.moving_forward = true;
        else if (action == GLFW_RELEASE)
            g_Player.moving_forward = false;
    }

    if (key == GLFW_KEY_S)
    {
        if (action == GLFW_PRESS)
            g_Player.moving_backward = true;
        else if (action == GLFW_RELEASE)
            g_Player.moving_backward = false;
    }

    if (key == GLFW_KEY_A)
    {
        if (action == GLFW_PRESS)
            g_Player.moving_left = true;
        else if (action == GLFW_RELEASE)
            g_Player.moving_left = false;
    }

    if (key == GLFW_KEY_D)
    {
        if (action == GLFW_PRESS)
            g_Player.moving_right = true;
        else if (action == GLFW_RELEASE)
            g_Player.moving_right = false;
    }

    // Shift para correr
    if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
    {
        if (action == GLFW_PRESS)
            g_Player.is_running = true;
        else if (action == GLFW_RELEASE)
            g_Player.is_running = false;
    }

    // Se o usuário apertar a tecla P, utilizamos projeção perspectiva.
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = true;
    }

    // Se o usuário apertar a tecla O, utilizamos projeção ortográfica.
    if (key == GLFW_KEY_O && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = false;
    }

    // Se o usuário apertar a tecla H, fazemos um "toggle" do texto informativo mostrado na tela.
    if (key == GLFW_KEY_H && action == GLFW_PRESS)
    {
        g_ShowInfoText = !g_ShowInfoText;
    }

    // Se o usuário apertar a tecla R, recarregamos os shaders dos arquivos "shader_fragment.glsl" e "shader_vertex.glsl".
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        // Recarrega o carregador se não estiver recarregando e o carregador não estiver cheio
        if (!g_Player.is_reloading && g_Player.magazine_ammo < g_Player.magazine_size)
        {
            g_Player.is_reloading = true;
            g_Player.reload_time = g_Player.reload_time_total;
        }
    }
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char* description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

// Esta função recebe um vértice com coordenadas de modelo p_model e passa o
// mesmo por todos os sistemas de coordenadas armazenados nas matrizes model,
// view, e projection; e escreve na tela as matrizes e pontos resultantes
// dessas transformações.
void TextRendering_ShowModelViewProjection(
    GLFWwindow* window,
    glm::mat4 projection,
    glm::mat4 view,
    glm::mat4 model,
    glm::vec4 p_model
)
{
    if ( !g_ShowInfoText )
        return;

    glm::vec4 p_world = model*p_model;
    glm::vec4 p_camera = view*p_world;
    glm::vec4 p_clip = projection*p_camera;
    glm::vec4 p_ndc = p_clip / p_clip.w;

    float pad = TextRendering_LineHeight(window);

    TextRendering_PrintString(window, " Model matrix             Model     In World Coords.", -1.0f, 1.0f-pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, model, p_model, -1.0f, 1.0f-2*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-6*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-7*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-8*pad, 1.0f);

    TextRendering_PrintString(window, " View matrix              World     In Camera Coords.", -1.0f, 1.0f-9*pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, view, p_world, -1.0f, 1.0f-10*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-14*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-15*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-16*pad, 1.0f);

    TextRendering_PrintString(window, " Projection matrix        Camera                    In NDC", -1.0f, 1.0f-17*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductDivW(window, projection, p_camera, -1.0f, 1.0f-18*pad, 1.0f);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glm::vec2 a = glm::vec2(-1, -1);
    glm::vec2 b = glm::vec2(+1, +1);
    glm::vec2 p = glm::vec2( 0,  0);
    glm::vec2 q = glm::vec2(width, height);

    glm::mat4 viewport_mapping = Matrix(
        (q.x - p.x)/(b.x-a.x), 0.0f, 0.0f, (b.x*p.x - a.x*q.x)/(b.x-a.x),
        0.0f, (q.y - p.y)/(b.y-a.y), 0.0f, (b.y*p.y - a.y*q.y)/(b.y-a.y),
        0.0f , 0.0f , 1.0f , 0.0f ,
        0.0f , 0.0f , 0.0f , 1.0f
    );

    TextRendering_PrintString(window, "                                                       |  ", -1.0f, 1.0f-22*pad, 1.0f);
    TextRendering_PrintString(window, "                            .--------------------------'  ", -1.0f, 1.0f-23*pad, 1.0f);
    TextRendering_PrintString(window, "                            V                           ", -1.0f, 1.0f-24*pad, 1.0f);

    TextRendering_PrintString(window, " Viewport matrix           NDC      In Pixel Coords.", -1.0f, 1.0f-25*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductMoreDigits(window, viewport_mapping, p_ndc, -1.0f, 1.0f-26*pad, 1.0f);
}

// Escrevemos na tela qual matriz de projeção está sendo utilizada.
void TextRendering_ShowProjection(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    if ( g_UsePerspectiveProjection )
        TextRendering_PrintString(window, "Perspective", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
    else
        TextRendering_PrintString(window, "Orthographic", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
}

// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    // Variáveis estáticas (static) mantém seus valores entre chamadas
    // subsequentes da função!
    static float old_seconds = (float)glfwGetTime();
    static int   ellapsed_frames = 0;
    static char  buffer[20] = "?? fps";
    static int   numchars = 7;

    ellapsed_frames += 1;

    // Recuperamos o número de segundos que passou desde a execução do programa
    float seconds = (float)glfwGetTime();

    // Número de segundos desde o último cálculo do fps
    float ellapsed_seconds = seconds - old_seconds;

    if ( ellapsed_seconds > 1.0f )
    {
        numchars = snprintf(buffer, 20, "%.2f fps", ellapsed_frames / ellapsed_seconds);

        old_seconds = seconds;
        ellapsed_frames = 0;
    }

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    // Position the FPS text using normalized device coordinates (-1.0 to 1.0)
    float x_pos = 1.0f - (numchars + 1) * charwidth;
    float y_pos = 1.0f - lineheight;

    TextRendering_PrintString(window, buffer, x_pos, y_pos, 1.0f);
}

// Função para debugging: imprime no terminal todas informações de um modelo
// geométrico carregado de um arquivo ".obj".
// Veja: https://github.com/syoyo/tinyobjloader/blob/22883def8db9ef1f3ffb9b404318e7dd25fdbb51/loader_example.cc#L98
void PrintObjModelInfo(ObjModel* model)
{
  const tinyobj::attrib_t                & attrib    = model->attrib;
  const std::vector<tinyobj::shape_t>    & shapes    = model->shapes;
  const std::vector<tinyobj::material_t> & materials = model->materials;

  printf("# of vertices  : %d\n", (int)(attrib.vertices.size() / 3));
  printf("# of normals   : %d\n", (int)(attrib.normals.size() / 3));
  printf("# of texcoords : %d\n", (int)(attrib.texcoords.size() / 2));
  printf("# of shapes    : %d\n", (int)shapes.size());
  printf("# of materials : %d\n", (int)materials.size());

  for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
    printf("  v[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.vertices[3 * v + 0]),
           static_cast<const double>(attrib.vertices[3 * v + 1]),
           static_cast<const double>(attrib.vertices[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.normals.size() / 3; v++) {
    printf("  n[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.normals[3 * v + 0]),
           static_cast<const double>(attrib.normals[3 * v + 1]),
           static_cast<const double>(attrib.normals[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.texcoords.size() / 2; v++) {
    printf("  uv[%ld] = (%f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.texcoords[2 * v + 0]),
           static_cast<const double>(attrib.texcoords[2 * v + 1]));
  }

  // For each shape
  for (size_t i = 0; i < shapes.size(); i++) {
    printf("shape[%ld].name = %s\n", static_cast<long>(i),
           shapes[i].name.c_str());
    printf("Size of shape[%ld].indices: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.indices.size()));

    size_t index_offset = 0;

    assert(shapes[i].mesh.num_face_vertices.size() ==
           shapes[i].mesh.material_ids.size());

    printf("shape[%ld].num_faces: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.num_face_vertices.size()));

    // For each face
    for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
      size_t fnum = shapes[i].mesh.num_face_vertices[f];

      printf("  face[%ld].fnum = %ld\n", static_cast<long>(f),
             static_cast<unsigned long>(fnum));

      // For each vertex in the face
      for (size_t v = 0; v < fnum; v++) {
        tinyobj::index_t idx = shapes[i].mesh.indices[index_offset + v];
        printf("    face[%ld].v[%ld].idx = %d/%d/%d\n", static_cast<long>(f),
               static_cast<long>(v), idx.vertex_index, idx.normal_index,
               idx.texcoord_index);
      }

      printf("  face[%ld].material_id = %d\n", static_cast<long>(f),
             shapes[i].mesh.material_ids[f]);

      index_offset += fnum;
    }

    printf("shape[%ld].num_tags: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.tags.size()));
    for (size_t t = 0; t < shapes[i].mesh.tags.size(); t++) {
      printf("  tag[%ld] = %s ", static_cast<long>(t),
             shapes[i].mesh.tags[t].name.c_str());
      printf(" ints: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].intValues.size(); ++j) {
        printf("%ld", static_cast<long>(shapes[i].mesh.tags[t].intValues[j]));
        if (j < (shapes[i].mesh.tags[t].intValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" floats: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].floatValues.size(); ++j) {
        printf("%f", static_cast<const double>(
                         shapes[i].mesh.tags[t].floatValues[j]));
        if (j < (shapes[i].mesh.tags[t].floatValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" strings: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].stringValues.size(); ++j) {
        printf("%s", shapes[i].mesh.tags[t].stringValues[j].c_str());
        if (j < (shapes[i].mesh.tags[t].stringValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");
      printf("\n");
    }
  }

  for (size_t i = 0; i < materials.size(); i++) {
    printf("material[%ld].name = %s\n", static_cast<long>(i),
           materials[i].name.c_str());
    printf("  material.Ka = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].ambient[0]),
           static_cast<const double>(materials[i].ambient[1]),
           static_cast<const double>(materials[i].ambient[2]));
    printf("  material.Kd = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].diffuse[0]),
           static_cast<const double>(materials[i].diffuse[1]),
           static_cast<const double>(materials[i].diffuse[2]));
    printf("  material.Ks = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].specular[0]),
           static_cast<const double>(materials[i].specular[1]),
           static_cast<const double>(materials[i].specular[2]));
    printf("  material.Tr = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].transmittance[0]),
           static_cast<const double>(materials[i].transmittance[1]),
           static_cast<const double>(materials[i].transmittance[2]));
    printf("  material.Ke = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].emission[0]),
           static_cast<const double>(materials[i].emission[1]),
           static_cast<const double>(materials[i].emission[2]));
    printf("  material.Ns = %f\n", static_cast<const double>(materials[i].shininess));
    printf("  material.Ni = %f\n", static_cast<const double>(materials[i].ior));
    printf("  material.dissolve = %f\n", static_cast<const double>(materials[i].dissolve));
    printf("  material.illum = %d\n", materials[i].illum);
    printf("  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
    printf("  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
    printf("  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
    printf("  material.map_Ns = %s\n", materials[i].specular_highlight_texname.c_str());
    printf("  material.map_bump = %s\n", materials[i].bump_texname.c_str());
    printf("  material.map_d = %s\n", materials[i].alpha_texname.c_str());
    printf("  material.disp = %s\n", materials[i].displacement_texname.c_str());
    printf("  <<PBR>>\n");
    printf("  material.Pr     = %f\n", materials[i].roughness);
    printf("  material.Pm     = %f\n", materials[i].metallic);
    printf("  material.Ps     = %f\n", materials[i].sheen);
    printf("  material.Pc     = %f\n", materials[i].clearcoat_thickness);
    printf("  material.Pcr    = %f\n", materials[i].clearcoat_thickness);
    printf("  material.aniso  = %f\n", materials[i].anisotropy);
    printf("  material.anisor = %f\n", materials[i].anisotropy_rotation);
    printf("  material.map_Ke = %s\n", materials[i].emissive_texname.c_str());
    printf("  material.map_Pr = %s\n", materials[i].roughness_texname.c_str());
    printf("  material.map_Pm = %s\n", materials[i].metallic_texname.c_str());
    printf("  material.map_Ps = %s\n", materials[i].sheen_texname.c_str());
    printf("  material.norm   = %s\n", materials[i].normal_texname.c_str());
    std::map<std::string, std::string>::const_iterator it(materials[i].unknown_parameter.begin());
    std::map<std::string, std::string>::const_iterator itEnd(materials[i].unknown_parameter.end());

    for (; it != itEnd; it++) {
      printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
    }
    printf("\n");
  }
}

// Função que desenha um indicador de direção (linha) mostrando para onde a entidade está olhando
void DrawDirectionIndicator(glm::vec4 position, glm::vec4 forward, float length, glm::mat4 view, glm::mat4 projection, bool is_player)
{
    // Cria VAO e VBO se ainda não existirem
    if (g_LineVAO == 0)
    {
        glGenVertexArrays(1, &g_LineVAO);
        glGenBuffers(1, &g_LineVBO);
    }

    // Calcula o ponto final da linha
    glm::vec4 end_position = position + forward * length;

    // Define os vértices da linha
    float line_vertices[] = {
        position.x, position.y, position.z, 1.0f,
        end_position.x, end_position.y, end_position.z, 1.0f
    };

    // Configura o VAO
    glBindVertexArray(g_LineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_LineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    // Usa o shader principal
    glUseProgram(g_GpuProgramID);

    // Matriz de modelagem identidade (linha já está em coordenadas do mundo)
    glm::mat4 model = Matrix_Identity();
    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(projection));

    // Define cor (verde para player, vermelho para inimigos)
    #define DIRECTION_LINE_PLAYER 3
    #define DIRECTION_LINE_ENEMY 5
    glUniform1i(g_object_id_uniform, is_player ? DIRECTION_LINE_PLAYER : DIRECTION_LINE_ENEMY);

    // Desabilita culling para linhas
    glDisable(GL_CULL_FACE);

    // Desenha a linha
    glLineWidth(3.0f);
    glDrawArrays(GL_LINES, 0, 2);
    glLineWidth(1.0f);

    // Reabilita culling
    glEnable(GL_CULL_FACE);

    glBindVertexArray(0);
}

// Função que desenha um crosshair no centro da tela
void DrawCrosshair(GLFWwindow* window)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    // Salva o estado atual
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Desabilita depth test para o crosshair
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Configura viewport para coordenadas de tela
    glViewport(0, 0, width, height);

    // Cria VAO e VBO se ainda não existirem
    if (g_LineVAO == 0)
    {
        glGenVertexArrays(1, &g_LineVAO);
        glGenBuffers(1, &g_LineVBO);
    }

    // Tamanho do crosshair em pixels
    float crosshair_size = 10.0f;
    float outline_offset = 1.5f; // Offset do contorno em pixels

    // Converte coordenadas de tela para NDC (-1 a 1)
    float center_x_ndc = 0.0f;
    float center_y_ndc = 0.0f;
    float size_x_ndc = (crosshair_size * 2.0f) / width;
    float size_y_ndc = (crosshair_size * 2.0f) / height;
    float outline_x_ndc = (outline_offset * 2.0f) / width;
    float outline_y_ndc = (outline_offset * 2.0f) / height;

    // Usa o shader principal
    glUseProgram(g_GpuProgramID);

    // Matrizes de transformação para 2D (coordenadas normalizadas - já estamos em NDC)
    glm::mat4 model = Matrix_Identity();
    glm::mat4 view = Matrix_Identity();
    glm::mat4 projection = Matrix_Identity(); // Já estamos em coordenadas normalizadas

    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(projection));

    // Primeiro desenha o contorno escuro (um pouco maior)
    #define CROSSHAIR_OUTLINE 6
    glUniform1i(g_object_id_uniform, CROSSHAIR_OUTLINE);

    // Define os vértices do contorno do crosshair (linha horizontal e vertical, ligeiramente deslocadas)
    float outline_vertices[] = {
        // Linha horizontal (com offset para criar contorno)
        center_x_ndc - size_x_ndc - outline_x_ndc, center_y_ndc, 0.0f, 1.0f,
        center_x_ndc + size_x_ndc + outline_x_ndc, center_y_ndc, 0.0f, 1.0f,
        // Linha vertical (com offset para criar contorno)
        center_x_ndc, center_y_ndc - size_y_ndc - outline_y_ndc, 0.0f, 1.0f,
        center_x_ndc, center_y_ndc + size_y_ndc + outline_y_ndc, 0.0f, 1.0f
    };

    // Configura o VAO para o contorno
    glBindVertexArray(g_LineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_LineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(outline_vertices), outline_vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    // Desenha o contorno (mais grosso)
    glLineWidth(4.0f);
    glDrawArrays(GL_LINES, 0, 4);

    // Agora desenha o crosshair verde (sobre o contorno)
    #define CROSSHAIR 4
    glUniform1i(g_object_id_uniform, CROSSHAIR);

    // Define os vértices do crosshair verde em NDC (linha horizontal e vertical)
    float crosshair_vertices[] = {
        // Linha horizontal
        center_x_ndc - size_x_ndc, center_y_ndc, 0.0f, 1.0f,
        center_x_ndc + size_x_ndc, center_y_ndc, 0.0f, 1.0f,
        // Linha vertical
        center_x_ndc, center_y_ndc - size_y_ndc, 0.0f, 1.0f,
        center_x_ndc, center_y_ndc + size_y_ndc, 0.0f, 1.0f
    };

    // Atualiza o buffer com os vértices do crosshair verde
    glBufferData(GL_ARRAY_BUFFER, sizeof(crosshair_vertices), crosshair_vertices, GL_DYNAMIC_DRAW);

    // Desenha o crosshair verde (mais fino, sobre o contorno)
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 4);
    glLineWidth(1.0f);

    glBindVertexArray(0);

    // Restaura o viewport original
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    // Restaura estados
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// Função que desenha uma barra de vida acima de um inimigo
void DrawHealthBar(GLFWwindow* window, glm::vec4 world_position, float health, float max_health, glm::mat4 view, glm::mat4 projection)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    // Projeta a posição 3D do mundo para coordenadas de tela
    glm::mat4 model = Matrix_Identity();
    glm::vec4 position_camera = view * model * world_position;
    glm::vec4 position_clip = projection * position_camera;

    // Se está atrás da câmera, não desenha
    if (position_clip.w < 0.0f)
        return;

    // Converte para NDC
    glm::vec3 position_ndc = glm::vec3(position_clip) / position_clip.w;

    // Converte NDC para coordenadas de tela
    float screen_x = (position_ndc.x + 1.0f) * 0.5f * width;
    float screen_y = (1.0f - position_ndc.y) * 0.5f * height; // Inverte Y

    // Offset para posicionar a barra acima do modelo (em pixels)
    float bar_offset_y = 40.0f;
    float bar_y = screen_y - bar_offset_y;

    // Tamanho da barra de vida
    float bar_width = 60.0f;
    float bar_height = 8.0f;
    float outline_thickness = 2.0f;

    // Calcula a porcentagem de vida
    float health_percentage = (max_health > 0.0f) ? (health / max_health) : 0.0f;
    if (health_percentage < 0.0f) health_percentage = 0.0f;
    if (health_percentage > 1.0f) health_percentage = 1.0f;

    // Converte para NDC
    float bar_width_ndc = (bar_width * 2.0f) / width;
    float bar_height_ndc = (bar_height * 2.0f) / height;
    float outline_ndc = (outline_thickness * 2.0f) / width;
    float bar_x_ndc = (screen_x * 2.0f) / width - 1.0f;
    float bar_y_ndc = 1.0f - (bar_y * 2.0f) / height;

    // Salva o estado atual
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Desabilita depth test para a barra de vida
    // IMPORTANTE: Mantém depth test desabilitado mas garante ordem de renderização
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Garante que o último desenho fica por cima
    glDepthFunc(GL_ALWAYS);

    // Configura viewport para coordenadas de tela
    glViewport(0, 0, width, height);

    // Cria VAO e VBO se ainda não existirem
    if (g_LineVAO == 0)
    {
        glGenVertexArrays(1, &g_LineVAO);
        glGenBuffers(1, &g_LineVBO);
    }

    // Usa o shader principal
    glUseProgram(g_GpuProgramID);

    // Matrizes de transformação para 2D
    model = Matrix_Identity();
    glm::mat4 view_2d = Matrix_Identity();
    glm::mat4 projection_2d = Matrix_Identity();

    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(view_2d));
    glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(projection_2d));

    // Configura o VAO
    glBindVertexArray(g_LineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_LineVBO);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    // Desenha o contorno escuro (retângulo externo)
    #define HEALTH_BAR_OUTLINE 7
    glUniform1i(g_object_id_uniform, HEALTH_BAR_OUTLINE);

    float outline_vertices[] = {
        // Retângulo do contorno (4 linhas formando um retângulo)
        bar_x_ndc - bar_width_ndc/2.0f - outline_ndc, bar_y_ndc - bar_height_ndc/2.0f - outline_ndc, 0.0f, 1.0f,
        bar_x_ndc + bar_width_ndc/2.0f + outline_ndc, bar_y_ndc - bar_height_ndc/2.0f - outline_ndc, 0.0f, 1.0f,
        bar_x_ndc + bar_width_ndc/2.0f + outline_ndc, bar_y_ndc - bar_height_ndc/2.0f - outline_ndc, 0.0f, 1.0f,
        bar_x_ndc + bar_width_ndc/2.0f + outline_ndc, bar_y_ndc + bar_height_ndc/2.0f + outline_ndc, 0.0f, 1.0f,
        bar_x_ndc + bar_width_ndc/2.0f + outline_ndc, bar_y_ndc + bar_height_ndc/2.0f + outline_ndc, 0.0f, 1.0f,
        bar_x_ndc - bar_width_ndc/2.0f - outline_ndc, bar_y_ndc + bar_height_ndc/2.0f + outline_ndc, 0.0f, 1.0f,
        bar_x_ndc - bar_width_ndc/2.0f - outline_ndc, bar_y_ndc + bar_height_ndc/2.0f + outline_ndc, 0.0f, 1.0f,
        bar_x_ndc - bar_width_ndc/2.0f - outline_ndc, bar_y_ndc - bar_height_ndc/2.0f - outline_ndc, 0.0f, 1.0f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(outline_vertices), outline_vertices, GL_DYNAMIC_DRAW);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 8);

    // Desenha o fundo cinza (HP faltando) - usando triângulos para preencher
    #define HEALTH_BAR_BACKGROUND 8
    glUniform1i(g_object_id_uniform, HEALTH_BAR_BACKGROUND);

    float background_vertices[] = {
        // Triângulo 1
        bar_x_ndc - bar_width_ndc/2.0f, bar_y_ndc - bar_height_ndc/2.0f, 0.0f, 1.0f,
        bar_x_ndc + bar_width_ndc/2.0f, bar_y_ndc - bar_height_ndc/2.0f, 0.0f, 1.0f,
        bar_x_ndc + bar_width_ndc/2.0f, bar_y_ndc + bar_height_ndc/2.0f, 0.0f, 1.0f,
        // Triângulo 2
        bar_x_ndc - bar_width_ndc/2.0f, bar_y_ndc - bar_height_ndc/2.0f, 0.0f, 1.0f,
        bar_x_ndc + bar_width_ndc/2.0f, bar_y_ndc + bar_height_ndc/2.0f, 0.0f, 1.0f,
        bar_x_ndc - bar_width_ndc/2.0f, bar_y_ndc + bar_height_ndc/2.0f, 0.0f, 1.0f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(background_vertices), background_vertices, GL_DYNAMIC_DRAW);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Desenha a barra verde (HP atual) - apenas a parte proporcional
    // IMPORTANTE: Desenha DEPOIS do fundo cinza para ficar por cima
    #define HEALTH_BAR_FILL 9
    glUniform1i(g_object_id_uniform, HEALTH_BAR_FILL);

    float fill_width_ndc = bar_width_ndc * health_percentage;
    float fill_left = bar_x_ndc - bar_width_ndc/2.0f;
    float fill_right = fill_left + fill_width_ndc;

    // Só desenha se houver HP (health_percentage > 0)
    // IMPORTANTE: Desenha por último para garantir que fica por cima do fundo cinza
    if (health_percentage > 0.001f)
    {
        // Pequeno offset interno para garantir que a barra verde fica dentro do contorno
        float inner_offset = 0.5f / width; // 0.5 pixel em NDC
        float fill_vertices[] = {
            // Triângulo 1 - barra verde (HP atual)
            fill_left + inner_offset, bar_y_ndc - bar_height_ndc/2.0f + inner_offset, 0.0f, 1.0f,
            fill_right - inner_offset, bar_y_ndc - bar_height_ndc/2.0f + inner_offset, 0.0f, 1.0f,
            fill_right - inner_offset, bar_y_ndc + bar_height_ndc/2.0f - inner_offset, 0.0f, 1.0f,
            // Triângulo 2
            fill_left + inner_offset, bar_y_ndc - bar_height_ndc/2.0f + inner_offset, 0.0f, 1.0f,
            fill_right - inner_offset, bar_y_ndc + bar_height_ndc/2.0f - inner_offset, 0.0f, 1.0f,
            fill_left + inner_offset, bar_y_ndc + bar_height_ndc/2.0f - inner_offset, 0.0f, 1.0f
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof(fill_vertices), fill_vertices, GL_DYNAMIC_DRAW);
        // Garante que está desenhando com fill mode
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);

    // Restaura o viewport original
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    // Restaura estados
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); // Restaura função de depth padrão
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1.0f);
}

// Função que desenha o HUD com HP e munição do jogador
void DrawHUD(GLFWwindow* window)
{
    // Configurações de texto
    float text_scale = 1.0f;
    float line_height = TextRendering_LineHeight(window);
    float char_width = TextRendering_CharWidth(window);

    // Posição do HUD (canto inferior esquerdo em coordenadas NDC)
    // NDC: x de -1.0 (esquerda) a 1.0 (direita), y de -1.0 (baixo) a 1.0 (cima)
    // Usa uma margem segura para garantir que o texto fique visível
    float hud_x = -1.0f + 10.0f * char_width;  // 10 caracteres da esquerda
    float hud_y_start = -1.0f + 5.0f * line_height;  // Posição inicial mais alta para caber tudo

    // Calcula número da wave atual (maior ID de wave + 1, ou total de waves)
    int current_wave = 0;
    if (!g_Waves.empty())
    {
        // Encontra o maior ID de wave
        int max_wave_id = -1;
        for (const auto& wave : g_Waves)
        {
            if (wave.wave_id > max_wave_id)
                max_wave_id = wave.wave_id;
        }
        current_wave = max_wave_id + 1; // Wave number is 1-indexed for display
    }

    // Conta inimigos vivos (não mortos)
    int enemies_left = 0;
    for (const auto& enemy : g_Enemies)
    {
        if (!enemy.IsDead())
        {
            enemies_left++;
        }
    }

    // Desenha HP (garante que está usando os dados corretos do jogador)
    float current_y = hud_y_start;
    char hp_text[64];
    snprintf(hp_text, 64, "HP: %.0f/%.0f", g_Player.health, g_Player.max_health);
    TextRendering_PrintString(window, hp_text, hud_x, current_y, text_scale);

    // Desenha munição do carregador (garante que está usando os dados corretos do jogador)
    current_y -= 1.5f * line_height;
    char ammo_text[64];
    snprintf(ammo_text, 64, "Ammo: %d/%d", g_Player.magazine_ammo, g_Player.magazine_size);
    TextRendering_PrintString(window, ammo_text, hud_x, current_y, text_scale);

    // Desenha número da wave
    current_y -= 1.5f * line_height;
    char wave_text[64];
    snprintf(wave_text, 64, "Wave: %d", current_wave);
    TextRendering_PrintString(window, wave_text, hud_x, current_y, text_scale);

    // Desenha inimigos restantes
    current_y -= 1.5f * line_height;
    char enemies_text[64];
    snprintf(enemies_text, 64, "Enemies: %d", enemies_left);
    TextRendering_PrintString(window, enemies_text, hud_x, current_y, text_scale);

    // Desenha status de reload se estiver recarregando
    if (g_Player.is_reloading)
    {
        current_y -= 1.5f * line_height;
        char reload_text[64];
        snprintf(reload_text, 64, "Reloading: %.1fs", g_Player.reload_time);
        TextRendering_PrintString(window, reload_text, hud_x, current_y, text_scale * 0.9f);
    }
}

// Função auxiliar para verificar interseção de raio com AABB (Axis-Aligned Bounding Box)
// Retorna true se houver interseção e armazena a distância em t
bool RayAABBIntersection(const glm::vec4& ray_origin, const glm::vec4& ray_dir,
                         const glm::vec3& box_min, const glm::vec3& box_max, float& t)
{
    // Algoritmo de interseção raio-AABB (Slab method)
    float tmin = 0.0f;
    float tmax = std::numeric_limits<float>::max();

    for (int i = 0; i < 3; ++i)
    {
        // Evita divisão por zero quando o raio é paralelo ao eixo
        if (fabs(ray_dir[i]) < 1e-6f)
        {
            // Raio paralelo ao eixo - verifica se está dentro do AABB neste eixo
            if (ray_origin[i] < box_min[i] || ray_origin[i] > box_max[i])
                return false;
            continue;
        }

        float invD = 1.0f / ray_dir[i];
        float t0 = (box_min[i] - ray_origin[i]) * invD;
        float t1 = (box_max[i] - ray_origin[i]) * invD;

        if (invD < 0.0f)
        {
            float temp = t0;
            t0 = t1;
            t1 = temp;
        }

        tmin = t0 > tmin ? t0 : tmin;
        tmax = t1 < tmax ? t1 : tmax;

        if (tmax < tmin)
            return false;
    }

    t = tmin;
    return tmin >= 0.0f;
}

// Função que realiza raycast e verifica interseções com entidades
void CameraRaycast(glm::vec4 camera_position, glm::vec4 ray_direction)
{
    const float max_ray_distance = 100.0f;
    const float entity_radius = 0.3f; // Raio aproximado das entidades (baseado na escala)

    // Verifica interseção com caixas (obstáculos) e encontra a mais próxima
    float closest_box_t = max_ray_distance;
    bool hit_box = false;

    for (const auto& box : g_Boxes)
    {
        // Calcula o AABB da caixa no espaço do mundo
        // O cubo base tem coordenadas de -1 a 1, então precisamos escalar e transladar
        // Como a rotação é apenas em torno do eixo Y, o AABB em Y não muda,
        // mas em X e Z precisamos considerar a diagonal máxima após rotação
        float max_horizontal_extent = sqrt(box.scale.x * box.scale.x + box.scale.z * box.scale.z);
        
        glm::vec3 box_min = glm::vec3(
            box.position.x - max_horizontal_extent,
            box.position.y - box.scale.y,
            box.position.z - max_horizontal_extent
        );
        glm::vec3 box_max = glm::vec3(
            box.position.x + max_horizontal_extent,
            box.position.y + box.scale.y,
            box.position.z + max_horizontal_extent
        );

        float t_box = 0.0f;
        if (RayAABBIntersection(camera_position, ray_direction, box_min, box_max, t_box))
        {
            if (t_box > 0.0f && t_box < closest_box_t)
            {
                closest_box_t = t_box;
                hit_box = true;
            }
        }
    }

    // Verifica interseção com o jogador
    float closest_player_t = max_ray_distance;
    bool hit_player = false;
    
    glm::vec4 to_player = glm::vec4(
        g_Player.position.x - camera_position.x,
        g_Player.position.y - camera_position.y,
        g_Player.position.z - camera_position.z,
        0.0f
    );

    float t_player = glm::dot(glm::vec3(to_player), glm::vec3(ray_direction));

    if (t_player > 0.0f && t_player < max_ray_distance)
    {
        glm::vec4 closest_point = camera_position + ray_direction * t_player;
        glm::vec4 to_closest = closest_point - g_Player.position;
        float distance_sq = to_closest.x * to_closest.x + to_closest.y * to_closest.y + to_closest.z * to_closest.z;

        if (distance_sq <= entity_radius * entity_radius)
        {
            closest_player_t = t_player;
            hit_player = true;
        }
    }

    // Verifica interseção com inimigos
    const float player_damage_amount = 34.0f; // Quantidade de dano por tiro
    float closest_enemy_t = max_ray_distance;
    size_t closest_enemy_index = SIZE_MAX;
    bool hit_enemy = false;

    for (size_t i = 0; i < g_Enemies.size(); ++i)
    {
        auto& enemy = g_Enemies[i];

        // Pula inimigos que já estão mortos
        if (enemy.IsDead())
            continue;

        glm::vec4 to_enemy = glm::vec4(
            enemy.position.x - camera_position.x,
            enemy.position.y - camera_position.y,
            enemy.position.z - camera_position.z,
            0.0f
        );

        float t_enemy = glm::dot(glm::vec3(to_enemy), glm::vec3(ray_direction));

        if (t_enemy > 0.0f && t_enemy < max_ray_distance)
        {
            glm::vec4 closest_point = camera_position + ray_direction * t_enemy;
            glm::vec4 to_closest = closest_point - enemy.position;
            float distance_sq = to_closest.x * to_closest.x + to_closest.y * to_closest.y + to_closest.z * to_closest.z;

            if (distance_sq <= entity_radius * entity_radius)
            {
                if (t_enemy < closest_enemy_t)
                {
                    closest_enemy_t = t_enemy;
                    closest_enemy_index = i;
                    hit_enemy = true;
                }
            }
        }
    }

    // Verifica qual foi o hit mais próximo: caixa, jogador ou inimigo
    // A caixa bloqueia se estiver na frente de qualquer outro objeto
    if (hit_box)
    {
        // Verifica se a caixa está na frente do jogador
        if (hit_player && closest_box_t < closest_player_t)
        {
            printf("Raycast hit: BOX at distance %.2f (blocking player)\n", closest_box_t);
            return;
        }
        
        // Verifica se a caixa está na frente do inimigo
        if (hit_enemy && closest_box_t < closest_enemy_t)
        {
            printf("Raycast hit: BOX at distance %.2f (blocking enemy)\n", closest_box_t);
            return;
        }
        
        // Se não há jogador nem inimigo, ou se a caixa não está na frente, ainda pode ser o hit mais próximo
        if (!hit_player && !hit_enemy)
        {
            printf("Raycast hit: BOX at distance %.2f\n", closest_box_t);
            return;
        }
    }

    // Se o jogador foi atingido (e não há caixa na frente)
    if (hit_player && (!hit_box || closest_player_t < closest_box_t))
    {
        printf("Raycast hit: PLAYER at distance %.2f\n", closest_player_t);
        // Aqui você pode adicionar lógica adicional, como dano, etc.
        return;
    }

    // Se um inimigo foi atingido (e não há caixa na frente)
    if (hit_enemy && closest_enemy_index != SIZE_MAX && (!hit_box || closest_enemy_t < closest_box_t))
    {
        auto& enemy = g_Enemies[closest_enemy_index];
        
        // Aplica dano ao inimigo através do método TakeDamage
        enemy.TakeDamage(player_damage_amount);

        printf("Raycast hit: ENEMY %zu at distance %.2f - Health: %.1f/%.1f\n",
               closest_enemy_index, closest_enemy_t, enemy.health, enemy.max_health);

        // Se o inimigo morreu
        if (enemy.IsDead())
        {
            printf("ENEMY %zu DEFEATED!\n", closest_enemy_index);
        }

        return;
    }

    printf("Raycast: No hit\n");
}

// Realiza raycast a partir do centro do jogador na direção que ele está olhando
void PlayerRaycast()
{
    // Posição do centro do jogador (usando a posição do jogador)
    glm::vec4 ray_origin = g_Player.position;
    
    // Direção é o vetor forward do jogador (direção que ele está olhando)
    glm::vec4 ray_direction = g_Player.forward_vector;
    
    // Normaliza o vetor de direção
    float dir_length = sqrt(ray_direction.x * ray_direction.x + 
                           ray_direction.y * ray_direction.y + 
                           ray_direction.z * ray_direction.z);
    if (dir_length > 0.001f)
    {
        ray_direction.x /= dir_length;
        ray_direction.y /= dir_length;
        ray_direction.z /= dir_length;
    }
    
    printf("=== PlayerRaycast: From player center (%.2f, %.2f, %.2f) in direction (%.2f, %.2f, %.2f) ===\n",
           ray_origin.x, ray_origin.y, ray_origin.z,
           ray_direction.x, ray_direction.y, ray_direction.z);
    
    // Usa a mesma lógica do CameraRaycast mas com origem e direção diferentes
    CameraRaycast(ray_origin, ray_direction);
}

// Função auxiliar para desenhar linha de raycast (amarela)
void DrawRaycastLine(glm::vec4 start, glm::vec4 end, glm::mat4 view, glm::mat4 projection)
{
    // Cria VAO e VBO se ainda não existirem
    if (g_LineVAO == 0)
    {
        glGenVertexArrays(1, &g_LineVAO);
        glGenBuffers(1, &g_LineVBO);
    }

    // Define os vértices da linha
    float line_vertices[] = {
        start.x, start.y, start.z, 1.0f,
        end.x, end.y, end.z, 1.0f
    };

    // Configura o VAO
    glBindVertexArray(g_LineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_LineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    // Usa o shader principal
    glUseProgram(g_GpuProgramID);

    // Matriz de modelagem identidade (linha já está em coordenadas do mundo)
    glm::mat4 model = Matrix_Identity();
    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(projection));

    // Define cor amarela para raycast de inimigo
    #define ENEMY_RAYCAST_LINE 11
    glUniform1i(g_object_id_uniform, ENEMY_RAYCAST_LINE);

    // Desabilita culling para linhas
    glDisable(GL_CULL_FACE);

    // Desenha a linha
    glLineWidth(3.0f);
    glDrawArrays(GL_LINES, 0, 2);
    glLineWidth(1.0f);

    // Reabilita culling
    glEnable(GL_CULL_FACE);

    glBindVertexArray(0);
}

// Realiza raycast de um inimigo específico em direção ao jogador
void EnemyToPlayerRaycast(size_t enemy_index)
{
    // Verifica se o índice é válido
    if (enemy_index >= g_Enemies.size())
    {
        printf("EnemyToPlayerRaycast: Invalid enemy index %zu (total enemies: %zu)\n", 
               enemy_index, g_Enemies.size());
        return;
    }
    
    auto& enemy = g_Enemies[enemy_index];
    
    // Verifica se o inimigo está morto
    if (enemy.IsDead())
    {
        printf("EnemyToPlayerRaycast: Enemy %zu is dead\n", enemy_index);
        return;
    }
    
    // Posição do centro do inimigo
    glm::vec4 ray_origin = enemy.position;
    
    // Direção do inimigo para o jogador
    glm::vec4 ray_direction = glm::vec4(
        g_Player.position.x - enemy.position.x,
        g_Player.position.y - enemy.position.y,
        g_Player.position.z - enemy.position.z,
        0.0f
    );
    
    // Normaliza o vetor de direção
    float dir_length = sqrt(ray_direction.x * ray_direction.x + 
                           ray_direction.y * ray_direction.y + 
                           ray_direction.z * ray_direction.z);
    if (dir_length > 0.001f)
    {
        ray_direction.x /= dir_length;
        ray_direction.y /= dir_length;
        ray_direction.z /= dir_length;
    }
    else
    {
        printf("EnemyToPlayerRaycast: Enemy %zu is at same position as player\n", enemy_index);
        return;
    }
    
    // Rotaciona o inimigo para enfrentar o jogador
    // Usa atan2 para calcular o ângulo de rotação em torno do eixo Y
    // Similar ao que é feito para o jogador, mas com sinal invertido para corrigir direção
    enemy.rotation_y = atan2(ray_direction.x, -ray_direction.z);
    enemy.UpdateDirectionVectors();
    
    printf("=== EnemyToPlayerRaycast: From enemy %zu (%.2f, %.2f, %.2f) to player (%.2f, %.2f, %.2f) ===\n",
           enemy_index,
           ray_origin.x, ray_origin.y, ray_origin.z,
           g_Player.position.x, g_Player.position.y, g_Player.position.z);
    
    // Realiza o raycast e encontra o ponto de impacto
    const float max_ray_distance = 100.0f;
    glm::vec4 hit_point = ray_origin + ray_direction * max_ray_distance; // Default: max distance
    
    // Verifica interseção com caixas primeiro
    float closest_box_t = max_ray_distance;
    bool hit_box = false;
    
    for (const auto& box : g_Boxes)
    {
        float max_horizontal_extent = sqrt(box.scale.x * box.scale.x + box.scale.z * box.scale.z);
        glm::vec3 box_min = glm::vec3(
            box.position.x - max_horizontal_extent,
            box.position.y - box.scale.y,
            box.position.z - max_horizontal_extent
        );
        glm::vec3 box_max = glm::vec3(
            box.position.x + max_horizontal_extent,
            box.position.y + box.scale.y,
            box.position.z + max_horizontal_extent
        );
        
        float t_box = 0.0f;
        if (RayAABBIntersection(ray_origin, ray_direction, box_min, box_max, t_box))
        {
            if (t_box > 0.0f && t_box < closest_box_t)
            {
                closest_box_t = t_box;
                hit_box = true;
            }
        }
    }
    
    // Verifica interseção com o jogador
    const float entity_radius = 0.3f;
    float closest_player_t = max_ray_distance;
    bool hit_player = false;
    
    glm::vec4 to_player = glm::vec4(
        g_Player.position.x - ray_origin.x,
        g_Player.position.y - ray_origin.y,
        g_Player.position.z - ray_origin.z,
        0.0f
    );
    
    float t_player = glm::dot(glm::vec3(to_player), glm::vec3(ray_direction));
    
    if (t_player > 0.0f && t_player < max_ray_distance)
    {
        glm::vec4 closest_point = ray_origin + ray_direction * t_player;
        glm::vec4 to_closest = closest_point - g_Player.position;
        float distance_sq = to_closest.x * to_closest.x + to_closest.y * to_closest.y + to_closest.z * to_closest.z;
        
        if (distance_sq <= entity_radius * entity_radius)
        {
            closest_player_t = t_player;
            hit_player = true;
        }
    }
    
    // Determina o ponto de impacto mais próximo
    if (hit_box && (!hit_player || closest_box_t < closest_player_t))
    {
        hit_point = ray_origin + ray_direction * closest_box_t;
    }
    else if (hit_player)
    {
        hit_point = ray_origin + ray_direction * closest_player_t;
    }
    
    // Usa a mesma lógica do CameraRaycast para aplicar dano, etc.
    CameraRaycast(ray_origin, ray_direction);
    
    // Armazena informações do raycast para desenhar a linha amarela
    g_EnemyRaycastStart = ray_origin;
    g_EnemyRaycastEnd = hit_point;
    g_EnemyRaycastTime = (float)glfwGetTime(); // Registra o tempo atual
    g_DrawEnemyRaycast = true;
}

// Spawna uma wave de monstros nas posições especificadas
// Retorna o ID da wave criada
int SpawnWave(const std::vector<glm::vec4>& spawn_positions)
{
    int wave_id = g_NextWaveID++;
    Wave new_wave(wave_id);

    // Cria os inimigos e adiciona à lista global
    for (const auto& pos : spawn_positions)
    {
        size_t enemy_index = g_Enemies.size();
        g_Enemies.push_back(Enemy(pos, wave_id));
        new_wave.enemy_indices.push_back(enemy_index);
    }

    g_Waves.push_back(new_wave);

    printf("Wave %d spawned with %zu enemies\n", wave_id, spawn_positions.size());
    return wave_id;
}

// Verifica se todos os monstros de uma wave estão mortos
bool IsWaveComplete(int wave_id)
{
    for (auto& wave : g_Waves)
    {
        if (wave.wave_id == wave_id)
        {
            return wave.CheckCompletion(g_Enemies);
        }
    }
    return false; // Wave não encontrada
}

// Atualiza o status de todas as waves
void UpdateWaves()
{
    for (auto& wave : g_Waves)
    {
        if (wave.is_active && !wave.is_complete)
        {
            if (wave.CheckCompletion(g_Enemies))
            {
                printf("Wave %d COMPLETE! All enemies defeated!\n", wave.wave_id);
            }
        }
    }
}

// Retorna os IDs de todas as waves ativas (ainda não completas)
std::vector<int> GetActiveWaves()
{
    std::vector<int> active_waves;
    for (const auto& wave : g_Waves)
    {
        if (wave.is_active && !wave.is_complete)
        {
            active_waves.push_back(wave.wave_id);
        }
    }
    return active_waves;
}

// Retorna os IDs de todas as waves completas
std::vector<int> GetCompleteWaves()
{
    std::vector<int> complete_waves;
    for (const auto& wave : g_Waves)
    {
        if (wave.is_complete)
        {
            complete_waves.push_back(wave.wave_id);
        }
    }
    return complete_waves;
}

// set makeprg=cd\ ..\ &&\ make\ run\ >/dev/null
// vim: set spell spelllang=pt_br :

