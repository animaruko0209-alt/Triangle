#include <windows.h> 
#include <d3d11.h> 
#include <d3dcompiler.h> 
#pragma comment(lib, "d3d11.lib") 
#pragma comment(lib, "d3dcompiler.lib")// シェーダー用 
#include <cmath> 
#include <iostream> 
#include <DirectXMath.h>// append 
#include <WICTextureLoader.h> /// append texture read 
// グローバル 
HWND g_hWnd = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
ID3D11Buffer* g_vb = nullptr;
ID3D11VertexShader* g_vs = nullptr;
ID3D11PixelShader* g_ps = nullptr;
ID3D11InputLayout* g_layout = nullptr;
/// DepthBuffer 
ID3D11DepthStencilView* g_dsv = nullptr;
// 頂点  + color 
struct Vertex
{
    float x, y, z;
    float r, g, b, a;
    float u, v;
};
using namespace DirectX;// append  
// 定数バッファ  append 
ID3D11Buffer* g_constantBuffer = nullptr;
// 回転角度     append 
float g_angle = 0.0f;
float xr = 0.0f;
float yr = 0.0f;
float zr = 1.0f;
// 時間     append 
float g_time = 0.0f;
////////////////// 
/// wave >wave2 append 
// 移動距離 
float xoffset = 0.0f, yoffset = 0.0f;
float zoffset = 0.0f;
float zoffset_back = 5.0f;
// 拡大・縮小 
float xscale = 1.0f, yscale = 1.0f;
// camera position 
float camera_xposition = 0.0f;
float camera_yposition = 0.0f;
float camera_zposition = -5.0f;
// ====================================================== 
// 定数バッファ構造体  New 
// ====================================================== 
struct ConstantBuffer
{
    XMMATRIX world;// model 
    XMMATRIX view;// camera 
    XMMATRIX proj;// view 

};
struct MaterialBuffer
{
    XMFLOAT4 color;

};
struct Material {
    ID3D11ShaderResourceView* diffuse;// Lanbert 照明 
    ID3D11ShaderResourceView* normal;// normal(法線) mapping 
    ID3D11ShaderResourceView* mask;// mask(マスク) mapping 
};
Material material;
// Texture Buffer  //// append 
ID3D11ShaderResourceView* g_texture0 = nullptr;// move = 0; diffuse 
ID3D11ShaderResourceView* g_texture1 = nullptr;// move = 1; diffuse 
//ID3D11ShaderResourceView* g_texture_normal = nullptr;//  normal 
// 
//ID3D11ShaderResourceView* g_texture_mask = nullptr;// mask 
// Sampler 
ID3D11SamplerState* g_sampler = nullptr;
ID3D11Buffer* g_materialBuffer = nullptr;
// Light Buffer (点光源 + アンビエント)
struct LightBuffer
{
    XMFLOAT4 lightPos;   // xyz = ライト位置, w = unused
    XMFLOAT4 atten;      // x=constant, y=linear, z=quadratic, w=unused
    XMFLOAT4 ambient;    // xyz = ambient color, w = ambient strength
};
ID3D11Buffer* g_lightBuffer = nullptr;
float light_x = 0.0f;
float light_y = -1.0f;
float light_z = -1.0f;
///  IndexBuffer  //// 
ID3D11Buffer* g_ib = nullptr;
UINT indices[100];// Index Array0 
UINT indices_back[100];// Index Array1 
int IndexCount0 = 0;
int IndexCount1 = 0;
//////////////////////////////////////// 
// ウィンドウプロシージャ 
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM
    lParam)
{
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);

}
Vertex vertices[100];// first object 
Vertex vertices_back[100];// second object 

