/*
 * SDL OpenGL Tutorial.
 * (c) Michael Vance, 2000
 * briareos@lokigames.com
 *
 * Distributed under terms of the LGPL.
 */

#include <SDL2/SDL.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#else
#include <GL/gl.h>
#endif

#include <iostream>
#include <thread>
#include <future>
#include <numeric>
#include <tuple>
#include <filesystem>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_opengl3.h"

#define CODE(...) #__VA_ARGS__

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;

    Vertex(glm::vec3 position, glm::vec3 normal)
        : position(position), normal(normal)
    {
    }
};

struct Triangle
{
    Vertex* vA;
    Vertex* vB;
    Vertex* vC;

    Triangle(Vertex* vA, Vertex* vB, Vertex* vC)
    : vA(vA), vB(vB), vC(vC)
    {
    }

    static const Triangle GetTriangle(std::vector<Vertex>& vertices, const std::vector<int>& indices, size_t i)
    {
        return Triangle(&vertices[indices[i]], &vertices[indices[i + 1]], &vertices[indices[i + 2]]);
    }

    glm::vec3 GetNormal() const
    {
        const glm::vec3 e1 = vA->position - vB->position;
        const glm::vec3 e2 = vC->position - vB->position;
        return glm::cross(e1, e2);
    }
};

struct TriangleStatistics
{
    float minArea;
    float maxArea;
    float avgArea;

    TriangleStatistics()
        : minArea(FLT_MAX), maxArea(0), avgArea(0)
    {
    }
};

int LoadMesh(const char* path, std::vector<Vertex>& vertices, std::vector<int>& indices)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        std::cerr << "Failed to open file" << std::endl;
        return 1;
    }

    // Parse json file
    char buffer[65536];
    rapidjson::FileReadStream inputStream(file, buffer, sizeof(buffer));

    rapidjson::Document doc;
    doc.ParseStream(inputStream);

    fclose(file);

    if (doc.HasParseError())
    {
        std::cerr << "Failed to parse JSON" << std::endl;
        return 1;
    }

    if (!doc.IsObject() || !doc.HasMember("geometry_object"))
    {
        std::cerr << "Invalid JSON format" << std::endl;
        return 1;
    }

    const rapidjson::Value& vertexObject = doc["geometry_object"];
    if (!vertexObject.IsObject() || !vertexObject.HasMember("vertices") || !vertexObject.HasMember("triangles")) {
        std::cerr << "Invalid vertex object format" << std::endl;
        return 1;
    }

    const rapidjson::Value& verticesArray = vertexObject["vertices"];
    const rapidjson::Value& trianglesArray = vertexObject["triangles"];

    if (!verticesArray.IsArray() || !trianglesArray.IsArray()) {
        std::cerr << "Invalid vertices or triangles array format" << std::endl;
        return 1;
    }

    vertices.reserve(verticesArray.Size() / 3);
    for (rapidjson::SizeType i = 0; i <= verticesArray.Size() - 3; i += 3)
    {
        if (!verticesArray[i].IsNumber())
        {
            std::cerr << "Invalid vertex format" << std::endl;
            return 1;
        }

        float x = verticesArray[i].GetFloat();
        float y = verticesArray[i + 1].GetFloat();
        float z = verticesArray[i + 2].GetFloat();

        vertices.emplace_back(glm::vec3(x, y, z), glm::vec3(0.0f));
    }

    vertices.reserve(trianglesArray.Size());
    for (rapidjson::SizeType i = 0; i < trianglesArray.Size(); i++)
    {
        if (!trianglesArray[i].IsInt())
        {
            std::cerr << "Invalid index format" << std::endl;
            return 1;
        }

        indices.emplace_back(trianglesArray[i].GetInt());
    }

    // Calculate smooth vertex normals (non-normalized)
    for (int i = 0; i < indices.size(); i += 3)
    {
        const Triangle triangle = Triangle::GetTriangle(vertices, indices, i);
        const glm::vec3 normal = triangle.GetNormal();

        triangle.vA->normal += normal;
        triangle.vB->normal += normal;
        triangle.vC->normal += normal;
    }

    return 0;
}

