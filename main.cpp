#include <SFML/Graphics.hpp>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdint>

#include <fstream>
#include <strstream>

#include <limits>
#include <cmath>
#include <unordered_map>

#define VC_EXTRALEAN
#include <Windows.h>
#include <deque>
using namespace std;

float toRadians(float degree)
{
	return degree * 3.14159f / 180.f;
}

constexpr size_t INVALID_TRI_INDEX = std::numeric_limits<size_t>::max();
constexpr unsigned SCREEN_W = 1600;
constexpr unsigned SCREEN_H = 900;
constexpr float width = 1600.f;
constexpr float height = 900.f;

struct v3d
{
	union {
		struct { float x, y, z, w; };
		struct { float pitch, yaw, roll; };
		float n[3] = { 0, 0, 0 };
	};

	v3d()
	{
		x = y = z = 0;
		w = 1.f;
	}

	v3d(float a, float b, float c)
	{
		x = a;  y = b;  z = c;
		w = 1.f;
	}

	v3d(float a, float b, float c, float d)
	{
		x = a;  y = b;  z = c;
		w = d;
	}

	float length() const
	{
		return sqrtf(x * x + y * y + z * z);
	}

	v3d normal()
	{
		return *this / length();
	}



	float lawofcosines_cuz_chatGPT_is_a_pain_in_my_ass(const v3d& b) const
	{
		return (x * b.x + y * b.y + z * b.z) / (length() * b.length());
	}

	float cosBetweenVecs(const v3d& b) const
	{
		return lawofcosines_cuz_chatGPT_is_a_pain_in_my_ass(b);
	}

	float dot(const v3d& b) const
	{
		return x * b.x + y * b.y + z * b.z;
	}

	float angleTo(const v3d& b) const
	{
		float denom = length() * b.length();

		if (denom == 0.0f)
		{
			return 0.0f;
		}

		float c = dot(b) / denom;
		c = std::clamp(c, -1.0f, 1.0f);

		return acosf(c);
	}


	v3d cross(const v3d& b)
	{
		return { n[1] * b.n[2] - n[2] * b.n[1],
					 n[2] * b.n[0] - n[0] * b.n[2],
					 n[0] * b.n[1] - n[1] * b.n[0] };
	}

	v3d& operator+=(const v3d& rhs)
	{
		this->x += rhs.x;
		this->y += rhs.y;
		this->z += rhs.z;
		return *this;
	}
	v3d& operator-=(const v3d& rhs)
	{
		this->x -= rhs.x;
		this->y -= rhs.y;
		this->z -= rhs.z;
		return *this;
	}
	v3d& operator*=(const float rhs)
	{
		this->x *= rhs;
		this->y *= rhs;
		this->z *= rhs;
		return *this;
	}
	v3d& operator/=(const float rhs)
	{
		this->x /= rhs;
		this->y /= rhs;
		this->z /= rhs;
		return *this;
	}

	v3d operator+(const v3d& rhs)
	{
		v3d r = { 0 , 0 , 0 };
		r.x = this->x + rhs.x;
		r.y = this->y + rhs.y;
		r.z = this->z + rhs.z;
		return r;
	}

	v3d operator-(const v3d& rhs)
	{
		v3d r = { 0 , 0 , 0 };
		r.x = this->x - rhs.x;
		r.y = this->y - rhs.y;
		r.z = this->z - rhs.z;
		return r;
	}

	v3d operator*(const float rhs)
	{
		v3d r = { 0 , 0 , 0 };
		r.x = this->x * rhs;
		r.y = this->y * rhs;
		r.z = this->z * rhs;
		return r;
	}

	v3d operator/(const float rhs)
	{
		v3d r = { 0 , 0 , 0 };
		r.x = this->x / rhs;
		r.y = this->y / rhs;
		r.z = this->z / rhs;
		return r;
	}
};
struct triangle
{
	v3d p[3];

	int vi[3] = { -1, -1, -1 };

	wchar_t sym;
	short col;
};
struct mesh
{
	vector<triangle> tris;

	bool LoadObjectFile(string filename)
	{
		ifstream f(filename);
		if (!f.is_open())
		{
			return false;
		}

		vector<v3d> verts;

		while (!f.eof())
		{
			char line[128];
			f.getline(line, 128);

			strstream s;
			s << line;

			char junk;

			if (line[0] == 'v' && line[1] == ' ')
			{
				v3d v;
				s >> junk >> v.x >> v.y >> v.z;
				verts.push_back(v);
			}
			if (line[0] == 'f' && line[1] == ' ')
			{
				int f[3];
				s >> junk >> f[0] >> f[1] >> f[2];

				triangle tri;
				tri.p[0] = verts[f[0] - 1];
				tri.p[1] = verts[f[1] - 1];
				tri.p[2] = verts[f[2] - 1];

				tri.vi[0] = f[0] - 1;
				tri.vi[1] = f[1] - 1;
				tri.vi[2] = f[2] - 1;

				tris.push_back(tri);
			}
		}

		return true;
	}
};

mesh meshCube;
mesh meshObject;

struct m4x4
{
	float m[4][4] = { 0 };
};

m4x4 matProj;
v3d camera;
v3d lookDir;

float camYaw;

