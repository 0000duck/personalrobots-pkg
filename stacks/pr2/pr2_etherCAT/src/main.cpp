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

#include <stdio.h>
#include <getopt.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <diagnostic_updater/DiagnosticStatusWrapper.h>
#include <pr2_mechanism_control/mechanism_control.h>
#include <ethercat_hardware/ethercat_hardware.h>

#include <ros/ros.h>
#include <std_srvs/Empty.h>

#include <realtime_tools/realtime_publisher.h>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
using namespace boost::accumulators;

static struct
{
  char *program_;
  char *interface_;
  char *xml_;
  bool allow_unprogrammed_;
} g_options;

std::string g_robot_desc;

void Usage(string msg = "")
{
  fprintf(stderr, "Usage: %s [options]\n", g_options.program_);
  fprintf(stderr, "  Available options\n");
  fprintf(stderr, "    -i, --interface <interface> Connect to EtherCAT devices on this interface\n");
  fprintf(stderr, "    -x, --xml <file|param>      Load the robot description from this file or parameter name\n");
  fprintf(stderr, "    -u, --allow_unprogrammed    Allow control loop to run with unprogrammed devices\n");
  fprintf(stderr, "    -h, --help                  Print this message and exit\n");
  if (msg != "")
  {
    fprintf(stderr, "Error: %s\n", msg.c_str());
    exit(-1);
  }
  else
  {
    exit(0);
  }
}

static int g_quit = 0;
static bool g_reset_motors = true;
static bool g_halt_motors = false;
static const int NSEC_PER_SEC = 1e+9;

static struct
{
  accumulator_set<double, stats<tag::max, tag::mean> > ec_acc;
  accumulator_set<double, stats<tag::max, tag::mean> > mc_acc;
} g_stats;

static void publishDiagnostics(realtime_tools::RealtimePublisher<diagnostic_msgs::DiagnosticArray> &publisher)
{
  if (publisher.trylock())
  {
    accumulator_set<double, stats<tag::max, tag::mean> > zero;
    vector<diagnostic_msgs::DiagnosticStatus> statuses;
    diagnostic_updater::DiagnosticStatusWrapper status;

    static double max_ec = 0, max_mc = 0;
    double avg_ec, avg_mc;

    avg_ec = extract_result<tag::mean>(g_stats.ec_acc);
    avg_mc = extract_result<tag::mean>(g_stats.mc_acc);
    max_ec = std::max(max_ec, extract_result<tag::max>(g_stats.ec_acc));
    max_mc = std::max(max_mc, extract_result<tag::max>(g_stats.mc_acc));
    g_stats.ec_acc = zero;
    g_stats.mc_acc = zero;

    static bool first = true;
    if (first)
    {
      first = false;
      status.add("Robot Description", g_robot_desc);
    }

    status.addf("Max EtherCAT roundtrip (us)", "%.2f", max_ec*1e+6);
    status.addf("Avg EtherCAT roundtrip (us)", "%.2f", avg_ec*1e+6);
    status.addf("Max Mechanism Control roundtrip (us)", "%.2f", max_mc*1e+6);
    status.addf("Avg Mechanism Control roundtrip (us)", "%.2f", avg_mc*1e+6);

    status.name = "Realtime Control Loop";
    status.level = 0;
    status.message = "OK";

    statuses.push_back(status);
    publisher.msg_.set_status_vec(statuses);
    publisher.unlockAndPublish();
  }
}

static inline double now()
{
  struct timespec n;
  clock_gettime(CLOCK_MONOTONIC, &n);
  return double(n.tv_nsec) / NSEC_PER_SEC + n.tv_sec;
}


void *diagnosticLoop(void *args)
{
  EthercatHardware *ec((EthercatHardware *) args);
  struct timespec tick;
  clock_gettime(CLOCK_MONOTONIC, &tick);
  while (!g_quit) {
    ec->collectDiagnostics();
    tick.tv_sec += 1;
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tick, NULL);
  }
  return NULL;
}

