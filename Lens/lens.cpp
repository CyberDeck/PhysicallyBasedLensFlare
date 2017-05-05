#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>

#include <vector>
#include <string>
#include <sstream>

#include "resource.h"
#include "ray_trace.h"

//#define DRAW2D
#define DRAW3D

using namespace DirectX;

//--------------------------------------------------------------------------------------
// DEFAULT
//--------------------------------------------------------------------------------------

const std::string vertex_shader_source = R"(
cbuffer Uniforms : register(b0) {
	float4 color;
	float4 placement;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
float4 VS( float4 Pos : POSITION ) : SV_POSITION {
	float4 p = Pos;
	if (placement.y == 1.f) {
		p.xy *= placement.zw;
		p.x += placement.z;
		p.x += placement.x;
	}
	return p;
}
)";

const std::string pixel_shader_source = R"(
cbuffer Uniforms : register(b0) {
	float4 color;
	float4 placement;
};

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( float4 Pos : SV_POSITION ) : SV_Target {
    return color;
}
)";

//--------------------------------------------------------------------------------------
// FLARE
//--------------------------------------------------------------------------------------

const std::string flare_vertex_shader_source = R"(
)";

const std::string flare_pixel_shader_source = R"(
)";

struct PatentFormat {
	float r;
	float d;
	float n;
	bool flat;
	float w;
	float h;
};

struct SimpleVertex {
	XMFLOAT3 pos;
};

struct InstanceUniforms {
	XMFLOAT4 color;
	XMFLOAT4 placement;
};

struct GlobalUniforms {
	float time;
	float g1;
	float g2;
	float padding;
	XMFLOAT4 direction;

};

namespace LensShapes {
	struct Rectangle {
		ID3D11Buffer* vertices;
		ID3D11Buffer* lines;
	};

	struct Circle {
		float x, y, r;
		ID3D11Buffer* triangles;
		ID3D11Buffer* lines;
	};

	struct Patch {
		int subdiv;
		ID3D11Buffer* vertices;
		ID3D11Buffer* indices;
		ID3D11UnorderedAccessView* unordered_access_view;
	};
}

LensShapes::Circle unit_circle;
LensShapes::Rectangle unit_square;
LensShapes::Patch unit_patch;

std::vector<LensInterface> nikon_28_75mm_lens_interface;

const float d6  = 53.142f;
const float d10 =  7.063f;
const float d14 =  1.532f;
const float dAp =  2.800f;
const float d20 = 16.889f;
const float Bf  = 39.683f;

std::vector<PatentFormat> nikon_28_75mm_lens_components = {
	{    72.747f,  2.300f, 1.60300f, false, 0.2f, 29.0f },
	{    37.000f, 13.000f, 1.00000f, false, 0.2f, 29.0f },

	{  -172.809f,  2.100f, 1.58913f, false, 2.7f, 26.2f },
	{    39.894f,  1.000f, 1.00000f, false, 2.7f, 26.2f },

	{    49.820f,  4.400f, 1.86074f, false, 0.5f, 20.0f },
	{    74.750f,      d6, 1.00000f, false, 0.5f, 20.0f },

	{    63.402f,  1.600f, 1.86074f, false, 0.5f, 16.1f },
	{    37.530f,  8.600f, 1.51680f, false, 0.5f, 16.1f },

	{   -75.887f,  1.600f, 1.80458f, false, 0.5f, 16.0f },
	{   -97.792f,     d10, 1.00000f, false, 0.5f, 16.5f },

	{    96.034f,  3.600f, 1.62041f, false, 0.5f, 18.0f },
	{   261.743f,  0.100f, 1.00000f, false, 0.5f, 18.0f },

	{    54.262f,  6.000f, 1.69680f, false, 0.5f, 18.0f },
	{ -5995.277f,     d14, 1.00000f, false, 0.5f, 18.0f },

	{       0.0f,     dAp, 1.00000f, true,  18.f, 10.0f },

	{   -74.414f,  2.200f, 1.90265f, false, 0.5f, 13.0f },

	{   -62.929f,  1.450f, 1.51680f, false, 0.1f, 13.0f },
	{   121.380f,  2.500f, 1.00000f, false, 4.0f, 13.1f },

	{   -85.723f,  1.400f, 1.49782f, false, 4.0f, 13.0f },

	{    31.093f,  2.600f, 1.80458f, false, 4.0f, 13.1f },
	{    84.758f,     d20, 1.00000f, false, 0.5f, 13.0f },

	{   459.690f,  1.400f, 1.86074f, false, 1.0f, 15.0f },

	{    40.240f,  7.300f, 1.49782f, false, 1.0f, 15.0f },
	{   -49.771f,  0.100f, 1.00000f, false, 1.0f, 15.2f },

	{    62.369f,  7.000f, 1.67025f, false, 1.0f, 16.0f },
	{   -76.454f,  5.200f, 1.00000f, false, 1.0f, 16.0f },

	{   -32.524f,  2.000f, 1.80454f, false, 0.5f, 17.0f },
	{   -50.194f,      Bf, 1.00000f, false, 0.5f, 17.0f },

	{        0.f,     5.f, 1.00000f,  true, 17.f,  0.0f },
};

int num_of_rays = 151;
int num_of_lens_components = (int)nikon_28_75mm_lens_components.size() + 1;

int ghost_bounce_1 = 16;
int ghost_bounce_2 = 1;

int num_of_intersections_1 = num_of_lens_components;
int num_of_intersections_2 = num_of_lens_components;
int num_of_intersections_3 = num_of_lens_components;

int num_points_per_cirlces = 200;
int num_vertices_per_cirlces = num_points_per_cirlces * 3;
float backbuffer_width = 1800;
float backbuffer_height = 900;
float ratio = backbuffer_height / backbuffer_width;
float min_ior = 1000.f;
float max_ior = -1000.f;
float global_scale = 0.009;
float total_lens_distance = 0.f;

float time         = (float)ghost_bounce_1;
float speed        = 11.5f;
float focus_speed  = 0.0f;
float swing_angle  = 0.0f;
float swing_speed  = 0.0f;
float spread_speed = 2.0f;
float rays_spread1 = 1.75f;
float rays_spread2 = 1.75f;

int patch_tessellation = 50;

#ifdef SAVE_BACK_BUFFER_TO_DISK
#include <DirectXTex.h>
void SaveBackBuffer() {
	ScratchImage scratch_image;
	ID3D11Resource* resource = nullptr;
	g_pRenderTargetView->GetResource(&resource);
	CaptureTexture(g_pd3dDevice, g_pImmediateContext, resource, scratch_image);

	static int frame_number = 0;
	frame_number++;
	std::wstringstream finle_name;
	finle_name << "Lens";
	if (frame_number < 10)
		finle_name << "00";
	else if (frame_number < 100)
		finle_name << "0";

	finle_name << frame_number;
	finle_name << ".tga";

	const Image* image = scratch_image.GetImage(0, 0, 0);
	SaveToTGAFile(*image, finle_name.str().c_str());
}
#endif

