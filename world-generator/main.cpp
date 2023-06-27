#define _USE_MATH_DEFINES
#include <SFML/Graphics.hpp>
#include <iostream>
#include <random>
#include <assert.h>

const int WINDOW_WIDTH	= 1280;
const int WINDOW_HEIGHT = 720;
const size_t GRID_CELL_SIZE = 20;

template<typename T>
inline float dotProduct(const sf::Vector2<T>& lh, const sf::Vector2<T>& rh) {
	return lh.x * rh.x + lh.y * rh.y;
}

float smoothstep(float x) {
	//return (6 * x * x * x * x * x - 15 * x * x * x * x + 10 * x * x * x);
	return (3.0 - x * 2.0) * x * x;
	//return x;
	//return ((x * (x * 6.0 - 15.0) + 10.0) * x * x * x);
}

float interpolate(float a, float b, float w) {
	if (0.0 > w) return a;
	if (1.0 < w) return b;
	return a + smoothstep(w) * (b - a);
}

void f(float* map, size_t width, size_t height, size_t grid_cell_size) {
	assert(width % grid_cell_size == 0 && height % grid_cell_size == 0);
	int grid_w = width  / grid_cell_size + 1;
	int grid_h = height / grid_cell_size + 1;

	std::random_device dev;
	//std::mt19937 rng(std::mt19937::default_seed);
	std::mt19937 rng(dev());
	std::uniform_real_distribution<double> dist(-1.f, 1.f);
	sf::Vector2f* grid = new sf::Vector2f[grid_w * grid_h];
	for (size_t i = 0; i < grid_h; i++)
	{
		for (size_t j = 0; j < grid_w; j++)
		{
			float angle = dist(rng) * M_PI;
			grid[i * (grid_w) + j].x = cos(angle);
			grid[i * (grid_w) + j].y = sin(angle);
		}
	}

	for (size_t i = 0; i < height; i++)
	{
		for (size_t j = 0; j < width; j++)
		{
			sf::Vector2f point(j, i);

			sf::Vector2f* top_left		= grid + (i / grid_cell_size * grid_w + j / grid_cell_size);
			sf::Vector2f* top_right		= grid + (i / grid_cell_size * grid_w + j / grid_cell_size + 1);
			sf::Vector2f* bottom_left	= grid + ((i / grid_cell_size + 1) * grid_w + j / grid_cell_size);
			sf::Vector2f* bottom_right	= grid + ((i / grid_cell_size + 1) * grid_w + j / grid_cell_size + 1);

			//sf::Vector2f top_left_offset	 = (point / (float)grid_cell_size - sf::Vector2f((float)j / grid_cell_size,		(float)i / grid_cell_size))		;// (float)grid_cell_size;
			//sf::Vector2f top_right_offset	 = (point / (float)grid_cell_size - sf::Vector2f((float)j / grid_cell_size + 1,	(float)i / grid_cell_size))		;// (float)grid_cell_size;
			//sf::Vector2f bottom_left_offset	 = (point / (float)grid_cell_size - sf::Vector2f((float)j / grid_cell_size,		(float)i / grid_cell_size + 1)) ;// (float)grid_cell_size;
			//sf::Vector2f bottom_right_offset = (point / (float)grid_cell_size - sf::Vector2f((float)j / grid_cell_size + 1,	(float)i / grid_cell_size + 1)) ;// (float)grid_cell_size;

			sf::Vector2f relative_point = point - sf::Vector2f(j / grid_cell_size * grid_cell_size, i / grid_cell_size * grid_cell_size);
			sf::Vector2f top_left_offset	 = relative_point / (float)grid_cell_size;
			sf::Vector2f top_right_offset	 = relative_point / (float)grid_cell_size - sf::Vector2f(1.f, 0);
			sf::Vector2f bottom_left_offset  = relative_point / (float)grid_cell_size - sf::Vector2f(0, 1.f);
			sf::Vector2f bottom_right_offset = relative_point / (float)grid_cell_size - sf::Vector2f(1.f, 1.f);


			float top_left_dp	  = dotProduct(top_left_offset, *top_left);
			float top_right_dp	  = dotProduct(top_right_offset, *top_right);
			float bottom_left_dp  = dotProduct(bottom_left_offset, *bottom_left);
			float bottom_right_dp = dotProduct(bottom_right_offset, *bottom_right);

			float sx = (float)(j % grid_cell_size) / grid_cell_size;
			float sy = (float)(i % grid_cell_size) / grid_cell_size;

			float n0 = interpolate(top_left_dp, top_right_dp, sx);
			float n1 = interpolate(bottom_left_dp, bottom_right_dp, sx);
			float value = interpolate(n0, n1, sy);
			map[i * width + j] = value;
		}
	}
}

size_t autoWidth(size_t max = WINDOW_WIDTH) {
	return max / GRID_CELL_SIZE * GRID_CELL_SIZE;
}

size_t autoHeight(size_t max = WINDOW_HEIGHT) {
	return max / GRID_CELL_SIZE * GRID_CELL_SIZE;
}

int main(int argc, char** argv) {
	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "wg");
	window.setKeyRepeatEnabled(false);

	float* map = new float[WINDOW_WIDTH * WINDOW_HEIGHT] { 0 };
	sf::Uint8* pixels = new sf::Uint8[WINDOW_WIDTH * WINDOW_HEIGHT * 4]{ 0 };
	for (size_t i = 0; i < WINDOW_HEIGHT; i++)
	{
		for (size_t j = 0; j < WINDOW_WIDTH; j++)
		{
			pixels[(i * WINDOW_WIDTH + j) * 4 + 3] = 255;
		}
	}

	const size_t map_height = autoHeight();
	const size_t map_width = autoWidth();
	//f(map, map_width, map_height, GRID_CELL_SIZE);
	
	sf::Texture mapTex;
	mapTex.create(WINDOW_WIDTH, WINDOW_HEIGHT);
	sf::Sprite s(mapTex);
	float scale_factor = std::min(WINDOW_WIDTH / map_width, WINDOW_HEIGHT / map_height);
	s.setScale(scale_factor, scale_factor);
	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == event.Closed) {
				window.close();
			}
			if (event.type == event.KeyPressed) {
				if (event.key.code == sf::Keyboard::D) {
					window.clear(sf::Color::White);
					f(map, map_width, map_height, GRID_CELL_SIZE);
					system("cls");
					for (size_t i = 0; i < map_height; i++)
					{
						sf::Uint8 color = 0.f;
						for (size_t j = 0; j < map_width; j++)
						{
							/*if (i % GRID_CELL_SIZE == 0 || j % GRID_CELL_SIZE == 0) {
								pixels[(i * WINDOW_WIDTH + j) * 4] = 255;
								pixels[(i * WINDOW_WIDTH + j) * 4 + 1] = 255;
								pixels[(i * WINDOW_WIDTH + j) * 4 + 2] = 255;
								continue;
							}*/
							color = map[i * map_width + j] * 10 ;
							pixels[(i * WINDOW_WIDTH + j) * 4]	   = color;
							pixels[(i * WINDOW_WIDTH + j) * 4 + 1] = color;
							pixels[(i * WINDOW_WIDTH + j) * 4 + 2] = color;
						}
						std::cout << color << "\n";
					}
					mapTex.update(pixels);
					window.draw(s);
					window.display();
				}
			}
		}
		
		//std::cin.get();
	}
	return 0;
}