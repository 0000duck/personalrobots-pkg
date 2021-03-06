#include <ros/node.h>
#include "boost/thread/mutex.hpp"
#include <iostream>
#include <fstream>
#include "opencv/cxcore.h"
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "deprecated_msgs/ImageArray.h"
#include "sensor_msgs/PointCloud.h"
#include "std_msgs/String.h"
#include "image_utils/cv_bridge.h"
#include <time.h>
#include "rosrecord/Player.h"

#include <set>


using namespace std;

set<string> g_names;

void SplitFilename (const string& str, string *dir, string *file)
{
  size_t found;
  found=str.find_last_of("/\\");
  *file = str.substr(found+1);
  *dir = str.substr(0,found);

  //If there was no leading directory.
  if(found == str.npos) {
     *file = *dir;
     *dir = string(".");
   }

}

template <class T>
void copyMsg(string name, ros::Message* m, ros::Time t, ros::Time t_no_use, void* n)
{
  if (m != 0) {
    g_names.insert(name);
    *((T*)(n)) = *((T*)(m));
  }
}
  

struct imgData
{
  string label;
  IplImage *cv_image;
  CvBridge<deprecated_msgs::Image> *bridge;
};


class calib_converter
{
public:
  

  boost::mutex cv_mutex;  
  ros::record::Player lp;
  map<string, imgData> images;
  deprecated_msgs::ImageArray image_msg;
  std_msgs::String calparams;
  sensor_msgs::PointCloud cloud;
  string fullname;

  calib_converter(string bag) : fullname(bag)
  {}


  int run_conversion() {

    // -- Get the filenames to save to.
    string dir, filename;
    SplitFilename(fullname, &dir, &filename);
    //cout << " dir: " << dir << "    filename: " << filename << endl;
    string num = filename.substr(0, filename.find_first_of("-"));
    string filebase = filename.substr(0, filename.find_first_of("."));
    string outputdir = dir; // + string("/") + filebase; 
    mkdir(outputdir.c_str(), 0755);


    // -- Load the messages.
    lp.open(fullname, ros::Time());
    lp.addHandler<deprecated_msgs::ImageArray>(string("videre/images"), &copyMsg<deprecated_msgs::ImageArray>, (void*)(&image_msg), true);
    lp.addHandler<std_msgs::String>(string("videre/cal_params"), &copyMsg<std_msgs::String>, (void*)(&calparams), true);
    lp.addHandler<sensor_msgs::PointCloud>(string("full_cloud"), &copyMsg<sensor_msgs::PointCloud>, (void*)(&cloud), true);
    while(lp.nextMsg());
    

    // -- If we don't have all the expected messages, yell.
    bool hasImages = false, hasPtcld = false, hasCalparams = false;

    for (set<string>::iterator i = g_names.begin(); i != g_names.end(); i++)
    { 
      if(*i == "videre/images") {
	hasImages = true;
      }
      else if (*i == "videre/cal_params") {
	hasCalparams = true;
      }
      else if(*i == "full_cloud") {
	hasPtcld = true;
      }
    }
    if(!hasImages || !hasPtcld || !hasCalparams) {
      cerr << fullname << " does not have the required message types!  Note that this check is based on topic name, not message type; yell at Alex if this is the problem." << endl;
      cerr << "Doing as much conversion as possible..." << endl;
    }
    
    string outputname;

    // -- Save the pointcloud.
    if(hasPtcld) {
      outputname = outputdir + string("/") + num + string("-J.xml");
      cout << "Saving pointcloud of " << cloud.get_points_size() << " points to " << outputname << "..." << endl;
    
      CvMat *M = cvCreateMat(cloud.get_points_size(), 4, CV_64FC1);
      for(unsigned int i=0; i<cloud.get_points_size(); i++) {
	cvmSet(M, i, 0, cloud.points[i].x);
	cvmSet(M, i, 1, cloud.points[i].y);
	cvmSet(M, i, 2, cloud.points[i].z);
	cvmSet(M, i, 3, cloud.channels[0].values[i]);
      }
      cvSave(outputname.c_str(), M);
      cout << " done." << endl;
    }
    
    // -- Save the camera calibration information file.
    if(hasCalparams) {
      outputname = outputdir + string("/") + num + string("-cal_params.txt");
      cout << "Saving cal_params to " << outputname << "..." << endl;
      ofstream f(outputname.c_str());
      f << calparams.data;
      f.close();
      cout << " done." << endl;
    }

    if(hasImages) {
      // -- For each image in the message.
      cv_mutex.lock();
      for (uint32_t i = 0; i < image_msg.get_images_size(); i++)
	{
	  // -- Get the image into openCV and display.
	  string l = image_msg.images[i].label;
	  map<string, imgData>::iterator j = images.find(l);

	  if (j == images.end())
	    {
	      images[l].label = image_msg.images[i].label;
	      images[l].bridge = new CvBridge<deprecated_msgs::Image>(&image_msg.images[i], CvBridge<deprecated_msgs::Image>::CORRECT_BGR | CvBridge<deprecated_msgs::Image>::MAXDEPTH_8U);
	      cvNamedWindow(l.c_str(), CV_WINDOW_AUTOSIZE);
	      images[l].cv_image = 0;
	    } 
      
	  j = images.find(l);      
	  j->second.bridge->to_cv(&j->second.cv_image);

	  // -- Make sure we've got the right image types.
	  if(l.compare(string("left_rectified")) != 0 && l.compare(string("right_rectified")) != 0) {
	    cerr << fullname << " has something other than a left and right rectified image in it!" << endl;
	  }
	
	  // -- Save the image.
	  if(l.find(string("left")) != l.npos)
	    outputname = outputdir + string("/") + num + string("-L.png");
	  else
	    outputname = outputdir + string("/") + num + string("-R.png");

	  cout << "Saving image to " << outputname << "..." << endl;
	  cvSaveImage(outputname.c_str(), j->second.cv_image);
	  cout << " done." << endl;
	}
      cv_mutex.unlock();
    }

    return 0;
  }
};

int main(int argc, char **argv) {
  if(argc==1) {
    cout << "\nUsage: \ncalib_converter BAGFILE [BAGFILE ...]\n";
    cout << "The bagfiles must have a rectified left and right Videre image, a full_cloud from the Hokuyo, and a videre/cal_params message.\n" << endl;
    return 1;
  }

  for(int i=1; i<argc; i++) {
    string name = argv[i];
    cout << "\n\n===  Starting processing on " << name << endl;
    calib_converter cc(name);
    cc.run_conversion();
  }
}
  
