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


    Model lija("res/low-poly-fox.obj");
    Model tracks("res/tracks.obj");
    Model car("res/car.obj");
    Model seats("res/seats.obj");
    Model human("res/human.obj");
    //Tjemena i baferi su definisani u model klasi i naprave se pri stvaranju objekata

    Shader unifiedShader("basic.vert", "basic.frag");

    //Render petlja
    unifiedShader.use();
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
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 10.0f, -10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    //glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 20.0f, -60.0f), glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    unifiedShader.setMat4("uV", view);
    glm::mat4 model = glm::mat4(1.0f);

    glEnable(GL_DEPTH_TEST);
    while (!glfwWindowShouldClose(window))
    {

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //model = glm::rotate(model, glm::radians(0.1f), glm::vec3(0.0f, 1.0f, 0.0f));
        unifiedShader.setMat4("uM", model);
        //lija.Draw(unifiedShader);
        tracks.Draw(unifiedShader);
        car.Draw(unifiedShader);
        seats.Draw(unifiedShader);
        /*human.Draw(unifiedShader);*/

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}