struct TriangleToRaster
{
	v3d p[3];
	float lum = 0.f;
	size_t meshTriIndex = 0;
};
struct MeshEdge
{
	int v0 = -1;
	int v1 = -1;


	size_t triA = INVALID_TRI_INDEX;
	size_t triB = INVALID_TRI_INDEX;

	int edgeA = -1;
	int edgeB = -1;
};

std::vector<MeshEdge> meshEdges;
std::vector<TriangleToRaster> rasterByMeshTri;
std::vector<bool> rasterValidByMeshTri;
std::vector<v3d> normalByMeshTri;
std::vector<std::uint8_t> framePixels(SCREEN_W* SCREEN_H * 4);
std::vector<float> depthBuffer(SCREEN_W* SCREEN_H);
sf::Texture frameTexture(sf::Vector2u{ SCREEN_W, SCREEN_H });
std::vector<TriangleToRaster> trisToRaster;
constexpr float FPS = 1.f / 60.f;
float theta;

bool update(float dt_);
void render(sf::RenderWindow& wnd);

/// <summary>
/// GOODIES FUNCTIONS
/// </summary>
namespace {
float EdgeFunction(const v3d & a, const v3d & b, float x, float y);
void ClearSoftwareFrame();
void PutPixelDepth(int x, int y, float z, std::uint8_t r, std::uint8_t g, std::uint8_t b);
void RasterizeTriangleDepth(const TriangleToRaster& tri);
void PutPixelLineIfVisible(int x, int y, float z);
void RasterizeLineDepth(const v3d& a, const v3d& b);
std::uint64_t MakeEdgeKey(int a, int b);
void BuildMeshEdges(const mesh& m);
float Dot(const v3d& a, const v3d& b);
void RasterizeVisibleMeshOutlineDepth();
v3d Matrix_MultVector(const m4x4& m, const v3d& i);
v3d Vector_Add(const v3d& v1, const v3d& v2);
v3d Vector_Sub(const v3d& v1, const v3d& v2);
v3d Vector_Mul(const v3d& v1, float k);
v3d Vector_Div(const v3d& v1, float k);
float Vector_DotProduct(const v3d& v1, const v3d& v2);
float Vector_Length(const v3d& v);
v3d Vector_Normalize(const v3d& v);
v3d Vector_CrossProduct(const v3d& v1, const v3d& v2);
m4x4 Matrix_MakeIdentity();
m4x4 Matrix_MakeRotationX(float angleRad);
m4x4 Matrix_MakeRotationY(float angleRad);
m4x4 Matrix_MakeRotationZ(float angleRad);
m4x4 Matrix_MakeTranslation(float x, float y, float z);
m4x4 Matrix_MakeProjection(float fovDegrees, float aspectRatio, float zNear, float zFar);
m4x4 Matrix_MultiplyMatrix(const m4x4& m1, const m4x4& m2);
m4x4 Matrix_PointAt(const v3d& pos, const v3d& target, const v3d& up);
m4x4 Matrix_QuickInverse(const m4x4& m);
v3d Vector_IntersectPlane(const v3d& plane_p, const v3d& plane_n, const v3d& lineStart, const v3d& lineEnd);
int Triangle_ClipAgainstPlane(const v3d& plane_p, v3d plane_n, const triangle& in_tri, triangle& out_tri1,  triangle& out_tri2);
int ClipRasterTriangleAgainstPlane(const v3d& plane_p,	v3d plane_n,	const TriangleToRaster& in_tri,	TriangleToRaster& out_tri1,	TriangleToRaster& out_tri2);
}

int main()
{
	meshCube.LoadObjectFile("assets/models/mountains.obj");
	::BuildMeshEdges(meshCube);

	matProj = ::Matrix_MakeProjection(90.f, height / width, 0.1f, 1000.f);

	sf::RenderWindow wnd{ sf::VideoMode{{(unsigned)width,(unsigned)height},32U}, "3D Engine"};
	sf::VideoMode vid = sf::VideoMode::getDesktopMode();
	wnd.setPosition({ int(vid.size.x - wnd.getSize().x) / 2,int(vid.size.y - wnd.getSize().y) / 3});
	::ShowWindow(::GetConsoleWindow(), SW_HIDE);

	float dt = 0.f;
	sf::Clock dtClock{};
	while (wnd.isOpen())
	{
		// input
		while (const std::optional event = wnd.pollEvent())
		{
			if (auto result = event->getIf<sf::Event::KeyPressed>())
			{
				if (result->code == sf::Keyboard::Key::Escape)	  { wnd.close(); }
			}
			if (event->is<sf::Event::Closed>())		{ wnd.close(); }
		}

		// update
		dt += dtClock.restart().asSeconds();
		if (dt > 0.05f) { dt = 0.05f; }
		bool repaint = false;
		while (dt >= FPS)
		{
			repaint = true;
			if (!update(FPS))
			{
				wnd.close();
				return 1;
			}
			dt -= FPS;
		}

		// draw
		if (repaint)
			render(wnd);
	}
	return 0;
}

