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
#include "product-button.h"

#include "classifier.h"
#include "recommendation.h"

#include "KinectJointFilter.h"

const int bytes_per_pixel = 4;
const int width = kinect_t::width;
const int height = kinect_t::height;

kinect_t kinect;

float cursor_x, cursor_y;
float hand_tip_x, hand_tip_y;

GLuint textureId;              // ID of the texture to contain Kinect RGB Data

std::vector<GLubyte> data(width*height * bytes_per_pixel);  // BGRA array containing the texture data
std::vector<GLubyte> last_frame(width*height * bytes_per_pixel);  // BGRA array containing the texture data
std::vector<GLubyte> frame(width*height * bytes_per_pixel);  // BGRA array containing the texture data

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

std::mutex gui_mutex;


std::set<std::shared_ptr<clickable_t> > clickables;
std::set<std::shared_ptr<button_t> > buttons;

std::shared_ptr<button_t> button_next;
std::shared_ptr<button_t> button_prev;

std::vector<std::shared_ptr<product_button_t>> product_buttons;

enum state_t { s_idle, s_processing_photo, s_recommendations_view };

volatile state_t state = s_idle;


bool init(int argc, char* argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(width, height);
    glutCreateWindow("MagicMirror");
    glutDisplayFunc(draw);
    glutIdleFunc(idle);
    return true;
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

float Scale(int maxPixel, float maxSkeleton, float position)
{
	float value = ((((maxPixel / maxSkeleton) / 2) * position) + (maxPixel / 2));
	if (value > maxPixel)
		return maxPixel;
	if (value < 0)
		return 0;
	return value;
}

Joint ScaleTo(Joint joint, int width, int height, float skeletonMaxX = 1.0, float skeletonMaxY = 1.0)
{
	Joint r = joint;

	r.Position.X = Scale(width, skeletonMaxX, joint.Position.X);
	r.Position.Y = Scale(height, skeletonMaxY, -joint.Position.Y);
	r.Position.Z = joint.Position.Z;

	return r;
}

// https://stackoverflow.com/questions/13313005/kinect-sdk-1-6-and-joint-scaleto-method

double ScaleY(Joint joint)
{
	double y = ((1080 / 0.4) * -joint.Position.Y) + (1920 / 2);
	return y;
}

void ScaleXY(Joint shoulderCenter, bool rightHand, Joint joint, float& scaledX, float& scaledY)
{
	double screenWidth = 1920;

	double x = 0;
	double y = ScaleY(joint);

	// if rightHand then place shouldCenter on left of screen
	// else place shouldCenter on right of screen
	if (rightHand)
	{
		x = (joint.Position.X - shoulderCenter.Position.X) * screenWidth * 2;
	}
	else
	{
		x = screenWidth - ((shoulderCenter.Position.X - joint.Position.X) * (screenWidth * 2));
	}


	if (x < 0)
	{
		x = 0;
	}
	else if (x > screenWidth - 5)
	{
		x = screenWidth - 5;
	}

	if (y < 0)
	{
		y = 0;
	}

	scaledX = x;
	scaledY = y;
}


std::vector<hand_tip_pos> hand_tip_trajectory;
volatile bool press_detected = false;

void detect_press()
{

	Joint handLeft = joints[JointType_HandTipLeft];
	Joint handRight = joints[JointType_HandTipRight];

	if (handLeft.TrackingState == TrackingState_NotTracked || handRight.TrackingState == TrackingState_NotTracked)
		return;

	// Select the hand that is closer to the sensor.
	Joint activeHand = handRight.Position.Z <= handLeft.Position.Z ? handRight : handLeft;
	bool right_hand = handRight.Position.Z <= handLeft.Position.Z;
	Joint scaled_active_hand = ScaleTo(activeHand, 1920, 1080);

	static const int cDepthWidth = 512;
	static const int cDepthHeight = 424;

	DepthSpacePoint depthPoint = { 0 };
	/*
	kinect.mapper->MapCameraPointToDepthSpace(scaled_active_hand.Position, &depthPoint);

	cursor_x = static_cast<float>(depthPoint.X * width) / cDepthWidth;
	cursor_y = static_cast<float>(depthPoint.Y * height) / cDepthHeight;

	*/

	//cursor_x = scaled_active_hand.Position.X;
	//cursor_y = scaled_active_hand.Position.Y;

	ScaleXY(joints[JointType_SpineShoulder], right_hand, activeHand, cursor_x, cursor_y);


	kinect.mapper->MapCameraPointToDepthSpace(activeHand.Position, &depthPoint);

	hand_tip_x = static_cast<float>(depthPoint.X * width) / cDepthWidth;
	hand_tip_y = static_cast<float>(depthPoint.Y * height) / cDepthHeight;

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


	std::chrono::steady_clock::duration jesture_start_min = std::chrono::milliseconds(500);
	std::chrono::steady_clock::duration jesture_start_max = std::chrono::milliseconds(100);

	std::chrono::time_point<std::chrono::steady_clock> earliest = p.time_point - jesture_start_min;
	std::chrono::time_point<std::chrono::steady_clock> latest = p.time_point - jesture_start_max;

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
			if (d > max_distance && i->y < p.y)
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

}

Sample::FilterDoubleExponential filter;



void get_body_data(IMultiSourceFrame* frame) {
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

			filter.Update(joints);
			auto filtered = filter.GetFilteredJoints();


			for (int j = 0;j < JointType_Count;j++)
			{
				joints[j].Position.X = DirectX::XMVectorGetX(filtered[j]);
				joints[j].Position.Y = DirectX::XMVectorGetY(filtered[j]);
				joints[j].Position.Z = DirectX::XMVectorGetZ(filtered[j]);
			}

			//body[i]->GetJoints(JointType_Count, joints);
			break;
		}
	}

	if (bodyframe) bodyframe->Release();
}

