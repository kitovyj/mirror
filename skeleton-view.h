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
#include "agg_font_win32_tt.h"
#include "agg_renderer_primitives.h"
#include "agg_rasterizer_outline.h"
#include "agg_path_storage.h"

#include "kinect-utils.h"

struct skeleton_view_t
{
	const double width_f = 0.3;
	const double horz_space_f = 0.01;

	int width;
	int height;
	int space;

	double scaling_k;

	typedef agg::pixfmt_bgra32 pixfmt;
	typedef agg::renderer_base<pixfmt> renderer_base;
	typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_solid;
	typedef agg::renderer_scanline_bin_solid<renderer_base> renderer_bin;

	const int bytes_per_pixel = 4;
	agg::rendering_buffer rendering_buffer;
	pixfmt pixf;
	renderer_base ren_base;
	//agg::rasterizer_scanline_aa<> ras;
	agg::scanline_u8 sl;

	kinect_t& kinect;

	skeleton_view_t(unsigned char* screen_data, int screen_width, int screen_height, kinect_t& kinect)
		: kinect(kinect)
	{
		space = horz_space_f * screen_width;
		width = screen_width * width_f;
		height = (width * screen_height) / screen_width;
		scaling_k = float(width) / screen_width;
		int offset = (space * screen_width + space) * bytes_per_pixel;
		rendering_buffer = agg::rendering_buffer(screen_data + offset, width, height, screen_width * bytes_per_pixel);
		pixf = pixfmt(rendering_buffer);
		ren_base = renderer_base(pixf);
	}

	std::pair<float, float> joint_view_pos(Joint& j)
	{
		auto sp = kinect.joint_screen_pos(j);
		sp.first *= scaling_k;
		sp.second *= scaling_k;
		return sp;
	}

	void draw_joints(const std::vector<int>& joints_to_draw, Joint* joints, agg::path_storage& path, agg::rasterizer_scanline_aa<>& ras)
	{
		bool first = true;
		for(auto j : joints_to_draw)
		{
			if (joints[j].TrackingState == TrackingState_NotTracked) return;
			auto jp = joint_view_pos(joints[j]);
			if (first)
			{
				path.move_to(jp.first, jp.second);
				first = false;
			} else
				path.line_to(jp.first, jp.second);

			int er = 3;
			ras.add_path(agg::ellipse(jp.first, jp.second, er, er, 50));

		}

	}

	void draw(Joint* joints)
	{
		ren_base.clear(agg::rgba8(0, 0, 0, 0));

		auto lth = joint_view_pos(joints[JointType_ThumbLeft]);
		auto l = joint_view_pos(joints[JointType_HandLeft]);
		auto lt = joint_view_pos(joints[JointType_HandTipLeft]);
		auto lh = joint_view_pos(joints[JointType_WristLeft]);
		auto le = joint_view_pos(joints[JointType_ElbowLeft]);
		auto ls = joint_view_pos(joints[JointType_ShoulderLeft]);
		
		agg::rasterizer_scanline_aa<> ras;
		agg::scanline_p8 sl;
		agg::path_storage path;


		draw_joints({ JointType_ShoulderLeft, JointType_ElbowLeft, JointType_WristLeft, JointType_HandLeft, JointType_HandTipLeft }, joints, path, ras);
		draw_joints({ JointType_ThumbLeft, JointType_HandLeft }, joints, path, ras);

		draw_joints({ JointType_ShoulderRight, JointType_ElbowRight, JointType_WristRight, JointType_HandRight, JointType_HandTipRight }, joints, path, ras);
		draw_joints({ JointType_ThumbRight, JointType_HandRight }, joints, path, ras);

		draw_joints({ JointType_Neck, JointType_Head }, joints, path, ras);

		draw_joints({ JointType_Neck, JointType_SpineShoulder, JointType_SpineMid, JointType_SpineBase }, joints, path, ras);

		draw_joints({ JointType_ShoulderLeft, JointType_SpineShoulder, JointType_ShoulderRight }, joints, path, ras);

		draw_joints({ JointType_HipLeft, JointType_SpineBase, JointType_HipRight }, joints, path, ras);

		draw_joints({ JointType_HipLeft, JointType_KneeLeft, JointType_AnkleLeft, JointType_FootLeft }, joints, path, ras);
		draw_joints({ JointType_HipRight, JointType_KneeRight, JointType_AnkleRight, JointType_FootRight }, joints, path, ras);

		agg::conv_stroke<agg::path_storage> stroke(path);
		//stroke.line_join(join);
		//stroke.line_cap(cap);
		//stroke.miter_limit(m_miter_limit.value());
		stroke.width(5.0);
		ras.add_path(stroke);

		agg::render_scanlines_aa_solid(ras, sl, ren_base, agg::rgba8(50, 255, 50, 255));

	}

};