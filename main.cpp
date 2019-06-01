#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>

#include <Windows.h>
#include <Ole2.h>

#include <gl/GL.h>
#include <gl/GLU.h>
#include <gl/glut.h>

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

#include <Kinect.h>

#include "agg_draw_text.h"

#include <iostream>
#include <vector>
#include <algorithm>

#include <thread>
#include <mutex>

#include <chrono> 
#include <functional>
#include <memory>
#include <set>

#include "kinect-utils.h"
#include "skeleton-view.h"
#include "button.h"

#include "classifier.h"
#include "recommendation.h"

const int bytes_per_pixel = 4;
const int width = kinect_t::width;
const int height = kinect_t::height;

kinect_t kinect;

float cursor_x, cursor_y;

// OpenGL Variables`
GLuint textureId;              // ID of the texture to contain Kinect RGB Data
GLubyte data[width*height * bytes_per_pixel];  // BGRA array containing the texture data
GLubyte data_os[width*height * bytes_per_pixel];  // BGRA array containing the texture data
GLubyte raw_frame[width*height * bytes_per_pixel];  // BGRA array containing the texture data

typedef agg::pixfmt_bgra32 pixfmt;
typedef agg::renderer_base<pixfmt> renderer_base;
typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_solid;
typedef agg::renderer_scanline_bin_solid<renderer_base> renderer_bin;

// Body tracking variables
BOOLEAN tracked;							// Whether we see a body
Joint joints[JointType_Count];				// List of joints in the tracked body

skeleton_view_t seleleton_view(&data[0], width, height, kinect);


agg::rendering_buffer rendering_buffer(&data[0], width, height, width * bytes_per_pixel);
pixfmt pixf(rendering_buffer);
renderer_base rb(pixf);

// https://github.com/Vangos/kinect-controls/blob/master/KinectControls/KinectControls.Test/MainWindow.xaml.cs

void draw(void);
void idle(void);

std::mutex draw_mutex;
std::mutex frame_mutex;

std::set<std::shared_ptr<clickable_t> > clickables;
std::set<std::shared_ptr<button_t> > buttons;

bool init(int argc, char* argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(width, height);
    glutCreateWindow("MagicMirror");
    glutDisplayFunc(draw);
    glutIdleFunc(idle);
    return true;
}


void getBodyData(IMultiSourceFrame* frame) {
	IBodyFrame* bodyframe;
	IBodyFrameReference* frameref = NULL;
	frame->get_BodyFrameReference(&frameref);
	frameref->AcquireFrame(&bodyframe);
	if (frameref) frameref->Release();

	if (!bodyframe) return;

	IBody* body[BODY_COUNT] = { 0 };
	bodyframe->GetAndRefreshBodyData(BODY_COUNT, body);
	for (int i = 0; i < BODY_COUNT; i++) {
		body[i]->get_IsTracked(&tracked);
		if (tracked) {
			body[i]->GetJoints(JointType_Count, joints);
			break;
		}
	}

	if (bodyframe) bodyframe->Release();
}

struct hand_tip_pos {

	std::chrono::time_point<std::chrono::steady_clock> time_point;

	double x;
	double y;
	double z;

};

struct hand_tip_pos_compare {

	bool operator()(const hand_tip_pos& a, const hand_tip_pos& b) const
	{
		return a.time_point < b.time_point;
	}

	bool operator()(const std::chrono::time_point<std::chrono::steady_clock>& a, const hand_tip_pos& b) const
	{
		return a < b.time_point;
	}

	bool operator()(const hand_tip_pos& a, const std::chrono::time_point<std::chrono::steady_clock>& b) const
	{
		return a.time_point < b;
	}

};

std::vector<hand_tip_pos> hand_tip_trajectory;
volatile bool press_detected = false;