// --- 既存の Triangle 系クラス群はそのまま残す ---
class Triangle {
public:
    Vertex vertices_[100];
    void MkRegular_polygon(int num, float radius);
    void Copy_polygon(int num, Vertex* v);
    void Copy_polygon_apex(int num, Vertex* v, float zp);
    Triangle();
};
Triangle::Triangle() {
    for (int i = 0; i < 100; i++) {
        vertices_[i] = { 0.0f, 0.0f,0.0f ,1.0f,1.0f,1.0f,1.0f,0.0f,0.0f }; //white  uv = { 0.0f,0.0f } 
    }
}
void Triangle::MkRegular_polygon(int num, float radius) {
    float x, y, z = 0.0f;
    float angle, radians;
    radians = 3.14159f / 180.0f;
    Vertex* p;
    p = vertices_;
    for (int i = num; i > 0; i--) {
        angle = 360.0f / (float)num * i;
        x = radius * std::cos(angle * radians);
        y = radius * std::sin(angle * radians);
        *p = { x , y,  z ,1.0f,1.0f,0.0f,1.0f ,0.0f,0.0f };// yellow uv ={0.0f,0.0f} 
        std::cout << x; std::cout << y; std::cout << z;

        p++;
    }
}
void Triangle::Copy_polygon(int num, Vertex * v) {
    Vertex* p = vertices_;
    for (int i = 0; i < num; i++) {
        *v = *p;//first vertex 
        v->u = 0.0f;
        v->v = 0.0f;
        v++;
        if (i == (num - 1)) {
            *v = *(vertices_);
            v->u = 1.0f;
            v->v = 0.0f;
        }
        else {
            *v = *(p + 1);//Second vertex 
            v->u = 1.0f;
            v->v = 0.0f;
            p++;
        }
        v++;
        *v = { 0.0f,0.0f,0.0f,1.0f,1.0f,1.0f,1.0f,0.5f,1.0f };// center White; uv ={ 0.5f,1.0f} 
        v++;

    }
}
void Triangle::Copy_polygon_apex(int num, Vertex * v, float zp) {
    Vertex* p = vertices_;
    for (int i = 0; i < num; i++) {
        if (i == (num - 1)) {
            *v = *(vertices_);
        }
        else {
            *v = *(p + 1);//first vertex 
        }
        v->u = 0.0f;
        v->v = 0.0f;
        v++;
        *v = *(p);//Second vertex 
        v->u = 1.0f;
        v->v = 0.0f;
        p++;
        v++;
        *v = { 0.0f,0.0f,zp ,1.0f,0.0f,0.0f,1.0f,0.5f,1.0f };// center zp= apex zposition Red uv = { 0.5f,1.0f }
        v++;
    }
}
class Triangle2 : public Triangle {
private:
    float radius;
public:
    Triangle2(float radius);
    void MkRegular_polygon2(int num);
    void Copy_polygon2(int num, Vertex* v);
    void Copy_polygon_apex2(int num, Vertex* v, float zp);
};
Triangle2::Triangle2(float r = 0.5) :Triangle() {
    radius = r;
}
void Triangle2::MkRegular_polygon2(int num) {
    MkRegular_polygon(num, radius);
}
void Triangle2::Copy_polygon2(int num, Vertex * v) {
    Copy_polygon(num, v);
}
void Triangle2::Copy_polygon_apex2(int num, Vertex * v, float zp) {
    Copy_polygon_apex(num, v, zp);
}
class TriangleIndex : public Triangle {/// 2026/7/22 append 
private:
    float radius;
public:
    TriangleIndex(float radius);
    void  MkRegular_polygonIndex(int num, Vertex* v, float zp);//正多角錐の頂点座標を作成
        int  MkRegular_polyHedronIndex(int num, UINT* indieces_);//正多角錐のインデックスを作成
        void  MkCylinder_Vertexs(int num, Vertex* v, float zp);//円柱の頂点座標を作成 底面は正num 角形 
        int  MkCylinder_Index(int num, UINT* indieces_);//円柱のインデックスを作成 
        void  MkCylinder_Vertexs2(int num, Vertex* v, float zp);//円柱の頂点座標を作成 (底面は正num角形
        int  MkCylinder_Index2(int num, UINT* indieces_);
};
TriangleIndex::TriangleIndex(float r = 0.5) :Triangle() {
    radius = r;
}
void TriangleIndex::MkRegular_polygonIndex(int num, Vertex * v, float zp) {
    float x, y, z = 0.0f;
    float angle, radians;
    float u_position;
    radians = 3.14159f / 180.0f;
    Vertex* p;
    p = v;
    for (int i = num; i > 0; i--) {
        angle = 360.0f / (float)num * i;
        x = radius * std::cos(angle * radians);
        y = radius * std::sin(angle * radians);
        u_position = 1.0f - 1.0f / (float)num * (float)i;
        *p = { x , y,  z ,1.0f,1.0f,0.0f,1.0f ,u_position,0.0f };// yellow uv ={u_positon,0.0f} 
        p++;
    }
    *p = { 0.0f,0.0f,0.0f,1.0f,1.0f,1.0f,1.0f,0.5f,1.0f };// center White; uv ={ 0.5f,1.0f} 
    p++;
    *p = { 0.0f,0.0f,zp ,1.0f,0.0f,0.0f,1.0f,0.5f,1.0f };// center zp= apex zposition Red uv = { 0.5f,1.0f }
    p++;
}
int  TriangleIndex::MkRegular_polyHedronIndex(int num, UINT * indices_)//正多角錐のイデックスを作成
{
    int IndexCount = 0;
// base polygon; 
UINT* u;
u = indices_;
for (int i = 0; i < num; i++) {
     *u = i;  std::cout << "[ " << *u << ","; IndexCount++; u++;
    if (i == (num - 1)) {
        //indices[IndexCount] = 0; 
        *u = 0;
    }
    else {
        //indices[IndexCount] = i + 1; 
        *u = i + 1;
    }
     std::cout << *u << ","; IndexCount++; u++;
     *u = num;// ceter Index number; 
     std::cout << *u << "] \n"; IndexCount++; u++;
}

// side polygon; 
for (int i = 0; i < num; i++) {
    if (i == (num - 1)) {
        //indices[IndexCount] = 0; 
        *u = 0;
    }
    else {
        //indices[IndexCount] = i + 1; 
        *u = i + 1;
    }
     std::cout << "[ " << *u << ","; IndexCount++; u++;

     *u = i;  std::cout << *u << ","; IndexCount++; u++;

*u = num + 1;// Apex Index number; 
std::cout << *u << "] \n"; IndexCount++; u++;
}
return IndexCount;
}

void TriangleIndex::MkCylinder_Vertexs(int num, Vertex* v, float zp) {//円柱の頂点座標を作成 
    float x, y, z = 0.0f;
    float angle, radians;
    float u_position;
    radians = 3.14159f / 180.0f;
    Vertex* p;
    p = v;
    for (int i = num; i > 0; i--) {
        angle = 360.0f / (float)num * i;
        x = radius * std::cos(angle * radians);
        y = radius * std::sin(angle * radians);
        u_position = 1.0f - 1.0f / (float)num * (float)i;

        *p = { x , y,  z ,1.0f,1.0f,0.0f,1.0f ,u_position,0.0f };// yellow uv ={u_positon,0.0f} 

        p++;
    }
    *p = { 0.0f,0.0f,0.0f,1.0f,1.0f,1.0f,1.0f,0.5f,1.0f };// center base White; uv ={ 0.5f,1.0f} 
    p++;
    for (int i = num; i > 0; i--) {
        angle = 360.0f / (float)num * i;
        x = radius * std::cos(angle * radians);
        y = radius * std::sin(angle * radians);
        z = zp;
        u_position = 1.0f - 1.0f / (float)num * (float)i;

        *p = { x , y,  z ,1.0f,1.0f,0.0f,1.0f ,u_position,1.0f };// yellow uv ={u_positon,0.0f,1.0f} 

        p++;
    }

    *p = { 0.0f,0.0f,zp ,1.0f,0.0f,0.0f,1.0f,0.5f,1.0f };// center top zp= apex zposition Red uv ={ 0.5f,1.0f} 
    p++;
}

