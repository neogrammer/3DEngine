#include <SFML/Graphics.hpp>
#include <vector>
using namespace std;
struct v3d
{
	float x, y, z;
};

struct triangle
{
	v3d p[3];
};

struct mesh
{
	vector<triangle> tris;
};

struct m4x4
{
	float m[4][4] = { 0 };
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

bool update(float dt_);
void MultMatVec(v3d& i, v3d& o, m4x4& m);

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
			wnd.clear();

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

			// for each tiangle to be rendered in the model
			for (auto tri : meshCube.tris)
			{
				// decides outer lines for cube outline
				static bool even{ false };
				even = !even;

				// transforms
				triangle triProjected, triTranslated, triRotatedZ, triRotatedZX;


				// Rotate Z
				MultMatVec(tri.p[0], triRotatedZ.p[0], matRotZ);
				MultMatVec(tri.p[1], triRotatedZ.p[1], matRotZ);
				MultMatVec(tri.p[2], triRotatedZ.p[2], matRotZ);


				// Rotate X
				MultMatVec(triRotatedZ.p[0], triRotatedZX.p[0], matRotX);
				MultMatVec(triRotatedZ.p[1], triRotatedZX.p[1], matRotX);
				MultMatVec(triRotatedZ.p[2], triRotatedZX.p[2], matRotX);

				// translate
				triTranslated = triRotatedZX;
				triTranslated.p[0].z = triRotatedZX.p[0].z + 3.f;
				triTranslated.p[1].z = triRotatedZX.p[1].z + 3.f;
				triTranslated.p[2].z = triRotatedZX.p[2].z + 3.f;

				// setup for dot product between camera and normal
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

				float l = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
				normal.x /= l;
				normal.y /= l;
				normal.z /= l;


			   
				//  If we can see the normal
				if(normal.x * (triTranslated.p[0].x - camera.x) +
				   normal.y * (triTranslated.p[0].y - camera.y) +
				   normal.z * (triTranslated.p[0].z - camera.z) < 0.f)
				{
					// project final vertice transform
					MultMatVec(triTranslated.p[0], triProjected.p[0], matProj);
					MultMatVec(triTranslated.p[1], triProjected.p[1], matProj);
					MultMatVec(triTranslated.p[2], triProjected.p[2], matProj);

					// Scale into screen space
					triProjected.p[0].x += 1.f;	triProjected.p[0].y += 1.f;
					triProjected.p[1].x += 1.f;	triProjected.p[1].y += 1.f;
					triProjected.p[2].x += 1.f;	triProjected.p[2].y += 1.f;
					triProjected.p[0].x *= 0.5f * (float)width;
					triProjected.p[0].y *= 0.5f * (float)height;
					triProjected.p[1].x *= 0.5f * (float)width;
					triProjected.p[1].y *= 0.5f * (float)height;
					triProjected.p[2].x *= 0.5f * (float)width;
					triProjected.p[2].y *= 0.5f * (float)height;

					// Define three points, on three lines for the triangle being rendered
					sf::Vector2f point1A(triProjected.p[0].x, triProjected.p[0].y);
					sf::Vector2f point1B(triProjected.p[1].x, triProjected.p[1].y);
					sf::Vector2f point2A(triProjected.p[1].x, triProjected.p[1].y);
					sf::Vector2f point2B(triProjected.p[2].x, triProjected.p[2].y);
					sf::Vector2f point3A(triProjected.p[2].x, triProjected.p[2].y);
					sf::Vector2f point3B(triProjected.p[0].x, triProjected.p[0].y);

					// colored model
					sf::ConvexShape shape{};
					shape.setPointCount(3);
					shape.setPoint(0, point1A);
					shape.setPoint(1, point2A);
					shape.setPoint(2, point3A);
					shape.setFillColor(sf::Color::Blue);

					// draw the shape
					wnd.draw(shape);
				
					// draw the shape outline
					if (even)
					{
						// Create a VertexArray with 2 vertices
						outline[0][0].position = point1A;
						outline[0][0].color = sf::Color::White;
						outline[0][1].position = point1B;
						outline[0][1].color = sf::Color::White;
						wnd.draw(outline[0]);
					}

					outline[1][0].position = point2A;
					outline[1][0].color = sf::Color::White;
					outline[1][1].position = point2B;
					outline[1][1].color = sf::Color::White;

					wnd.draw(outline[1]);

					if (!even)
					{
						outline[2][0].position = point3A;
						outline[2][0].color = sf::Color::White;
						outline[2][1].position = point3B;
						outline[2][1].color = sf::Color::White;
						wnd.draw(outline[2]);

					}
				}
			}
			wnd.display();
		}
	}


	return 0;
}

bool update(float dt_)
{
	return true;
}

void MultMatVec(v3d& i, v3d& o, m4x4& m)
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