ULONGLONG TimeToTick(int t) {
	return (ULONGLONG)(t * 0.5f * 1000.0f * speed);
}

ULONGLONG timer_start = (ULONGLONG)GetTickCount64() - TimeToTick(ghost_bounce_1);

INT sampleMask = 0x0F;
UINT offset = 0;
UINT stride = sizeof(SimpleVertex);
float blendFactor[4] = { 1.f, 1.f, 1.f, 1.f };

XMFLOAT4 sRGB(XMFLOAT4 c) {
	XMFLOAT4 rgb = c;
	rgb.x /= 255.f;
	rgb.y /= 255.f;
	rgb.z /= 255.f;
	return rgb;
};

#define COLOR_THEME1
#ifdef COLOR_THEME1
	float opaque_alpha           = 0.65;
	XMFLOAT4 fill_color1         = sRGB({  64.f, 215.f, 242.f, 0.2f });
	XMFLOAT4 fill_color2         = sRGB({ 179.f, 178.f, 210.f, 0.2f });
	XMFLOAT4 flat_fill_color     = sRGB({ 190.f, 190.f, 190.f, 1.0f });
	XMFLOAT4 stroke_color        = sRGB({ 115.f, 115.f, 115.f, 1.0f });
	XMFLOAT4 stroke_color1       = sRGB({ 115.f, 115.f, 115.f, 1.0f });
	XMFLOAT4 stroke_color2       = sRGB({ 165.f, 165.f, 165.f, 1.0f });
	XMFLOAT4 background_color1   = sRGB({ 240.f, 240.f, 240.f, 1.0f });
	XMFLOAT4 background_color2   = sRGB({  40.f,  40.f,  40.f, 1.0f });

	XMFLOAT4 intersection_color1 = sRGB({   0.f,   0.f,   0.f, 0.1f });
	XMFLOAT4 intersection_color2 = sRGB({  64.f, 215.f, 242.f, 0.5f });
	XMFLOAT4 intersection_color3 = sRGB({ 179.f, 178.f, 210.f, 0.5f });
#endif

inline XMFLOAT3 point_to_d3d(vec3& point) {
	float x = point.x;
	float y = point.y / ratio* global_scale;
	float z = point.z * global_scale;
	return XMFLOAT3(-(z - 1.f), y, x);
}

inline float sign(float v) {
	return v < 0.f ? -1.f : 1.f;
}

inline float lerp(float a, float b, float l) {
	return a * (1.f - l) + b * l;
}
XMFLOAT4 lerp(XMFLOAT4& a, XMFLOAT4& b, float l) {
	float x = lerp(a.x, b.x, l);
	float y = lerp(a.y, b.y, l);
	float z = lerp(a.z, b.z, l);
	float w = lerp(a.w, b.w, l);
	return{ x, y, z, w };
}

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                g_hInst = nullptr;
HWND                     g_hWnd = nullptr;
D3D_DRIVER_TYPE          g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL        g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*            g_pd3dDevice = nullptr;
ID3D11Device1*           g_pd3dDevice1 = nullptr;
ID3D11DeviceContext*     g_pImmediateContext = nullptr;
ID3D11DeviceContext1*    g_pImmediateContext1 = nullptr;
IDXGISwapChain*          g_pSwapChain = nullptr;
IDXGISwapChain1*         g_pSwapChain1 = nullptr;
ID3D11RenderTargetView*  g_pRenderTargetView = nullptr;
ID3D11VertexShader*      g_pVertexShader = nullptr;
ID3D11PixelShader*       g_pPixelShader = nullptr;

ID3D11VertexShader*      g_pFlareVertexShader = nullptr;
ID3D11PixelShader*       g_pFlarePixelShader = nullptr;
ID3D11GeometryShader*    g_pGeometryShader = nullptr;