bool update(float dt_)
{
	if (GetAsyncKeyState(VK_UP))
	{
		camera.y -= 8.0f * dt_;
	}
	if (GetAsyncKeyState(VK_DOWN))
	{
		camera.y += 8.0f * dt_;
	}

	// Rebuild camera direction before movement
	v3d up = { 0, 1, 0 };
	v3d targetForward = { 0, 0, 1 };

	m4x4 matCameraRot = ::Matrix_MakeRotationY(camYaw);
	lookDir = ::Matrix_MultVector(matCameraRot, targetForward);
	lookDir = ::Vector_Normalize(lookDir);

	// Camera-local right vector
	v3d rightDir = ::Vector_CrossProduct(up, lookDir);
	rightDir = ::Vector_Normalize(rightDir);
	v3d mvForward = ::Vector_Mul(lookDir, 8.0f * dt_);
	v3d mvRight = ::Vector_Mul(rightDir,  8.0f * dt_);

	// Strafe left/right relative to camera view
	if (GetAsyncKeyState(VK_LEFT) & 0x8000)
	{
		camera = ::Vector_Sub(camera, mvRight);
	}

	if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
	{
		camera = ::Vector_Add(camera, mvRight);
	}

	if (GetAsyncKeyState('W') & 0x8000)
	{
		camera = ::Vector_Add(camera, mvForward);
	}
	if (GetAsyncKeyState('A') & 0x8000)
	{
		camYaw += 2.f * dt_;
	}

	if (GetAsyncKeyState('S') & 0x8000)
	{
		camera = ::Vector_Sub(camera, mvForward);
	}
	if (GetAsyncKeyState('D') & 0x8000)
	{
		camYaw -= 2.f * dt_;
	}


	m4x4 matRotZ, matRotX, matRotY;
	theta = 90.f;
	matRotZ = ::Matrix_MakeRotationZ(toRadians(180.f));
	matRotY = ::Matrix_MakeRotationY(toRadians(0.f));

	matRotX = ::Matrix_MakeRotationX(toRadians(0.f));
	m4x4 matTrans;
	matTrans = ::Matrix_MakeTranslation(-14.f, 12.f, 19.5f);
	m4x4 matWorld;
	matWorld = ::Matrix_MakeIdentity();
	matWorld = ::Matrix_MultiplyMatrix(matRotZ, matRotX);
	matWorld = ::Matrix_MultiplyMatrix(matWorld, matRotY);
	matWorld = ::Matrix_MultiplyMatrix(matWorld, matTrans);


	v3d target = { 0,0,1 }; // Vector_Add(camera, lookDir)
    matCameraRot = ::Matrix_MakeRotationY(camYaw);
	lookDir = ::Matrix_MultVector(matCameraRot, target);
	target = ::Vector_Add(camera, lookDir);

	m4x4 matCamera = ::Matrix_PointAt(camera, target, up);
	m4x4 matView = ::Matrix_QuickInverse(matCamera);


	trisToRaster.clear();
	rasterByMeshTri.clear();
	rasterByMeshTri.resize(meshCube.tris.size());
	rasterValidByMeshTri.clear();
	rasterValidByMeshTri.resize(meshCube.tris.size(), false);
	normalByMeshTri.clear();
	normalByMeshTri.resize(meshCube.tris.size());

	// Transform, cull, light, project
	for (size_t triIndex = 0; triIndex < meshCube.tris.size(); ++triIndex)
	{
		triangle& tri = meshCube.tris[triIndex];

		triangle triProjected, triTransformed, triViewed;

		triTransformed.p[0] = ::Matrix_MultVector(matWorld, tri.p[0]);
		triTransformed.p[1] = ::Matrix_MultVector(matWorld, tri.p[1]);
		triTransformed.p[2] = ::Matrix_MultVector(matWorld, tri.p[2]);

		// Calculate normal
		v3d normal, line1, line2;
		line1 = ::Vector_Sub(triTransformed.p[1], triTransformed.p[0]);
		line2 = ::Vector_Sub(triTransformed.p[2], triTransformed.p[0]);
		normal = ::Vector_CrossProduct(line1, line2);
		normal = ::Vector_Normalize(normal);
		normalByMeshTri[triIndex] = normal;

		// Backface culling
		v3d cameraRay = ::Vector_Sub(triTransformed.p[0], camera);
		if (::Vector_DotProduct(normal, cameraRay) < 0.f)
		{
			// Directional light pointing toward the cube from the camera side
			v3d light_direction = { 0.f, 1.f, -1.f };
			light_direction = ::Vector_Normalize(light_direction);
			float dp = max<float>(0.1f, ::Vector_DotProduct(light_direction, normal));
			dp = std::clamp(dp, 0.f, 1.f);

			// Convert World Space --> View Space
			triViewed.p[0] = Matrix_MultVector(matView, triTransformed.p[0]);
			triViewed.p[1] = Matrix_MultVector(matView, triTransformed.p[1]);
			triViewed.p[2] = Matrix_MultVector(matView, triTransformed.p[2]);
			
			// CLip viewed triangle near plane, this could form two additional triangles
			int clippedTriangles = 0;
			triangle clipped[2];
			clippedTriangles = ::Triangle_ClipAgainstPlane({ 0.f, 0.f, 0.1f }, { 0.f,0.f,1.f }, triViewed, clipped[0], clipped[1]);
		
			for (int n = 0; n < clippedTriangles; n++)
			{


				// Project
				triProjected.p[0] = ::Matrix_MultVector(matProj, clipped[n].p[0]);
				triProjected.p[1] = ::Matrix_MultVector(matProj, clipped[n].p[1]);
				triProjected.p[2] = ::Matrix_MultVector(matProj, clipped[n].p[2]);
				triProjected.p[0] = ::Vector_Div(triProjected.p[0], triProjected.p[0].w);
				triProjected.p[1] = ::Vector_Div(triProjected.p[1], triProjected.p[1].w);
				triProjected.p[2] = ::Vector_Div(triProjected.p[2], triProjected.p[2].w);

				//// X/Y are inverted now so put them back
				//triProjected.p[0].x *= -1.f;
				//triProjected.p[1].x *= -1.f;
				//triProjected.p[2].x *= -1.f;
				//triProjected.p[0].y *= -1.f;
				//triProjected.p[1].y *= -1.f;
				//triProjected.p[2].y *= -1.f;

				// offset view into visible normalized space
				v3d offsetView = { 1, 1, 0 };
				triProjected.p[0] = ::Vector_Add(triProjected.p[0], offsetView);
				triProjected.p[1] = ::Vector_Add(triProjected.p[1], offsetView);
				triProjected.p[2] = ::Vector_Add(triProjected.p[2], offsetView);
				// Scale into screen space
				triProjected.p[0].x *= 0.5f * width;
				triProjected.p[0].y *= 0.5f * height;
				triProjected.p[1].x *= 0.5f * width;
				triProjected.p[1].y *= 0.5f * height;
				triProjected.p[2].x *= 0.5f * width;
				triProjected.p[2].y *= 0.5f * height;

				// used for pixel perfect depth and lighting without GPU
				TriangleToRaster rasterTri;
				rasterTri.p[0] = triProjected.p[0];
				rasterTri.p[1] = triProjected.p[1];
				rasterTri.p[2] = triProjected.p[2];
				rasterTri.lum = dp;
				rasterTri.meshTriIndex = triIndex;
				rasterByMeshTri[triIndex] = rasterTri;
				rasterValidByMeshTri[triIndex] = true;
				trisToRaster.push_back(rasterTri);

			}
		}
	}



	return true;
}

