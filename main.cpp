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

float t = 0.0f;        // parametar napredovanja duž staze
float waitTimer = 0.0f;
float minSpeed = 0.05f;
float maxSpeed = 1.4f;
float gravityFactor = 9.7f;

const int maxSeats = 8;

bool allowBoarding = true;

struct Passenger {
    float offsetX, offsetY, offsetZ;
    bool beltOn;
    bool isSick;
    bool active;            
};

std::vector<Passenger> passengers;

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



glm::vec3 carFront(0.0f, 0.0f, 1.0f);      
glm::vec3 carFrontTarget(0.0f, 0.0f, 1.0f); 


void startRide(GLFWwindow* window, int key, int scancode, int action, int mods) {


    bool allBelts = true;
    
    if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && carState == STOPPED && !passengers.empty()) {
        for (Passenger& p : passengers) {
            if (!p.beltOn) {
                allBelts = false;
                break;
            }
        }

        if (allBelts) {
            carState = MOVING;
            allowBoarding = false;
        }


    }
}



void addPassanger(GLFWwindow* window, int key, int scancode, int action, int mods) {

    if (action == GLFW_PRESS && carState != MOVING) {
        if (key == GLFW_KEY_SPACE && allowBoarding) {

            if (passengers.size() >= maxSeats) return; // limit
            int seatIndex = passengers.size();

            int row = seatIndex % 2;
            int col = seatIndex / 2;

            float horizontalSpacing = 1.2f;  // razmak između kolona
            float verticalSpacing = 1.2f;  // razmak između redova
            float seatHeight = 0.0f;

            Passenger p;

            p.offsetX = -1.5 + col * horizontalSpacing;  //levo-desno
            p.offsetY = seatHeight;                        //visina
            p.offsetZ = -0.6f + row * verticalSpacing;    //napred-nazad

            p.beltOn = false;
            p.isSick = false;
            p.active = true;

            passengers.push_back(p);

        }
    }
}


void removePassenger(GLFWwindow* window, int key, int scancode, int action, int mods) {
    
    if (action == GLFW_PRESS && carState == STOPPED) {
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_8) {
            int index = key - GLFW_KEY_1;
            if (index < passengers.size() && passengers[index].active) {
                passengers[index].active = false;
                passengers[index].beltOn = false;
                passengers[index].isSick = false;
            }
        }
    }

    if (passengers.size() == 0) {
        allowBoarding = true;
    }
}

void makePassengerSick(int index) {
    if (passengers.size() > index) {      
        passengers[index].isSick = true;
        carState = SLOWING_DOWN;
    }
}


void sickPassenger(GLFWwindow* window, int key, int scancode, int action, int mods) {

    if (action == GLFW_PRESS && carState == MOVING) {
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_8) {
            int index = key - GLFW_KEY_1;
            makePassengerSick(index);
        }
    }
}

void putBeltOn(GLFWwindow* window, int key, int scancode, int action, int mods) {

    if (action == GLFW_PRESS && carState == STOPPED) {
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_8) {
            int index = key - GLFW_KEY_1;
            if (index < passengers.size() && passengers[index].active) {
                passengers[index].beltOn = true;
            }
        }
    }
}


void stopCar() {
    for (Passenger& p : passengers) {
        p.beltOn = false;
    }
}

std::pair<glm::vec3, glm::vec3> getCarPosition(const std::vector<glm::vec3>& points, float t) {
    int n = points.size();
    int i0 = (int)(t * n);
    int i1 = (i0 + 1) % n;
    float alpha = t * n - i0;

    glm::vec3 pos = glm::mix(points[i0], points[i1], alpha);
    glm::vec3 forward = glm::normalize(points[i1] - points[i0]);
    return { pos, forward };
}


void updateCarPosition(float deltaTime) {
    if (sortedPoints.empty()) return;

    int n = sortedPoints.size();
    float dtSlope = 0.001f;

    // indeks trenutnog segmenta
    int i0 = (int)(t * n);
    int i1 = (i0 + 1) % n;

    glm::vec3 p1 = sortedPoints[i0];
    glm::vec3 p2 = sortedPoints[i1];

    // nagib u Y osi
    float dy = p2.y - p1.y;
    float acc = -dy * gravityFactor;

    switch (carState) {
    case MOVING:
        carSpeed += acc * deltaTime;
        carSpeed = glm::clamp(carSpeed, minSpeed, maxSpeed);
        t += carSpeed * deltaTime;
        break;
    case SLOWING_DOWN:
        carSpeed -= 0.1f * deltaTime;
        if (carSpeed <= 0.0f) { carSpeed = 0.0f; carState = WAITING; waitTimer = 0.0f; }
        else t += carSpeed * deltaTime;
        break;
    case WAITING:
        waitTimer += deltaTime;
        if (waitTimer >= 10.0f) {
            carSpeed = 0.1f;
            carState = RETURNING;
        }
        break;
    case RETURNING:
        t -= carSpeed * deltaTime;
        if (t <= 0.0f) { t = 0.0f; carState = STOPPED; carSpeed = 0.0f; stopCar(); }
        break;
    case STOPPED:
        carSpeed = 0.0f;
        break;
    }

    if (t >= 1.0f) t -= floor(t);
    if (t < 0.0f) t += 1.0f;

    // pozicija automobila
    auto pos = getCarPosition(sortedPoints, t);
    carPosition = pos.first;
    carFront = pos.second;  // orizontalni forward vector, normalize
}