ID3D11InputLayout*       g_pVertexLayout = nullptr;
ID3D11Buffer*            g_pVertexBuffer = nullptr;
ID3D11Buffer*            g_GlobalUniforms = nullptr;
ID3D11Buffer*            g_InstanceUniforms = nullptr;
ID3D11Buffer*            g_IntersectionPoints1 = nullptr;
ID3D11Buffer*            g_IntersectionPoints2 = nullptr;
ID3D11Buffer*            g_IntersectionPoints3 = nullptr;
ID3D11Texture2D*         g_pDepthStencil = nullptr;
ID3D11DepthStencilView*  g_pDepthStencilView = nullptr;
ID3D11BlendState*        g_pBlendStateBlend = NULL;
ID3D11BlendState*        g_pBlendStateMask = NULL;
ID3D11DepthStencilState* g_pDepthStencilState = NULL;
ID3D11DepthStencilState* g_pDepthStencilStateFill = NULL;
ID3D11DepthStencilState* g_pDepthStencilStateGreaterOrEqualIncr = NULL;
ID3D11DepthStencilState* g_pDepthStencilStateGreaterOrEqualDecr = NULL;
ID3D11DepthStencilState* g_pDepthStencilStateGreaterOrEqualRead = NULL;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
HRESULT InitDevice();
void CleanupDevice();
void Render();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( lpCmdLine );

	if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
		return 0;

	if( FAILED( InitDevice() ) ) {
		CleanupDevice();
		return 0;
	}

	// Main message loop
	MSG msg = {0};
	while( WM_QUIT != msg.message ) {
		if( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		} else {
			Render();
		}
	}

	CleanupDevice();

	return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow ) {
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof( WNDCLASSEX );
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon( hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
	wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
	wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"LensClass";
	wcex.hIconSm = LoadIcon( wcex.hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
	if( !RegisterClassEx( &wcex ) )
		return E_FAIL;

	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, (LONG)backbuffer_width, (LONG)backbuffer_height };
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	g_hWnd = CreateWindow( L"LensClass", L"Lens Interface",
							WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
							CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
							nullptr );
	if( !g_hWnd )
		return E_FAIL;

	ShowWindow( g_hWnd, nCmdShow );

	return S_OK;
}

HRESULT CompileShaderFromSource(std::string shaderSource, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut) {
	ID3DBlob* temp = nullptr;
	HRESULT hr = D3DCompile(shaderSource.c_str(), shaderSource.length(), nullptr, nullptr, nullptr, szEntryPoint, szShaderModel, D3DCOMPILE_ENABLE_STRICTNESS, 0, ppBlobOut, &temp);
	char* msg = temp ? (char*)temp : nullptr; msg;
	return hr;
}

HRESULT CompileShaderFromFile(LPCWSTR shaderFile, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut) {
	ID3DBlob* temp = nullptr;
	HRESULT hr = D3DCompileFromFile(shaderFile, nullptr, nullptr, szEntryPoint, szShaderModel, D3DCOMPILE_ENABLE_STRICTNESS, 0, ppBlobOut, &temp);	
	char* msg = temp ? (char*)temp->GetBufferPointer() : nullptr; msg;
	return hr;
}

LensShapes::Rectangle CreateUnitRectangle() {

	float l = -1.f;
	float r =  1.f;
	float b = -1.f / ratio;
	float t =  1.f / ratio;

	SimpleVertex vertices[] = {
		XMFLOAT3(l, b, 0.f),
		XMFLOAT3(l, t, 0.f),
		XMFLOAT3(r, t, 0.f),
		XMFLOAT3(l, b, 0.f),
		XMFLOAT3(r, b, 0.f),
		XMFLOAT3(r, t, 0.f),
	};

	SimpleVertex lines[] = {
		XMFLOAT3(l, b, 0.f),
		XMFLOAT3(l, t, 0.f),
		XMFLOAT3(r, t, 0.f),
		XMFLOAT3(r, b, 0.f),
		XMFLOAT3(l, b, 0.f),
	};

	LensShapes::Rectangle rectangle;

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 6;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = vertices;

	g_pd3dDevice->CreateBuffer(&bd, &InitData, &rectangle.vertices);

	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 5;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData2;
	ZeroMemory(&InitData2, sizeof(InitData2));
	InitData2.pSysMem = lines;

	g_pd3dDevice->CreateBuffer(&bd, &InitData2, &rectangle.lines);

	return rectangle;
}

LensShapes::Circle CreateUnitCircle() {

	std::vector<SimpleVertex> triangle_vertices;
	std::vector<SimpleVertex> line_vertices;

	for (int i = 0; i < num_points_per_cirlces - 1; i++) {

		float t1 = (float)i / (float)(num_points_per_cirlces - 1);
		float a1 = t1 * 2.f * PI;
		float x1 = sin(a1);
		float y1 = cos(a1) / ratio;

		float t2 = (float)(i + 1) / (float)(num_points_per_cirlces - 1);
		float a2 = t2 * 2.f * PI;
		float x2 = sin(a2);
		float y2 = cos(a2) / ratio;

		SimpleVertex to_add1;
		SimpleVertex to_add2;
		SimpleVertex to_add3;
		to_add1.pos = XMFLOAT3(x1, y1, 0.f);
		to_add2.pos = XMFLOAT3(x2, y2, 0.f);
		to_add3.pos = XMFLOAT3(0.f, 0.f, 0.f);

		triangle_vertices.push_back(to_add1);
		triangle_vertices.push_back(to_add2);
		triangle_vertices.push_back(to_add3);

		line_vertices.push_back(to_add1);
	}

	SimpleVertex to_add;
	to_add.pos = XMFLOAT3(0.f, 1.f / ratio, 0.f);
	line_vertices.push_back(to_add);

	D3D11_BUFFER_DESC bd1;
	ZeroMemory(&bd1, sizeof(bd1));
	bd1.Usage = D3D11_USAGE_DEFAULT;
	bd1.ByteWidth = sizeof(SimpleVertex) * num_vertices_per_cirlces;
	bd1.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd1.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData1;
	ZeroMemory(&InitData1, sizeof(InitData1));
	InitData1.pSysMem = (float*)&triangle_vertices[0];

	D3D11_BUFFER_DESC bd2;
	ZeroMemory(&bd2, sizeof(bd2));
	bd2.Usage = D3D11_USAGE_DEFAULT;
	bd2.ByteWidth = sizeof(SimpleVertex) * num_points_per_cirlces;
	bd2.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd2.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData2;
	ZeroMemory(&InitData2, sizeof(InitData2));
	InitData2.pSysMem = (float*)&line_vertices[0];

	LensShapes::Circle circle;
	circle.x = 0.f;
	circle.y = 0.f;
	circle.r = 1.f;

	g_pd3dDevice->CreateBuffer(&bd1, &InitData1, &circle.triangles);
	g_pd3dDevice->CreateBuffer(&bd2, &InitData2, &circle.lines);

	return circle;
}

LensShapes::Patch CreateUnitPatch(int subdiv) {

	float l = -1.0f;
	float r =  1.0f;
	float b = -1.0f;
	float t =  1.0f;

	std::vector<SimpleVertex> vertices;
	for (int y = 0; y < subdiv; ++y) {
		float ny = (float)y / (float)(subdiv - 1);
		for (int x = 0; x < subdiv; ++x) {
			float nx = (float)x / (float)(subdiv - 1);
			float x_pos = lerp(l, r, nx);
			float y_pos = lerp(t, b, ny);
			vertices.push_back({ XMFLOAT3(x_pos, y_pos, 0.f) });
		}
	}

	std::vector<int> indices;
	int current_corner = 0;
	for (int y = 0; y < (subdiv - 1); ++y) {
		for (int x = 0; x < (subdiv - 1); ++x) {
			int i1 = current_corner;
			int i2 = current_corner + 1;
			int i3 = current_corner + (subdiv);
			int i4 = current_corner + (subdiv + 1);

			indices.push_back(i1);
			indices.push_back(i2);
			indices.push_back(i3);

			indices.push_back(i2);
			indices.push_back(i4);
			indices.push_back(i3);

			current_corner++;
		}
		current_corner++;
	}

	D3D11_BUFFER_DESC bd1;
	ZeroMemory(&bd1, sizeof(bd1));
	bd1.Usage = D3D11_USAGE_DEFAULT;
	bd1.ByteWidth = sizeof(SimpleVertex) * (subdiv * subdiv);
	bd1.StructureByteStride = sizeof(SimpleVertex);
	bd1.CPUAccessFlags = 0;
	bd1.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA InitData1;
	ZeroMemory(&InitData1, sizeof(InitData1));
	InitData1.pSysMem = (float*)&vertices[0];

	D3D11_BUFFER_DESC bd2;
	ZeroMemory(&bd2, sizeof(bd2));
	bd2.Usage = D3D11_USAGE_DEFAULT;
	bd2.ByteWidth = 3 * 2 * sizeof(int) * ((subdiv - 1) * (subdiv - 1));
	bd2.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd2.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData2;
	ZeroMemory(&InitData2, sizeof(InitData2));
	InitData2.pSysMem = (int*)&indices[0];

	LensShapes::Patch patch;
	patch.subdiv = subdiv;
	g_pd3dDevice->CreateBuffer(&bd1, &InitData1, &patch.vertices);
	g_pd3dDevice->CreateBuffer(&bd2, &InitData2, &patch.indices);

	return patch;
}

void ParseLensComponents() {
	// Parse the lens components into the LensInterface the ray_trace routine expects
	nikon_28_75mm_lens_interface.resize(nikon_28_75mm_lens_components.size());

	for (int i = (int)nikon_28_75mm_lens_components.size() - 1; i >= 0; --i) {
		PatentFormat& entry = nikon_28_75mm_lens_components[i];
		total_lens_distance += entry.d;

		float left_ior = i == 0 ? 1.f : nikon_28_75mm_lens_components[i - 1].n;
		float right_ior = entry.n;

		if (right_ior != 1.f) {
			min_ior = min(min_ior, right_ior);
			max_ior = max(max_ior, right_ior);
		}

		vec3 center = { 0.f, 0.f, total_lens_distance - entry.r };
		vec3 n = { left_ior, 1.f, right_ior };

		LensInterface component = { total_lens_distance, center, entry.r, n, entry.h, 1.38f, entry.flat, entry.w, entry.h };
		nikon_28_75mm_lens_interface[i] = component;
	}
	
	std::stringstream s;
	s << "#define NUM_INTERFACE " << nikon_28_75mm_lens_components.size() << "\n";
	s << "static LensInterface interfaces[NUM_INTERFACE] = {\n\n";
	for (int i = 0; i < (int)nikon_28_75mm_lens_interface.size(); ++i) {
		LensInterface& c = nikon_28_75mm_lens_interface[i];
		s << "    { float3(" << c.center.x << ", " << c.center.y << ", " << c.center.z << "), " << c.radius << ", ";
		s << "float3(" << c.n.x << ", " << c.n.y << ", " << c.n.z << "), " << c.sa << ", " << c.d1 << ", ";
		s << (c.flat ? "true" : "false") << ", " << c.h << " },\n";
	}
	s << "\n};";

	std::string sss = s.str();
	int tt = 0; tt;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect( g_hWnd, &rc );
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
	#ifdef _DEBUG
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	#endif

	D3D_DRIVER_TYPE driverTypes[] = {
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};

	UINT numDriverTypes = ARRAYSIZE( driverTypes );

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	UINT numFeatureLevels = ARRAYSIZE( featureLevels );

	for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ ) {
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice( nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
								D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );

		if ( hr == E_INVALIDARG ) {
			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			hr = D3D11CreateDevice( nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
									D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );
		}

		if( SUCCEEDED( hr ) )
			break;
	}

	if( FAILED( hr ) )
		return hr;

	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	IDXGIDevice* dxgiDevice = nullptr;
	hr = g_pd3dDevice->QueryInterface( __uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice) );
	if (SUCCEEDED(hr)) {
		IDXGIAdapter* adapter = nullptr;
		hr = dxgiDevice->GetAdapter(&adapter);
		if (SUCCEEDED(hr)) {
			hr = adapter->GetParent( __uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory) );
			adapter->Release();
		}
		dxgiDevice->Release();
	}

	if (FAILED(hr))
		return hr;

	// Create swap chain
	IDXGIFactory2* dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface( __uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2) );
	if ( dxgiFactory2 ) {
		// DirectX 11.1 or later
		hr = g_pd3dDevice->QueryInterface( __uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_pd3dDevice1) );
		if (SUCCEEDED(hr)) {
			(void) g_pImmediateContext->QueryInterface( __uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1) );
		}

		DXGI_SWAP_CHAIN_DESC1 sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.Width = width;
		sd.Height = height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;

		hr = dxgiFactory2->CreateSwapChainForHwnd( g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1 );
		if (SUCCEEDED(hr)) {
			hr = g_pSwapChain1->QueryInterface( __uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain) );
		}

		dxgiFactory2->Release();
	} else {
		// DirectX 11.0 systems
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 1;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = g_hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;

		hr = dxgiFactory->CreateSwapChain( g_pd3dDevice, &sd, &g_pSwapChain );
	}

	// Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
	dxgiFactory->MakeWindowAssociation( g_hWnd, DXGI_MWA_NO_ALT_ENTER );

	dxgiFactory->Release();

	if (FAILED(hr))
		return hr;

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil);
	
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &pBackBuffer ) );
	if( FAILED( hr ) )
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &g_pRenderTargetView );
	pBackBuffer->Release();
	if( FAILED( hr ) )
		return hr;

	g_pImmediateContext->OMSetRenderTargets( 1, &g_pRenderTargetView, g_pDepthStencilView);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	vp.TopLeftX = 0.f;
	vp.TopLeftY = 0.f;
	g_pImmediateContext->RSSetViewports( 1, &vp );

	// Compile the vertex shader
	ID3DBlob* blob = nullptr;	
	D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, };
	UINT numElements = ARRAYSIZE(layout);

	// Default shader
	hr = CompileShaderFromSource(vertex_shader_source, "VS", "vs_5_0", &blob);
	hr = g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_pVertexShader);
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, blob->GetBufferPointer(), blob->GetBufferSize(), &g_pVertexLayout);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
	blob->Release();

	hr = CompileShaderFromSource(pixel_shader_source, "PS", "ps_5_0", &blob);
	hr = g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_pPixelShader);
	blob->Release();

	// Flare shader
	//hr = CompileShaderFromSource(flare_vertex_shader_source, "VS", "vs_5_0", &blob);
	hr = CompileShaderFromFile(L"lens.hlsl", "VS", "vs_5_0", &blob);
	hr = g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_pFlareVertexShader);
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, blob->GetBufferPointer(), blob->GetBufferSize(), &g_pVertexLayout);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
	blob->Release();

	//hr = CompileShaderFromSource(flare_pixel_shader_source, "PS", "ps_5_0", &blob);
	hr = CompileShaderFromFile(L"lens.hlsl", "PS", "ps_5_0", &blob);
	hr = g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_pFlarePixelShader);
	blob->Release();

	hr = CompileShaderFromFile(L"lens.hlsl", "GS", "gs_5_0", &blob);
	hr = g_pd3dDevice->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_pGeometryShader);
	blob->Release();

	D3D11_BLEND_DESC BlendState;
	D3D11_BLEND_DESC MaskedBlendState;
	ZeroMemory(&BlendState, sizeof(D3D11_BLEND_DESC));
	ZeroMemory(&MaskedBlendState, sizeof(D3D11_BLEND_DESC));

	BlendState.RenderTarget[0].BlendEnable = TRUE;
	BlendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	BlendState.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendState.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	BlendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	BlendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	BlendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

	MaskedBlendState.RenderTarget[0] = BlendState.RenderTarget[0];
	MaskedBlendState.RenderTarget[0].RenderTargetWriteMask = 0x0;

	g_pd3dDevice->CreateBlendState(&BlendState, &g_pBlendStateBlend);
	g_pd3dDevice->CreateBlendState(&MaskedBlendState, &g_pBlendStateMask);

	D3D11_DEPTH_STENCIL_DESC DepthStencilState;
	ZeroMemory(&DepthStencilState, sizeof(D3D11_DEPTH_STENCIL_DESC));
	DepthStencilState.DepthEnable = FALSE;
	DepthStencilState.StencilEnable = FALSE;
	DepthStencilState.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilState.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilState.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilState.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	DepthStencilState.BackFace = DepthStencilState.FrontFace;
	
	D3D11_DEPTH_STENCIL_DESC DepthStencilStateFill;
	D3D11_DEPTH_STENCIL_DESC DepthStencilStateGreaterOrEqualIncr;
	D3D11_DEPTH_STENCIL_DESC DepthStencilStateGreaterOrEqualDecr;
	D3D11_DEPTH_STENCIL_DESC DepthStencilStateGreaterOrEqualRead;

	ZeroMemory(&DepthStencilStateFill, sizeof(D3D11_DEPTH_STENCIL_DESC));
	ZeroMemory(&DepthStencilStateGreaterOrEqualIncr, sizeof(D3D11_DEPTH_STENCIL_DESC));
	ZeroMemory(&DepthStencilStateGreaterOrEqualDecr, sizeof(D3D11_DEPTH_STENCIL_DESC));
	ZeroMemory(&DepthStencilStateGreaterOrEqualRead, sizeof(D3D11_DEPTH_STENCIL_DESC));

	DepthStencilStateFill.DepthEnable = FALSE;
	DepthStencilStateFill.StencilEnable = TRUE;
	DepthStencilStateFill.StencilWriteMask = 0xFF;
	DepthStencilStateFill.StencilReadMask = 0xFF;
	DepthStencilStateFill.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
	DepthStencilStateFill.FrontFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
	DepthStencilStateFill.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	DepthStencilStateFill.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	DepthStencilStateFill.BackFace = DepthStencilStateFill.FrontFace;

	DepthStencilStateGreaterOrEqualIncr.DepthEnable = FALSE;
	DepthStencilStateGreaterOrEqualIncr.StencilEnable = TRUE;
	DepthStencilStateGreaterOrEqualIncr.StencilWriteMask = 0xFF;
	DepthStencilStateGreaterOrEqualIncr.StencilReadMask = 0xFF;
	DepthStencilStateGreaterOrEqualIncr.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilStateGreaterOrEqualIncr.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilStateGreaterOrEqualIncr.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
	DepthStencilStateGreaterOrEqualIncr.FrontFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
	DepthStencilStateGreaterOrEqualIncr.BackFace = DepthStencilStateGreaterOrEqualIncr.FrontFace;

	DepthStencilStateGreaterOrEqualDecr = DepthStencilStateGreaterOrEqualIncr;
	DepthStencilStateGreaterOrEqualDecr.FrontFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
	DepthStencilStateGreaterOrEqualDecr.BackFace = DepthStencilStateGreaterOrEqualDecr.FrontFace;

	DepthStencilStateGreaterOrEqualRead = DepthStencilStateGreaterOrEqualIncr;
	DepthStencilStateGreaterOrEqualRead.StencilWriteMask = 0X00;
	DepthStencilStateGreaterOrEqualRead.FrontFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
	DepthStencilStateGreaterOrEqualRead.BackFace = DepthStencilStateGreaterOrEqualRead.FrontFace;

	g_pd3dDevice->CreateDepthStencilState(&DepthStencilState, &g_pDepthStencilState);
	g_pd3dDevice->CreateDepthStencilState(&DepthStencilStateFill, &g_pDepthStencilStateFill);
	g_pd3dDevice->CreateDepthStencilState(&DepthStencilStateGreaterOrEqualIncr, &g_pDepthStencilStateGreaterOrEqualIncr);
	g_pd3dDevice->CreateDepthStencilState(&DepthStencilStateGreaterOrEqualDecr, &g_pDepthStencilStateGreaterOrEqualDecr);
	g_pd3dDevice->CreateDepthStencilState(&DepthStencilStateGreaterOrEqualRead, &g_pDepthStencilStateGreaterOrEqualRead);

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.ByteWidth = sizeof(InstanceUniforms);
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_InstanceUniforms);

	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.ByteWidth = sizeof(GlobalUniforms);
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_GlobalUniforms);

	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.ByteWidth = sizeof(SimpleVertex) * num_of_intersections_1;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_IntersectionPoints1);
	
	bd.ByteWidth = sizeof(SimpleVertex) * num_of_intersections_2;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_IntersectionPoints2);
	
	bd.ByteWidth = sizeof(SimpleVertex) * num_of_intersections_3;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_IntersectionPoints3);

	unit_circle = CreateUnitCircle();
	unit_square = CreateUnitRectangle();
	unit_patch = CreateUnitPatch(patch_tessellation);

	ParseLensComponents();

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice() {
	if( g_pImmediateContext ) g_pImmediateContext->ClearState();
	if( g_pVertexBuffer ) g_pVertexBuffer->Release();
	if( g_pVertexLayout ) g_pVertexLayout->Release();
	if( g_pVertexShader ) g_pVertexShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();
	if( g_pRenderTargetView ) g_pRenderTargetView->Release();
	if( g_pSwapChain1 ) g_pSwapChain1->Release();
	if( g_pSwapChain ) g_pSwapChain->Release();
	if( g_pImmediateContext1 ) g_pImmediateContext1->Release();
	if( g_pImmediateContext ) g_pImmediateContext->Release();
	if( g_pd3dDevice1 ) g_pd3dDevice1->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
	if (g_InstanceUniforms) g_InstanceUniforms->Release();
	if (g_IntersectionPoints1) g_IntersectionPoints1->Release();
	if (g_IntersectionPoints2) g_IntersectionPoints2->Release();
	if (g_IntersectionPoints3) g_IntersectionPoints3->Release();
	if (g_pDepthStencil) g_pDepthStencil->Release();
	if (g_pDepthStencilView) g_pDepthStencilView->Release();
	if (g_pBlendStateBlend) g_pBlendStateBlend->Release();
	if (g_pBlendStateMask) g_pBlendStateMask->Release();
	if (g_pDepthStencilState) g_pDepthStencilState->Release();
	if (g_pDepthStencilStateFill) g_pDepthStencilStateFill->Release();
	if (g_pDepthStencilStateGreaterOrEqualIncr) g_pDepthStencilStateGreaterOrEqualIncr->Release();
	if (g_pDepthStencilStateGreaterOrEqualDecr) g_pDepthStencilStateGreaterOrEqualDecr->Release();
	if (g_pDepthStencilStateGreaterOrEqualRead) g_pDepthStencilStateGreaterOrEqualRead->Release();
}

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) {
	PAINTSTRUCT ps;
	HDC hdc;

	switch( message ) {
		case WM_PAINT:
			hdc = BeginPaint( hWnd, &ps );
			EndPaint( hWnd, &ps );
			break;

		case WM_DESTROY:
			PostQuitMessage( 0 );
			break;

		default:
			return DefWindowProc( hWnd, message, wParam, lParam );
	}

	return 0;
}