void render(sf::RenderWindow& wnd)
{
	::ClearSoftwareFrame();

	std::vector<TriangleToRaster> clippedToRaster;
	clippedToRaster.reserve(trisToRaster.size());

	for (const TriangleToRaster& srcTri : trisToRaster)
	{
		std::deque<TriangleToRaster> listTriangles;
		listTriangles.push_back(srcTri);

		for (int p = 0; p < 4; ++p)
		{
			int trianglesToProcess = static_cast<int>(listTriangles.size());

			while (trianglesToProcess > 0)
			{
				TriangleToRaster test = listTriangles.front();
				listTriangles.pop_front();
				trianglesToProcess--;

				TriangleToRaster clipped[2];
				int trisToAdd = 0;

				switch (p)
				{
				case 0:
					// Top plane: y >= 0
					trisToAdd = ::ClipRasterTriangleAgainstPlane(
						{ 0.f, 0.f, 0.f },
						{ 0.f, 1.f, 0.f },
						test,
						clipped[0],
						clipped[1]
					);
					break;

				case 1:
					// Bottom plane: y <= height - 1
					trisToAdd = ::ClipRasterTriangleAgainstPlane(
						{ 0.f, height - 1.f, 0.f },
						{ 0.f, -1.f, 0.f },
						test,
						clipped[0],
						clipped[1]
					);
					break;

				case 2:
					// Left plane: x >= 0
					trisToAdd = ::ClipRasterTriangleAgainstPlane(
						{ 0.f, 0.f, 0.f },
						{ 1.f, 0.f, 0.f },
						test,
						clipped[0],
						clipped[1]
					);
					break;

				case 3:
					// Right plane: x <= width - 1
					trisToAdd = ::ClipRasterTriangleAgainstPlane(
						{ width - 1.f, 0.f, 0.f },
						{ -1.f, 0.f, 0.f },
						test,
						clipped[0],
						clipped[1]
					);
					break;
				}

				for (int i = 0; i < trisToAdd; ++i)
				{
					listTriangles.push_back(clipped[i]);
				}
			}
		}

		for (const TriangleToRaster& clippedTri : listTriangles)
		{
			clippedToRaster.push_back(clippedTri);
		}
	}

	for (const TriangleToRaster& tri : clippedToRaster)
	{
		::RasterizeTriangleDepth(tri);
	}

	// ::RasterizeVisibleMeshOutlineDepth();

	frameTexture.update(framePixels.data());
	sf::Sprite frameSprite(frameTexture);

	wnd.clear();
	wnd.draw(frameSprite);
	wnd.display();
}
//////////////////////////////
// MATH
/////////////////////////////
namespace {

	v3d Matrix_MultVector(const m4x4 & m, const v3d & i)
	{
		v3d v;
		v.x = i.x * m.m[0][0] + i.y * m.m[1][0] + i.z * m.m[2][0] + i.w * m.m[3][0];
		v.y = i.x * m.m[0][1] + i.y * m.m[1][1] + i.z * m.m[2][1] + i.w * m.m[3][1];
		v.z = i.x * m.m[0][2] + i.y * m.m[1][2] + i.z * m.m[2][2] + i.w * m.m[3][2];
		v.w = i.x * m.m[0][3] + i.y * m.m[1][3] + i.z * m.m[2][3] + i.w * m.m[3][3];
		return v;
	}

