#pragma once

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

#include <algorithm>

//#include "agg_font_win32_tt.h"
//typedef agg::font_engine_win32_tt_int32 font_engine_type;

typedef agg::font_engine_freetype_int32 font_engine_type;
typedef agg::font_cache_manager<font_engine_type> font_manager_type;

extern font_engine_type m_feng;
extern font_manager_type m_fman;

// Pipeline to process the vectors glyph paths (curves + contour)
extern agg::conv_curve<font_manager_type::path_adaptor_type> m_curves;
extern agg::conv_contour<agg::conv_curve<font_manager_type::path_adaptor_type> > m_contour;

template<class Rasterizer, class Scanline, class RenSolid, class RenBin>
unsigned draw_text(Rasterizer& ras, Scanline& sl,
	RenSolid& ren_solid, RenBin& ren_bin, int pos_x, int pos_y, agg::rgba8 color, const char* text, int m_height = 32)
{
	agg::glyph_rendering gren = agg::glyph_ren_native_mono;
	switch (4)
	{
	case 0: gren = agg::glyph_ren_native_mono;  break;
	case 1: gren = agg::glyph_ren_native_gray8; break;
	case 2: gren = agg::glyph_ren_outline;      break;
	case 3: gren = agg::glyph_ren_agg_mono;     break;
	case 4: gren = agg::glyph_ren_agg_gray8;    break;
	}

	unsigned num_glyphs = 0;

	int width = 1920;
	int height = 1080;

	int m_weight = 2;
	int m_width = m_height;
	bool m_kerning = true;

	m_contour.width(-m_weight * m_height * 0.05);

	if (!m_feng.load_font("Chunkfive.otf", 0, gren))
		return 0;
	//if (m_feng.create_font("Arial", gren))
		//	if (m_feng.load_font("Chunkfive.otf", 0, gren))
	m_feng.hinting(false);
	m_feng.height(m_height);
	//m_feng.width(m_width);
	m_feng.width((m_width == m_height) ? 0.0 : m_width / 2.4);
	m_feng.flip_y(true);

	agg::trans_affine mtx;
	mtx *= agg::trans_affine_rotation(agg::deg2rad(-4.0));
	mtx *= agg::trans_affine_skewing(-0.4, 0);
	//mtx *= agg::trans_affine_translation(1, 0);
	//m_feng.transform(mtx);

	double x = pos_x;
	double t_y = pos_y;
	double y0 = t_y + m_height;
	//double y0 = t_y;
	double y = y0;
	const char* p = text;

	while (*p)
	{
		const agg::glyph_cache* glyph = m_fman.glyph(*p);
		if (glyph)
		{
			if (m_kerning)
			{
				m_fman.add_kerning(&x, &y);
			}

			if (x >= width - m_height)
			{
				x = 10.0;
				y0 -= m_height;
				if (y0 <= 120) break;
				y = y0;
			}

			m_fman.init_embedded_adaptors(glyph, x, y);

			switch (glyph->data_type)
			{
			default: break;
			case agg::glyph_data_mono:
				ren_bin.color(color);
				agg::render_scanlines(m_fman.mono_adaptor(),
					m_fman.mono_scanline(),
					ren_bin);
				break;

			case agg::glyph_data_gray8:
				ren_solid.color(color);
				agg::render_scanlines(m_fman.gray8_adaptor(),
					m_fman.gray8_scanline(),
					ren_solid);
				break;

			case agg::glyph_data_outline:
				ras.reset();
				if (fabs(m_weight) <= 0.01)
				{
					// For the sake of efficiency skip the
					// contour converter if the weight is about zero.
					//-----------------------
					ras.add_path(m_curves);
				}
				else
				{
					ras.add_path(m_contour);
				}
				ren_solid.color(color);
				agg::render_scanlines(ras, sl, ren_solid);
				//dump_path(m_fman.path_adaptor());
				break;
			}

			// increment pen position
			x += glyph->advance_x;
			y += glyph->advance_y;
			++num_glyphs;
		}
		++p;
	}

	return num_glyphs;
}

template<class Rasterizer, class Scanline, class RenSolid, class RenBin>
std::pair<int, int> text_size(Rasterizer& ras, Scanline& sl, RenSolid& ren_solid, RenBin& ren_bin, const char* text, int m_height = 32)
{
	agg::glyph_rendering gren = agg::glyph_ren_native_mono;
	switch (4)
	{
	case 0: gren = agg::glyph_ren_native_mono;  break;
	case 1: gren = agg::glyph_ren_native_gray8; break;
	case 2: gren = agg::glyph_ren_outline;      break;
	case 3: gren = agg::glyph_ren_agg_mono;     break;
	case 4: gren = agg::glyph_ren_agg_gray8;    break;
	}

	unsigned num_glyphs = 0;

	int width = 1920;
	int height = 1080;

	int m_weight = 2;
	int m_width = m_height;
	bool m_kerning = true;

	m_contour.width(-m_weight * m_height * 0.05);

	if (!m_feng.load_font("Chunkfive.otf", 0, gren))
		return std::make_pair<int, int>(0, 0);
	//if (m_feng.create_font("Arial", gren))
		//	if (m_feng.load_font("Chunkfive.otf", 0, gren))
	m_feng.hinting(false);
	m_feng.height(m_height);
	//m_feng.width(m_width);
	m_feng.width((m_width == m_height) ? 0.0 : m_width / 2.4);
	m_feng.flip_y(true);

	agg::trans_affine mtx;
	mtx *= agg::trans_affine_rotation(agg::deg2rad(-4.0));
	mtx *= agg::trans_affine_skewing(-0.4, 0);
	//mtx *= agg::trans_affine_translation(1, 0);
	//m_feng.transform(mtx);

	int pos_x = 0;
	int pos_y = 0;

	double x = pos_x;
	double t_y = pos_y;
	//double y0 = t_y - m_height - 10.0;
	double y0 = m_height + 10.0;
	double y = y0;
	const char* p = text;

	while (*p)
	{
		const agg::glyph_cache* glyph = m_fman.glyph(*p);
		if (glyph)
		{
			if (m_kerning)
			{
				m_fman.add_kerning(&x, &y);
			}

			if (x >= width - m_height)
			{
				x = 10.0;
				y0 -= m_height;
				if (y0 <= 120) break;
				y = y0;
			}

			m_fman.init_embedded_adaptors(glyph, x, y);

			// increment pen position
			x += glyph->advance_x;
			y += glyph->advance_y;
			++num_glyphs;
		}
		++p;
	}

	return std::make_pair<int, int>(int(x), int(y));
}
