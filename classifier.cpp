#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/image_view_factory.hpp>

#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>

#include "classifier.h"

template <typename T>
std::vector<T> as_vector(boost::property_tree::ptree const& pt, boost::property_tree::ptree::key_type const& key)
{
	std::vector<T> r;
	for (auto& item : pt.get_child(key))
		r.push_back(item.second.get_value<T>());
	return r;
}


items_detection_t classify_image(unsigned char* pdata, int width, int height)
{

	// https://www.kaylyn.ink/journal/using-boost-gil-to-convert-image-data/

	std::stringstream out_buffer(std::ios_base::out | std::ios_base::binary);

	const int bytes_per_pixels = 4;

	auto view = boost::gil::interleaved_view(width, height, reinterpret_cast<const boost::gil::bgra8c_pixel_t*>(pdata), width * bytes_per_pixels);
	
	boost::gil::write_view(out_buffer, view, boost::gil::png_tag());

	std::string str = out_buffer.str();
	std::vector<char> data(str.begin(), str.end());

	using boost::asio::ip::tcp;

	boost::asio::io_service io_service;
	tcp::resolver resolver(io_service);
	tcp::resolver::query query("classifire.upyourstyle.ru", "http");
	tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
	tcp::resolver::iterator end;

	tcp::socket socket(io_service);
	boost::system::error_code error = boost::asio::error::host_not_found;
	while (error && endpoint_iterator != end)
	{
		socket.close();
		socket.connect(*endpoint_iterator++, error);
	}
	if (error)
		throw boost::system::system_error(error);

	std::string PREFIX = "--";
	//Use GUID as boundary
	std::string BOUNDARY = boost::uuids::to_string(boost::uuids::random_generator()());
	std::string NEWLINE = "\r\n";
	int NEWLINE_LENGTH = NEWLINE.length();

	//Calculate length of entire HTTP request - goes into header
	long long lengthOfRequest = 0;
	lengthOfRequest += PREFIX.length() + BOUNDARY.length() + NEWLINE_LENGTH;
	lengthOfRequest += std::string("Content-Disposition: form-data; name=\"file\"; filename=\"upyourstyle.png\"").length();
	lengthOfRequest += NEWLINE_LENGTH;
	lengthOfRequest += std::string("Content-Type: image/png").length();
	lengthOfRequest += NEWLINE_LENGTH + NEWLINE_LENGTH;
	lengthOfRequest += data.size();
	lengthOfRequest += NEWLINE_LENGTH + PREFIX.length() + BOUNDARY.length() + PREFIX.length() + NEWLINE_LENGTH;

	boost::asio::streambuf request;
	std::ostream request_stream(&request);

	request_stream << "POST /proxy/evaluate HTTP/1.1" << NEWLINE;
	request_stream << "Host: classifire.upyourstyle.ru" << NEWLINE; // << ":" << port << NEWLINE;
	request_stream << "User-Agent: Mirror" << NEWLINE;
	request_stream << "Accept: application/json" << NEWLINE;
	request_stream << "Cache-Control: no-cache" << NEWLINE;
	request_stream << "Connection: close" << NEWLINE;
	request_stream << "Content-Length: " << lengthOfRequest << NEWLINE;
	request_stream << "Content-Type: multipart/form-data; boundary=" << BOUNDARY << NEWLINE;

	request_stream << "Origin: http://upyourstyle.ru" << NEWLINE;
	request_stream << "Referer: http://upyourstyle.ru/" << NEWLINE;

	request_stream << NEWLINE;

	request_stream << PREFIX;
	request_stream << BOUNDARY;
	request_stream << NEWLINE;
	request_stream << "Content-Disposition: form-data; name=\"file\"; filename=\"upyourstyle.png\"";
	request_stream << NEWLINE;
	request_stream << "Content-Type: image/png";

	request_stream << NEWLINE;
	request_stream << NEWLINE;

	auto r_data = request.data();
	socket.write_some(buffer(r_data));

	//Send Data (Paytload)
	auto bytesSent = 0;
	while (bytesSent < data.size())
	{
		int bytesToSendNow = std::min(data.size() - bytesSent, unsigned(1024 * 100));
		socket.write_some(boost::asio::buffer(&data[0] + bytesSent, bytesToSendNow));
		bytesSent += bytesToSendNow;
	}

	//Close request
	socket.write_some(boost::asio::buffer(NEWLINE));
	socket.write_some(boost::asio::buffer(PREFIX));
	socket.write_some(boost::asio::buffer(BOUNDARY));
	socket.write_some(boost::asio::buffer(PREFIX));
	socket.write_some(boost::asio::buffer(NEWLINE));


	/*
	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	request_stream << "POST " << "/proxy/evaluate" << " HTTP/1.0\r\n";
	request_stream << "Host: classifire.upyourstyle.ru \r\n";
	*/
	//request_stream << "Accept: */*\r\n";
	/*
	request_stream << "Content-type: multipart/form-data\r\n";
	request_stream << "Connection: close\r\n\r\n";
	request_stream << "datei=";
	request_stream.write(&data.front(), data.size());

	// https://stackoverflow.com/questions/38514601/synchronous-https-post-with-boost-asio



	boost::asio::write(socket, request, boost::asio::transfer_all(), error);
	*/

	// Read the response status line. The response streambuf will automatically
	// grow to accommodate the entire line. The growth may be limited by passing
	// a maximum size to the streambuf constructor.
	boost::asio::streambuf response;
	boost::asio::read_until(socket, response, "\r\n");

	// Check that response is OK.
	std::istream response_stream(&response);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		throw std::runtime_error("Invalid response");
	}
	if (status_code != 200)
	{
		std::stringstream ss;
		ss << "Response returned with status code " << status_code;
		throw std::runtime_error(ss.str());
	}

	// Read the response headers, which are terminated by a blank line.
	boost::asio::read_until(socket, response, "\r\n\r\n");

	// Process the response headers.
	std::string header;
	while (std::getline(response_stream, header) && header != "\r")
		std::cout << header << "\n";
	std::cout << "\n";

	std::stringstream ss;

	// Write whatever content we already have to output.
	if (response.size() > 0)
		ss << &response;

	// Read until EOF, writing data to output as we go.
	//boost::system::error_code error;
	while (boost::asio::read(socket, response,
		boost::asio::transfer_at_least(1), error))
		ss << &response;
	if (error != boost::asio::error::eof)
		throw boost::system::system_error(error);

	boost::property_tree::ptree pt;
	boost::property_tree::read_json(ss, pt);


	int top_index = pt.get<int>("top_index", 0);
	int bottom_index = pt.get<int>("bottom_index", 0);

	std::vector<std::string> top_types = { "dress", "blouse", "shirt", "top", "hoodie", "sweater", "coat", "bikini" };
	std::vector<std::string> bottom_types = { "skirt", "bikini", "coat", "dress", "jeans", "pants", "shorts", "sport%20pants" };
	std::vector<std::string> colors = { "black", "blue", "brown", "gray", "green", "light-blue", "red", "rose", "white", "yellow" };

	std::vector<float> top_colors = as_vector<float>(pt, "top_colors");
	std::vector<float> bottom_colors = as_vector<float>(pt, "bottom_colors");


	std::cout << ss.str();

	items_detection_t id;

	id.top_type = top_types[top_index];
	id.bottom_type = bottom_types[bottom_index];

	int top_color_index = std::max_element(top_colors.begin(), top_colors.end()) - top_colors.begin();
	int bottom_color_index = std::max_element(bottom_colors.begin(), bottom_colors.end()) - bottom_colors.begin();

	bool same_type = id.top_type == id.bottom_type;

	if (same_type)
	{

		int color_index = bottom_color_index;
		// whichever is more confident
		if (bottom_colors[bottom_color_index] < top_colors[top_color_index]) {
			color_index = top_color_index;
		}

		bottom_color_index = color_index;
		top_color_index = color_index;
	}

	id.top_color = colors[top_color_index];
	id.bottom_color = colors[bottom_color_index];

	return id;

}


