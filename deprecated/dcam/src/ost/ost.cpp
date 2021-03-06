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

/*#########################################
 * ost.cpp
 *
 * Main functions for open stereo GUI
 *
 *#########################################
 */

/**
 ** ost.cpp
 **
 ** Kurt Konolige
 ** Senior Researcher
 ** Willow Garage
 ** 68 Willow Road
 ** Menlo Park, CA 94025
 ** E-mail:  konolige@willowgarage.com
 **
 **/


#include <stdlib.h>
#include <malloc.h>
#include <stdarg.h>
#include <math.h>
#include <vector>
#include <string>
#include <algorithm>
#include <ctype.h>
#ifdef WIN32
#pragma warning (disable: 4267 4244 4800 4996)
#include <time.h>
#else
#include <sys/time.h>
#endif
#include "stereogui.h"
#include "imwin/im3Dwin.h"
#include "stereo_image_proc/stereolib.h"
#include "dcam/stereodcam.h"

#include <cv.h>
#include <cxmisc.h>
#include <cvaux.h>
#include <highgui.h>

using namespace std;


//
// Stereo cal application
// Read in images
// Calibrate and write out parameters
//


// device
dcam::StereoDcam *dev = NULL;	// camera object
int numDevs;			// number of devices
bool isVideo;			// true if video streaming
int devIndex;			// index of camera, starts at 0
bool startCam, stopCam;		// camera device stop and start flags
bool isColor;			// true if color requested
bool isRectify;			// true if rectification requested
bool isStereo;			// true if stereo requested
bool is3D;			// true if 3D points requested
bool isPlanar;			// true if plane extraction requested
bool useSTOC;			// true if we want to use STOC processing
bool isTracking;		// true if we want to track the chessboard live
bool isExit;			// true if we want to exit
dc1394video_mode_t videoMode;	// current video mode
dc1394framerate_t videoRate;	// current video rate
size_coding_t videoSize;	// current video size

// current stereo image data
StereoData *stIm = NULL;	// current ptr
StereoData stImdata;		// real data
bool isRefresh;			// refresh the current stereo image display

// plane object and parameters
//FindPlanes planeObj;
float planeParams[4];
float planeThresh = 0.01;	// 1 cm inlier threshold

// GUI stuff
stereogui *stg;			// GUI object
im3DWindow *w3d;		// OpenGL 3D display
Fl_Window *w3dwin;		// Enclosing window


// interval saving
//int isSaving = 60*1;		// zero interval means don't save, otherwise in seconds
int isSaving = 0;		// zero interval means don't save, otherwise in seconds
void save_images(char *fname);	// save helper

#define MAXIMAGES 21

// image storage, need it for undistortions
IplImage *imgs_left[MAXIMAGES];
IplImage *imgs_right[MAXIMAGES];
IplImage *rect_left[MAXIMAGES];
IplImage *rect_right[MAXIMAGES];

// corner storage
int num_x_ints = 8;		// number of x and y corners
int num_y_ints = 6;
int num_pts = 6*8;		// number of total points

CvPoint2D32f *leftcorners[MAXIMAGES];
CvPoint2D32f *rectleftcorners[MAXIMAGES];
CvPoint2D32f *rightcorners[MAXIMAGES];
CvPoint2D32f *rectrightcorners[MAXIMAGES];
int nleftcorners[MAXIMAGES];
int nrightcorners[MAXIMAGES];

// good images
bool goodleft[MAXIMAGES];
bool goodright[MAXIMAGES];
bool goodpair[MAXIMAGES];	// true if features found

// image size in pixels, shouldn't change across images
CvSize imsize_left, imsize_right;

// target square size
double squareSize = 0.100;	// target square size in m, default 100 mm

// rectification mappings
IplImage *mapx_left, *mapy_left, *mapx_right, *mapy_right;
CvMat *rMapxy_left, *rMapa_left, *rMapxy_right, *rMapa_right;

// intrinsics and extrinsics
double K1[3][3], K2[3][3];	// camera matrices
double D1[5], D2[5];		// distortion coefficients, k1, k2, t1, t2, k3
double OM[3], T[3];		// stereo camera 3D relative pose
CvMat K_left  = cvMat(3, 3, CV_64F, K1 );
CvMat K_right = cvMat(3, 3, CV_64F, K2 );
CvMat D_left  = cvMat(1, 5, CV_64F, D1 );
CvMat D_right = cvMat(1, 5, CV_64F, D2 );
CvMat Rotv    = cvMat(3, 1, CV_64F, OM );
CvMat Transv  = cvMat(3, 1, CV_64F, T );

double pl[3][4], pr[3][4];
double rl[3][3], rr[3][3];
CvMat P_left   = cvMat(3, 4, CV_64F, pl );
CvMat P_right  = cvMat(3, 4, CV_64F, pr );
CvMat R_left   = cvMat(3, 3, CV_64F, rl );
CvMat R_right  = cvMat(3, 3, CV_64F, rr );


// helpers
void set_current_tab_index(int ind);
bool parse_filename(char *fname, char **lbase, char **rbase, int *num, char **bname = NULL);
int load_left_cal(char *fname);
int load_right_cal(char *fname);
char * cal_get_param_string();


// epipolar checks
double epi_scanline_error(bool horz = true); // <horz> = 0 for horizontal, 1 for vertical epilines
double epi_scanline_error0(bool horz = true); // <horz> = 0 for horizontal, 1 for vertical epilines

// timing fns
double get_ms();		// for timing
void wait_ms(int ms);		// for waiting


// stereo function headings and globals
bool is_fixed_aspect = true;
bool is_zero_disparity = true;
bool use_kappa1 = true;
bool use_kappa2 = true;
bool use_kappa3 = false;
bool use_tau    = false;

// stereo processing parameters
static int sp_dlen    = 64;	// 64 disparities
static int sp_xoff    =  0;	// no offset
static int sp_corr    = 15;	// correlation window size
static int sp_tthresh = 30;	// texture threshold
static int sp_uthresh = 30;	// uniqueness threshold, percent
static int sp_sthresh = 30;	// uniqueness threshold, percent
static int sp_ssize   = 100;	// speckle size, pixels
static int sp_sdiff   = 8;	// speckle diff, disparities
static stereo_algorithm_t sp_alg=NORMAL_ALGORITHM; // type of stereo algorithm we're using
bool is_unique_check = 0;

// printing matrices
void PrintMat(CvMat *A, FILE *fp = stdout);
int  PrintMatStr(CvMat *A, char *str);


// main program, just put up the GUI dialog

imInfoWindow *iwin = NULL;
calImageWindow * get_current_left_win();
calImageWindow * get_current_right_win();
void info_message(const char *str, ...);	// print in the info line
void debug_message(const char *str, ...);	// print in the info line and to debug window
videre_proc_mode_t checkProcMode(videre_proc_mode_t mode); // consistent STOC mode
bool FindCorners(CvPoint2D32f **corners, int *nc, bool *good, IplImage *img);
dcam::StereoDcam *initcam(uint64_t guid);
int do_button(int e, int x, int y, int b, int m, imWindow *w);

// IPL images for transfers to OpenCV domain
IplImage *img1 = NULL, *img2 = NULL;


