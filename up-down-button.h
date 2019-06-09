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

struct up_down_button_t : public button_t {

	bool down;

	up_down_button_t(float x_pos_f, float y_pos_f, float width_f, float size_ratio, std::function<void()> on_click, bool down)
		: button_t(x_pos_f, y_pos_f, width_f, size_ratio, on_click, "")
	{
		this->down = down;
	}

	~up_down_button_t()
	{
	}

	virtual void draw(renderer_base& rb)
	{

		button_t::draw(rb);

		agg::rasterizer_scanline_aa<> ras;
		agg::scanline_p8 sl;
		agg::path_storage path;

		double space_x = 0.2 * width;
		double space_y = 0.25 * height;

		double arrow_width = width - 2 * space_x;
		double arrow_height = height - 2 * space_y;

		int x1, y1, x2, y2, x3, y3;

		x1 = x_pos + space_x;
		y1 = y_pos + height - space_y;

		x2 = x1 + arrow_width / 2;
		y2 = y_pos + space_y;

		x3 = x1 + arrow_width;
		y3 = y1;

		if (down)
		{
			auto t = y1;
			y1 = y3 = y2;
			y2 = t;
		}

		path.move_to(x1, y1);
		path.line_to(x2, y2);
		path.line_to(x3, y3);

		agg::conv_stroke<agg::path_storage> stroke(path);
		stroke.line_join(agg::round_join);
		stroke.line_cap(agg::round_cap);
		//stroke.miter_limit(m_miter_limit.value());
		stroke.width(20.0);
		ras.add_path(stroke);

		agg::render_scanlines_aa_solid(ras, sl, rb, agg::rgba8(255, 255, 255, 255));


	}


};