	float Dot(const v3d& a, const v3d& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	m4x4 Matrix_MakeIdentity()
	{
		m4x4 matrix;
		matrix.m[0][0] = 1.f;
		matrix.m[1][1] = 1.f;
		matrix.m[2][2] = 1.f;
		matrix.m[3][3] = 1.f;
		return matrix;
	}

	m4x4 Matrix_MakeRotationX(float angleRad)
	{
		m4x4 matrix;
		matrix.m[0][0] = 1.f;
		matrix.m[1][1] = cosf(angleRad);
		matrix.m[1][2] = -sinf(angleRad);
		matrix.m[2][1] = sinf(angleRad);
		matrix.m[2][2] = cosf(angleRad);
		matrix.m[3][3] = 1.f;
		return matrix;
	}

	m4x4 Matrix_MakeRotationY(float angleRad)
	{
		m4x4 matrix;
		matrix.m[0][0] = cosf(angleRad);
		matrix.m[0][2] = sinf(angleRad);
		matrix.m[2][0] = -sinf(angleRad);
		matrix.m[1][1] = 1.f;
		matrix.m[2][2] = cosf(angleRad);
		matrix.m[3][3] = 1.f;
		return matrix;
	}

	m4x4 Matrix_MakeRotationZ(float angleRad)
	{
		m4x4 matrix;
		matrix.m[0][0] = cosf(angleRad);
		matrix.m[0][1] = -sinf(angleRad);
		matrix.m[1][0] = sinf(angleRad);
		matrix.m[1][1] = cosf(angleRad);
		matrix.m[2][2] = 1.f;
		matrix.m[3][3] = 1.f;
		return matrix;
	}

	m4x4 Matrix_MakeTranslation(float x, float y, float z)
	{
		m4x4 matrix;
		matrix.m[0][0] = 1.f;
		matrix.m[1][1] = 1.f;
		matrix.m[2][2] = 1.f;
		matrix.m[3][3] = 1.f;
		matrix.m[3][0] = x;
		matrix.m[3][1] = y;
		matrix.m[3][2] = z;
		return matrix;
	}

	m4x4 Matrix_MakeProjection(float fovDegrees, float aspectRatio, float zNear, float zFar)
	{
		float fovRad = 1.f / tanf(fovDegrees * 0.5f / 180.f * 3.14159f);
		m4x4 matrix;
		matrix.m[0][0] = aspectRatio * fovRad;
		matrix.m[1][1] = fovRad;
		matrix.m[2][2] = zFar / (zFar - zNear);
		matrix.m[3][2] = (-zFar * zNear) / (zFar - zNear);
		matrix.m[2][3] = 1.f;
		matrix.m[3][3] = 0.f;
		return matrix;
	}

	m4x4 Matrix_MultiplyMatrix(const m4x4 & m1, const m4x4 & m2)
	{
		m4x4 matrix;
		for (int c = 0; c < 4; c++)
		{
			for (int r = 0; r < 4; r++)
			{
				matrix.m[r][c] = m1.m[r][0] * m2.m[0][c] + m1.m[r][1] * m2.m[1][c] + m1.m[r][2] * m2.m[2][c] + m1.m[r][3] * m2.m[3][c];
			}
		}
		return matrix;
	}

	m4x4 Matrix_PointAt(const v3d& pos, const v3d& target, const v3d& up)
	{
		// Calculate the new forward direction
		v3d newForward = ::Vector_Sub(target, pos);
		newForward = ::Vector_Normalize(newForward);

		// Calculate new Up direction
		v3d a = ::Vector_Mul(newForward, ::Vector_DotProduct(up, newForward));
		v3d newUp = ::Vector_Sub(up, a);
		newUp = ::Vector_Normalize(newUp);

		// New Right is easy, cross product
		v3d newRight = ::Vector_CrossProduct(newUp, newForward);

		// Construct Dimensioning and Translation Matrix
		m4x4 matrix;
		matrix.m[0][0] = newRight.x;		matrix.m[0][1] = newRight.y;		matrix.m[0][2] = newRight.z;
		matrix.m[1][0] = newUp.x;		matrix.m[1][1] = newUp.y;		matrix.m[1][2] = newUp.z;
		matrix.m[2][0] = newForward.x;		matrix.m[2][1] = newForward.y;		matrix.m[2][2] = newForward.z;
		matrix.m[3][0] = pos.x;		matrix.m[3][1] = pos.y;		matrix.m[3][2] = pos.z;

		return matrix;
	}

	m4x4 Matrix_QuickInverse(const m4x4& m)
	{
	// Construct Dimensioning and Translation Matrix
	m4x4 matrix;
	matrix.m[0][0] = m.m[0][0]; matrix.m[0][1] = m.m[1][0]; matrix.m[0][2] = m.m[2][0];
	matrix.m[1][0] = m.m[0][1]; matrix.m[1][1] = m.m[1][1]; matrix.m[1][2] = m.m[2][1];
	matrix.m[2][0] = m.m[0][2]; matrix.m[2][1] = m.m[1][2]; matrix.m[2][2] = m.m[2][2];
	matrix.m[3][0] = -(m.m[3][0] * matrix.m[0][0] + m.m[3][1] * matrix.m[1][0] + m.m[3][2] * matrix.m[2][0]);
	matrix.m[3][1] = -(m.m[3][0] * matrix.m[0][1] + m.m[3][1] * matrix.m[1][1] + m.m[3][2] * matrix.m[2][1]);
	matrix.m[3][2] = -(m.m[3][0] * matrix.m[0][2] + m.m[3][1] * matrix.m[1][2] + m.m[3][2] * matrix.m[2][2]);
	matrix.m[3][3] = 1.0f;
	return matrix;
	}

	v3d Vector_IntersectPlane(const v3d& plane_p, v3d& plane_n, const v3d& lineStart, const v3d& lineEnd)
	{
		plane_n = ::Vector_Normalize(plane_n);
		float plane_d = -::Vector_DotProduct(plane_n, plane_p);
		float ad = ::Vector_DotProduct(lineStart, plane_n);
		float bd = ::Vector_DotProduct(lineEnd, plane_n);
		float t = (-plane_d - ad) / (bd - ad);
		v3d lineStartToEnd = ::Vector_Sub(lineEnd, lineStart);
		v3d lineToIntersect = ::Vector_Mul(lineStartToEnd, t);
		return ::Vector_Add(lineStart, lineToIntersect);
	}

	int ClipRasterTriangleAgainstPlane(
		const v3d& plane_p,
		v3d plane_n,
		const TriangleToRaster& in_tri,
		TriangleToRaster& out_tri1,
		TriangleToRaster& out_tri2)
	{
		triangle rawIn{};
		rawIn.p[0] = in_tri.p[0];
		rawIn.p[1] = in_tri.p[1];
		rawIn.p[2] = in_tri.p[2];

		triangle rawOut1{};
		triangle rawOut2{};

		int count = ::Triangle_ClipAgainstPlane(
			plane_p,
			plane_n,
			rawIn,
			rawOut1,
			rawOut2
		);

		auto copyBack = [&](const triangle& raw, TriangleToRaster& out)
			{
				// Copy metadata first: lum, meshTriIndex, future color fields, etc.
				out = in_tri;

				// Then replace only geometry.
				out.p[0] = raw.p[0];
				out.p[1] = raw.p[1];
				out.p[2] = raw.p[2];
			};

		if (count >= 1) { copyBack(rawOut1, out_tri1); }
		if (count >= 2) { copyBack(rawOut2, out_tri2); }

		return count;
	}


	/// <summary>
	/// returns how many triangles are in the test after clipping across a plane 
	/// </summary>
	/// <param name="plane_p"></param>
	/// <param name="plane_n"></param>
	/// <param name="in_tri"></param>
	/// <param name="out_tri1"></param>
	/// <param name="out_tri2"></param>
	/// <returns></returns>
	int Triangle_ClipAgainstPlane(const v3d& plane_p, v3d plane_n, const triangle& in_tri, triangle& out_tri1, triangle& out_tri2)
	{
		// make sure plane normal is indeed normal
		plane_n = ::Vector_Normalize(plane_n);

		// Return signed shortest distance from point to plane, plane normal must be normal
		auto dist = [&](const v3d& p)
			{
				v3d n = ::Vector_Normalize(p);
				return (plane_n.x * p.x + plane_n.y * p.y + plane_n.z * p.z - ::Vector_DotProduct(plane_n, plane_p));
			};

		// Create two temp storage arrays to classify points either side of plane 
		// if distance sign is positive, point lies on "inside" of plane
		v3d const* inside_points[3]; int insidePointCount = 0;
		v3d const* outside_points[3]; int outsidePointCount = 0;

		// Get signed distance of each point in triangle to plane
		float d0 = dist(in_tri.p[0]);
		float d1 = dist(in_tri.p[1]);
		float d2 = dist(in_tri.p[2]);

		if (d0 >= 0) { inside_points[insidePointCount++] = &in_tri.p[0]; }
		else { outside_points[outsidePointCount++] = &in_tri.p[0]; }
		if (d1 >= 0) { inside_points[insidePointCount++] = &in_tri.p[1]; }
		else { outside_points[outsidePointCount++] = &in_tri.p[1]; }
		if (d2 >= 0) { inside_points[insidePointCount++] = &in_tri.p[2]; }
		else { outside_points[outsidePointCount++] = &in_tri.p[2]; }

		// now classify triabgle points, and break the input triangle into
		// smaller output triangles if required.  There are four possible
		//  outcomes..

		if (insidePointCount == 0)
		{
			// all points lie on the outside of plane, so clip whole triangle
			// it ceases to exist

			return 0; // no returned tirangles are valid
		}

		if (insidePointCount == 3)
		{
			// all points lie on the inside of plane, so do nothing
			// and allow the triangle to simply pass through

			out_tri1 = in_tri;

			return 1;
		}

		if (insidePointCount == 1 && outsidePointCount == 2)
		{
			// triangle should be clipped.  As two points lie outside
			// the plane, the triangle simple becomes a smaller triangle

			// copy appearance info to new triangle
			out_tri1 = in_tri;

			// the inside point is valid, so keep that
			out_tri1.p[0] = *inside_points[0];

			//but the two new points are at the locations where the
			// original sides of the triangle (lines) intersect with the plane
			out_tri1.p[1] = ::Vector_IntersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[0]);
			out_tri1.p[2] = ::Vector_IntersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[1]);


			return 1; // no returned tirangles are valid
		}