int
main(int argc, char **argv)	// no arguments
{
  Fl_Window *gwin;
  w3d = NULL;			// no OpenGL window yet
  w3dwin = NULL;
  isExit = false;
  int otime = 0;		// for interval saving
  int savenum = 0;

  // initialize data structures
  for (int i=0; i<MAXIMAGES; i++)
    {
      imgs_left[i] = imgs_right[i] = rect_left[i] = rect_right[i] = NULL;
      leftcorners[i] = NULL;
      rectleftcorners[i] = NULL;
      rightcorners[i] = NULL;
      rectrightcorners[i] = NULL;
      nleftcorners[i] = 0;
      nrightcorners[i] = 0;
      goodpair[i] = goodleft[i] = goodright[i] = false;
    }
  mapx_left = mapy_left = mapx_right = mapy_right = NULL;
  
  // device flags
  startCam = false;
  stopCam = false;
  isVideo = false;
  isRefresh = false;
  numDevs = 0;
  useSTOC = true;
  isTracking = false;

  IplImage *trackImgL = NULL;
  IplImage *trackImgR = NULL;
  IplImage *trackCornersL = NULL;
  IplImage *trackCornersR = NULL;

  // start up dialog window
  stg = new stereogui;
  gwin = stg->ost_main;
  set_current_tab_index(1);
  gwin->show();
  stg->cal_window->hide();	// have to request cal window

  // button handler
  stg->mainLeft->ButtonHandler(do_button);
  stg->mainRight->ButtonHandler(do_button);

  // start up debug window
  iwin = new imInfoWindow(500,400,"OST output");
  //  iwin->show();

  // get devices
  try 
    {
      dcam::init();
      numDevs = dcam::numCameras();
      debug_message("[oST] Number of cameras: %d", numDevs);

      // print out GUIDs, vendor, model; set up Video pulldown
      Fl_Choice *devs = stg->cam_select;
      for (int i=0; i<numDevs; i++)
	{
	  debug_message("[oST] Camera %d GUID: %llx  Vendor: %s  Model: %s", 
			i, dcam::getGuid(i), dcam::getVendor(i), dcam::getModel(i));
	  char buf[1024];
	  sprintf(buf, "%s %s %llx", dcam::getVendor(i), dcam::getModel(i), dcam::getGuid(i));
	  devs->add(buf);
	}
      if (numDevs > 0) 
	devs->value(0);

    }
  catch (dcam::DcamException) 
    {
      debug_message("[Dcam] Can't initialize libdc1394, check /dev/raw1394/, /dev/video1394/0 permissions");
      numDevs = 0;
    }


#if 0
  // don't fool around, open the first device
  debug_message("[Dcam] Finding first available camera");
  dev = initcam(dcam::getGuid(0));

  // set range max value
  dev->setRangeMax(4.0);	// in meters
  dev->setRangeMin(0.5);	// in meters
#endif

  dev = NULL;

  static videre_proc_mode_t pmode = PROC_MODE_NONE;
  videoRate = DC1394_FRAMERATE_30;
  videoSize = SIZE_640x480;
  stg->video_size->value(1);	// set to 640x480

  while (fltk_check() && !isExit) // process GUI commands, serve video
    {
      // check for video stream commands
      if (startCam)
	{
	  if (dev)
	    {
	      debug_message("[Dcam] Setting format, frame rate and PROC mode");
	      if (dev->isVidereStereo)
		{
		  videoMode = VIDERE_STEREO_640x480;
		  switch(videoSize)
		    {
		    case SIZE_640x480:
		      videoMode = VIDERE_STEREO_640x480;
		      break;
		    case SIZE_320x240:
		      videoMode = VIDERE_STEREO_320x240;
		      break;
		    case SIZE_1280x960:
		      videoMode = VIDERE_STEREO_1280x960;
		      break;
                    default:
                      debug_message("[Dcam] Unexpected format %d\n", videoSize);
                      break;
		    }
		}


	      try 
		{ 
		  dev->setFormat(videoMode, videoRate); 
		  if (dev->isSTOC)
		    stg->stoc_button->value(true); // turn it on
		  dev->setProcMode(pmode);
		  debug_message("[Dcam] Starting device");
		  dev->start();
		  dev->setTextureThresh(sp_tthresh);
		  dev->setUniqueThresh(sp_uthresh);
		  dev->setSpeckleSize(sp_ssize);
		  dev->setSpeckleDiff(sp_sdiff);
		  dev->setCorrsize(sp_corr);
		  // set auto exp, auto gain max
		  dev->setMaxAutoVals(100,48);
		  dev->setGain(0,true);
		  dev->setExposure(0,true);
		  dev->setBrightness(0,true);

		  isVideo = true;	// needed to keep thread running
		}

	      catch (dcam::DcamException &err)
		{ 
		  debug_message("[Dcam] Can't set video format"); 
		  stg->video_button->value(false);
		}

	      startCam = false;
	    }
	}

      if (stopCam)
	{
	  if (dev)
	    {
	      debug_message("[Dcam] Stopping device");
	      isVideo = false;
	      dev->stop();
	      stg->video_button->value(0);
	      stopCam = false;
	    }
	}

      // check for streaming video
      if (isVideo)
	{
	  // check PROC modes, make consistent with requested info
	  pmode = checkProcMode(pmode);
	  bool ret = dev->getImage(500);
	  if (ret)
	    {
	      stIm = dev->stIm;
	      isRefresh = true;
	    }
	}
      else			// no video, yield a little
	{
	  wait_ms(10);
	  if (stIm && isRefresh && !isVideo) // refresh the stereo
	    stIm->hasDisparity = false;	    
	}

      if (isRefresh & stIm != NULL)
	{
	  double tt0,tt1;

	  isRefresh = false;
	  int w = stIm->imWidth;
	  int h = stIm->imHeight;

	  tt0 = get_ms();
	  // check processing
	  if (isRectify)	// rectify images
	    stIm->doRectify();
	  if (isStereo)	// get stereo disparity
	    stIm->doDisparity(sp_alg);
	  tt1 = get_ms();
//	  printf("Stereo time: %d ms\n", (int)(tt1-tt0));

	  if (is3D)		// get 3D points
	    {
	      tt0 = get_ms();
	      stIm->doCalcPts(true); // make it an array
	      tt1 = get_ms();
//	      printf("3D time: %d ms\n", (int)(tt1-tt0));

	      // check for extracting a plane or planes
	      isPlanar = false;
	      if (isPlanar)
		{
#if 0
		  int n,nn;
		  tt0 = get_ms();
		  // get first largest plane
		  planeObj.SetPointCloud(stIm->imPtArray(), stIm->numPts, 10);
		  n = planeObj.FindPlane(planeParams, planeThresh, 50);
		  nn = planeObj.IndexPlane(1, planeThresh, planeParams);
		  // get second largest plane
		  planeObj.SetPointCloud(stIm->imPtArray(), stIm->numPts, 10);
		  planeObj.FindPlane(planeParams, planeThresh, 50);
		  planeObj.IndexPlane(2, planeThresh, planeParams);
		  tt1 = get_ms();
//		  printf("Planar time: %d ms\n", (int)(tt1-tt0));
		  printf("Found plane %f %f %f %f with %d/%d inliers\n", 
			 planeParams[0], planeParams[1], planeParams[2], planeParams[3], n, nn);
#endif
		}

	      // check for 3D window
	      if (!w3d)
		{
		  w3dwin = new Fl_Window(640,480,"3D Display");
		  w3d = new im3DWindow(10,10,w3dwin->w()-20,w3dwin->h()-20);
		  w3dwin->end();
		  w3dwin->resizable(w3d);
		  w3dwin->show();
		}
	      w3d->DisplayImage(stIm);
	    }


	  // check for tracking of chessboard
	  // first find images and convert into IPL images
	  // TODO: check header size if IplImage is already created
	  if (isTracking)
	    {
	      // set up grayscale image
	      if (trackImgL == NULL)
		{
		  trackImgL = cvCreateImageHeader(cvSize(w,h),IPL_DEPTH_8U,1); 
		  trackCornersL = cvCreateImage(cvGetSize(trackImgL), IPL_DEPTH_8U, 3 );
		}
	      if (stIm->imLeft->imRectType != COLOR_CODING_NONE)
		cvSetData(trackImgL,stIm->imLeft->imRect,w);
	      else if (stIm->imLeft->imType != COLOR_CODING_NONE)
		cvSetData(trackImgL,stIm->imLeft->im,w);
	      else
		isTracking = false;		    

	      if (trackImgR == NULL)
		{
		  trackImgR = cvCreateImageHeader(cvSize(w,h),IPL_DEPTH_8U,1); 
		  trackCornersR = cvCreateImage(cvGetSize(trackImgR), IPL_DEPTH_8U, 3 );
		}
	      if (stIm->imRight->imRectType != COLOR_CODING_NONE)
		cvSetData(trackImgR,stIm->imRight->imRect,w);
	      else if (stIm->imRight->imType != COLOR_CODING_NONE)
		cvSetData(trackImgR,stIm->imRight->im,w);
	      else
		isTracking = false;

	      if (!isTracking)
		{
		  stg->track_button->value(0);
		  calImageWindow *cwin;
		  cwin = stg->mainLeft;
		  cwin->clear2DFeatures();
		  cwin = stg->mainRight;
		  cwin->clear2DFeatures();
		}
	    }

	  if (isTracking)
	    {
	      FindCorners(&leftcorners[0], &nleftcorners[0], &goodleft[0], trackImgL);
	      FindCorners(&rightcorners[0], &nrightcorners[0], &goodright[0], trackImgR);
	    }

	  // display left image
	  calImageWindow *cwin = stg->mainLeft;
	  if (stIm->imLeft->imRectColorType != COLOR_CODING_NONE)
	    cwin->DisplayImage((unsigned char *)stIm->imLeft->imRectColor, w, h, w, RGB24);
	  else if (stIm->imLeft->imRectType != COLOR_CODING_NONE)
	    cwin->DisplayImage((unsigned char *)stIm->imLeft->imRect, w, h, w);
	  else if (stIm->imLeft->imColorType != COLOR_CODING_NONE)
	    cwin->DisplayImage((unsigned char *)stIm->imLeft->imColor, w, h, w, RGB24);
	  else if (stIm->imLeft->imType != COLOR_CODING_NONE)
	    cwin->DisplayImage((unsigned char *)stIm->imLeft->im, w, h, w);

	  if (isTracking)
	    cwin->display2DFeatures(leftcorners[0],nleftcorners[0],goodleft[0]);

	  // display right image
	  cwin = stg->mainRight;
	  if (stIm->hasDisparity)
	    cwin->DisplayImage((unsigned char *)stIm->imDisp, w, h, w, DISPARITY, sp_dlen*16);
	  else if (stIm->imRight->imRectColorType != COLOR_CODING_NONE)
	    cwin->DisplayImage((unsigned char *)stIm->imRight->imRectColor, w, h, w, RGB24);
	  else if (stIm->imRight->imRectType != COLOR_CODING_NONE)
	    cwin->DisplayImage((unsigned char *)stIm->imRight->imRect, w, h, w);
	  else if (stIm->imRight->imColorType != COLOR_CODING_NONE)
	    cwin->DisplayImage((unsigned char *)stIm->imRight->imColor, w, h, w, RGB24);
	  else if (stIm->imRight->imType != COLOR_CODING_NONE)
	    cwin->DisplayImage((unsigned char *)stIm->imRight->im, w, h, w);

	  if (isTracking)
	    cwin->display2DFeatures(rightcorners[0],nrightcorners[0],goodright[0]);

	  if (isTracking)
	    {
	      if (goodright[0] && goodleft[0])
		{
		  goodpair[0] = true;
		  double ee = epi_scanline_error0();
		  info_message("Epipolar error: %0.2f pixels", ee);
		}
	    }

	  if (isSaving)	// save images at intervals
	    {
	      int ctime = (int)(0.001 * get_ms());
	      if (ctime > otime+isSaving) // interval exceeded
		{
		  char fname[1024];
		  sprintf(fname,"im-%06d",savenum);
		  savenum++;
		  save_images(fname);
		  otime = ctime;
		}
	    }

	}

    }

  // exiting, turn off camera
  if (dev)
    {
      debug_message("[Dcam] Stopping device");
      isVideo = false;
      dev->stop();
      stg->video_button->value(0);
      stopCam = false;
    }
}


//
// initialize a camera
//

dcam::StereoDcam *
initcam(uint64_t guid)
{
  if (dev)			// get rid of old one
    delete dev;

  dev = new dcam::StereoDcam(guid);
  stg->exposure_val->range(dev->expMax,dev->expMin);
  stg->gain_val->range(dev->gainMax,dev->gainMin);
  stg->brightness_val->range(dev->brightMax,dev->brightMin);

  return dev;
}




//
// utility fns
//

// find corners
static int numcpts = 0;		// for managing corner points
bool
FindCorners(CvPoint2D32f **corners, int *nc, bool *good, IplImage *img)
{
  // find corners
  if (num_pts != numcpts)
    {
      if (*corners) delete [] *corners;
      *corners = NULL;
      numcpts = num_pts;
    }

  if (*corners == NULL)
    *corners = new CvPoint2D32f[num_pts];

  int numc = 0;
  int ret = cvFindChessboardCorners(img, cvSize(num_x_ints, num_y_ints),
		   *corners, &numc, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);
  *nc = numc;
  *good = ret;

  // do subpixel calculation, if corners have been found
  if (ret)
    {
      cvFindCornerSubPix(img, *corners, numc, 
			 cvSize(5,5),cvSize(-1,-1), 
			 cvTermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 30, 0.1 ));
    }
  return ret;
}



// check PROC mode, reset if necessary
videre_proc_mode_t
checkProcMode(videre_proc_mode_t mode)
{
  videre_proc_mode_t mm = mode;

  // check STOC processing
  if (dev->isSTOC && useSTOC)	
    {
      if (isStereo && isColor && mode != PROC_MODE_DISPARITY_RAW)
	mm = PROC_MODE_DISPARITY_RAW;
      if (isStereo && !isColor && mode != PROC_MODE_DISPARITY)
	mm = PROC_MODE_DISPARITY;
      if (!isStereo && isColor && mode != PROC_MODE_NONE)
	mm = PROC_MODE_NONE;
      if (!isStereo && !isColor && !isRectify && mode != PROC_MODE_NONE)
	mm = PROC_MODE_NONE;
      if (!isStereo && !isColor && isRectify && mode != PROC_MODE_RECTIFIED)
	mm = PROC_MODE_RECTIFIED;
    }

  // check non-STOC processing
  if (dev->isSTOC && !useSTOC)	
    {
      if (mode != PROC_MODE_NONE)
	mm = PROC_MODE_NONE;
    }

  if (mm != mode)
    dev->setProcMode(mm);

  return mm;
}


// info line only
void
info_message(const char *str, ...)	// print in the info line
{
  char buf[1024];
  va_list ptr;
  va_start(ptr,str);
  if (str)
    {
      vsprintf(buf,str,ptr);
      stg->info_message->value(buf);
    }
}

// print in message line and on debug window
void
debug_message(const char *str, ...)	// print in the info line
{
  char buf[1024];
  va_list ptr;
  va_start(ptr,str);
  if (str)
    {
      vsprintf(buf,str,ptr);
      stg->info_message->value(buf);
      if (iwin)
	iwin->Print(buf);
      printf("%s\n",buf);
    }
}


int
get_current_tab_index()
{
  Fl_Tabs *tb = (Fl_Tabs *)stg->window_tab;
  Fl_Widget *wd;
  wd = tb->value();
  int ind = (long int)wd->user_data();
  return ind;
}

void
set_current_tab_index(int ind)
{
  Fl_Tabs *tb = (Fl_Tabs *)stg->window_tab;
  Fl_Widget *wd;
  wd = tb->child(ind-1);
  tb->value(wd);
}


calImageWindow *
get_current_left_win()
{
  int ind = get_current_tab_index();
  if (ind < 1)
    return NULL;
  switch (ind)
    {
    case 1:
      return stg->calLeft1;
    case 2:
      return stg->calLeft2;
    case 3:
      return stg->calLeft3;
    case 4:
      return stg->calLeft4;
    case 5:
      return stg->calLeft5;
    case 6:
      return stg->calLeft6;
    case 7:
      return stg->calLeft7;
    case 8:
      return stg->calLeft8;
    case 9:
      return stg->calLeft9;
    case 10:
      return stg->calLeft10;
    case 11:
      return stg->calLeft11;
    case 12:
      return stg->calLeft12;
    case 13:
      return stg->calLeft13;
    case 14:
      return stg->calLeft14;
    case 15:
      return stg->calLeft15;
    case 16:
      return stg->calLeft16;
    case 17:
      return stg->calLeft17;
    case 18:
      return stg->calLeft18;
    case 19:
      return stg->calLeft19;
    case 20:
      return stg->calLeft20;
    default:
      return NULL;
    }
}

