// ================================================================
// OpenGL 1st Assignment - Exploring OpenGL
// TVG Genap 2025-2026
//
// Implementasi:
//   Task 2: Vertex & Fragment Shader (Phong Model) + Material
//   Task 3: Load OBJ model dengan VBO & VAO
//   Task 4: Model, View, Projection Matrix
//
// Kontrol:
//   W/A/S/D     - Gerak kamera
//   Mouse       - Lihat sekitar
//   1           - Material: Gold
//   2           - Material: Silver
//   3           - Material: Bronze
//   4           - Material: Rubber (matte)
//   5           - Material: Texture (pakai PNG asli)
//   Scroll/Z/X  - Zoom in/out
//   R           - Reset kamera ke tampilan awal
//   ESC         - Keluar
// ================================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// GLM: Library matematika untuk OpenGL (vector, matrix, dll)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// STB Image: single-header library untuk membaca tekstur PNG/JPG.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

// ================================================================
// Pengaturan window
// ================================================================
const unsigned int SCR_WIDTH  = 1280;
const unsigned int SCR_HEIGHT = 720;

// ================================================================
// State kamera (global untuk callback)
// ================================================================
const glm::vec3 INITIAL_CAMERA_POS   = glm::vec3(0.0f, 0.4f, 4.0f);
const glm::vec3 INITIAL_CAMERA_FRONT = glm::normalize(glm::vec3(0.0f, -0.1f, -1.0f));
const float INITIAL_YAW = -90.0f;
const float INITIAL_PITCH = -5.7f;
const float INITIAL_ZOOM_FOV = 35.0f;

glm::vec3 cameraPos   = INITIAL_CAMERA_POS;
glm::vec3 cameraFront = INITIAL_CAMERA_FRONT;
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = INITIAL_YAW, pitch = INITIAL_PITCH;
float lastX = SCR_WIDTH / 2.0f, lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float zoomFov = INITIAL_ZOOM_FOV;

// Titik tengah dan skala model TungTungTungSahur.obj agar tampil jelas di awal.
const glm::vec3 MODEL_CENTER = glm::vec3(-0.0067575f, 0.909096f, 0.389391f);
const float MODEL_SCALE = 1.35f;

// Timing (untuk smooth movement)
float deltaTime = 0.0f, lastFrame = 0.0f;

// Material aktif saat ini
int currentMaterial = 4; // Default: tampilkan tekstur

// ================================================================
// Struktur data untuk menyimpan vertex
// ================================================================
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

// ================================================================
// Struktur Mesh: menyimpan data + handle GPU (VAO, VBO, EBO)
// ================================================================
struct Mesh {
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    unsigned int VAO = 0, VBO = 0, EBO = 0;

    // -----------------------------------------------------------------
    // Task 3: Setup VAO & VBO - Upload data vertex ke GPU
    // -----------------------------------------------------------------
    void setupMesh() {
        // 1. Buat dan bind VAO (Vertex Array Object)
        //    VAO menyimpan "cara baca" data dari VBO
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        // 2. Buat dan isi VBO (Vertex Buffer Object)
        //    VBO menyimpan data vertex aktual di GPU
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     (GLsizei)(vertices.size() * sizeof(Vertex)),
                     vertices.data(),
                     GL_STATIC_DRAW);

        // 3. Buat dan isi EBO (Element Buffer Object)
        //    EBO menyimpan indeks untuk indexed drawing
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     (GLsizei)(indices.size() * sizeof(unsigned int)),
                     indices.data(),
                     GL_STATIC_DRAW);

        // 4. Definisikan layout atribut vertex
        //    Ini memberitahu OpenGL bagaimana membaca data dari VBO

        // Atribut 0: Position (vec3) - offset 0
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);

        // Atribut 1: Normal (vec3) - offset setelah Position
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Normal));
        glEnableVertexAttribArray(1);

        // Atribut 2: TexCoords (vec2) - offset setelah Normal
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, TexCoords));
        glEnableVertexAttribArray(2);

        // Unbind VAO (best practice)
        glBindVertexArray(0);

        std::cout << "[VAO/VBO] Setup selesai: "
                  << vertices.size() << " vertices, "
                  << indices.size() / 3 << " triangles\n";
    }
};

// ================================================================
// Task 2b: Material presets
// Setiap material punya sifat cahaya berbeda
// ================================================================
struct Material {
    std::string name;
    glm::vec3   ambient;     // Warna ambient  (r,g,b) 0.0-1.0
    glm::vec3   diffuse;     // Warna diffuse  (r,g,b)
    glm::vec3   specular;    // Warna specular (r,g,b)
    float       shininess;   // Kilap: 1=matte, 128=sangat mengkilap
};

struct FaceIndex {
    int vertex = -1;
    int texcoord = -1;
    int normal = -1;
};

