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

#include <cstdio>
#include <vector>
#include <map>
#include "opencv/cxcore.h"
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "ros/node.h"
#include "boost/thread/mutex.hpp"
#include "deprecated_msgs/ImageArray.h"
#include "image_utils/cv_bridge.h"

#include <sys/stat.h>

#include "color_calib.h"

using namespace std;
using namespace color_calib;

class ColorCalib : public ros::Node
{
public:
  deprecated_msgs::ImageArray image_msg;

  boost::mutex cv_mutex;

  bool first;

  Calibration color_cal;

  ColorCalib() : Node("color_calib", ros::Node::ANONYMOUS_NAME), first(true), color_cal(this)
  { 
    subscribe("images", image_msg, &ColorCalib::image_cb, 1);
  }

  void image_cb()
  {
    if (!first) 
      return;
    else
      first = false;

    boost::mutex::scoped_lock lock(cv_mutex);

    for (uint32_t i = 0; i < image_msg.get_images_size(); i++)
    {
      string l = image_msg.images[i].label;

      if (image_msg.images[i].colorspace == std::string("rgb24"))
      {
        CvBridge<deprecated_msgs::Image>* cv_bridge = new CvBridge<deprecated_msgs::Image>(&image_msg.images[i], CvBridge<deprecated_msgs::Image>::CORRECT_BGR | CvBridge<deprecated_msgs::Image>::MAXDEPTH_8U);
        IplImage* img;

        if (cv_bridge->to_cv(&img))
        {
          IplImage* img2 = cvCreateImage(cvGetSize(img), IPL_DEPTH_32F, 3);

          decompand(img, img2);

          find_calib(img2, color_cal, COLOR_CAL_BGR | COLOR_CAL_COMPAND_DISPLAY);
          
          IplImage* corrected_img = cvCreateImage(cvGetSize(img), IPL_DEPTH_32F, 3);

          cvTransform(img2, corrected_img, color_cal.getCal(COLOR_CAL_BGR));

          color_cal.setParam(mapName("images") + std::string("/") + l);

          cvNamedWindow("color_rect", CV_WINDOW_AUTOSIZE);
          cvShowImage("color_rect", corrected_img);

          cvReleaseImage(&img);
          cvReleaseImage(&img2);
        }

        delete cv_bridge;
      }
    }
  }

  void check_keys() 
  {
    boost::mutex::scoped_lock lock(cv_mutex);
    if (cvWaitKey(3) == 10)
      shutdown();
  }

};

int main(int argc, char **argv)
{
  ros::init(argc, argv);
  ColorCalib view;
  while (view.ok()) 
  {
    usleep(10000);
    view.check_keys();
  }
  
  return 0;
}