calImageWindow *
get_current_right_win()
{
  int ind = get_current_tab_index();
  if (ind < 1)
    return NULL;
  switch (ind)
    {
    case 1:
      return stg->calRight1;
    case 2:
      return stg->calRight2;
    case 3:
      return stg->calRight3;
    case 4:
      return stg->calRight4;
    case 5:
      return stg->calRight5;
    case 6:
      return stg->calRight6;
    case 7:
      return stg->calRight7;
    case 8:
      return stg->calRight8;
    case 9:
      return stg->calRight9;
    case 10:
      return stg->calRight10;
    case 11:
      return stg->calRight11;
    case 12:
      return stg->calRight12;
    case 13:
      return stg->calRight13;
    case 14:
      return stg->calRight14;
    case 15:
      return stg->calRight15;
    case 16:
      return stg->calRight16;
    case 17:
      return stg->calRight17;
    case 18:
      return stg->calRight18;
    case 19:
      return stg->calRight19;
    case 20:
      return stg->calRight20;
    default:
      return NULL;
    }
}


//
// callbacks
//

// Load left image into image pair
// Also find chessboard points

void
cal_load_left_cb(Fl_Button* b, void* arg)
{
  char *fname = fl_file_chooser("Load file", NULL, NULL);
  if (fname == NULL)
    return;

  printf("File name: %s\n", fname);

  load_left_cal(fname);
}


int
load_left_cal(char *fname)
{
  IplImage *dbg_corners = 0;


  // get window tab
  int ind = get_current_tab_index();
  
  IplImage *im;			// OpenCV image

  calImageWindow *cwin = get_current_left_win();

  im = cvLoadImage(fname);	// load image, could be color
  if (im == NULL)
    debug_message("Can't load file %s\n", fname);
  else
    {
      // make grayscale, display
      IplImage* img=cvCreateImage(cvGetSize(im),IPL_DEPTH_8U,1); 
      dbg_corners = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 3 );
      cvConvertImage(im,img);
      imgs_left[ind] = img;
      imsize_left = cvGetSize(im);
      debug_message("Size: %d x %d, ch: %d", img->width, img->height, img->nChannels);
      cwin->DisplayImage((unsigned char *)img->imageData, img->width, img->height, img->width);

      if (ind < 1) return ind;	// not a calibration image

      // find corners
      if (leftcorners[ind])
	delete [] leftcorners[ind];
      leftcorners[ind] = new CvPoint2D32f[num_x_ints*num_y_ints];
      rectleftcorners[ind] = new CvPoint2D32f[num_x_ints*num_y_ints];
      int numc = 0;
      int ret = cvFindChessboardCorners(img, cvSize(num_x_ints, num_y_ints),
		leftcorners[ind], &numc, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);
      nleftcorners[ind] = numc;
      debug_message("Found %d corners", numc);
      cwin->display2DFeatures(leftcorners[ind],numc,ret);

      // do subpixel calculation, if corners have been found
      if (ret)
	cvFindCornerSubPix(img, leftcorners[ind], numc, 
			   cvSize(5,5),cvSize(-1,-1), 
			   cvTermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 30, 0.1 ));

      // set good/bad image
      goodleft[ind] = ret;

      cvReleaseImage(&im);
      return ind;
    }
  return -1;
}


// Load right image into image pair
// Also find chessboard points

void
cal_load_right_cb(Fl_Button* b, void* arg)
{
  char *fname = fl_file_chooser("Load file", NULL, NULL);
  if (fname == NULL)
    return;

  printf("File name: %s\n", fname);

  load_right_cal(fname);
}


int
load_right_cal(char *fname)
{
  // get window tab
  int ind = get_current_tab_index();
  
  IplImage *im;			// OpenCV image

  calImageWindow *cwin = get_current_right_win();


  im = cvLoadImage(fname);	// load image, could be color
  if (im == NULL)
    debug_message("Can't load file %s\n", fname);
  else
    {
      // convert to grayscale
      IplImage* img=cvCreateImage(cvGetSize(im),IPL_DEPTH_8U,1); 
      cvConvertImage(im,img);
      imgs_right[ind] = img;
      imsize_right = cvGetSize(im);
      debug_message("Size: %d x %d, ch: %d", img->width, img->height, img->nChannels);
      cwin->DisplayImage((unsigned char *)img->imageData, img->width, img->height, img->width);

      if (ind < 1) return ind;	// not a calibration image

      // find corners
      if (rightcorners[ind])
	delete [] rightcorners[ind];
      rightcorners[ind] = new CvPoint2D32f[num_x_ints*num_y_ints];
      rectrightcorners[ind] = new CvPoint2D32f[num_x_ints*num_y_ints];
      int numc = 0;
      int ret = cvFindChessboardCorners(img, cvSize(num_x_ints, num_y_ints),
					rightcorners[ind], &numc,
					CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);
      nrightcorners[ind] = numc;
      debug_message("Found %d corners", numc);
      cwin->display2DFeatures(rightcorners[ind],numc,ret);

      // do subpixel calculation, if corners have been found
      if (ret)
	{
	  cvFindCornerSubPix(img, rightcorners[ind], numc, 
			   cvSize(5,5),cvSize(-1,-1), 
			   cvTermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 30, 0.1 ));
	}

      // set good/bad image
      goodright[ind] = ret;

      cvReleaseImage(&im);
      return ind;
    }

  return -1;
}


// Load both images of a pair, or a single image

void cal_load_cb(Fl_Button*, void*) 
{
  // get window tab
  char *fname = fl_file_chooser("Load file", NULL, NULL);
  if (fname == NULL)
    return;

  printf("File name: %s\n", fname);

  char *lname = NULL, *rname = NULL;
  bool ret;
  ret = parse_filename(fname, &lname, &rname, NULL);

  if (!ret)			// just load left
    load_left_cal(lname);
  else
    {
      load_left_cal(lname);
      load_right_cal(rname);
    }
}


// Load a sequence
// Specify either right or left, will try to find the other one
// Names should be like this:  *NNN-L.eee *NNN-R.eee
//                   or this:  left*NNN.eee right*NNN.eee
// Sequence numbers should always have the same number of digits across
//    images, use leading zeros, e.g., image_001-L.bmp

void
cal_load_seq_cb(Fl_Button* b, void* arg)
{
  // get window tab
  int ind = get_current_tab_index();
  printf("Current tab: %d\n", ind);
  
  char *fname = fl_file_chooser("Load file", NULL, NULL);
  if (fname == NULL)
    return;

  debug_message("[OST] File name: %s", fname);

  char *lname = NULL, *rname = NULL;
  int seq;
  bool ret;
  ret = parse_filename(fname, &lname, &rname, &seq);

  if (!ret)			// just load left
    load_left_cal(lname);
  else if (seq < 0)		// stereo only
    {
      load_left_cal(lname);
      load_right_cal(rname);
    }
  else if (rname == NULL)	// mono sequence
    {
      int indx = ind;
      while (ind < MAXIMAGES)
	{
	  char fname[1024];
	  int ret;
	  set_current_tab_index(ind); 
	  sprintf(fname,lname,seq); 
	  ret = load_left_cal(fname);
	  if (ret < 0)
	    break;
	  ind++;
	  seq++;
	}
      if (indx != ind)
	set_current_tab_index(ind-1); // set to last found images

    }
  else				// stereo sequence
    {
      int indx = ind;
      while (ind < MAXIMAGES)
	{
	  char fname[1024];
	  int ret;
	  set_current_tab_index(ind); 
	  sprintf(fname,lname,seq); 
	  ret = load_left_cal(fname);
	  if (ret < 0)
	    break;
	  sprintf(fname,rname,seq);
	  ret = load_right_cal(fname);
	  if (ret < 0)
	    break;
	  ind++;
	  seq++;
	}
      if (indx != ind)
	set_current_tab_index(ind-1); // set to last found images
    }
}


//
// delete an image or image pair
//

void cal_delete_image(Fl_Button*, void*) 
{
  // get window tab
  int ind = get_current_tab_index();
  calImageWindow *lwin = get_current_left_win();
  calImageWindow *rwin = get_current_right_win();
  
  if (imgs_right[ind])
    {
      cvReleaseImage(&imgs_right[ind]);
      imgs_right[ind] = NULL;
    }

  rwin->clearAll();

  if (rightcorners[ind])
    {
      delete [] rightcorners[ind];
      rightcorners[ind] = NULL;
    }
  if (rectrightcorners[ind])
    {
      delete [] rectrightcorners[ind];
      rectrightcorners[ind] = NULL;
    }
  nrightcorners[ind] = 0;

  goodright[ind] = false;

  if (imgs_left[ind])
    {
      cvReleaseImage(&imgs_left[ind]);
      imgs_left[ind] = NULL;
    }

  lwin->clearAll();

  if (leftcorners[ind])
    {
      delete [] leftcorners[ind];
      leftcorners[ind] = NULL;
    }
  if (rectleftcorners[ind])
    {
      delete [] rectleftcorners[ind];
      rectleftcorners[ind] = NULL;
    }
  nleftcorners[ind] = 0;

  goodleft[ind] = false;


}



//
// calibrate stereo
// uses Vadim's rewrite of Bouguet's MATLAB code
// gradient descent algorithm
//

void cal_calibrate_single(void);

