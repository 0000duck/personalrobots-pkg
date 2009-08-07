#include "trex_ros/ros_state_adapter.h"
#include "Domains.hh"
#include <nav_msgs/Odometry.h>

namespace TREX {
  namespace deprecated {
    class BaseStateAdapter: public ROSStateAdapter<nav_msgs::Odometry> {
    public:
      BaseStateAdapter(const LabelStr& agentName, const TiXmlElement& configData)
	: ROSStateAdapter<nav_msgs::Odometry> ( agentName, configData) {
      }

      virtual ~BaseStateAdapter(){}


    private:
      void fillObservationParameters(ObservationByValue* obs){
	// Get the 2D Pose
	//double x, y, th;
	//get2DPose(x, y, th);
	obs->push_back("x", new IntervalDomain(1.0));
	obs->push_back("y", new IntervalDomain(1.0));
	obs->push_back("th",new IntervalDomain(0.0));
      }
    };

    // Allocate a Factory
    TeleoReactor::ConcreteFactory<BaseStateAdapter> l_BaseStateAdapter_Factory("BaseStateAdapter");
  }
}
