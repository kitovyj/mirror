#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>

#include "recommendation.h"

recommendations_t get_recommendations(const items_detection_t& detection, int page)
{

	namespace beast = boost::beast;     // from <boost/beast.hpp>
	namespace http = beast::http;       // from <boost/beast/http.hpp>
	namespace net = boost::asio;        // from <boost/asio.hpp>
	using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

	std::string host = "service.upyourstyle.ru";
	std::string port = "80";
	std::string target = "/proxy/get_items?";


	target += "items[]=bags&items[]=shoes&items[]=belts&items[]=hats&items[]=shawls&items[]=jackets&items[]=sweaters&items[]=blouses&items[]=pants&items[]=skirts&items[]=dresses&items[]=tops&store=1&rules=1&upload_id=0";
	target += "&top_type=" + detection.top_type;
	target += "&bottom_type=" + detection.bottom_type;
	target += "&top_color=" + detection.top_color;
	target += "&bottom_color=" + detection.bottom_color;
	target += "&face_warm_cold=n/a";
	target += "&face_contrast=n/a";
	target += "&page=" + std::to_string(page);
	target += "&per_page=" + std::to_string(6);

	int version = 11;

	// The io_context is required for all I/O
	net::io_context ioc;

	// These objects perform our I/O
	tcp::resolver resolver(ioc);
	beast::tcp_stream stream(ioc);
	// Look up the domain name
	auto const results = resolver.resolve(host, port);

	// Make the connection on the IP address we get from a lookup
	stream.connect(results);

	// Set up an HTTP GET request message
	http::request<http::string_body> req { http::verb::get, target, version };
	req.set(http::field::host, host);
	req.set(http::field::user_agent, "UpYourStyle");

	// Send the HTTP request to the remote host
	http::write(stream, req);

	// This buffer is used for reading and must be persisted
	beast::flat_buffer buffer;

	// Declare a container to hold the response
	http::response<http::dynamic_body> res;

	// Receive the HTTP response
	http::read(stream, buffer, res);


	std::stringstream ss;


	// Write the message to standard out
	ss << boost::beast::buffers_to_string(res.body().data());

	std::string s = ss.str();

	// Gracefully close the socket
	beast::error_code ec;
	stream.socket().shutdown(tcp::socket::shutdown_both, ec);

	// not_connected happens sometimes
	// so don't bother reporting it.
	//
	if (ec && ec != beast::errc::not_connected)
		throw beast::system_error{ ec };

}