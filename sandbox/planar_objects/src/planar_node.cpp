/*
 * planar_node.cpp
 *
 *  Created on: Jul 7, 2009
 *      Author: sturm
 */
#include "assert.h"
#include "planar_node.h"
#include "find_planes.h"
#include "vis_utils.h"

#include "opencv_latest/CvBridge.h"
#include "opencv/cxcore.h"
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "vis_utils.h"

using namespace ros;
using namespace std;
using namespace robot_msgs;
using namespace vis_utils;

#define CVWINDOW(a) cvNamedWindow(a,CV_WINDOW_AUTOSIZE); cvMoveWindow(a,(cvWindows /3 )* 500,(cvWindows%3)*500); cvWindows++;
#define SQR(a) ((a)*(a))

#define RECT_MAX_SIZE 1.00
#define RECT_MIN_SIZE 0.10
#define RECT_MAX_DISPLACE 0.30

// Constructor
PlanarNode::PlanarNode() :
  sync_(&PlanarNode::syncCallback, this)
{
  int cvWindows = 0;
  CVWINDOW("left");
  //  CVWINDOW("right");
  CVWINDOW("disparity");

  CVWINDOW("occupied");
  CVWINDOW("distance");
  CVWINDOW("debug");
  //  CVWINDOW("free");
  //  CVWINDOW("unknown");


//  CVWINDOW("canny");
  //  CVWINDOW("free-dilated");
  //  CVWINDOW("canny-filtered");
  //
    CVWINDOW("canny-filtered-dilated");
  //  CVWINDOW("hough");


  nh_.param("~n_planes_max", n_planes_max_, 4);
  nh_.param("~point_plane_distance", point_plane_distance_, 0.015);

  // subscribe to topics
  cloud_sub_ = nh_.subscribe("stereo/cloud", 1, sync_.synchronize(&PlanarNode::cloudCallback, this));
  disp_sub_ = nh_.subscribe("stereo/disparity", 1, sync_.synchronize(&PlanarNode::dispCallback, this));
  dinfo_sub_ = nh_.subscribe("stereo/disparity_info", 1, sync_.synchronize(&PlanarNode::dinfoCallback, this));
  limage_sub_ = nh_.subscribe("stereo/left/image_rect", 1, sync_.synchronize(&PlanarNode::limageCallback, this));
  rimage_sub_ = nh_.subscribe("stereo/right/image_rect", 1, sync_.synchronize(&PlanarNode::rimageCallback, this));
  linfo_sub_ = nh_.subscribe("stereo/left/cam_info", 1, sync_.synchronize(&PlanarNode::linfoCallback, this));
  rinfo_sub_ = nh_.subscribe("stereo/right/cam_info", 1, sync_.synchronize(&PlanarNode::rinfoCallback, this));

  // advertise topics
  cloud_planes_pub_ = nh_.advertise<PointCloud> ("~planes", 1);
  cloud_outliers_pub_ = nh_.advertise<PointCloud> ("~outliers", 1);
  visualization_pub_ = nh_.advertise<visualization_msgs::Marker> ("visualization_marker", 1);

  currentTime = Time::now();
  lastTime = Time::now();
  lastDuration = Duration::Duration(0);
}

void PlanarNode::cloudCallback(const sensor_msgs::PointCloud::ConstPtr& point_cloud)
{
  cloud_ = point_cloud;
}

void PlanarNode::dinfoCallback(const sensor_msgs::DisparityInfo::ConstPtr& dinfo)
{
  dinfo_ = dinfo;
}

void PlanarNode::linfoCallback(const sensor_msgs::CamInfo::ConstPtr& linfo)
{
  linfo_ = linfo;
}

void PlanarNode::rinfoCallback(const sensor_msgs::CamInfo::ConstPtr& rinfo)
{
  rinfo_ = rinfo;
}

void PlanarNode::dispCallback(const sensor_msgs::Image::ConstPtr& disp_img)
{
  dimage_ = disp_img;

  if (dbridge_.fromImage(*dimage_))
  {
    // Disparity has to be scaled to be be nicely displayable
    IplImage* disp = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 1);
    cvCvtScale(dbridge_.toIpl(), disp, 4.0 / dinfo_->dpp);
    cvShowImage("disparity", disp);
    cvReleaseImage(&disp);
  }
}

void PlanarNode::limageCallback(const sensor_msgs::Image::ConstPtr& left_img)
{
  limage_ = left_img;

  if (lbridge_.fromImage(*limage_))
  {
    IplImage* disp = cvCreateImage(cvGetSize(lbridge_.toIpl()), IPL_DEPTH_8U, 1);
    cvShowImage("left", lbridge_.toIpl());
    cvReleaseImage(&disp);
  }
}

