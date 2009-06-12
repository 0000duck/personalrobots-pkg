function plotdoorsolution
close all
clear all
global handle2
doordata;

door_p1 = hinge_global_position;
door_p2 = rotZ(door_angle)*[door_length 0 0 1]';
door_p2 = door_p2(1:2,1) + hinge_global_position(1:2)';

initRobotBox = [local_footprint';
    zeros(1,length(local_footprint));
     ones(1,length(local_footprint))];

a = load('sol.txt');
figure(1)
hold on
axis([-4 4 -4 4]);
axis equal
plot(a(:,1),a(:,2),'g');
for i=1:size(a,1)
    [px py] = robotBox(a(i,1),a(i,2),a(i,3),initRobotBox);
    if(i==1)
        handle2 = plot(px,py,'b-');
    else
        set(handle2,'XData',px,'YData',py);
    end
    
    door_p2 = rotZ(a(i,4))*[door_length 0 0 1]';
    door_p2 = door_p2(1:2,1) + hinge_global_position(1:2)';
    if i==1
      handle1 = plot([door_p1(1) door_p2(1)],[door_p1(2) door_p2(2)],'r-');
    else
        set(handle1,'XData',[door_p1(1) door_p2(1)],'YData',[door_p1(2) door_p2(2)]);
    end
    pause(0.2);
end

function [px, py] = robotBox(x,y,theta,initBox)
result = translateRobot([x y 0])*rotZ(theta)*initBox;
px = [result(1,:) result(1,1)];
py = [result(2,:) result(2,1)];