void DrawRectangle(ID3D11DeviceContext* context, LensShapes::Rectangle& rectangle, XMFLOAT4& color, XMFLOAT4& placement, bool filled) {
	InstanceUniforms cb = { color, placement };
	context->UpdateSubresource(g_InstanceUniforms, 0, nullptr, &cb, 0, 0);
	if (filled) {
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		context->IASetVertexBuffers(0, 1, &rectangle.vertices, &stride, &offset);
		context->Draw(6, 0);
	}
	else {
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
		context->IASetVertexBuffers(0, 1, &rectangle.lines, &stride, &offset);
		context->Draw(5, 0);
	}
}

void DrawCircle(ID3D11DeviceContext* context, LensShapes::Circle& circle, XMFLOAT4& color, XMFLOAT4& placement, bool filled) {
	InstanceUniforms cb = { color, placement };
	context->UpdateSubresource(g_InstanceUniforms, 0, nullptr, &cb, 0, 0);
	if (filled) {
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		context->IASetVertexBuffers(0, 1, &circle.triangles, &stride, &offset);
		context->Draw(num_vertices_per_cirlces, 0);
	}
	else {
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
		context->IASetVertexBuffers(0, 1, &circle.lines, &stride, &offset);
		context->Draw(num_points_per_cirlces, 0);
	}
}

