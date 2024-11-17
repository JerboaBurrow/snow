#ifndef MAIN_H
#define MAIN_H

#include <jGL/jGL.h>
#include <jGL/OpenGL/openGLInstance.h>
#include <jGL/OpenGL/Shader/glShader.h>
#include <jGL/shape.h>

#include <logo.h>
#include <jGL/Display/desktopDisplay.h>
#include <jGL/orthoCam.h>

#include <jLog/jLog.h>

#include <rand.h>
#include <algorithm>
#include <chrono>
#include <sstream>

#include <glCompute.h>

using namespace std::chrono;

int resX = 1024;
int resY = 1024;

int cells = 256;

uint8_t frameId = 0;
double deltas[60];

bool debug = false;
bool paused = false;

std::unique_ptr<jGL::jGLInstance> jGLInstance;

std::string fixedLengthNumber(double x, unsigned length)
{
    std::string d = std::to_string(x);
    std::string dtrunc(length,' ');
    for (unsigned c = 0; c < dtrunc.length(); c++/*ayy lmao*/)
    {

        if (c >= d.length())
        {
            dtrunc[c] = '0';
        }
        else
        {
            dtrunc[c] = d[c];
        }
    }
    return dtrunc;
}

struct Visualise
{
    Visualise(GLuint particlesTexture, GLuint obstaclesTexture)
    : particlesTexture(particlesTexture), obstaclesTexture(obstaclesTexture)
    {
        glGenVertexArrays(1, &pvao);
        glBindVertexArray(pvao);
        glGenBuffers(1, &pvbo);
        glBindBuffer(GL_ARRAY_BUFFER, pvbo);
        glBufferData
        (
            GL_ARRAY_BUFFER,
            sizeof(float)*2,
            &p[0],
            GL_STATIC_DRAW
        );
        glEnableVertexAttribArray(0);
        glVertexAttribPointer
        (
            0,
            2,
            GL_FLOAT,
            false,
            2*sizeof(float),
            0
        );
        glVertexAttribDivisor(0,0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        glGenVertexArrays(1, &qvao);
        glBindVertexArray(qvao);
        glGenBuffers(1, &qvbo);
        glBindBuffer(GL_ARRAY_BUFFER, qvbo);
        glBufferData
        (
            GL_ARRAY_BUFFER,
            sizeof(float)*6*4,
            &quad[0],
            GL_STATIC_DRAW
        );
        glEnableVertexAttribArray(0);
        glVertexAttribPointer
        (
            0,
            4,
            GL_FLOAT,
            false,
            4*sizeof(float),
            0
        );
        glVertexAttribDivisor(0,0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glGenFramebuffers(1, &frameBuffer);
    }

    void drawParticles(uint64_t particles, float scale, glm::mat4 proj)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, particlesTexture);
        shader = jGL::GL::glShader(vertexShader, fragmentShader);
        shader.compile();
        shader.use();
        shader.setUniform("tex", jGL::Sampler2D(1));
        shader.setUniform("proj", proj);

        glBindVertexArray(qvao);
        glBindBuffer(GL_ARRAY_BUFFER, qvbo);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    void drawObstacles(uint64_t obstacles, float scale, glm::mat4 proj)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, obstaclesTexture);
        shader = jGL::GL::glShader(vertexShader, fragmentShader);
        shader.compile();
        shader.use();
        shader.setUniform("tex", jGL::Sampler2D(1));
        shader.setUniform("proj", proj);

        glBindVertexArray(qvao);
        glBindBuffer(GL_ARRAY_BUFFER, qvbo);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    jGL::GL::glShader shader;
    GLuint particlesTexture, obstaclesTexture, pvao, pvbo, qvao, qvbo, frameBuffer;
    float p[2] =
    {
        0.0f,0.0f
    };

        float quad[6*4] =
    {
        -1.0, -1.0, 0.0, 0.0,
         1.0, -1.0, 1.0, 0.0,
         1.0,  1.0, 1.0, 1.0,
        -1.0, -1.0, 0.0, 0.0,
        -1.0,  1.0, 0.0, 1.0,
         1.0,  1.0, 1.0, 1.0
    };

