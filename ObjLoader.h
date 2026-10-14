#pragma once

#include "Vertex.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct ObjMesh
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

// Keeps the previous mesh intact on failure. Existing PNG materials are used
// by the renderer; OBJ material libraries are not imported.
bool LoadOBJ(const std::filesystem::path& filename, ObjMesh& mesh,
    std::string& error, std::string* warning = nullptr);