void cal_calibrate_cb(Fl_Button*, void*)
{
  int i, j, nframes;	
  bool is_horz = true;		// true if horizontal epipolar lines
  vector<CvPoint3D32f> objectPoints; // points on the targets
  vector<CvPoint2D32f> points[2]; // points on the image, left is index 0, right 1
  vector<int> npoints;		// number of points in each image
  vector<uchar> active[2];	// active images (found pattern), left is 0, right 1
  vector<CvPoint2D32f> temp(num_pts); // gather image points
  CvSize imageSize = imsize_left; // image size, should be same for left/right
  double R[3][3], E[3][3], F[3][3];
  CvMat _R = cvMat(3, 3, CV_64F, R );
  CvMat _E = cvMat(3, 3, CV_64F, E );
  CvMat _F = cvMat(3, 3, CV_64F, F );

  // find left/right pairs 
  int n_left = 0, n_right = 0, n_pair = 0, n_tot = 0;
  
  for (int i=1; i<MAXIMAGES; i++) // first index is not a cal image
    {
      goodpair[i] = false;
      if (goodleft[i])
	{
	  n_tot = i;
	  n_left++;
	  if (goodright[i])
	    {
	      n_pair++;
	      goodpair[i] = true;
	      n_right++;
	    }
	}
      else if (goodright[i])
	{
	  n_tot = i;
	  n_right++;
	}
    }
  
  if (n_pair < 4)
    {
      debug_message("Fewer than 4 stereo pairs, calibrating single cameras only");
      cal_calibrate_single();
      return;
    }


  // set up image points from found corners
  for(i=1; i<n_tot; i++)	// first index is not a cal image
    {
      // set up active image vectors
      active[0].push_back((uchar)goodleft[i]);
      active[1].push_back((uchar)goodright[i]);

      int N; 
      vector<CvPoint2D32f>& pts = points[0]; // left image points
      N = pts.size();
      pts.resize(N + num_pts, cvPoint2D32f(0,0)); // add to array

      int n = N;
      if (goodleft[i])		// have good image points?
	for (int j=0; j<num_pts; j++)
	  pts[n++] = leftcorners[i][j];

      vector<CvPoint2D32f>& pts2 = points[1]; // right image points
      N = pts2.size();
      pts2.resize(N + num_pts, cvPoint2D32f(0,0)); // add to array

      n = N;
      if (goodright[i])		// have good image points?
	for (int j=0; j<num_pts; j++)
	  pts2[n++] = rightcorners[i][j];
    }

    nframes = active[0].size();

    objectPoints.resize(nframes*num_pts);
    for( i = 0; i < num_y_ints; i++ )
        for( j = 0; j < num_x_ints; j++ )
            objectPoints[i*num_x_ints + j] = cvPoint3D32f(i*squareSize, j*squareSize, 0);
    for( i = 1; i < nframes; i++ )
        copy( objectPoints.begin(), objectPoints.begin() + num_pts, objectPoints.begin() + i*num_pts );

    npoints.resize(nframes,num_pts);

    CvMat _objectPoints = cvMat(1, objectPoints.size(), CV_32FC3, &objectPoints[0] );
    CvMat _imagePoints1 = cvMat(1, points[0].size(), CV_32FC2, &points[0][0] );
    CvMat _imagePoints2 = cvMat(1, points[1].size(), CV_32FC2, &points[1][0] );
    CvMat _npoints = cvMat(1, npoints.size(), CV_32S, &npoints[0] );

    cvSetIdentity(&K_left);
    cvSetIdentity(&K_right);

    int flags = 0;
    if (is_fixed_aspect)
      flags |= CV_CALIB_FIX_ASPECT_RATIO;
    if (!use_kappa1)
      flags |= CV_CALIB_FIX_K1;
    if (!use_kappa2)
      flags |= CV_CALIB_FIX_K2;
    if (!use_kappa3)
      flags |= CV_CALIB_FIX_K3;
    if (!use_tau)
      flags |= CV_CALIB_ZERO_TANGENT_DIST;
//  flags |= CV_CALIB_ZERO_TANGENT_DIST;
    
    cvStereoCalibrate( &_objectPoints, &_imagePoints1, &_imagePoints2, &_npoints,
		       &K_left, &D_left, &K_right, &D_right,
		       imageSize, &_R, &Transv, &_E, &_F, 
		       cvTermCriteria(CV_TERMCRIT_ITER+CV_TERMCRIT_EPS, 1, 1e-5),
		       flags);


    // show params
    cvRodrigues2( &_R, &Rotv );
    debug_message("\nTranslation vector:\n");
    PrintMat(&Transv);

    debug_message("\nRotation vector:\n");
    PrintMat(&Rotv);

    debug_message("\nLeft camera matrix:\n");
    PrintMat(&K_left);

    debug_message("\nLeft distortion vector:\n");
    PrintMat(&D_left);

    debug_message("\nRight camera matrix:\n");
    PrintMat(&K_right);

    debug_message("\nRight distortion vector:\n");
    PrintMat(&D_right);

    debug_message("\n\n");

    // calculate rectification and "new" projection matrices
    flags = 0;
    if (is_zero_disparity)
      flags |= CV_CALIB_ZERO_DISPARITY;
    cvStereoRectify(&K_left, &K_right, &D_left, &D_right,
		    imageSize, &_R, &Transv,
		    &R_left, &R_right, &P_left, &P_right, NULL, flags);
    is_horz = fabs(pr[1][3]) <= fabs(pr[0][3]);

    debug_message("\n\nRectified image parameters\n");
    if (is_horz)
      debug_message("(Horizontal epilines)\n");
    else
      debug_message("(Vertical epilines)\n");


    debug_message("\nLeft camera projection matrix:\n");
    PrintMat(&P_left);

    debug_message("\nRight camera projection matrix:\n");
    PrintMat(&P_right);    

    debug_message("\nLeft camera rectification matrix:\n");
    PrintMat(&R_left);

    debug_message("\nRight camera rectification matrix:\n");
    PrintMat(&R_right);

    debug_message("\n\n");


    // undistort images

    // Set up rectification mapping
    mapx_left = cvCreateImage(imsize_left, IPL_DEPTH_32F, 1 );
    mapy_left = cvCreateImage(imsize_left, IPL_DEPTH_32F, 1 );
    rMapxy_left = cvCreateMat(imsize_left.height, imsize_left.width, CV_16SC2);
    rMapa_left  = cvCreateMat(imsize_left.height, imsize_left.width, CV_16SC1);

    // rectified K matrix
    CvMat *Krect  = cvCreateMat(3,3,CV_64F);
    CvMat kstub;
    cvCopy(cvGetSubRect(&P_left,&kstub,cvRect(0,0,3,3)),Krect);

#if 0
    cvInitUndistortRectifyMap(&K_left, &D_left, &R_left, Krect, mapx_left, mapy_left);
    cvConvertMaps(mapx_left,mapy_left,rMapxy_left,rMapa_left);
  
    // Save rectified images in array and display
    // Also calculated rectified corner points
    // Should probably enable re-display of original images
    for (int i=1; i<MAXIMAGES; i++)
      {
	if (!goodleft[i]) continue;
	if (rect_left[i] == NULL)
	  rect_left[i] = cvCloneImage(imgs_left[i]);
	cvRemap(imgs_left[i], rect_left[i], mapx_left, mapy_left);
	//	cvRemap(imgs_left[i], rect_left[i], rMapxy_left, rMapa_left);
	IplImage *img = rect_left[i];
	set_current_tab_index(i);
	calImageWindow *cwin = get_current_left_win();
	cwin->clear2DFeatures();
	if (nleftcorners[i]>0)
	  {
	    CvMat ipts = cvMat(1,nleftcorners[i],CV_32FC2,leftcorners[i]);
	    CvMat opts = cvMat(1,nleftcorners[i],CV_32FC2,rectleftcorners[i]);
	    cvUndistortPoints(&ipts,&opts,&K_left,&D_left,&R_left,&P_left);
	    cwin->display2DFeatures(rectleftcorners[i],nleftcorners[i],1);
	  }
	cwin->DisplayImage((unsigned char *)img->imageData, img->width, img->height, img->width);
      }

    // Set up rectification mapping
    mapx_right = cvCreateImage(imsize_right, IPL_DEPTH_32F, 1 );
    mapy_right = cvCreateImage(imsize_right, IPL_DEPTH_32F, 1 );
    rMapxy_right = cvCreateMat(imsize_left.height, imsize_left.width, CV_16SC2);
    rMapa_right  = cvCreateMat(imsize_left.height, imsize_left.width, CV_16SC1);


    cvInitUndistortRectifyMap(&K_right, &D_right, &R_right, Krect, mapx_right, mapy_right);
    cvConvertMaps(mapx_right,mapy_right,rMapxy_right,rMapa_right);
  
    // Save rectified images in array and display
    // Should probably enable re-dispaly of original images
    for (int i=1; i<MAXIMAGES; i++)
      {
	if (!goodright[i]) continue;
	if (rect_right[i] == NULL)
	  rect_right[i] = cvCloneImage(imgs_right[i]);
	cvRemap(imgs_right[i], rect_right[i], mapx_right, mapy_right);
	IplImage *img = rect_right[i];
	set_current_tab_index(i);
	calImageWindow *cwin = get_current_right_win();
	cwin->clear2DFeatures();
	if (nrightcorners[i]>0)
	  {
	    CvMat ipts = cvMat(1,nrightcorners[i],CV_32FC2,rightcorners[i]);
	    CvMat opts = cvMat(1,nrightcorners[i],CV_32FC2,rectrightcorners[i]);
	    cvUndistortPoints(&ipts,&opts,&K_right,&D_right,&R_right,&P_right);
	    cwin->display2DFeatures(rectrightcorners[i],nrightcorners[i],1);
	  }
	cwin->DisplayImage((unsigned char *)img->imageData, img->width, img->height, img->width);
      }

    // compute and show epipolar line RMS error
    double err = epi_scanline_error(is_horz);
    debug_message("RMS error from scanline: %f pixels\n\n", err);
#endif

    // get calibration string and set current device with it
    char *bb = cal_get_param_string();
    stIm->extractParams(bb);	// set up parameters
    if (dev) dev->putParameters(bb);	// save in device object
}



// calibrate single camera(s)

void cal_calibrate_single(void)
{
  // find left/right pairs 
  int n_left = 0, n_right = 0, n_pair = 0;
  
  for (int i=1; i<MAXIMAGES; i++)
    {
      goodpair[i] = false;
      if (goodleft[i])
	{
	  n_left++;
	  if (goodright[i])
	    {
	      n_pair++;
	      goodpair[i] = true;
	    }
	}
      if (goodright[i])
	n_right++;
    }
  

  // calibrate left camera
  if (n_left < 3)		// not enough left images
    {
      debug_message("Only %d good left images, need 3\n", n_left);
      return;
    }

  // create matrices, total pts x 2 or 3
  // each row is a point
  CvMat *opts_left = cvCreateMat(n_left*num_pts, 3, CV_32FC1);
  CvMat *ipts_left = cvCreateMat(n_left*num_pts, 2, CV_32FC1);
  CvMat *npts_left = cvCreateMat(n_left, 1, CV_32SC1);

  // fill them from corner data
  int ii = 0;
  for (int i=1; i<MAXIMAGES; i++)
    {
      if (!goodleft[i]) continue; // skip over bad images
      int k = ii*num_pts;	// matrix row index
      for (int j=0; j<num_pts; j++, k++)
	{
	  CV_MAT_ELEM(*ipts_left, float, k, 0) = (leftcorners[i])[j].x;
	  CV_MAT_ELEM(*ipts_left, float, k, 1) = (leftcorners[i])[j].y;
	  CV_MAT_ELEM(*opts_left, float, k, 0) = j/num_x_ints; // corner x pos
	  CV_MAT_ELEM(*opts_left, float, k, 1) = j%num_x_ints; // corner y pos
	  CV_MAT_ELEM(*opts_left, float, k, 2) = 0.0f; // corner z pos
	}
      CV_MAT_ELEM(*npts_left, int, ii, 0) = num_pts; // number of points
      ii++;
    }

  // save for debug
  cvSave("ipts_left.xml", ipts_left);
  cvSave("opts_left.xml", opts_left);

  // now call the cal routine
  CvMat* intrinsics_left = cvCreateMat(3,3,CV_32FC1);
  CvMat* distortion_left = cvCreateMat(4,1,CV_32FC1);
  // focal lengths have 1/1 ratio
  CV_MAT_ELEM(*intrinsics_left, float, 0, 0 ) = 1.0f;
  CV_MAT_ELEM(*intrinsics_left, float, 1, 1 ) = 1.0f;
  debug_message("cvCalibrateCamera2\n");
  cvCalibrateCamera2(opts_left, ipts_left, npts_left,
		     imsize_left, intrinsics_left,
		     distortion_left, NULL, NULL,
		     0);

  // Save for debug
  debug_message("Saving intrinsics and distortion in <Intrinsics_left.xml>\n  \
and <Distortion_left.xml>");
  cvSave("Intrinsics_left.xml",intrinsics_left);
  cvSave("Distortion_left.xml",distortion_left);
 
  // Set up rectification mapping
  mapx_left = cvCreateImage(imsize_left, IPL_DEPTH_32F, 1 );
  mapy_left = cvCreateImage(imsize_left, IPL_DEPTH_32F, 1 );
  rMapxy_left = cvCreateMat(imsize_left.height, imsize_left.width, CV_16SC2);
  rMapa_left  = cvCreateMat(imsize_left.height, imsize_left.width, CV_16SC1);
  debug_message("cvInitUndistortMap\n");
  cvInitUndistortMap(intrinsics_left, distortion_left, mapx_left, mapy_left);
  cvConvertMaps(mapx_left,mapy_left,rMapxy_left,rMapa_left);
  
  // Save rectified images in array and display
  // Should probably enable re-dispaly of original images
  for (int i=1; i<MAXIMAGES; i++)
    {
      if (!goodleft[i]) continue;
      if (rect_left[i] == NULL)
	rect_left[i] = cvCloneImage(imgs_left[i]);
      cvRemap(imgs_left[i], rect_left[i], mapx_left, mapy_left);
      IplImage *img = rect_left[i];
      set_current_tab_index(i);
      calImageWindow *cwin = get_current_left_win();
      cwin->clear2DFeatures();
      cwin->DisplayImage((unsigned char *)img->imageData, img->width, img->height, img->width);
    }


  // Calibrate right camera
  if (n_right < 3)		// not enough right images
    {
      debug_message("Only %d good right images, need 3\n", n_right);
      return;
    }

  // create matrices, total pts x 2 or 3
  // each row is a point
  CvMat *opts_right = cvCreateMat(n_right*num_pts, 3, CV_32FC1);
  CvMat *ipts_right = cvCreateMat(n_right*num_pts, 2, CV_32FC1);
  CvMat *npts_right = cvCreateMat(n_right, 1, CV_32SC1);

  // fill them from corner data
  ii = 0;
  for (int i=1; i<MAXIMAGES; i++)
    {
      if (!goodright[i]) continue;
      int k = ii*num_pts;	// matrix row index
      for (int j=0; j<num_pts; j++, k++)
	{
	  CV_MAT_ELEM(*ipts_right, float, k, 0) = (rightcorners[i])[j].x;
	  CV_MAT_ELEM(*ipts_right, float, k, 1) = (rightcorners[i])[j].y;
	  CV_MAT_ELEM(*opts_right, float, k, 0) = j/num_x_ints; // corner x pos
	  CV_MAT_ELEM(*opts_right, float, k, 1) = j%num_x_ints; // corner y pos
	  CV_MAT_ELEM(*opts_right, float, k, 2) = 0.0f; // corner z pos
	}
      CV_MAT_ELEM(*npts_right, int, ii, 0) = num_pts; // number of points
      ii++;
    }

  // save for debug
  cvSave("ipts.xml", ipts_right);
  cvSave("opts.xml", opts_right);

  // now call the cal routine
  CvMat* intrinsics_right = cvCreateMat(3,3,CV_32FC1);
  CvMat* distortion_right = cvCreateMat(4,1,CV_32FC1);
  // focal lengths have 1/1 ratio
  CV_MAT_ELEM(*intrinsics_right, float, 0, 0 ) = 1.0f;
  CV_MAT_ELEM(*intrinsics_right, float, 1, 1 ) = 1.0f;
  debug_message("cvCalibrateCamera2\n");
  cvCalibrateCamera2(opts_right, ipts_right, npts_right,
		     imsize_right, intrinsics_right,
		     distortion_right, NULL, NULL,
		     0);

  // Save for debug
  debug_message("Saving intrinsics and distortion in <Intrinsics_right.xml>\n  \
and <Distortion_right.xml>");
  cvSave("Intrinsics_right.xml",intrinsics_right);
  cvSave("Distortion_right.xml",distortion_right);
 
  // Set up rectification mapping
  mapx_right = cvCreateImage(imsize_right, IPL_DEPTH_32F, 1 );
  mapy_right = cvCreateImage(imsize_right, IPL_DEPTH_32F, 1 );
  rMapxy_right = cvCreateMat(imsize_left.height, imsize_left.width, CV_16SC2);
  rMapa_right  = cvCreateMat(imsize_left.height, imsize_left.width, CV_16SC1);
  debug_message("cvInitUndistortMap\n");
  cvInitUndistortMap(intrinsics_right, distortion_right, mapx_right, mapy_right);
  cvConvertMaps(mapx_right,mapy_right,rMapxy_right,rMapa_right);
  
  // Save rectified images in array and display
  // Should probably enable re-dispaly of original images
  for (int i=1; i<MAXIMAGES; i++)
    {
      if (!goodright[i]) continue;
      if (rect_right[i] == NULL)
	rect_right[i] = cvCloneImage(imgs_right[i]);
      cvRemap(imgs_right[i], rect_right[i], mapx_right, mapy_right);
      IplImage *img = rect_right[i];
      set_current_tab_index(i);
      calImageWindow *cwin = get_current_right_win();
      cwin->clear2DFeatures();
      cwin->DisplayImage((unsigned char *)img->imageData, img->width, img->height, img->width);
    }

}


