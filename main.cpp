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


using namespace std;

struct v3d
{
	float x, y, z;
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

			if (line[0] == 'v')
			{
				v3d v;
				s >> junk >> v.x >> v.y >> v.z;
				verts.push_back(v);
			}
			if (line[0] == 'f')
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

struct m4x4
{
	float m[4][4] = { 0 };
};

struct TriangleToRaster
{
	v3d p[3];
	float lum = 0.f;
	//float avgDepth = 0.f;
	size_t meshTriIndex = 0;
};
constexpr size_t INVALID_TRI_INDEX = std::numeric_limits<size_t>::max();

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

mesh meshCube;
m4x4 matProj;

v3d camera;

constexpr unsigned SCREEN_W = 1600;
constexpr unsigned SCREEN_H = 900;

constexpr float width = 1600.f;
constexpr float height = 900.f;

std::vector<std::uint8_t> framePixels(SCREEN_W* SCREEN_H * 4);
std::vector<float> depthBuffer(SCREEN_W* SCREEN_H);

sf::Texture frameTexture(sf::Vector2u{ SCREEN_W, SCREEN_H });

float aspect;
float fov;
float fovRad;
float zFar;
float zNear;
float theta;
std::vector<TriangleToRaster> trisToRaster;


bool update(float dt_);
void render(sf::RenderWindow& wnd);
void MultMatVec(const v3d& i, v3d& o, const m4x4& m);

sf::Color BlueFadeToBlack(float lum);
void DrawEdge(sf::RenderWindow& wnd, const sf::Vector2f& a, const sf::Vector2f& b);
void DrawOuterFaceEdges(
	sf::RenderWindow& wnd,
	const sf::Vector2f& p0,
	const sf::Vector2f& p1,
	const sf::Vector2f& p2,
	size_t meshTriIndex
);
float EdgeFunction(const v3d& a, const v3d& b, float x, float y);
void ClearSoftwareFrame();
void PutPixelDepth(int x, int y, float z, std::uint8_t r, std::uint8_t g, std::uint8_t b);
void RasterizeTriangleDepth(const TriangleToRaster& tri);
void PutPixelLineIfVisible(int x, int y, float z);
void RasterizeLineDepth(const v3d& a, const v3d& b);
void RasterizeTriangleEdgesDepth(const TriangleToRaster& tri);
std::uint64_t MakeEdgeKey(int a, int b);
void BuildMeshEdges(const mesh& m);
float Dot(const v3d& a, const v3d& b);
void RasterizeVisibleMeshOutlineDepth();


int main()
{

	//meshCube.tris = {
	//	// South
	//	{0.f,0.f,0.f, 0.f,1.f,0.f, 1.f,1.f,0.f},
	//	{0.f,0.f,0.f, 1.f,1.f,0.f, 1.f,0.f,0.f},

	//	// East
	//	{1.f,0.f,0.f,  1.f,1.f,0.f,  1.f,1.f,1.f},
	//	{1.f,0.f,0.f,  1.f,1.f,1.f,  1.f,0.f,1.f},

	//	// North
	//	{1.f,0.f,1.f,  1.f,1.f,1.f,  0.f,1.f,1.f},
	//	{1.f,0.f,1.f,  0.f,1.f,1.f,  0.f,0.f,1.f},


	//	// West
	//	{0.f,0.f,1.f,  0.f,1.f,1.f,  0.f,1.f,0.f},
	//	{0.f,0.f,1.f,  0.f,1.f,0.f,  0.f,0.f,0.f},

	//	// Top
	//	{0.f,1.f,0.f,  0.f,1.f,1.f,  1.f,1.f,1.f},
	//	{0.f,1.f,0.f,  1.f,1.f,1.f,  1.f,1.f,0.f},

	//	// Bottom
	//	{0.f,0.f,1.f,  0.f,0.f,0.f,  1.f,0.f,0.f},
	//	{0.f,0.f,1.f,  1.f,0.f,0.f,  1.f,0.f,1.f},
	//};

	meshCube.LoadObjectFile("assets/models/spaceship/spaceship.obj");
	BuildMeshEdges(meshCube);

	// Projection Matrix
	zNear = 0.1f;
	zFar = 1000.f;
	fov = 90.f;
	aspect = height / width;
	fovRad = 1.f / tanf(fov * 0.5f / 180.f * 3.14159f);

	matProj.m[0][0] = aspect * fovRad;
	matProj.m[1][1] = fovRad;
	matProj.m[2][2] = zFar / (zFar - zNear);
	matProj.m[3][2] = (-zFar * zNear) / (zFar - zNear);
	matProj.m[2][3] = 1.f;
	matProj.m[3][3] = 0.f;

	sf::RenderWindow wnd{ sf::VideoMode{{(unsigned)width,(unsigned)height},32U}, "3D Engine"};


	float dt = 0.f;
	const float FPS = 1.f / 60.f;
	sf::Clock dtClock{};

	vector<sf::VertexArray> outline;
	outline.push_back(sf::VertexArray{ sf::PrimitiveType::Lines, 2 });
	outline.push_back(sf::VertexArray{ sf::PrimitiveType::Lines, 2 });
	outline.push_back(sf::VertexArray{ sf::PrimitiveType::Lines, 2 });

	while (wnd.isOpen())
	{
		while (const std::optional event = wnd.pollEvent())
		{
			if (auto result = event->getIf<sf::Event::KeyPressed>())
			{
				if (result->code == sf::Keyboard::Key::Escape)
				{
					wnd.close();
				}
			}
			if (event->is<sf::Event::Closed>())
			{
				wnd.close();
			}
		}


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
			theta += 1.f * FPS;
		
			dt -= FPS;
		}

		if (repaint)
		{
			render(wnd);
		}
	}