// Preset material standar (dari tabel Phong material umum)
std::vector<Material> materials = {
    // name       ambient                    diffuse                     specular                    shininess
    {"Gold",    {0.247f, 0.199f, 0.074f}, {0.752f, 0.606f, 0.226f}, {0.628f, 0.556f, 0.366f},   51.2f},
    {"Silver",  {0.192f, 0.192f, 0.192f}, {0.508f, 0.508f, 0.508f}, {0.508f, 0.508f, 0.508f},   51.2f},
    {"Bronze",  {0.213f, 0.128f, 0.054f}, {0.714f, 0.428f, 0.181f}, {0.394f, 0.272f, 0.167f},   25.6f},
    {"Rubber",  {0.02f,  0.02f,  0.02f }, {0.04f,  0.04f,  0.04f }, {0.04f,  0.04f,  0.04f },    5.0f},
    {"Texture", {0.3f,   0.3f,   0.3f  }, {1.0f,   1.0f,   1.0f  }, {0.5f,   0.5f,   0.5f  },   32.0f},
};

// ================================================================
// Helper: Baca dan compile shader dari file
// ================================================================
unsigned int compileShader(const char* vertPath, const char* fragPath) {
    // Baca source code dari file
    auto readFile = [](const char* path) -> std::string {
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cerr << "[Shader] Tidak bisa buka: " << path << "\n";
            return "";
        }
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    };

    std::string vertSrc = readFile(vertPath);
    std::string fragSrc = readFile(fragPath);
    const char* vCode   = vertSrc.c_str();
    const char* fCode   = fragSrc.c_str();

    int  success;
    char log[512];

    // Compile vertex shader
    unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vCode, NULL);
    glCompileShader(vert);
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vert, 512, NULL, log);
        std::cerr << "[Vertex Shader Error]\n" << log << "\n";
    } else {
        std::cout << "[Shader] Vertex shader OK\n";
    }

    // Compile fragment shader
    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fCode, NULL);
    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(frag, 512, NULL, log);
        std::cerr << "[Fragment Shader Error]\n" << log << "\n";
    } else {
        std::cout << "[Shader] Fragment shader OK\n";
    }

    // Link menjadi shader program
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(prog, 512, NULL, log);
        std::cerr << "[Program Link Error]\n" << log << "\n";
    } else {
        std::cout << "[Shader] Program linked OK\n";
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

// ================================================================
// Helper: Load tekstur dari file gambar
// ================================================================
unsigned int loadTexture(const char* path) {
    unsigned int texID;
    glGenTextures(1, &texID);

    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);

    if (data) {
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Filter dan wrapping
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        std::cout << "[Texture] Loaded: " << path
                  << " (" << w << "x" << h << ", " << channels << " ch)\n";
    } else {
        std::cerr << "[Texture] Gagal load: " << path << "\n";

        unsigned char fallback[] = {
            255, 255, 255, 255,
            255,   0, 255, 255,
            255,   0, 255, 255,
            255, 255, 255, 255
        };
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, fallback);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    stbi_image_free(data);
    return texID;
}

FaceIndex parseFaceIndex(const std::string& token) {
    FaceIndex out;
    std::array<int*, 3> targets = {&out.vertex, &out.texcoord, &out.normal};
    size_t start = 0;

    for (size_t part = 0; part < targets.size() && start <= token.size(); ++part) {
        size_t slash = token.find('/', start);
        std::string value = token.substr(start, slash - start);
        if (!value.empty()) {
            *targets[part] = std::atoi(value.c_str()) - 1;
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }

    return out;
}

Vertex makeVertex(const FaceIndex& index,
                  const std::vector<glm::vec3>& positions,
                  const std::vector<glm::vec3>& normals,
                  const std::vector<glm::vec2>& texcoords) {
    Vertex vertex{};

    if (index.vertex >= 0 && index.vertex < (int)positions.size()) {
        vertex.Position = positions[(size_t)index.vertex];
    }

    if (index.normal >= 0 && index.normal < (int)normals.size()) {
        vertex.Normal = normals[(size_t)index.normal];
    } else {
        vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    if (index.texcoord >= 0 && index.texcoord < (int)texcoords.size()) {
        vertex.TexCoords = texcoords[(size_t)index.texcoord];
    } else {
        vertex.TexCoords = glm::vec2(0.0f);
    }

    return vertex;
}

// ================================================================
// Task 3: Load OBJ file dan buat Mesh dengan VBO & VAO
// ================================================================
Mesh loadOBJ(const std::string& path) {
    Mesh mesh;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[OBJ] Tidak bisa buka: " << path << "\n";
        return mesh;
    }

    std::cout << "[OBJ] Loading: " << path << "\n";

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v") {
            glm::vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        } else if (tag == "vn") {
            glm::vec3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            normals.push_back(glm::normalize(normal));
        } else if (tag == "vt") {
            glm::vec2 texcoord;
            ss >> texcoord.x >> texcoord.y;
            texcoords.push_back(texcoord);
        } else if (tag == "f") {
            std::vector<FaceIndex> face;
            std::string token;

            while (ss >> token) {
                face.push_back(parseFaceIndex(token));
            }

            // Triangulasi fan: mendukung face segitiga maupun quad dari Blender.
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                std::array<FaceIndex, 3> triangle = {face[0], face[i], face[i + 1]};
                for (const FaceIndex& index : triangle) {
                    mesh.vertices.push_back(makeVertex(index, positions, normals, texcoords));
                    mesh.indices.push_back((unsigned int)mesh.indices.size());
                }
            }
        }
    }

    std::cout << "[OBJ] Positions: " << positions.size()
              << ", Normals: " << normals.size()
              << ", TexCoords: " << texcoords.size() << "\n";

    // Upload ke GPU (VAO & VBO)
    mesh.setupMesh();
    return mesh;
}