void PlanarNode::rimageCallback(const sensor_msgs::Image::ConstPtr& right_img)
{
  rimage_ = right_img;

  if (rbridge_.fromImage(*rimage_))
  {
    IplImage* disp = cvCreateImage(cvGetSize(rbridge_.toIpl()), IPL_DEPTH_8U, 1);
    cvShowImage("right", rbridge_.toIpl());
    cvReleaseImage(&disp);
  }
}

double inner_prod(std::vector<double> vec1, std::vector<double> vec2)
{
  double sum = 0;
  for (size_t i = 0; i < 3; i++)
  {
    sum += vec1[i] * vec2[i];
  }
  return (sum);
}

void PlanarNode::buildRP()
{
  double offx = 0;

  // reprojection matrix
  double Tx = rinfo_->P[0] / rinfo_->P[3];
  // first column
  RP[0] = 1.0;
  RP[4] = RP[8] = RP[12] = 0.0;

  // second column
  RP[5] = 1.0;
  RP[1] = RP[9] = RP[13] = 0.0;

  // third column
  RP[2] = RP[6] = RP[10] = 0.0;
  RP[14] = -Tx;

  // fourth column
  RP[3] = -linfo_->P[2]; // cx
  RP[7] = -linfo_->P[6]; // cy
  RP[11] = linfo_->P[0]; // fx, fy
  RP[15] = (linfo_->P[2] - rinfo_->P[2] - (double)offx) / Tx;

  for(int i=0;i<16;i++)
    P[i] = linfo_->P[i];

//  for(int i=0;i<16;i++)
//    printf("%d -- %f \n",i,P[i]);

}

btVector3 PlanarNode::calcPt(int x, int y, std::vector<double>& coeff)
{
  double cx = RP[3];
  double cy = RP[7];
  double f = RP[11];

  double ax = (double)x + cx;
  double ay = (double)y + cy;
  double aw = -coeff[3] / (ax * coeff[0] + ay * coeff[1] + f * coeff[2]);

  return (btVector3(ax * aw, ay * aw, f * aw));
}

bool sortCornersByAngle(const CornerCandidate& d1, const CornerCandidate& d2)
{
  return fabs(d1.angle - M_PI_2) < fabs(d2.angle - M_PI_2);
}

void PlanarNode::syncCallback()
{
  ROS_INFO("PlanarNode::syncCallback(), %d points in cloud",cloud_->get_pts_size());

  currentTime = Time::now();
  if (lastTime + lastDuration * 1.5 > currentTime)
  {
    ROS_INFO("skipping frame..");
    return;
  }

  buildRP();

  vector<PointCloud> plane_cloud;
  vector<vector<double> > plane_coeff;
  vector<vector<int> > plane_indices;
  PointCloud outside;

  find_planes::findPlanes(*cloud_, n_planes_max_, point_plane_distance_, plane_indices, plane_cloud, plane_coeff,
                          outside);

//  int backplane = 0; // assume that the largest plane belongs to the background
//  int frontplane = 1;
//  findFrontAndBackPlane(frontplane, backplane, plane_indices, plane_coeff);
//  visualizeFrontAndBackPlane(frontplane, backplane, *cloud_, plane_indices, plane_cloud, plane_coeff, outside, false);

  // occupied pixels in plane
  IplImage* pixOccupied = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 1);

  // free pixels in plane (because something sensed behind plane)
  IplImage* pixFree = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 1);

  // unknown/undefined pixels in plane
  IplImage* pixUnknown = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 1);

  // canny edge image of occupied pixels
  IplImage* pixDist= cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 1);

  pixDebug = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 3);
  cvCvtColor(lbridge_.toIpl(), pixDebug, CV_GRAY2BGR);

  for(size_t frontplane=1;frontplane<plane_coeff.size();frontplane++) {
    cvSetZero(pixOccupied);
    cvSetZero(pixFree);
    cvSetZero(pixUnknown);
    cvSetZero(pixDist);
//  int frontplane=1;
    find_planes::createPlaneImage(*cloud_, plane_indices[frontplane], plane_coeff[frontplane], pixOccupied, pixFree,
                                  pixUnknown);
    cvShowImage("occupied", pixOccupied);
    cvShowImage("free", pixFree);
    cvShowImage("unknown", pixUnknown);

    std::vector<CornerCandidate> corner;
    findCornerCandidates(pixOccupied, pixFree, pixUnknown, pixDist,plane_coeff[frontplane], corner);

    corner = groupCorners(corner, 20);

    findRectangles(corner, pixDist);

    corner = filterRectanglesBySupport(corner,pixOccupied, 0.50);

    cout << "corners in plane="<<frontplane<<" with high support: "<<corner.size()<<endl;;
    visualizeRectangles2d(corner);
    visualizeRectangles3d(corner,frontplane*1000);
    visualizeCorners(corner,frontplane*1000);
  }

  cvReleaseImage(&pixDist);
  cvReleaseImage(&pixOccupied);
  cvReleaseImage(&pixFree);
  cvReleaseImage(&pixUnknown);
  cvShowImage("debug", pixDebug);
  cvReleaseImage(&pixDebug);

  lastDuration = Time::now() - currentTime;
  lastTime = currentTime;

  ROS_INFO("processing took %g s",lastDuration.toSec());
}

