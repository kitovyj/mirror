#pragma once

#include <string>
#include <vector>

#include "classifier.h"

struct recommendations_t
{
	struct item_t {

		std::string picture_url;
		std::string description;
		std::string price;

	};

	std::vector<item_t> items;

	int total;

};

recommendations_t get_recommendations(const items_detection_t&, int page, int per_page);