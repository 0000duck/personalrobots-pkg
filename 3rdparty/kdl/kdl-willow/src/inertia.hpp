// Copyright  (C)  2007  Ruben Smits <ruben dot smits at mech dot kuleuven dot be>

// Version: 1.0
// Author: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// Maintainer: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// URL: http://www.orocos.org/kdl

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#ifndef KDLINERTIA_HPP
#define KDLINERTIA_HPP

#include "frames.hpp"


namespace KDL {

  class InertiaMatrix{
  public:
    InertiaMatrix(double Ixx=0,double Iyy=0,double Izz=0,double Ixy=0,double Ixz=0,double Iyz=0);
    InertiaMatrix(const InertiaMatrix& inert);
    InertiaMatrix operator=(const InertiaMatrix& inert);
    ~InertiaMatrix() {};
    
    static inline InertiaMatrix Zero(){  return InertiaMatrix(); };
    Vector operator*(const KDL::Vector& v) const;

    double data[9];
  };



  /**
     This class offers the inertia-structure of a body
  */
  class Inertia{
  public:
    Inertia(double m=0, const Vector& cog=Vector::Zero(), double Ixx=0,double Iyy=0,double Izz=0,double Ixy=0,double Ixz=0,double Iyz=0);
    ~Inertia() {};
    
    static inline Inertia Zero(){ return Inertia(); };
    Vector getCog() const {return cog_;};

  public:
    Vector cog_;
    double m_;
    InertiaMatrix I_;
  };


  

}

#endif