		if (insidePointCount == 2 && outsidePointCount == 1)
		{
			// triangle should be clipped.  As two points lie outside
			// the plane, the triangle simple becomes a smaller triangle

			// copy appearance info to new triangle
			out_tri1 = in_tri;
			out_tri2 = in_tri;

			// the inside point is valid, so keep that
			out_tri1.p[0] = *inside_points[0];
			out_tri1.p[1] = *inside_points[1];
			out_tri1.p[2] = ::Vector_IntersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[0]);

			out_tri2.p[0] = *inside_points[1];
			out_tri2.p[1] = out_tri1.p[2];
			out_tri2.p[2] = ::Vector_IntersectPlane(plane_p, plane_n, *inside_points[1], *outside_points[0]);

			return 2; // no returned tirangles are valid
		}

	}

	v3d Vector_Add(const v3d & v1, const v3d & v2)
	{
		return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
	}

	v3d Vector_Sub(const v3d & v1, const v3d & v2)
	{
		return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
	}

	v3d Vector_Mul(const v3d & v1, float k)
	{
		return { v1.x * k, v1.y * k, v1.z * k };
	}

	v3d Vector_Div(const v3d & v1, float k)
	{
		return { v1.x / k, v1.y / k, v1.z / k };
	}

	float Vector_DotProduct(const v3d & v1, const v3d & v2)
	{
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	float Vector_Length(const v3d & v)
	{
		return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	v3d Vector_Normalize(const v3d & v)
	{
		float l = ::Vector_Length(v);
		return { v.x / l, v.y / l, v.z / l };
	}

	v3d Vector_CrossProduct(const v3d & v1, const v3d & v2)
	{
		v3d v;
		v.x = v1.y * v2.z - v1.z * v2.y;
		v.y = v1.z * v2.x - v1.x * v2.z;
		v.z = v1.x * v2.y - v1.y * v2.x;
		return v;
	}
}

//////////////////////////
// RASTER FUNCTIONS
/////////////////////////
namespace {
	void ClearSoftwareFrame()
	{
		std::fill(depthBuffer.begin(), depthBuffer.end(), std::numeric_limits<float>::infinity());

		for (size_t i = 0; i < framePixels.size(); i += 4)
		{
			framePixels[i + 0] = 0;   // R
			framePixels[i + 1] = 0;   // G
			framePixels[i + 2] = 0;   // B
			framePixels[i + 3] = 255; // A
		}
	}


	void PutPixelDepth(int x, int y, float z, std::uint8_t r, std::uint8_t g, std::uint8_t b)
	{
		if (x < 0 || x >= static_cast<int>(SCREEN_W) ||
			y < 0 || y >= static_cast<int>(SCREEN_H))
		{
			return;
		}
		size_t pixelIndex = static_cast<size_t>(y) * SCREEN_W + static_cast<size_t>(x);

		// Smaller projected z is closer in your projection setup.
		if (z < depthBuffer[pixelIndex])
		{
			depthBuffer[pixelIndex] = z;

			size_t colorIndex = pixelIndex * 4;
			framePixels[colorIndex + 0] = r;
			framePixels[colorIndex + 1] = g;
			framePixels[colorIndex + 2] = b;
			framePixels[colorIndex + 3] = 255;
		}
	}


	void PutPixelLineIfVisible(int x, int y, float z)
	{
		if (x < 0 || x >= static_cast<int>(SCREEN_W) || y < 0 || y >= static_cast<int>(SCREEN_H)) { return; }
		size_t pixelIndex = static_cast<size_t>(y) * SCREEN_W + static_cast<size_t>(x);

		// Edges are drawn after triangles.
		// We allow a small epsilon because the line and triangle rasterizers
		// will not hit perfectly identical depth values every pixel.
		constexpr float EDGE_DEPTH_EPSILON = 0.0001f;

		if (z <= depthBuffer[pixelIndex] + EDGE_DEPTH_EPSILON)
		{
			size_t colorIndex = pixelIndex * 4;
			framePixels[colorIndex + 0] = 255;
			framePixels[colorIndex + 1] = 255;
			framePixels[colorIndex + 2] = 255;
			framePixels[colorIndex + 3] = 255;
		}
	}


	void RasterizeTriangleDepth(const TriangleToRaster& tri)
	{
		const v3d& p0 = tri.p[0];
		const v3d& p1 = tri.p[1];
		const v3d& p2 = tri.p[2];
		float minXf = std::min({ p0.x, p1.x, p2.x });
		float maxXf = std::max({ p0.x, p1.x, p2.x });
		float minYf = std::min({ p0.y, p1.y, p2.y });
		float maxYf = std::max({ p0.y, p1.y, p2.y });
		int minX = std::max(0, static_cast<int>(std::floor(minXf)));
		int maxX = std::min(static_cast<int>(SCREEN_W) - 1, static_cast<int>(std::ceil(maxXf)));
		int minY = std::max(0, static_cast<int>(std::floor(minYf)));
		int maxY = std::min(static_cast<int>(SCREEN_H) - 1, static_cast<int>(std::ceil(maxYf)));
		float area = ::EdgeFunction(p0, p1, p2.x, p2.y);
		if (std::fabs(area) < 0.000001f)
		{
			return;
		}

		float lum = std::clamp(tri.lum, 0.f, 1.f);
		std::uint8_t blue = static_cast<std::uint8_t>(255.f * lum);

		for (int y = minY; y <= maxY; ++y)
			for (int x = minX; x <= maxX; ++x)
			{
				float px = static_cast<float>(x) + 0.5f;
				float py = static_cast<float>(y) + 0.5f;

				float w0 = ::EdgeFunction(p1, p2, px, py) / area;
				float w1 = ::EdgeFunction(p2, p0, px, py) / area;
				float w2 = ::EdgeFunction(p0, p1, px, py) / area;

				if (w0 >= 0.f && w1 >= 0.f && w2 >= 0.f)
				{
					float z =
						w0 * p0.z +
						w1 * p1.z +
						w2 * p2.z;

					::PutPixelDepth(x, y, z, 0, blue, 0);
				}
			}
	}

	void RasterizeLineDepth(const v3d& a, const v3d& b)
	{
		float dx = b.x - a.x;
		float dy = b.y - a.y;
		float dz = b.z - a.z;

		float stepsF = std::max(std::fabs(dx), std::fabs(dy));
		if (stepsF < 1.f)
		{
			::PutPixelLineIfVisible(
				static_cast<int>(std::round(a.x)),
				static_cast<int>(std::round(a.y)),
				a.z
			);
			return;
		}

		int steps = static_cast<int>(stepsF);
		for (int i = 0; i <= steps; ++i)
		{
			float t = static_cast<float>(i) / static_cast<float>(steps);
			float x = a.x + dx * t;
			float y = a.y + dy * t;
			float z = a.z + dz * t;

			::PutPixelLineIfVisible(static_cast<int>(std::round(x)), static_cast<int>(std::round(y)), z);
		}
	}

	void RasterizeVisibleMeshOutlineDepth()
	{
		// Higher value = fewer edges.
		// 0.98 means only draw edges where faces differ by about 11 degrees or more.
		// For blocky models, try 0.95 or 0.90 if you want more chunky hard edges.
	    //constexpr float CREASE_DOT_THRESHOLD = 01.0000015f;
		constexpr float CREASE_DOT_THRESHOLD = 0.98f;

		//constexpr float CREASE_DOT_THRESHOLD = -100.01f;

		for (const MeshEdge& edge : meshEdges)
		{
			bool aVisible = edge.triA != INVALID_TRI_INDEX && rasterValidByMeshTri[edge.triA];
			bool bVisible = edge.triB != INVALID_TRI_INDEX && rasterValidByMeshTri[edge.triB];

			if (!aVisible && !bVisible) { continue; }

			bool boundaryEdge = edge.triB == INVALID_TRI_INDEX;
			bool silhouetteEdge = aVisible != bVisible;
			bool hardCreaseEdge = false;
			if (aVisible && bVisible)
			{
				float normalDot = ::Dot(normalByMeshTri[edge.triA], normalByMeshTri[edge.triB]);
				hardCreaseEdge = normalDot < CREASE_DOT_THRESHOLD;
			}

			if (!boundaryEdge && !silhouetteEdge && !hardCreaseEdge) { continue; }

			size_t drawTriIndex = aVisible ? edge.triA : edge.triB;
			int drawEdgeIndex = aVisible ? edge.edgeA : edge.edgeB;
			const TriangleToRaster& tri = rasterByMeshTri[drawTriIndex];
			const v3d& p0 = tri.p[drawEdgeIndex];
			const v3d& p1 = tri.p[(drawEdgeIndex + 1) % 3];

			::RasterizeLineDepth(p0, p1);
		}
	}
}

////////////////////////
// EDGE FUNCTIONS
////////////////////////
namespace {
	float EdgeFunction(const v3d& a, const v3d& b, float x, float y)
	{
		return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
	}

	std::uint64_t MakeEdgeKey(int a, int b)
	{
		if (a > b) { std::swap(a, b); }
		return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32) | static_cast<std::uint32_t>(b);
	}

	void BuildMeshEdges(const mesh& m)
	{
		meshEdges.clear();
		std::unordered_map<std::uint64_t, size_t> edgeMap;
		for (size_t triIndex = 0; triIndex < m.tris.size(); ++triIndex)
		{
			const triangle& tri = m.tris[triIndex];
			for (int edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
			{
				int i0 = tri.vi[edgeIndex];
				int i1 = tri.vi[(edgeIndex + 1) % 3];

				std::uint64_t key = MakeEdgeKey(i0, i1);
				auto found = edgeMap.find(key);
				if (found == edgeMap.end())
				{
					MeshEdge edge;
					edge.v0 = std::min(i0, i1);
					edge.v1 = std::max(i0, i1);
					edge.triA = triIndex;
					edge.edgeA = edgeIndex;
					meshEdges.push_back(edge);
					edgeMap[key] = meshEdges.size() - 1;
				}
				else
				{
					MeshEdge& edge = meshEdges[found->second];
					if (edge.triB == INVALID_TRI_INDEX)
					{
						edge.triB = triIndex;
						edge.edgeB = edgeIndex;
					}
				}
			}
		}
	}
}
