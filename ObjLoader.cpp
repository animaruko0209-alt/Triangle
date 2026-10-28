#define NOMINMAX
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION
#include "third_party/tinyobjloader/tiny_obj_loader.h"
#include "ObjLoader.h"
#include <DirectXMath.h>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>

using namespace DirectX;

namespace
{
XMVECTOR Position(const Vertex& v)
{
    return XMVectorSet(v.x, v.y, v.z, 0.0f);
}

XMVECTOR OrthogonalTangent(FXMVECTOR normal)
{
    const XMVECTOR axis = std::fabs(XMVectorGetX(normal)) < 0.9f
        ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0);
    return XMVector3Normalize(axis - normal * XMVector3Dot(axis, normal));
}

void CalculateTangent(Vertex (&triangle)[3])
{
    const XMVECTOR e1 = Position(triangle[1]) - Position(triangle[0]);
    const XMVECTOR e2 = Position(triangle[2]) - Position(triangle[0]);
    const float du1 = triangle[1].u - triangle[0].u;
    const float dv1 = triangle[1].v - triangle[0].v;
    const float du2 = triangle[2].u - triangle[0].u;
    const float dv2 = triangle[2].v - triangle[0].v;
    const float determinant = du1 * dv2 - du2 * dv1;
    const bool hasUVFrame = std::fabs(determinant) > 1e-8f;
    const XMVECTOR rawTangent = hasUVFrame ? (e1 * dv2 - e2 * dv1) / determinant : XMVectorZero();
    const XMVECTOR rawBitangent = hasUVFrame ? (e2 * du1 - e1 * du2) / determinant : XMVectorZero();

    // Orthogonalize against each vertex's normal, preserving smooth shading.
    for (Vertex& vertex : triangle) {
        const XMVECTOR normal = XMVectorSet(vertex.nx, vertex.ny, vertex.nz, 0);
        XMVECTOR tangent = rawTangent - normal * XMVector3Dot(normal, rawTangent);
        if (XMVectorGetX(XMVector3LengthSq(tangent)) < 1e-12f)
            tangent = OrthogonalTangent(normal);
        else
            tangent = XMVector3Normalize(tangent);
        XMFLOAT3 stored;
        XMStoreFloat3(&stored, tangent);
        vertex.tx = stored.x; vertex.ty = stored.y; vertex.tz = stored.z;
        vertex.tangentSign = hasUVFrame &&
            XMVectorGetX(XMVector3Dot(XMVector3Cross(normal, tangent), rawBitangent)) < 0.0f ? -1.0f : 1.0f;
    }
}
}