//
// epipolar constraint check
// uses the left and right corner arrays
// <horz> is true (default) for horizontal epilines
//

double epi_scanline_error(bool horz)
{
  double sum = 0.0;
  int cnt = 0;
  for (int i=1; i<MAXIMAGES; i++)
    if (goodpair[i])		// have stereo images with corners found?
      {
	for (int j=0; j<nleftcorners[i]; j++)
	  {
	    double dd;
	    if (horz)
	      dd = (rectleftcorners[i][j].y - rectrightcorners[i][j].y);
	    else
	      dd = (rectleftcorners[i][j].x - rectrightcorners[i][j].x);
	    dd = dd*dd;
	    sum += dd;
	    cnt++;
	  }
      }
  if (cnt == 0) return 0.0;
  return sqrt(sum/((double)cnt));
}

// just for the first image
double epi_scanline_error0(bool horz)
{
  double sum = 0.0;
  int cnt = 0;
  if (goodpair[0])		// have stereo images with corners found?
    {
      for (int j=0; j<nleftcorners[0]; j++)
	{
	  double dd;
	  if (horz)
	    dd = (leftcorners[0][j].y - rightcorners[0][j].y);
	  else
	    dd = (leftcorners[0][j].x - rightcorners[0][j].x);
	  dd = dd*dd;
	  sum += dd;
	  cnt++;
	}
    }
  if (cnt == 0) return 0.0;
  return sqrt(sum/((double)cnt));
}



// stereo button state check
void
check_stereo_buttons()
{
  if (is3D)
    {
      isStereo = true;
      stg->stereo_button->value(true);
    }

  if (isStereo)
    {
      isRectify = true;
      stg->rectify_button->value(true);
    }
  isRefresh = true;
}

//
// do rectification
// assumes rectification warping information is available
//

void do_rectify_cb(Fl_Light_Button* w, void*)
{
  // set flag
  if (w->value())
    isRectify = true;
  else
    isRectify = false;

  // check state of buttons
  check_stereo_buttons();

  // check for video streaming
  if (isVideo)
    return;

  isRefresh = true;
}

//
// do stereo using new stereolib
// checks for stereo capability of STOC device, uses it
// else does stereo using stereolib on current images
// 

void do_stereo_cb(Fl_Light_Button* w, void*)
{
  // set flag
  if (w->value())
    isStereo = true;
  else
    isStereo = false;

  // check state of buttons
  check_stereo_buttons();

  // check for video streaming
  if (isVideo)
    return;

  // check for a loaded file
  if (isStereo && stIm)
    isRefresh = true;
}


//
// do 3d point calculation
// assumes stereo has been calculated
//

void do_3d_cb(Fl_Light_Button* w, void*)
{
  // set flag
  if (w->value())
    is3D = true;
  else
    is3D = false;

  // check state of buttons
  check_stereo_buttons();

  // check for video streaming
  if (isVideo)
    return;

  // check for a loaded file
  if (isStereo && stIm)
    isRefresh = true;
}


//
// track chessboard
//

void 
do_track_cb(Fl_Light_Button* w, void*)
{
  if (w->value())
    isTracking = true;
  else
    {
      isTracking = false;
      calImageWindow *cwin;
      cwin = get_current_left_win();
      cwin->clear2DFeatures();
      cwin = get_current_right_win();
      cwin->clear2DFeatures();
    }
}



// capture callback
// save un-rectified images into next open cal buffer

static int calInd = 1;

void cal_capture_cb(Fl_Button*, void*) 
{
  int w = dev->stIm->imWidth;
  int h = dev->stIm->imHeight;

  // left image
  if (img1 == NULL)
    img1 = cvCreateImageHeader(cvSize(w,h),IPL_DEPTH_8U,1);
  cvInitImageHeader(img1, cvSize(w,h), IPL_DEPTH_8U, 1);
  if (dev->stIm->imLeft->imType != COLOR_CODING_NONE)
    cvSetData(img1,dev->stIm->imLeft->im,w);  
  else
    {
      debug_message("[Capture] No image to capture");
      return;
    }

  if (calInd < 20)
    set_current_tab_index(calInd);
  calImageWindow *cwin = get_current_left_win();

  // convert to grayscale, should just do a clone
  // TBD: release previous image
  IplImage* img=cvCloneImage(img1);
  imgs_left[calInd] = img;
  imsize_left = cvGetSize(img);
  cwin->DisplayImage((unsigned char *)img->imageData, img->width, img->height, img->width);
  bool ret = FindCorners(&leftcorners[calInd], &nleftcorners[calInd], &goodleft[calInd], img);
  cwin->display2DFeatures(leftcorners[calInd],nleftcorners[calInd],ret);

  // right image
  if (dev->stIm->imRight->imType != COLOR_CODING_NONE)
    cvSetData(img1,dev->stIm->imRight->im,w);  
  else
    {
      debug_message("[Capture] No image to capture");
      return;
    }

  cwin = get_current_right_win();
  // convert to grayscale
  img = cvCloneImage(img1);
  imgs_right[calInd] = img;
  imsize_right = cvGetSize(img);
  debug_message("Size: %d x %d, ch: %d", img->width, img->height, img->nChannels);
  cwin->DisplayImage((unsigned char *)img->imageData, img->width, img->height, img->width);
  ret = FindCorners(&rightcorners[calInd], &nrightcorners[calInd], &goodright[calInd], img) && ret;
  cwin->display2DFeatures(rightcorners[calInd],nrightcorners[calInd],ret);

  if (ret)
    calInd++;
}



// save an image
// should save all current possibilities
//   currently just save grayscale images, if they exist

void
cal_save_image_cb(Fl_Button*, void*) 
{
}


void 
save_images(char *fname)
{
  // find suffix, remove it if there
  int n = strlen(fname);
  int sufn = 0;
  for (int i=n-1; i>n-6; i--)
    {
      if (fname[i] == '.')
	{
	  sufn = i;
	  break;
	}
    }
  
  char fn[4096];		// filename we want to use
  strcpy(fn,fname);
  if (sufn != 0)		// has suffix
    fn[sufn] = 0;
  else
    sufn = n;
  debug_message("[oST] Base name: %s", fn);

  int w = dev->stIm->imWidth;
  int h = dev->stIm->imHeight;
  IplImage *im = cvCreateImageHeader(cvSize(w,h),IPL_DEPTH_8U,1); 

  // write left image
  if (dev->stIm->imLeft->imRectType != COLOR_CODING_NONE)
    {
      cvSetData(im,dev->stIm->imLeft->imRect,w);
      strcpy(&fn[sufn],"-LR.png");
      cvSaveImage(fn, im);
      debug_message("[oST] Wrote %s", fn);
    }
  if (dev->stIm->imLeft->imType != COLOR_CODING_NONE)
    {
      cvSetData(im,dev->stIm->imLeft->im,w);
      strcpy(&fn[sufn],"-L.png");
      cvSaveImage(fn, im);
      debug_message("[oST] Wrote %s", fn);
    }

  // write right image
  if (dev->stIm->imRight->imRectType != COLOR_CODING_NONE)
    {
      cvSetData(im,dev->stIm->imRight->imRect,w);
      strcpy(&fn[sufn],"-RR.png");
      cvSaveImage(fn, im);
      debug_message("[oST] Wrote %s", fn);
    }
  if (dev->stIm->imRight->imType != COLOR_CODING_NONE)
    {
      cvSetData(im,dev->stIm->imRight->im,w);
      strcpy(&fn[sufn],"-R.png");
      cvSaveImage(fn, im);
      debug_message("[oST] Wrote %s", fn);
    }
}


// save all calibration images
// current save to name "cal-XXX"

void cal_save_all_cb(Fl_Button*, void*) 
{
  int ind = 1;
  char fname[4096];
  for (int i=1; i<MAXIMAGES; i++)
    {
      if (imgs_left[i] != NULL && imgs_right[i] != NULL)
	{
	  sprintf(fname, "cal%d-L.png", ind);
	  cvSaveImage(fname, imgs_left[i]);
	  sprintf(fname, "cal%d-R.png", ind);
	  cvSaveImage(fname, imgs_right[i]);
	  ind++;
	}
    }
  info_message("[oST] Saved %d calibration images", ind-1);
}




// get parameter string from current parameters
char *
cal_get_param_string()
{
  char *str = new char[4096];
  int n = 0;

  // header
  n += sprintf(str,"# oST version %d.%d parameters\n\n", OST_MAJORVERSION, OST_MINORVERSION);

  // externals
  n += sprintf(&str[n],"\n[externals]\n");

  n += sprintf(&str[n],"\ntranslation\n");
  n += PrintMatStr(&Transv,&str[n]);

  n += sprintf(&str[n],"\nrotation\n");
  n += PrintMatStr(&Rotv,&str[n]);

  // left camera
  n += sprintf(&str[n],"\n[left camera]\n");
  
  n += sprintf(&str[n],"\ncamera matrix\n");
  n += PrintMatStr(&K_left,&str[n]);

  n += sprintf(&str[n],"\ndistortion\n");
  n += PrintMatStr(&D_left,&str[n]);

  n += sprintf(&str[n],"\nrectification\n");
  n += PrintMatStr(&R_left,&str[n]);

  n += sprintf(&str[n],"\nprojection\n");
  n += PrintMatStr(&P_left,&str[n]);    

  // right camera
  n += sprintf(&str[n],"\n[right camera]\n");
  n += sprintf(&str[n],"\ncamera matrix\n");
  n += PrintMatStr(&K_right,&str[n]);

  n += sprintf(&str[n],"\ndistortion\n");
  n += PrintMatStr(&D_right,&str[n]);

  n += sprintf(&str[n],"\nrectification\n");
  n += PrintMatStr(&R_right,&str[n]);

  n += sprintf(&str[n],"\nprojection\n");
  n += PrintMatStr(&P_right,&str[n]);    

  str[n] = 0;			// just in case
  return str;
}