    const char * vertexShader =
    "#version " GLSL_VERSION "\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "layout(location = 0) in vec4 a_position;\n"
    "uniform mat4 proj;\n"
    "out vec2 o_texCoords;\n"
    "void main(){\n"
    "   gl_Position = vec4(a_position.xy,0.0,1.0);\n"
    "   o_texCoords = vec2(a_position.z, 1.0-a_position.w);\n"
    "}";

    const char * fragmentShader =
    "#version " GLSL_VERSION "\n"
    "uniform highp sampler2D tex;\n"
    "in vec2 o_texCoords;\n"
    "out vec4 colour;\n"
    "void main(void){\n"
    "   vec4 t = texture(tex, o_texCoords);\n"
    "   if (t.r == 0) { discard; }\n"
    "   colour = vec4(1.0,1.0,1.0,t.r);\n"
    "}";
};

const char * toMargolusShader =
    "#version " GLSL_VERSION "\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "in vec2 o_texCoords;\n"
    "layout(location=0) out vec4 output;\n"
    "uniform highp sampler2D cells;\n"
    "uniform highp sampler2D noise;\n"
    "uniform int width;\n"
    "uniform int type;"
    "uniform float spawnProb;\n"
    "uniform float seed;\n"
    "float random(vec2 st){\n"
    "    return clamp(fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123), 0.000000001, 1.0);\n"
    "}\n"
    "int get(ivec2 coords){\n"
    "    ivec2 c = coords % width;\n"
    "    if (float(c.y) < float(width)*0.5) {"
    "       float rng = random(vec2(texture(noise, c).r+seed, texture(noise, c).r-seed));\n"
    "       if (rng < spawnProb/(1.0-float(c.y))) { return 1; }\n"
    "    }"
    "    return int(texture(cells, vec2(float(c.x)/float(width), float(c.y)/float(width))).r);\n"
    "}"
    "void main(){\n"
    "    int mwidth = int(float(width)/2.0);"
    "    int i = int(o_texCoords.x*float(mwidth)); int j = int(o_texCoords.y*float(mwidth));\n"
    "    ivec2 offset = ivec2(type, type);\n"
    "    int hash = get(ivec2(2*i, 2*j)+offset) + "
    "               get(ivec2(2*i+1, 2*j)+offset)*2 + "
    "               get(ivec2(2*i, 2*j+1)+offset)*4 + "
    "               get(ivec2(2*i+1, 2*j+1)+offset)*8;\n"
    "    int wall = 0;\n"
    "    if (2*i+1 >= width-1 && 2*j+1 < width-1){ wall += 16; }\n"
    "    else if (2*i+1 < width-1 && 2*j+1 >= width-1){ wall += 32; }\n"
    "    else if (2*i+1 >= width-1 && 2*j+1 >= width-1){ wall += 48; }\n"
    "    output = vec4(hash+wall);\n"
    "}";

const char * fromMargolusShader =
    "#version " GLSL_VERSION "\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "in vec2 o_texCoords;\n"
    "layout(location=0) out vec4 output;\n"
    "uniform highp sampler2D margolus;\n"
    "uniform int width;\n"
    "uniform int type;"
    "vec4 map[16] = vec4[]( vec4(0.0,0.0,0.0,0.0), vec4(1.0,0.0,0.0,0.0), vec4(0.0,1.0,0.0,0.0),\n"
    "                       vec4(1.0,1.0,0.0,0.0), vec4(0.0,0.0,1.0,0.0), vec4(1.0,0.0,1.0,0.0),\n"
    "                       vec4(0.0,1.0,1.0,0.0), vec4(1.0,1.0,1.0,0.0), vec4(0.0,0.0,0.0,1.0),\n"
    "                       vec4(1.0,0.0,0.0,1.0), vec4(0.0,1.0,0.0,1.0), vec4(1.0,1.0,0.0,1.0),\n"
    "                       vec4(0.0,0.0,1.0,1.0), vec4(1.0,0.0,1.0,1.0), vec4(0.0,1.0,1.0,1.0),\n"
    "                       vec4(1.0,1.0,1.0,1.0));\n"
    "void main(){\n"
    "    float mwidth = width/2.0;\n"
    "    int i = int(o_texCoords.x*float(width)); int j = int(o_texCoords.y*float(width));\n"
    "    vec2 coord = mod((vec2(float(i)-float(type), float(j)-float(type))/2.0)/float(mwidth), 1.0);\n"
    "    vec4 block = map[int(texture(margolus, coord).r)%16];\n"
    "    int bx = abs(i-type) % 2; int by = abs(j-type) % 2;\n"
    "    if (bx == 0 && by == 0) { output = vec4(block.x); }"
    "    else if (bx == 1 && by == 0) { output = vec4(block.y); }"
    "    else if (bx == 0 && by == 1) { output = vec4(block.z); }"
    "    else if (bx == 1 && by == 1) { output = vec4(block.w); }"
    "}";

