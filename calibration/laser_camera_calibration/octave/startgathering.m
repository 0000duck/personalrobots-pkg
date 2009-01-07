%% calibdata = startgathering(robot)
%%
%% processes ROS messages for laser scans, robot joints, and checkerboards, and outputs the calibration data
%% usually a bag file needs to be played (but can be run in real-time)
%% robot is optional and used to extract the correct joint values from mechanism control

%% by Rosen Diankov
function calibdata = startgathering(robot)
global lastobjdet lastlaserscan lastmecstate __calibdata __iterationcount

if( ~exist('robot') )
    robot = [];
end

rosoct_add_msgs('std_msgs');
rosoct_add_msgs('checkerboard_detector');
rosoct_add_msgs('robot_msgs');

queuesize = 1000;
__calibdata = {};
lastobjdet = {};
__iterationcount = 0;
__rosoct_msg_unsubscribe("new_ObjectDetection");
success = rosoct_subscribe("new_ObjectDetection", @checkerboard_detector_ObjectDetection, @(msg) objdetcb(msg,robot),queuesize);
if( ~success )
    error('subscribe failed');
end

lastlaserscan = {};
__rosoct_msg_unsubscribe("new_tile_scan");
success = rosoct_subscribe("new_tile_scan", @std_msgs_LaserScan, @(msg) laserscancb(msg,robot),queuesize);
if( ~success )
    error('subscribe failed');
end

lastmecstate = {};
__rosoct_msg_unsubscribe("new_mechanism_state");
success = rosoct_subscribe("new_mechanism_state", @robot_msgs_MechanismState, @(msg) mecstatecb(msg,robot),queuesize);
if( ~success )
    error('subscribe failed');
end

%% wait
input('press any key after data is started streaming: ');
while(1)
    pause(0.1);
    numprocessed = __rosoct_worker(1);
    if( numprocessed == 0 && __iterationcount > 0 )
        display('stream done');
        break;
    end
end

__rosoct_msg_unsubscribe("new_ObjectDetection");
__rosoct_msg_unsubscribe("new_tile_scan");
__rosoct_msg_unsubscribe("new_mechanism_state");

display('gathering calibration done');
calibdata = __calibdata;
__calibdata = {};

function objdetcb(msg, robot)
global lastobjdet
lastobjdet{end+1} = msg;
gathermsgs(robot);

function laserscancb(msg, robot)
global lastlaserscan
lastlaserscan{end+1} = msg;
gathermsgs(robot);

function mecstatecb(msg, robot)
global lastmecstate
lastmecstate{end+1} = msg;
gathermsgs(robot);

function equal = cmp_stamps(stamp1,stamp2)
equal = stamp1.sec==stamp2.sec & abs(stamp1.nsec-stamp2.nsec)<0.001;

function gathermsgs(robot)
global lastobjdet lastlaserscan lastmecstate __calibdata __iterationcount

if( length(lastobjdet) == 0 || length(lastlaserscan) == 0 || length(lastmecstate) == 0 )
    return;
end

%% if all the time stamps are the same, process
if( ~cmp_stamps(lastobjdet{1}.header.stamp,lastlaserscan{1}.header.stamp) || ~cmp_stamps(lastobjdet{1}.header.stamp,lastmecstate{1}.header.stamp) )
    return;
end

objdetmsg = lastobjdet{1};
laserscanmsg = lastlaserscan{1};
mecstatemsg = lastmecstate{1};
%remove
lastobjdet(1) = [];
lastlaserscan(1) = [];
lastmecstate(1) = [];

__iterationcount = __iterationcount + 1;
data = [];
rawjointvalues = cellfun(@(x) x.position, mecstatemsg.joint_states);
if( ~isempty(robot) )
    %% if this fails, then find the joint name
    jointindices = cellfun(@(x) find(cellfun(@(y) strcmp(x,y.name), mecstatemsg.joint_states),1,'first'), robot.jointnames);
    data.jointvalues = rawjointvalues(jointindices);
else
    data.jointvalues = rawjointvalues;
end

%% get plane normal in camera frame
pose = objdetmsg.objects{1}.pose;
R = RotationMatrixFromQuat([pose.orientation.w pose.orientation.x pose.orientation.y pose.orientation.z]);
Tcheckerboard = [R [pose.position.x;pose.position.y;pose.position.z]];
data.Ncamera = [Tcheckerboard(1:3,3);-dot(Tcheckerboard(1:3,3), Tcheckerboard(1:3,4))];

%% get plane normal in laser frame
ranges = transpose(laserscanmsg.ranges);
angles = laserscanmsg.angle_min + (0:(length(ranges)-1))*laserscanmsg.angle_increment;
laserpoints = [cos(angles).*ranges; sin(angles).*ranges];
validpoints = find(ranges > laserscanmsg.range_min & ranges < laserscanmsg.range_max);
if( length(validpoints)<2 )
    display('no valid laser points');
    return;
end

laserpoints = laserpoints(:,validpoints);
linedata = SegmentLines(laserpoints, laserscanmsg.range_max, 0);

%% take the line intersecting with x-axis (middle of laser range)
laserline = linedata(:,find( sign(linedata(2,:)) ~= sign(linedata(4,:)), 1, 'first'));
if( isempty(laserline) )
    display('could not find laser line');
    return;
end

%% threshold length of line
laserdir = laserline(1:2)-laserline(3:4);
if( norm(laserdir) < 0.8 )
    display(sprintf('line not long enough %f', norm(laserdir)));
    return;
end

if( min(norm(laserline(1:2)),norm(laserline(3:4))) > 1.5 )
    display('line is too far');
    return;
end

data.laserline = laserline;

__calibdata{end+1} = data;
display(sprintf('adding data %d', length(__calibdata)));