int  TriangleIndex::MkCylinder_Index(int num, UINT* indices_)//円柱のインデックスを作成 
{
    int IndexCount = 0;
    // base polygon;(円柱の底面) 
    UINT* u;
    u = indices_;
    for (int i = 0; i < num; i++) {
        *u = i;  std::cout << "[ " << *u << ","; IndexCount++; u++;
        if (i == (num - 1)) {
            //indices[IndexCount] = 0; 
            *u = 0;
        }
        else {
            //indices[IndexCount] = i + 1; 
            *u = i + 1;
        }
        std::cout << *u << ","; IndexCount++; u++;
        *u = num;// base center Index number; 
        std::cout << *u << "] \n"; IndexCount++; u++;
    }
    // top polygon; (円柱の上面) 
    for (int i = num + 1; i < (num + num + 1); i++) {
        if (i == (num * 2)) {
            //indices[IndexCount] = 0; 
            *u = num + 1;
        }
        else {
            //indices[IndexCount] = i + 1; 
            *u = i + 1;
        }
        std::cout << "[ " << *u << ","; IndexCount++; u++;

        *u = i;  std::cout << *u << ","; IndexCount++; u++;

        *u = num * 2 + 1;// top center Index number; 
        std::cout << *u << "] \n"; IndexCount++; u++;

    }
    // side polygon(円柱の側面) 
    for (int i = 0; i < num; i++) {
        // left triangle 
        if (i == (num - 1)) {//num == 3,0 
            //indices[IndexCount] = 0; 
            *u = 0;
        }
        else {//1 
            //indices[IndexCount] = i + 1; 
            *u = i + 1;
        }
        std::cout << "[ " << *u << ","; IndexCount++; u++;

        *u = i;  std::cout << *u << ","; IndexCount++; u++;//0 

        *u = i + num + 1;//5 
        std::cout << *u << " ]\n"; IndexCount++; u++;
        // right triangle 
        if (i == (num - 1)) {// num == 3 ,0 
            *u = 0; std::cout << "[ " << *u << ","; IndexCount++; u++;//0 
        }
        else {
            *u = i + 1; std::cout << "[ " << *u << ","; IndexCount++; u++;//1 
        }

        *u = i + num + 1; std::cout << *u << ","; IndexCount++; u++;//5  num == 3,8 
        if (i == (num - 1)) {
            *u = i + 2;// num == 3 ,5 
        }
        else {//6 
            *u = i + num + 2;
        }
        std::cout << *u << " ] \n"; IndexCount++; u++;

    }

    return IndexCount;
}
void TriangleIndex::MkCylinder_Vertexs2(int num, Vertex* v, float zp) {//円柱の頂点座標を作成2（円周の最初と最後の頂点を共有しない頂点座標）
        float x, y, z = 0.0f;
    float angle, radians;
    float u_position;
    radians = 3.14159f / 180.0f;
    Vertex* p;
    p = v;
    int IndexCount = 0;
    for (int i = num; i >= 0; i--) {
        angle = 360.0f / (float)num * i;
        x = radius * std::cos(angle * radians);
        y = radius * std::sin(angle * radians);
        u_position = 1.0f - 1.0f / (float)num * (float)i;
        std::cout << IndexCount << ": "; IndexCount++;
        *p = { x , y,  z ,1.0f,1.0f,0.0f,1.0f ,u_position,0.0f };// yellow uv ={u_positon,0.0f} 
        std::cout << p->x << "," << p->y << "," << p->z << "," << p->r << "," << p->g << "," <<
            p->b << "," << p->a << "," << p->u << "," << p->v << " \n";
        p++;
    }
    std::cout << IndexCount << ": "; IndexCount++;
    *p = { 0.0f,0.0f,0.0f,1.0f,1.0f,1.0f,1.0f,0.5f,1.0f };// center base White; uv ={ 0.5f,1.0f} 
    std::cout << p->x << "," << p->y << "," << p->z << "," << p->r << "," << p->g << "," << p->b
        << "," << p->a << "," << p->u << "," << p->v << " \n";
    p++;
    for (int i = num; i >= 0; i--) {
        angle = 360.0f / (float)num * i;
        x = radius * std::cos(angle * radians);
        y = radius * std::sin(angle * radians);
        z = zp;
        u_position = 1.0f - 1.0f / (float)num * (float)i;
        std::cout << IndexCount << ": "; IndexCount++;
        *p = { x , y,  z ,1.0f,1.0f,0.0f,1.0f ,u_position,1.0f };// yellow uv ={u_positon,0.0f,1.0f} 
        std::cout << p->x << "," << p->y << "," << p->z << "," << p->r << "," << p->g << "," <<
            p->b << "," << p->a << "," << p->u << "," << p->v << " \n";
        p++;
    }
    std::cout << IndexCount << ": "; IndexCount++;
    *p = { 0.0f,0.0f,zp ,1.0f,0.0f,0.0f,1.0f,0.5f,1.0f };// center top zp= apex zposition Red uv ={ 0.5f,1.0f} 
    std::cout << p->x << "," << p->y << "," << p->z << "," << p->r << "," << p->g << "," << p->b
        << "," << p->a << "," << p->u << "," << p->v << " \n";
    p++;
}
int  TriangleIndex::MkCylinder_Index2(int num, UINT* indices_)//円柱のインデックスを作成 2 
{
    int IndexCount = 0;
    // base polygon;(円柱の底面) 
    UINT* u;
    u = indices_;
    for (int i = 0; i < num; i++) {
        *u = i;  std::cout << "[ " << *u << ","; IndexCount++; u++;
        //indices[IndexCount] = i + 1; 
        *u = i + 1;
        std::cout << *u << ","; IndexCount++; u++;
        *u = num + 1;// base center Index number; 
        std::cout << *u << "] \n"; IndexCount++; u++;
    }
    // top polygon;(円柱の上面) 
    for (int i = num + 2; i < (num + num + 2); i++) {
        //indices[IndexCount] = i + 1; 
        *u = i + 1;
        std::cout << "[ " << *u << ","; IndexCount++; u++;
        *u = i;  std::cout << *u << ","; IndexCount++; u++;

        *u = num * 2 + 3;// top center Index number; 
        std::cout << *u << "] \n"; IndexCount++; u++;

    }
    // side polygon(円柱の側面) 
    for (int i = 0; i < num; i++) {
        // left triangle 

        //indices[IndexCount] = i + 1; 
        *u = i + 1;

        std::cout << "[ " << *u << ","; IndexCount++; u++;

        *u = i;  std::cout << *u << ","; IndexCount++; u++;//0 

        *u = i + num + 2;//6 
        std::cout << *u << " ]\n"; IndexCount++; u++;
        // right triangle 

        *u = i + 1; std::cout << "[ " << *u << ","; IndexCount++; u++;//1 


        *u = i + num + 2; std::cout << *u << ","; IndexCount++; u++;//6  num == 3,8 

        *u = i + num + 3;//7 num == 3,10 

        std::cout << *u << " ] \n"; IndexCount++; u++;

    }
    return IndexCount;
}

