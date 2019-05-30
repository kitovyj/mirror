#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>

#include <iostream>
#include <vector>
#include <algorithm>

void classify_image()
{

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

	std::vector<char> data(10);

	std::string PREFIX = "--";
	//Use GUID as boundary
	std::string BOUNDARY = boost::uuids::to_string(boost::uuids::random_generator()());
	std::string NEWLINE = "\r\n";
	int NEWLINE_LENGTH = NEWLINE.length();

	//Calculate length of entire HTTP request - goes into header
	long long lengthOfRequest = 0;
	lengthOfRequest += PREFIX.length() + BOUNDARY.length() + NEWLINE_LENGTH;
	lengthOfRequest += std::string("Content-Disposition: form-data; name=\"file\"; filename=\"test.zip\"").length();
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
	request_stream << "Content-Disposition: form-data; name=\"file\"; filename=\"test.zip\"";
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
		std::cout << "Invalid response\n";
		return;
	}
	if (status_code != 200)
	{
		std::cout << "Response returned with status code " << status_code << "\n";
		return;
	}

	// Read the response headers, which are terminated by a blank line.
	boost::asio::read_until(socket, response, "\r\n\r\n");

	// Process the response headers.
	std::string header;
	while (std::getline(response_stream, header) && header != "\r")
		std::cout << header << "\n";
	std::cout << "\n";

	// Write whatever content we already have to output.
	if (response.size() > 0)
		std::cout << &response;

	// Read until EOF, writing data to output as we go.
	//boost::system::error_code error;
	while (boost::asio::read(socket, response,
		boost::asio::transfer_at_least(1), error))
		std::cout << &response;
	if (error != boost::asio::error::eof)
		throw boost::system::system_error(error);
}


