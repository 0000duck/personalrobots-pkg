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

#include <robotmodels/kinematicODE.h>

void KinematicModelODE::build(URDF &model, const char *group)
{
    KinematicModel::build(model, group);
    m_space = dHashSpaceCreate(0);
    for (unsigned int i = 0 ; i < m_robots.size() ; ++i)
	buildODEGeoms(m_robots[i]);
}

void KinematicModelODE::setGeomPose(dGeomID geom, libTF::Pose3D &pose) const
{
    libTF::Pose3D::Position pos = pose.getPosition();
    dGeomSetPosition(geom, pos.x, pos.y, pos.z);
    libTF::Pose3D::Quaternion quat = pose.getQuaternion();
    dQuaternion q; q[0] = quat.w; q[1] = quat.x; q[2] = quat.y; q[3] = quat.z;
    dGeomSetQuaternion(geom, q);
}

void KinematicModelODE::updateCollisionPositions(void)
{
    for (unsigned int i = 0 ; i < m_kgeoms.size() ; ++i)
	setGeomPose(m_kgeoms[i].geom, m_kgeoms[i].link->globalTrans);
}

void KinematicModelODE::buildODEGeoms(Robot *robot)
{
    //   double params[robot->stateDimension];
    //    for (unsigned int i = 0 ; i < robot->stateDimension ; ++i)
    //	params[i] = (robot->stateBounds[i * 2] + robot->stateBounds[i * 2 + 1]) / 2.0;
    //    robot->computeTransforms(params);
    for (unsigned int i = 0 ; i < robot->links.size() ; ++i)
    {
	kGeom kg;
	kg.link = robot->links[i];
	kg.geom = buildODEGeom(robot->links[i]->geom);
	if (!kg.geom) continue;
	m_kgeoms.push_back(kg);
    }
}

dGeomID KinematicModelODE::buildODEGeom(Geometry *geom)
{
    dGeomID g = NULL;
    
    switch (geom->type)
    {
    case Geometry::SPHERE:
	g = dCreateSphere(m_space, geom->size[0]);
	break;
    case Geometry::BOX:
	g = dCreateBox(m_space, geom->size[0], geom->size[1], geom->size[2]);
	break;
    case Geometry::CYLINDER:
	g = dCreateCylinder(m_space, geom->size[0], geom->size[1]);
	break;
    default:
	break;
    }
    
    return g;
}

dSpaceID KinematicModelODE::getODESpace(void) const
{
    return m_space;
}
