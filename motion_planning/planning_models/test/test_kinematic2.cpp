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

/** \author Ioan Sucan */

#include <planning_models/kinematic_model.h>
#include <gtest/gtest.h>
#include <sstream>
#include <ctype.h>

static bool sameStringIgnoringWS(const std::string &s1, const std::string &s2)
{
    unsigned int i1 = 0;
    unsigned int i2 = 0;
    while (i1 < s1.size() && isspace(s1[i1])) i1++;
    while (i2 < s2.size() && isspace(s2[i2])) i2++;
    while (i1 < s1.size() && i2 < s2.size())
    {
	if (i1 < s1.size() && i2 < s2.size())
	{
	    if (s1[i1] != s2[i2])
		return false;
	    i1++;
	    i2++;
	}
	while (i1 < s1.size() && isspace(s1[i1])) i1++;
	while (i2 < s2.size() && isspace(s2[i2])) i2++;
    }
    return i1 == s1.size() && i2 == s2.size();
}

TEST(Loading, SimpleRobot)
{
    static const std::string MODEL0 = 
	"<?xml version=\"1.0\" ?>" 
	"<robot name=\"myrobot\">" 
	"  <joint name=\"base_joint\" type=\"planar\">"
	"  <origin rpy=\"0 0 0\" xyz=\"0 0 0.0\"/>"
	"  <parent link=\"world\"/>"
	"  <child link=\"base_link\"/>"
	"</joint>"
	"<link name=\"base_link\">"
	"  <collision name=\"base_collision\">"
        "  <origin rpy=\"0 0 0\" xyz=\"0 0 0.165\"/>"
        "  <geometry name=\"base_collision_geom\">"
	"  <box size=\"0.65 0.65 0.23\"/>"
        "</geometry>"
	"</collision>"
	"</link>"
	"</robot>";

    urdf::Model urdfModel;
    urdfModel.initString(MODEL0);
    
    std::map < std::string, std::vector<std::string> > groups;
    planning_models::KinematicModel *model = new planning_models::KinematicModel(urdfModel, groups);
    
    EXPECT_EQ(std::string("myrobot"), model->getName());
    EXPECT_EQ((unsigned int)3, model->getDimension());
    
    std::vector<const planning_models::KinematicModel::Link*> links;
    model->getLinks(links);
    EXPECT_EQ((unsigned int)1, links.size());
    
    std::vector<const planning_models::KinematicModel::Joint*> joints;
    model->getJoints(joints);
    EXPECT_EQ((unsigned int)1, joints.size());
    
    std::vector<std::string> pgroups;
    model->getGroupNames(pgroups);    
    EXPECT_EQ((unsigned int)0, pgroups.size());
    
    delete model;
}

TEST(LoadingAndFK, SimpleRobot)
{   
    static const std::string MODEL1 = 
	"<?xml version=\"1.0\" ?>" 
	"<robot name=\"one_robot\">"
	"  <joint name=\"base_joint\" type=\"planar\">"
	"  <parent link=\"world\"/>"
	"  <child link=\"base_link\"/>"
	"  <origin rpy=\"1 0 0 \" xyz=\"0 0 0 \"/>"
	"  </joint>"
	"<link name=\"base_link\">"
	"  <inertial>"
	"    <mass value=\"2.81\"/>"
	"    <origin rpy=\"0 0 0\" xyz=\"0.0 0.099 .0\"/>"
	"    <inertia ixx=\"0.1\" ixy=\"-0.2\" ixz=\"0.5\" iyy=\"-.09\" iyz=\"1\" izz=\"0.101\"/>"
	"  </inertial>"
	"  <collision name=\"my_collision\">"
	"    <origin rpy=\"0 0 -1\" xyz=\"-0.1 0 0\"/>"
	"    <geometry>"
	"      <box size=\"1 2 1\" />"
	"    </geometry>"
	"  </collision>"
	"  <visual>"
	"    <origin rpy=\"0 0 0\" xyz=\"0.0 0 0\"/>"
	"    <geometry>"
	"      <box size=\"1 2 1\" />"
	"    </geometry>"
	"  </visual>"
	"</link>"
	"</robot>";
    
    static const std::string MODEL1_INFO = 
	"Complete model state dimension = 3\n"
	"State bounds: [0.00000, 0.00000] [0.00000, 0.00000] [-3.14159, 3.14159] \n"
	"Floating joints : \n"
	"Planar joints : base_joint \n"
	"Available groups: base \n"
	"Group base has 1 roots: base_joint \n"
	"The state components for this group are: 0 1 2 \n";
    
    urdf::Model urdfModel;
    urdfModel.initString(MODEL1);
    
    std::map < std::string, std::vector<std::string> > groups;
    groups["base"].push_back("base_joint");

    planning_models::KinematicModel *model = new planning_models::KinematicModel(urdfModel, groups);
        
    EXPECT_EQ((unsigned int)3, model->getDimension());
    
    model->defaultState();
    
    std::vector<const planning_models::KinematicModel::Joint*> joints;
    model->getJoints(joints);
    EXPECT_EQ((unsigned int)1, joints.size());
    EXPECT_EQ((unsigned int)3, joints[0]->usedParams);

    EXPECT_EQ((unsigned int)1, model->getPlanarJoints().size());

    std::stringstream ssi;
    model->printModelInfo(ssi);
    EXPECT_TRUE(sameStringIgnoringWS(MODEL1_INFO, ssi.str()));
        
    
    double param[3] = { 10, 8, 0 };
    model->computeTransforms(param);

    EXPECT_NEAR(9.9, model->getLink("base_link")->globalTrans.getOrigin().x(), 1e-5);
    EXPECT_NEAR(8.0, model->getLink("base_link")->globalTrans.getOrigin().y(), 1e-5);
    EXPECT_NEAR(0.0, model->getLink("base_link")->globalTrans.getOrigin().z(), 1e-5);
    EXPECT_NEAR(0.0, model->getLink("base_link")->globalTrans.getRotation().x(), 1e-5);
    EXPECT_NEAR(0.0, model->getLink("base_link")->globalTrans.getRotation().y(), 1e-5);
    EXPECT_NEAR(-0.479426, model->getLink("base_link")->globalTrans.getRotation().z(), 1e-5);
    EXPECT_NEAR(0.877583, model->getLink("base_link")->globalTrans.getRotation().w(), 1e-5);
    
    delete model;    
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
