#include <SFML/Graphics.hpp>
#include <vector>
#include <iostream>
#include <cstdint>


using namespace std;

// Define masks and shifts
constexpr uint16_t FG_MASK = 0x000F; // bits 0-3
constexpr uint16_t BG_MASK = 0x00F0; // bits 4-7
constexpr uint8_t  BG_SHIFT = 4;

// Helper macros/functions
constexpr uint16_t FG_COLOR(uint16_t fg) { return fg & FG_MASK; }
constexpr uint16_t BG_COLOR(uint16_t bg) { return (bg & FG_MASK) << BG_SHIFT; }

short BG_BLACK = 0, BG_DARK_GREY = 1, BG_GREY = 2;
short FG_BLACK = 3, FG_DARK_GREY = 4, FG_GREY = 5, FG_WHITE = 6;
wchar_t PIXEL_QUARTER = L'Q', PIXEL_HALF = L'H', PIXEL_THREEQUARTERS = L'T', PIXEL_SOLID = L'S';

struct CHAR_INFO
{
	union {
		wchar_t UnicodeChar;
		char  AsciiChar;
	} Char;
	uint16_t Attributes;
};


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
CHAR_INFO GetColor(float lum);
sf::Color ConvertColorInfo(CHAR_INFO c);
float GetLightMultiplier(wchar_t sym);
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
					v3d light_direction = { 0.f, 0.f, -1.f };
					float lightn = sqrtf(light_direction.x * light_direction.x + light_direction.y * light_direction.y + light_direction.z * light_direction.z);
					light_direction.x /= lightn;
					light_direction.y /= lightn;
					light_direction.z /= lightn;

					float dp = normal.x * light_direction.x + normal.y * light_direction.y + normal.z * light_direction.z;

					CHAR_INFO c = GetColor(dp);
					triTranslated.col = c.Attributes;
					triTranslated.sym = c.Char.UnicodeChar;

					// project final vertice transform
					MultMatVec(triTranslated.p[0], triProjected.p[0], matProj);
					MultMatVec(triTranslated.p[1], triProjected.p[1], matProj);
					MultMatVec(triTranslated.p[2], triProjected.p[2], matProj);
					triProjected.col = triTranslated.col;
					triProjected.sym = triTranslated.sym;


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
				
					auto col = ConvertColorInfo(c);
					float multiplier = GetLightMultiplier(triProjected.sym);
					col.r = (uint8_t)((float)col.r * multiplier);
					col.g = (uint8_t)((float)col.g * multiplier);
					col.b = (uint8_t)((float)col.b * multiplier);

					shape.setFillColor(col);

					// draw the shape
					wnd.draw(shape);
				
					// draw the shape outline
					//if (even)
					//{
					//	// Create a VertexArray with 2 vertices
					//	outline[0][0].position = point1A;
					//	outline[0][0].color = sf::Color::White;
					//	outline[0][1].position = point1B;
					//	outline[0][1].color = sf::Color::White;
					//	wnd.draw(outline[0]);
					//}

					//outline[1][0].position = point2A;
					//outline[1][0].color = sf::Color::White;
					//outline[1][1].position = point2B;
					//outline[1][1].color = sf::Color::White;

					//wnd.draw(outline[1]);

					//if (!even)
					//{
					//	outline[2][0].position = point3A;
					//	outline[2][0].color = sf::Color::White;
					//	outline[2][1].position = point3B;
					//	outline[2][1].color = sf::Color::White;
					//	wnd.draw(outline[2]);

					//}
				}
			}
			// display the frame
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

CHAR_INFO GetColor(float lum)
{
	short bg_col, fg_col;
	wchar_t sym;
	int pixel_bw = (int)(13.f * lum);
	switch (pixel_bw)
	{
	case 0: bg_col = BG_BLACK; fg_col = FG_BLACK; sym = PIXEL_SOLID;  break;

	case 1: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_QUARTER;  break;
	case 2: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_HALF;  break;
	case 3: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_THREEQUARTERS;  break;
	case 4: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_SOLID;  break;
	
	case 5: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_QUARTER;  break;
	case 6: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_HALF;  break;
	case 7: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_THREEQUARTERS;  break;
	case 8: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_SOLID;  break;
	
	case 9: bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_QUARTER;  break;
	case 10: bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_HALF;  break;
	case 11: bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_THREEQUARTERS;  break;
	case 12: bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_SOLID;  break;
	default:
		bg_col = BG_BLACK; fg_col = FG_BLACK; sym = PIXEL_SOLID;
	}

	CHAR_INFO c;
	c.Attributes = BG_COLOR(bg_col) | FG_COLOR(fg_col);
	c.Char.UnicodeChar = sym;
	return c;
}

sf::Color ConvertColorInfo(CHAR_INFO c)
{
	uint16_t fg = c.Attributes & FG_MASK;
	uint16_t bg = (c.Attributes & BG_MASK) >> BG_SHIFT;

	if (fg == FG_BLACK)
	{
		return sf::Color::Black;
	}
	else if (fg == FG_DARK_GREY)
	{
		return sf::Color(0ui8, 0ui8, 20ui8, 255ui8);
	}
	else if (fg == FG_GREY)
	{
		return sf::Color(0ui8, 0ui8, 127ui8, 255ui8);
	}
	else if (fg == FG_WHITE)
	{
		return sf::Color::Blue;
	}
	else
	{
		return sf::Color::Transparent;
	}
}

float GetLightMultiplier(wchar_t sym)
{
	if (sym == PIXEL_SOLID)
	{
		return 1.f;
	}
	else if (sym == PIXEL_QUARTER)
	{
		return 0.25f;
	}
	else if (sym == PIXEL_HALF)
	{
		return 0.5f;
	}
	else if (sym == PIXEL_THREEQUARTERS)
	{
		return 0.75f;
	}
	else
	{
		return 0.f;
	}
}