void DrawFlat(LensInterface& right) {
	float mx = -(right.pos * global_scale - 1.f);
	float mw = global_scale * 0.4f;
	
	XMFLOAT4 mask_placement1 = { mx, 1.f, mw * 1.00f, global_scale * right.w };
	XMFLOAT4 mask_placement2 = { mx, 1.f, mw * 1.01f, global_scale * right.h };
	XMFLOAT4 mask_placement3 = { mx + 0.0001f, 1.f, mw * 0.9f, global_scale * right.w * 0.9f };

	g_pImmediateContext->OMSetBlendState(g_pBlendStateMask, blendFactor, sampleMask);
	g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateFill, 1);
	DrawRectangle(g_pImmediateContext, unit_square, flat_fill_color, mask_placement1, true);

	g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateFill, 0);
	DrawRectangle(g_pImmediateContext, unit_square, flat_fill_color, mask_placement2, true);

	g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateGreaterOrEqualRead, 1);
	g_pImmediateContext->OMSetBlendState(g_pBlendStateBlend, blendFactor, sampleMask);
	DrawRectangle(g_pImmediateContext, unit_square, flat_fill_color, mask_placement3, true);
	DrawRectangle(g_pImmediateContext, unit_square, stroke_color, mask_placement3, false);
}

