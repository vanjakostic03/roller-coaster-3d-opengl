// Vanja Kostic SV/2022

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.hpp"
#include "model.hpp"

// ================= STRUCTURES =================
struct Passenger {
    float offsetX, offsetY;
    bool beltOn;
    bool isSick;
    bool active; // flag da li je izasao iz vozila
};

// ================= GLOBAL VARIABLES =================
enum CarState { MOVING, SLOWING_DOWN, RETURNING, STOPPED, WAITING };

CarState carState = STOPPED;
float waitTimer = 0.0f;

float carSpeed = 0.02f;

const int maxSeats = 8;
bool allowBoarding = true;

std::vector<Passenger> passengers;
std::vector<glm::vec3> rawVertices;   // svi vertex-i iz .obj fajla
std::vector<glm::vec3> keyPoints;     // bitne tacke po kojima se auto krece
std::vector<float> segmentLengths;

int currentSegment = 0;
float alpha = 0.0f; // progres između keypoints

glm::vec3 carPosition(0.0f, 0.5f, -5.0f);
glm::vec3 seatsOffset(0.0f, 0.3f, 0.0f);

// ================= PASSENGERS FUNCTIONS =================
bool allGone() {
    for (Passenger& p : passengers) {
        if (p.active) return false;
    }
    return true;
}

// ================= HELPERS =================
void loadTrackVertices(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "Cannot open " << path << "\n";
        return;
    }

    rawVertices.clear();
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;
        if (prefix == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            rawVertices.push_back(glm::vec3(x, y, z));
        }
    }
    file.close();
}

// ================= KEYPOINT GENERATION =================
// uzimamo 1 tacku po delu staze
void generateKeyPoints() {
    keyPoints.clear();

    const int verticesPerPart = 382;
    const int numParts = 300;

    for (int part = 0; part < numParts; ++part) {
        int midIdx = part * verticesPerPart + verticesPerPart / 2;
        if (midIdx >= rawVertices.size()) midIdx = rawVertices.size() - 1;
        keyPoints.push_back(rawVertices[midIdx]);
    }

    // segment lengths
    segmentLengths.clear();
    for (size_t i = 0; i < keyPoints.size(); ++i) {
        glm::vec3 p1 = keyPoints[i];
        glm::vec3 p2 = keyPoints[(i + 1) % keyPoints.size()];
        segmentLengths.push_back(glm::length(p2 - p1));
    }

    std::cout << "Generated " << keyPoints.size() << " keypoints for smooth movement.\n";
}

glm::vec3 catmullRom(float t, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3) {
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}


// ================= CAR UPDATE =================
void updateCarPosition() {
    if (keyPoints.size() < 4) return;

    int idx1 = currentSegment;
    int idx0 = (idx1 == 0) ? keyPoints.size() - 1 : idx1 - 1;
    int idx2 = (idx1 + 1) % keyPoints.size();
    int idx3 = (idx1 + 2) % keyPoints.size();

    float segLen = segmentLengths[idx1];
    alpha += carSpeed / segLen;

    if (alpha >= 1.0f) {
        alpha = 0.0f;
        currentSegment = (currentSegment + 1) % keyPoints.size();
    }

    carPosition = catmullRom(alpha, keyPoints[idx0], keyPoints[idx1], keyPoints[idx2], keyPoints[idx3]);


}

// ================= MAIN =================
int main() {
    if (!glfwInit()) {
        std::cout << "GLFW fail!\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    int screenWidth = mode->width;
    int screenHeight = mode->height;

    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "OpenGL Window", NULL, NULL);
    if (!window) {
        std::cout << "Window fail!\n";
        glfwTerminate();
        return -2;
    }
    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        std::cout << "GLEW fail!\n";
        return -3;
    }

    Model tracks("res/tracks.obj");
    Model car("res/car.obj");
    Model seats("res/seats.obj");
    Model human("res/human.obj");

    Shader unifiedShader("basic.vert", "basic.frag");

    // Postavljanje svetla i kamere
    unifiedShader.use();
    unifiedShader.setVec3("uLightPos1", 0, 100, 75);
    unifiedShader.setVec3("uLightColor1", 2, 2, 2);
    unifiedShader.setVec3("uLightPos2", 0, 100, 20);
    unifiedShader.setVec3("uLightColor2", 2, 2, 2);
    unifiedShader.setVec3("uLightPos3", 100, -20, -30);
    unifiedShader.setVec3("uLightColor3", 2, 2, 2);
    unifiedShader.setVec3("uLightPos4", 0, 0, 50);
    unifiedShader.setVec3("uLightColor4", 2, 2, 2);
    unifiedShader.setVec3("uViewPos", 0, 0, 5);

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)screenWidth / (float)screenHeight, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 10.0f, -60.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    unifiedShader.setMat4("uP", projection);
    unifiedShader.setMat4("uV", view);

    loadTrackVertices("res/tracks.obj");
    generateKeyPoints();

    glm::mat4 modelTracks = glm::mat4(1.0f);

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // update pozicije kola
        updateCarPosition();

        // draw tracks
        unifiedShader.setMat4("uM", modelTracks);
        tracks.Draw(unifiedShader);

        // draw car
        glm::mat4 modelCar = glm::mat4(1.0f);
        modelCar = glm::translate(modelCar, carPosition);
        modelCar = glm::scale(modelCar, glm::vec3(0.8f));
        unifiedShader.setMat4("uM", modelCar);
        car.Draw(unifiedShader);

        // draw seats
        glm::mat4 modelSeats = glm::mat4(1.0f);
        modelSeats = glm::translate(modelSeats, carPosition + seatsOffset);
        modelSeats = glm::scale(modelSeats, glm::vec3(0.8f));
        unifiedShader.setMat4("uM", modelSeats);
        seats.Draw(unifiedShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