	return 0;
}

bool update(float dt_)
{
	m4x4 matRotZ, matRotX;

	// Z Rotation Matrix
	matRotZ.m[0][0] = cosf(theta);
	matRotZ.m[0][1] = sinf(theta);
	matRotZ.m[1][0] = -sinf(theta);
	matRotZ.m[1][1] = cosf(theta);
	matRotZ.m[2][2] = 1.f;
	matRotZ.m[3][3] = 1.f;

	// X Rotation Matrix
	matRotX.m[0][0] = 1.f;
	matRotX.m[1][1] = cosf(theta * 0.5f);
	matRotX.m[1][2] = sinf(theta * 0.5f);
	matRotX.m[2][1] = -sinf(theta * 0.5f);
	matRotX.m[2][2] = cosf(theta * 0.5f);
	matRotX.m[3][3] = 1.f;

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
		const triangle& tri = meshCube.tris[triIndex];

		triangle triProjected, triTranslated, triRotatedZ, triRotatedZX;

		// Rotate Z
		MultMatVec(tri.p[0], triRotatedZ.p[0], matRotZ);
		MultMatVec(tri.p[1], triRotatedZ.p[1], matRotZ);
		MultMatVec(tri.p[2], triRotatedZ.p[2], matRotZ);

		// Rotate X
		MultMatVec(triRotatedZ.p[0], triRotatedZX.p[0], matRotX);
		MultMatVec(triRotatedZ.p[1], triRotatedZX.p[1], matRotX);
		MultMatVec(triRotatedZ.p[2], triRotatedZX.p[2], matRotX);

		// Translate
		triTranslated = triRotatedZX;
		triTranslated.p[0].z += 3.f;
		triTranslated.p[1].z += 3.f;
		triTranslated.p[2].z += 3.f;

		// Calculate normal
		v3d normal, line1, line2;

		line1.x = triTranslated.p[1].x - triTranslated.p[0].x;
		line1.y = triTranslated.p[1].y - triTranslated.p[0].y;
		line1.z = triTranslated.p[1].z - triTranslated.p[0].z;

		line2.x = triTranslated.p[2].x - triTranslated.p[0].x;
		line2.y = triTranslated.p[2].y - triTranslated.p[0].y;
		line2.z = triTranslated.p[2].z - triTranslated.p[0].z;

		normal.x = line1.y * line2.z - line1.z * line2.y;
		normal.y = line1.z * line2.x - line1.x * line2.z;
		normal.z = line1.x * line2.y - line1.y * line2.x;

		float normalLength = sqrtf(
			normal.x * normal.x +
			normal.y * normal.y +
			normal.z * normal.z
		);



		normal.x /= normalLength;
		normal.y /= normalLength;
		normal.z /= normalLength;

		normalByMeshTri[triIndex] = normal;

		// Backface culling
		float cameraRayDot =
			normal.x * (triTranslated.p[0].x - camera.x) +
			normal.y * (triTranslated.p[0].y - camera.y) +
			normal.z * (triTranslated.p[0].z - camera.z);

		if (cameraRayDot < 0.f)
		{
			// Directional light pointing toward the cube from the camera side
			v3d light_direction = { 0.f, 0.f, -1.f };

			float lightLength = sqrtf(
				light_direction.x * light_direction.x +
				light_direction.y * light_direction.y +
				light_direction.z * light_direction.z
			);

			light_direction.x /= lightLength;
			light_direction.y /= lightLength;
			light_direction.z /= lightLength;

			float dp =
				normal.x * light_direction.x +
				normal.y * light_direction.y +
				normal.z * light_direction.z;

			dp = std::clamp(dp, 0.f, 1.f);

			// Project
			MultMatVec(triTranslated.p[0], triProjected.p[0], matProj);
			MultMatVec(triTranslated.p[1], triProjected.p[1], matProj);
			MultMatVec(triTranslated.p[2], triProjected.p[2], matProj);

			// Scale into screen space
			triProjected.p[0].x += 1.f;
			triProjected.p[0].y += 1.f;
			triProjected.p[1].x += 1.f;
			triProjected.p[1].y += 1.f;
			triProjected.p[2].x += 1.f;
			triProjected.p[2].y += 1.f;

			triProjected.p[0].x *= 0.5f * width;
			triProjected.p[0].y *= 0.5f * height;
			triProjected.p[1].x *= 0.5f * width;
			triProjected.p[1].y *= 0.5f * height;
			triProjected.p[2].x *= 0.5f * width;
			triProjected.p[2].y *= 0.5f * height;

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

	return true;
}

void render(sf::RenderWindow& wnd)
{
	ClearSoftwareFrame();

	// First pass: fill solid triangles and populate depth buffer
	for (const TriangleToRaster& tri : trisToRaster)
	{
		RasterizeTriangleDepth(tri);
	}

	// Second pass: draw only silhouette / boundary / hard crease edges
	RasterizeVisibleMeshOutlineDepth();

	frameTexture.update(framePixels.data());

	sf::Sprite frameSprite(frameTexture);

	wnd.clear();
	wnd.draw(frameSprite);
	wnd.display();

}


void MultMatVec(const v3d& i, v3d& o, const m4x4& m)
{
	o.x = i.x * m.m[0][0] + i.y * m.m[1][0] + i.z * m.m[2][0] + m.m[3][0];
	o.y = i.x * m.m[0][1] + i.y * m.m[1][1] + i.z * m.m[2][1] + m.m[3][1];
	o.z = i.x * m.m[0][2] + i.y * m.m[1][2] + i.z * m.m[2][2] + m.m[3][2];
	float w = i.x * m.m[0][3] + i.y * m.m[1][3] + i.z * m.m[2][3] + m.m[3][3];

	if (w != 0.f)
	{
		o.x /= w; o.y /= w; o.z /= w;
	}
}

sf::Color BlueFadeToBlack(float lum)
{
	lum = std::clamp(lum, 0.f, 1.f);

	std::uint8_t blue = static_cast<std::uint8_t>(255.f * lum);

	return sf::Color(
		blue,
		blue,
		blue,
		static_cast<std::uint8_t>(255)
	);
}

void DrawEdge(sf::RenderWindow& wnd, const sf::Vector2f& a, const sf::Vector2f& b)
{
	sf::VertexArray edge(sf::PrimitiveType::Lines, 2);

	edge[0].position = a;
	edge[0].color = sf::Color::Black;

	edge[1].position = b;
	edge[1].color = sf::Color::Black;

	wnd.draw(edge);
}

void DrawOuterFaceEdges(
	sf::RenderWindow& wnd,
	const sf::Vector2f& p0,
	const sf::Vector2f& p1,
	const sf::Vector2f& p2,
	size_t meshTriIndex
)
{
	// cube mesh has 2 triangles per face.
	// First triangle of face: draw p0-p1 and p1-p2.
	// Second triangle of face: draw p1-p2 and p2-p0.
	// This skips the diagonal shared inside each square face.

	bool firstTriangleOfFace = (meshTriIndex % 2) == 0;
	DrawEdge(wnd, p0, p1);
	DrawEdge(wnd, p1, p2);
	DrawEdge(wnd, p2, p0);
}


float EdgeFunction(const v3d& a, const v3d& b, float x, float y)
{
	return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}

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

	float area = EdgeFunction(p0, p1, p2.x, p2.y);

	if (std::fabs(area) < 0.000001f)
	{
		return;
	}

	float lum = std::clamp(tri.lum, 0.f, 1.f);
	std::uint8_t blue = static_cast<std::uint8_t>(255.f * lum);

	for (int y = minY; y <= maxY; ++y)
	{
		for (int x = minX; x <= maxX; ++x)
		{
			float px = static_cast<float>(x) + 0.5f;
			float py = static_cast<float>(y) + 0.5f;

			float w0 = EdgeFunction(p1, p2, px, py) / area;
			float w1 = EdgeFunction(p2, p0, px, py) / area;
			float w2 = EdgeFunction(p0, p1, px, py) / area;

			if (w0 >= 0.f && w1 >= 0.f && w2 >= 0.f)
			{
				float z =
					w0 * p0.z +
					w1 * p1.z +
					w2 * p2.z;

				PutPixelDepth(x, y, z, blue, blue, blue);
			}
		}
	}
}