XMFLOAT4 LensColor(float ior, XMFLOAT4& c1, XMFLOAT4&c2) {
	float normalized_ior = (ior - min_ior) / (max_ior - min_ior);
	return lerp(c1, c2, normalized_ior);
	//return normalized_ior < 0.5f ? c1 : c2;
}

XMFLOAT4 IntersectionColor(int i) {
	float ior1 = nikon_28_75mm_lens_interface[i].n.x == 1.f ? nikon_28_75mm_lens_interface[i].n.z : nikon_28_75mm_lens_interface[i].n.x;
	return LensColor(ior1, intersection_color2, intersection_color3);
}

void DrawLens(LensInterface& left, LensInterface& right, bool opaque) {
	
	XMFLOAT4 fill_color = LensColor(right.n.x, fill_color1, fill_color2);
	fill_color.w = opaque ? opaque_alpha : fill_color.w;
	
	if (opaque)
		stroke_color = stroke_color1;
	else
		stroke_color = stroke_color2;

	//  |\      /|
	//  | |    | |
	//  | | or | |
	//  | |    | |
	//  |/      \|
	if (sign(left.radius) == sign(right.radius)) {
		LensInterface _left = left;
		LensInterface _right = right;
		float eps = 0.001f;
		
		float min_radius = max(left.radius, right.radius);
		if ((right.radius) > 0 && (left.radius > 0.f)) {
			min_radius = min(left.radius, right.radius);
			eps *= -1.f;
			_left = right;
			_right = left;
		}

		float delta = abs(_right.pos - _left.pos) * 0.5f;
		float mx = -(_right.pos * global_scale - 1.f) + eps;
		float mw = (min_radius - delta) * global_scale * right.w;
		float mh = global_scale * right.h;
		XMFLOAT4 mask_placement = { mx, 1.f, mw, mh * 1.001f };
		XMFLOAT4 mask_placement2 = { mx, 1.f, mw * 0.995f, mh * 0.997f };

		float rx = -(_right.pos * global_scale - 1.f);
		float rr = _right.radius * global_scale;
		XMFLOAT4 right_placement = { rx, 1.f, rr, rr };

		float lx = -(_left.pos * global_scale - 1.f);
		float lr = _left.radius * global_scale;
		XMFLOAT4 left_placement = { lx, 1.f, lr, lr };

		g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_STENCIL, 1.0f, 0);
		g_pImmediateContext->OMSetBlendState(g_pBlendStateMask, blendFactor, sampleMask);
		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateFill, 1);
		DrawRectangle(g_pImmediateContext, unit_square, stroke_color, mask_placement, true);

		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateGreaterOrEqualIncr, 1);
		DrawCircle(g_pImmediateContext, unit_circle, fill_color, right_placement, true);

		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateGreaterOrEqualDecr, 2);
		DrawCircle(g_pImmediateContext, unit_circle, fill_color, left_placement, true);

		g_pImmediateContext->OMSetBlendState(g_pBlendStateBlend, blendFactor, sampleMask);
		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateGreaterOrEqualRead, 2);
		DrawRectangle(g_pImmediateContext, unit_square, fill_color, mask_placement, true);
		DrawRectangle(g_pImmediateContext, unit_square, stroke_color, mask_placement2, false);

		g_pImmediateContext->OMSetBlendState(g_pBlendStateMask, blendFactor, sampleMask);
		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateFill, 1);
		DrawRectangle(g_pImmediateContext, unit_square, stroke_color, mask_placement, true);

		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateGreaterOrEqualRead, 1);
		g_pImmediateContext->OMSetBlendState(g_pBlendStateBlend, blendFactor, sampleMask);
		DrawCircle(g_pImmediateContext, unit_circle, stroke_color, left_placement, false);
		DrawCircle(g_pImmediateContext, unit_circle, stroke_color, right_placement, false);
	}
	//     / \
	//    |   |
	//    |   |
	//    |   |
	//     \ /
	else if (left.radius > 0.f && right.radius < 0.f) {
		float eps = 0.001f;
		float delta = abs(right.pos - left.pos);
		float mx = -(right.pos * global_scale - 1.f) + eps;
		float mw = -delta * global_scale * right.w - eps;
		float mh = global_scale * right.h;
		XMFLOAT4 mask_placement = { mx, 1.f, mw, mh };
		XMFLOAT4 mask_placement2 = { mx, 1.f, mw * 0.995f, mh * 0.997f };

		float lx = -(left.pos * global_scale - 1.f);
		float lr = left.radius * global_scale;
		XMFLOAT4 left_placement = { lx, 1.f, lr, lr };

		float rx = -(right.pos * global_scale - 1.f);
		float rr = right.radius * global_scale;
		XMFLOAT4 right_placement = { rx, 1.f, rr, rr };

		g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_STENCIL, 1.0f, 0);
		g_pImmediateContext->OMSetBlendState(g_pBlendStateMask, blendFactor, sampleMask);
		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateFill, 1);
		DrawRectangle(g_pImmediateContext, unit_square, stroke_color, mask_placement, true);
		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateGreaterOrEqualIncr, 1);
		DrawCircle(g_pImmediateContext, unit_circle, stroke_color, left_placement, true);

		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateGreaterOrEqualIncr, 2);
		g_pImmediateContext->OMSetBlendState(g_pBlendStateBlend, blendFactor, sampleMask);
		DrawCircle(g_pImmediateContext, unit_circle, fill_color, right_placement, true);

		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateGreaterOrEqualRead, 3);
		DrawRectangle(g_pImmediateContext, unit_square, stroke_color, mask_placement2, false);

		g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_STENCIL, 1.0f, 0);
		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateFill, 1);
		g_pImmediateContext->OMSetBlendState(g_pBlendStateMask, blendFactor, sampleMask);
		DrawRectangle(g_pImmediateContext, unit_square, stroke_color, mask_placement, true);

		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateGreaterOrEqualRead, 1);
		g_pImmediateContext->OMSetBlendState(g_pBlendStateBlend, blendFactor, sampleMask);
		DrawCircle(g_pImmediateContext, unit_circle, stroke_color, left_placement, false);
		DrawCircle(g_pImmediateContext, unit_circle, stroke_color, right_placement, false);
	}
	//   \    /
	//    |  |
	//    |  |
	//    |  |
	//   /    \ 
	else if (left.radius < 0.f && right.radius > 0.f) {
		float delta = abs(right.pos - left.pos);
		float w = delta * right.w;
		float mx = -((right.pos + delta * 0.5f + w) * global_scale - 1.f);
		float mw = global_scale * w;
		float mh = global_scale * right.h;
		XMFLOAT4 mask_placement = { mx, 1.f, mw, mh };
		XMFLOAT4 mask_placement2 = { mx, 1.f, mw * 0.995f, mh * 0.995f };

		float lx = -(left.pos * global_scale - 1.f);
		float lr = left.radius * global_scale;
		XMFLOAT4 left_placement = { lx, 1.f, lr, lr };

		float rx = -(right.pos * global_scale - 1.f);
		float rr = right.radius * global_scale;
		XMFLOAT4 right_placement = { rx, 1.f, rr, rr };

		g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_STENCIL, 1.0f, 0);
		g_pImmediateContext->OMSetBlendState(g_pBlendStateMask, blendFactor, sampleMask);
		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateFill, 1);
		DrawRectangle(g_pImmediateContext, unit_square, fill_color, mask_placement, true);

		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateFill, 0);
		DrawCircle(g_pImmediateContext, unit_circle, fill_color, left_placement, true);
		DrawCircle(g_pImmediateContext, unit_circle, fill_color, right_placement, true);

		g_pImmediateContext->OMSetBlendState(g_pBlendStateBlend, blendFactor, sampleMask);
		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateGreaterOrEqualRead, 1);
		DrawRectangle(g_pImmediateContext, unit_square, fill_color, mask_placement, true);
		DrawRectangle(g_pImmediateContext, unit_square, stroke_color, mask_placement2, false);

		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateFill, 1);
		g_pImmediateContext->OMSetBlendState(g_pBlendStateMask, blendFactor, sampleMask);
		DrawRectangle(g_pImmediateContext, unit_square, fill_color, mask_placement, true);

		g_pImmediateContext->OMSetBlendState(g_pBlendStateBlend, blendFactor, sampleMask);
		g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateGreaterOrEqualRead, 1);
		DrawCircle(g_pImmediateContext, unit_circle, stroke_color, left_placement, false);
		DrawCircle(g_pImmediateContext, unit_circle, stroke_color, right_placement, false);
	}
}

