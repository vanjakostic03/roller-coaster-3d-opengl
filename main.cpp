// Vanja Kostic SV/2022

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
float carSpeed = 0.01f;
std::vector<glm::vec3> rawVertices;
std::vector<glm::vec3> keyPoints;
std::vector<glm::vec3> sortedPoints; 

glm::vec3 carPosition(1.0f);
glm::vec3 seatsOffset(0.0f, 0.3f, 0.0f);
int currentTargetIdx = 0;

// kontrola kamere misem
float yaw = -180.0f;	 
float pitch = 0.0f;	 
float lastX = 1280.0f / 2.0f;
float lastY = 720.0f / 2.0f;
bool firstMouse = true;

enum CarState { MOVING, SLOWING_DOWN, RETURNING, STOPPED, WAITING };
CarState carState = STOPPED;

float t = 0.0f;        // parametar napredovanja 
float waitTimer = 0.0f;
float minSpeed = 0.05f;
float maxSpeed = 0.50f;
float gravityFactor = 9.7f;

bool allowBoarding = true;
const int maxSeats = 8;


struct Passenger {
    float offsetX, offsetY, offsetZ;
    bool beltOn;
    bool isSick;
    bool active;            
};

std::vector<Passenger> passengers;
int activeCameraPassenger = -1;



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