bool PlanarNode::spin()
{
  while (nh_.ok())
  {
    int key = cvWaitKey(100) & 0x00FF;
    if (key == 27) //ESC
      break;

    ros::spinOnce();
  }

  return true;
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "planar_node");

  PlanarNode node;
  node.spin();

  return 0;
}

void PlanarNode::findFrontAndBackPlane(int& frontplane, int& backplane, std::vector<std::vector<int> >& plane_indices,
                                       vector<vector<double> >& plane_coeff)
{
  int n = plane_coeff.size();
  double ip3 = inner_prod(plane_coeff[backplane], plane_coeff[backplane]);
  ROS_INFO("backplane %d, ip %f", backplane, ip3);

  for (int i = 0; i < n; i++)
  {
    ROS_INFO ("plane %d: %d inliers: [%g, %g, %g, %g]", i, plane_indices[i].size(),
        plane_coeff[i][0], plane_coeff[i][1], plane_coeff[i][2], plane_coeff[i][3]);
    //    ROS_INFO("i=%d, prod=%f",i,inner_prod(plane_coeff[backplane],plane_coeff[i]));
  }

  double ip = inner_prod(plane_coeff[backplane], plane_coeff[frontplane]);
  int i = frontplane + 1;
  //  ROS_INFO("backplane %d, frontplane %d, ip %f", backplane, frontplane,ip);
  while (i < n)
  {
    double ip2 = inner_prod(plane_coeff[backplane], plane_coeff[i]);
    if (ip2 > ip && plane_coeff[backplane][3] > plane_coeff[i][3])
    {
      frontplane = i;
      ip = ip2;
    }
    //    ROS_INFO("backplane %d, frontplane %d, ip %f, i %d, ip2 %f", backplane, frontplane,ip,i,ip2);
    i++;
  }
  //  ROS_INFO("backplane %d, frontplane %d", backplane, frontplane);
}

void PlanarNode::visualizeFrontAndBackPlane(int frontplane, int backplane, const robot_msgs::PointCloud& cloud,
                                            std::vector<std::vector<int> >& plane_indices, std::vector<
                                                robot_msgs::PointCloud>& plane_cloud,
                                            std::vector<std::vector<double> >& plane_coeff,
                                            robot_msgs::PointCloud& outside, bool convexHull)
{
  std::vector<float> plane_color;
  plane_color.resize(plane_coeff.size());
  for (size_t i = 0; i < plane_coeff.size(); i++)
  {
    plane_color[i] = HSV_to_RGBf(0.7, 0, 0.3);
  }
  plane_color[backplane] = HSV_to_RGBf(0.3, 0.3, 1);
  plane_color[frontplane] = HSV_to_RGBf(0., 0.3, 1);
  vis_utils::visualizePlanes(*cloud_, plane_indices, plane_cloud, plane_coeff, plane_color, outside, cloud_planes_pub_,
                             visualization_pub_, convexHull);

}

