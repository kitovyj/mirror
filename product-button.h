#pragma once

#include <thread>
#include <sstream>

#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/image_view_factory.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/gil/image.hpp>
#include <boost/gil/typedefs.hpp>
//#include <boost/gil/extension/io/jpeg_io.hpp>
#include <boost/gil.hpp>

#include <boost/algorithm/string.hpp>

#include "button.h"
#include "http-get.h"

struct product_button_t : public button_t {

	std::thread thread;

	boost::gil::rgba8_image_t image;
	boost::gil::rgb8_image_t image_rgb;
	bool use_rgb_image;
	boost::gil::bgra8_image_t scaled_image;
	boost::gil::bgra8c_view_t scaled_image_view;

	volatile bool image_loaded;

	std::string url;

	int image_x, image_y, image_width, image_height;

	product_button_t(float x_pos_f, float y_pos_f, float width_f, float size_ratio, std::function<void()> on_click, const std::string& url)
		: button_t(x_pos_f, y_pos_f, width_f, size_ratio, on_click, "")
	{

		this->url = url;

		image_loaded = false;
		use_rgb_image = false;

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
				image_read_settings<png_tag> settings;
				read_image(ss, image, settings);
			}
			else if (boost::ends_with(url, "jpg"))
			{
				image_read_settings<jpeg_tag> settings;
				read_image(ss, image_rgb, settings);
				use_rgb_image = true;

				//boost::gil::copy_pixels(boost::gil::color_converted_view<rgba8_pixel_t>(image_rgb), boost::gil::view(image));
				//boost::gil::copy_pixels(boost::gil::view(image_rgb), boost::gil::view(image));
				//image.operator=(image_rgb);
			}
			else {
				return;
			}

			float space = 0.05;
			image_x = width * space;
			image_y = height * space;
			image_width = width - 2 * image_x;
			image_height = height - 2 * image_y;

			scaled_image = boost::gil::bgra8_image_t(image_width, image_height);
			
			if(use_rgb_image)
				resize_view(const_view(image_rgb), view(scaled_image), bilinear_sampler());
			else
				resize_view(const_view(image), view(scaled_image), bilinear_sampler());

			scaled_image_view = boost::gil::const_view(scaled_image);

			image_loaded = true;

			int bytes_per_pixel = 4;
			auto rendered_view = boost::gil::interleaved_view(ren_buf.width(), ren_buf.height(), reinterpret_cast<boost::gil::bgra8_pixel_t*>(&rendered.front()), ren_buf.width() * bytes_per_pixel);
			auto rendered_view_hl = boost::gil::interleaved_view(ren_buf.width(), ren_buf.height(), reinterpret_cast<boost::gil::bgra8_pixel_t*>(&rendered_highlighted.front()), ren_buf.width() * bytes_per_pixel);

			auto renederd_si_view = boost::gil::subimage_view(rendered_view, dx + image_x, dy + image_y, scaled_image.width(), scaled_image.height());
			auto renederd_hl_si_view = boost::gil::subimage_view(rendered_view_hl, dx + image_x, dy + image_y, scaled_image.width(), scaled_image.height());

			std::lock_guard<std::mutex> guard(draw_mutex);

			boost::gil::copy_pixels(scaled_image_view, renederd_si_view);
			boost::gil::copy_pixels(scaled_image_view, renederd_hl_si_view);

			//auto rp = rb.ren().row_ptr(0);

			//auto screen_view = boost::gil::interleaved_view(rb.ren().width(), rb.ren().height(), reinterpret_cast<boost::gil::bgra8_pixel_t*>(rp), rb.ren().width() * bytes_per_pixel);

			// auto& screen_view = canvas.view;

			// check screen height

			/*
			int h = std::min(int(scaled_image.height()), int(canvas.rb.ren().height()) - this->y_pos - image_y - 1);

			if (h <= 0)
			{
				return;
			}

			if (h < scaled_image.height())
			{
				auto cut_view = boost::gil::subimage_view(scaled_image_view, 0, 0, scaled_image.width(), h);
				boost::gil::copy_pixels(cut_view, boost::gil::subimage_view(screen_view, this->x_pos + image_x, this->y_pos + image_y, scaled_image.width(), h));
			}
			else
			{
				boost::gil::copy_pixels(scaled_image_view, boost::gil::subimage_view(screen_view, this->x_pos + image_x, this->y_pos + image_y, scaled_image.width(), h));
			}*/


		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << std::endl;
		}
	}

	virtual void draw(canvas_t& canvas)
	{
		button_t::draw(canvas);
		
		/*
		if (image_loaded)
		{



			int bytes_per_pixel = 4;

			//auto rp = rb.ren().row_ptr(0);

			//auto screen_view = boost::gil::interleaved_view(rb.ren().width(), rb.ren().height(), reinterpret_cast<boost::gil::bgra8_pixel_t*>(rp), rb.ren().width() * bytes_per_pixel);

			auto& screen_view = canvas.view;

			// check screen height

			int h = std::min(int(scaled_image.height()), int(canvas.rb.ren().height()) - this->y_pos - image_y - 1);

			if (h <= 0)
			{
				return;
			}

			if (h < scaled_image.height())
			{
				auto cut_view = boost::gil::subimage_view(scaled_image_view, 0, 0, scaled_image.width(), h);
				boost::gil::copy_pixels(cut_view, boost::gil::subimage_view(screen_view, this->x_pos + image_x, this->y_pos + image_y, scaled_image.width(), h));
			}
			else
			{
				boost::gil::copy_pixels(scaled_image_view, boost::gil::subimage_view(screen_view, this->x_pos + image_x, this->y_pos + image_y, scaled_image.width(), h));
			}

		}*/

	}


};