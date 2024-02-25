#pragma once

#include "glm/glm.hpp"
#include <vector>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;

    Vertex(glm::vec3 position, glm::vec3 normal);
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

    TriangleStatistics();
};

class Mesh
{
public:
    std::vector<Vertex> vertices;
    std::vector<int> indices;

    Mesh(const char* path);
    Mesh(Mesh&& other)
        : vertices(std::move(other.vertices)), indices(std::move(other.indices))
    {
    }

    void CalculateStatistics(TriangleStatistics& stats, bool& didCalculate);
    bool IsPointInside(glm::vec3 p);
};
