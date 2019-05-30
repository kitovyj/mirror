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

	void draw(Joint* joints)
	{
		ren_base.clear(agg::rgba8(0, 0, 0, 0));

		auto lth = joint_view_pos(joints[JointType_ThumbLeft]);
		auto l = joint_view_pos(joints[JointType_HandLeft]);
		auto lt = joint_view_pos(joints[JointType_HandTipLeft]);
		auto lh = joint_view_pos(joints[JointType_WristLeft]);
		auto le = joint_view_pos(joints[JointType_ElbowLeft]);
		auto ls = joint_view_pos(joints[JointType_ShoulderLeft]);

		auto rth = joint_view_pos(joints[JointType_ThumbRight]);
		auto r = joint_view_pos(joints[JointType_HandRight]);
		auto rt = joint_view_pos(joints[JointType_HandTipRight]);
		auto rh = joint_view_pos(joints[JointType_WristRight]);
		auto re = joint_view_pos(joints[JointType_ElbowRight]);
		auto rs = joint_view_pos(joints[JointType_ShoulderRight]);

		auto head = joint_view_pos(joints[JointType_Head]);
		auto neck = joint_view_pos(joints[JointType_Neck]);
		auto spine_shoulder = joint_view_pos(joints[JointType_SpineShoulder]);
		auto spine_mid = joint_view_pos(joints[JointType_SpineMid]);
		auto spine_base = joint_view_pos(joints[JointType_SpineBase]);

		auto hip_l = joint_view_pos(joints[JointType_HipLeft]);
		auto knee_l = joint_view_pos(joints[JointType_KneeLeft]);
		auto ankle_l = joint_view_pos(joints[JointType_AnkleLeft]);
		auto foot_l = joint_view_pos(joints[JointType_FootLeft]);

		auto hip_r = joint_view_pos(joints[JointType_HipRight]);
		auto knee_r = joint_view_pos(joints[JointType_KneeRight]);
		auto ankle_r = joint_view_pos(joints[JointType_AnkleRight]);
		auto foot_r = joint_view_pos(joints[JointType_FootRight]);

		/*
		TrackingState_NotTracked = 0,
		TrackingState_Inferred = 1,
		TrackingState_Tracked = 2
		*/

		agg::rasterizer_scanline_aa<> ras;
		agg::scanline_p8 sl;

		agg::path_storage path;

		path.move_to(lt.first, lt.second);
		path.line_to(l.first, l.second);
		path.line_to(lh.first, lh.second);
		path.line_to(le.first, le.second);
		path.line_to(ls.first, ls.second);

		path.move_to(lth.first, lth.second);
		path.line_to(lh.first, lh.second);

		path.move_to(rt.first, rt.second);
		path.line_to(r.first, r.second);
		path.line_to(rh.first, rh.second);
		path.line_to(re.first, re.second);
		path.line_to(rs.first, rs.second);

		path.move_to(rth.first, rth.second);
		path.line_to(rh.first, rh.second);

		path.move_to(head.first, head.second);
		path.line_to(neck.first, neck.second);
		path.line_to(spine_shoulder.first, spine_shoulder.second);
		path.line_to(spine_mid.first, spine_mid.second);

		path.line_to(spine_base.first, spine_base.second);

		// shoulders

		path.move_to(ls.first, ls.second);
		path.line_to(spine_shoulder.first, spine_shoulder.second);
		path.line_to(rs.first, rs.second);

		// hips

		path.move_to(hip_l.first, hip_l.second);
		path.line_to(spine_base.first, spine_base.second);
		path.line_to(hip_r.first, hip_r.second);

		// left leg

		path.move_to(hip_l.first, hip_l.second);

		if (joints[JointType_KneeLeft].TrackingState != TrackingState_NotTracked)
		{
			path.line_to(knee_l.first, knee_l.second);
			if (joints[JointType_AnkleLeft].TrackingState != TrackingState_NotTracked)
			{
				path.line_to(ankle_l.first, ankle_l.second);
				if (joints[JointType_FootLeft].TrackingState != TrackingState_NotTracked)
					path.line_to(foot_l.first, foot_l.second);
			}
		}

		// right leg

		path.move_to(hip_r.first, hip_r.second);
		if (joints[JointType_KneeRight].TrackingState != TrackingState_NotTracked)
		{
			path.line_to(knee_r.first, knee_r.second);
			if (joints[JointType_AnkleRight].TrackingState != TrackingState_NotTracked)
			{
				path.line_to(ankle_r.first, ankle_r.second);
				if (joints[JointType_FootRight].TrackingState != TrackingState_NotTracked)
					path.line_to(foot_r.first, foot_r.second);
			}
		}
		agg::conv_stroke<agg::path_storage> stroke(path);
		//stroke.line_join(join);
		//stroke.line_cap(cap);
		//stroke.miter_limit(m_miter_limit.value());
		stroke.width(5.0);
		ras.add_path(stroke);

		agg::render_scanlines_aa_solid(ras, sl, ren_base, agg::rgba8(50, 255, 50, 255));

		ras.reset();

		int er = 3;

		ras.add_path(agg::ellipse(lt.first, lt.second, er, er, 50));
		ras.add_path(agg::ellipse(l.first, l.second, er, er, 50));
		ras.add_path(agg::ellipse(lh.first, lh.second, er, er, 50));
		ras.add_path(agg::ellipse(le.first, le.second, er, er, 50));
		ras.add_path(agg::ellipse(ls.first, ls.second, er, er, 50));

		ras.add_path(agg::ellipse(rt.first, rt.second, er, er, 50));
		ras.add_path(agg::ellipse(r.first, r.second, er, er, 50));
		ras.add_path(agg::ellipse(rh.first, rh.second, er, er, 50));
		ras.add_path(agg::ellipse(re.first, re.second, er, er, 50));
		ras.add_path(agg::ellipse(rs.first, rs.second, er, er, 50));

		agg::render_scanlines_aa_solid(ras, sl, ren_base, agg::rgba8(150, 255, 150, 255));

		/*
		agg::renderer_primitives<renderer_base> ren(ren_base);

		typedef agg::rasterizer_outline<agg::renderer_primitives<renderer_base> > outline_rasterizer;
		//outline_rasterizer ras(ren);
		agg::rasterizer_scanline_aa<> ras;


		ren.line_color(agg::srgba8(255, 255, 255, 255));

		ren.move_to(lh.first, lh.second);
		ren.line_to(le.first, le.second);
		ren.line_to(ls.first, ls.second);

		ren.move_to(rh.first, rh.second);
		ren.line_to(re.first, re.second);
		ren.line_to(rs.first, rs.second);

		agg::render_scanlines(ras, sl, ren);
		*/

		/*
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);

		glBegin(GL_LINES);
		glColor3f(1.f, 0.f, 0.f);
		glVertex3f(lh.X, lh.Y, lh.Z);
		glVertex3f(le.X, le.Y, le.Z);
		glVertex3f(le.X, le.Y, le.Z);
		glVertex3f(ls.X, ls.Y, ls.Z);
		glVertex3f(rh.X, rh.Y, rh.Z);
		glVertex3f(re.X, re.Y, re.Z);
		glVertex3f(re.X, re.Y, re.Z);
		glVertex3f(rs.X, rs.Y, rs.Z);
		glEnd();
		*/


	}

};