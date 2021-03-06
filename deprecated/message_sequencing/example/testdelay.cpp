#include "ros/node.h"
#include "geometry_msgs/PointStamped.h"
#include "ros/time.h"


int g_argc;
char** g_argv;

class TestDelay : public ros::Node
{

public:

  ros::Duration delay;
  int num;
  geometry_msgs::PointStamped point;

  TestDelay() : ros::Node("test_delay", ros::Node::ANONYMOUS_NAME)
  {
    delay.fromSec(atof(g_argv[1]));
    num = atoi(g_argv[2]);

    subscribe("orig",point,&TestDelay::cb,1);
    advertise<geometry_msgs::PointStamped>("delay",10);
  }

  void cb()
  {
    delay.sleep();
    point.point.y = num;
    publish("delay", point);
  }
};

int main(int argc, char** argv)
{
  ros::init(argc, argv);
  g_argc = argc;
  g_argv = argv;
  TestDelay t;
  t.spin();
  
}