// ================================================================
// Callback: Resize window
// ================================================================
void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

// ================================================================
// Callback: Mouse untuk kamera FPS
// ================================================================
void mouse_callback(GLFWwindow*, double xIn, double yIn) {
    float x = (float)xIn, y = (float)yIn;
    if (firstMouse) { lastX = x; lastY = y; firstMouse = false; }

    float sens = 0.1f;
    yaw   += (x - lastX) * sens;
    pitch += (lastY - y) * sens; // reversed Y
    lastX = x; lastY = y;

    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    cameraFront = glm::normalize(glm::vec3(
        cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(pitch)),
        sin(glm::radians(yaw)) * cos(glm::radians(pitch))
    ));
}

// ================================================================
// Callback: Scroll mouse untuk zoom in / zoom out
// ================================================================
void scroll_callback(GLFWwindow*, double, double yOffset) {
    zoomFov -= (float)yOffset * 2.5f;
    zoomFov = glm::clamp(zoomFov, 10.0f, 75.0f);
}

// ================================================================
// Input: keyboard setiap frame
// ================================================================
void processInput(GLFWwindow* win) {
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(win, true);

    float speed = 2.5f * deltaTime;
    glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));

    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) cameraPos += speed * cameraFront;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= speed * cameraFront;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= speed * right;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) cameraPos += speed * right;
    if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) cameraPos += speed * cameraUp;
    if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS) cameraPos -= speed * cameraUp;

    // Zoom alternatif dari keyboard: Z = zoom in, X = zoom out
    float zoomSpeed = 35.0f * deltaTime;
    if (glfwGetKey(win, GLFW_KEY_Z) == GLFW_PRESS) zoomFov -= zoomSpeed;
    if (glfwGetKey(win, GLFW_KEY_X) == GLFW_PRESS) zoomFov += zoomSpeed;
    zoomFov = glm::clamp(zoomFov, 10.0f, 75.0f);

    if (glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS) {
        cameraPos = INITIAL_CAMERA_POS;
        cameraFront = INITIAL_CAMERA_FRONT;
        yaw = INITIAL_YAW;
        pitch = INITIAL_PITCH;
        zoomFov = INITIAL_ZOOM_FOV;
        firstMouse = true;
    }

    // Ganti material dengan tombol 1-5
    static bool keyWasPressed = false;
    bool anyNumPressed = false;
    for (int i = 0; i < (int)materials.size(); i++) {
        if (glfwGetKey(win, GLFW_KEY_1 + i) == GLFW_PRESS) {
            anyNumPressed = true;
            if (!keyWasPressed) {
                currentMaterial = i;
                std::cout << "[Material] Switched to: " << materials[i].name << "\n";
            }
        }
    }
    keyWasPressed = anyNumPressed;
}