const char * blockCAComputeShader =
    "#version " GLSL_VERSION "\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "in vec2 o_texCoords;\n"
    "layout(location=0) out vec4 output;\n"
    "uniform highp sampler2D cells;\n"
    "uniform highp sampler2D noise;\n"
    "uniform highp sampler2D margolus;\n"
    "uniform highp sampler2D obstacles;\n"
    "uniform float reset;\n"
    "uniform vec2 seed;\n"
    "uniform float p1; uniform float p2; uniform float p31;\n"
    "uniform float p32; uniform float p6; uniform float p7; uniform float p9; uniform float p11;\n"
    "float random(vec2 st){\n"
    "    return clamp(fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123), 0.001, 1.0);\n"
    "}\n"
    "void main(){\n"
    "    int hash = int(texture(margolus, o_texCoords).r);\n"
    "    bool wallx = false; bool wally = false;"
    "    if (hash >= 16 && hash < 32) { wallx = true; }\n"
    "    else if (hash >= 32 && hash < 48) { wally = true; }\n"
    "    else if (hash >= 48) { wallx = true; wally = true; }\n"
    "    hash = hash % 16;\n"
    "    float d = random(seed+texture(noise, o_texCoords).r*o_texCoords);\n"
    "    output = vec4(hash);\n"
    "    if (!wally && hash == 1) { if (!wallx && d<p1) { output = vec4(4); } else { output = vec4(8); } }"
    "    else if (!wally && hash == 2) { if (d<p2) { output = vec4(8); } else { output = vec4(4); } }"
    "    else if (!wally && hash == 3) { if (d<p31) { output = vec4(3); } else { if (d<p32){ output = vec4(10); } else { output = vec4(5); } } }"
    "    else if (hash == 5) { output = vec4(12); }"
    "    else if (hash == 6) { if (d<p6) { output = vec4(12); } else { output = vec4(6); } }"
    "    else if (hash == 7) { if (d<p7) { output = vec4(7); } else { output = vec4(14); } }"
    "    else if (!wallx && hash == 9) { if (d<p9) { output = vec4(12); } else { output = vec4(9); } }"
    "    else if (hash == 10) { output = vec4(12); }"
    "    else if (hash == 11) { if (d<p11) { output = vec4(11); } else { output = vec4(13); } }"
    "}";

float clamp(float x, float low, float high)
{
    return std::min(std::max(x, low), high);
}

float poly(float x, float p0, float p1, float p2, float p3, float p4)
{
   float x2 = x*x; float x4 = x2*x2; float x3 = x2*x;
   return clamp(p0+p1*x+p2*x2+p3*x3+p4*x4,0.0,1.0);
}

glm::vec3 cmap(float t)
{
    return glm::vec3( poly(t,0.91, 3.74, -32.33, 57.57, -28.99), poly(t,0.2, 5.6, -18.89, 25.55, -12.25), poly(t,0.22, -4.89, 22.31, -23.58, 5.97) );
}

void placeOrRemove(std::vector<float> & into, int i, int j, int brush, int l, float value)
{
    for (int n = -brush; n <= brush; n++)
    {
        for (int m = -brush; m <= brush; m++)
        {
            int ix = (n+i) % l;
            int iy = (m+j) % l;
            if (ix < 0) { ix += l; }
            if (iy < 0) { iy += l; }
            into[iy*l+ix] = value;
        }
    }
}

#endif /* MAIN_H */
