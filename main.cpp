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
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include "mesh.h"
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
    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK);

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

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);
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

    Shader solidShader("shaders/shader.vert", "shaders/shader.frag");
    Shader wireframeShader("shaders/shader.vert", "shaders/wireframe.frag");
    Shader normalsShader("shaders/normal.vert", "shaders/normal.frag", "shaders/normal.geom");

    SDL_Event windowEvent;
    Uint32 prevTicks = SDL_GetTicks();
    float rotation = 0.0f;

    bool isOpenMeshPicker = false;
    std::vector<std::filesystem::path> meshFilePaths;
    TriangleStatistics meshStatistics;
    bool didCalculateStats = false;
    bool isCalculatingStats = false;
    bool isPointInside = false;
    bool didCalculatePoint = false;
    bool isWireframeRendering = false;
    bool isNormalRendering = false;

    bool isCameraMoveOn = false;

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
            if (windowEvent.type == SDL_MOUSEWHEEL)
                cameraPos.z += windowEvent.wheel.y * deltaTime * 10;
            if (windowEvent.type == SDL_MOUSEBUTTONDOWN)
                isCameraMoveOn = true;
            else if (windowEvent.type == SDL_MOUSEBUTTONUP)
                isCameraMoveOn = false;
            if (isCameraMoveOn && windowEvent.type == SDL_MOUSEMOTION && !ImGui::GetIO().WantCaptureMouse)
            {
                cameraPos.x -= windowEvent.motion.xrel * deltaTime * 0.2f;
                cameraPos.y += windowEvent.motion.yrel * deltaTime * 0.2f;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        glClearColor(0.045f, 0.045f, 0.045f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (didLoadMesh)
        {
            Shader& currentShader = isWireframeRendering ? wireframeShader : solidShader;
            glUseProgram(currentShader.id);
            glBindVertexArray(vao);

            rotation += deltaTime * M_PI * 5;
            model = glm::rotate(glm::mat4(1.0f), glm::radians(rotation), glm::vec3(1.0f, 1.0f, 0.0f)) * baseModel;
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

            currentShader.SetUniform("model", model);
            currentShader.SetUniform("view", view);
            currentShader.SetUniform("projection", proj);
            glm::vec3 lightPos(500.0f, 500.0f, 500.0f);
            currentShader.SetUniform("lightPos", lightPos);

            glPolygonMode(GL_FRONT_AND_BACK, isWireframeRendering ? GL_LINE : GL_FILL);
            glDrawElements(GL_TRIANGLES, mesh->indices.size(), GL_UNSIGNED_INT, nullptr);

            if (isNormalRendering)
            {
                glUseProgram(normalsShader.id);

                normalsShader.SetUniform("model", model);
                normalsShader.SetUniform("view", view);
                normalsShader.SetUniform("projection", proj);
                glDrawElements(GL_TRIANGLES, mesh->indices.size(), GL_UNSIGNED_INT, nullptr);
            }

            glBindVertexArray(0);
        }

        ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
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

            if (didLoadMesh)
                isLoadingMesh = false;
        }

        if (ImGui::Button("Reset Camera"))
            cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);

        if (ImGui::Button(isWireframeRendering ? "Smooth shading" : "Wireframe"))
            isWireframeRendering = !isWireframeRendering;

        if (ImGui::Button(isNormalRendering ? "Hide Normals" : "Show Normals"))
            isNormalRendering = !isNormalRendering;

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
                    didCalculatePoint = false;
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
            ImGui::TextUnformatted("Triangle Area Statistics:\nMax: -\nMin: -\nAvg: -");
        }

        if (ImGui::Button("Subdivide"))
        {
            mesh->Subdivide();
            PopulateBuffers(mesh->vertices, mesh->indices, vao, vbo, ibo);
        }

        static float point[3] = { 0.10f, 0.20f, 0.30f };

        if (ImGui::Button("Test Point Local"))
        {
            isPointInside = mesh->IsPointInside(glm::vec3(point[0], point[1], point[2]));
            didCalculatePoint = true;
        }

        ImGui::SameLine();

        const std::string pointResIndicator = std::string("Is point inside the mesh: ") + (didCalculatePoint ? (isPointInside ? "Yes" : "No") : "-");
        ImGui::TextUnformatted(pointResIndicator.c_str());
        ImGui::InputFloat3("", point);

        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(width - 240, 40));
        ImGui::SetNextWindowSize(ImVec2(200, 100));
        ImGuiWindowFlags flags = 0;
        flags |= ImGuiWindowFlags_NoBackground;
        flags |= ImGuiWindowFlags_NoMouseInputs;
        flags |= ImGuiWindowFlags_NoTitleBar;
        flags |= ImGuiWindowFlags_NoResize;
        ImGui::Begin("Stats", nullptr, flags);

        std::string vertexCountText = std::to_string(mesh->vertices.size()) + " vertices";
        float vertexCountTextW = ImGui::CalcTextSize(vertexCountText.c_str()).x;
        std::string triangleCountText = std::to_string(mesh->indices.size() / 3) + " triangles";
        float triangleCountTextW = ImGui::CalcTextSize(triangleCountText.c_str()).x;
        std::string indexCountText = std::to_string(mesh->indices.size()) + " indices";
        float indexCountTextW = ImGui::CalcTextSize(indexCountText.c_str()).x;

        ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - vertexCountTextW - ImGui::GetStyle().ItemSpacing.x);
        ImGui::TextUnformatted(vertexCountText.c_str());
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - triangleCountTextW - ImGui::GetStyle().ItemSpacing.x);
        ImGui::TextUnformatted(triangleCountText.c_str());
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - indexCountTextW - ImGui::GetStyle().ItemSpacing.x);
        ImGui::TextUnformatted(indexCountText.c_str());

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);

        prevTicks = currentTicks;
    }

    glDeleteProgram(wireframeShader.id);
    glDeleteProgram(solidShader.id);
    glDeleteProgram(normalsShader.id);
    SDL_GL_DeleteContext(context);
    SDL_GL_DeleteContext(loaderContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
