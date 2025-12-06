#version 330 core

// Atributos de fragmentos recebidos como entrada ("in") pelo Fragment Shader.
// Neste exemplo, este atributo foi gerado pelo rasterizador como a
// interpolação da posição global e a normal de cada vértice, definidas em
// "shader_vertex.glsl" e "main.cpp".
in vec4 position_world;
in vec4 normal;

// Posição do vértice atual no sistema de coordenadas local do modelo.
in vec4 position_model;

// Coordenadas de textura obtidas do arquivo OBJ (se existirem!)
in vec2 texcoords;

in vec3 gouraud_color;
uniform int shading_mode;

// Matrizes computadas no código C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Identificador que define qual objeto está sendo desenhado no momento
#define PLANE  0
#define PLAYER 1
#define ENEMY  2
#define BOX    10
uniform int object_id;

// Parâmetros da axis-aligned bounding box (AABB) do modelo
uniform vec4 bbox_min;
uniform vec4 bbox_max;

// Variáveis para acesso das imagens de textura
uniform sampler2D TextureImage0;
uniform sampler2D TextureImage1;
uniform sampler2D TextureImage2;
uniform int use_texture;

// O valor de saída ("out") de um Fragment Shader é a cor final do fragmento.
out vec4 color;

// Constantes
#define M_PI   3.14159265358979323846
#define M_PI_2 1.57079632679489661923