// --- ここから Cube クラスを追加（既存のコードはそのまま） ---
Vertex vertices_cube[24];
UINT indices_cube[36];
int IndexCountCube = 36;

class Cube {
public:
    Vertex v[24];
    UINT idx[36];

    // 一辺 length の立方体を原点中心で作成（UV と色を簡易設定）
    void Make(float length)
    {
        const float h = length * 0.5f;
        // +X face
        v[0] = { h, -h, -h, 1,0,0,1, 0.0f, 1.0f };
        v[1] = { h,  h, -h, 1,0,0,1, 1.0f, 1.0f };
        v[2] = { h,  h,  h, 1,0,0,1, 1.0f, 0.0f };
        v[3] = { h, -h,  h, 1,0,0,1, 0.0f, 0.0f };
        // -X face
        v[4] = { -h, -h,  h, 0,1,0,1, 0.0f, 1.0f };
        v[5] = { -h,  h,  h, 0,1,0,1, 1.0f, 1.0f };
        v[6] = { -h,  h, -h, 0,1,0,1, 1.0f, 0.0f };
        v[7] = { -h, -h, -h, 0,1,0,1, 0.0f, 0.0f };
        // +Y face
        v[8]  = { -h,  h, -h, 0,0,1,1, 0.0f, 1.0f };
        v[9]  = { -h,  h,  h, 0,0,1,1, 1.0f, 1.0f };
        v[10] = {  h,  h,  h, 0,0,1,1, 1.0f, 0.0f };
        v[11] = {  h,  h, -h, 0,0,1,1, 0.0f, 0.0f };
        // -Y face
        v[12] = { -h, -h,  h, 1,1,0,1, 0.0f, 1.0f };
        v[13] = { -h, -h, -h, 1,1,0,1, 1.0f, 1.0f };
        v[14] = {  h, -h, -h, 1,1,0,1, 1.0f, 0.0f };
        v[15] = {  h, -h,  h, 1,1,0,1, 0.0f, 0.0f };
        // +Z face
        v[16] = { -h, -h, -h, 1,0,1,1, 0.0f, 1.0f };
        v[17] = { -h,  h, -h, 1,0,1,1, 1.0f, 1.0f };
        v[18] = {  h,  h, -h, 1,0,1,1, 1.0f, 0.0f };
        v[19] = {  h, -h, -h, 1,0,1,1, 0.0f, 0.0f };
        // -Z face
        v[20] = {  h, -h,  h, 0,1,1,1, 0.0f, 1.0f };
        v[21] = {  h,  h,  h, 0,1,1,1, 1.0f, 1.0f };
        v[22] = { -h,  h,  h, 0,1,1,1, 1.0f, 0.0f };
        v[23] = { -h, -h,  h, 0,1,1,1, 0.0f, 0.0f };

        // 各面 2 三角形
        for (int f = 0; f < 6; ++f) {
            int b = f * 4;
            int ii = f * 6;
            idx[ii + 0] = b + 0;
            idx[ii + 1] = b + 1;
            idx[ii + 2] = b + 2;
            idx[ii + 3] = b + 0;
            idx[ii + 4] = b + 2;
            idx[ii + 5] = b + 3;
        }
    }

    void CopyVertices(Vertex* dest) const
    {
        memcpy(dest, v, sizeof(v));
    }
    void CopyIndices(UINT* dest) const
    {
        memcpy(dest, idx, sizeof(idx));
    }
};
/// --- Cube クラス追加ここまで ---

// 初期化 
bool InitD3D()
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = 800;
    scd.BufferDesc.Height = 600;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = g_hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    if (FAILED(D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &scd, &g_swapChain, &g_device, nullptr, &g_context)))
        return false;
    // InitD3D() DepthBuffer //////////// append 
    ID3D11Texture2D* depthBuffer = nullptr;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 800;
    desc.Height = 600;
    desc.MipLevels = 1;
    desc.ArraySize = 1;

    desc.Format =
        DXGI_FORMAT_D24_UNORM_S8_UINT;

    desc.SampleDesc.Count = 1;

    desc.BindFlags =
        D3D11_BIND_DEPTH_STENCIL;


    HRESULT hr =
        g_device->CreateTexture2D(
            &desc,
            nullptr,
            &depthBuffer);

    if (FAILED(hr))
    {
        MessageBoxA(nullptr,
            "DepthBufferの作成に失敗しました",
            "Error",
            MB_OK);

        return false;
    }

    //g_device->CreateTexture2D( 
    //    &desc, 
    //    nullptr, 
    //    &depthBuffer); 
    //////////////////////////////////// append  

    /*g_device->CreateDepthStencilView(
        depthBuffer,
        nullptr,
        &g_dsv);*/

    hr = g_device->CreateDepthStencilView(
        depthBuffer,
        nullptr,
        &g_dsv);

    if (FAILED(hr))// append alert  2026/7/5 
    {
        depthBuffer->Release();
        return false;
    }

    depthBuffer->Release();// append alert 2026/7/5 


    ID3D11Texture2D* backBuffer = nullptr;
    hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

    if (FAILED(hr))// append alert 2026/7/5 
    {
        backBuffer->Release();
        return false;
    }
    hr = g_device->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);

    if (FAILED(hr))// append alert 2026/7/5 
    {
        backBuffer->Release();
        return false;
    }
    backBuffer->Release();

    g_context->OMSetRenderTargets(1, &g_rtv, g_dsv);/// append g_dsv 

    D3D11_VIEWPORT vp = {};
    vp.Width = 800;
    vp.Height = 600;
    vp.MinDepth = 0;
    vp.MaxDepth = 1;
    g_context->RSSetViewports(1, &vp);
    //D3D11_BUFFER_DESC bd = {};      
  //  >> > 2026 / 7 / 22 move Render()
        //bd.Usage = D3D11_USAGE_DYNAMIC; 
        //bd.ByteWidth = sizeof(Vertex) * 100; // 最大数 
        //bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; 
        //bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; 
        ////g_device->CreateBuffer(&bd, nullptr, &g_vb); 
        //D3D11_SUBRESOURCE_DATA data = {}; 
        //data.pSysMem = vertices; 
        //g_device->CreateBuffer(&bd, &data, &g_vb); 
        //if (g_vb == nullptr) 
        //{ 
        //   
        //       
        //       
        //       
        //       
        //} 
        MessageBoxA(
            nullptr,
            "g_vb == nullptr",
            "Error",
            MB_OK);
    // 頂点シェーダー  (Raw data Type) 
    const char* vsCode = R"( 