bool get_kinect_data(GLubyte* dest) {

	bool result = false;

	//IColorFrame* frame = NULL;
	IMultiSourceFrame* frame = NULL;

	
	if (SUCCEEDED(kinect.reader->AcquireLatestFrame(&frame))) {
		
		get_body_data(frame);

		IColorFrameReference * color_frame_ref;
		frame->get_ColorFrameReference(&color_frame_ref);
		IColorFrame * color_frame;
		color_frame_ref->AcquireFrame(&color_frame);
		
		if (color_frame_ref) color_frame_ref->Release();

		if (color_frame)
		{

			color_frame->CopyConvertedFrameDataToArray(width*height * bytes_per_pixel, dest, ColorImageFormat_Bgra);
			if (color_frame) color_frame->Release();
			result = true;

		}
		

	}
	
    if (frame) frame->Release();

	return result;
}

std::chrono::time_point<std::chrono::steady_clock> processing_photo_start;

void draw_kinect_data() {

	press_detected = false;

	bool r = get_kinect_data(&frame.front());

	if (r)

	{
		
		std::copy(frame.begin(), frame.end(), data.begin());

		{
			std::lock_guard<std::mutex> guard(frame_mutex);
			std::copy(frame.begin(), frame.end(), last_frame.begin());
		}
		
		
		detect_press();

		if (press_detected)
		{
			std::lock_guard<std::mutex> guard(gui_mutex);

			for (auto &c : clickables) {
				bool r = c->check_click(cursor_x, cursor_y);
				if (r)
					break;
			}

		}
		
	}
	else

	{
		std::copy(last_frame.begin(), last_frame.end(), data.begin());
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if(tracked != 0)
		seleleton_view.draw(&joints[0]);

	{
		std::lock_guard<std::mutex> guard(gui_mutex);

		for (auto &b : buttons) {
			b->check_highlighted(cursor_x, cursor_y);
			b->draw(rb);
		}
	}

	{

		agg::ellipse ell;
		int r = 10;
		auto color = agg::rgba(1.0, 1.0, 0.0, 0.3);
		ell.init(hand_tip_x, hand_tip_y, r, r, 50);
		agg::rasterizer_scanline_aa<> ras;
		agg::scanline_p8 sl;
		ras.add_path(ell);
		agg::render_scanlines_aa_solid(ras, sl, rb, color);
	}

	{

		agg::ellipse ell;
		int r = 30;
		auto color = agg::rgba(0.0, 1.0, 0.0, 0.5);
		if (press_detected)
		{
			r = 50;
			color = agg::rgba(1.0, 0.0, 0.0, 7.0);
		}

		ell.init(cursor_x, cursor_y, r, r, 50);
		agg::rasterizer_scanline_aa<> ras;
		agg::scanline_p8 sl;
		ras.add_path(ell);
		agg::render_scanlines_aa_solid(ras, sl, rb, color);
	}

	if (state == s_processing_photo)
	{

		auto passed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - processing_photo_start);

		agg::rasterizer_scanline_aa<> ras;
		agg::scanline_p8 sl;

		double al = 3.14 / 2;
		double ar = 300;

		double as = passed_ms.count() * 0.01;

		auto arc = agg::arc(width/2, height/2, ar, ar, as, as + al);

		agg::conv_stroke<agg::arc> p(arc);
		p.width(30.0);
		ras.add_path(p);

		auto color = agg::rgba(1.0, 1.0, 1.0, 0.8);
		agg::render_scanlines_aa_solid(ras, sl, rb, color);
		
		if ((passed_ms.count() / 300) % 2 == 0)
		{

			ras.reset();

			renderer_solid ren(rb);
			renderer_bin ren_bin(rb);

			std::string text = "Styling you up...";
			int text_height = 42;

			auto sz = text_size(ras, sl, ren, ren_bin, text.c_str(), text_height);

			int x = width / 2 - sz.first / 2;
			int y = height / 2 - sz.second / 2;

			draw_text(ras, sl, ren, ren_bin, x, y, agg::rgba(1.0, 1.0, 1.0, 7.0), text.c_str(), text_height);

		}

	}

	{


		//glClearColor(0.0, 0.0, 0.0, 0.0);
		//glClearDepth(10.0);

		glBindTexture(GL_TEXTURE_2D, textureId);


		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (GLvoid*)&data[0]);



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
   draw_kinect_data();
   glutSwapBuffers();
}