void setupGreenFilter(unsigned int& VAO, unsigned int& VBO)
{
    float vertices[] = {
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f,
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f,   0.0f, 1.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

unsigned int createGreenFilter()
{
    unsigned int greenTextureID;
    glGenTextures(1, &greenTextureID);
    glBindTexture(GL_TEXTURE_2D, greenTextureID);

    unsigned char greenPixel[] = {50, 255, 0, 150 };  
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, greenPixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return greenTextureID;
}

void generateKeyPoints() {
    keyPoints.clear();
    const int verticesPerPart = 382;
    int maxVertices = rawVertices.size();

    for (int i = 0; i < maxVertices; i += verticesPerPart) {
        glm::vec3 sum(0.0f);
        int count = 0;
        for (int j = 0; j < verticesPerPart && (i + j) < maxVertices; ++j) {
            sum += rawVertices[i + j];
            count++;
        }
        if (count > 0) keyPoints.push_back(sum / (float)count);
    }

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

bool allGone() {
    bool gone = true;
    for (Passenger& p : passengers) {
        if (p.active) {
            gone = false;
            break;
        }
    }
    return gone;
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

    if (action == GLFW_PRESS && carState != MOVING && allowBoarding) {
        if (key == GLFW_KEY_SPACE) {

            if (passengers.size() >= maxSeats) return;
            int seatIndex = passengers.size();

            int row = seatIndex % 2;
            int col = seatIndex / 2;

            if (seatIndex == 0) {
                activeCameraPassenger = 0;
            }

            float horizontalSpacing = 1.1f;  // razmak između kolona
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
    
    if (action == GLFW_PRESS && carState == STOPPED && !allowBoarding) {
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_8) {
            int index = key - GLFW_KEY_1;
            if (key == GLFW_KEY_1) {
                activeCameraPassenger = -1;
            }
            if (index < passengers.size() && passengers[index].active) {
                passengers[index].active = false;
                passengers[index].isSick = false;
            }
        }
    }

    if (allGone()) {
        passengers.clear();
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

    if (action == GLFW_PRESS && carState == STOPPED && allowBoarding) {
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

    auto pos = getCarPosition(sortedPoints, t);
    carPosition = pos.first;
    carFront = pos.second;  
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f; 
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw -= xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}   

void allKeys(GLFWwindow* window, int key, int scancode, int action, int mods) {
    startRide(window, key, scancode, action, mods);
    addPassanger(window, key, scancode, action, mods);
    sickPassenger(window, key, scancode, action, mods);
    putBeltOn(window, key, scancode, action, mods);
    removePassenger(window, key, scancode, action, mods);
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
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);


    Model lija("res/low-poly-fox.obj");
    Model tracks("res/tracks.obj");
    Model car("res/car.obj");
    Model seats("res/seats.obj");
    Model passengerModel("res/human.obj");
    Model beltModel("res/belt.obj");

    Shader unifiedShader("basic.vert", "basic.frag");

    unifiedShader.use();
    unifiedShader.setVec3("uTint", 1.0f, 1.0f, 1.0f);

    Shader overlayShader("overlay.vert", "overlay.frag");
 
    unsigned int greenOverlayVAO, greenOverlayVBO;
    setupGreenFilter(greenOverlayVAO, greenOverlayVBO);
    unsigned int greenTexture = createGreenFilter();


    unifiedShader.setVec3("uLightPos1", 50, 100, 75);
    unifiedShader.setVec3("uLightColor1", 2, 2, 2);
   unifiedShader.setVec3("uLightPos2", -50, 0, 0);
   unifiedShader.setVec3("uLightColor2", 0.5, 0.5, 0.5);
    unifiedShader.setVec3("uViewPos", 0, 0, 5);

    loadTrackVertices("res/tracks.obj");
    generateKeyPoints();

    if (!sortedPoints.empty()) carPosition = sortedPoints[0];

    glEnable(GL_DEPTH_TEST);

    glClearColor(0.12f, 0.8f, 1.0f, 1.0f);

    double lastTime = glfwGetTime();
    glm::mat4 passengerRotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    glm::vec3 cameraHeightOffset(0.0f, 1.5f, 0.0f);

    glm::mat4 view;

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

            carPosition = glm::mix(p1, p2, alpha);
            carFront = glm::normalize(p2 - p1);
        }

       
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
 

        unifiedShader.setMat4("uP", projection);


        unifiedShader.setMat4("uM", glm::mat4(1.0f));
        tracks.Draw(unifiedShader);

        glm::mat4 modelCar = glm::mat4(1.0f);
        modelCar = glm::translate(modelCar, carPosition + glm::vec3(0.2f, 1.5f, 0.65f));

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
        modelSeats = glm::translate(modelSeats, carPosition + glm::vec3(0.2f, 1.5f, 0.65f));
        modelSeats = modelSeats * rotationMatrix;             
        modelSeats = glm::translate(modelSeats, seatsOffset); 
        modelSeats = glm::scale(modelSeats, glm::vec3(0.8f));

        unifiedShader.setMat4("uM", modelSeats);
        seats.Draw(unifiedShader);


        for (const Passenger& p : passengers) {
            if (!p.active) continue;

            if (p.isSick)
                unifiedShader.setVec3("uTint", 0.2f, 1.0f, 0.2f);
            else
                unifiedShader.setVec3("uTint", 1.0f, 1.0f, 1.0f);

            glm::mat4 modelPassenger = glm::mat4(1.0f);
            modelPassenger = glm::translate(modelPassenger, carPosition);
            modelPassenger = modelPassenger * rotationMatrix; 
            modelPassenger = modelPassenger * passengerRotation; 
            modelPassenger = glm::translate(modelPassenger, glm::vec3(p.offsetX-0.2f, p.offsetY + 1.5f, p.offsetZ+0.53f));
            modelPassenger = glm::scale(modelPassenger, glm::vec3(1.0f));


            unifiedShader.setMat4("uM", modelPassenger);
            passengerModel.Draw(unifiedShader);

            if (p.beltOn) {
                glm::mat4 modelBelt = glm::mat4(1.0f);
                modelBelt = glm::translate(modelBelt, carPosition);
                modelBelt = modelBelt * rotationMatrix; 
                modelBelt = modelBelt * passengerRotation; 
                //modelBelt = glm::translate(modelBelt, glm::vec3(p.offsetX - 0.2f, p.offsetY + 1.5f, p.offsetZ - 0.1f));
                modelBelt = glm::translate(modelBelt, glm::vec3(p.offsetX - 0.5f, p.offsetY + 1.8f, p.offsetZ+ 0.53f));
                modelBelt = glm::rotate(modelBelt, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                modelBelt = glm::scale(modelBelt, glm::vec3(1.0f));

                unifiedShader.setMat4("uM", modelBelt);
                beltModel.Draw(unifiedShader);
            }
        }

        //if (activeCameraPassenger == 0 && !passengers.empty() && passengers[0].active) {
        //    // 1. Izračunaj poziciju glave prvog putnika
        //    glm::mat4 headPosMatrix = glm::mat4(1.0f);
        //    headPosMatrix = glm::translate(headPosMatrix, carPosition);
        //    headPosMatrix = headPosMatrix * rotationMatrix;
        //    headPosMatrix = headPosMatrix * passengerRotation;

        //    glm::vec3 p0Offset = glm::vec3(passengers[0].offsetX - 1.5f, passengers[0].offsetY + 0.8f, passengers[0].offsetZ);
        //    headPosMatrix = glm::translate(headPosMatrix, p0Offset + cameraHeightOffset);
        //    glm::vec3 eyePos = glm::vec3(headPosMatrix[3]);

        //    float lookAheadT = t + 0.02f; // Gledaj 2% staze unapred
        //    if (lookAheadT > 1.0f) lookAheadT -= 1.0f;

        //    auto futurePos = getCarPosition(sortedPoints, lookAheadT);
        //    glm::vec3 lookAtTarget = futurePos.first + glm::vec3(0.0f, 1.5f, 0.0f); // Gledaj u visini očiju

        //    view = glm::lookAt(eyePos, lookAtTarget, up);
        //    //glm::vec3 p0Offset = glm::vec3(passengers[0].offsetX - 1.5f, passengers[0].offsetY + 0.8f, passengers[0].offsetZ);
        //    //headPosMatrix = glm::translate(headPosMatrix, p0Offset + cameraHeightOffset);

        //    //glm::vec3 eyePos = glm::vec3(headPosMatrix[3]);
        //    //glm::vec3 lookAtTarget = eyePos + carFront; // Gleda pravo napred

        //    //view = glm::lookAt(eyePos, lookAtTarget, up);
        //}
        if (activeCameraPassenger == 0 && !passengers.empty()) {
            glm::mat4 headPosMatrix = glm::mat4(1.0f);
            headPosMatrix = glm::translate(headPosMatrix, carPosition);
            headPosMatrix = headPosMatrix * rotationMatrix;
            headPosMatrix = headPosMatrix * passengerRotation;

            glm::vec3 p0Offset = glm::vec3(passengers[0].offsetX - 1.0f, passengers[0].offsetY + 0.8f, passengers[0].offsetZ);
            headPosMatrix = glm::translate(headPosMatrix, p0Offset + cameraHeightOffset);
            glm::vec3 eyePos = glm::vec3(headPosMatrix[3]) + glm::vec3(0.0f, 1.0f, 0.0f);

            glm::vec3 relativeFront;
            relativeFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            relativeFront.y = sin(glm::radians(pitch));
            relativeFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            relativeFront = glm::normalize(relativeFront);

            glm::vec3 finalFront = glm::vec3(rotationMatrix * passengerRotation * glm::vec4(relativeFront, 0.0f));

            view = glm::lookAt(eyePos, eyePos + finalFront, up);

    
        }
        else {
            
            view = glm::lookAt(glm::vec3(-40.0f, 0.0f, -35.0f), glm::vec3(-20.0f, 10.0f, 15.0f), glm::vec3(0.0f, 1.0f, 0.0f));;
            //view = glm::lookAt(glm::vec3(-40.0f, 0.0f, -10.0f), glm::vec3(-10.0f, 10.0f, 10.0f), glm::vec3(0.0f, 1.0f, 0.0f));;
            //view = glm::lookAt(glm::vec3(40.0f, 0.0f, -20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));;
        }

        unifiedShader.setMat4("uV", view);

        unifiedShader.setVec3("uTint", 1.0f, 1.0f, 1.0f);

        unifiedShader.use();
        glm::mat4 currentView = view;

        if (!passengers.empty() && passengers[0].active && passengers[0].isSick) {
            glDisable(GL_DEPTH_TEST);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            overlayShader.use();

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, greenTexture);
            overlayShader.setInt("overlayTexture", 0);

            glBindVertexArray(greenOverlayVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);

            unifiedShader.use();
            unifiedShader.setMat4("uV", currentView);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();

        while (glfwGetTime() - currentTime < 1 / 75.0) {}
    }
    glfwTerminate();
    return 0;
}