void PlanarNode::findCornerCandidates(IplImage* pixOccupied, IplImage *pixFree, IplImage* pixUnknown,IplImage* &pixCannyDist,
                                      vector<double>& plane_coeff, std::vector<CornerCandidate> &corner)
{
  //  // occupied pixels in plane, distance transform
  //  IplImage* pixOccupiedDist = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 1);
  //
  //  cvNot(pixOccupied,pixOccupied);
  //  cvDistTransform(pixOccupied, pixOccupiedDist,CV_DIST_L1);
  //  cvNot(pixOccupied,pixOccupied);

  //  cvShowImage( "occupied-dist", pixOccupiedDist );

  //  IplImage* pixMerged = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 3);
  //  cvMerge(pixOccupied,pixFree,pixUnknown,NULL,pixMerged);
  //  cvShowImage( "merged", pixMerged);

  // dilate (increase) free pixels
  IplImage* pixFreeDilated = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 1);
  cvDilate(pixFree, pixFreeDilated, NULL, 10);
  cvShowImage("free-dilated", pixFreeDilated);

  // canny edge image of occupied pixels
  IplImage* pixCanny = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 1);
  cvCanny(pixOccupied, pixCanny, 50, 200, 3);
  cvShowImage("canny", pixCanny);

  // only keep edges that lie substantially close to free pixels
  IplImage* pixCannyFiltered = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 1);
  cvAnd(pixCanny, pixFreeDilated, pixCannyFiltered);
  cvShowImage("canny-filtered", pixCannyFiltered);

  // dilate filtered canny edge image..
  IplImage* pixCannyFilteredAndDilated = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 1);
  cvDilate(pixCannyFiltered, pixCannyFilteredAndDilated, NULL, 1);
  cvShowImage("canny-filtered-dilated", pixCannyFilteredAndDilated);

  // find edges via hough transform
  IplImage* pixHough = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 3);
  cvCvtColor(lbridge_.toIpl(), pixHough, CV_GRAY2BGR);

  // visualize edges in 2D
  CvMemStorage* storage = cvCreateMemStorage(0);
  CvSeq* lines = 0;
  lines = cvHoughLines2(pixCannyFilteredAndDilated, storage, CV_HOUGH_PROBABILISTIC, 1, CV_PI / 180, 5, 30, 15);

  for (int i = 0; i < lines->total; i++)
  {
    CvPoint* line = (CvPoint*)cvGetSeqElem(lines, i);
    cvLine(pixHough, line[0], line[1], CV_RGB(rand()%255,rand()%255,rand()%255), 1, 8);
  }

  // compute 3d lines
  std::vector<std::pair<btVector3, btVector3> > lines3d;
  std::vector<btVector3> linesVec;
  lines3d.resize(lines->total);
  linesVec.resize(lines->total);
  for (int i = 0; i < lines->total; i++)
  {
    CvPoint* line = (CvPoint*)cvGetSeqElem(lines, i);
    lines3d[i].first = calcPt(line[0].x, line[0].y, plane_coeff);
    lines3d[i].second = calcPt(line[1].x, line[1].y, plane_coeff);
    linesVec[i] = lines3d[i].second - lines3d[i].first;
  }
  //  visualizeLines(lines3d);

  // compute corner candidates
  btVector3 vecPlane(plane_coeff[0], plane_coeff[1], plane_coeff[2]);
  CornerCandidate candidate;
  candidate.RP = RP;
  candidate.P = P;
  for (size_t i = 0; i < lines3d.size(); i++)
  {
    for (size_t j = i + 1; j < lines3d.size(); j++)
    {
      CvPoint* line1 = (CvPoint*)cvGetSeqElem(lines, i);
      CvPoint* line2 = (CvPoint*)cvGetSeqElem(lines, j);
      double x1 = line1[0].x;
      double x2 = line1[1].x;
      double x3 = line2[0].x;
      double x4 = line2[1].x;

      double y1 = line1[0].y;
      double y2 = line1[1].y;
      double y3 = line2[0].y;
      double y4 = line2[1].y;

      // check whether parallel
      if ((y4 - y3) * (x2 - x1) == (x4 - x3) * (y2 - y1))
        continue;

      // compute intersection point
      double ua = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / ((y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1));
      double ub = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / ((y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1));
      candidate.x = x1 + ua * (x2 - x1);
      candidate.y = y1 + ua * (y2 - y1);

      if(candidate.x < 0) continue;
      if(candidate.y < 0) continue;
      if(candidate.x >=pixHough->width) continue;
      if(candidate.y >= pixHough->height) continue;

      candidate.w = 0.10;
      candidate.h = 0.10;

      btVector3 position = calcPt(candidate.x, candidate.y, plane_coeff);

      // ratio line distance/length
      candidate.dist1 = ua > 1.0 ? ua - 1.0 : (ua < 0.0 ? -ua : 0.0);
      candidate.dist2 = ub > 1.0 ? ua - 1.0 : (ub < 0.0 ? -ub : 0.0);

      double dist_threshold = 1; // intersection point not further away as length of line segment

      if (ua > 1.0 + dist_threshold)
        continue;
      if (ub > 1.0 + dist_threshold)
        continue;
      if (ua < 0.0 - dist_threshold)
        continue;
      if (ub < 0.0 - dist_threshold)
        continue;

      btVector3& vec1 = (lines3d[i].first.distance(position) > lines3d[i].second.distance(position) ? lines3d[i].first
          - position : lines3d[i].second - position).normalize();
      btVector3& vec2 = (lines3d[j].first.distance(position) > lines3d[j].second.distance(position) ? lines3d[j].first
          - position : lines3d[j].second - position).normalize();

      // compute angle
      candidate.angle = vec1.angle(vec2);
      //      cout << "vec1="<<vec1.x()<<" "<<vec1.y()<<" "<<vec1.z()<< endl;
      //      cout << "vec2="<<vec2.x()<<" "<<vec2.y()<<" "<<vec2.z()<< endl;
      //      cout <<"angle between vec1 and vec2: "<<(angle/M_PI*180.0)<<endl;

      // angle between 90deg +- 22.5deg?
      if (fabs(candidate.angle - M_PI_2) > M_PI / 8)
        continue;

      // correct error in vec1 and vec2 to achieve 90deg
      double s = vecPlane.dot(vec1.cross(vec2)) > 0 ? 1 : -1;
      if (s < 0)
      {
        btVector3 vTemp = vec2;
        vec2 = vec1;
        vec1 = vTemp;
      }
      btVector3 vec1corr = vec1.rotate(vecPlane, 1 * (candidate.angle - M_PI_2) / 2);
      btVector3 vec2corr = vec2.rotate(vecPlane, -1 * (candidate.angle - M_PI_2) / 2);
      //      cout <<"angle between vec1corr and vec2corr: "<<(vec1corr.angle(vec2corr)/M_PI*180.0)<<" s="<<s<<endl;

      // orientation
      btQuaternion orientation;
      btMatrix3x3 rotation;
      rotation[0] = vec1corr.rotate(vecPlane, 0); // x
      rotation[1] = vec2corr.rotate(vecPlane, 0); // y
      rotation[2] = vecPlane; // z
      rotation = rotation.transpose();
      rotation.getRotation(orientation);
      candidate.tf = btTransform(orientation, position);

      corner.push_back(candidate);
    }
  }
  cout << "corner candidates: " << (corner.size()) << endl;
  if (corner.size() > 500)
    return;

  // sort by angle
  std::sort(corner.begin(), corner.end(), sortCornersByAngle);
  //  for(size_t i=0;i<MIN(1,corner.size());i++) {
  //    cout << "angle: "<<(corner[i].second/M_PI*180.0)<<endl;
  //    // show frames
  //    char buf[50];
  //    sprintf(buf,"x%d",i);
  //    tf::Stamped<tf::Pose> table_pose_frame(corner[i].first, cloud_->header.stamp, buf, cloud_->header.frame_id);
  //    broadcaster_.sendTransform(table_pose_frame);
  //  }

  cvClearMemStorage(storage);
  cvReleaseMemStorage(&storage);
  cvShowImage("hough", pixHough);

  cvNot(pixCannyFiltered, pixCannyFiltered);
  cvDistTransform(pixCannyFiltered, pixCannyDist, CV_DIST_L1);

  cvShowImage("distance", pixCannyDist);

  //  cvSaveImage("/tmp/pixOccupied.png",pixOccupied);
  //  cvSaveImage("/tmp/pixFree.png",pixFree);
  //  cvSaveImage("/tmp/pixUnknown.png",pixUnknown);
  //  cvSaveImage("/tmp/pixCanny.png",pixCanny);
  //  cvSaveImage("/tmp/pixFreeDilated.png",pixFreeDilated);
  //  cvSaveImage("/tmp/pixCannyFiltered.png",pixCannyFiltered);
  //  cvSaveImage("/tmp/pixCannyFilteredAndDilated.png",pixCannyFilteredAndDilated);
  //  cvSaveImage("/tmp/pixHough.png",pixOccupied);

  //  cvReleaseImage(&pixOccupiedDist);
  //  cvReleaseImage(&pixOccupied);
  //  cvReleaseImage(&pixFree);
  //  cvReleaseImage(&pixUnknown);
