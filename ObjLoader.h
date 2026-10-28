#pragma once

#include "Vertex.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct ObjMaterial
{
    std::string name;
    float diffuse[3] = {1, 1, 1};
    float specular[3] = {1, 1, 1};
    std::filesystem::path diffuseTexture;
    std::filesystem::path normalTexture;
    float opacity = 1.0f;
    float shininess = 32.0f;
};

struct ObjMaterialBatch
{
    std::uint32_t startIndex, indexCount;
    int materialId; // -1 uses the renderer fallback
};

struct ObjMesh
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<ObjMaterial> materials;
    std::vector<ObjMaterialBatch> batches;
};

// Keeps the previous mesh intact on failure. MTL paths are relative to the OBJ.
// Imports numeric values and texture paths; GPU resources belong to the renderer.
bool LoadOBJ(const std::filesystem::path& filename, ObjMesh& mesh,
    std::string& error, std::string* warning = nullptr);
