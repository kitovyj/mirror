#pragma once

#include <Windows.h>
#include <Ole2.h>

#include <Kinect.h>

#include <stdexcept>

struct kinect_t {

	static const int width = 1920;
	static const int height = 1080;

	IKinectSensor* sensor;         // Kinect sensor
	IMultiSourceFrameReader* reader;   // Kinect data source
	ICoordinateMapper* mapper;         // Converts between depth, color, and 3d coordinates

	kinect_t() {

		if (FAILED(GetDefaultKinectSensor(&sensor))) {
			throw std::runtime_error("can't initialize senser");
		}
		if (sensor) {
			sensor->get_CoordinateMapper(&mapper);

			sensor->Open();
			sensor->OpenMultiSourceFrameReader(
				FrameSourceTypes::FrameSourceTypes_Depth | FrameSourceTypes::FrameSourceTypes_Color | FrameSourceTypes::FrameSourceTypes_Body,
				&reader);

		}
		else {
			throw std::runtime_error("can't initialize senser");
		}


	}

	std::pair<float, float> joint_screen_pos(Joint& j) {

		DepthSpacePoint depthPoint = { 0 };
		mapper->MapCameraPointToDepthSpace(j.Position, &depthPoint);

		static const int        cDepthWidth = 512;
		static const int        cDepthHeight = 424;

		float x = static_cast<float>(depthPoint.X * width) / cDepthWidth;
		float y = static_cast<float>(depthPoint.Y * height) / cDepthHeight;

		return std::make_pair(x, y);
	}

};