void allKeys(GLFWwindow* window, int key, int scancode, int action, int mods) {
    startRide(window, key, scancode, action, mods);
    addPassanger(window, key, scancode, action, mods);
    sickPassenger(window, key, scancode, action, mods);
    putBeltOn(window, key, scancode, action, mods);
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
    Model passengerModel("res/human.obj");
    Model beltModel("res/belt.obj");

    Shader unifiedShader("basic.vert", "basic.frag");

    unifiedShader.use();
    unifiedShader.setVec3("uTint", 1.0f, 1.0f, 1.0f);


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

    glClearColor(0.12f, 0.8f, 1.0f, 1.0f);

    double lastTime = glfwGetTime();
    glm::mat4 passengerRotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, -1.0f, 0.0f));


    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!sortedPoints.empty()) {

            // indeksi za interpolaciju
            int n = sortedPoints.size();
            int i0 = (int)(t * n);
            int i1 = (i0 + 1) % n;
            float alpha = t * n - i0;

            glm::vec3 p1 = sortedPoints[i0];
            glm::vec3 p2 = sortedPoints[i1];

            // nagib i ubrzanje
            float dy = p2.y - p1.y;
            float acc = -dy * gravityFactor;

            switch (carState) {
            case MOVING:
                carSpeed += acc * deltaTime;
                carSpeed = glm::clamp(carSpeed, minSpeed, maxSpeed);
                t += carSpeed * deltaTime;
                break;
            case SLOWING_DOWN:
                carSpeed -= 0.1f * deltaTime;
                if (carSpeed <= 0.0f) { carSpeed = 0.0f; carState = WAITING; waitTimer = 0.0f; }
                else t += carSpeed * deltaTime;
                break;
            case WAITING:
                waitTimer += deltaTime;
                if (waitTimer >= 10.0f) {
                    carSpeed = 0.1f;
                    carState = RETURNING;
                }
                break;
            case RETURNING:
                t -= carSpeed * deltaTime;
                if (t <= 0.0f) { t = 0.0f; carState = STOPPED; carSpeed = 0.0f; stopCar(); }
                break;
            case STOPPED:
                carSpeed = 0.0f;
                break;
            }

            if (t >= 1.0f) t -= floor(t);
            if (t < 0.0f) t += 1.0f;

            // pozicija i forward vektor automobila
            carPosition = glm::mix(p1, p2, alpha);
            carFront = glm::normalize(p2 - p1);
        }

        

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

        //cout << passengers.size();
        for (const Passenger& p : passengers) {
            if (!p.active) continue;

            if (p.isSick)
                unifiedShader.setVec3("uTint", 0.2f, 1.0f, 0.2f);
            else
                unifiedShader.setVec3("uTint", 1.0f, 1.0f, 1.0f);

            glm::mat4 modelPassenger = glm::mat4(1.0f);
            modelPassenger = glm::translate(modelPassenger, carPosition);
            modelPassenger = modelPassenger * rotationMatrix; // auto rotacija
            modelPassenger = modelPassenger * passengerRotation; // globalna rotacija putnika
            modelPassenger = glm::translate(modelPassenger, glm::vec3(p.offsetX, p.offsetY + 0.8f, p.offsetZ));
            modelPassenger = glm::scale(modelPassenger, glm::vec3(1.0f));

            unifiedShader.setMat4("uM", modelPassenger);
            passengerModel.Draw(unifiedShader);

            if (p.beltOn) {
                glm::mat4 modelBelt = glm::mat4(1.0f);
                modelBelt = glm::translate(modelBelt, carPosition);
                modelBelt = modelBelt * rotationMatrix; 
                modelBelt = modelBelt * passengerRotation; 
                modelBelt = glm::translate(modelBelt, glm::vec3(p.offsetX - 1.0f, p.offsetY + 0.8f, p.offsetZ));
                modelBelt = glm::scale(modelBelt, glm::vec3(1.0f));

                unifiedShader.setMat4("uM", modelBelt);
                beltModel.Draw(unifiedShader);
            }
        }

        unifiedShader.setVec3("uTint", 1.0f, 1.0f, 1.0f);




        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}