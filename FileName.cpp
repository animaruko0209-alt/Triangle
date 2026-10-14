#include <windows.h>
#include <windowsx.h>
#include <d3d11.h> 
#include <d3dcompiler.h> 
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "d3d11.lib") 
#pragma comment(lib, "d3dcompiler.lib")// シェーダー用 
#include <cstddef>
#include <cstring>
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
    // PDF vertex layout: tangent and normal, plus UV orientation for normal mapping.
    float tx = 1.0f, ty = 0.0f, tz = 0.0f;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    float tangentSign = 1.0f;
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
    XMMATRIX normalWorld; // inverse transpose of world
    XMMATRIX proj;// view 

};
// Material parameters from DirectX11_Diffuse_Specular4 (16-byte aligned).
struct MaterialBuffer
{
    XMFLOAT4 color;
    XMFLOAT4 specularColor;
    float specularPower;
    float specularIntensity;
    float diffuseWeight;
    float surfaceType;
};
enum class SurfaceType { Textured, Mirror, Light };
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
// Point light: position/range and color/intensity, matching the PDF.
struct LightBuffer
{
    XMFLOAT3 position;
    float range;
    XMFLOAT3 color;
    float intensity;
};
struct CameraBuffer
{
    XMFLOAT3 cameraPosition;
    float padding;
};
static_assert(sizeof(MaterialBuffer) % 16 == 0, "Material buffer alignment");
static_assert(sizeof(LightBuffer) == 32, "Light buffer layout");
static_assert(sizeof(CameraBuffer) == 16, "Camera buffer layout");
ID3D11Buffer* g_lightBuffer = nullptr;
ID3D11Buffer* g_cameraBuffer = nullptr;
float light_x = -1.2f, light_y = 1.0f, light_z = -1.5f;
float pointLightRange = 10.0f;
float pointLightIntensity = 2.0f;
XMFLOAT3 pointLightColor(1.0f, 1.0f, 1.0f);

// Mouse orbit controls coexist with the existing keyboard camera controls.
float cameraYaw = 0.0f, cameraPitch = 0.0f, cameraDistance = 5.0f;
bool mouseDraggingL = false, mouseDraggingR = false;
int lastMouseX = 0, lastMouseY = 0;
enum class BoxShape { Cube, Cuboid123, Cuboid231, Cuboid312 };
BoxShape g_boxShape = BoxShape::Cube;
bool g_boxShapeDirty = false;

