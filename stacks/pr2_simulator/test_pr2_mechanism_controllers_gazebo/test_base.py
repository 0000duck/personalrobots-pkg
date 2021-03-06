import unittest
import math

import rospy
from geometry_msgs.msg import Twist,Vector3
from nav_msgs.msg import Odometry

class E:
    def __init__(self,x,y,z):
        self.x = x
        self.y = y
        self.z = z
    def shortest_euler_distance(self, e_from, e_to):
       # takes two sets of euler angles, finds minimum transform between the two, FIXME: return some hacked norm for now
       # start from the euler-from, and apply reverse euler-to transform, see how far we are from 0,0,0
       r0 = e_from.x
       p0 = e_from.y
       y0 = e_from.z

       r1 =  math.cos(e_to.z)*r0       + math.sin(e_to.z)*p0
       p1 = -math.sin(e_to.z)*r0       + math.cos(e_to.z)*p0
       y1 =  y0

       r2 =  math.cos(e_to.y)*r1     - math.sin(e_to.y)*y1
       p2 =  p1
       y2 =  math.sin(e_to.y)*r1     + math.cos(e_to.y)*y1

       self.x =  r2
       self.y =  math.cos(e_to.x)*p1     + math.sin(e_to.x)*y1
       self.z = -math.sin(e_to.x)*p1     + math.cos(e_to.x)*y1

class Q:
    def __init__(self,x,y,z,u):
        self.x = x
        self.y = y
        self.z = z
        self.u = u
    def normalize(self):
        s = math.sqrt(self.u * self.u + self.x * self.x + self.y * self.y + self.z * self.z)
        self.u /= s
        self.x /= s
        self.y /= s
        self.z /= s
    def getEuler(self):
        self.normalize()
        squ = self.u
        sqx = self.x
        sqy = self.y
        sqz = self.z
        # init euler angles
        vec = E(0,0,0)
        # Roll
        vec.x = math.atan2(2 * (self.y*self.z + self.u*self.x), squ - sqx - sqy + sqz);
        # Pitch
        vec.y = math.asin(-2 * (self.x*self.z - self.u*self.y));
        # Yaw
        vec.z = math.atan2(2 * (self.x*self.y + self.u*self.z), squ + sqx - sqy - sqz);
        return vec