// ================================================================
// MAIN
// ================================================================
int main() {
    // -------------------------------------------------------
    // Inisialisasi GLFW
    // -------------------------------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "TVG - OpenGL Tung Tung Tung Sahur", NULL, NULL);
    if (!window) {
        std::cerr << "Gagal buat window GLFW!\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Inisialisasi GLAD (load pointer fungsi OpenGL)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Gagal init GLAD!\n";
        return -1;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << "\n";
    glEnable(GL_DEPTH_TEST); // Aktifkan depth testing

    // -------------------------------------------------------
    // Task 2: Load shader (vertex + fragment - Phong model)
    // -------------------------------------------------------
    unsigned int shader = compileShader("src/shader.vert", "src/shader.frag");

    // -------------------------------------------------------
    // Task 3: Load model OBJ + setup VBO & VAO
    // SESUAIKAN PATH SESUAI LOKASI FILE OBJ KAMU
    // -------------------------------------------------------
    Mesh model = loadOBJ("models/TungTungTungSahur.obj");

    // Load tekstur BaseColor
    unsigned int texDiffuse = loadTexture("models/TungTungTungSahur_BaseColor.png");

    // -------------------------------------------------------
    // Render loop
    // -------------------------------------------------------
    float modelRotation = 0.0f;

    std::cout << "\n=== Kontrol ===\n";
    std::cout << "  W/A/S/D  - Gerak kamera\n";
    std::cout << "  Q/E      - Naik/turun\n";
    std::cout << "  Mouse    - Lihat sekitar\n";
    std::cout << "  Scroll   - Zoom in/out\n";
    std::cout << "  Z/X      - Zoom in/out alternatif\n";
    std::cout << "  R        - Reset kamera ke tampilan awal\n";
    std::cout << "  1-5      - Ganti material (1=Gold, 2=Silver, 3=Bronze, 4=Rubber, 5=Texture)\n";
    std::cout << "  ESC      - Keluar\n\n";

    while (!glfwWindowShouldClose(window)) {
        // Hitung delta time
        float now   = (float)glfwGetTime();
        deltaTime   = now - lastFrame;
        lastFrame   = now;

        processInput(window);

        // Clear screen
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader);

        // -------------------------------------------------------
        // Task 4a: MODEL MATRIX
        // Transformasi objek: posisi, rotasi, skala
        // -------------------------------------------------------
        modelRotation += 30.0f * deltaTime; // Rotasi otomatis

        glm::mat4 modelMat = glm::mat4(1.0f);              // Identity matrix
        modelMat = glm::translate(modelMat, glm::vec3(0.0f, -0.5f, 0.0f)); // Geser ke bawah
        modelMat = glm::rotate(modelMat,
                               glm::radians(modelRotation),
                               glm::vec3(0.0f, 1.0f, 0.0f)); // Rotasi sumbu Y
        modelMat = glm::scale(modelMat, glm::vec3(MODEL_SCALE));
        modelMat = glm::translate(modelMat, -MODEL_CENTER);  // Pusatkan model di tengah scene

        // -------------------------------------------------------
        // Task 4b: VIEW MATRIX
        // Posisi dan arah kamera di dunia
        // -------------------------------------------------------
        glm::mat4 viewMat = glm::lookAt(
            cameraPos,              // Posisi kamera
            cameraPos + cameraFront,// Titik yang dilihat
            cameraUp                // Arah "atas"
        );

        // -------------------------------------------------------
        // Task 4c: PROJECTION MATRIX
        // Menentukan perspektif (bagaimana 3D diproyeksi ke layar 2D)
        // -------------------------------------------------------
        glm::mat4 projMat = glm::perspective(
            glm::radians(zoomFov),                        // FOV dinamis untuk zoom
            (float)SCR_WIDTH / (float)SCR_HEIGHT,         // Aspect ratio
            0.1f,                                         // Near clipping plane
            1000.0f                                       // Far clipping plane
        );

        // Upload matrices ke shader
        glUniformMatrix4fv(glGetUniformLocation(shader, "model"),      1, GL_FALSE, glm::value_ptr(modelMat));
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"),       1, GL_FALSE, glm::value_ptr(viewMat));
        glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projMat));

        // Upload posisi cahaya dan kamera
        glUniform3f(glGetUniformLocation(shader, "lightPos"),   5.0f, 5.0f, 5.0f);
        glUniform3f(glGetUniformLocation(shader, "lightColor"), 1.0f, 1.0f, 1.0f);
        glUniform3fv(glGetUniformLocation(shader, "viewPos"), 1, glm::value_ptr(cameraPos));

        // -------------------------------------------------------
        // Task 2b: Upload material properties ke shader
        // -------------------------------------------------------
        Material& mat = materials[currentMaterial];
        glUniform3fv(glGetUniformLocation(shader, "material_ambient"),  1, glm::value_ptr(mat.ambient));
        glUniform3fv(glGetUniformLocation(shader, "material_diffuse"),  1, glm::value_ptr(mat.diffuse));
        glUniform3fv(glGetUniformLocation(shader, "material_specular"), 1, glm::value_ptr(mat.specular));
        glUniform1f (glGetUniformLocation(shader, "material_shininess"), mat.shininess);

        // Mode tekstur (material ke-5 = pakai PNG)
        bool useTexture = (currentMaterial == 4);
        glUniform1i(glGetUniformLocation(shader, "useTexture"), useTexture ? 1 : 0);

        if (useTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texDiffuse);
            glUniform1i(glGetUniformLocation(shader, "texture_diffuse"), 0);
        }

        // Draw model
        glBindVertexArray(model.VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)model.indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &model.VAO);
    glDeleteBuffers(1, &model.VBO);
    glDeleteBuffers(1, &model.EBO);
    glDeleteProgram(shader);

    glfwTerminate();
    return 0;
}
