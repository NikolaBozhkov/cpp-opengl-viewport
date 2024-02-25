#include <future>
#include <iostream>
#include <numeric>
#include <thread>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include "mesh.h"

Vertex::Vertex(glm::vec3 position, glm::vec3 normal)
    : position(position), normal(normal)
{
}

TriangleStatistics::TriangleStatistics()
    : minArea(FLT_MAX), maxArea(0), avgArea(0)
{
}

Mesh::Mesh(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        std::cerr << "Failed to open file" << std::endl;
        exit(1);
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
        exit(1);
    }

    if (!doc.IsObject() || !doc.HasMember("geometry_object"))
    {
        std::cerr << "Invalid JSON format" << std::endl;
        exit(1);
    }

    const rapidjson::Value& vertexObject = doc["geometry_object"];
    if (!vertexObject.IsObject() || !vertexObject.HasMember("vertices") || !vertexObject.HasMember("triangles"))
    {
        std::cerr << "Invalid vertex object format" << std::endl;
        exit(1);
    }

    const rapidjson::Value& verticesArray = vertexObject["vertices"];
    const rapidjson::Value& trianglesArray = vertexObject["triangles"];

    if (!verticesArray.IsArray() || !trianglesArray.IsArray())
    {
        std::cerr << "Invalid vertices or triangles array format" << std::endl;
        exit(1);
    }

    vertices.reserve(verticesArray.Size() / 3);
    for (rapidjson::SizeType i = 0; i <= verticesArray.Size() - 3; i += 3)
    {
        if (!verticesArray[i].IsNumber())
        {
            std::cerr << "Invalid vertex format" << std::endl;
            exit(1);
        }

        float x = verticesArray[i].GetFloat();
        float y = verticesArray[i + 1].GetFloat();
        float z = verticesArray[i + 2].GetFloat();

        vertices.emplace_back(glm::vec3(x, y, z), glm::vec3(0.0f));
    }

    indices.reserve(trianglesArray.Size());
    for (rapidjson::SizeType i = 0; i < trianglesArray.Size(); i++)
    {
        if (!trianglesArray[i].IsInt())
        {
            std::cerr << "Invalid index format" << std::endl;
            exit(1);
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
}

void Mesh::CalculateStatistics(TriangleStatistics& stats, bool& didCalculate)
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

    std::thread(
        [](std::vector<std::future<TriangleStatistics>> statsFutures, TriangleStatistics& stats, bool& didCalculate)
        {
            // Accumulate statistics from all threads
            stats = std::accumulate(
                std::begin(statsFutures), std::end(statsFutures), TriangleStatistics(),
                [](TriangleStatistics result, auto& current)
                {
                    const TriangleStatistics currentStats = current.get();
                    result.avgArea += currentStats.avgArea;
                    result.minArea = result.minArea > currentStats.minArea ? currentStats.minArea : result.minArea;
                    result.maxArea = result.maxArea < currentStats.maxArea ? currentStats.maxArea : result.maxArea;
                    return result;
                });
            didCalculate = true;
        },
        std::move(triangleStatsFutures), std::ref(stats), std::ref(didCalculate))
        .detach();
}

// Möller–Trumbore intersection (yoinked from Wikipedia)
bool DoesRayIntersectTriangle(glm::vec3 ray_origin, glm::vec3 ray_vector, const Triangle& triangle, glm::vec3& out_intersection_point)
{
    constexpr float epsilon = std::numeric_limits<float>::epsilon();

    glm::vec3 edge1 = triangle.vB->position - triangle.vA->position;
    glm::vec3 edge2 = triangle.vC->position - triangle.vA->position;
    glm::vec3 ray_cross_e2 = cross(ray_vector, edge2);
    float det = dot(edge1, ray_cross_e2);

    if (det > -epsilon && det < epsilon)
        return false;    // This ray is parallel to this triangle.

    float inv_det = 1.0 / det;
    glm::vec3 s = ray_origin - triangle.vA->position;
    float u = inv_det * dot(s, ray_cross_e2);

    if (u < 0 || u > 1)
        return false;

    glm::vec3 s_cross_e1 = cross(s, edge1);
    float v = inv_det * dot(ray_vector, s_cross_e1);

    if (v < 0 || u + v > 1)
        return false;

    // At this stage we can compute t to find out where the intersection point is on the line.
    float t = inv_det * dot(edge2, s_cross_e1);

    if (t > epsilon) // ray intersection
    {
        out_intersection_point = ray_origin + ray_vector * t;
        return true;
    }
    else // This means that there is a line intersection but not a ray intersection.
        return false;
}

bool Mesh::IsPointInside(const glm::vec3 p)
{
    const glm::vec3 rayOrigin = p;
    const glm::vec3 rayDirection = glm::vec3(1.0f, 1.0f, 0.0f);
    int intersectionCount = 0;
    glm::vec3 intersectionPoint;

    for (int i = 0; i < indices.size(); i += 3)
    {
        const Triangle triangle = Triangle::GetTriangle(vertices, indices, i);
        if (DoesRayIntersectTriangle(rayOrigin, rayDirection, triangle, intersectionPoint))
            intersectionCount++;
    }

    return intersectionCount % 2 == 1;
}