// set parameter string
void
cal_set_dev_params()
{
  if (!dev)
    return;			// no device yet...
  dev->camIm->params = dev->stIm->createParams(true);
}


// save the parameters to a file
void cal_save_params(char *fname)
{
  FILE *fp;
  fp = fopen(fname, "w");

  char *str = cal_get_param_string();
  fprintf(fp, "%s", str);
  fclose(fp);
  delete [] str;
}

void cal_save_params_cb(Fl_Button*, void*)
{
  char *fname = fl_file_chooser("Save params (.ini)", "{*.ini}", NULL);
  if (fname == NULL)
    return;

  // find suffix, add one if not there
  debug_message("[oST] File name: %s", fname);
  int n = strlen(fname);
  int sufn = 0;
  for (int i=n-1; i>n-6; i--)
    {
      if (fname[i] == '.')
	{
	  sufn = i;
	  break;
	}
    }
  
  char fn[1024];		// filename we want to use
  strcpy(fn,fname);
  if (sufn == 0)		// no suffix
    strcpy(&fn[n],".ini");

  cal_save_params(fn);
  debug_message("[oST] Wrote %s", fn);
}


// upload to device

void cal_upload_params_cb(Fl_Button*, void*)
{
  FILE *fp;
  int fd;
  if (dev)
    {
      cal_set_dev_params();	// set the parameters from the current values
      debug_message("[oST] Setting FW parameters");
      int ret = fl_choice("Really upload, erasing old camera calibration?","No","Yes",NULL);
      if (ret == 1)
	{
	  debug_message("[Dcam] Stopping device");
	  isVideo = false;
	  dev->stop();
	  stopCam = false;
	  dev->setParameters();
	}
      return;

      debug_message("[oST] Setting STOC parameters");
      // ok, load up things here...
      uint8_t *cbuf = NULL, *lbuf = NULL, *rbuf = NULL;
      int cn = 0, rn = 0, ln = 0;

      // set up choice for config filesptr
      int fwver = dev->camFirmware;
      int imver = dev->imFirmware;
      //      debug_message("[oST] FW firmware is %04x", fwver);

      // FW versions:
      //   < 5.0  no embedded warp table checksums
      //   5.0    embedded warp table checksums
      //
      // Cam versions
      //   < 6    standard 640 width, no horiz offset
      //   6      752 width, horiz pixel offset
      //
      // Mapping to STOC config files
      //   STOC   FW    CAM
      //   2.x    <5.0  <6
      //   3.x    <5.0   6+
      //   4.x     5+    6+
      //

      const char *types;
      if (dev->isColor)
	{
	  if (fwver < 0x0500 && imver < 6)
	    types = "{do_sys_color-2.*.bin,do_sys_color-2.*.dat}";
	  if (fwver < 0x0500 && imver >= 6)
	    types = "{do_sys_color-3.*.bin,do_sys_color-3.*.dat}";
	  if (fwver >= 0x0500 && imver >= 6)
	    types = "{do_sys_color-4.*.bin,do_sys_color-4.*.dat}";
	}
      else
	{
	  if (fwver < 0x0500 && imver < 6)
	    types = "{do_sys-2.*.bin,do_sys-2.*.dat}";
	  if (fwver < 0x0500 && imver >= 6)
	    types = "{do_sys-3.*.bin,do_sys-3.*.dat}";
	  if (fwver >= 0x0500 && imver >= 6)
	    types = "{do_sys-4.*.bin,do_sys-4.*.dat}";
	}

      char *name = fl_file_chooser("Config files", types, NULL);
      if (!name)
	return;
  
      debug_message("[Device] Config file name: %s", name);

      fp = fopen(name,"rb");
      if (fp == NULL)
	{
	  debug_message("[Device] Can't find file %s", name);
	  return;
	}
      else
	debug_message("[Device] Opened file %s", name);

      Fl::check();

      unsigned int xx = 0;
      int n = strlen(name);
      if (strcmp(&name[n-3], "dat") == 0) // scrambled
	xx = 0x1a2b3c4d;

      debug_message("[Device] Setting FPGA Configuration");

      // read in bytes, save them out to flash
      fd = fileno(fp);
#ifdef MSW
      _setmode(fd,_O_BINARY);
#endif

      cbuf = new uint8_t[512000];
      uint8_t *cptr = cbuf;

      // read in bytes from file
      while (cn < 512000)
	{
	  int ret = read(fd,cptr,4);
	  cn += ret;
	  if (ret < 4)
	    break;
	  // unscramble
	  xx = xx*xx+xx;
	  unsigned int *cint = (unsigned int *)cptr;
	  *cint = *cint ^ xx;
	  cptr += 4;
	}

      fclose(fp);
      debug_message("[Device] Read %d bytes from file", cn);


      dev->setSTOCParams(cbuf, cn, lbuf, ln, rbuf, rn);	// set the STOC firmware
    }
  else
    debug_message("[oST] No device selected");
}


// cal parameters

void cal_fixed_aspect_cb(Fl_Check_Button *w, void*)
{
  if (w->value())
    is_fixed_aspect = true;
  else
    is_fixed_aspect = false;
}


void cal_fixed_kappa2_cb(Fl_Check_Button *w, void*)
{
  if (w->value())
    use_kappa2 = true;
  else
    use_kappa2 = false;
}

void cal_fixed_kappa3_cb(Fl_Check_Button *w, void*)
{
  if (w->value())
    use_kappa3 = true;
  else
    use_kappa3 = false;
}

void cal_fixed_tau_cb(Fl_Check_Button *w, void*)
{
  if (w->value())
    use_tau = true;
  else
    use_tau = false;
}

void cal_zero_disparity_cb(Fl_Check_Button *w, void*)
{
  if (w->value())
    is_zero_disparity = true;
  else
    is_zero_disparity = false;
}


// callbacks for changing the number of intersections in target
// and the target size

void cal_check_size_cb(Fl_Value_Input *w, void*)
{
  squareSize = w->value()/1000.0; // size in mm
  debug_message("Target square size is %d mm", (int)w->value());
}

void cal_check_x_cb(Fl_Value_Input *w, void*)
{
  num_x_ints = (int)w->value();
  debug_message("Target X corners: %d", num_x_ints);
  num_pts = num_x_ints*num_y_ints;
}


void cal_check_y_cb(Fl_Value_Input *w, void*)
{
  num_y_ints = (int)w->value();
  debug_message("Target Y corners: %d", num_y_ints);
  num_pts = num_x_ints*num_y_ints;
}



// stereo parameter callbacks

void stereo_window_cb(Fl_Menu_ *w, void *u)
{
  stg->stereo_window->show();
}

// video size
void
stereo_algorithm_cb(Fl_Choice *w, void *u)
{
  Fl_Menu_ *mw = (Fl_Menu_ *)w;
  const Fl_Menu_Item* m = mw->mvalue();
  
  //printf("Label: %s\n", m->label());

  if (!strcmp(m->label(), "Normal"))
    {
	sp_alg = NORMAL_ALGORITHM;      
	//printf("Normal algorithm\n");
    }
  if (!strcmp(m->label(), "Scanline Opt"))
    {
      sp_alg = SCANLINE_ALGORITHM;    
      //printf("Scanline algorithm\n");
    }
 if (!strcmp(m->label(), "DP"))
    {
      sp_alg = DP_ALGORITHM;    
     // printf("DP algorithm\n");
    }
 if (!strcmp(m->label(), "Multiple Windows"))
    {
      sp_alg = MW_ALGORITHM;    
     // printf("MW algorithm\n");
    }

 if (!strcmp(m->label(), "Local Smoothness"))
    {
      sp_alg = LS_ALGORITHM;    
     // printf("MW algorithm\n");
    }

 if (!strcmp(m->label(), "NCC"))
    {
      sp_alg = NCC_ALGORITHM;    
     // printf("MW algorithm\n");
    }
 isRefresh = true;
}


void
disparity_cb(Fl_Counter *w, void *x)
{
  sp_dlen = (int)w->value();
  if (dev)
    dev->setNumDisp(sp_dlen);
  if (stIm)
    stIm->setNumDisp(sp_dlen);
  isRefresh = true;

}

void
unique_cb(Fl_Counter *w, void *x)
{
  sp_uthresh = (int)w->value();
  if (dev)
    dev->setUniqueThresh(sp_uthresh);
  if (stIm)
    stIm->setUniqueThresh(sp_uthresh);
  isRefresh = true;

}

void
texture_cb(Fl_Counter *w, void *x)
{
  sp_tthresh = (int)w->value();
  if (dev)
    dev->setTextureThresh(sp_tthresh);
  if (stIm)
    stIm->setTextureThresh(sp_tthresh);
  isRefresh = true;

}

void
smoothness_cb(Fl_Counter *w, void *x)
{
  sp_sthresh = (int)w->value();
  if (dev)
    dev->setSmoothnessThresh(sp_sthresh);
  if (stIm)
    stIm->setSmoothnessThresh(sp_sthresh);
  isRefresh = true;

}

void
xoff_cb(Fl_Counter *w, void *x)
{
  sp_xoff = (int)w->value();
  if (dev)
    dev->setHoropter(sp_xoff);
  if (stIm)
    stIm->setHoropter(sp_xoff);
  isRefresh = true;

}

void
corrsize_cb(Fl_Counter *w, void *x)
{
  static int pcorr = 15;
  int c = (int)w->value();
  if (!(c & 0x1))		// even
    {
      if (c > pcorr)
	c++;
      else
	c--;
      w->value(c);
    }
  sp_corr = pcorr = c;
  if (dev)
    dev->setCorrsize(sp_corr);
  if (stIm)
    stIm->setCorrSize(sp_corr);
  isRefresh = true;

}

void
speckle_size_cb(Fl_Counter *w, void *x)
{
  sp_ssize = (int)w->value();
  if (dev)
    dev->setSpeckleSize(sp_ssize);
  if (stIm)
    stIm->setSpeckleRegionSize(sp_ssize);
  isRefresh = true;
}

void
speckle_diff_cb(Fl_Counter *w, void *x)
{
  sp_sdiff = (int)w->value();
  if (dev)
    dev->setSpeckleDiff(sp_sdiff);
  if (stIm)
    stIm->setSpeckleDiff(sp_sdiff);
  isRefresh = true;

}

void unique_check_cb(Fl_Light_Button* w, void*)
{
  // set flag
  if (w->value())
    is_unique_check = true;
  else
    is_unique_check = false;

  if (dev)
    dev->setUniqueCheck(is_unique_check);
  if (stIm)
    stIm->setUniqueCheck(is_unique_check);
  isRefresh = true;
 }


//
// video parameters
//

void
do_auto_exposure_cb(Fl_Light_Button *w, void *x)
{
  if (dev)
    {
      if (w->value())		// turn it on
	dev->setExposure(0,true);
      else
	{
	  int val = (int)stg->exposure_val->value();
	  dev->setExposure(val,false);
	}
    }
}


void
do_exposure_cb(Fl_Slider *w, void *x)
{
  if (dev)
    {
      int val = (int)w->value();
      dev->setExposure(val,false);
      stg->exposure_auto_button->value(false);
    }
}


void
do_auto_gain_cb(Fl_Light_Button *w, void *x)
{
  if (dev)
    {
      if (w->value())		// turn it on
	dev->setGain(0,true);
      else
	{
	  int val = (int)stg->gain_val->value();
	  dev->setGain(val,false);
	}
    }
}


void
do_gain_cb(Fl_Slider *w, void *x)
{
  if (dev)
    {
      int val = (int)w->value();
      dev->setGain(val,false);
      stg->gain_auto_button->value(false);
    }
}


void
do_auto_brightness_cb(Fl_Light_Button *w, void *x)
{
  if (dev)
    {
      if (w->value())		// turn it on
	dev->setBrightness(0,true);
      else
	{
	  int val = (int)stg->brightness_val->value();
	  dev->setBrightness(val,false);
	}
    }
}


void
do_brightness_cb(Fl_Slider *w, void *x)
{
  if (dev)
    {
      int val = (int)w->value();
      dev->setBrightness(val,false);
      stg->brightness_auto_button->value(false);
    }
}


