#include <curl/curl.h>
#include <curl/easy.h>

#include <sstream>
#include <string>

struct http_downloader_t {
	
	http_downloader_t();
	~http_downloader_t();

	std::string download(const std::string& url);

	void* curl;

};

std::size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
	std::string data((const char*)ptr, (size_t)size * nmemb);
	*((std::stringstream*)stream) << data;
	return size * nmemb;
}

http_downloader_t::http_downloader_t() {
	curl = curl_easy_init();
}

http_downloader_t::~http_downloader_t() {
	curl_easy_cleanup(curl);
}

std::string http_downloader_t::download(const std::string& url) {

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	/* example.com is redirected, so we tell libcurl to follow redirection */
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); //Prevent "longjmp causes uninitialized stack frame" bug
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "deflate");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	std::stringstream out;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
	/* Perform the request, res will get the return code */
	CURLcode res = curl_easy_perform(curl);
	/* Check for errors */
	if (res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
			curl_easy_strerror(res));
		return "";
	}
	return out.str();

}

std::string http_get(const std::string& url)
{
	http_downloader_t d;
	return d.download(url);
}