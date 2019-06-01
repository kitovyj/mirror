#pragma once

#include <string>

struct items_detection_t
{
	std::string top_type, bottom_type, top_color, bottom_color;

};

items_detection_t classify_image(unsigned char* data, int width, int height);