void
do_gamma_cb(Fl_Light_Button *w, void *x)
{
  if (dev)
    {
      if (w->value())		// turn it on
	dev->setCompanding(true);
      else
	dev->setCompanding(false);
    }
}


void
mode_cb(Fl_Counter *w, void *x)
{
  int mode = (int)w->value();
  if (dev)
    dev->setProcMode((videre_proc_mode_t)mode);
  isRefresh = true;
}


// parsing file names

// parses a sequence number, if it exists
// <n> is just after the rightmost number of the putative sequence
// returns the number of chars in the sequence
// also sets <num> to the current value of the sequence
//

int 
parse_sequence(char *fname, int n, int *num)
{
  int j = n-1;
  int sum = 0;
  int fact = 1;
  while (j >= 0)
    {
      if (isdigit(fname[j]))	// ok, have a sequence digit
	sum += fact*(fname[j]-'0');
      else
	break;
      j--;
      fact *= 10;
    }

  if (j == n-1)			// didn't find a digit
    return -1;

  *num = sum;
  return n-1-j;			// number of digits
}

// parses a file name to see if it is a sequence or stereo pair
//    or both
// returns TRUE if so
// lname is non-null if mono or stereo image is indicated
// rname is non-null if stereo is indicated
// num is -1 for no sequence, else sequence start number
//       

bool
parse_filename(char *fname, char **lname, char **rname, int *num, char **bname)
{
  if (num)
    *num = -1;			// default
  char *lbase = NULL, *rbase = NULL;
  if (fname == NULL)
    return false;
  int n = strlen(fname);
  if (n < 5)
    return false;

  // find suffix
  int sufn = 0;
  for (int i=n-1; i>n-6; i--)
    {
      if (fname[i] == '.')
	{
	  sufn = i;
	  break;
	}
    }
  
  if (sufn < 3)
    {
      if (bname)
	{
	  lbase = new char[n+1];
	  strcpy(lbase,fname);      
	  *bname = lbase;
	  *lname = NULL;
	  *rname = NULL;
	  return true;
	}
      return false;
    }

  // find start of base file name
  int basen = 0;
  for (int i=sufn-1; i>=0; i--)
    {
      if (fname[i] == '/')
	{
	  basen = i;
	  break;
	}
    }
  if (basen > 0)		// go to first letter of base name
    basen++;



  // check for -L/R type endings
  int stn = sufn-2;		// start of -L/R
  int seq = -1;

  if (strncmp(&fname[stn],"-L",2) == 0)
    {
      // check for sequence
      if (num)
	seq = parse_sequence(fname,stn,num);
      if (seq < 0)		// nope
	{	
	  // just return stereo names
	  lbase = new char[n+1];
	  rbase = new char[n+1];
	  strcpy(lbase,fname);
	  strcpy(rbase,fname);
	  rbase[stn+1] = 'R';
	  debug_message("lbase is [%s]\nrbase is [%s]\n", lbase, rbase);
	  *lname = lbase;
	  *rname = rbase;
	}
      else			// yup, add in sequence
	{
	  lbase = new char[n-seq+5];
	  rbase = new char[n-seq+5];
	  strcpy(lbase,fname);
	  lbase[stn-seq] = 0;	// terminate before sequence
	  strcat(lbase,"%0");
	  sprintf(&lbase[stn-seq+2],"%d",seq);
	  strcat(lbase,"d-L");
	  strcat(lbase,&fname[sufn]); // suffix
	  strcpy(rbase,lbase);
	  rbase[stn-seq+4+1] = 'R';
	  debug_message("lbase is [%s]\nrbase is [%s]\n", lbase, rbase);
	  *lname = lbase;
	  *rname = rbase;
	}

      if (bname)		// output base name only
	{
	  delete [] rbase;
	  lbase[stn] = 0;
	  *bname = lbase;
	  *lname = NULL;
	  *rname = NULL;
	}
      return true;
    }

  // check for left/right type beginnings
  else if (strncmp(&fname[basen],"left",4) == 0)
    {
      // check for sequence
      if (num)
        seq = parse_sequence(fname,sufn,num);
      if (seq < 0)		// nope
	{	
	  // just return stereo names
	  lbase = new char[n+1];
	  rbase = new char[n+2];
	  strcpy(lbase,fname);
	  strcpy(rbase,fname);
	  strcpy(&rbase[basen],"right");
	  strcpy(&rbase[basen+5],&fname[basen+4]);
	  debug_message("lbase is [%s]\nrbase is [%s]\n", lbase, rbase);
	  *lname = lbase;
	  *rname = rbase;
	  return true;
	}
      else			// yup, add in sequence
	{
	  lbase = new char[n-seq+5];
	  rbase = new char[n-seq+6];
	  strcpy(lbase,fname);
	  lbase[sufn-seq] = 0;	// terminate before sequence
	  strcat(lbase,"%0");
	  sprintf(&lbase[sufn-seq+2],"%d",seq);
	  strcat(lbase,"d");
	  strcat(lbase,&fname[sufn]); // suffix
	  strcpy(rbase,lbase);
	  strcpy(&rbase[basen],"right");
	  strcpy(&rbase[basen+5],&lbase[basen+4]);
	  debug_message("lbase is [%s]\nrbase is [%s]\n", lbase, rbase);
	  *lname = lbase;
	  *rname = rbase;
	  return true;
	  
	}

      if (bname)		// output base name only
	{
	  delete [] rbase;
	  lbase[basen] = 0;
	  *bname = lbase;
	  *lname = NULL;
	  *rname = NULL;
	}
      return true;


    }

  else if (bname)
    {
      lbase = new char[n+1];
      strcpy(lbase,fname);      
      lbase[sufn] = 0;
      *bname = lbase;
      *lname = NULL;
      *rname = NULL;
      return true;
    }

  return false;
}


//
// Calibration dialog
//

void cal_window_cb(Fl_Menu_ *w, void *u)
{
  stg->cal_window->show();
  stg->cal_images->show();
  set_current_tab_index(1);	// set to first calibration image
}



//
// loading images into stIm
// assumes image file names have suffixes -L/-LR/-LC [.png]
//

uint8_t *
load_png_grayscale(char *fname, color_coding_t *cc, int *width, int *height, int *size);


// returns number of images loaded

int
load_left(char *fname)		// fname is the base name, to be added to
{
  int w,h,s;
  color_coding_t cc;
  char fn[4096];
  int ret = 0;

  stIm = &stImdata;		// set the stereo data ptr
  stIm->imLeft->imType = COLOR_CODING_NONE;
  stIm->imLeft->imColorType = COLOR_CODING_NONE;
  stIm->imLeft->imRectType = COLOR_CODING_NONE;
  stIm->imLeft->imRectColorType = COLOR_CODING_NONE;

  sprintf(fn,"%s-L.png",fname);
  debug_message("[oST] Trying file %s\n", fn);
  uint8_t *data = load_png_grayscale(fn, &cc, &w, &h, &s);
  if (data != NULL)
    {
      debug_message("Size: %d x %d, cc: %d", w, h, cc);
      stIm->imLeft->im = data;
      stIm->imLeft->imType = cc;
      stIm->setSize(w,h);
      ret++;
    }

  sprintf(fn,"%s-LR.png",fname);
  debug_message("[oST] Trying file %s\n", fn);
  data = load_png_grayscale(fn, &cc, &w, &h, &s);
  if (data != NULL)
    {
      debug_message("Size: %d x %d, cc: %d", w, h, cc);
      stIm->imLeft->imRect = data;
      stIm->imLeft->imRectType = cc;
      stIm->setSize(w,h);
      ret++;
    }

  return ret;
}

int
load_right(char *fname)		// fname is the base name, to be added to
{
  int w,h,s;
  color_coding_t cc;
  char fn[4096];
  int ret = 0;

  stIm = &stImdata;		// set the stereo data ptr
  stIm->imRight->imType = COLOR_CODING_NONE;
  stIm->imRight->imColorType = COLOR_CODING_NONE;
  stIm->imRight->imRectType = COLOR_CODING_NONE;
  stIm->imRight->imRectColorType = COLOR_CODING_NONE;

  sprintf(fn,"%s-R.png",fname);
  uint8_t *data = load_png_grayscale(fn, &cc, &w, &h, &s);
  if (data != NULL)
    {
      debug_message("Size: %d x %d, cc: %d", w, h, cc);
      stIm->imRight->im = data;
      stIm->imRight->imType = cc;
      stIm->setSize(w,h);
      ret++;
    }

  sprintf(fn,"%s-RR.png",fname);
  data = load_png_grayscale(fn, &cc, &w, &h, &s);
  if (data != NULL)
    {
      debug_message("Size: %d x %d, cc: %d", w, h, cc);
      stIm->imRight->imRect = data;
      stIm->imRight->imRectType = cc;
      stIm->setSize(w,h);
      ret++;
    }

  return ret;
}


//
// parses a file name to see if it is a sequence or stereo pair
//    or both
// returns TRUE if so
// lname is non-null if mono or stereo image is indicated
// rname is non-null if stereo is indicated
// num is -1 for no sequence, else sequence start number
//       

bool
parseit(char *fname, char **lname, char **rname, int *num, char **bname)
{
  if (num)
    *num = -1;			// default
  char *lbase = NULL, *rbase = NULL, *base = NULL;
  if (fname == NULL)
    return false;
  int n = strlen(fname);
  if (n < 5)
    return false;

  // find suffix
  int sufn = n;
  for (int i=n-1; i>n-6; i--)
    {
      if (fname[i] == '.')
	{
	  sufn = i;
	  break;
	}
    }
  

  // find start of base file name
  int basen = 0;
  for (int i=sufn-1; i>=0; i--)
    {
      if (fname[i] == '/')
	{
	  basen = i;
	  break;
	}
    }
  if (basen > 0)		// go to first letter of base name
    basen++;



  // check for -L/R type endings
  int stn = sufn-2;		// start of -L/R
  int seq = -1;

  if (strncmp(&fname[stn],"-L",2) == 0 || 
      strncmp(&fname[stn],"-R",2) == 0 || 
      strncmp(&fname[stn-1],"-LR",3) == 0 || 
      strncmp(&fname[stn-1],"-RR",3) == 0)
    {
      if (strncmp(&fname[stn-1],"-LR",3) == 0 || 
	  strncmp(&fname[stn-1],"-RR",3) == 0)
	stn--;

      // check for sequence
      if (num)
	seq = parse_sequence(fname,stn,num);
      if (seq < 0)		// nope
	{	
	  // just return stereo names
	  lbase = new char[n+1];
	  rbase = new char[n+1];
	  base = new char[n+1];
	  strcpy(lbase,fname);
	  strcpy(rbase,fname);
	  strcpy(base,fname);
	  rbase[stn+1] = 'R';
	  lbase[stn+1] = 'L';
	  base[stn] = 0;	// base name
	  debug_message("lbase is [%s]\nrbase is [%s]\n", lbase, rbase);
	  *lname = lbase;
	  *rname = rbase;
	  *bname = base;
	}
      else			// yup, add in sequence
	{
	  lbase = new char[n-seq+5];
	  rbase = new char[n-seq+5];
	  strcpy(lbase,fname);
	  lbase[stn-seq] = 0;	// terminate before sequence
	  strcat(lbase,"%0");
	  sprintf(&lbase[stn-seq+2],"%d",seq);
	  strcat(lbase,"d-L");
	  strcat(lbase,&fname[sufn]); // suffix
	  strcpy(rbase,lbase);
	  rbase[stn-seq+4+1] = 'R';
	  debug_message("lbase is [%s]\nrbase is [%s]\n", lbase, rbase);
	  *lname = lbase;
	  *rname = rbase;
	}

      return true;
    }

  return false;
}




//
// Loading and saving stereo images
//

bool load_params(char *fname);

void load_images_cb(Fl_Menu_ *w, void *u)
{
  char *fname = fl_file_chooser("Load file", NULL, NULL);
  if (fname == NULL)
    return;

  debug_message("[oST] File name: %s", fname);

  char *lname = NULL, *rname = NULL, *bname = NULL;
  bool ret;
  ret = parseit(fname, &lname, &rname, NULL, &bname);

  if (!ret)			// just load left
    load_left(bname);
  else
    {
      load_left(bname);
      load_right(bname);
    }

  char fn[4096];
  sprintf(fn,"%s.ini",bname);
  load_params(fn);

  isRefresh = true;
}


