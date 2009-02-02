#include <cstdio>
#include <stdexcept>
#include <vector>
//#include "kniBase.h"
#include "katana/katana.h"
#include "ros/common.h"
#include "ros/node.h"
#include "std_srvs/StringString.h"
#include "katana/StringArmCSpace.h"
#include "katana/ArmCSpaceSeqString.h"
#include "katana/ArmCSpaceString.h"
#include "katana/ArmCSpaceSeqString.h"
#include "std_srvs/UInt32String.h"
#include "std_srvs/Float32String.h"
#include "katana/KatanaIK.h"
#include "katana/KatanaPose.h"

using std::vector;
using std::cout;
using std::endl;

class KatanaServer : public ros::Node
{
  public:
    Katana *katana;
    KatanaServer() : ros::Node("katana_server")
    {
      advertiseService("katana_calibrate_service", &KatanaServer::calibrateSrv);
      advertiseService("katana_move_upright_service", &KatanaServer::move_to_upright);
      advertiseService("katana_back_to_upright_service", &KatanaServer::move_back_to_upright);
      advertiseService("katana_move_for_camera_service", &KatanaServer::move_for_camera);
      advertiseService("katana_get_joint_angles_service", &KatanaServer::get_current_joint_angles);
      advertiseService("katana_move_joints_single_service", &KatanaServer::moveJointsSingle);
      advertiseService("katana_move_joint_sequence_rad_service", &KatanaServer::moveJointsSeqRad);
      advertiseService("katana_move_joint_sequence_deg_service", &KatanaServer::moveJointsSeqDeg);
      advertiseService("katana_gripper_cmd_service", &KatanaServer::gripperCmd);
      advertiseService("katana_gripper_position_service", &KatanaServer::gripperPositionCmd);
      advertiseService("katana_ik_calculate_service", &KatanaServer::ik_calculate_srv);
      advertiseService("katana_get_pose_service", &KatanaServer::get_current_pose);
      advertiseService("katana_move_linear_service", &KatanaServer::move_robot_linear);
      katana = new Katana();
    }
    virtual ~KatanaServer() { delete katana; }
    
    bool get_current_pose(katana::KatanaPose::Request &req,
                    katana::KatanaPose::Response &res)
    {
      vector<double> katana_pose = katana->get_pose();
      res.pose.x = katana_pose[0];
      res.pose.y = katana_pose[1];
      res.pose.z = katana_pose[2];
      res.phi = katana_pose[3];
      res.theta = katana_pose[4];
      res.psi = katana_pose[5];
      return true;
    }

    bool move_robot_linear(katana::KatanaPose::Request &req,
                    katana::KatanaPose::Response &res)
    {
      vector<double> dst_pose;
      dst_pose.push_back(req.pose.x);
      dst_pose.push_back(req.pose.y);
      dst_pose.push_back(req.pose.z);
      dst_pose.push_back(req.phi);
      dst_pose.push_back(req.theta);
      dst_pose.push_back(req.psi);
      bool status = katana->linear_move(dst_pose, 10000);
      cout << "done. status = " << status;
      //catch (KNI::NoSolutionException)
      return status;
    }

/* This function uses the kni-3.9.2 inverse kinematics function (ik_calculate) to 
 * compute a joint angle solution for the given pose.
 * x,y,z = end-effector coordinates in katana frame in mm
 * theta = orientation of wrist (last link) measure from vertical in radians
 * psi = rotation angle of wrist (motor 4) in radians; note that, although psi and joint
 *        angle 4 correspond to exactly the same degree of freedom, these
 *        angles will not be equal for a give wrist position (one is measured
 *        positive CW and the other CCW).
 * max_theta_dev = amount that theta is allowed to deviate from the specified value
 *        in the positive and negative direction in order to find a solution if one is
 *        not found for the specified wrist orientation.
*/
    bool ik_calculate_srv(katana::KatanaIK::Request &req,
                    katana::KatanaIK::Response &res)
    {
      cout << "Using ik calculate for pose: " << req.pose.x << " " << req.pose.y << " "
            << req.pose.z << " " << req.theta << " " << req.psi << endl;
      vector<double> solution;
      bool success = katana->ik_joint_solution(req.pose.x, req.pose.y, req.pose.z,
          req.theta, req.psi, req.max_theta_dev, solution);
      if (success) {
        res.solution.set_angles_size(solution.size());
        for (size_t i=0; i<solution.size(); i++) {
          res.solution.angles[i] = solution[i];
        }
      } else res.solution.set_angles_size(0);
      return (success);
    }

    bool calibrateSrv(std_srvs::StringString::Request &req,
                   std_srvs::StringString::Response &res)
    {
      katana->allow_crash_limits(true);
      bool success = katana->calibrate();
      if (success) {
        res.str = "Done";
				cout << "Done!" << endl;
      } else {
				res.str = "Error";
				cout << "Error!" << endl;
			}
      return(success);
    }