void main()
{
    // Obtemos a posição da câmera utilizando a inversa da matriz que define o
    // sistema de coordenadas da câmera.
    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;

    // O fragmento atual é coberto por um ponto que percente à superfície de um
    // dos objetos virtuais da cena. Este ponto, p, possui uma posição no
    // sistema de coordenadas global (World coordinates). Esta posição é obtida
    // através da interpolação, feita pelo rasterizador, da posição de cada
    // vértice.
    vec4 p = position_world;

    // Normal do fragmento atual, interpolada pelo rasterizador a partir das
    // normais de cada vértice.
    vec4 n = normalize(normal);

    // Vetor que define o sentido da fonte de luz em relação ao ponto atual.
    vec4 l = normalize(vec4(1.0,1.0,0.0,0.0));

    // Vetor que define o sentido da câmera em relação ao ponto atual.
    vec4 v = normalize(camera_position - p);

    // Coordenadas de textura U e V
    float U = 0.0;
    float V = 0.0;

    // ------------------------------
    // SE MODO = GOURAUD → usa cor pronta
    // ------------------------------
    if (shading_mode == 1)
    {
        // para objetos com textura
        vec3 texcolor = vec3(1.0);
        if (use_texture == 1)
            texcolor = texture(TextureImage0, texcoords).rgb;
    
        color.rgb = texcolor * gouraud_color;
        color.a = 1.0;
        color.rgb = pow(color.rgb, vec3(1.0/2.2));
        return;
    }

    if (object_id == PLANE)
    {
        float TILE = 10.0;
        vec2 uv = fract(texcoords * TILE);
        vec3 texcolor = texture(TextureImage0, uv).rgb;

        float lambert = max(0.0, dot(n, l));
        color.rgb = texcolor * (lambert + 0.2);
        return;
    }

    // Tratamento especial para linhas de direção, crosshair e barras de vida
    if ( object_id == 3 ) // DIRECTION_LINE_PLAYER - linha verde para player
    {
        // Cor verde para indicadores de direção do player
        color.rgb = vec3(0.0, 1.0, 0.0);
    }
    else if ( object_id == 4 ) // CROSSHAIR - verde
    {
        // Cor verde para crosshair
        color.rgb = vec3(0.0, 1.0, 0.0);
    }
    else if ( object_id == 5 ) // DIRECTION_LINE_ENEMY - linha vermelha para inimigos
    {
        // Cor vermelha para indicadores de direção dos inimigos
        color.rgb = vec3(1.0, 0.0, 0.0);
    }
    else if ( object_id == 6 ) // CROSSHAIR_OUTLINE - contorno escuro
    {
        // Cor escura para contorno do crosshair
        color.rgb = vec3(0.0, 0.0, 0.0);
    }
    else if ( object_id == 7 ) // HEALTH_BAR_OUTLINE - contorno escuro da barra de vida
    {
        // Cor escura para contorno da barra de vida
        color.rgb = vec3(0.0, 0.0, 0.0);
    }
    else if ( object_id == 8 ) // HEALTH_BAR_BACKGROUND - fundo cinza (HP faltando)
    {
        // Cor cinza para HP faltando
        color.rgb = vec3(0.5, 0.5, 0.5);
    }
    else if ( object_id == 9 ) // HEALTH_BAR_FILL - barra verde (HP atual)
    {
        // Cor verde para HP atual
        color.rgb = vec3(0.0, 1.0, 0.0);
    }
    else if ( object_id == 11 ) // ENEMY_RAYCAST_LINE - linha amarela para raycast de inimigo
    {
        // Cor amarela para raycast de inimigo
        color.rgb = vec3(1.0, 1.0, 0.0);
    }
    else if ( object_id == 12 ) // ENEMY_HITBOX - hitbox do inimigo (ciano)
    {
        // Cor ciano para hitbox do inimigo
        color.rgb = vec3(0.0, 1.0, 1.0);
    }
    else if ( object_id == 13 ) // BEZIER_SPLINE - spline Bezier (roxa/magenta)
    {
        // Cor roxa/magenta para spline Bezier
        color.rgb = vec3(1.0, 0.0, 1.0);
    }
    else if ( object_id == 14 ) // PLAYER_HITBOX - hitbox do jogador (verde)
    {
        // Cor verde para hitbox do jogador
        color.rgb = vec3(0.0, 1.0, 0.0);
    }
    else if (object_id == PLAYER)
    {
        // Difusa (Lambert) a partir da textura
        vec3 kd = texture(TextureImage0, texcoords).rgb;

        // Normaliza vetores que já existem no shader
        vec3 N = normalize(n.xyz);
        vec3 L = normalize(l.xyz);
        vec3 V = normalize(v.xyz);

        // Componente difusa (Lambert)
        float lambert = max(dot(N, L), 0.0);

        // Half-vector para Blinn-Phong
        vec3 H = normalize(L + V);

        // Componente especular Blinn-Phong
        float shininess = 32.0;          // “brilho” do material
        float spec = pow(max(dot(N, H), 0.0), shininess);
        vec3 ks = vec3(0.25);            // intensidade especular

        // Cor final: difusa + especular + um ambientezinho
        color.rgb = kd * (lambert + 0.2) + ks * spec;
    }
    else if (object_id == ENEMY)
    {
        // UV REAL do OBJ
        vec2 uv = texcoords;
        if (uv == vec2(0.0, 0.0))
            uv = vec2(0.5, 0.5);

        // kd: cor difusa (da textura ou fallback)
        vec3 kd;
        if (use_texture == 1)
            kd = texture(TextureImage0, uv).rgb;
        else
            kd = vec3(0.6, 0.5, 0.4);  // cor “marrom” pros materiais sem textura

        // Normaliza vetores já existentes
        vec3 N = normalize(n.xyz);
        vec3 L = normalize(l.xyz);
        vec3 V = normalize(v.xyz);

        // Difusa
        float lambert = max(dot(N, L), 0.0);

        // Half-vector Blinn-Phong
        vec3 H = normalize(L + V);
        float shininess = 16.0;        // pode ser menor que o do player
        float spec = pow(max(dot(N, H), 0.0), shininess);
        vec3 ks = vec3(0.20);          // especular um pouco mais fraca

        color.rgb = kd * (lambert + 0.2) + ks * spec;
    }

    else if (object_id == BOX)
    {
        // Cor marrom/amadeirada para caixas e barrils
        vec3 box_color = vec3(0.6, 0.4, 0.2); // Marrom/amadeirado
        float lambert = max(0, dot(n,l));
        color.rgb = box_color * (lambert + 0.2);
    }
    else
    {
        vec3 neutral_color = vec3(0.75, 0.75, 0.8);
        float lambert = max(0,dot(n,l));
        color.rgb = neutral_color * (lambert + 0.1);
    }


    // NOTE: Se você quiser fazer o rendering de objetos transparentes, é
    // necessário:
    // 1) Habilitar a operação de "blending" de OpenGL logo antes de realizar o
    //    desenho dos objetos transparentes, com os comandos abaixo no código C++:
    //      glEnable(GL_BLEND);
    //      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // 2) Realizar o desenho de todos objetos transparentes *após* ter desenhado
    //    todos os objetos opacos; e
    // 3) Realizar o desenho de objetos transparentes ordenados de acordo com
    //    suas distâncias para a câmera (desenhando primeiro objetos
    //    transparentes que estão mais longe da câmera).
    // Alpha default = 1 = 100% opaco = 0% transparente
    color.a = 1;

    // Cor final com correção gamma, considerando monitor sRGB.
    // Veja https://en.wikipedia.org/w/index.php?title=Gamma_correction&oldid=751281772#Windows.2C_Mac.2C_sRGB_and_TV.2Fvideo_standard_gammas
    color.rgb = pow(color.rgb, vec3(1.0,1.0,1.0)/2.2);
} 
