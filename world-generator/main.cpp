#include "imgui.h"
#include "imgui-SFML.h"

#define _USE_MATH_DEFINES
#include <SFML/Graphics.hpp>
#include <iostream>
#include <random>
#include <assert.h>
#include <thread>
#include <functional>
#include <sstream>

const int WINDOW_WIDTH	= 1280;
const int WINDOW_HEIGHT = 720;
const size_t GRID_CELL_SIZE = 700;
const size_t OPTIMAL_THREAD_NUM = 128;

void generateMap(float* map, size_t width, size_t height, size_t grid_cell_size);

void mapToPixels(float* map, size_t width, size_t height, sf::Uint8* pixels, size_t p_width = WINDOW_WIDTH);

template<typename T>
inline float dotProduct(const sf::Vector2<T>& lh, const sf::Vector2<T>& rh) {
	return lh.x * rh.x + lh.y * rh.y;
}

float smoothstep(float x) {
	return (6 * x * x * x * x * x - 15 * x * x * x * x + 10 * x * x * x);
	//return (3.0 - x * 2.0) * x * x;
	//return x;
	//return ((x * (x * 6.0 - 15.0) + 10.0) * x * x * x);
}

float interpolate(float a, float b, float w) {
	if (0.0 > w) return a;
	if (1.0 < w) return b;
	return a + smoothstep(w) * (b - a);
}

template<typename T>
inline T ceil_int_div(T divided, T divisor) {
	// https://stackoverflow.com/questions/2745074/fast-ceiling-of-an-integer-division-in-c-c
	return 1 + ((divided - 1) / divisor);
}

void foo(float* map, size_t map_width, sf::Vector2f* grid, size_t size, size_t off_x, size_t off_y, size_t width = 0, size_t height = 0) {
	std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
	start = std::chrono::high_resolution_clock::now();
	if (width == 0 && height == 0) { 
		width = height = size;
	}
	size_t grid_w = map_width / size + 1;
	for (size_t i = 0; i < height; i++)
	{
		for (size_t j = 0; j < width; j++)
		{
			sf::Vector2f point(j, i);

			size_t absolute_y = (i + off_y * size) / size;
			size_t absolute_x = (j + off_x * size) / size;
			sf::Vector2f* top_left		= grid + ( absolute_y      * grid_w + absolute_x);
			sf::Vector2f* top_right		= grid + ( absolute_y      * grid_w + absolute_x + 1);
			sf::Vector2f* bottom_left	= grid + ((absolute_y + 1) * grid_w + absolute_x);
			sf::Vector2f* bottom_right	= grid + ((absolute_y + 1) * grid_w + absolute_x + 1);

			sf::Vector2f relative_point		 = point / (float)size;
			sf::Vector2f top_left_offset	 = relative_point;
			sf::Vector2f top_right_offset	 = relative_point - sf::Vector2f(1.f, 0.f);
			sf::Vector2f bottom_left_offset	 = relative_point - sf::Vector2f(0.f, 1.f);
			sf::Vector2f bottom_right_offset = relative_point - sf::Vector2f(1.f, 1.f);


			float top_left_dp	  = dotProduct(top_left_offset,		*top_left);
			float top_right_dp	  = dotProduct(top_right_offset,	*top_right);
			float bottom_left_dp  = dotProduct(bottom_left_offset,	*bottom_left);
			float bottom_right_dp = dotProduct(bottom_right_offset, *bottom_right);

			float sx = (float)j / width;
			float sy = (float)i / height;

			float n0 = interpolate(top_left_dp, top_right_dp, sx);
			float n1 = interpolate(bottom_left_dp, bottom_right_dp, sx);
			float value = interpolate(n0, n1, sy);
			map[(off_y * size + i) * map_width + (off_x * size + j)] = value;
		}
	}
	end = std::chrono::high_resolution_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	std::stringstream s;
	s << std::this_thread::get_id() << "\t.Elapsed time: " << elapsed.count() << "ms\n";
	std::cout << s.str();
}

void perlinNoise(float* map, size_t width, size_t height, size_t grid_cell_size) {
	assert(width % grid_cell_size == 0 && height % grid_cell_size == 0);
	// TODO rename to smth appropriate
	size_t grid_w = width / grid_cell_size + 1;
	size_t grid_h = height / grid_cell_size + 1;
	size_t grid_cell_w = width / grid_cell_size;
	size_t grid_cell_h = height / grid_cell_size;

	std::random_device dev;
	//std::mt19937 rng(std::mt19937::default_seed);
	std::mt19937 rng(dev());
	std::uniform_real_distribution<double> dist(-M_PI, M_PI);
	sf::Vector2f* grid = new sf::Vector2f[grid_w * grid_h];
	for (size_t i = 0; i < grid_h; i++)
	{
		for (size_t j = 0; j < grid_w; j++)
		{
			float angle = dist(rng);
			grid[i * (grid_w) + j].x = cos(angle);
			grid[i * (grid_w) + j].y = sin(angle);
		}
	}
	std::vector<std::thread> threads(grid_cell_w * grid_cell_h);
	for (size_t i = 0; i < grid_cell_h; i++)
	{
		for (size_t j = 0; j < grid_cell_w; j++)
		{
			threads[i * grid_cell_w + j] = std::thread(foo, map, width, grid, grid_cell_size, j, i, 0, 0);
		}
	}
	std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
}

