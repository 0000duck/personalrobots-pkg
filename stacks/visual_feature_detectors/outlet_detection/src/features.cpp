/*
 *  features.cpp
 *  online_patch
 *
 *  Created by Victor  Eruhimov on 4/23/09.
 *  Copyright 2009 Argus Corp. All rights reserved.
 *
 */

#include "outlet_detection/features.h"
#include "outlet_detection/outlet_model.h"
#include <highgui.h>

void GetSURFFeatures(IplImage* src, vector<feature_t>& features)
{
    CvMemStorage* storage = cvCreateMemStorage();
    CvSeq* surf_points = 0;
    cvExtractSURF(src, 0, &surf_points, 0, storage, cvSURFParams(512));
    
    features.clear();
    for(int i = 0; i < surf_points->total; i++)
    {
        CvSURFPoint* point = (CvSURFPoint*)cvGetSeqElem(surf_points, i);
        CvPoint center = cvPoint(point->pt.x, point->pt.y);
        features.push_back(feature_t(center, (float)point->size));
    }
    
    cvReleaseMemStorage(&storage);
}

void GetStarFeatures(IplImage* src, vector<feature_t>& features)
{
    CvMemStorage* storage = cvCreateMemStorage();
    CvSeq* star_points = cvGetStarKeypoints(src, storage, cvStarDetectorParams(45));
    
    features.clear();
    for(int i = 0; i < star_points->total; i++)
    {
        CvStarKeypoint* keypoint = (CvStarKeypoint*)cvGetSeqElem(star_points, i);
        features.push_back(feature_t(keypoint->pt, (float)keypoint->size));
    }
    
    cvReleaseMemStorage(&storage);
}

void GetHarrisFeatures(IplImage* src, vector<feature_t>& features)
{
    IplImage* grey = src;
    if(src->nChannels > 1)
    {
        grey = cvCreateImage(cvSize(src->width, src->height), IPL_DEPTH_8U, 1);
        cvCvtColor(src, grey, CV_RGB2GRAY);
    }
    
    
    IplImage* eig_img = cvCreateImage(cvSize(src->width, src->height), IPL_DEPTH_32F, 1);
    IplImage* temp_img = cvCloneImage(eig_img);
    
    int corner_count = 1024;
    CvPoint2D32f* corners = new CvPoint2D32f[corner_count];
    cvGoodFeaturesToTrack(grey, eig_img, temp_img, corners, &corner_count, .5, 0);//, 0, 3, 1);
    
    for(int i = 0; i < corner_count; i++)
    {
        features.push_back(feature_t(cvPoint(corners[i].x, corners[i].y), 1.0f));
    }
    
    if(src->nChannels > 1)
    {
        cvReleaseImage(&grey);
    }
    cvReleaseImage(&eig_img);
    cvReleaseImage(&temp_img);
}

void GetHoleFeatures(IplImage* src, vector<feature_t>& features, float hole_contrast)
{
    vector<outlet_feature_t> outlet_features;
    find_outlet_features_fast(src, outlet_features, hole_contrast, 0, 0);
    for(size_t i = 0; i < outlet_features.size(); i++)
    {
        features.push_back(feature_t(feature_center(outlet_features[i]), outlet_features[i].bbox.width));
    }
}

void DrawFeatures(IplImage* img, const vector<feature_t>& features)
{
    for(size_t i = 0; i < features.size(); i++)
    {
        cvCircle(img, features[i].pt, features[i].size, CV_RGB(255, 0, 0), 2);
    }
}

void FilterFeatures(vector<feature_t>& features, float min_scale, float max_scale)
{
    vector<feature_t> selected;
    for(size_t i = 0; i < features.size(); i++)
    {
        if(features[i].size >= min_scale && features[i].size <= max_scale)
        {
            selected.push_back(features[i]);
        }
    }
    
    features = selected;
}

void SelectNeighborFeatures(vector<feature_t>& features, const vector<feature_t>& voc)
{
    const int max_dist = 10;
    vector<feature_t> filtered;
    for(int i = 0; i < (int)features.size(); i++)
    {
        for(int j = 0; j < (int)voc.size(); j++)
        {
            if(length(features[i].pt - voc[j].pt) < max_dist)
            {
                filtered.push_back(features[i]);
            }
        }
    }
    
    features = filtered;
}

int LoadFeatures(const char* filename, vector<vector<feature_t> >& features, vector<Ptr<IplImage> >& images)
{
    IplImage* train_image = loadImageRed(filename);
    
    size_t pyr_levels = features.size();
    images.resize(pyr_levels);
    IplImage* image_features = cvCloneImage(train_image);
    for(size_t i = 0; i < features.size(); i++)
    {
        images[i] = image_features;
        GetHoleFeatures(image_features, features[i]);
        IplImage* temp_image = cvCreateImage(cvSize(image_features->width/2, image_features->height/2), IPL_DEPTH_8U, 1);
        cvPyrDown(image_features, temp_image);
        image_features = temp_image;
    }
    cvReleaseImage(&image_features);
    
    int feature_count = 0;
    for(size_t i = 0; i < pyr_levels; i++)
    {
        feature_count += features[i].size();
    }
    
    cvReleaseImage(&train_image);
    
    return feature_count;
}

IplImage* loadImageRed(const char* filename)
{
    IplImage* temp = cvLoadImage(filename);
    IplImage* red = cvCreateImage(cvSize(temp->width, temp->height), IPL_DEPTH_8U, 1);
    cvSetImageCOI(temp, 3);
    cvCopy(temp, red);
    cvReleaseImage(&temp);
    
#if defined(_SCALE_IMAGE_2)
    IplImage* red2 = cvCreateImage(cvSize(red->width/2, red->height/2), IPL_DEPTH_8U, 1);
    cvResize(red, red2);
    cvReleaseImage(&red);
    red = red2;
#endif //_SCALE_IMAGE_2
    
    return red;
}