//  cvReleaseImage(&pixCanny);
  cvReleaseImage(&pixFreeDilated);
  cvReleaseImage(&pixCannyFiltered);
  cvReleaseImage(&pixCannyFilteredAndDilated);
  cvReleaseImage(&pixHough);
}

void PlanarNode::visualizeLines(std::vector<std::pair<btVector3, btVector3> > lines, int id, double r, double g,
                                double b)
{
  // visualize edges in 3D
  visualization_msgs::Marker marker;
  marker.header.frame_id = cloud_->header.frame_id;
  marker.header.stamp = ros::Time((uint64_t)0ULL);
  marker.ns = "edges";
  marker.id = id;
  marker.type = visualization_msgs::Marker::LINE_LIST;
  marker.action = lines.size()==0?visualization_msgs::Marker::DELETE:visualization_msgs::Marker::ADD;
  marker.pose.orientation.w = 1.0;
  marker.scale.x = 0.002;
  marker.color.a = 1.0;
  marker.color.r = r;
  marker.color.g = g;
  marker.color.b = b;

  marker.set_points_size(lines.size() * 2);
  for (size_t i = 0; i < lines.size(); i++)
  {
    marker.points[i * 2 + 0].x = lines[i].first.x();
    marker.points[i * 2 + 0].y = lines[i].first.y();
    marker.points[i * 2 + 0].z = lines[i].first.z();
    marker.points[i * 2 + 1].x = lines[i].second.x();
    marker.points[i * 2 + 1].y = lines[i].second.y();
    marker.points[i * 2 + 1].z = lines[i].second.z();
  }
  visualization_pub_.publish(marker);
}