cbuffer ConstantBuffer : register(b0) 
    { 
      matrix world; 
      matrix view; 
      matrix proj; 
    }; 
    struct VS_IN 
    { 
        float3 pos : POSITION; 
        float4 col : COLOR; 
        float2 uv : TEXCOORD; 
    }; 
    struct PS_IN 
    { 
        float4 pos : SV_POSITION; 
        float4 col : COLOR; 
        float2 uv : TEXCOORD; 
        float3 worldPos : TEXCOORD1; // ワールド空間位置をピクセルシェーダに渡す
    }; 
    PS_IN main(VS_IN input) 
    { 
        PS_IN output; 
 
        float4 worldPos4 = mul(float4(input.pos, 1.0f), world); // ワールド空間位置
        float4 p = mul(worldPos4, view);
        p = mul(p, proj);
 
        output.pos = p; 
        output.worldPos = worldPos4.xyz;
 
        output.col = input.col; 
        //// append uv 
        output.uv = input.uv; 
 
        return output; 
    } 
    )";

    /// ピクセルシェーダー(Raw data Type) 
    const char* psCode = R"( 
    cbuffer MaterialBuffer : register(b1) 
    { 
        float4 color; 
    }; 
    cbuffer LightBuffer : register(b2) 
    { 
        float4 lightPos; 
        float4 atten; 
        float4 ambient; 
    }; 
    Texture2D diffuse : register(t0); 
    Texture2D normal : register(t1); 
    Texture2D mask : register(t2); 
 
    SamplerState sam : register(s0); 
 
    struct PS_IN 
    { 
        float4 pos : SV_POSITION; 
        float4 col : COLOR; 
        float2 uv : TEXCOORD; 
        float3 worldPos : TEXCOORD1; 
    }; 

    float4 main(PS_IN input) : SV_TARGET 
    { 
     float4  diffuseColor = diffuse.Sample(sam,input.uv); 
 
    // 画素ごとにジオメトリ法線を導出（面の向きを正確に反映）
    float3 n = normalize(cross(ddx(input.worldPos), ddy(input.worldPos)));
    n = normalize(n);

    // 点光源の計算
    float3 L = lightPos.xyz - input.worldPos; // ライトからサーフェスへのベクトル
    float dist = length(L);
    float3 Ldir = normalize(L);

    // 減衰 (attenuation)
    float att = 1.0f / max(atten.x + atten.y * dist + atten.z * dist * dist, 0.0001f);

    float diffuseLight = saturate(dot(n, Ldir)) * att; // 法線との内積に減衰を乗算

    // アンビエント成分 (ambient.xyz = 色, ambient.w = 強さ)
    float3 ambientComponent = ambient.xyz * ambient.w;

    float maskValue = mask.Sample(sam,input.uv).r; 
 
    float3 lighting = diffuseLight + ambientComponent;
 
    return  diffuseColor * float4(lighting,1.0f) * maskValue * input.col * color; 
    } 
    )";


    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;

    D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr,
        "main", "vs_4_0", 0, 0, &vsBlob, nullptr);

    ID3DBlob* errorBlob = nullptr;

    hr = D3DCompile(
        psCode,
        strlen(psCode),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_4_0",
        0,
        0,
        &psBlob,
        &errorBlob);

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            MessageBoxA(
                nullptr,
                (char*)errorBlob->GetBufferPointer(),
                "Pixel Shader Error",
                MB_OK);

            errorBlob->Release();
        }

        return false;
    }
    /*   D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr,
           "main", "ps_4_0", 0, 0, &psBlob, nullptr);*/

    g_device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr, &g_vs);

    g_device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr, &g_ps);

    // -------------------------------------------------- 
    // 入力レイアウト 
    // -------------------------------------------------- 

    // Vertex color  
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,
          D3D11_INPUT_PER_VERTEX_DATA,0},
{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,
D3D11_INPUT_PER_VERTEX_DATA,0},
{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,28,//// append 
D3D11_INPUT_PER_VERTEX_DATA,0},
    };
    //////// Vertex color  
    g_device->CreateInputLayout(
        layout, ARRAYSIZE(layout),////  ARRAYSIZE(layout) change  
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &g_layout);
    ///////////////////////////// 
    vsBlob->Release();
    psBlob->Release();
    // -------------------------------------------------- 
    // 定数バッファ  append 
    // -------------------------------------------------- 
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(ConstantBuffer);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.Usage = D3D11_USAGE_DEFAULT;
    g_device->CreateBuffer(
        &cbd,
        nullptr,
        &g_constantBuffer);
    ////////////////////////////////////// 
    // Material Bufffer >>DepthBuffer2 
    D3D11_BUFFER_DESC mbd = {};

    mbd.ByteWidth = sizeof(MaterialBuffer);
    mbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    mbd.Usage = D3D11_USAGE_DEFAULT;

    hr = g_device->CreateBuffer(
        &mbd,
        nullptr,
        &g_materialBuffer);

    if (FAILED(hr))
    {
        MessageBoxA(nullptr,
            "g_materialBufferの作成に失敗しました",
            "Error",
            MB_OK);

        return false;
    }

    // Texture Read 0,1 
    CreateWICTextureFromFile(
        g_device,
        g_context,
        L"check.png",
        nullptr,
        &g_texture0);

    CreateWICTextureFromFile(
        g_device,
        g_context,
        L"check2.png",
        nullptr,
        &g_texture1);

    CreateWICTextureFromFile(
        g_device,
        g_context,
        L"mask5b.png",
        nullptr,
        &material.normal);

    CreateWICTextureFromFile(
        g_device,
        g_context,
        L"mask5b_normal.png",
        nullptr,
        &material.mask);

    // Sampler create 
    D3D11_SAMPLER_DESC sd = {};

    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

    g_device->CreateSamplerState(
        &sd,
        &g_sampler);

    // Light Buffer Create 2026/7/14 
    D3D11_BUFFER_DESC ld = {};

    ld.ByteWidth = sizeof(LightBuffer);
    ld.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ld.Usage = D3D11_USAGE_DEFAULT;

    g_device->CreateBuffer(&ld, nullptr, &g_lightBuffer);

    /// IndexBuffer  >> Render() 移動します 

    return true;
}
///  
/// Renderする前にviewをクリアー 
void Clear_view()
{
    float color[4] = { 0.1f, 0.2f, 0.4f, 1 };

    g_context->ClearRenderTargetView(g_rtv, color);

    //// DepthBuffer Clear ///////append 
    g_context->ClearDepthStencilView(
        g_dsv,
        D3D11_CLEAR_DEPTH,
        1.0f,
        0);
}

