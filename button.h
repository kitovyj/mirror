#pragma once

#include <functional>
#include <iostream>
#include <chrono>

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

#include "agg_image_accessors.h"
#include "agg_span_interpolator_linear.h"
#include "agg_span_image_filter_rgba.h"
#include "agg_span_allocator.h"

#include "agg_draw_text.h"



#include <boost/gil.hpp>

#include "screen.h"

struct clickable_t {

	float x_pos_f = 0;
	float y_pos_f = 0;
	float width_f = 0.3;
	float height_f = 0.5;

	int x_pos, y_pos;
	int width, height;
	int dx, dy;

	const int screen_width = screen_t::width;
	const int screen_height = screen_t::height;

	std::function<void()> on_click;

	clickable_t(float x_pos_f, float y_pos_f, float width_f, float height_f, std::function<void()> on_click)
	{

		this->x_pos_f = x_pos_f;
		this->y_pos_f = y_pos_f;
		this->width_f = width_f;
		this->height_f = height_f;

		width = screen_t::dpc_x * width_f;
		double scaling_k = float(width) / screen_width;

		height = screen_t::dpc_y * height_f;
		x_pos = x_pos_f * screen_t::dpc_x;
		y_pos = y_pos_f * screen_t::dpc_y;

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

struct canvas_t
{

	typedef agg::pixfmt_bgra32 pixfmt;
	typedef agg::renderer_base<pixfmt> renderer_base;

	typedef agg::rgba8 color_type;
	typedef agg::order_bgra component_order;
	typedef agg::rendering_buffer rbuf_type;

	typedef agg::blender_rgba_pre<color_type, component_order> blender_type_pre;
	typedef agg::pixfmt_alpha_blend_rgba<blender_type_pre, rbuf_type> pixfmt_pre;
	typedef agg::renderer_base<pixfmt_pre> ren_base_pre;
	
	agg::rendering_buffer ren_buffer;
	renderer_base& rb;
	ren_base_pre& rb_pre;
	boost::gil::bgra8_view_t& view;


	canvas_t(renderer_base& rb, ren_base_pre& rb_pre, boost::gil::bgra8_view_t& view, agg::rendering_buffer& ren_buf)
		: rb(rb), rb_pre(rb_pre), view(view), ren_buffer(ren_buf)
	{
	}

};

struct button_t : public clickable_t {

	//typedef agg::pixfmt_bgra32_plain pixfmt;
	//typedef agg::renderer_base<pixfmt> renderer_base;
//	typedef agg::renderer_outline_aa<renderer_base> renderer_outline;

	//typedef agg::pixfmt_bgra32 pixfmt;
	//typedef agg::pixfmt_bgra32_pre pixfmt_pre;
	typedef agg::rgba8 color_type;
	typedef agg::order_bgra component_order;
	typedef agg::rendering_buffer rbuf_type;
	
	typedef agg::blender_rgba<color_type, component_order> prim_blender_type;
	typedef agg::pixfmt_alpha_blend_rgba<prim_blender_type, rbuf_type> prim_pixfmt_type;
	typedef agg::renderer_base<prim_pixfmt_type> prim_ren_base_type;
	
	typedef agg::blender_rgba_pre<color_type, component_order> blender_type_pre;
	typedef agg::pixfmt_alpha_blend_rgba<blender_type_pre, rbuf_type> pixfmt_pre;
	typedef agg::renderer_base<pixfmt_pre> ren_base_pre;

	const int bytes_per_pixel = 4;

	std::vector<uint8_t> rendered;
	std::vector<uint8_t> rendered_highlighted;

	agg::rendering_buffer ren_buf;
	agg::rendering_buffer ren_buf_highlighted;

	prim_pixfmt_type pixfmt_normal;
	prim_pixfmt_type pixfmt_highlighted;


	std::string text;

	bool highlighted;

	std::mutex draw_mutex;

	int radius;

	button_t(float x_pos_f, float y_pos_f, float width_f, float height_f, std::function<void()> on_click, const std::string& text, int radius = 20)
		: clickable_t(x_pos_f, y_pos_f, width_f, height_f, on_click)
	{

		this->radius = radius;

		highlighted = false;
		this->text = text;

		// normal

		typedef prim_pixfmt_type pixfmt;
		typedef prim_ren_base_type ren_base;
		typedef agg::renderer_scanline_aa_solid<ren_base> renderer_solid;
		typedef agg::renderer_scanline_bin_solid<ren_base> renderer_bin;

		dx = 20;
		dy = 20;
		int full_width = width + 2 * dx;
		int full_height = height + 2 * dy;

		rendered = std::vector<uint8_t>(full_width * full_height * bytes_per_pixel, 0);

		{
			agg::rendering_buffer rendering_buffer(&rendered[0], full_width, full_height, full_width * bytes_per_pixel);
			pixfmt pixf(rendering_buffer);
			
			ren_base rb(pixf);

			agg::rasterizer_scanline_aa<> ras;
			agg::scanline_p8 sl;

			renderer_solid ren;
			renderer_bin ren_bin;

			int w = width;
			int x = dx;//x_pos;
			int y = dy;//y_pos;
			agg::rounded_rect r(x, y, x + width, y + height, radius);
			r.normalize_radius();

			// Drawing as an outline
			ras.add_path(r);

			//ren.color(agg::rgba(1, 1, 1, 0.1));
			//agg::render_scanlines_aa_solid(ras, sl, rb, agg::rgba(1.0, 1.0, 1.0, 0.1));
			//agg::render_scanlines_aa_solid(ras, sl, rb, agg::rgba(0.0, 0.0, 0.0, 1.0));
			agg::render_scanlines_aa_solid(ras, sl, rb, agg::rgba(1.0, 1.0, 1.0, 0.4));

			rendered_highlighted = rendered;

			if (!text.empty()) {

				int text_height = 50;
				agg::rasterizer_scanline_aa<> ras;
				agg::scanline_u8 sl;
				renderer_solid ren(rb);
				renderer_bin ren_bin(rb);

				auto sz = text_size(ras, sl, ren, ren_bin, text.c_str(), text_height);

				int x = /*x_pos*/dx + (width - sz.first) / 2;
				int y = /*y_pos*/dy + (height - sz.second) / 2;

				draw_text(ras, sl, ren, ren_bin, x, y, agg::rgba8(255, 255, 255, 255), text.c_str(), text_height);
			}

			ren_buf = rendering_buffer;

			pixfmt_normal = pixfmt(ren_buf);

		}

		// highlighted

		{

			agg::rendering_buffer rendering_buffer(&rendered_highlighted[0], full_width, full_height, full_width * bytes_per_pixel);
			pixfmt pixf(rendering_buffer);

			ren_base rb(pixf);

			agg::rasterizer_scanline_aa<> ras;
			agg::scanline_p8 sl;

			//ras.reset();

			int w = width;
			int x = dx;//x_pos;
			int y = dy;//y_pos;
			agg::rounded_rect r(x, y, x + width, y + height, radius);
			r.normalize_radius();


			agg::conv_stroke<agg::rounded_rect> p(r);
			p.width(10.0);
			ras.add_path(p);
			//ren.color(agg::rgba(1, 1, 1, 0.9));
			agg::render_scanlines_aa_solid(ras, sl, rb, agg::rgba(1, 1, 1, 0.9));

			if (!text.empty()) {

				int text_height = 56;
				agg::rasterizer_scanline_aa<> ras;
				agg::scanline_u8 sl;
				renderer_solid ren(rb);
				renderer_bin ren_bin(rb);

				auto sz = text_size(ras, sl, ren, ren_bin, text.c_str(), text_height);

				int x = /*x_pos*/dx + (width - sz.first) / 2;
				int y = /*y_pos*/dy + (height - sz.second) / 2;

				draw_text(ras, sl, ren, ren_bin, x, y, agg::rgba8(255, 255, 255, 255), text.c_str(), text_height);
			}

			ren_buf_highlighted = rendering_buffer;

			pixfmt_highlighted = pixfmt(ren_buf_highlighted);

		}


	}

	virtual void draw(canvas_t& canvas)
	{


		//typedef agg::blender_rgba_pre<color_type, component_order> blender_type_pre;
		//typedef agg::pixfmt_alpha_blend_rgba<blender_type_pre, rbuf_type> pixfmt_pre;
		//typedef agg::renderer_base<pixfmt_pre> ren_base_pre;

		//pixfmt_pre pf(canvas.ren_buffer);
		//ren_base_pre rb(pf);


		//return;

		agg::cover_type ct = agg::cover_full;

		//auto start = std::chrono::steady_clock::now();

		std::lock_guard<std::mutex> guard(draw_mutex);

		canvas.rb_pre.blend_from(highlighted ? pixfmt_highlighted : pixfmt_normal, 0, x_pos - dx, y_pos - dy, ct);

		//auto end = std::chrono::steady_clock::now();

		//int count = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

		//std::cout << "Elapsed time in milliseconds : "
		//	<< count
		//	<< " ms" << std::endl;

		/*

		//#define image_filter_2x2_type agg::span_image_filter_rgba_2x2
		#define image_resample_affine_type agg::span_image_resample_rgba_affine

		typedef image_resample_affine_type<source_type> span_gen_type;

		//	typedef image_filter_2x2_type<source_type,
		//interpolator_type> span_gen_type;


		typedef pixfmt::color_type color_type;
		typedef agg::span_allocator<color_type> span_alloc_type;

		agg::rasterizer_scanline_aa<> ras;
		agg::scanline_u8 sl;

		span_alloc_type sa;

		//span_gen_type sg(source, interpolator, filter);

		//g_rasterizer.clip_box(0, 0, agdst->width(), agdst->height());
		//g_rasterizer.reset();


		agg::render_scanlines_aa(ras, sl, canvas.rb.ren(), sa, sg);


		if (highlighted)
		{


		}

		*/
	}

	virtual bool check_highlighted(int x, int y)
	{
		if (x < x_pos || y < y_pos || x >(x_pos + width) || y >(y_pos + height))
		{
			highlighted = false;
		}
		else
		{
			highlighted = true;
		}
		return highlighted;
	}


};