std::vector<CornerCandidate> PlanarNode::groupCorners(std::vector<CornerCandidate> &corner, double group_dist)
{
  // group within distance
  std::vector<std::vector<CornerCandidate> > grouped_corners;
  for (size_t i = 0; i < corner.size(); i++)
  {
    std::vector<CornerCandidate> vec;
    vec.push_back(corner[i]);
    grouped_corners.push_back(vec);
  }
  bool changed = true;
  while (changed)
  {
    changed = false;
    for (size_t i = 0; i < grouped_corners.size(); i++)
    {
      for (size_t j = i + 1; j < grouped_corners.size(); j++)
      {
        // check whether these two groups are close enough --> merge groups and break loop
        for (size_t a = 0; a < grouped_corners[i].size(); a++)
        {
          for (size_t b = 0; b < grouped_corners[j].size(); b++)
          {
            //            cout << grouped_corners[i][a].x<<";"<<grouped_corners[i][a].y<<"   "<<grouped_corners[j][b].x <<";"<<grouped_corners[j][b].y << endl;
            if (SQR(grouped_corners[i][a].x - grouped_corners[j][b].x)
                + SQR(grouped_corners[i][a].y - grouped_corners[j][b].y) < SQR(group_dist))
            {
              // merge groups
              //              cout << "merging.." << endl;
              grouped_corners[i].insert(grouped_corners[i].end(), grouped_corners[j].begin(), grouped_corners[j].end());
              grouped_corners.erase(grouped_corners.begin() + j);
              changed = true;
              j--;
              break;
            }
          }
          if (changed)
            break;
        }
      }
    }
  }

//  // filter groups with low support
//  size_t min_support = 1;
//  changed = true;
//  while (changed)
//  {
//    changed = false;
//    for (size_t i = 0; i < grouped_corners.size(); i++)
//    {
//      if (grouped_corners[i].size() < min_support)
//      {
//        grouped_corners.erase(grouped_corners.begin() + i);
//        changed = true;
//        break;
//      }
//    }
//  }

  cout << "corner groups: " << (grouped_corners.size()) << endl;
  std::vector<CornerCandidate> result;
//  result.resize(MIN(1,grouped_corners.size()));
  result.resize(grouped_corners.size());

  for (size_t i = 0; i < result.size(); i++)
  {
    result[i] = grouped_corners[i][0];

    btQuaternion o = grouped_corners[i][0].tf.getRotation();
    btVector3 t = grouped_corners[i][0].tf.getOrigin();

    for (size_t j = 1; j < grouped_corners[i].size(); j++)
    {
      o = o.slerp(grouped_corners[i][j].tf.getRotation(), 1 / (double)grouped_corners[i].size());
      t = t.lerp(grouped_corners[i][j].tf.getOrigin(), 1 / (double)grouped_corners[i].size());
    }

    result[i].tf = btTransform(o, t);
  }
  //  for (size_t i = 0; i < MIN(4,grouped_corners.size()); i++)
  //  {
  //    char buf[50];
  //    sprintf(buf, "x%d", i);
  //    broadcaster_.sendTransform(grouped_corners[i][0].tf, ros::Time::now(), buf, "/stereo");
  //  }
  return result;
}

void PlanarNode::visualizeCorners(std::vector<CornerCandidate> &corner, int id)
{
  std::vector<std::pair<btVector3, btVector3> > lines;

  lines.resize(corner.size());
  for (int j = 0; j < 3; j++)
  {
    btVector3 vec(j == 0 ? 1.00 : 0.00, j == 1 ? 1.00 : 0.00, j == 2 ? 1.00 : 0.00);
    for (size_t i = 0; i < corner.size(); i++)
    {
      lines[i].first = corner[i].tf.getOrigin();
      lines[i].second = corner[i].tf * (vec * 0.05);
    }
    visualizeLines(lines, id+10 + j, j == 0 ? 1.00 : 0.00, j == 1 ? 1.00 : 0.00, j == 2 ? 1.00 : 0.00);
  }
}


void PlanarNode::visualizeRectangles3d(std::vector<CornerCandidate> &corner,int id)
{
  std::vector<std::pair<btVector3, btVector3> > lines;

  lines.resize(4);
  for (size_t i = 0; i <MIN(100, corner.size()); i++)
  {
    corner[i].updatePoints3d();
    lines[0].first = corner[i].points3d[0];
    lines[0].second = corner[i].points3d[1];

    lines[1].first = corner[i].points3d[1];
    lines[1].second = corner[i].points3d[2];

    lines[2].first = corner[i].points3d[2];
    lines[2].second = corner[i].points3d[3];

    lines[3].first = corner[i].points3d[3];
    lines[3].second = corner[i].points3d[0];

    visualizeLines(lines, id+100 + i, MIN(i*0.1,1.00), MAX(0.00, 1.00-i*.01), 0.00);
  }
  lines.resize(0);
  for(size_t i=corner.size();i<100;i++)
    visualizeLines(lines,id+100+i);
}

void PlanarNode::visualizeRectangles2d(std::vector<CornerCandidate> &corner) {
  for(size_t i=0;i<corner.size();i++) {
    visualizeRectangle2d(corner[i]);
  }
}

