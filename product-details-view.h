#pragma once

#include <thread>

#include <boost/gil.hpp>

#include <boost/algorithm/string.hpp>

#include "recommendation.h"

#include "button.h"
#include "http-get.h"

struct product_details_view_t : public button_base_t {

	const recommendations_t::item_t& item;

	std::thread thread;

	boost::gil::rgba8_image_t image;
	boost::gil::rgb8_image_t image_rgb;
	bool use_rgb_image;
	boost::gil::bgra8_image_t scaled_image;
	boost::gil::bgra8c_view_t scaled_image_view;

	volatile bool image_loaded;

	int image_x, image_y, image_width, image_height;
	
	product_details_view_t(float x_pos_f, float y_pos_f, float width_f, float size_ratio, std::function<void()> on_click, const recommendations_t::item_t& item)
		: button_base_t(x_pos_f, y_pos_f, width_f, size_ratio, on_click, "", 10),
		  item(item)
	{

		image_loaded = false;
		use_rgb_image = false;

		render();

		thread = std::thread(&product_details_view_t::get_picture, this);

	}

	virtual void render_content(prim_ren_base_type& rb, bool hl) {

		std::string text = item.description;

		if (text.empty())
			text = "No description";

		int last_space = -1;
		int max_chars = 23;

		for (int i = 0;i < text.size();i++)
		{
			if (text[i] == ' ')
				last_space = i;

			if (i > max_chars - 1)
			{
				if (last_space == -1)
					text = text.substr(0, max_chars);
				else
					text = text.substr(0, last_space);
				break;
			}

		}

		typedef prim_ren_base_type ren_base;
		typedef agg::renderer_scanline_aa_solid<ren_base> renderer_solid;
		typedef agg::renderer_scanline_bin_solid<ren_base> renderer_bin;


		auto color = agg::rgba8(255, 255, 255, 255);

		int rr_space = 20;
		int rr_x = dx;
		int rr_width = width;
		int rr_height = 100;
		int rr_y = dx + height - rr_height;

		agg::rounded_rect r(rr_x, rr_y, rr_x + rr_width, rr_y + rr_height, 10);
		r.radius(0, 0, 10, 10);
		r.normalize_radius();

		int text_height = 34;

		std::pair<int, int> price_text_size;

		{

			agg::rasterizer_scanline_aa<> ras;
			agg::scanline_u8 sl;
			renderer_solid ren(rb);
			renderer_bin ren_bin(rb);

			price_text_size = text_size(ras, sl, ren, ren_bin, item.price.c_str(), text_height);

		}

		float space = width * 0.05 + 10;

		int x = dx + space;
		int y = rr_y + (rr_height - price_text_size.second) / 2;

		int price_x = dx + width - space - price_text_size.first;
		int price_y = y;

		{

			agg::rasterizer_scanline_aa<> ras;
			agg::scanline_u8 sl;
			renderer_solid ren(rb);
			renderer_bin ren_bin(rb);

			ras.add_path(r);
			agg::render_scanlines_aa_solid(ras, sl, rb, agg::rgba(0.0, 0.0, 0.0, 0.4));

			ras.reset();

			draw_text(ras, sl, ren, ren_bin, x, y, color, text.c_str(), text_height);
			draw_text(ras, sl, ren, ren_bin, price_x, price_y, color, item.price.c_str(), text_height);
		}

	}

	~product_details_view_t()
	{
		if (thread.joinable())
			thread.join();
	}

	void get_picture()
	{
		try
		{

			std::string data = http_get(item.picture_url);

			if (data.empty()) return;

			// sanity check
			if (data[0] == '<') return;

			std::stringstream ss;
			ss << data;

			using namespace boost::gil;

			if (boost::ends_with(item.picture_url, "png"))
			{
				image_read_settings<png_tag> settings;
				read_image(ss, image, settings);
			}
			else if (boost::ends_with(item.picture_url, "jpg"))
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
			float bottom_space = 0.2;
			image_x = width * space;
			image_y = height * space;
			image_width = width - 2 * image_x;
			image_height = height - image_y -  bottom_space * width;

			scaled_image = boost::gil::bgra8_image_t(image_width, image_height);

			if (use_rgb_image)
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
		button_base_t::draw(canvas);

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
