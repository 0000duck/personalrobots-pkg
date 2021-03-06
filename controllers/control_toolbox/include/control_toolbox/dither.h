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
#pragma once

#include <cstdlib>
#include <ctime>
#include <math.h>
#include <ros/ros.h>

namespace control_toolbox {
/***************************************************/
/*! \class Dither

  \brief Gives white noise at specified amplitude.

  This class gives white noise at the given amplitude when 
  update() is called. Gets parameter "~amplitude".

*/
/***************************************************/

class Dither
{
public:

  /*!
   * \brief Constructor
   */
  Dither(double seed);

  /*!
   * \brief Destructor.
   */
  ~Dither();

  /*!
   * \brief Get next Gaussian white noise point
   */
  double update();

  /*!
   * \brief Intializes everything and calculates the constants for the sweep.
   * \param n NodeHandle to look for parameters with
   */
  bool init(const ros::NodeHandle &n);

private:
  double amplitude_;                        /**< Amplitude of the sweep. */
  double s_;
  double x_;
  long idum; // Random seed

  /*
   *\brief Returns uniform deviate between 0.0 and 1.0.
   */
  double uni();
};
}