void PlanarNode::visualizeRectangle2d(CornerCandidate &corner) {
  corner.updatePoints3d();
  corner.updatePoints2d();

  for(int j=0;j<4;j++) {
    cvLine(pixDebug, corner.points2d[j], corner.points2d[(j+1)%4],
            CV_RGB(MIN(j*50,255),MAX(0,255-j*50),0), 1, 8);
//      cout << "j="<<j<<" x="<< corner[i].points2d[j].x <<" y="<<corner[i].points2d[j].y << endl;
  }
}

void PlanarNode::findRectangles(std::vector<CornerCandidate> &corner, IplImage* pixDist)
{

  for(size_t i=0;i<corner.size();i++) {
    initializeRectangle(corner[i],pixDist);
  }

//  for(size_t i=0;i<corner.size();i++) {
//    optimizeRectangle(corner[i],pixCannyDist);
//  }

//  cvReleaseImage(&pixDist);
}

void PlanarNode::initializeRectangle(CornerCandidate &corner, IplImage* pixDist) {
  corner.w = RECT_MIN_SIZE;
  corner.h = RECT_MIN_SIZE;

  for(int i=1;i<4;i++) {
    visualizeRectangle2d(corner);
    corner.optimizeWidth(pixDist, -(RECT_MAX_SIZE-RECT_MIN_SIZE)/(2), (RECT_MAX_SIZE-RECT_MIN_SIZE)/(2),100 );
    visualizeRectangle2d(corner);
    corner.optimizeHeight(pixDist, -(RECT_MAX_SIZE-RECT_MIN_SIZE)/(2), (RECT_MAX_SIZE-RECT_MIN_SIZE)/(2),100 );
    visualizeRectangle2d(corner);
    corner.optimizePhi(pixDist, -M_PI/(4), +M_PI/(4),100 );
    visualizeRectangle2d(corner);
    corner.optimizeX(pixDist, -RECT_MAX_DISPLACE/(2),RECT_MAX_DISPLACE/(2),20 );
    visualizeRectangle2d(corner);
    corner.optimizeY(pixDist, -RECT_MAX_DISPLACE/(2),RECT_MAX_DISPLACE/(2),20 );
    visualizeRectangle2d(corner);
  }
}

void CornerCandidate::updatePoints3d()
{
  points3d[0] = tf * btVector3(0,0,0);
  points3d[1] = tf * btVector3(w,0,0);
  points3d[2] = tf * btVector3(w,h,0);
  points3d[3] = tf * btVector3(0,h,0);

//  for(int i=0;i<4;i++) {
//    cout << "X="<<points3d[i].x()<<" "<< "Y="<<points3d[i].y()<<" "<< "Z="<<points3d[i].z()<<" "<<endl;
//  }
}

void CornerCandidate::updatePoints2d()
{
  for(int i=0;i<4;i++) {
    points2d[i].x = (P[0]*points3d[i].x() + P[1]*points3d[i].y() + P[2]*points3d[i].z() + P[3])
          / (P[8]*points3d[i].x() + P[9]*points3d[i].y() + P[10]*points3d[i].z() + P[11]);
    points2d[i].y = (P[4]*points3d[i].x() + P[5]*points3d[i].y() + P[6]*points3d[i].z() + P[7])
          / (P[8]*points3d[i].x() + P[9]*points3d[i].y() + P[10]*points3d[i].z() + P[11]);

  }
}

double CornerCandidate::computeDistance(IplImage* distImage) {
  updatePoints3d();
  updatePoints2d();

  bool invalidPoints = false;
  for(int i=0;i<4;i++) {
    if(points2d[i].x < 0) invalidPoints = true;
    if(points2d[i].y < 0) invalidPoints = true;
    if(points2d[i].x >= distImage->width) invalidPoints = true;
    if(points2d[i].y >= distImage->height) invalidPoints = true;
  }
  if(invalidPoints)
    return DBL_MAX;

  int global_sum=0;
  int global_count = 0;
  for(int i=0;i<4;i++) {
    CvLineIterator iterator;

    int sum = 0;
    int count = cvInitLineIterator( distImage, points2d[i], points2d[(i+1)%4], &iterator, 8, 0 );
    for( int j = 0; j < count; j++ ){
      sum += iterator.ptr[0];
      CV_NEXT_LINE_POINT(iterator);
    }
    global_sum += sum;
    global_count += count;
  }
//  cout <<"global_sum="<<global_sum<<" global_count="<<global_count<<endl;
  return(global_sum / (double)global_count);
}


void CornerCandidate::optimizeWidth(IplImage* distImage, double a,double b,int steps) {
  double best_d=DBL_MAX;
  double best_w=w;
  double d;
  double start = w;
  for(int i=0;i<steps;i++) {
    w = MIN(RECT_MAX_SIZE,MAX( RECT_MIN_SIZE,start + a + ((b-a)*(i))/(double)steps));
    d = computeDistance(distImage);
//    cout << "i="<<i<<" steps="<<steps<< " w="<<w<<" d="<<d << endl;
    if(d < best_d) {
      best_d = d;
      best_w = w;
    }
  }
//  cout << "found w="<< best_w << endl;

  w = best_w;
}


