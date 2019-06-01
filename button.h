#pragma once

#include <functional>

#include "agg_basics.h"
#include "agg_rendering_buffer.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_scanline_p.h"
#include "agg_renderer_scanline.h"
#include "agg_pixfmt_rgba.h"
#include "agg_ellipse.h"
#include "agg_rounded_rect.h"
#include "agg_conv_stroke.h" 
#include "agg_font_freetype.h"
#include "agg_conv_curve.h"
#include "agg_conv_contour.h"
#include "agg_bezier_arc.h"
#include "agg_font_win32_tt.h"
#include "agg_renderer_primitives.h"
#include "agg_rasterizer_outline.h"
#include "agg_path_storage.h"

#include "agg_draw_text.h"

struct clickable_t {

	float x_pos_f = 0;
	float y_pos_f = 0;
	float width_f = 0.3;
	float size_ratio = 0.5;

	int x_pos, y_pos;
	int width, height;

	const int screen_width = 1920;
	const int screen_height = 1080;

	std::function<void()> on_click;

	clickable_t(float x_pos_f, float y_pos_f, float width_f, float size_ratio, std::function<void()> on_click)
	{

		this->x_pos_f = x_pos_f;
		this->y_pos_f = y_pos_f;
		this->width_f = width_f;
		this->size_ratio = size_ratio;

		width = screen_width * width_f;
		double scaling_k = float(width) / screen_width;

		height = width / size_ratio;
		x_pos = x_pos_f * screen_width;
		y_pos = y_pos_f * screen_height;

		this->on_click = on_click;

	}

	virtual ~clickable_t()
	{
	}

	virtual bool check_click(int x, int y)
	{
		if (x < x_pos || y < y_pos || x >(x_pos + width) || y >(y_pos + height))
			return false;

		on_click();

		return true;
	}

};

struct button_t : public clickable_t {

	typedef agg::pixfmt_bgra32 pixfmt;
	typedef agg::renderer_base<pixfmt> renderer_base;
	typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_solid;
	typedef agg::renderer_scanline_bin_solid<renderer_base> renderer_bin;

	std::string text;

	button_t(float x_pos_f, float y_pos_f, float width_f, float size_ratio, std::function<void()> on_click, const std::string& text)
		: clickable_t(x_pos_f, y_pos_f, width_f, size_ratio, on_click)
	{
		this->text = text;
	}

	virtual void draw(renderer_base& rb)
	{

		{

			int w = width;
			int x = x_pos;
			int y = y_pos;
			agg::rounded_rect r(x, y, x + width, y + height, 20);
			r.normalize_radius();

			agg::rasterizer_scanline_aa<> ras;
			agg::scanline_p8 sl;
			renderer_solid ren(rb);
			renderer_bin ren_bin(rb);

			// Drawing as an outline
			agg::conv_stroke<agg::rounded_rect> p(r);
			p.width(10.0);
			ras.add_path(r);
			//ren.color(agg::rgba(1, 1, 1, 0.1));
			//agg::render_scanlines_aa_solid(ras, sl, rb, agg::rgba(1.0, 1.0, 1.0, 0.1));
			//agg::render_scanlines_aa_solid(ras, sl, rb, agg::rgba(0.0, 0.0, 0.0, 1.0));
			agg::render_scanlines_aa_solid(ras, sl, rb, agg::rgba(1.0, 1.0, 1.0, 0.4));
			//agg::render_scanlines(ras, sl, ren);
		}

		{
			agg::rasterizer_scanline_aa<> ras;
			agg::scanline_u8 sl;
			renderer_solid ren(rb);
			renderer_bin ren_bin(rb);

			int x = x_pos;
			int y = y_pos + 100;

			draw_text(ras, sl, ren, ren_bin, x, y, agg::rgba(1.0, 1.0, 1.0, 7.0), text.c_str());
		}


	}


};
