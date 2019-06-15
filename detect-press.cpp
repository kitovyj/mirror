#include <vector>
#include <algorithm>
#include <chrono> 

#include <Kinect.h>

#include "detect-press.h"

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

bool detect_press(kinect_t &kinect, Joint& hand)
{

	hand_tip_pos p;

	p.x = hand.Position.X;
	p.y = hand.Position.Y;
	p.z = hand.Position.Z;

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
			return true;
		}

	}

	return false;

}