void CornerCandidate::optimizeHeight(IplImage* distImage, double a,double b,int steps) {
  double best_d=DBL_MAX;
  double best_h=h;
  double d;
  double start = h;
  for(int i=0;i<steps;i++) {
    h = MIN(RECT_MAX_SIZE,MAX( RECT_MIN_SIZE,start + a + ((b-a)*(i))/(double)steps));
    d = computeDistance(distImage);
    if(d < best_d) {
      best_d = d;
      best_h = h;
    }
  }
  h = best_h;
}

void CornerCandidate::optimizePhi(IplImage* distImage, double a,double b,int steps) {
  double best_d=DBL_MAX;
  int best_i=0;
  double d;

  btTransform org_tf = tf;

  for(int i=0;i<steps;i++) {
    tf = org_tf * btTransform( btQuaternion( btVector3(0,0,1), a + ((b-a)*(i))/(double)steps ));
    d = computeDistance(distImage);
    if(d < best_d) {
      best_d = d;
      best_i = i;
    }
  }
  tf = org_tf * btTransform( btQuaternion( btVector3(0,0,1), a + ((b-a)*(best_i))/(double)steps ));
}

void CornerCandidate::optimizeX(IplImage* distImage, double a,double b,int steps) {
  double best_d=DBL_MAX;
  int best_i=0;
  double d;

  btTransform org_tf = tf;

  for(int i=0;i<steps;i++) {
    tf = org_tf * btTransform( btQuaternion::getIdentity(), btVector3( a + ((b-a)*(i))/(double)steps ,0,0) );
    d = computeDistance(distImage);
//    cout << "i="<<i<<" steps="<<steps<< " x+="<<a + (((b-a)*(i))/(double)steps)<<" d="<<d
//    <<  " x="<<tf.getOrigin().x()
//<<  " y="<<tf.getOrigin().y()
//<<  " z="<<tf.getOrigin().z()
//    <<endl;
    if(d < best_d) {
      best_d = d;
      best_i = i;
    }
  }
  tf = org_tf * btTransform( btQuaternion::getIdentity(), btVector3( a + ((b-a)*(best_i))/(double)steps ,0,0) );
}

void CornerCandidate::optimizeY(IplImage* distImage, double a,double b,int steps) {
  double best_d=DBL_MAX;
  int best_i=0;
  double d;

  btTransform org_tf = tf;

  for(int i=0;i<steps;i++) {
    tf = org_tf * btTransform( btQuaternion::getIdentity(), btVector3( 0, a + ((b-a)*(i))/(double)steps ,0) );
    d = computeDistance(distImage);
    if(d < best_d) {
      best_d = d;
      best_i = i;
    }
  }
  tf = org_tf * btTransform( btQuaternion::getIdentity(), btVector3( 0,a + ((b-a)*(best_i))/(double)steps ,0) );
}

double CornerCandidate::computeSupport(IplImage* pixOccupied,IplImage* pixDebug) {
  std::map<int,std::map<int,bool> > rect;

  for(int i=0;i<4;i++) {
    CvLineIterator iterator;

    int count = cvInitLineIterator( pixOccupied, points2d[i], points2d[(i+1)%4], &iterator, 8, 0 );
    for( int j = 0; j < count; j++ ){
      int y = (iterator.ptr - (uchar*)pixOccupied->imageData) / pixOccupied->widthStep;
      int x = ((iterator.ptr - (uchar*)pixOccupied->imageData) % pixOccupied->widthStep)/pixOccupied->nChannels;
      rect[x][y] = true;
      CV_NEXT_LINE_POINT(iterator);
    }
  }

  int count=0;
  int sum=0;
  std::map<int,std::map<int,bool> >::iterator i;
  for(i=rect.begin();i!=rect.end();i++) {
    int x = i->first;
    int y1 = i->second.begin()->first;
    int y2 = i->second.rbegin()->first;
    for(int y=y1;y<=y2;y++) {
      if(pixOccupied->imageData[ y*pixOccupied->widthStep + x*pixOccupied->nChannels ]!=0)
        sum++;
      if(pixDebug!=NULL)
        pixDebug->imageData[ y*pixDebug->widthStep + x*pixDebug->nChannels ] = 255;
    }
    count += y2-y1+1;
  }
  return(sum / (double)count);
}

std::vector<CornerCandidate> PlanarNode::filterRectanglesBySupport(std::vector<CornerCandidate> &corner, IplImage* pixOccupied, double min_support) {
  std::vector<CornerCandidate> result;
  for(size_t i=0;i<corner.size();i++) {
    if(corner[i].computeSupport(pixOccupied) >= min_support)
      result.push_back(corner[i]);
  }
  return(result);
}