float ClampValue(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}
void SyncOrbitFromCamera()
{
    const float distance = std::sqrt(camera_xposition * camera_xposition +
        camera_yposition * camera_yposition + camera_zposition * camera_zposition);
    cameraDistance = ClampValue(distance, 1.0f, 20.0f);
    cameraYaw = std::atan2(camera_xposition, -camera_zposition);
    cameraPitch = distance > 0.0001f ? std::asin(ClampValue(camera_yposition / distance, -1.0f, 1.0f)) : 0.0f;
}
void UpdateCameraFromOrbit()
{
    camera_xposition = cameraDistance * std::cos(cameraPitch) * std::sin(cameraYaw);
    camera_yposition = cameraDistance * std::sin(cameraPitch);
    camera_zposition = -cameraDistance * std::cos(cameraPitch) * std::cos(cameraYaw);
}
void ResetCamera()
{
    cameraYaw = cameraPitch = 0.0f;
    cameraDistance = 5.0f;
    UpdateCameraFromOrbit();
}
void SelectBoxShape(BoxShape shape)
{
    if (g_boxShape != shape) { g_boxShape = shape; g_boxShapeDirty = true; }
}
///  IndexBuffer  //// 
ID3D11Buffer* g_ib = nullptr;
UINT indices[1200];// Index Array0
UINT indices_back[1200];// Index Array1
int IndexCount0 = 0;
int IndexCount1 = 0;
//////////////////////////////////////// 
// ウィンドウプロシージャ 
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        SyncOrbitFromCamera();
        mouseDraggingL = msg == WM_LBUTTONDOWN;
        mouseDraggingR = msg == WM_RBUTTONDOWN;
        lastMouseX = GET_X_LPARAM(lParam);
        lastMouseY = GET_Y_LPARAM(lParam);
        SetCapture(hWnd);
        return 0;
    case WM_MOUSEMOVE:
        if (mouseDraggingL || mouseDraggingR) {
            const int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
            const int dx = x - lastMouseX, dy = y - lastMouseY;
            if (mouseDraggingL) {
                cameraYaw += dx * 0.01f;
                const float limit = XMConvertToRadians(80.0f);
                cameraPitch = ClampValue(cameraPitch + dy * 0.01f, -limit, limit);
                UpdateCameraFromOrbit();
            }
            if (mouseDraggingR) {
                light_x += dx * 0.05f;
                light_z -= dy * 0.05f;
            }
            lastMouseX = x; lastMouseY = y;
        }
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_CANCELMODE:
    case WM_KILLFOCUS:
        mouseDraggingL = mouseDraggingR = false;
        if (GetCapture() == hWnd) ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        mouseDraggingL = mouseDraggingR = false;
        return 0;
    case WM_MOUSEWHEEL:
        SyncOrbitFromCamera();
        cameraDistance = ClampValue(cameraDistance - GET_WHEEL_DELTA_WPARAM(wParam) * 0.01f, 1.0f, 20.0f);
        UpdateCameraFromOrbit();
        return 0;
    case WM_KEYDOWN:
        switch (wParam) {
        case 'H': ResetCamera(); return 0;
        case 'C': SelectBoxShape(BoxShape::Cube); return 0;
        case 'V': SelectBoxShape(BoxShape::Cuboid123); return 0;
        case 'B': SelectBoxShape(BoxShape::Cuboid231); return 0;
        case 'N': SelectBoxShape(BoxShape::Cuboid312); return 0;
        }
        break;
    case WM_DESTROY:
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
        void MkCylinder_Vertexs3(int num, Vertex* v, float zp);
        int MkCylinder_Index3(int num, UINT* indices_);
        static int CylinderVertexCount3(int num) { return 4 * num + 6; }
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

// PDF version 3: caps and sides have separate vertices and UVs.
void TriangleIndex::MkCylinder_Vertexs3(int num, Vertex* v, float zp)
{
    int next = 0;
    // Cap rings include a duplicate seam vertex and a center at UV (0.5, 0.5).
    for (int cap = 0; cap < 2; ++cap) {
        const float z = cap == 0 ? 0.0f : zp;
        const float nz = cap == 0 ? -1.0f : 1.0f;
        for (int i = 0; i <= num; ++i) {
            const float angle = i == num ? 0.0f : -XM_2PI * float(i) / float(num);
            const float c = std::cos(angle), sn = std::sin(angle);
            v[next++] = { radius * c, radius * sn, z, 1,1,0,1,
                0.5f + 0.5f * c, 0.5f + 0.5f * sn,
                1,0,0, 0,0,nz, nz };
        }
        v[next++] = { 0,0,z, 1,1,1,1, 0.5f,0.5f, 1,0,0, 0,0,nz, nz };
    }
    // Side rings: U goes around the circumference, V along the height.
    for (int ring = 0; ring < 2; ++ring) {
        for (int i = 0; i <= num; ++i) {
            const float angle = i == num ? 0.0f : -XM_2PI * float(i) / float(num);
            const float c = std::cos(angle), sn = std::sin(angle);
            v[next++] = { radius * c, radius * sn, ring == 0 ? 0.0f : zp,
                1,1,0,1, float(i) / float(num), float(ring),
                sn,-c,0, c,sn,0, -1.0f };
        }
    }
}

int TriangleIndex::MkCylinder_Index3(int num, UINT* indices_)
{
    int count = 0;
    const UINT top = num + 2;
    const UINT side = 2 * num + 4;
    const UINT upperSide = side + num + 1;
    for (int i = 0; i < num; ++i) {
        indices_[count++] = i;
        indices_[count++] = i + 1;
        indices_[count++] = num + 1;
        indices_[count++] = top + i + 1;
        indices_[count++] = top + i;
        indices_[count++] = top + num + 1;
    }
    for (int i = 0; i < num; ++i) {
        indices_[count++] = side + i + 1;
        indices_[count++] = side + i;
        indices_[count++] = upperSide + i;
        indices_[count++] = side + i + 1;
        indices_[count++] = upperSide + i;
        indices_[count++] = upperSide + i + 1;
    }
    return count;
}

