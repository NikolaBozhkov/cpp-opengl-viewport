#include <SDL2/SDL.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#else
#include <GL/gl.h>
#endif

#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_sdl2.h"
#include "mesh.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "shader.h"

void GenerateBuffers(uint& vao, uint& vbo, uint& ibo)
{
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void PopulateBuffers(std::vector<Vertex>& vertices, const std::vector<int>& indices, uint& vao, uint& vbo, uint& ibo)
{
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), &indices[0], GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

int main(int argc, char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    const int width = 1280, height = 720;
    SDL_Window* window = SDL_CreateWindow(
        "OpenGL", 100, 100, width, height, SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GLContext loaderContext = SDL_GL_CreateContext(window);

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
    GenerateBuffers(vao, vbo, ibo);

    const char* meshFileName = "teapot";
    std::unique_ptr<Mesh> mesh;
    bool isLoadingMesh = false;
    bool didLoadMesh = false;

    auto loadMesh = [&](std::unique_ptr<Mesh>& m, std::string path)
    {
        Mesh* loadedMesh = new Mesh(path.c_str());
        m = std::make_unique<Mesh>(std::move(*loadedMesh));
        SDL_GL_MakeCurrent(window, loaderContext);
        PopulateBuffers(m->vertices, m->indices, vao, vbo, ibo);
        SDL_GL_MakeCurrent(window, context);
        didLoadMesh = true;
    };

    std::thread(loadMesh, std::ref(mesh), "task_input/teapot.json").detach();

    Shader shader("shader.vert", "shader.frag");

    SDL_Event windowEvent;
    Uint32 prevTicks = SDL_GetTicks();
    float rotation = 0.0f;

    bool isOpenMeshPicker = false;
    std::vector<std::filesystem::path> meshFilePaths;
    TriangleStatistics meshStatistics;
    bool didCalculateStats = false;
    bool isCalculatingStats = false;

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

        if (didLoadMesh)
        {
            glUseProgram(shader.id);
            glBindVertexArray(vao);

            rotation += deltaTime * M_PI * 5;
            model = glm::rotate(glm::mat4(1.0f), glm::radians(rotation), glm::vec3(1.0f, 1.0f, 0.0f)) * baseModel;

            shader.SetUniform("model", model);
            shader.SetUniform("view", view);
            shader.SetUniform("projection", proj);
            glm::vec3 lightPos(5.0f, -10.0f, -1.0f);
            shader.SetUniform("lightPos", lightPos);

            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDrawElements(GL_TRIANGLES, mesh->indices.size(), GL_UNSIGNED_INT, nullptr);

            glBindVertexArray(0);
        }

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

        // Mesh loading indicator
        if (isLoadingMesh)
        {
            ImGui::SameLine();
            ImGui::Text(" %c", "|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
        }

        if (didLoadMesh)
            isLoadingMesh = false;

        if (ImGui::BeginPopup("mesh_selection"))
        {
            ImGui::SeparatorText("Task Input Meshes");
            for (int i = 0; i < meshFilePaths.size(); i++)
            {
                const auto path = meshFilePaths[i];
                if (ImGui::Selectable(path.filename().c_str(), false))
                {
                    // Load new mesh
                    didLoadMesh = false;
                    isLoadingMesh = true;
                    std::thread(loadMesh, std::ref(mesh), path.string()).detach();

                    // Update state
                    isOpenMeshPicker = false;
                    didCalculateStats = false;
                    meshFileName = path.stem().c_str();
                }
            }

            ImGui::EndPopup();
        }

        if (ImGui::Button("Calculate Statistics") && !didCalculateStats && !isCalculatingStats)
        {
            isCalculatingStats = true;
            mesh->CalculateStatistics(meshStatistics, didCalculateStats);
        }

        // Calculation loading indicator
        if (isCalculatingStats)
        {
            ImGui::SameLine();
            ImGui::Text("%c", "|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
        }

        if (didCalculateStats)
        {
            isCalculatingStats = false;
            ImGui::Text("Triangle Area Statistics:\nMax: %f\nMin: %f\nAvg: %f", meshStatistics.maxArea, meshStatistics.minArea, meshStatistics.avgArea);
        }
        else
        {
            ImGui::Text("Triangle Area Statistics:\nMax: -\nMin: -\nAvg: -");
        }

        ImGui::End();

        ImGui::ShowDemoWindow();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);

        prevTicks = currentTicks;
    }

    glDeleteProgram(shader.id);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