TriangleStatistics CalculateMeshStatistics(std::vector<Vertex>& vertices, std::vector<int>& indices)
{
    // Calculate triangle statistics using all available threads
    int triangleCount = indices.size() / 3;
    uint threadCount = std::thread::hardware_concurrency() > triangleCount ? triangleCount : std::thread::hardware_concurrency();

    std::vector<std::future<TriangleStatistics>> triangleStatsFutures;
    int start = 0;
    for (int i = 0; i < threadCount; i++)
    {
        // Calculate the number of triangles for the current workload
        int batchSize = (i * triangleCount + triangleCount) / threadCount - (i * triangleCount) / threadCount;

        triangleStatsFutures.emplace_back(std::async(
            std::launch::async,
            [triangleCount](std::vector<int>& indices, std::vector<Vertex>& vertices, int start, int end)
            {
                TriangleStatistics stats;

                for (int i = start; i < end; i += 3)
                {
                    const Triangle triangle = Triangle::GetTriangle(vertices, indices, i);
                    const float area = glm::length(triangle.GetNormal()) * 0.5;

                    if (stats.minArea > area && area != 0)
                        stats.minArea = area;
                    if (stats.maxArea < area)
                        stats.maxArea = area;
                    stats.avgArea += area / triangleCount;
                }

                return stats;
            },
            std::ref(indices), std::ref(vertices), start * 3, (start + batchSize) * 3));

        start += batchSize;
    }

    // Accumulate statistics from all threads
    TriangleStatistics stats = std::accumulate(
        std::begin(triangleStatsFutures), std::end(triangleStatsFutures), TriangleStatistics(),
        [](TriangleStatistics result, auto& current)
        {
            const TriangleStatistics currentStats = current.get();
            result.avgArea += currentStats.avgArea;
            result.minArea = result.minArea > currentStats.minArea ? currentStats.minArea : result.minArea;
            result.maxArea = result.maxArea < currentStats.maxArea ? currentStats.maxArea : result.maxArea;
            return result;
        });

    return stats;
}

void CreateBuffers(std::vector<Vertex>& vertices, const std::vector<int>& indices, uint& vao, uint& vbo, uint& ibo)
{
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), &indices[0], GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