void DrawLensInterface(std::vector<LensInterface>& lens_interface) {

	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_STENCIL, 1.0f, 0);
	g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilState, 0);
	g_pImmediateContext->OMSetBlendState(g_pBlendStateBlend, blendFactor, sampleMask);

	int i = 0;
	while (i < (int)lens_interface.size()) {
		bool opaque1 = (i == ghost_bounce_1 - 1);
		bool opaque2 = (i == ghost_bounce_2 - 1);
		if (lens_interface[i].flat) {
			DrawFlat(lens_interface[i]);
			i += 1;
		} else if (lens_interface[i].n.x == 1.f) {
			opaque1 = opaque1 || (i == ghost_bounce_1 - 2);
			opaque2 = opaque2 || (i == ghost_bounce_2 - 2);
			DrawLens(lens_interface[i], lens_interface[i + 1], opaque1 || opaque2);
			i += 2;
		} else {
			DrawLens(lens_interface[i - 1], lens_interface[i], opaque1 || opaque2);
			i += 1;
		}
	}
}

void DrawIntersections(ID3D11DeviceContext* context, ID3D11Buffer* buffer, std::vector<vec3>& intersections, int max_points, XMFLOAT4& color) {
	InstanceUniforms cb;
	cb.color = color;
	cb.placement = XMFLOAT4(0.f, 0.f, 0.f, 0.f);

	std::vector<XMFLOAT3> points(max_points);
	for (int i = 0; i < (int)intersections.size(); ++i) {
		points[i] = (point_to_d3d(intersections[i]));
	}

	void* ptr = &points.front();
	context->UpdateSubresource(buffer, 0, nullptr, ptr, 0, 0);
	context->UpdateSubresource(g_InstanceUniforms, 0, nullptr, &cb, 0, 0);
	
	g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilState, 0);
	g_pImmediateContext->OMSetBlendState(g_pBlendStateBlend, blendFactor, sampleMask);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
	context->Draw((int)intersections.size(), 0);
}

