/*
 * visualization_utils.cpp
 *
 *  Created on: Jul 8, 2009
 *      Author: sturm
 */

#include "vis_utils.h"
#include <point_cloud_mapping/geometry/areas.h>

using namespace ros;
using namespace std;

namespace planar_objects {

ros::Publisher* vis_utils_vis_pub;
ros::Publisher* vis_utils_cloud_pub;
roslib::Header vis_utils_header;

#define RETURN_RGB(r1,g1,b1) r=(int)(r1*255);g=(int)(g1*255);b=(int)(b1*255);

void setVisualization(ros::Publisher *visualization_pub_,
		ros::Publisher *cloud_pub_, roslib::Header header) {
	vis_utils_vis_pub = visualization_pub_;
	vis_utils_cloud_pub = cloud_pub_;
	vis_utils_header = header;
}

int HSV_to_RGB(float h, float s, float v) {
	// H is given on [0, 1] or UNDEFINED. S and V are given on [0, 1].
	// RGB are each returned on [0, 1].
	h -= floor(h);
	h *= 6;
	int i;
	float m, n, f;

	i = floor(h);
	f = h - i;
	if (!(i & 1))
		f = 1 - f; // if i is even
	m = v * (1 - s);
	n = v * (1 - s * f);
	int r, g, b;
	switch (i) {
	case 6:
	case 0:
	RETURN_RGB(v, n, m)
		;
		break;
	case 1:
	RETURN_RGB(n, v, m)
		;
		break;
	case 2:
	RETURN_RGB(m, v, n)
		;
		break;
	case 3:
	RETURN_RGB(m, n, v)
		;
		break;
	case 4:
	RETURN_RGB(n, m, v)
		;
		break;
	case 5:
	RETURN_RGB(v, m, n)
		;
		break;
	default:
	RETURN_RGB(1,0.5,0.5)
		;
	}
	int rgb;
	rgb = r << 16 | g << 8 | b;
	////        rgb=b;
	//        ROS_INFO("%f %d: %f %f %f; %d %d %d => %d",h,i,v,n,m,r,g,b,rgb);
	return (rgb);
}

float HSV_to_RGBf(float h, float s, float v) {
	int rgb = HSV_to_RGB(h, s, v);
	return (*(float*) &rgb);
}

float mix_color(float mix, float a, float b) {
	int ai = *(int*) &a;
	int bi = *(int*) &b;
	int ar = (ai >> 16) & 0xff;
	int ag = (ai >> 8) & 0xff;
	int ab = (ai >> 0) & 0xff;
	int br = (bi >> 16) & 0xff;
	int bg = (bi >> 8) & 0xff;
	int bb = (bi >> 0) & 0xff;
	int mr = (int) (mix * ar + (1 - mix) * br);
	int mg = (int) (mix * ag + (1 - mix) * bg);
	int mb = (int) (mix * ab + (1 - mix) * bb);
	int mi = (mr << 16) | (mg << 8) | mb;
	return (*(float*) &mi);
}

void visualizePlanes2(const sensor_msgs::PointCloud& cloud, std::vector<
		std::vector<int> >& plane_indices,
		std::vector<sensor_msgs::PointCloud>& plane_cloud, std::vector<
				std::vector<double> >& plane_coeff,
		std::vector<float>& plane_color, sensor_msgs::PointCloud& outside,
		bool convexHull) {
	sensor_msgs::PointCloud colored_cloud = cloud;

	int rgb_chann = -1;
	for (size_t i = 0; i < cloud.channels.size(); i++)
		if (cloud.channels[i].name == "rgb")
			rgb_chann = i;

	int rgb;
	if (rgb_chann != -1) {
		rgb = HSV_to_RGB(0.7, 0, 0.3);
		for (size_t i = 0; i < cloud.points.size(); i++) {
			colored_cloud.channels[rgb_chann].values[i] = mix_color(0.7,
					cloud.channels[rgb_chann].values[i], *(float*) &rgb);
		}

		for (size_t i = 0; i < plane_indices.size(); i++) {
			if (i < plane_color.size())
				rgb = *(int*) &plane_color[i];
			else
				rgb = HSV_to_RGB(i / (float) plane_indices.size(), 0.3, 1);
			for (size_t j = 0; j < plane_indices[i].size(); j++) {
				colored_cloud.channels[rgb_chann].values[plane_indices[i][j]]
						= mix_color(
								0.7,
								cloud.channels[rgb_chann].values[plane_indices[i][j]],
								*(float*) &rgb);
			}

			rgb = HSV_to_RGB(i / (float) plane_indices.size(), 0.5, 1);
			if (convexHull) {
				geometry_msgs::Polygon polygon;
				cloud_geometry::areas::convexHull2D(cloud, plane_indices[i],
						plane_coeff[i], polygon);
				visualizePolygon(cloud, polygon, rgb, i);
				cout << " sending convex hull " << endl;
			}
		}
	}

	vis_utils_cloud_pub->publish(colored_cloud);
}

void visualizePolygon(const sensor_msgs::PointCloud& cloud,
		geometry_msgs::Polygon &polygon, int rgb, int id) {
	visualization_msgs::Marker marker;
	marker.header = vis_utils_header;
	marker.ns = "polygon";
	marker.id = id;
	marker.type = visualization_msgs::Marker::LINE_STRIP;
	marker.action = visualization_msgs::Marker::ADD;
	marker.pose.orientation.w = 1.0;
	marker.scale.x = 0.002;
	marker.color.a = 1.0;
	marker.color.r = ((rgb >> 16) & 0xff) / 255.0;
	marker.color.g = ((rgb >> 8) & 0xff) / 255.0;
	marker.color.b = ((rgb >> 0) & 0xff) / 255.0;

	marker.set_points_size(polygon.get_points_size());

	for (size_t i = 0; i < polygon.get_points_size(); ++i) {
		marker.points[i].x = polygon.points[i].x;
		marker.points[i].y = polygon.points[i].y;
		marker.points[i].z = polygon.points[i].z;
	}
	//  ROS_INFO("visualization_marker polygon with n=%d points",polygon.get_points_size());
	vis_utils_vis_pub->publish(marker);
}

void visualizeLines(
		std::vector<std::pair<btVector3, btVector3> > lines, int id, double r,
		double g, double b, double scale) {
	// visualize edges in 3D
	visualization_msgs::Marker marker;
	marker.header = vis_utils_header;
	marker.ns = "edges";
	marker.id = id;
	marker.type = visualization_msgs::Marker::LINE_LIST;
	marker.action = lines.size() == 0 ? visualization_msgs::Marker::DELETE
			: visualization_msgs::Marker::ADD;
	marker.pose.orientation.w = 1.0;
	marker.scale.x = scale;
	marker.color.a = 1.0;
	marker.color.r = r;
	marker.color.g = g;
	marker.color.b = b;

	marker.set_points_size(lines.size() * 2);
	for (size_t i = 0; i < lines.size(); i++) {
		marker.points[i * 2 + 0].x = lines[i].first.x();
		marker.points[i * 2 + 0].y = lines[i].first.y();
		marker.points[i * 2 + 0].z = lines[i].first.z();
		marker.points[i * 2 + 1].x = lines[i].second.x();
		marker.points[i * 2 + 1].y = lines[i].second.y();
		marker.points[i * 2 + 1].z = lines[i].second.z();
	}
	vis_utils_vis_pub->publish(marker);
}

void visualizeLines(
		std::vector<std::pair<btVector3, btVector3> > lines, int id, int rgb,
		double scale) {
	visualizeLines(lines, id, (rgb & 0xff) / 255.0,
			((rgb >> 8) & 0xff) / 255.0, ((rgb >> 16) & 0xff) / 255.0, scale);
}

}
