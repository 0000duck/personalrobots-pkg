/*
 * articulation_models.h
 *
 *  Created on: Aug 3, 2009
 *      Author: sturm
 */

#ifndef ARTICULATION_MODELS_H_
#define ARTICULATION_MODELS_H_

#include "track_utils.h"

namespace planar_objects {

class ManifoldModel {
public:
  double err_trans;
  double err_rot;
  btBoxTrack* track;
  double w;
  double h;
  ManifoldModel(btBoxTrack* track);
  virtual void findParameters(ros::Publisher* pub = NULL);
  virtual double getRotationalError();
  virtual double getTranslationalError();
  virtual double getLoglikelihood(double sigma_trans,double sigma_rot);
  virtual void computeError();
  virtual void getError(double &err_trans,double &err_rot);
  virtual size_t getDOFs() = 0;
  virtual size_t getComplexityClass() = 0;
  virtual std::vector< std::pair<double,double> > getRange();
  virtual bool isArticulated(double thres_trans,double thres_rot);
  virtual std::vector<double> getConfiguration( btTransform transform) = 0;
  virtual btTransform getPrediction( std::vector<double> q ) = 0;
  virtual btBoxObservation getPredictedObservation( std::vector<double> q );
  virtual bool isValid();
};

class RigidModel:public ManifoldModel {
public:
  btTransform center;
  RigidModel(btBoxTrack* track);
  void findParameters(ros::Publisher* pub);
  size_t getDOFs();
  size_t getComplexityClass();
  std::vector<double> getConfiguration( btTransform transform);
  btTransform getPrediction( std::vector<double> configuration );
  bool isValid();
  bool isArticulated(double sigma_trans,double sigma_rot);
};

class PrismaticModel:public ManifoldModel {
public:
  btTransform center;
  btVector3 direction;
  double qMin, qMax;
  PrismaticModel(btBoxTrack* track);
  void findParameters(ros::Publisher* pub);
  size_t getDOFs();
  size_t getComplexityClass();
  std::vector<double> getConfiguration( btTransform transform);
  btTransform getPrediction( std::vector<double> configuration );
  bool isValid();
  bool isArticulated(double sigma_trans,double sigma_rot);
};

class RotationalModel:public ManifoldModel {
public:
  btTransform center;
  btTransform radius;
  btTransform offset;
  double qMin, qMax;
  RotationalModel(btBoxTrack* track);
  void findParameters(ros::Publisher* pub);
  size_t getDOFs();
  size_t getComplexityClass();
  std::vector<double> getConfiguration( btTransform transform);
  btTransform getPrediction( std::vector<double> configuration );
  bool isValid();
  bool isArticulated(double sigma_trans,double sigma_rot);
};

}

#endif /* ARTICULATION_MODELS_H_ */