class BaseTest(unittest.TestCase):
    def __init__(self, *args):
        super(BaseTest, self).__init__(*args)
        self.success = False

        self.odom_xi = 0;
        self.odom_yi = 0;
        self.odom_ti = 0;
        self.odom_ei = E(0,0,0)
        self.odom_initialized = False;

        self.odom_x = 0;
        self.odom_y = 0;
        self.odom_t = 0;
        self.odom_e = E(0,0,0)
        
        self.p3d_xi = 0;
        self.p3d_yi = 0;
        self.p3d_ti = 0;
        self.p3d_ei = E(0,0,0)
        self.p3d_initialized = False;

        self.p3d_x = 0;
        self.p3d_y = 0;
        self.p3d_t = 0;
        self.p3d_e = E(0,0,0)

        self.pub = None
        
    def normalize_angle_positive(self, angle):
        return math.fmod(math.fmod(angle, 2*math.pi) + 2*math.pi, 2*math.pi)

    def normalize_angle(self, angle):
        anorm = self.normalize_angle_positive(angle)
        if anorm > math.pi:
          anorm -= 2*math.pi
        return anorm

    def shortest_angular_distance(self, angle_from, angle_to):
        angle_diff = self.normalize_angle_positive(angle_to) - self.normalize_angle_positive(angle_from)
        if angle_diff > math.pi:
          angle_diff = -(2*math.pi - angle_diff)
        return self.normalize_angle(angle_diff)

    def printBaseOdom(self, odom):
        orientation = odom.pose.orientation
        q = Q(orientation.x, orientation.y, orientation.z, orientation.w)
        q.normalize()
        print "odom received"
        print "odom pos " + "x: " + str(odom.pose.pose.position.x)
        print "odom pos " + "y: " + str(odom.pose.pose.position.y)
        print "odom pos " + "t: " + str(q.getEuler().z)
        print "odom vel " + "x: " + str(odom.twist.twist.linear.x)
        print "odom vel " + "y: " + str(odom.twist.twist.linear.y)
        print "odom vel " + "t: " + str(odom.twist.twist.angular.z)

    def printBaseP3D(self, p3d):
        print "base pose ground truth received"
        print "P3D pose translan: " + "x: " + str(p3d.pose.pose.position.x)
        print "                   " + "y: " + str(p3d.pose.pose.position.y)
        print "                   " + "z: " + str(p3d.pose.pose.position.z)
        print "P3D pose rotation: " + "x: " + str(p3d.pose.pose.orientation.x)
        print "                   " + "y: " + str(p3d.pose.pose.orientation.y)
        print "                   " + "z: " + str(p3d.pose.pose.orientation.z)
        print "                   " + "w: " + str(p3d.pose.pose.orientation.w)
        print "P3D rate translan: " + "x: " + str(p3d.twist.twist.linear.x)
        print "                   " + "y: " + str(p3d.twist.twist.linear.y)
        print "                   " + "z: " + str(p3d.twist.twist.linear.z)
        print "P3D rate rotation: " + "x: " + str(p3d.twist.twist.angular.vx)
        print "                   " + "y: " + str(p3d.twist.twist.angular.vy)
        print "                   " + "z: " + str(p3d.twist.twist.angular.vz)

    def odomInput(self, odom):
        #self.printBaseOdom(odom)
        orientation = odom.pose.pose.orientation
        q = Q(orientation.x, orientation.y, orientation.z, orientation.w)
        q.normalize()
        if self.odom_initialized == False:
            self.odom_initialized = True
            self.odom_xi = odom.pose.pose.position.x
            self.odom_yi = odom.pose.pose.position.y
            self.odom_ti = q.getEuler().z
            self.odom_ei = q.getEuler()
        self.odom_x = odom.pose.pose.position.x   - self.odom_xi
        self.odom_y = odom.pose.pose.position.y   - self.odom_yi
        self.odom_t = q.getEuler().z  - self.odom_ti
        self.odom_e.shortest_euler_distance(self.odom_ei, q.getEuler())
        
    def p3dInput(self, p3d):
        q = Q(p3d.pose.pose.orientation.x , p3d.pose.pose.orientation.y , p3d.pose.pose.orientation.z , p3d.pose.pose.orientation.w)
        q.normalize()
        v = q.getEuler()

        if self.p3d_initialized == False:
            self.p3d_initialized = True
            self.p3d_xi = p3d.pose.pose.position.x
            self.p3d_yi = p3d.pose.pose.position.y
            self.p3d_ti = v.z
            self.p3d_ei = E(v.x,v.y,v.z)
            
        self.p3d_x = p3d.pose.pose.position.x - self.p3d_xi
        self.p3d_y = p3d.pose.pose.position.y - self.p3d_yi
        self.p3d_t = v.z                      - self.p3d_ti
        self.p3d_e.shortest_euler_distance(self.p3d_ei,E(v.x,v.y,v.z))    

    def debug_e(self):
        # display what odom thinks
        print " odom    " + " x: " + str(self.odom_e.x) + " y: " + str(self.odom_e.y) + " t: " + str(self.odom_e.t)
        # display what ground truth is
        print " p3d     " + " x: " + str(self.p3d_e.x)  + " y: " + str(self.p3d_e.y)  + " t: " + str(self.p3d_e.t)

    def debug_pos(self):
        # display what odom thinks
        print " odom    " + " x: " + str(self.odom_x) + " y: " + str(self.odom_y) + " t: " + str(self.odom_t)
        # display what ground truth is
        print " p3d     " + " x: " + str(self.p3d_x)  + " y: " + str(self.p3d_y)  + " t: " + str(self.p3d_t)

    def init_ros(self, name):
        print "LNK\n"
        self.pub = rospy.Publisher("/cmd_vel", Twist)
        rospy.Subscriber("/base_pose_ground_truth", Odometry, self.p3dInput)
        rospy.Subscriber("pr2_odometry/odom",                   Odometry, self.odomInput)
        rospy.init_node(name, anonymous=True)