void DrawPatch(LensShapes::Patch& patch, XMFLOAT4& color) {
	InstanceUniforms cb;
	cb.color = color;
	cb.placement = XMFLOAT4(0.f, 0.f, 0.f, 0.f);

	g_pImmediateContext->UpdateSubresource(g_InstanceUniforms, 0, nullptr, &cb, 0, 0);

	g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilState, 0);
	g_pImmediateContext->OMSetBlendState(g_pBlendStateBlend, blendFactor, sampleMask);

	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	g_pImmediateContext->IASetVertexBuffers(0, 1, &patch.vertices, &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(patch.indices, DXGI_FORMAT_R32_UINT, 0);
	
	int t = patch_tessellation - 1;
	g_pImmediateContext->DrawIndexed(t * t * 3 * 2, 0, 0);

	cb.color.w = 1.f;
	g_pImmediateContext->UpdateSubresource(g_InstanceUniforms, 0, nullptr, &cb, 0, 0);
	
	//g_pImmediateContext->GSSetShader(nullptr, nullptr, 0);
	//g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	//g_pImmediateContext->DrawIndexed(t * t * 3 * 2, 0, 0);

}

void CycleBounces() {

	ghost_bounce_1 = ghost_bounce_2 + 1 + (int)time;

	if (ghost_bounce_1 == 15) timer_start -= TimeToTick(1);
	if (ghost_bounce_2 == 15) ghost_bounce_2++;

	if (ghost_bounce_1 >= (int)(nikon_28_75mm_lens_interface.size() - 1)) {
		ghost_bounce_2++;
		timer_start = GetTickCount64();
	}

	if (ghost_bounce_2 >= (int)(nikon_28_75mm_lens_interface.size() - 1)) {
		ghost_bounce_2 = 1;
		timer_start = GetTickCount64();
	}
}

void AnimateFocusGroup() {
	float anim_g4_lens = sin(time * focus_speed) * 0.01f;
	for (int i = 6; i < 14; ++i) {
		nikon_28_75mm_lens_interface[i].center.z += anim_g4_lens;
		nikon_28_75mm_lens_interface[i].pos += anim_g4_lens;
	}
}

float AnimateRayDirection() {
	return sin(time * swing_speed * PI) * swing_angle;
}

float AnimateSpread() {
	float anim_rays_spread = (cos(time * spread_speed * PI) + 1.f) * 0.5f;
	intersection_color1.w = lerp(0.05f, 0.01f, anim_rays_spread);
	intersection_color2.w = lerp(0.30f, 0.01f, anim_rays_spread);
	intersection_color3.w = lerp(0.30f, 0.01f, anim_rays_spread);
	return lerp(rays_spread2, rays_spread1, anim_rays_spread);
}

void Tick() {
	//time += speed * 0.01f;
	time = (GetTickCount64() - timer_start) / 1000.0f * speed;

	GlobalUniforms cb = { time, (float)ghost_bounce_1 , (float)ghost_bounce_2 };
	g_pImmediateContext->UpdateSubresource(g_GlobalUniforms, 0, nullptr, &cb, 0, 0);
}

//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render() {
	Tick();
	CycleBounces();

	#if defined(DRAW3D)
		g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, XMVECTORF32{ background_color2.x, background_color2.y, background_color2.z, background_color2.w });
		g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	
		g_pImmediateContext->VSSetShader(g_pFlareVertexShader, nullptr, 0);
		g_pImmediateContext->PSSetShader(g_pFlarePixelShader, nullptr, 0);
		g_pImmediateContext->GSSetShader(g_pGeometryShader, nullptr, 0);
		
		g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_InstanceUniforms);
		g_pImmediateContext->VSSetConstantBuffers(1, 1, &g_GlobalUniforms);
		g_pImmediateContext->GSSetConstantBuffers(0, 1, &g_InstanceUniforms);
		g_pImmediateContext->GSSetConstantBuffers(1, 1, &g_GlobalUniforms);
		g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_InstanceUniforms);
		g_pImmediateContext->PSSetConstantBuffers(1, 1, &g_GlobalUniforms);

		DrawPatch(unit_patch, fill_color1);
	#else
		g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, XMVECTORF32{ background_color1.x, background_color1.y, background_color1.z, background_color1.w });
		g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	
		g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
		g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
		g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_InstanceUniforms);
		g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_InstanceUniforms);

		float anim_ray_direction = AnimateRayDirection();
		float rays_spread = AnimateSpread();
		AnimateFocusGroup();

		// Trace all rays
		std::vector<std::vector<vec3>> intersections1(num_of_rays);
		std::vector<std::vector<vec3>> intersections2(num_of_rays);
		std::vector<std::vector<vec3>> intersections3(num_of_rays);
		for (int i = 0; i < num_of_rays; ++i) {
			float pos = lerp(-1.f, 1.f, (float)i / (float)num_of_rays) * rays_spread;
			vec3 a = vec3(0.0f, pos, total_lens_distance);
			vec3 b = vec3(0.0f, pos + anim_ray_direction, total_lens_distance + 10.f);
			vec3 c = normalize(a - b);
			Ray r = { a - c * 20.f, c };
			Trace(r, 1.f, nikon_28_75mm_lens_interface, intersections1[i], intersections2[i], intersections3[i], int2{ ghost_bounce_1, ghost_bounce_2 });
		}

		// Draw all rays
		XMFLOAT4 ghost_color1 = IntersectionColor(ghost_bounce_1 - 1);
		XMFLOAT4 ghost_color2 = IntersectionColor(ghost_bounce_2 - 1);
		for (int i = 0; i < num_of_rays; ++i)
			DrawIntersections(g_pImmediateContext, g_IntersectionPoints1, intersections1[i], num_of_intersections_1, intersection_color1);

		for (int i = 0; i < num_of_rays; ++i)
			DrawIntersections(g_pImmediateContext, g_IntersectionPoints2, intersections2[i], num_of_intersections_2, ghost_color1);

		for (int i = 0; i < num_of_rays; ++i)
			DrawIntersections(g_pImmediateContext, g_IntersectionPoints3, intersections3[i], num_of_intersections_3, ghost_color2);

		// Draw lenses
		DrawLensInterface(nikon_28_75mm_lens_interface);
	#endif

	g_pSwapChain->Present(0, 0);
	// SaveBackBuffer();
}