int main(int argc, char *argv[])
{
    std::vector<Vertex> vertices;
    std::vector<int> indices;

    const char* meshFileName = "teapot";
    if (LoadMesh("task_input/teapot.json", vertices, indices) == 1)
        return 1;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    const int width = 1280, height = 720;
    SDL_Window *window = SDL_CreateWindow(
        "OpenGL", 100, 100, width, height, SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);
    glViewport(0, 0, 1280, 720);

    glEnable(GL_DEPTH_TEST);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init();

    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 baseModel = glm::rotate(model, -float(M_PI) / 2, glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(model, glm::vec3(0.4f));

    uint vao, vbo, ibo;
    CreateBuffers(vertices, indices, vao, vbo, ibo);

    // vertex shader
    const GLchar *vs_code = "#version 330 core\n" CODE(
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        out vec3 fragPosition;
        out vec3 normal;

        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
            fragPosition = vec3(model * vec4(aPos, 1.0));
            normal = normalize(mat3(transpose(inverse(model))) * aNormal);
        });

    uint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_code, NULL);
    glCompileShader(vs);

    int success;
    char infolog[512];
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vs, 512, NULL, infolog);
        printf("%s\n", infolog);
    }

    // fragment shader
    const GLchar *fs_code = "#version 330 core\n" CODE(
        in vec3 fragPosition;
        in vec3 normal;

        uniform vec3 lightPos;

        out vec4 color;

        void main() {
            vec3 lightDir = normalize(lightPos - fragPosition);

            float ambientStrength = 0.2f;
            vec3 ambient = ambientStrength * vec3(1.0f);

            float diff = max(dot(normal, lightDir), 0.0f);
            vec3 diffuse = diff * vec3(1.0f);

            vec3 result = (ambient + diffuse) * vec3(1.0f, 0.5f, 0.2f);
            color = vec4(result, 1.0f);
        });

    uint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_code, NULL);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fs, 512, NULL, infolog);
        printf("%s\n", infolog);
    }

    // create program
    uint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success)
    {
        glGetProgramInfoLog(program, 512, NULL, infolog);
        printf("%s\n", infolog);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    SDL_Event windowEvent;
    Uint32 prevTicks = SDL_GetTicks();
    float rotation = 0.0f;

    bool isOpenMeshPicker = false;
    std::vector<std::filesystem::path> meshFilePaths;
    TriangleStatistics meshStatistics;
    bool didCalculateStatsForCurrentMesh = false;

    while (true)
    {
        Uint32 currentTicks = SDL_GetTicks();
        float deltaTime = float(currentTicks - prevTicks) / 1000;

        ImGui_ImplSDL2_ProcessEvent(&windowEvent);

        if (SDL_PollEvent(&windowEvent))
        {
            if (windowEvent.type == SDL_QUIT)
                break;
            if (windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_ESCAPE)
                break;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        glClearColor(0.03f, 0.03f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);
        glBindVertexArray(vao);

        rotation += deltaTime * M_PI * 5;
        model = glm::rotate(glm::mat4(1.0f), glm::radians(rotation), glm::vec3(1.0f, 1.0f, 0.0f)) * baseModel;

        int modelLoc = glGetUniformLocation(program, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        int viewLoc = glGetUniformLocation(program, "view");
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

        int projectionLoc = glGetUniformLocation(program, "projection");
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(proj));

        int lightPosLoc = glGetUniformLocation(program, "lightPos");
        glUniform3fv(lightPosLoc, 1, glm::value_ptr(glm::vec3(5.0f, -10.0f, -1.0f)));

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);

        glBindVertexArray(0);

        ImGui::Begin("Demo");
        if (ImGui::Button("Choose Mesh"))
        {
            ImGui::OpenPopup("mesh_selection");
            isOpenMeshPicker = true;
            meshFilePaths.clear();
            std::string path = "./task_input";
            for (const auto& entry : std::filesystem::directory_iterator(path))
            {
                if (entry.path().extension().compare(".json") == 0)
                    meshFilePaths.push_back(entry.path());
            }
        }

        ImGui::SameLine();
        ImGui::TextUnformatted(meshFileName);
        if (ImGui::BeginPopup("mesh_selection"))
        {
            ImGui::SeparatorText("Task Input Meshes");
            for (int i = 0; i < meshFilePaths.size(); i++)
            {
                const auto path = meshFilePaths[i];
                if (ImGui::Selectable(path.filename().c_str(), false))
                {
                    vertices.clear();
                    indices.clear();
                    LoadMesh(path.c_str(), vertices, indices);
                    CreateBuffers(vertices, indices, vao, vbo, ibo);
                    isOpenMeshPicker = false;
                    didCalculateStatsForCurrentMesh = false;
                    meshFileName = path.stem().c_str();
                }
            }

            ImGui::EndPopup();
        }

        if (ImGui::Button("Calculate Statistics"))
        {
            meshStatistics = CalculateMeshStatistics(vertices, indices);
            didCalculateStatsForCurrentMesh = true;
        }

        if (didCalculateStatsForCurrentMesh)
            ImGui::Text("Triangle Area Statistics:\nMax: %f\nMin: %f\nAvg: %f", meshStatistics.maxArea, meshStatistics.minArea, meshStatistics.avgArea);
        else
            ImGui::Text("Triangle Area Statistics:\nMax: -\nMin: -\nAvg: -");

        ImGui::End();

        ImGui::ShowDemoWindow();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);

        prevTicks = currentTicks;
    }

    glDeleteProgram(program);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