size_t autoWidth(size_t max = WINDOW_WIDTH) {
	return max / GRID_CELL_SIZE * GRID_CELL_SIZE;
}

size_t autoHeight(size_t max = WINDOW_HEIGHT) {
	return max / GRID_CELL_SIZE * GRID_CELL_SIZE;
}

int main(int argc, char** argv) {
	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "wg");
	window.setVerticalSyncEnabled(true);
	window.setKeyRepeatEnabled(false);
	window.setFramerateLimit(60);

	ImGui::SFML::Init(window);

	sf::Color bgColor;
	float color[3] = { 0.f, 0.f, 0.f };

	char windowTitle[255] = "ImGui + SFML = <3";
	window.setTitle(windowTitle);

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
	
	sf::Texture mapTex;
	mapTex.create(WINDOW_WIDTH, WINDOW_HEIGHT);
	sf::Sprite s(mapTex);
	float scale_factor = std::min(WINDOW_WIDTH / map_width, WINDOW_HEIGHT / map_height);
	s.setScale(scale_factor, scale_factor);

	//generateMap(map, map_width, map_height, GRID_CELL_SIZE);
	perlinNoise(map, map_width, map_height, GRID_CELL_SIZE);
	mapToPixels(map, map_width, map_height, pixels);
	mapTex.update(pixels);
	sf::Clock deltaClock;
	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			ImGui::SFML::ProcessEvent(event);

			if (event.type == event.Closed) {
				window.close();
			}
			if (event.type == event.KeyPressed) {
				if (event.key.code == sf::Keyboard::D) {
					//generateMap(map, map_width, map_height, GRID_CELL_SIZE);
					//perlinNoise(map, map_width, map_height, GRID_CELL_SIZE);
					//mapToPixels(map, map_width, map_height, pixels);
					mapTex.update(pixels);
					
				}
				if (event.key.code == sf::Keyboard::S) {
					perlinNoise(map, map_width, map_height, GRID_CELL_SIZE);
					mapToPixels(map, map_width, map_height, pixels);
					mapTex.update(pixels);

				}
			}
		}
		ImGui::SFML::Update(window, deltaClock.restart());
		ImGui::Begin("Sample window"); // создаём окно
		if (ImGui::ColorEdit3("Background color", color)) {
			// код вызывается при изменении значения, поэтому всё
			// обновляется автоматически
			bgColor.r = static_cast<sf::Uint8>(color[0] * 255.f);
			bgColor.g = static_cast<sf::Uint8>(color[1] * 255.f);
			bgColor.b = static_cast<sf::Uint8>(color[2] * 255.f);
		}
		ImGui::InputText("Window title", windowTitle, 255);
		if (ImGui::Button("Update window title")) {
			// этот код выполняется, когда юзер жмёт на кнопку
			// здесь можно было бы написать 
			// if(ImGui::InputText(...))
			window.setTitle(windowTitle);
		}
		ImGui::End(); // end window

		window.clear(bgColor);

		window.draw(s);
		
		ImGui::SFML::Render(window); 
		
		window.display();
	}
	return 0;
}

void generateMap(float* map, size_t width, size_t height, size_t grid_cell_size)
{
	const size_t iterations = 5;
	const float impact[iterations] { 0.5, 0.25, 0.125, 0.0625, 0.03125 };
	// TODO Not actually precision, should be changed later.
	const float precision[iterations] { 1, 0.5, 0.25, 0.125, 0.0625 };
	float* temp_map = new float[width * height];
	memset(map, 0, sizeof(float) * width * height);
	for (size_t i = 0; i < iterations; i++) {
		perlinNoise(temp_map, width, height, grid_cell_size * precision[i]);
		for (size_t j = 0; j < width*height; j++)
		{
			map[j] += temp_map[j] * impact[i];
		}
	}
	delete[] temp_map;
	
}

void mapToPixels(float* map, size_t width, size_t height, sf::Uint8* pixels, size_t p_width)
{
	for (size_t i = 0; i < height; i++)
	{
		for (size_t j = 0; j < width; j++)
		{
			float color = ((map[i * width + j] + 1.f) * 0.5) * 255;
			pixels[(i * p_width + j) * 4]	  = color;
			pixels[(i * p_width + j) * 4 + 1] = color;
			pixels[(i * p_width + j) * 4 + 2] = color;
		}
	}
}