bool LoadOBJ(const std::filesystem::path& filename, ObjMesh& mesh,
    std::string& error, std::string* warning)
{
    error.clear();
    if (warning) warning->clear();
    // filesystem::path keeps Japanese/other Unicode Windows filenames usable.
    std::ifstream file(filename, std::ios::binary);
    if (!file) { error = "Could not open the OBJ file."; return false; }
    class RelativeMaterialReader : public tinyobj::MaterialReader {
    public:
        explicit RelativeMaterialReader(std::filesystem::path base) : base_(std::move(base)) {}
        bool operator()(const std::string& name, std::vector<tinyobj::material_t>* materials,
            std::map<std::string, int>* materialMap, std::string* warn, std::string* err) override
        {
            std::ifstream mtl(base_ / std::filesystem::u8path(name));
            if (!mtl) {
                if (warn) *warn += "Could not open MTL: " + name + "\n";
                return false;
            }
            tinyobj::LoadMtl(materialMap, materials, &mtl, warn, err);
            return true;
        }
    private:
        std::filesystem::path base_;
    } materialReader(filename.parent_path());
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string parserWarning;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &parserWarning, &error,
        &file, &materialReader, true, false)) return false;
    if (file.bad()) { error = "Could not read the OBJ file."; return false; }
    if (warning) *warning = parserWarning;
    ObjMesh loaded;
    for (const auto& source : materials) {
        ObjMaterial mat;
        mat.name = source.name;
        for (int i = 0; i < 3; ++i) {
            mat.diffuse[i] = std::isfinite(source.diffuse[i]) ? source.diffuse[i] : 1.0f;
            mat.specular[i] = std::isfinite(source.specular[i]) ? source.specular[i] : 0.0f;
        }
        mat.opacity = std::isfinite(source.dissolve) ? (std::max)(0.0f, (std::min)(1.0f, source.dissolve)) : 1.0f;
        mat.shininess = std::isfinite(source.shininess) ? (std::max)(1.0f, source.shininess) : 32.0f;
        loaded.materials.push_back(mat);
        if (warning && (!source.diffuse_texname.empty() || !source.bump_texname.empty()))
            *warning += "MTL texture maps are not imported: " + source.name + "\n";
    }
    size_t skipped = 0;
    for (const auto& shape : shapes) {
        size_t offset = 0;
        for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face) {
            const auto faceSize = shape.mesh.num_face_vertices[face];
            if (faceSize != 3 || offset + 3 > shape.mesh.indices.size()) {
                error = "OBJ triangulation did not produce valid triangles.";
                return false;
            }
            Vertex triangle[3] = {};
            bool needsNormal[3] = {};
            for (size_t corner = 0; corner < 3; ++corner) {
                const auto& idx = shape.mesh.indices[offset + corner];
                if (idx.vertex_index < 0 || size_t(idx.vertex_index) >= attrib.vertices.size() / 3) {
                    error = "OBJ contains an invalid position index.";
                    return false;
                }
                const size_t position = size_t(idx.vertex_index) * 3;
                Vertex& v = triangle[corner];
                v.x = attrib.vertices[position]; v.y = attrib.vertices[position + 1]; v.z = attrib.vertices[position + 2];
                v.r = v.g = v.b = v.a = 1.0f;
                if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) {
                    error = "OBJ contains a non-finite position.";
                    return false;
                }
                if (idx.texcoord_index >= 0) {
                    if (size_t(idx.texcoord_index) >= attrib.texcoords.size() / 2) {
                        error = "OBJ contains an invalid UV index.";
                        return false;
                    }
                    const size_t uv = size_t(idx.texcoord_index) * 2;
                    v.u = attrib.texcoords[uv]; v.v = 1.0f - attrib.texcoords[uv + 1];
                    if (!std::isfinite(v.u) || !std::isfinite(v.v)) {
                        error = "OBJ contains non-finite UV coordinates.";
                        return false;
                    }
                }
                if (idx.normal_index >= 0) {
                    if (size_t(idx.normal_index) >= attrib.normals.size() / 3) {
                        error = "OBJ contains an invalid normal index.";
                        return false;
                    }
                    const size_t n = size_t(idx.normal_index) * 3;
                    v.nx = attrib.normals[n]; v.ny = attrib.normals[n + 1]; v.nz = attrib.normals[n + 2];
                }
                const float lengthSq = v.nx * v.nx + v.ny * v.ny + v.nz * v.nz;
                needsNormal[corner] = !std::isfinite(lengthSq) || lengthSq < 1e-12f;
                if (!needsNormal[corner]) {
                    const float inverseLength = 1.0f / std::sqrt(lengthSq);
                    v.nx *= inverseLength; v.ny *= inverseLength; v.nz *= inverseLength;
                }
            }
            offset += 3;
            XMVECTOR faceNormal = XMVector3Cross(Position(triangle[1]) - Position(triangle[0]),
                Position(triangle[2]) - Position(triangle[0]));
            if (XMVectorGetX(XMVector3LengthSq(faceNormal)) < 1e-12f) { ++skipped; continue; }
            XMFLOAT3 storedNormal;
            XMStoreFloat3(&storedNormal, XMVector3Normalize(faceNormal));
            for (size_t corner = 0; corner < 3; ++corner) {
                if (needsNormal[corner]) {
                    triangle[corner].nx = storedNormal.x;
                    triangle[corner].ny = storedNormal.y;
                    triangle[corner].nz = storedNormal.z;
                }
            }
            CalculateTangent(triangle);
            // Render() uses UINT byte widths and signed draw counts.
            if (loaded.vertices.size() > size_t((std::numeric_limits<int>::max)()) - 3 ||
                loaded.vertices.size() + 3 > size_t((std::numeric_limits<std::uint32_t>::max)()) / sizeof(Vertex)) {
                error = "OBJ exceeds the supported D3D11 vertex-buffer size.";
                return false;
            }
            int materialId = face < shape.mesh.material_ids.size() ? shape.mesh.material_ids[face] : -1;
            if (materialId < 0 || size_t(materialId) >= loaded.materials.size()) materialId = -1;
            if (!loaded.batches.empty() && loaded.batches.back().materialId == materialId)
                loaded.batches.back().indexCount += 3;
            else
                loaded.batches.push_back({static_cast<std::uint32_t>(loaded.indices.size()), 3, materialId});
            for (const Vertex& v : triangle) {
                loaded.indices.push_back(static_cast<std::uint32_t>(loaded.vertices.size()));
                loaded.vertices.push_back(v);
            }
        }
    }
    if (loaded.vertices.empty()) { error = "OBJ contains no drawable triangles."; return false; }
    if (skipped && warning) *warning += "Skipped " + std::to_string(skipped) + " degenerate triangle(s).\n";
    mesh = std::move(loaded);
    return true;
}
