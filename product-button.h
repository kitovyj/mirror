#pragma once

#include <thread>
#include <sstream>

#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/image_view_factory.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/algorithm/string.hpp>

#include "button.h"
#include "http-get.h"

struct product_button_t : public button_t {

	std::thread thread;

	boost::gil::rgb8_image_t image;
	boost::gil::bgra8_image_t scaled_image;

	volatile bool image_loaded;

	std::string url;

	product_button_t(float x_pos_f, float y_pos_f, float width_f, float size_ratio, std::function<void()> on_click, const std::string& url)
		: button_t(x_pos_f, y_pos_f, width_f, size_ratio, on_click, "")
	{

		this->url = url;

		image_loaded = false;

		thread = std::thread(&product_button_t::get_picture, this);

	}

	~product_button_t()
	{
		if (thread.joinable())
			thread.join();
	}

	void get_picture()
	{
		try
		{

			std::string data = http_get(url);

			if (data.empty()) return;

			// sanity check
			if (data[0] == '<') return;

			std::stringstream ss;
			ss << data;

			using namespace boost::gil;

			if (boost::ends_with(url, "png"))
			{
				image_read_settings<png_tag> readSettings;
				read_image(ss, image, readSettings);
			}
			else if (boost::ends_with(url, "jpg"))
			{
				image_read_settings<jpeg_tag> readSettings;
				read_image(ss, image, readSettings);
			}
			else {
				return;
			}


			scaled_image = boost::gil::bgra8_image_t(width, height);


			resize_view(const_view(image), view(scaled_image), bilinear_sampler());

			image_loaded = true;

		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << std::endl;
		}
	}

	virtual void draw(renderer_base& rb)
	{
		button_t::draw(rb);

		if (image_loaded)
		{

			auto view = boost::gil::const_view(scaled_image);

			int bytes_per_pixel = 4;

			auto rp = rb.ren().row_ptr(0);

			auto screen_view = boost::gil::interleaved_view(rb.ren().width(), rb.ren().height(), reinterpret_cast<boost::gil::bgra8_pixel_t*>(rp), rb.ren().width() * bytes_per_pixel);

			boost::gil::copy_pixels(boost::gil::view(scaled_image), boost::gil::subimage_view(screen_view, this->x_pos, this->y_pos, scaled_image.width(), scaled_image.height()));

		}

	}


};