#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>

#include "recommendation.h"
#include "http-get.h"

recommendations_t get_recommendations(const items_detection_t& detection, int page)
{

	namespace beast = boost::beast;     // from <boost/beast.hpp>
	namespace http = beast::http;       // from <boost/beast/http.hpp>
	namespace net = boost::asio;        // from <boost/asio.hpp>
	using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

	std::string target = "service.upyourstyle.ru";
	target += "/proxy/get_items?";

	target += "face_warm_cold=n%2Fa&face_contrast=n%2Fa&items%5B%5D=bags&items%5B%5D=shoes&items%5B%5D=belts&items%5B%5D=hats&items%5B%5D=shawls&items%5B%5D=jackets&items%5B%5D=sweaters&items%5B%5D=blouses&items%5B%5D=pants&items%5B%5D=skirts&items%5B%5D=dresses&items%5B%5D=tops";
	target += "&top_type=" + detection.top_type;
	target += "&bottom_type=" + detection.bottom_type;
	target += "&top_color=" + detection.top_color;
	target += "&bottom_color=" + detection.bottom_color;
//	target += "&face_warm_cold=n/a";
//	target += "&face_contrast=n/a";
	target += "&page=" + std::to_string(page);
	target += "&per_page=" + std::to_string(6);
	target += "&store=" + std::to_string(1);
	target += "&rules=" + std::to_string(1);
	target += "&upload_id=" + std::to_string(0);

	std::string s = http_get(target);

	std::stringstream ss;
	ss << s;

	boost::property_tree::ptree pt;
	boost::property_tree::read_json(ss, pt);

	recommendations_t r;

	r.total = pt.get<int>("count", 0);

	for (auto const &item_node : pt.get_child("result"))
	{
		recommendations_t::item_t item;
		
		item.picture_url = item_node.second.get<std::string>("picture");
		item.description = item_node.second.get<std::string>("name");
		item.price = item_node.second.get<std::string>("price");

		r.items.push_back(item);

	}



	return r;

}