void PutPixelLineIfVisible(int x, int y, float z)
{
	if (x < 0 || x >= static_cast<int>(SCREEN_W) ||
		y < 0 || y >= static_cast<int>(SCREEN_H))
	{
		return;
	}

	size_t pixelIndex = static_cast<size_t>(y) * SCREEN_W + static_cast<size_t>(x);

	// Edges are drawn after triangles.
	// We allow a small epsilon because the line and triangle rasterizers
	// will not hit perfectly identical depth values every pixel.
	constexpr float EDGE_DEPTH_EPSILON = 0.0001f;

	if (z <= depthBuffer[pixelIndex] + EDGE_DEPTH_EPSILON)
	{
		size_t colorIndex = pixelIndex * 4;

		framePixels[colorIndex + 0] = 0;
		framePixels[colorIndex + 1] = 0;
		framePixels[colorIndex + 2] = 0;
		framePixels[colorIndex + 3] = 255;
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
		PutPixelLineIfVisible(
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

		PutPixelLineIfVisible(
			static_cast<int>(std::round(x)),
			static_cast<int>(std::round(y)),
			z
		);
	}
}

void RasterizeTriangleEdgesDepth(const TriangleToRaster& tri)
{
	RasterizeLineDepth(tri.p[0], tri.p[1]);
	RasterizeLineDepth(tri.p[1], tri.p[2]);
	RasterizeLineDepth(tri.p[2], tri.p[0]);
}

std::uint64_t MakeEdgeKey(int a, int b)
{
	if (a > b)
	{
		std::swap(a, b);
	}

	return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32) |
		static_cast<std::uint32_t>(b);
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

float Dot(const v3d& a, const v3d& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

void RasterizeVisibleMeshOutlineDepth()
{
	// Higher value = fewer edges.
	// 0.98 means only draw edges where faces differ by about 11 degrees or more.
	// For blocky models, try 0.95 or 0.90 if you want more chunky hard edges.
	constexpr float CREASE_DOT_THRESHOLD = 0.0f;

	for (const MeshEdge& edge : meshEdges)
	{
		bool aVisible =
			edge.triA != INVALID_TRI_INDEX &&
			rasterValidByMeshTri[edge.triA];

		bool bVisible =
			edge.triB != INVALID_TRI_INDEX &&
			rasterValidByMeshTri[edge.triB];

		if (!aVisible && !bVisible)
		{
			continue;
		}

		bool boundaryEdge = edge.triB == INVALID_TRI_INDEX;
		bool silhouetteEdge = aVisible != bVisible;

		bool hardCreaseEdge = false;

		if (aVisible && bVisible)
		{
			float normalDot = Dot(
				normalByMeshTri[edge.triA],
				normalByMeshTri[edge.triB]
			);

			hardCreaseEdge = normalDot < CREASE_DOT_THRESHOLD;
		}

		if (!boundaryEdge && !silhouetteEdge && !hardCreaseEdge)
		{
			continue;
		}

		size_t drawTriIndex = aVisible ? edge.triA : edge.triB;
		int drawEdgeIndex = aVisible ? edge.edgeA : edge.edgeB;

		const TriangleToRaster& tri = rasterByMeshTri[drawTriIndex];

		const v3d& p0 = tri.p[drawEdgeIndex];
		const v3d& p1 = tri.p[(drawEdgeIndex + 1) % 3];

		RasterizeLineDepth(p0, p1);
	}
}