//
// save all types of images available: left, right, rectified, color
// 
void save_images_cb(Fl_Menu_ *w, void *u)
{
  if (!dev)
    {
      debug_message("[oST] No open device");
      return;
    }

  char *fname = fl_file_chooser("Save images (.png)", "{*.png}", NULL);
  if (fname == NULL)
    return;

  save_images(fname);
}


//
// add a suffix if no one is present
// always returns a newly-minted char string
//

char *
add_suffix(char *fname, char *suf)
{
  int n = strlen(fname);
  if (n < 5) return strdup(fname);

  // find suffix
  int sufn = n;
  for (int i=n-1; i>n-4; i--)
    {
      if (fname[i] == '.')
	{
	  sufn = i;
	  break;
	}
    }
  
  if (sufn == n) return strdup(fname); // have a suffix
  char *fn = new char[n+strlen(suf)+1];
  sprintf(fn,"%s.%s",fname,suf);
  return fn;
}


//
// save the 3D point cloud as a .pcd file
//   -- not sure if the ARRAY line is valid PCD syntax
// format: w x h, then 1 line per point over full image, XYZA
//  A is the disparity for a good point, 0 for a bad one
//

void
save_3d_cb(Fl_Menu_ *w, void *u)
{
  if (!stIm || !stIm->hasDisparity)
    {
      debug_message("[OST] No disparity image");
      return;			// no disparity image
    }

  char *fname = fl_file_chooser("Save point file (.pcd)", "{*.pcd}", NULL);
  if (fname == NULL)
    return;

  fname = add_suffix(fname,(char*)"pcd");

  stIm->doCalcPts(true);
  FILE *fp = fopen(fname,"w");
  fprintf(fp, "COLUMNS x y z d\n");
  fprintf(fp, "POINTS %d\n", stIm->imWidth*stIm->imHeight);
  fprintf(fp, "ARRAY %d %d\n", stIm->imWidth, stIm->imHeight);
  fprintf(fp, "DATA ascii\n");

  pt_xyza_t *pts = stIm->imPtArray();
  int16_t *ds = stIm->imDisp;
  for (int i=0; i<stIm->imHeight; i++)
    for (int j=0; j<stIm->imWidth; j++, pts++, ds++)
      fprintf(fp, "%f %f %f %d\n", pts->X, pts->Y, pts->Z, *ds);
  fclose(fp);
  delete[](fname);
}



// saving and loading parameters

bool
load_params(char *fname)
{
  debug_message("[oST] File name: %s", fname);
  FILE *fp = fopen(fname,"r");
  if (fp == NULL)
    {
      debug_message("[oST] Can't read file %s",fname);
      return false;
    }
  
  char bb[4096];
  int n = fread(bb,1,4095,fp);
  fclose(fp);
  debug_message("[oST] Read %d bytes",n);
  
  if (n > 0)
    {
      bb[n] = 0;
      stIm->extractParams(bb);	// set up parameters
      if (dev) dev->putParameters(bb);	// save in device object
      return true;
    }
  return false;
}


void load_params_cb(Fl_Menu_ *w, void *u)
{
  if (!dev) return;		// no device
  char *fname = fl_file_chooser("Load params (.ini)", "{*.ini}", NULL);
  if (fname == NULL)
    return;

  load_params(fname);
}



//
// saves parameters, append ".ini" suffix if there is no suffix
//

void save_params_cb(Fl_Menu_ *w, void *u)
{
  if (!dev) return;		// no device
  char *fname = fl_file_chooser("Save params (.ini)", "{*.ini}", NULL);
  if (fname == NULL)
    return;

  // find suffix, add one if not there
  debug_message("[oST] File name: %s", fname);
  int n = strlen(fname);
  int sufn = 0;
  for (int i=n-1; i>n-6; i--)
    {
      if (fname[i] == '.')
	{
	  sufn = i;
	  break;
	}
    }
  
  char fn[1024];		// filename we want to use
  strcpy(fn,fname);
  if (sufn == 0)		// no suffix
    strcpy(&fn[n],".ini");
  char *bb = dev->stIm->createParams(); // get parameter string from current params
  FILE *fp = fopen(fn,"w");
  if (fp == NULL)
    {
      debug_message("[oST] Can't write to file %s",fn);
      return;
    }
  
  fprintf(fp, "%s", bb);
  fclose(fp);
  debug_message("[oST] Wrote parameters");
}


//
// uploads params to device
//

void upload_params_cb(Fl_Menu_ *w, void *u)
{
  if (!dev) return;		// no device
  int ret = fl_choice("Really upload, erasing old camera calibration?","No","Yes",NULL);
  if (ret == 1)
    {
      debug_message("[Dcam] Stopping device");
      isVideo = false;
      dev->stop();
      stopCam = false;
      dev->setParameters();
    }
}


//
// debug window
//

// pop it up
void 
debug_window_cb(Fl_Menu_ *w, void *u)
{
  iwin->show();
}



//
// video
//


// select device
void
video_dev_cb(Fl_Choice *w, void *u)
{
  if (numDevs > 0)
    {
      int devn = w->value();
      debug_message("[oST] Camera %d selected", devn);
    }
  else
    debug_message("[oST] No cameras found");
}


// pop it up
void 
video_window_cb(Fl_Menu_ *w, void *u)
{
  stg->video_window->show();
}

// video size
// set up correct video mode based on this 
void
video_size_cb(Fl_Choice *w, void *u)
{
  Fl_Menu_ *mw = (Fl_Menu_ *)w;
  const Fl_Menu_Item* m = mw->mvalue();
  
  if (!strcmp(m->label(), "640x480"))
    videoSize = SIZE_640x480;
  if (!strcmp(m->label(), "1280x960"))
    videoSize = SIZE_1280x960;
  if (!strcmp(m->label(), "320x240"))
    videoSize = SIZE_320x240;

}

// video rate
void
video_rate_cb(Fl_Choice *w, void *u)
{
  Fl_Menu_ *mw = (Fl_Menu_ *)w;
  const Fl_Menu_Item* m = mw->mvalue();
  
  if (!strcmp(m->label(), "30 Hz"))
    videoRate = DC1394_FRAMERATE_30;
  else if (!strcmp(m->label(), "15 Hz"))
    videoRate = DC1394_FRAMERATE_15;
  else if (!strcmp(m->label(), "7.5 Hz"))
    videoRate = DC1394_FRAMERATE_7_5;
  else if (!strcmp(m->label(), "3.75 Hz"))
    videoRate = DC1394_FRAMERATE_3_75;
}



// start up video
void 
do_video_cb(Fl_Light_Button *w, void*)
{
  if (w->value())
    {
      // first check for having a device
      if (numDevs == 0)
	{
	  debug_message("[oST] No devices found");
	  w->value(0);
	  return;
	}

      Fl_Choice *devs = stg->cam_select;
      if (!dev || devIndex != devs->value()) // have the same device?
	{
	  devIndex = devs->value(); // device number
	  debug_message("[oST] Opening camera #%d", devIndex);
	  try {  dev = initcam(dcam::getGuid(devIndex)); }
	  catch(dcam::DcamException e) 
	    { 
	      debug_message("Can't find or open a camera: %s", e.what());
	      dev = NULL;		// just in case
	      return;
	    }

	  // set range max value
	  dev->setRangeMax(4.0);	// in meters
	  dev->setRangeMin(0.5);	// in meters
	}
      startCam = true;
    }

  else				// turn it off
    stopCam = true;
}

// color in video
void 
do_color_cb(Fl_Light_Button *w, void*)
{
  if (dev && !dev->isColor)
    {
      w->value(false);
      isColor = false;
      return;
    }

  if (w->value())		// turn it on
    isColor = true;
  else
    isColor = false;
}

// STOC processing
void 
do_stoc_cb(Fl_Light_Button *w, void*)
{
  if (dev && !dev->isSTOC)
    {
      w->value(false);
      useSTOC = false;
      return;
    }

  if (w->value())		// turn it on
    useSTOC = true;
  else
    useSTOC = false;
}


// exit
void
do_exit_cb(Fl_Menu_ *x, void *)
{
  isExit = true;
}


//
// button handler
//


// mouse button callback

int
do_button(int e, int x, int y, int b, int m, imWindow *w)
{
  x = w->Win2ImX(x);
  y = w->Win2ImY(y);

  //  printf("[button] %d,%d\n", x,y);

  if (e == FL_PUSH && b == FL_BUTTON1) // left button, return values
    {
      int rval = 0, lval = 0;
      float fx=0, fy=0, fz=0;
      
      if (stIm == NULL)
        return 1;
      
      int w = stIm->imWidth;
      int h = stIm->imHeight;

      if (x > w || y > h)
        return 1;

      if (stIm->imLeft->imRectType != COLOR_CODING_NONE)
	lval = stIm->imLeft->imRect[y*w+x];
      else if (stIm->imLeft->imType != COLOR_CODING_NONE)
	lval = stIm->imLeft->im[y*w+x];

      if (stIm->flim)
	lval = (int8_t)stIm->flim[y*w+x] - 31;

      if (stIm->imRight->imRectType != COLOR_CODING_NONE)
	rval = stIm->imRight->imRect[y*w+x];
      else if (stIm->imRight->imType != COLOR_CODING_NONE)
	rval = stIm->imRight->im[y*w+x];

      if (stIm->hasDisparity)
        {
	  rval = stIm->imDisp[y*w+x];
          if (rval > 0)     // have a valid disparity
	    stIm->calcPt(x,y,&fx, &fy, &fz);
        }

      debug_message("[oST] (%d,%d) [v%d] [dv%d] X%d Y%d Z%d", 
		    x, y, lval, rval, (int)(1000*fx), (int)(1000*fy), (int)(1000*fz));
      return 1;
    }

  return 0;
}


//
// utilities
//

void PrintMat(CvMat *A, FILE *fp)
{
  int i, j;
  for (i = 0; i < A->rows; i++)
    {
      switch (CV_MAT_DEPTH(A->type))
	{
	case CV_32F:
	case CV_64F:
	  for (j = 0; j < A->cols; j++)
	    fprintf (fp,"%8.5f ", (float)cvGetReal2D(A, i, j));
	  break;
	case CV_8U:
	case CV_16U:
	  for(j = 0; j < A->cols; j++)
	    fprintf (fp,"%6d",(int)cvGetReal2D(A, i, j));
	  break;
	default:
	  break;
	}
      fprintf(fp,"\n");
    }
}

int
PrintMatStr(CvMat *A, char *str)
{
  int i, j;
  int n = 0;
  for (i = 0; i < A->rows; i++)
    {
      switch (CV_MAT_DEPTH(A->type))
	{
	case CV_32F:
	case CV_64F:
	  for (j = 0; j < A->cols; j++)
	    n += sprintf(&str[n],"%8.5f ", (float)cvGetReal2D(A, i, j));
	  break;
	case CV_8U:
	case CV_16U:
	  for(j = 0; j < A->cols; j++)
	    n += sprintf(&str[n],"%6d",(int)cvGetReal2D(A, i, j));
	  break;
	default:
	  break;
	}
      n += sprintf(&str[n],"\n");
    }
  return n;
}


// gets time in ms

#ifdef WIN32
LARGE_INTEGER freq={0,0};
double get_ms()
{
  double a;
  LARGE_INTEGER perf;
  //  if (freq.LowPart == 0 && freq.HighPart == 0)
  if (freq.QuadPart == 0)
    QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&perf);
  a = (double)perf.QuadPart / (double)freq.QuadPart;
  return 1000*a;
}
#else
double get_ms()
{
  struct timeval t0;
  gettimeofday(&t0,NULL);
  double ret = t0.tv_sec * 1000.0;
  ret += ((double)t0.tv_usec)*0.001;
  return ret;
}
#endif


// sleeps in ms
#ifdef WIN32
void
wait_ms(int ms)
{
  Sleep(ms);
}
#else
void
wait_ms(int ms)
{
    struct timeval delay;
    fd_set readfds, writefds, execfds;
    delay.tv_sec = ms / 1000;
    delay.tv_usec = (ms % 1000) * 1000;
    select (0, &readfds, &writefds, &execfds, &delay);
}
#endif
