#include <SFML/Graphics.hpp>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdint>


using namespace std;

struct v3d
{
	float x, y, z;
};

struct triangle
{
	v3d p[3];

	wchar_t sym;
	short col;
};

struct mesh
{
	vector<triangle> tris;
};

struct m4x4
{
	float m[4][4] = { 0 };
};

struct TriangleToRaster
{
	v3d p[3];
	float lum = 0.f;
	float avgDepth = 0.f;
	size_t meshTriIndex = 0;
};


mesh meshCube;
m4x4 matProj;

v3d camera;

float width, height;
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




int main()
{

	meshCube.tris = {
		// South
		{0.f,0.f,0.f, 0.f,1.f,0.f, 1.f,1.f,0.f},
		{0.f,0.f,0.f, 1.f,1.f,0.f, 1.f,0.f,0.f},

		// East
		{1.f,0.f,0.f,  1.f,1.f,0.f,  1.f,1.f,1.f},
		{1.f,0.f,0.f,  1.f,1.f,1.f,  1.f,0.f,1.f},

		// North
		{1.f,0.f,1.f,  1.f,1.f,1.f,  0.f,1.f,1.f},
		{1.f,0.f,1.f,  0.f,1.f,1.f,  0.f,0.f,1.f},


		// West
		{0.f,0.f,1.f,  0.f,1.f,1.f,  0.f,1.f,0.f},
		{0.f,0.f,1.f,  0.f,1.f,0.f,  0.f,0.f,0.f},

		// Top
		{0.f,1.f,0.f,  0.f,1.f,1.f,  1.f,1.f,1.f},
		{0.f,1.f,0.f,  1.f,1.f,1.f,  1.f,1.f,0.f},

		// Bottom
		{0.f,0.f,1.f,  0.f,0.f,0.f,  1.f,0.f,0.f},
		{0.f,0.f,1.f,  1.f,0.f,0.f,  1.f,0.f,1.f},
	};
	width = 1600.f;
	height = 900.f;
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

	trisToRaster.clear();

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

			rasterTri.avgDepth =
				(triTranslated.p[0].z +
					triTranslated.p[1].z +
					triTranslated.p[2].z) / 3.f;

			trisToRaster.push_back(rasterTri);
		}
	}

	// Painter's sort: draw farther triangles first
	std::sort(
		trisToRaster.begin(),
		trisToRaster.end(),
		[](const TriangleToRaster& a, const TriangleToRaster& b)
		{
			return a.avgDepth > b.avgDepth;
		}
	);

	return true;
}

void render(sf::RenderWindow& wnd)
{
	wnd.clear();

	// Draw
	for (const TriangleToRaster& tri : trisToRaster)
	{
		sf::Vector2f p0(tri.p[0].x, tri.p[0].y);
		sf::Vector2f p1(tri.p[1].x, tri.p[1].y);
		sf::Vector2f p2(tri.p[2].x, tri.p[2].y);

		sf::ConvexShape shape{};
		shape.setPointCount(3);
		shape.setPoint(0, p0);
		shape.setPoint(1, p1);
		shape.setPoint(2, p2);

		shape.setFillColor(BlueFadeToBlack(tri.lum));

		wnd.draw(shape);

		DrawOuterFaceEdges(wnd, p0, p1, p2, tri.meshTriIndex);
	}

	// display the frame
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
		static_cast<std::uint8_t>(0),
		static_cast<std::uint8_t>(0),
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

	if (firstTriangleOfFace)
	{
		DrawEdge(wnd, p0, p1);
		DrawEdge(wnd, p1, p2);
	}
	else
	{
		DrawEdge(wnd, p1, p2);
		DrawEdge(wnd, p2, p0);
	}
}