    bool move_to_upright(std_srvs::StringString::Request &req,
                   std_srvs::StringString::Response &res)
    {
      katana->allow_crash_limits(true);
      bool success = katana->goto_upright();
      if (success) {
        res.str = "Done";
				cout << "Done!" << endl;
      } else {
				res.str = "Error";
				cout << "Error!" << endl;
			}
      return(success);
    }
    
    bool move_for_camera(std_srvs::StringString::Request &req,
                   std_srvs::StringString::Response &res)
    {
      katana->allow_crash_limits(true);
      bool success = katana->move_for_camera();
      if (success) {
        res.str = "Done";
				cout << "Done!" << endl;
      } else {
				res.str = "Error";
				cout << "Error!" << endl;
			}
      return(success);
    }
    
    bool move_back_to_upright(std_srvs::StringString::Request &req,
                   std_srvs::StringString::Response &res)
    {
      katana->allow_crash_limits(true);
      bool success = katana->move_back_to_upright();
      if (success) {
        res.str = "Done";
				cout << "Done!" << endl;
      } else {
				res.str = "Error";
				cout << "Error!" << endl;
			}
      return(success);
    }

    bool get_current_joint_angles(katana::StringArmCSpace::Request &req,
                   katana::StringArmCSpace::Response &res)
    {
      vector<double> jointPositions = katana->get_joint_positions();
      res.jointAngles.set_angles_size(jointPositions.size());
      for (unsigned int i=0; i<jointPositions.size(); i++) {
        res.jointAngles.angles[i] = jointPositions.at(i);
      }
      return (jointPositions.size() > 0);
    }
    
    bool moveJointsSingle(katana::ArmCSpaceString::Request &req,
                   katana::ArmCSpaceString::Response &res)
    {
      katana->allow_crash_limits(false);
      bool success = katana->goto_joint_position_deg(req.jointAngles.angles[0], req.jointAngles.angles[1],
        req.jointAngles.angles[2], req.jointAngles.angles[3], req.jointAngles.angles[4]);
      if (success) {
        res.str = "Done";
				cout << "Done!" << endl;
      } else {
				res.str = "Error";
				cout << "Error!" << endl;
			}
      return (success);
    }
    
    bool gripperCmd(std_srvs::UInt32String::Request &req,
                   std_srvs::UInt32String::Response &res)
    {
      bool success = katana->gripper_fullstop(req.value);
      if (success) {
        res.str = "Done";
				cout << "Done!" << endl;
      } else {
				res.str = "Error";
				cout << "Error!" << endl;
			}
      return (success);
    }

    bool gripperPositionCmd(std_srvs::Float32String::Request &req,
                       std_srvs::Float32String::Response &res)
    {
      bool success = katana->move_gripper(req.value);
      if (success) {
        res.str = "Done";
        cout << "Done!" << endl;
      } else {
        res.str = "Error";
        cout << "Error!" << endl;
      }
      return (success);
    }

    
    bool moveJointsSeqDeg(katana::ArmCSpaceSeqString::Request &req,
                   katana::ArmCSpaceSeqString::Response &res)
    {
      katana->allow_crash_limits(false);
      bool success = false;
      for (size_t i=0; i<req.jointAngles.get_configs_size(); i++) {
        success = katana->goto_joint_position_deg(req.jointAngles.configs[i].angles[0], 
          req.jointAngles.configs[i].angles[1], req.jointAngles.configs[i].angles[2],
          req.jointAngles.configs[i].angles[3], req.jointAngles.configs[i].angles[4]);
        if (!success) break;
      }
      if (success) {
        res.str = "Done";
				cout << "Done!" << endl;
      } else {
				res.str = "Error";
				cout << "Error!" << endl;
			}
      return (success);
    }
    
    bool moveJointsSeqRad(katana::ArmCSpaceSeqString::Request &req,
                   katana::ArmCSpaceSeqString::Response &res)
    {
      katana->allow_crash_limits(false);
      bool success = false;
      for (size_t i=0; i<req.jointAngles.get_configs_size(); i++) {
        cout << "Moving arm to: " << req.jointAngles.configs[i].angles[0] << " "
          << req.jointAngles.configs[i].angles[1] << " " 
          << req.jointAngles.configs[i].angles[2] << " "
          << req.jointAngles.configs[i].angles[3]<< " " 
          << req.jointAngles.configs[i].angles[4] << endl;
        success = katana->goto_joint_position_rad(req.jointAngles.configs[i].angles[0], 
          req.jointAngles.configs[i].angles[1], req.jointAngles.configs[i].angles[2],
          req.jointAngles.configs[i].angles[3], req.jointAngles.configs[i].angles[4]);
        if (!success) break;
      }
      if (success) {
        res.str = "Done";
				cout << "Done!" << endl;
      } else {
				res.str = "Error";
				cout << "Error!" << endl;
			}
      return (success);
    }

};

int main(int argc, char **argv)
{
  ros::init(argc, argv);
  KatanaServer katanaSrv;
  katanaSrv.spin();
  ros::fini();
  return 0;
}
