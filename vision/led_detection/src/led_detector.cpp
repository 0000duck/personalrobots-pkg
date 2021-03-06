/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

//! \author Vijay Pradeep

#include <assert.h>

#include "led_detection/led_detector.h"

#include "opencv/cv.h"
#include "opencv/highgui.h"


using namespace led_detection ;
using namespace sensor_msgs ;
using namespace calibration_msgs ;
using namespace geometry_msgs ;

void poseToPixelCoords(const geometry_msgs::Pose& pose, const CameraInfo& info, ImagePoint& pix)
{
  assert(info.get_P_size() == 12) ;

  double point[4] = {pose.position.x, pose.position.y, pose.position.z, 1} ;
  double u = 0.0 ;
  double v = 0.0 ;
  double w = 0.0 ;

  for (int i=0; i< 4; i++)
  {
    u += info.P[i]   * point[i] ;
    v += info.P[i+4] * point[i] ;
    w += info.P[i+8] * point[i] ;
  }

  if (w < 1e-6)
    w = 1e-6 ;

  pix.x = u/w ;
  pix.y = v/w ;
}

// ******************** Led Detector ********************

LedDetector::LedDetector()
{
}

LedDetector::~LedDetector()
{

}

bool LedDetector::findLed(Image& image, const CameraInfo& info, const geometry_msgs::Pose* led_pose,
                            ImagePoint& led_pix, Image& debug_image)
{
  // Right now we're assuming that we're only going to be working in grayscale space.
  // However, it might make sense to work in some channel that matches the LED's color (ie. Red)
  if (!img_bridge_.fromImage(image, "mono8"))
  {
    ROS_ERROR("Error opening image") ;
    return false;                       // Got an error, so say we didn't find the led
  }

  // Convert the ROS Image into openCV's IPL image
  IplImage* cv_image = img_bridge_.toIpl() ;

  // Allocate an image to write debug data onto
  IplImage* cv_debug = cvCreateImage(cvGetSize(cv_image), IPL_DEPTH_8U, 3) ;

  // Actually do the openCV calls to find the LED
  findLed(cv_image, info, led_pose, led_pix, cv_debug) ;

  // Convert the debug image into a ROS Image
  if (cv_debug != NULL)
    img_bridge_.fromIpltoRosImage(cv_debug, debug_image) ;

  // Deallocate
  cvReleaseImage(&cv_debug) ;
  return true ;
}

bool LedDetector::findLed(const IplImage* image, const sensor_msgs::CameraInfo& info,
                          const geometry_msgs::Pose* led_pose,
                          calibration_msgs::ImagePoint& led_pix, IplImage* debug_image)
{
  IplImage* working = cvCloneImage(image) ;
  int smooth_type = CV_GAUSSIAN ;

  // Add a small gaussian blur to the image
  cvSmooth( working, working, smooth_type, 5, 5) ;

  // Determine where we expect to see the LED
  //! \todo Figure out how to use LED Prior in an effective way
  ImagePoint expected_loc ;
  if (led_pose)
    poseToPixelCoords(*led_pose, info, expected_loc) ;

  // Find the brightest pixel on the blurred image
  int max_x = 0 ;
  int max_y = 0 ;
  unsigned char max_val = 0 ;
  for (int i = 0; i < working->height; i++)
  {
    for (int j = 0; j < working->width; j++)
    {
      unsigned char val = working->imageData[i*working->widthStep + j] ;
      if (val > max_val)
      {
        max_val = val ;
        max_y = i ;
        max_x = j ;
      }
    }
  }

  // The the brightest pixel to be out LED
  led_pix.x = max_x ;
  led_pix.y = max_y ;

  if (debug_image != NULL)
  {
    cvCvtColor(image, debug_image, CV_GRAY2BGR) ;
    cvCircle(debug_image, cvPoint(led_pix.x, led_pix.y), 20, cvScalar(0,255,0), 1) ;
    if (led_pose)
      cvCircle(debug_image, cvPoint(expected_loc.x, expected_loc.y), 20, cvScalar(0,0,255), 1) ;
  }

  cvReleaseImage(&working) ;

  return true ;         // For now, assume that we always found the LED
}
