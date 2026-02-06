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

enum CarState { MOVING, SLOWING_DOWN, RETURNING, STOPPED, WAITING };
CarState carState = STOPPED;

struct Passenger {
    float offsetX, offsetY;
    bool beltOn;
    bool isSick;
    bool active;            //flag da li je izasao iz vozila
};

std::vector<Passenger> passengers;
bool allowBoarding = true;

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


// Dodaj ovo u Global Variables sekciju
glm::vec3 carFront(0.0f, 0.0f, 1.0f);      // trenutni forward
glm::vec3 carFrontTarget(0.0f, 0.0f, 1.0f); // smer ka sledećoj tački


void startRide(GLFWwindow* window, int key, int scancode, int action, int mods) {


    bool allBelts = true;
    if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && carState == STOPPED ) {
    //if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && carState == STOPPED && !passengers.empty()) {
       /* for (Passenger& p : passengers) {
            if (!p.beltOn) {
                allBelts = false;
                break;
            }
        }*/

        if (allBelts) {
            carState = MOVING;
            allowBoarding = false;
        }


    }
}

void updateCarPosition() {
    if (sortedPoints.size() < 2) return;

    glm::vec3 target = sortedPoints[currentTargetIdx];
    glm::vec3 delta = target - carPosition;
    float distToTarget = glm::length(delta);

    if (distToTarget < 0.001f) distToTarget = 0.001f;

    // Ažuriramo smer gledanja (Forward vektor)
    if (distToTarget > 0.01f) {
        carFrontTarget = glm::normalize(delta);
    }

    // Smooth interpolacija: blend trenutnog i target vektora
    float rotSpeed = 0.1f; // 0.1 = sporo, 0.5 = brže
    carFront = glm::normalize(glm::mix(carFront, carFrontTarget, rotSpeed));


    // Gravitacija
    float gravityFactor = 1.5f;
    float slope = delta.y / distToTarget;
    gravityFactor += -slope;

    gravityFactor = glm::clamp(gravityFactor, 0.5f, 2.0f);
    float moveStep = carSpeed * gravityFactor;

    if (distToTarget > moveStep) {
        carPosition += carFront * moveStep;
    }
    else {
        carPosition = target;
        currentTargetIdx = (currentTargetIdx + 1) % sortedPoints.size();
    }
}


void allKeys(GLFWwindow* window, int key, int scancode, int action, int mods) {
    startRide(window, key, scancode, action, mods);
    /*addPassanger(window, key, scancode, action, mods);
    sickPassenger(window, key, scancode, action, mods);*/
}
// ================= MAIN =================
int main() {
    if (!glfwInit()) return -1;

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

    glfwSetKeyCallback(window, allKeys);


    Model tracks("res/tracks.obj");
    Model car("res/car.obj");
    Model seats("res/seats.obj");

    Shader unifiedShader("basic.vert", "basic.frag");

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

    loadTrackVertices("res/tracks.obj");
    generateKeyPoints();

    if (!sortedPoints.empty()) carPosition = sortedPoints[0];

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window)) {

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        updateCarPosition();

        

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 15.0f, -70.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        unifiedShader.setMat4("uP", projection);
        unifiedShader.setMat4("uV", view);

        unifiedShader.setMat4("uM", glm::mat4(1.0f));
        tracks.Draw(unifiedShader);

        glm::mat4 modelCar = glm::mat4(1.0f);
        modelCar = glm::translate(modelCar, carPosition);

        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(worldUp, carFront));
        glm::vec3 up = glm::cross(carFront, right);

        glm::mat4 rotationMatrix = glm::mat4(1.0f);
        rotationMatrix[0] = glm::vec4(right, 0.0f);
        rotationMatrix[1] = glm::vec4(up, 0.0f);
        rotationMatrix[2] = glm::vec4(-carFront, 0.0f); 

        modelCar = modelCar * rotationMatrix;
        modelCar = glm::scale(modelCar, glm::vec3(0.8f));

        unifiedShader.setMat4("uM", modelCar);
        car.Draw(unifiedShader);


        glm::mat4 modelSeats = glm::mat4(1.0f);
        modelSeats = glm::translate(modelSeats, carPosition); 
        modelSeats = modelSeats * rotationMatrix;             
        modelSeats = glm::translate(modelSeats, seatsOffset); 
        modelSeats = glm::scale(modelSeats, glm::vec3(0.8f));

        unifiedShader.setMat4("uM", modelSeats);
        seats.Draw(unifiedShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}