void getKinectData(GLubyte* dest) {

	press_detected = false;

	//IColorFrame* frame = NULL;
	IMultiSourceFrame* frame = NULL;
	if (SUCCEEDED(kinect.reader->AcquireLatestFrame(&frame))) {


		IColorFrameReference * color_frame_ref;
		frame->get_ColorFrameReference(&color_frame_ref);
		IColorFrame * color_frame;
		color_frame_ref->AcquireFrame(&color_frame);
		if (color_frame_ref) color_frame_ref->Release();

		if (!color_frame) return;

		color_frame->CopyConvertedFrameDataToArray(width*height * bytes_per_pixel, data, ColorImageFormat_Bgra);

		{
			std::mutex frame_mutex;
			std::size_t sz = width * height * bytes_per_pixel;
			std::copy(data, data + sz, raw_frame);
		}
		
		if (color_frame) color_frame->Release();
		getBodyData(frame);

		Joint handLeft = joints[JointType_HandTipLeft];
		Joint handRight = joints[JointType_HandTipRight];

		if (handLeft.TrackingState != TrackingState_NotTracked && handRight.TrackingState != TrackingState_NotTracked)
		{
			// Select the hand that is closer to the sensor.
			Joint activeHand = handRight.Position.Z <= handLeft.Position.Z ? handRight : handLeft;

			DepthSpacePoint depthPoint = { 0 };
			kinect.mapper->MapCameraPointToDepthSpace(activeHand.Position, &depthPoint);

			static const int        cDepthWidth = 512;
			static const int        cDepthHeight = 424;

			cursor_x = static_cast<float>(depthPoint.X * width) / cDepthWidth;
			cursor_y = static_cast<float>(depthPoint.Y * height) / cDepthHeight;

			hand_tip_pos p;

			p.x = activeHand.Position.X;
			p.y = activeHand.Position.Y;
			p.z = activeHand.Position.Z;

			p.time_point = std::chrono::steady_clock::now();


			hand_tip_trajectory.push_back(p);

			std::chrono::steady_clock::duration two_seconds = std::chrono::milliseconds(2000);
			std::chrono::time_point<std::chrono::steady_clock> old_tp = p.time_point - two_seconds;

			auto old = std::lower_bound(hand_tip_trajectory.begin(), hand_tip_trajectory.end(), old_tp, hand_tip_pos_compare());

			hand_tip_trajectory.erase(hand_tip_trajectory.begin(), old);


			std::chrono::steady_clock::duration jesture_duration = std::chrono::milliseconds(200);
			std::chrono::steady_clock::duration jesture_duration_delta = std::chrono::milliseconds(200);

			std::chrono::time_point<std::chrono::steady_clock> earliest = p.time_point - jesture_duration - jesture_duration_delta;
			std::chrono::time_point<std::chrono::steady_clock> latest = p.time_point - jesture_duration + jesture_duration_delta;

			auto lower = std::lower_bound(hand_tip_trajectory.begin(), hand_tip_trajectory.end(), earliest, hand_tip_pos_compare());
			auto upper = std::lower_bound(hand_tip_trajectory.begin(), hand_tip_trajectory.end(), latest, hand_tip_pos_compare());

			double min_distance = 100;
			std::vector<hand_tip_pos>::iterator start = hand_tip_trajectory.end();


			for (auto i = lower;i < upper;i++)
			{
				double d = sqrt(pow(p.x - i->x, 2) + pow(p.y - i->y, 2) + pow(p.z - i->z, 2));
				if (d < min_distance)
				{
					min_distance = d;
					start = i;
				}
			}

			if (min_distance <= 0.02)
			{

				double max_distance = 0;

				for (auto i = start;i != hand_tip_trajectory.end();i++)
				{
					double d = sqrt(pow(p.x - i->x, 2) + pow(p.y - i->y, 2) + pow(p.z - i->z, 2));
					if (d > max_distance)
					{
						max_distance = d;
					}

				}

				// in meters!

				if (max_distance > 0.02)
				{
					press_detected = true;

				}

			}

			//cursor.Flip(activeHand);
			//cursor.Update(position);
		}

	}

	if (press_detected)
	{

		for (auto &c : clickables) {
			bool r = c->check_click(cursor_x, cursor_y);
			if (r)
				break;
		}


	}


    if (frame) frame->Release();
}



void drawKinectData() {
    getKinectData(data);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (tracked != 0)
		seleleton_view.draw(&joints[0]);

	std::size_t sz = width * height * 4;
	//std::copy(data, data + sz, data_os);
	//std::fill(data, data + sz, 0);

	for (auto &b : buttons) {
		b->draw(rb);
	}


	{

		agg::ellipse ell;
		int r = 30;
		auto color = agg::rgba(0.0, 1.0, 0.0, 0.1);
		if (press_detected)
		{
			r = 50;
			color = agg::rgba(1.0, 0.0, 0.0, 1.0);
		}

		ell.init(cursor_x, cursor_y, r, r, 50);
		agg::rasterizer_scanline_aa<> ras;
		agg::scanline_p8 sl;
		ras.add_path(ell);
		agg::render_scanlines_aa_solid(ras, sl, rb, color);
	}


	{

		std::size_t sz = width * height * bytes_per_pixel;
		std::copy(data, data + sz, data_os);
		//std::fill(data_os, data_os + 100000, 255);

		//glClearColor(0.0, 0.0, 0.0, 0.0);
		//glClearDepth(10.0);

		glBindTexture(GL_TEXTURE_2D, textureId);


		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (GLvoid*)data_os);



		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(0, 0, 0);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(width, 0, 0);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(width, height, 0.0f);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(0, height, 0.0f);
		glEnd();
		//first += 1;
	}


}

void draw() {
   std::lock_guard<std::mutex> guard(draw_mutex);
   drawKinectData();
   glutSwapBuffers();
}

void idle() {
	glutPostRedisplay();
}


enum state_t { s_idle, s_processing_photo };

state_t state = s_idle;

void up_my_style(std::vector<unsigned char>& frame)
{

	auto detection = classify_image(&frame.front(), width, height);
	auto recommendation = get_recommendations(detection);


}

std::thread up_my_style_thread;

void on_up_my_style_clicked()
{

	if (state != s_idle)
		return;

	state = s_processing_photo;

	std::vector<unsigned char> fc;
	
	{
		std::lock_guard<std::mutex> guard(frame_mutex);

		fc = std::vector<unsigned char>(&raw_frame[0], &raw_frame[0] + width * height*bytes_per_pixel);
	}

	up_my_style_thread = std::thread(up_my_style, fc);

}

bool fullscreen = true;

void key_pressed(unsigned char key, int x, int y) {
	
	if (key == 'u')
	{
		on_up_my_style_clicked();
		return;
	}

	if (fullscreen)
	{
		glutReshapeWindow(800, 600);
		glutPositionWindow(0, 0);
		fullscreen = false;
	}
	else {
		glutFullScreen();
		fullscreen = false;
	}

}

int main(int argc, char* argv[]) {
    
	if (!init(argc, argv)) return 1;

    // Initialize textures
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (GLvoid*) data_os);
    glBindTexture(GL_TEXTURE_2D, 0);

    // OpenGL setup
    glClearColor(0,0,0,0);
    glClearDepth(1.0f);
    glEnable(GL_TEXTURE_2D);

    // Camera setup
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, 1, -1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();


	auto b = std::make_shared<button_t>(0.8, 0.05, 0.175, 3.0, &on_up_my_style_clicked, "Up My Style!");

	buttons.insert(b);
	clickables.insert(b);
	
	glutKeyboardFunc(key_pressed);

	glutFullScreen();

    // Main loop
	glutMainLoop();

	return 0;
}
