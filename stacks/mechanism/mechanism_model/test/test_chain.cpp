/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <gtest/gtest.h>
#include <vector>
#include <boost/scoped_ptr.hpp>
#include "mechanism_model/robot.h"
#include "mechanism_model/chain.h"
#include <kdl/chainfksolverpos_recursive.hpp>

using namespace mechanism;
using namespace std;


// Just three links
class ShortChainTest : public testing::Test
{
protected:
  ShortChainTest() : hw(0) {}
  virtual ~ShortChainTest() {}

  virtual void SetUp() {}

  virtual void TearDown() {}

  HardwareInterface hw;
};

TEST_F(ShortChainTest, FKShouldMatchOnShortChainWhenStraight)
{
  TiXmlDocument urdf_xml;
  urdf_xml.LoadFile("pr2.urdf");
  TiXmlElement *root = urdf_xml.FirstChildElement("robot");
  ASSERT_TRUE(root != NULL);
  HardwareInterface hw(0);
  Robot model(&hw);
  ASSERT_TRUE(model.initXml(root));
  RobotState state(&model, &hw);

  // extract chain
  Chain chain;
  EXPECT_TRUE(chain.init(&model, "fl_caster_l_wheel_link", "r_gripper_palm_link"));
  KDL::Chain kdl;
  chain.toKDL(kdl);
  unsigned int nr_segments = 13;
  unsigned int nr_joints = 10;
  ASSERT_EQ(nr_segments, kdl.getNrOfSegments());
  ASSERT_EQ(nr_joints, kdl.getNrOfJoints());

  KDL::JntArray jnts(model.joints_.size());
  chain.getPositions(state.joint_states_, jnts);
}

int main(int argc, char **argv){
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