// --- ここから Cube クラスを追加（既存のコードはそのまま） ---
Vertex vertices_cube[24];
Vertex vertices_light[24];
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
            const XMVECTOR e1 = XMVectorSet(v[b+1].x-v[b].x, v[b+1].y-v[b].y, v[b+1].z-v[b].z, 0);
            const XMVECTOR e2 = XMVectorSet(v[b+2].x-v[b].x, v[b+2].y-v[b].y, v[b+2].z-v[b].z, 0);
            const float du1 = v[b+1].u-v[b].u, dv1 = v[b+1].v-v[b].v;
            const float du2 = v[b+2].u-v[b].u, dv2 = v[b+2].v-v[b].v;
            const float determinant = du1 * dv2 - dv1 * du2;
            const XMVECTOR n = XMVector3Normalize(XMVector3Cross(e1, e2));
            const XMVECTOR t = XMVector3Normalize((e1 * dv2 - e2 * dv1) / determinant);
            const XMVECTOR bitangent = (e2 * du1 - e1 * du2) / determinant;
            const float sign = XMVectorGetX(XMVector3Dot(XMVector3Cross(n, t), bitangent)) < 0 ? -1.0f : 1.0f;
            XMFLOAT3 normal, tangent;
            XMStoreFloat3(&normal, n); XMStoreFloat3(&tangent, t);
            for (int j = 0; j < 4; ++j) {
                v[b+j].tx = tangent.x; v[b+j].ty = tangent.y; v[b+j].tz = tangent.z;
                v[b+j].nx = normal.x; v[b+j].ny = normal.y; v[b+j].nz = normal.z;
                v[b+j].tangentSign = sign;
            }
            idx[ii + 0] = b + 0;
            idx[ii + 1] = b + 1;
            idx[ii + 2] = b + 2;
            idx[ii + 3] = b + 0;
            idx[ii + 4] = b + 2;
            idx[ii + 5] = b + 3;
        }
    }

    // Keep six separate faces and their UV/tangent frames for all box shapes.
    void MakeCuboid(float width, float height, float depth)
    {
        Make(1.0f);
        for (Vertex& vertex : v) {
            vertex.x *= width;
            vertex.y *= height;
            vertex.z *= depth;
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
void RebuildBoxShape()
{
    if (!g_boxShapeDirty) return;
    Cube box;
    switch (g_boxShape) {
    case BoxShape::Cuboid123: box.MakeCuboid(1.0f, 2.0f, 3.0f); break;
    case BoxShape::Cuboid231: box.MakeCuboid(2.0f, 3.0f, 1.0f); break;
    case BoxShape::Cuboid312: box.MakeCuboid(3.0f, 1.0f, 2.0f); break;
    default: box.Make(1.0f); break;
    }
    box.CopyVertices(vertices_cube);
    box.CopyIndices(indices_cube);
    g_boxShapeDirty = false;
}

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
    // 頂点シェーダー  (Raw data Type) 
    const char* vsCode = R"( 
cbuffer ConstantBuffer : register(b0) 
    { 
      matrix world; 
      matrix view;
      matrix normalWorld;
      matrix proj; 
    }; 
    struct VS_IN 
    { 
        float3 pos : POSITION; 
        float4 col : COLOR; 
        float2 uv : TEXCOORD;
        float3 tangent : TANGENT;
        float3 normal : NORMAL;
        float tangentSign : TEXCOORD4;
    };
    struct PS_IN 
    { 
        float4 pos : SV_POSITION; 
        float4 col : COLOR; 
        float2 uv : TEXCOORD; 
        float3 worldPos : TEXCOORD1;
        float3 worldNormal : TEXCOORD2;
        float4 worldTangent : TEXCOORD3;
    }; 
    PS_IN main(VS_IN input) 
    { 
        PS_IN output; 
 
        float4 worldPos4 = mul(float4(input.pos, 1.0f), world); // ワールド空間位置
        float4 p = mul(worldPos4, view);
        p = mul(p, proj);
 
        output.pos = p; 
        output.worldPos = worldPos4.xyz;
        output.worldNormal = mul(float4(input.normal, 0.0f), normalWorld).xyz;
        output.worldTangent = float4(mul(float4(input.tangent, 0.0f), world).xyz, input.tangentSign);
 
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
        float4 specularColor;
        float specularPower;
        float specularIntensity;
        float diffuseWeight;
        float surfaceType;
    };
    cbuffer LightBuffer : register(b2)
    {
        float3 lightPosition;
        float range;
        float3 lightColor;
        float intensity;
    };
    cbuffer CameraBuffer : register(b3)
    {
        float3 cameraPosition;
        float cameraPadding;
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
        float3 worldNormal : TEXCOORD2;
        float4 worldTangent : TEXCOORD3;
    }; 

    float4 main(PS_IN input) : SV_TARGET 
    { 
    // The small emissive marker shows the position of the point light.
    if (surfaceType > 1.5f)
        return float4(saturate(lightColor * intensity), 1.0f);
    float4 diffuseColor = diffuse.Sample(sam,input.uv);
 
    // Version 3 supplies per-vertex normals and tangents. Legacy meshes
    // without normals retain the geometric-normal fallback.
    float3 n = input.worldNormal;
    bool hasVertexNormal = dot(n, n) > 0.000001f;
    n = hasVertexNormal ? normalize(n) : normalize(cross(ddx(input.worldPos), ddy(input.worldPos)));
    if (hasVertexNormal) {
        float3 tangent = normalize(input.worldTangent.xyz - n * dot(n, input.worldTangent.xyz));
        float3 bitangent = cross(n, tangent) * input.worldTangent.w;
        float3 mappedNormal = normal.Sample(sam, input.uv).xyz * 2.0f - 1.0f;
        n = normalize(tangent * mappedNormal.x + bitangent * mappedNormal.y + n * mappedNormal.z);
    }

    // Phong diffuse + specular, using the real camera position.
    float3 toLight = lightPosition - input.worldPos;
    float distanceToLight = length(toLight);
    float3 L = toLight / max(distanceToLight, 0.0001f);
    float3 toCamera = cameraPosition - input.worldPos;
    float3 V = toCamera / max(length(toCamera), 0.0001f);
    float nDotL = saturate(dot(n, L));
    float attenuation = saturate(1.0f - distanceToLight / max(range, 0.0001f));
    float3 reflectedLight = reflect(-L, n);
    float specular = pow(saturate(dot(reflectedLight, V)), max(specularPower, 1.0f));
    specular *= nDotL > 0.0f ? 1.0f : 0.0f;

    // The cube uses its texture and material tint without the old face colors.
    float3 vertexTint = surfaceType > 0.5f ? float3(1,1,1) : input.col.rgb;
    float3 baseColor = diffuseColor.rgb * color.rgb * vertexTint;
    float3 diffuseTerm = baseColor * nDotL * diffuseWeight * lightColor;
    // Reflection color is independent of the diffuse image (white plastic / gold metal).
    float3 specularTerm = specularColor.rgb * specular * specularIntensity * lightColor;
    float3 lighting = (diffuseTerm + specularTerm) * attenuation * intensity;
    // Retain a faint ambient fill so texture remains readable on unlit faces.
    lighting += baseColor * 0.035f;
    float maskValue = mask.Sample(sam, input.uv).r;
    return float4(saturate(lighting * maskValue), diffuseColor.a * color.a * input.col.a);
    } 
    )";


    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;

    ID3DBlob* vsError = nullptr;
    hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr,
        "main", "vs_4_0", 0, 0, &vsBlob, &vsError);
    if (FAILED(hr)) {
        if (vsError) {
            MessageBoxA(nullptr, (char*)vsError->GetBufferPointer(), "Vertex Shader Error", MB_OK);
            vsError->Release();
        }
        return false;
    }
    if (vsError) vsError->Release();

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

        vsBlob->Release();
        return false;
    }
    if (errorBlob) errorBlob->Release();
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
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(Vertex,x),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(Vertex,r),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(Vertex,u),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(Vertex,tx),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(Vertex,nx),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",4,DXGI_FORMAT_R32_FLOAT,0,offsetof(Vertex,tangentSign),D3D11_INPUT_PER_VERTEX_DATA,0},
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
    hr = CreateWICTextureFromFile(
        g_device,
        g_context,
        L"check.png",
        nullptr,
        &g_texture0);
    if (FAILED(hr)) {
        MessageBoxW(nullptr, L"Texture load failed: check.png", L"Error", MB_OK);
        return false;
    }

    hr = CreateWICTextureFromFile(
        g_device,
        g_context,
        L"check2.png",
        nullptr,
        &g_texture1);
    if (FAILED(hr)) {
        MessageBoxW(nullptr, L"Texture load failed: check2.png", L"Error", MB_OK);
        return false;
    }

    hr = CreateWICTextureFromFile(
        g_device,
        g_context,
        L"mask5b_normal.png",
        nullptr,
        &material.normal);
    if (FAILED(hr)) {
        MessageBoxW(nullptr, L"Texture load failed: mask5b_normal.png", L"Error", MB_OK);
        return false;
    }

    hr = CreateWICTextureFromFile(
        g_device,
        g_context,
        L"mask5b.png",
        nullptr,
        &material.mask);
    if (FAILED(hr)) {
        MessageBoxW(nullptr, L"Texture load failed: mask5b.png", L"Error", MB_OK);
        return false;
    }

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

    hr = g_device->CreateBuffer(&ld, nullptr, &g_lightBuffer);
    if (FAILED(hr)) return false;
    ld.ByteWidth = sizeof(CameraBuffer);
    hr = g_device->CreateBuffer(&ld, nullptr, &g_cameraBuffer);
    if (FAILED(hr)) return false;

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