void Render(Vertex* vertices_, int vertexCount, UINT* indices_, int indexCount, int move)// Render change 7 / 22
{

    // >> from InitD3D() 
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(Vertex) * vertexCount; // Vertexサイズ X 頂点数 
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = vertices_;

    HRESULT hr = g_device->CreateBuffer(&bd, &data, &g_vb);
    if (FAILED(hr))
    {
        MessageBoxA(nullptr, "g_vb failed", "Error", MB_OK);
        return;
    }


    // ★ 頂点バッファ更新  7/22 移動 
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = g_context->Map(g_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    if (FAILED(hr))
    {
        MessageBoxA(nullptr, "Map(g_vb) failed", "Error", MB_OK);
        return;
    }
    memcpy(mapped.pData, vertices_, sizeof(Vertex)* vertexCount);

    g_context->Unmap(g_vb, 0);

    // インデックスバッファ追加 7/22 
    D3D11_BUFFER_DESC ibd = {};

    //ibd.Usage = D3D11_USAGE_DEFAULT; 
    ibd.Usage = D3D11_USAGE_DYNAMIC;
    ibd.ByteWidth = sizeof(UINT) * indexCount;// indicesのサイズ X インデックス数 
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA idata = {};
    idata.pSysMem = indices_;

    hr = g_device->CreateBuffer(
        &ibd,
        &idata,
        &g_ib);

    if (FAILED(hr))
    {
        MessageBoxA(nullptr, "g_ib failed", "Error", MB_OK);
        return;
    }
    // ★ インデックスバッファ更新 7/22 追加 
    D3D11_MAPPED_SUBRESOURCE mappedIB;

    hr = g_context->Map(g_ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIB);

    if (FAILED(hr))
    {
        MessageBoxA(nullptr, "Map(g_ib) failed", "Error", MB_OK);
        return;
    }
    memcpy(mappedIB.pData, indices_, sizeof(UINT)* indexCount);
    g_context->Unmap(g_ib, 0);
    ///////////////////// 
    // -------------------------------------------------- 
    // 回転行列  append 
    // -------------------------------------------------- 
    g_angle += 0.01f;
    // xr: x_rotation yr: y_rotation zr: z_rotation 
    //xr = 0.0f; 
    //yr = 1.0f; 
    //zr = 0.0f; 
    // ------------------------------------------------------ 
    // 時間更新  append2 
    // ------------------------------------------------------ 
    //g_time += 0.05f; 
    /// wave > wave2 change (world > rotation) 
    float g_angle_ = 0.0f;
    float xoffset_ = 0.0f;
    float yoffset_ = 0.0f;
    float zoffset_ = 0.0f;
    float xscale_ = 1.0f;
    float yscale_ = 1.0f;
    if (move == 1) {
        g_angle_ = g_angle;
        xoffset_ = xoffset;
        yoffset_ = yoffset;
        zoffset_ = zoffset;
        xscale_ = xscale;
        yscale_ = yscale;
    }
    if (move == 0) {
        zoffset_ = zoffset_back;
    }


    XMMATRIX rotation =
        /*    XMMatrixRotationY(g_angle);*/
        XMMatrixRotationRollPitchYaw(g_angle_ * xr, g_angle_ * yr, g_angle_ * zr);

    ///// wave >wave2 append 
    // 移動 
    XMMATRIX  trans =
        XMMatrixTranslation(
            xoffset_,
            yoffset_,
            zoffset_
        );
    // 拡大・縮小 
    XMMATRIX scale =
        XMMatrixScaling(
            xscale_,
            yscale_,
            1.0f
        );
    //回転してから移動,拡大・縮小 

    XMMATRIX world = scale * rotation * trans;

    //////////////////////////////////////////// 

    // HLSL用に転置 
    world = XMMatrixTranspose(world);

    //camera_zposition = -5.0f; 
    // camera 
    XMVECTOR eye =
        XMVectorSet(
            camera_xposition,
            camera_yposition,
            camera_zposition,
            0.0f);

    XMVECTOR target =
        XMVectorSet(
            0.0f,
            0.0f,
            0.0f,
            0.0f);

    XMVECTOR up =
        XMVectorSet(
            0.0f,
            1.0f,
            0.0f,
            0.0f);

    XMMATRIX view =
        XMMatrixLookAtLH(
            eye,
            target,
            up);

    // view 
    XMMATRIX proj =
        XMMatrixPerspectiveFovLH(
            XM_PIDIV4,      // 視野角(FOV) 45度 
            800.0f / 600.0f, // アスペクト比 
            1.0f,           // Near 0.f >> 1.0f; 
            100.0f);        // Far 

    // -------------------------------------------------- 
    // 定数バッファ更新   append 
    // -------------------------------------------------- 

    ConstantBuffer cb;
    cb.world = world;
    cb.view = XMMatrixTranspose(view);// HLSL用に転置 
    cb.proj = XMMatrixTranspose(proj);// HLSL用に転置 

    g_context->UpdateSubresource(
        g_constantBuffer,
        0,
        nullptr,
        &cb,
        0,
        0);
    // ----------------------------- 
    // 質感バッファ更新 append 
    // ----------------------------- 
    MaterialBuffer mb;
    /// purple // Vertex color 
    if (move == 0) {
        mb.color = XMFLOAT4(
            1.0f,
            0.5f,
            1.0f,
            1.0f);
    }
    else {// Yellow // Vertex color 
        mb.color = XMFLOAT4(
            1.0f,
            1.0f,
            0.5f,
            1.0f);
    }
    g_context->UpdateSubresource(
        g_materialBuffer,
        0,
        nullptr,
        &mb,
        0,
        0);
    


    // 設定 
    UINT stride = sizeof(Vertex);
    UINT offset = 0;

    g_context->IASetVertexBuffers(0, 1, &g_vb, &stride, &offset);


    /// IndexBuffer を IA(Input Assembler)へ 
    g_context->IASetIndexBuffer(
        g_ib,
        DXGI_FORMAT_R32_UINT,
        0);



    g_context-> IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
g_context->IASetInputLayout(g_layout);

g_context->VSSetShader(g_vs, nullptr, 0);
g_context->PSSetShader(g_ps, nullptr, 0);

// 定数バッファをVertexShaderへ append 
g_context->VSSetConstantBuffers(
    0,
    1,
    &g_constantBuffer);

//// 質感バッファをPixcelShader へ 
g_context->PSSetConstantBuffers(
    1,
    1,
    &g_materialBuffer);

/// texture setting 
g_context->PSSetShaderResources(
    0,
    1,
    &material.diffuse);

//g_context->PSSetShaderResources(1, 1, &material.diffuse); 

g_context->PSSetShaderResources(1, 1, &material.normal);
g_context->PSSetShaderResources(2, 1, &material.mask);

g_context->PSSetSamplers(
    0,
    1,
    &g_sampler);
//////////////////////////// 
// Point Light (点光源) 設定 — アンビエントを追加して全体を明るく
LightBuffer lb;
lb.lightPos = XMFLOAT4(light_x, light_y, light_z, 1.0f);
// attenuation を小さくして広く照るように調整（必要に応じて値を変えてください）
lb.atten = XMFLOAT4(1.0f, 0.045f, 0.0075f, 0.0f);
// ambient: xyz = 色、w = 強さ（0..1）
lb.ambient = XMFLOAT4(0.4f, 0.4f, 0.45f, 0.3f);

g_context->UpdateSubresource(
    g_lightBuffer,
    0,
    nullptr,
    &lb,
    0,
    0);

g_context->PSSetConstantBuffers(
    2,
    1,
    &g_lightBuffer);

// ★ 複数三角形描画  削除 
//g_context->Draw(vertexCount, 0); 

// ★★ Index指定 複数三角形描画  
g_context->DrawIndexed(indexCount, 0, 0);

/*g_swapChain->Present(1, 0);*/
}
void AfterRender()
{
    g_swapChain->Present(1, 0);
}
// 解放 
void Cleanup()
{
    if (g_constantBuffer) g_constantBuffer->Release();// append 
    if (g_materialBuffer) g_materialBuffer->Release();// append DepthBuffer2 

    if (g_lightBuffer) g_lightBuffer->Release(); // append lightBuffer(2026/7/14) 

    if (g_layout) g_layout->Release();
    if (g_ps) g_ps->Release();
    if (g_vs) g_vs->Release();
    if (g_vb) g_vb->Release();
    if (g_rtv) g_rtv->Release();
    if (g_swapChain) g_swapChain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
}
// エントリポイント 
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0, 0,
    hInst, nullptr, nullptr, nullptr, nullptr,
    L"DX11Window", nullptr };
    RegisterClassEx(&wc);
    g_hWnd = CreateWindow(L"DX11Window", L"DX11 Triangle",
        WS_OVERLAPPEDWINDOW, 100, 100, 800, 600,
        nullptr, nullptr, hInst, nullptr);
    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    if (!InitD3D()) return 0;
    int num = 5;//Regular number 

    TriangleIndex* triangle5;
    triangle5 = new TriangleIndex();// 半径 0.5 初期値 
    triangle5->MkCylinder_Vertexs2(num, vertices, 1.0f);// 底面半径0.5の正 num 角形の円柱頂点座標を計算 
    IndexCount1 = triangle5->MkCylinder_Index2(num, indices);// 底面半径0.5の正 num 角形の円柱のインデックスを作成
        delete triangle5;

        int num3 = 10;

        std::cout << "IndexCount1: " << IndexCount0;

        TriangleIndex* triangle7;
        triangle7 = new TriangleIndex();// 半径 0.5 初期値 
        triangle7->MkCylinder_Vertexs2(num3, vertices_back, 1.0f);// 底面半径0.5の正 num 角形の円柱頂点座標を計算


            IndexCount0 = triangle7->MkCylinder_Index2(num3, indices_back);// 底面半径0.5の正 num 角形の円柱のインデックスを作成

            delete triangle7;


    ///// 頂点座標計算 変更 
    TriangleIndex* triangle4;
    triangle4 = new TriangleIndex();// 半径 0.5 初期値 
    triangle4->MkRegular_polygonIndex(num, vertices, 1.0f);// 半径 0.5 の正 num 角錐の頂点座標を計算
        ///// 頂点インデックス作成   
        IndexCount1 = triangle4->MkRegular_polyHedronIndex(num, indices);// 正 num 角錐のインデックスを作成
        delete triangle4;
    int num2 = 5;
    ///// 頂点座標計算 変更 
    /// 静止オブジェクト 正 num2 角錐 
    //TriangleIndex* triangle5;

    triangle5 = new TriangleIndex();// 半径0.5 初期値 
    triangle5->MkRegular_polygonIndex(num2, vertices_back, 1.0f);// 半径0.5の正 num2 角錐の頂点座標を計算
        ///// 頂点インデックス作成   
        IndexCount0 = triangle5->MkRegular_polyHedronIndex(num2, indices_back);// 正num角錐のインデックスを作成

        delete triangle5;

    // --- 追加: Cube を作成して頂点・インデックス配列にコピー ---
    Cube cube;
    cube.Make(1.0f); // 一辺 = 1.0f
    cube.CopyVertices(vertices_cube);
    cube.CopyIndices(indices_cube);
    IndexCountCube = 36;
    // --- ここまで追加 ---

    bool flag = true;
    bool rflag = true;
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            if (rflag) {
                //Render(vertices, 6 * num,1); 
                //Render(vertices_back, 6 * num2, 0); 
                rflag = false;
            }

            while (flag) {
                Clear_view();
                material.diffuse = g_texture1;
                Render(vertices, num * 2 + 4, indices, IndexCount1, 1);
                material.diffuse = g_texture0;
                Render(vertices_back, num3 * 2 + 4, indices_back, IndexCount0, 0);// 7/22 変更

                // --- 追加: Cube の描画（既存の状態は変えずに回転を与える） ---
                // 位置を一時的に左へオフセットして描画し、元に戻す
                float old_x = xoffset;
                float old_y = yoffset;
                float old_z = zoffset;

                // 左へ移動（値を調整してください: 例 -1.5f が左寄せ）
                xoffset = -1.5f;
                yoffset = 0.0f;
                zoffset = 0.0f;

                // 回転軸を設定して描画
                float xr_old = xr;
                float yr_old = yr;
                float zr_old = zr;
                xr = 1.0f; yr = 1.0f; zr = 0.0f; // X/Y 回転で立体的に回転
                material.diffuse = g_texture1;
                Render(vertices_cube, 24, indices_cube, IndexCountCube, 1);

                // 状態を復元
                xr = xr_old; yr = yr_old; zr = zr_old;
                xoffset = old_x;
                yoffset = old_y;
                zoffset = old_z;
                // --- Cube 描画ここまで ---

                if (GetAsyncKeyState('3') & 0x8000) {
                    num = 3;
                    rflag = true;
                }
                if (GetAsyncKeyState('4') & 0x8000) {
                    num = 4;
                    rflag = true;
                }
                if (GetAsyncKeyState('5') & 0x8000) {
                    num = 5;
                    rflag = true;
                }
                if (GetAsyncKeyState('6') & 0x8000) {
                    num = 6;
                    rflag = true;
                }
                if (GetAsyncKeyState('7') & 0x8000) {
                    num = 7;
                    rflag = true;
                }
                if (GetAsyncKeyState('8') & 0x8000) {
                    num = 8;
                    rflag = true;
                }

                /////// wave >wave2  移動、スケール append  
                if (GetAsyncKeyState(VK_LEFT) & 0x8000) {/// ←key 
                    xoffset -= 0.01f;
                    rflag = true;
                }
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {/// →key 
                    xoffset += 0.01f;
                    rflag = true;
                }
                if (GetAsyncKeyState(VK_DOWN) & 0x8000) {/// ↓key 
                    yoffset -= 0.01f;
                    rflag = true;
                }
                if (GetAsyncKeyState(VK_UP) & 0x8000) {/// ↑key 
                    yoffset += 0.01f;
                    rflag = true;
                }
                if (GetAsyncKeyState('1') & 0x8000) {// スケールダウン 
                    yscale -= 0.01f;
                    xscale -= 0.01f;
                    if (xscale < 0.0001f) {
                        xscale = yscale = 0.0001f;
                    }
                    rflag = true;
                }
                if (GetAsyncKeyState('2') & 0x8000) {// スケールアップ 
                    yscale += 0.01f;
                    xscale += 0.01f;
                    if (xscale > 3.0f) {
                        xscale = yscale = 3.0f;
                    }
                    rflag = true;
                }
                // 
                if (GetAsyncKeyState('F') & 0x8000) {/// front 
                    zoffset += 0.01f;
                    rflag = true;
                }
                if (GetAsyncKeyState('B') & 0x8000) {/// back 
                    zoffset -= 0.01f;
                    rflag = true;
                }

                ////////////////////////////////////////// 
                if (GetAsyncKeyState('A') & 0x8000) {// camera_position Left 
                    camera_xposition -= 0.05f;
                    rflag = true;
                }
                if (GetAsyncKeyState('D') & 0x8000) {// camera_position Right 
                    camera_xposition += 0.05f;
                    rflag = true;
                }
                if (GetAsyncKeyState('W') & 0x8000) {// camera_position Right 
                    camera_yposition += 0.05f;
                    rflag = true;
                }
                if (GetAsyncKeyState('S') & 0x8000) {// camera_position Down 
                    camera_yposition -= 0.05f;
                    rflag = true;
                }
                if (GetAsyncKeyState('I') & 0x8000) {// camera_position Zoom in 
                    camera_zposition += 0.05f;
                    if (camera_zposition > -1.0f) { camera_zposition = -1.0f; }
                    rflag = true;
                }
                if (GetAsyncKeyState('O') & 0x8000) {// camera_position Zoom out 
                    camera_zposition -= 0.05f;

                    rflag = true;
                }
                if (GetAsyncKeyState('H') & 0x8000) {// camera_position Home 
                    camera_yposition = 0.0f;
                    camera_xposition = 0.0f;
                    camera_zposition = -5.0f;
                    rflag = true;
                }
                ///////////////////////camera position ////// 

                if (GetAsyncKeyState('K') & 0x8000) {// light_direction x Left 
                    light_x = 0.0f;
                    rflag = true;
                }
                if (GetAsyncKeyState('L') & 0x8000) {// light_direction x  Right 
                    light_x += 0.05f;
                    rflag = true;
                }
                if (GetAsyncKeyState('J') & 0x8000) {// light_direction x  Right 
                    light_x += 0.05f;
                    rflag = true;
                }
                //// light direction x 

                if (GetAsyncKeyState('X') & 0x8000) {// model x rotation 
                    xr = 1.0f; yr = 0.0f; zr = 0.0f; g_angle = 0.0;
                    rflag = true;
                }
                if (GetAsyncKeyState('Y') & 0x8000) {// model y rotation 
                    xr = 0.0f; yr = 1.0f; zr = 0.0f; g_angle = 0.0;
                    rflag = true;
                }
                if (GetAsyncKeyState('Z') & 0x8000) {// model z rotation 
                    xr = 0.0f; yr = 0.0f; zr = 1.0f; g_angle = 0.0;
                    rflag = true;
                }

                if (rflag) {
                    // 頂点座標計算 変更 
                    TriangleIndex* triangle5;
                    triangle5 = new TriangleIndex();// 半径0.5 初期値 
                    triangle5->MkCylinder_Vertexs2(num, vertices, 1.0f);
                    // 半径0.5の正 num 角錐の頂点座標を計算 
                                      // 頂点インデックス作成 7/22 変更 
                    IndexCount1 = triangle5->MkCylinder_Index2(num, indices);
                    // 正num角錐のインデックスを作成 

                    delete triangle5;


                    material.diffuse = g_texture1;
                    Render(vertices, num * 2 + 4, indices, IndexCount1, 1);
                    rflag = false;
                }
                //Sleep(100); 
                if (GetAsyncKeyState('Q') & 0x8000) {
                    Cleanup();
                    return 0;
                }
                AfterRender();
            }
            //Render(); 

        }
    }

    Cleanup();
    return 0;
}