void idle() {
	glutPostRedisplay();
}

void on_product_clicked()
{


}


void up_my_style(std::vector<unsigned char>& frame)
{

	auto detection = classify_image(&frame.front(), width, height);
	auto recommendation = get_recommendations(detection);

	{
		std::vector<std::shared_ptr<product_button_t>> new_product_buttons;
		
		auto ri = recommendation.items.begin();

		for (int i = 0;i < 4;i++)
			for (int j = 0;j < 2 && ri != recommendation.items.end();j++, ri++)
			{
				float w = 0.085;
				float r = 1920. / 1080;
				float hs = 0.01;
				float vs = 0.08;
				float x = 0.3 + i * (w + hs);
				float y = 0.67 + j * (w + vs);

				auto pb = std::make_shared<product_button_t>(x, y, w, 1.0, &on_product_clicked, ri->picture_url);

				new_product_buttons.push_back(pb);

			}

		std::lock_guard<std::mutex> guard(gui_mutex);
		state = s_recommendations_view;
		buttons.insert(button_prev);
		buttons.insert(button_next);
		for (auto& pb : product_buttons) {
			buttons.erase(pb);
			clickables.erase(pb);
		}

		buttons.insert(new_product_buttons.begin(), new_product_buttons.end());
		clickables.insert(button_prev);
		clickables.insert(button_next);
		clickables.insert(new_product_buttons.begin(), new_product_buttons.end());

		product_buttons = new_product_buttons;

	}

}

std::thread up_my_style_thread;

void on_up_my_style_clicked()
{

	if (state == s_processing_photo)
		return;

	state = s_processing_photo;

	std::vector<unsigned char> fc;
	
	{
		std::lock_guard<std::mutex> guard(frame_mutex);

		fc = std::vector<unsigned char>(last_frame.begin(), last_frame.end());
	}

	processing_photo_start = std::chrono::steady_clock::now();
	if (up_my_style_thread.joinable())
		up_my_style_thread.join();
	up_my_style_thread = std::thread(up_my_style, fc);

}

void on_prev_clicked()
{
}

void on_next_clicked()
{
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
		fullscreen = true;
	}

}

int main(int argc, char* argv[]) {
    
	if (!init(argc, argv)) return 1;

    // Initialize textures
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (GLvoid*) &data.front());
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

	button_prev = std::make_shared<button_t>(0.3, 0.55, 0.18, 3.0, &on_prev_clicked, "Back");
	button_next = std::make_shared<button_t>(0.5, 0.55, 0.175, 3.0, &on_next_clicked, "Next");

	glutKeyboardFunc(key_pressed);

	glutFullScreen();

    // Main loop
	glutMainLoop();

	return 0;
}