void *controlLoop(void *)
{
  ros::NodeHandle node;
  realtime_tools::RealtimePublisher<diagnostic_msgs::DiagnosticArray> publisher(node, "/diagnostics", 2);

  // Initialize the hardware interface
  EthercatHardware ec;
  ec.init(g_options.interface_, g_options.allow_unprogrammed_);

  // Create mechanism control
  pr2_mechanism::MechanismControl mc(ec.hw_);

  // Load robot description
  TiXmlDocument xml;
  struct stat st;
  if (0 == stat(g_options.xml_, &st))
  {
    xml.LoadFile(g_options.xml_);
  }
  else
  {
    ROS_INFO("Xml file not found, reading from parameter server\n");
    if (node.getParam(g_options.xml_, g_robot_desc))
      xml.Parse(g_robot_desc.c_str());
    else
    {
      ROS_FATAL("Could not load the xml from parameter server: %s\n", g_options.xml_);
      return (void *)-1;
    }
  }
  TiXmlElement *root_element = xml.RootElement();
  TiXmlElement *root = xml.FirstChildElement("robot");
  if (!root || !root_element)
  {
      ROS_FATAL("Could not parse the xml from %s\n", g_options.xml_);
      return (void *)-1;
  }

  // Initialize mechanism control from robot description
  mc.initXml(root);

  // Publish one-time before entering real-time to pre-allocate message vectors
  publishDiagnostics(publisher);

  //Start Non-realtime diagonostic thread
  int rv;
  static pthread_t diagnosticThread;
  if ((rv = pthread_create(&diagnosticThread, NULL, diagnosticLoop, &ec)) != 0)
  {
    ROS_FATAL("Unable to create control thread: rv = %d\n", rv);
    ROS_BREAK();
  }
  
  // Set to realtime scheduler for this thread
  struct sched_param thread_param;
  int policy = SCHED_FIFO;
  thread_param.sched_priority = sched_get_priority_max(policy);
  pthread_setschedparam(pthread_self(), policy, &thread_param);

  struct timespec tick;
  clock_gettime(CLOCK_MONOTONIC, &tick);
  int period = 1e+6; // 1 ms in nanoseconds

  double last_published = now();
  while (!g_quit)
  {
    double start = now();
    if (g_reset_motors)
    {
      ec.update(true, g_halt_motors);
      g_reset_motors = false;
    }
    else
    {
      ec.update(false, g_halt_motors);
    }
    g_halt_motors = false;
    double after_ec = now();
    mc.update();
    double end = now();

    g_stats.ec_acc(after_ec - start);
    g_stats.mc_acc(end - after_ec);

    if ((end - last_published) > 1.0)
    {
      publishDiagnostics(publisher);
      last_published = end;
    }

    tick.tv_nsec += period;
    while (tick.tv_nsec >= NSEC_PER_SEC)
    {
      tick.tv_nsec -= NSEC_PER_SEC;
      tick.tv_sec++;
    }
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tick, NULL);
  }

  /* Shutdown all of the motors on exit */
  for (unsigned int i = 0; i < ec.hw_->actuators_.size(); ++i)
  {
    ec.hw_->actuators_[i]->command_.enable_ = false;
    ec.hw_->actuators_[i]->command_.effort_ = 0;
  }
  ec.update(false, true);

  //pthread_join(diagnosticThread, 0);  

  publisher.stop();

  ros::shutdown();

  return 0;
}

void quitRequested(int sig)
{
  g_quit = 1;
}

bool shutdownService(std_srvs::Empty::Request &req, std_srvs::Empty::Response &resp)
{
  quitRequested(0);
  return true;
}

bool resetMotorsService(std_srvs::Empty::Request &req, std_srvs::Empty::Response &resp)
{
  g_reset_motors = true;
  return true;
}

bool haltMotorsService(std_srvs::Empty::Request &req, std_srvs::Empty::Response &resp)
{
  g_halt_motors = true;
  return true;
}

static int
lock_fd(int fd)
{
  struct flock lock;
  int rv;

  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  rv = fcntl(fd, F_SETLK, &lock);
  return rv;
}

