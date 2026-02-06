// Vanja Kostic SV/2022
// FIXED: Nearest-neighbor logic to prevent jumping off tracks due to unsorted OBJ vertices

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.hpp"
#include "model.hpp"

// ================= GLOBAL VARIABLES =================
float carSpeed = 0.02f;
std::vector<glm::vec3> rawVertices;
std::vector<glm::vec3> keyPoints;
std::vector<glm::vec3> sortedPoints; // Ovde će biti tačke u pravom redosledu

glm::vec3 carPosition(0.0f);
glm::vec3 seatsOffset(0.0f, 0.3f, 0.0f);
int currentTargetIdx = 0;

// ================= HELPERS =================
void loadTrackVertices(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;
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

void generateKeyPoints() {
    keyPoints.clear();
    const int verticesPerPart = 382;
    int maxVertices = rawVertices.size();

    // 1. Generiši grube središnje tačke
    for (int i = 0; i < maxVertices; i += verticesPerPart) {
        glm::vec3 sum(0.0f);
        int count = 0;
        for (int j = 0; j < verticesPerPart && (i + j) < maxVertices; ++j) {
            sum += rawVertices[i + j];
            count++;
        }
        if (count > 0) keyPoints.push_back(sum / (float)count);
    }

    // 2. SORTIRANJE (Greedy algorithm): Poveži tačke po blizini, ne po indeksu
    if (keyPoints.empty()) return;

    sortedPoints.clear();
    std::vector<bool> visited(keyPoints.size(), false);

    glm::vec3 current = keyPoints[0];
    sortedPoints.push_back(current);
    visited[0] = true;

    for (size_t i = 1; i < keyPoints.size(); ++i) {
        float minDist = 1000000.0f;
        int nextIdx = -1;

        for (size_t j = 0; j < keyPoints.size(); ++j) {
            if (!visited[j]) {
                float d = glm::distance(current, keyPoints[j]);
                if (d < minDist) {
                    minDist = d;
                    nextIdx = (int)j;
                }
            }
        }

        if (nextIdx != -1) {
            visited[nextIdx] = true;
            current = keyPoints[nextIdx];
            sortedPoints.push_back(current);
        }
    }
    std::cout << "Sorted " << sortedPoints.size() << " points for a continuous loop.\n";
}

// ================= UPDATE LOGIC =================
void updateCarPosition() {
    if (sortedPoints.size() < 2) return;

    glm::vec3 target = sortedPoints[currentTargetIdx];
    float distToTarget = glm::distance(carPosition, target);

    // Pomeraj auto ka meti
    if (distToTarget > 0.1f) {
        glm::vec3 direction = glm::normalize(target - carPosition);
        carPosition += direction * carSpeed;
    }
    else {
        // Stigli smo do tačke, pređi na sledeću (kružno)
        currentTargetIdx = (currentTargetIdx + 1) % sortedPoints.size();
    }
}

// ================= MAIN =================
int main() {
    if (!glfwInit()) return -1;

    // Prozor i OpenGL setup (tvoj originalni kod)
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Fixed Roller Coaster", NULL, NULL);
    glfwMakeContextCurrent(window);
    glewInit();

    Model tracks("res/tracks.obj");
    Model car("res/car.obj");
    Model seats("res/seats.obj");
    Shader unifiedShader("basic.vert", "basic.frag");

    loadTrackVertices("res/tracks.obj");
    generateKeyPoints();

    if (!sortedPoints.empty()) carPosition = sortedPoints[0];

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        updateCarPosition();

        unifiedShader.use();

        // --- TVOJA SVETLA ---
        unifiedShader.setVec3("uLightPos1", 0, 100, 75);
        unifiedShader.setVec3("uLightColor1", 2, 2, 2);
        unifiedShader.setVec3("uLightPos2", 0, 100, 20);
        unifiedShader.setVec3("uLightColor2", 2, 2, 2);
        unifiedShader.setVec3("uLightPos3", 100, -20, -30);
        unifiedShader.setVec3("uLightColor3", 2, 2, 2);
        unifiedShader.setVec3("uLightPos4", 0, 0, 50);
        unifiedShader.setVec3("uLightColor4", 2, 2, 2);
        unifiedShader.setVec3("uViewPos", 0, 0, 5);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 15.0f, -70.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        unifiedShader.setMat4("uP", projection);
        unifiedShader.setMat4("uV", view);

        // Draw Tracks
        unifiedShader.setMat4("uM", glm::mat4(1.0f));
        tracks.Draw(unifiedShader);

        // Draw Car
        glm::mat4 modelCar = glm::translate(glm::mat4(1.0f), carPosition);
        modelCar = glm::scale(modelCar, glm::vec3(0.8f));
        unifiedShader.setMat4("uM", modelCar);
        car.Draw(unifiedShader);

        // Draw Seats
        glm::mat4 modelSeats = glm::translate(glm::mat4(1.0f), carPosition + seatsOffset);
        modelSeats = glm::scale(modelSeats, glm::vec3(0.8f));
        unifiedShader.setMat4("uM", modelSeats);
        seats.Draw(unifiedShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}