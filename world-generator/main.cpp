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
const size_t GRID_CELL_SIZE = 318;
const size_t OPTIMAL_THREAD_NUM = 128;

void generateMap(float* map, size_t width, size_t height, size_t octaves, float persistence, float lacunarity);

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

void perlin_process_fraction(float* map, size_t map_width, sf::Vector2f* grid, size_t size, size_t off_x, size_t off_y, size_t grid_w, size_t width = 0, size_t height = 0) {
	if (width == 0 && height == 0) { 
		width = height = size;
	}
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

			float sx = (float)j / size;
			float sy = (float)i / size;

			float n0 = interpolate(top_left_dp, top_right_dp, sx);
			float n1 = interpolate(bottom_left_dp, bottom_right_dp, sx);
			float value = interpolate(n0, n1, sy);
			map[(off_y * size + i) * map_width + (off_x * size + j)] = value;
		}
	}
}

void perlin_process_blocks(float* map, size_t map_width, size_t map_height, sf::Vector2f* grid, size_t size, size_t off, size_t n) {
	size_t grid_cell_w = 1 + (map_width - 1) / size;
	size_t grid_w = grid_cell_w + 1;
	for (size_t i = 0; i < n; i++)
	{
		size_t block_offset_x = (off + i) % grid_cell_w;
		size_t block_offset_y = (off + i) / grid_cell_w;
		size_t width = std::min(size, map_width - (block_offset_x) * size);
		size_t height = std::min(size, map_height - (block_offset_y) * size);
		perlin_process_fraction(map, map_width, grid, size, block_offset_x, block_offset_y, grid_w, width, height);
	}
}

void perlinNoise(float* map, size_t width, size_t height, size_t grid_cell_size) {
	size_t grid_cell_w = 1 + (width - 1) / grid_cell_size;
	size_t grid_cell_h = 1 + (height - 1) / grid_cell_size;
	size_t grid_w = grid_cell_w + 1;
	size_t grid_h = grid_cell_h + 1;

	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_real_distribution<double> dist(-M_PI, M_PI);
	sf::Vector2f* grid = new sf::Vector2f[grid_w * grid_h];
	std::for_each(grid, grid + grid_w * grid_h, [&dist, &rng](sf::Vector2f& v) {
		float angle = dist(rng);
		v.x = cos(angle);
		v.y = sin(angle);
		});

	size_t blocks_n = grid_cell_h * grid_cell_w;
	size_t block_per_thread = blocks_n / OPTIMAL_THREAD_NUM;
	size_t last_thread_blocks = blocks_n - block_per_thread * OPTIMAL_THREAD_NUM;
	size_t threads_n = block_per_thread == 0 ? 0 : OPTIMAL_THREAD_NUM;

	std::vector<std::thread> threads(threads_n);
	for (size_t i = 0; i < threads_n; i++)
	{
		size_t off = i * block_per_thread;
		threads[i] = std::thread(perlin_process_blocks, map, width, height, grid, grid_cell_size, off, block_per_thread);
	}
	if (last_thread_blocks != 0) {
		size_t off = blocks_n - last_thread_blocks;
		perlin_process_blocks(map, width, height, grid, grid_cell_size, off, last_thread_blocks);
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

	char windowTitle[] = "World Designer";
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

	const size_t map_width = WINDOW_WIDTH;
	const size_t map_height = WINDOW_HEIGHT;
	
	sf::Texture mapTex;
	mapTex.create(WINDOW_WIDTH, WINDOW_HEIGHT);
	sf::Sprite s(mapTex);
	float scale_factor = std::min(WINDOW_WIDTH / map_width, WINDOW_HEIGHT / map_height);
	s.setScale(scale_factor, scale_factor);

	int octaves = 1;
	float persistance = 0.5f;
	float lacunarity = 2.0f;

	sf::Clock deltaClock;
	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			ImGui::SFML::ProcessEvent(event);

			if (event.type == event.Closed) {
				window.close();
			}
		}
		ImGui::SFML::Update(window, deltaClock.restart());
		ImGui::Begin("Sample window");
		if (ImGui::InputInt("Ocatves", &octaves)) {
			generateMap(map, map_width, map_height, octaves, persistance, lacunarity);
			mapToPixels(map, map_width, map_height, pixels);
			mapTex.update(pixels);
		}
		if (ImGui::SliderFloat("Persistance", &persistance, 0.f, 1.f)) {
			generateMap(map, map_width, map_height, octaves, persistance, lacunarity);
				mapToPixels(map, map_width, map_height, pixels);
				mapTex.update(pixels);
		}
		if (ImGui::SliderFloat("Lacunarity", &lacunarity, 1.f, 4.f)) {
			generateMap(map, map_width, map_height, octaves, persistance, lacunarity);
				mapToPixels(map, map_width, map_height, pixels);
				mapTex.update(pixels);
		}
		if(ImGui::Button("Generate")) {
			generateMap(map, map_width, map_height, octaves, persistance, lacunarity);
			mapToPixels(map, map_width, map_height, pixels);
			mapTex.update(pixels);
		}
		ImGui::End(); 

		window.clear(sf::Color::White);

		window.draw(s);
		
		ImGui::SFML::Render(window); 
		
		window.display();
	}
	return 0;
}

void generateMap(float* map, size_t width, size_t height, size_t octaves, float persistence, float lacunarity)
{
	float frequency = 2;
	float amplitude = 0.5;
	float* temp_map = new float[width * height];
	memset(map, 0, sizeof(float) * width * height);

	for (size_t i = 0; i < octaves; i++) {
		frequency *= lacunarity;
		amplitude *= persistence;
		perlinNoise(temp_map, width, height, std::max(1.f, width / frequency));
		for (size_t j = 0; j < width*height; j++)
		{
			map[j] += temp_map[j] * amplitude;
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
