    //Vanja Kostic SV/2022

    #define _CRT_SECURE_NO_WARNINGS

    #include <iostream>
    #include <fstream>
    #include <sstream>

    #include <GL/glew.h>
    #include <GLFW/glfw3.h>

    #include <glm/glm.hpp>
    #include <glm/gtc/matrix_transform.hpp>
    #include <glm/gtc/type_ptr.hpp>

    #include "shader.hpp"
    #include "model.hpp"

    // ================= STRUCTURES   =================
    struct Passenger {
        float offsetX, offsetY;
        bool beltOn;
        bool isSick;
        bool active;            //flag da li je izasao iz vozila
    };



    // ================= GLOBAL VARIABLES   =================
    enum CarState { MOVING, SLOWING_DOWN, RETURNING, STOPPED, WAITING };

    CarState carState = STOPPED;
    float waitTimer = 0.0f;

    float carSpeed = 0.5f;

    const int maxSeats = 8;
    bool allowBoarding = true;

    std::vector<Passenger> passengers;
    std::vector<glm::vec3> trackVertices;
    std::vector<glm::vec3> trackPath;       //sredina
    std::vector<float> segmentLengths;

    int currentSegment = 0;
    float t = 0.0f;     //progres izmedju keypointa

    glm::vec3 carPosition(0.0f, 0.5f, -5.0f);  
    float carRotation = 0.0f;                  

    glm::vec3 seatsOffset(0.0f, 0.3f, 0.0f);


    //==========================PASSENGERS FUNCTIONS======================
    bool allGone() {
        for (Passenger& p : passengers) {
            if (p.active) return false;
        }
        return true;
    }

    // ================= CATMULL-ROM INTERPOLATION =================
    glm::vec3 catmullRom(float t, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3) {
        float t2 = t * t;
        float t3 = t2 * t;
        return 0.5f * ((2.0f * p1) +
            (-p0 + p2) * t +
            (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
            (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    }


    //==========================HELPERS====================================

    std::vector<float> cumulativeLengths;
    float totalLength = 0.0f;
    void prepareArcLength() {
        cumulativeLengths.clear();
        cumulativeLengths.push_back(0.0f);
        totalLength = 0.0f;
        for (size_t i = 0; i < trackPath.size(); ++i) {
            glm::vec3 p1 = trackPath[i];
            glm::vec3 p2 = trackPath[(i + 1) % trackPath.size()];
            float len = glm::length(p2 - p1);
            totalLength += len;
            cumulativeLengths.push_back(totalLength);
        }
    }


    void loadTrackVertices(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cout << "Cannot open " << path << "\n";
            return;
        }

        // Učitaj sve vertex-e
        std::vector<glm::vec3> rawVertices;
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

        // check if track has left/right vertices
        bool hasLeft = false, hasRight = false;
        for (auto& v : rawVertices) {
            if (v.x < 0) hasLeft = true;
            if (v.x > 0) hasRight = true;
        }
        if (hasLeft && hasRight)
            std::cout << "Track has left and right edges.\n";
        else
            std::cout << "Track edges not clearly defined.\n";

        // generate center path with distance-based sampling
        trackPath.clear();
        if (rawVertices.size() < 2) return;

        glm::vec3 lastCenter = (rawVertices[0] + rawVertices[1]) * 0.5f;
        trackPath.push_back(lastCenter);

        const float minDist = 0.5f; // minimalna distanca između keypoint-a (možeš menjati)

        for (size_t i = 2; i + 1 < rawVertices.size(); i += 2) {
            glm::vec3 center = (rawVertices[i] + rawVertices[i + 1]) * 0.5f;
            if (glm::length(center - lastCenter) >= minDist) {
                trackPath.push_back(center);
                lastCenter = center;
            }
        }

        // segment lengths for arc-length parametrization
        segmentLengths.clear();
        for (size_t i = 0; i < trackPath.size(); ++i) {
            glm::vec3 p1 = trackPath[i];
            glm::vec3 p2 = trackPath[(i + 1) % trackPath.size()];
            segmentLengths.push_back(glm::length(p2 - p1));
        }

        if (trackPath.empty()) std::cout << "Warning: trackPath is empty!\n";
    }




    int main()
    {
        if(!glfwInit())
        {
            std::cout << "GLFW fail!\n" << std::endl;
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

        if (window == NULL)
        {
            std::cout << "Window fail!\n" << std::endl;
            glfwTerminate();
            return -2;
        }
        glfwMakeContextCurrent(window);

        if (glewInit() !=GLEW_OK)
        {
            std::cout << "GLEW fail! :(\n" << std::endl;
            return -3;
        }


        Model tracks("res/tracks.obj");
        Model car("res/car.obj");
        Model seats("res/seats.obj");
        Model human("res/human.obj");
        //Tjemena i baferi su definisani u model klasi i naprave se pri stvaranju objekata

        Shader unifiedShader("basic.vert", "basic.frag");

        //Render petlja
        unifiedShader.use();

        unifiedShader.setVec3("uLightPos1", 0, 100, 75);
        unifiedShader.setVec3("uLightColor1", 2, 2,2);

        unifiedShader.setVec3("uLightPos2", 0, 100, 20);
        unifiedShader.setVec3("uLightColor2", 2, 2, 2);

        // Donji uglovi
        unifiedShader.setVec3("uLightPos3", 100, -20, -30);
        unifiedShader.setVec3("uLightColor3", 2, 2, 2);

        unifiedShader.setVec3("uLightPos4", 0, 0, 50);
        unifiedShader.setVec3("uLightColor4", 2, 2, 2);

        unifiedShader.setVec3("uViewPos", 0, 0, 5);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)screenWidth / (float)screenHeight, 0.1f, 100.0f);
        unifiedShader.setMat4("uP", projection);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 10.0f, -60.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        //glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 20.0f, -60.0f), glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        unifiedShader.setMat4("uV", view);


        loadTrackVertices("res/tracks.obj");

        glm::mat4 modelTracks = glm::mat4(1.0f);


        glEnable(GL_DEPTH_TEST);
        while (!glfwWindowShouldClose(window))
        {

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);
                    

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            //model = glm::rotate(model, glm::radians(0.1f), glm::vec3(0.0f, 1.0f, 0.0f));


            // ---------------- Update car along track using Catmull-Rom ----------------
            float moveDist = carSpeed;
            if (trackPath.size() >= 4) {
                
                while (moveDist > 0.0f) {
                    float segLen = segmentLengths[currentSegment];
                    float remain = (1.0f - t) * segLen;
                    if (moveDist < remain) {
                        t += moveDist / segLen;
                        moveDist = 0.0f;
                    }
                    else {
                        moveDist -= remain;
                        t = 0.0f;
                        currentSegment = (currentSegment + 1) % trackPath.size();
                    }

                }

                // Catmull-Rom interpolation
                int idx1 = currentSegment;
                int idx0 = (idx1 == 0) ? trackPath.size() - 1 : idx1 - 1;
                int idx2 = (idx1 + 1) % trackPath.size();
                int idx3 = (idx1 + 2) % trackPath.size();

                carPosition = catmullRom(t, trackPath[idx0], trackPath[idx1], trackPath[idx2], trackPath[idx3]);

                glm::vec3 nextPos = catmullRom(t + 0.01f, trackPath[idx0], trackPath[idx1], trackPath[idx2], trackPath[idx3]);
                glm::vec3 dir = glm::normalize(nextPos - carPosition);
                carRotation = glm::degrees(atan2(dir.x, dir.z));
            }

            unifiedShader.setMat4("uM", modelTracks);
            tracks.Draw(unifiedShader);

            // Car
            glm::mat4 modelCar = glm::mat4(1.0f);
            modelCar = glm::translate(modelCar, carPosition);
            //modelCar = glm::rotate(modelCar, glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));
            modelCar = glm::scale(modelCar, glm::vec3(0.8f));
            unifiedShader.setMat4("uM", modelCar);
            car.Draw(unifiedShader);

            // Seats
            glm::mat4 modelSeats = glm::mat4(1.0f);
            modelSeats = glm::translate(modelSeats, carPosition + seatsOffset);
            //modelSeats = glm::rotate(modelSeats, glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));
            modelSeats = glm::scale(modelSeats, glm::vec3(0.8f));
            unifiedShader.setMat4("uM", modelSeats);
            seats.Draw(unifiedShader);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glfwTerminate();
        return 0;
    }