void Render(Vertex* vertices_, int vertexCount, UINT* indices_, int indexCount, int move, SurfaceType surfaceType = SurfaceType::Textured)// Render change 7 / 22
{

    if (g_vb) { g_vb->Release(); g_vb = nullptr; }
    if (g_ib) { g_ib->Release(); g_ib = nullptr; }
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
    // Rotation advances once per frame.
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
    if (surfaceType == SurfaceType::Light) {
        world = XMMatrixScaling(0.09f, 0.09f, 0.09f) *
            XMMatrixTranslation(light_x, light_y, light_z);
    }

    //////////////////////////////////////////// 

    const XMMATRIX normalWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
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
    cb.normalWorld = XMMatrixTranspose(normalWorld);
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
    MaterialBuffer mb = {};
    mb.diffuseWeight = 0.7f;
    if (move == 0) {
        mb.color = XMFLOAT4(1.0f, 0.9f, 1.0f, 1.0f);
        mb.specularColor = XMFLOAT4(1,1,1,1);
        mb.specularPower = 12.0f;
    }
    else {
        // Gold material: both diffuse and reflected light have a gold tint.
        mb.color = XMFLOAT4(1.0f, 0.7f, 0.2f, 1.0f);
        mb.specularColor = XMFLOAT4(1.0f, 0.7f, 0.2f, 1.0f);
        mb.specularPower = 64.0f;
    }
    mb.specularIntensity = 1.0f;
    if (surfaceType == SurfaceType::Mirror) {
        // Yellow glossy surface with white reflections, as in the PDF.
        mb.color = XMFLOAT4(1.0f, 1.0f, 0.12f, 1.0f);
        mb.specularColor = XMFLOAT4(1,1,1,1);
        mb.specularPower = 32.0f;
        mb.diffuseWeight = 0.65f;
        mb.surfaceType = 1.0f;
    }
    else if (surfaceType == SurfaceType::Light) {
        mb.surfaceType = 2.0f;
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
// Point-light position/range/color/intensity, and a separate camera buffer.
LightBuffer lb = {};
lb.position = XMFLOAT3(light_x, light_y, light_z);
lb.range = pointLightRange;
lb.color = pointLightColor;
lb.intensity = pointLightIntensity;
g_context->UpdateSubresource(g_lightBuffer, 0, nullptr, &lb, 0, 0);
g_context->PSSetConstantBuffers(2, 1, &g_lightBuffer);
CameraBuffer cameraBuffer = {};
cameraBuffer.cameraPosition = XMFLOAT3(camera_xposition, camera_yposition, camera_zposition);
g_context->UpdateSubresource(g_cameraBuffer, 0, nullptr, &cameraBuffer, 0, 0);
g_context->PSSetConstantBuffers(3, 1, &g_cameraBuffer);

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
    if (g_context) g_context->ClearState();
    if (g_ib) g_ib->Release();
    if (g_dsv) g_dsv->Release();
    if (g_sampler) g_sampler->Release();
    if (g_texture0) g_texture0->Release();
    if (g_texture1) g_texture1->Release();
    if (material.normal) material.normal->Release();
    if (material.mask) material.mask->Release();
    if (g_constantBuffer) g_constantBuffer->Release();// append 
    if (g_materialBuffer) g_materialBuffer->Release();// append DepthBuffer2 

    if (g_cameraBuffer) g_cameraBuffer->Release();
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
    HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) return 1;
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0, 0,
    hInst, nullptr, nullptr, nullptr, nullptr,
    L"DX11Window", nullptr };
    RegisterClassEx(&wc);
    g_hWnd = CreateWindow(L"DX11Window", L"DX11 Triangle",
        WS_OVERLAPPEDWINDOW, 100, 100, 800, 600,
        nullptr, nullptr, hInst, nullptr);
    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    if (!InitD3D()) { Cleanup(); CoUninitialize(); return 1; }
    int num = 5;//Regular number

    TriangleIndex* triangle5;
    triangle5 = new TriangleIndex();// 半径 0.5 初期値
    triangle5->MkCylinder_Vertexs3(num, vertices, 1.0f);// 底面半径0.5の正 num 角形の円柱頂点座標を計算
    IndexCount1 = triangle5->MkCylinder_Index3(num, indices);// 底面半径0.5の正 num 角形の円柱のインデックスを作成
        delete triangle5;

        int num3 = 10;

        std::cout << "IndexCount1: " << IndexCount0;

        TriangleIndex* triangle7;
        triangle7 = new TriangleIndex();// 半径 0.5 初期値
        triangle7->MkCylinder_Vertexs3(num3, vertices_back, 1.0f);// 底面半径0.5の正 num 角形の円柱頂点座標を計算


            IndexCount0 = triangle7->MkCylinder_Index3(num3, indices_back);// 底面半径0.5の正 num 角形の円柱のインデックスを作成

            delete triangle7;


    // --- 追加: Cube を作成して頂点・インデックス配列にコピー ---
    Cube cube;
    cube.Make(1.0f); // 一辺 = 1.0f
    cube.CopyVertices(vertices_cube);
    cube.CopyVertices(vertices_light);
    cube.CopyIndices(indices_cube);
    IndexCountCube = 36;
    // --- ここまで追加 ---

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }
        const int previousNum = num;
        if (GetActiveWindow() == g_hWnd) {
            if (GetAsyncKeyState('3') & 0x8000) {
                num = 3;
            }
            if (GetAsyncKeyState('4') & 0x8000) {
                num = 4;
            }
            if (GetAsyncKeyState('5') & 0x8000) {
                num = 5;
            }
            if (GetAsyncKeyState('6') & 0x8000) {
                num = 6;
            }
            if (GetAsyncKeyState('7') & 0x8000) {
                num = 7;
            }
            if (GetAsyncKeyState('8') & 0x8000) {
                num = 8;
            }

            /////// wave >wave2  移動、スケール append
            if (GetAsyncKeyState(VK_LEFT) & 0x8000) {/// ←key
                xoffset -= 0.01f;
            }
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {/// →key
                xoffset += 0.01f;
            }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) {/// ↓key
                yoffset -= 0.01f;
            }
            if (GetAsyncKeyState(VK_UP) & 0x8000) {/// ↑key
                yoffset += 0.01f;
            }
            if (GetAsyncKeyState('1') & 0x8000) {// スケールダウン
                yscale -= 0.01f;
                xscale -= 0.01f;
                if (xscale < 0.0001f) {
                    xscale = yscale = 0.0001f;
                }
            }
            if (GetAsyncKeyState('2') & 0x8000) {// スケールアップ
                yscale += 0.01f;
                xscale += 0.01f;
                if (xscale > 3.0f) {
                    xscale = yscale = 3.0f;
                }
            }
            //
            if (GetAsyncKeyState('F') & 0x8000) {/// front
                zoffset += 0.01f;
            }
            if (GetAsyncKeyState('G') & 0x8000) {/// back
                zoffset -= 0.01f;
            }

            //////////////////////////////////////////
            if (GetAsyncKeyState('A') & 0x8000) {// camera_position Left
                camera_xposition -= 0.05f;
            }
            if (GetAsyncKeyState('D') & 0x8000) {// camera_position Right
                camera_xposition += 0.05f;
            }
            if (GetAsyncKeyState('W') & 0x8000) {// camera_position Right
                camera_yposition += 0.05f;
            }
            if (GetAsyncKeyState('S') & 0x8000) {// camera_position Down
                camera_yposition -= 0.05f;
            }
            if (GetAsyncKeyState('I') & 0x8000) {
                SyncOrbitFromCamera();
                cameraDistance = ClampValue(cameraDistance - 0.05f, 1.0f, 20.0f);
                UpdateCameraFromOrbit();
            }
            if (GetAsyncKeyState('O') & 0x8000) {
                SyncOrbitFromCamera();
                cameraDistance = ClampValue(cameraDistance + 0.05f, 1.0f, 20.0f);
                UpdateCameraFromOrbit();
            }
            if (GetAsyncKeyState('H') & 0x8000) ResetCamera();
            ///////////////////////camera position //////

            if (GetAsyncKeyState('K') & 0x8000) {// reset point light
                light_x = -1.2f; light_y = 1.0f; light_z = -1.5f;
            }
            if (GetAsyncKeyState('L') & 0x8000) {// light_direction x  Right
                light_x += 0.05f;
            }
            if (GetAsyncKeyState('J') & 0x8000) {// light_direction x  Right
                light_x -= 0.05f;
            }
            //// light direction x

            if (GetAsyncKeyState('X') & 0x8000) {// model x rotation
                xr = 1.0f; yr = 0.0f; zr = 0.0f; g_angle = 0.0;
            }
            if (GetAsyncKeyState('Y') & 0x8000) {// model y rotation
                xr = 0.0f; yr = 1.0f; zr = 0.0f; g_angle = 0.0;
            }
            if (GetAsyncKeyState('Z') & 0x8000) {// model z rotation
                xr = 0.0f; yr = 0.0f; zr = 1.0f; g_angle = 0.0;
            }

            if (GetAsyncKeyState('Q') & 0x8000) PostMessage(g_hWnd, WM_CLOSE, 0, 0);
        }
        if (num != previousNum) {
            TriangleIndex cylinder;
            cylinder.MkCylinder_Vertexs3(num, vertices, 1.0f);
            IndexCount1 = cylinder.MkCylinder_Index3(num, indices);
        }
        RebuildBoxShape();
        g_angle += 0.01f;
        Clear_view();
        material.diffuse = g_texture1;
        Render(vertices, TriangleIndex::CylinderVertexCount3(num), indices, IndexCount1, 1);
        material.diffuse = g_texture0;
        Render(vertices_back, TriangleIndex::CylinderVertexCount3(num3), indices_back, IndexCount0, 0);// 7/22 変更

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
        Render(vertices_cube, 24, indices_cube, IndexCountCube, 1, SurfaceType::Mirror);

        // 状態を復元
        xr = xr_old; yr = yr_old; zr = zr_old;
        xoffset = old_x;
        yoffset = old_y;
        zoffset = old_z;
        // --- Cube 描画ここまで ---
        // Draw the source marker without changing the cube's transform or rotation.
        Render(vertices_light, 24, indices_cube, IndexCountCube, 0, SurfaceType::Light);
        AfterRender();
    }
    Cleanup();
    CoUninitialize();
    return 0;
}