#define PIDFILE "/var/run/pr2_etherCAT.pid"
static int setupPidFile(void)
{
  int rv = -1;
  pid_t pid;
  int fd;
  FILE *fp = NULL;

  fd = open(PIDFILE, O_RDWR | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
  if (fd == -1)
  {
    if (errno != EEXIST)
    {
      ROS_FATAL("Unable to create pid file '%s': %s", PIDFILE, strerror(errno));
      goto end;
    }

    if ((fd = open(PIDFILE, O_RDWR)) < 0)
    {
      ROS_FATAL("Unable to open pid file '%s': %s", PIDFILE, strerror(errno));
      goto end;
    }

    if ((fp = fdopen(fd, "rw")) == NULL)
    {
      ROS_FATAL("Can't read from '%s': %s", PIDFILE, strerror(errno));
      goto end;
    }
    pid = -1;
    if ((fscanf(fp, "%d", &pid) != 1) || (pid == getpid()) || (lock_fd(fileno(fp)) == 0))
    {
      int rc;

      if ((rc = unlink(PIDFILE)) == -1)
      {
        ROS_FATAL("Can't remove stale pid file '%s': %s", PIDFILE, strerror(errno));
        goto end;
      }
    } else {
      ROS_FATAL("Another instance of pr2_etherCAT is already running with pid: %d\n", pid);
      goto end;
    }
  }

  unlink(PIDFILE);
  fd = open(PIDFILE, O_RDWR | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);

  if (fd == -1)
  {
    ROS_FATAL("Unable to open pid file '%s': %s", PIDFILE, strerror(errno));
    goto end;
  }

  if (lock_fd(fd) == -1)
  {
    ROS_FATAL("Unable to lock pid file '%s': %s", PIDFILE, strerror(errno));
    goto end;
  }

  if ((fp = fdopen(fd, "w")) == NULL)
  {
    ROS_FATAL("fdopen failed: %s", strerror(errno));
    goto end;
  }

  fprintf(fp, "%d\n", getpid());

  /* We do NOT close fd, since we want to keep the lock. */
  fflush(fp);
  fcntl(fd, F_SETFD, (long) 1);
  rv = 0;
end:
  return rv;
}

static void cleanupPidFile(void)
{
  unlink(PIDFILE);
}

#define CLOCK_PRIO 0
#define CONTROL_PRIO 0

static pthread_t controlThread;
static pthread_attr_t controlThreadAttr;
int main(int argc, char *argv[])
{
  // Must run as root
  if (geteuid() != 0)
  {
    fprintf(stderr, "You must run as root!\n");
    exit(-1);
  }

  // Keep the kernel from swapping us out
  mlockall(MCL_CURRENT | MCL_FUTURE);

  // Setup single instance
  if (setupPidFile() < 0) return -1;

  // Initialize ROS and parse command-line arguments
  ros::init(argc, argv, "pr2_etherCAT");

  // Parse options
  g_options.program_ = argv[0];
  while (1)
  {
    static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"allow_unprogrammed", no_argument, 0, 'u'},
      {"interface", required_argument, 0, 'i'},
      {"xml", required_argument, 0, 'x'},
    };
    int option_index = 0;
    int c = getopt_long(argc, argv, "hi:ux:", long_options, &option_index);
    if (c == -1) break;
    switch (c)
    {
      case 'h':
        Usage();
        break;
      case 'u':
        g_options.allow_unprogrammed_ = 1;
        break;
      case 'i':
        g_options.interface_ = optarg;
        break;
      case 'x':
        g_options.xml_ = optarg;
        break;
    }
  }
  if (optind < argc)
  {
    Usage("Extra arguments");
  }

  if (!g_options.interface_)
    Usage("You must specify a network interface");
  if (!g_options.xml_)
    Usage("You must specify a robot description XML file");

  ros::NodeHandle node;

  // Catch attempts to quit
  signal(SIGTERM, quitRequested);
  signal(SIGINT, quitRequested);
  signal(SIGHUP, quitRequested);

  ros::ServiceServer shutdown = node.advertiseService("shutdown", shutdownService);
  ros::ServiceServer reset = node.advertiseService("reset_motors", resetMotorsService);
  ros::ServiceServer halt = node.advertiseService("halt_motors", haltMotorsService);

  //Start thread
  int rv;
  if ((rv = pthread_create(&controlThread, &controlThreadAttr, controlLoop, 0)) != 0)
  {
    ROS_FATAL("Unable to create control thread: rv = %d\n", rv);
    ROS_BREAK();
  }

  ros::spin();
  pthread_join(controlThread, (void **)&rv);

  // Cleanup pid file
  cleanupPidFile();

  return rv;
}
