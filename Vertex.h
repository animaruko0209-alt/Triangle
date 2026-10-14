#pragma once

// Shared by procedural geometry, OBJ geometry and the D3D11 input layout.
struct Vertex
{
    float x, y, z;
    float r, g, b, a;
    float u, v;
    float tx = 1.0f, ty = 0.0f, tz = 0.0f;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    float tangentSign = 1.0f;
};
