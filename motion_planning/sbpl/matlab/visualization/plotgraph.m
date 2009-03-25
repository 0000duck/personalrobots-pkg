% /*
%  * Copyright (c) 2008, Maxim Likhachev
%  * All rights reserved.
%  * 
%  * Redistribution and use in source and binary forms, with or without
%  * modification, are permitted provided that the following conditions are met:
%  * 
%  *     * Redistributions of source code must retain the above copyright
%  *       notice, this list of conditions and the following disclaimer.
%  *     * Redistributions in binary form must reproduce the above copyright
%  *       notice, this list of conditions and the following disclaimer in the
%  *       documentation and/or other materials provided with the distribution.
%  *     * Neither the name of the University of Pennsylvania nor the names of its
%  *       contributors may be used to endorse or promote products derived from
%  *       this software without specific prior written permission.
%  * 
%  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
%  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
%  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
%  * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
%  * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
%  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
%  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
%  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
%  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
%  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
%  * POSSIBILITY OF SUCH DAMAGE.
%  */

%close all;

LDOG = 0;


obsthresh = 254;

if LDOG
    robot_width = 0.1;
    robot_length = 0.2;    
    %robot_width = 0.01;
    %robot_length = 0.02;    
else
    %robot_width = 0.6;
    %robot_length = 0.9;
    robot_width = 0.16;
    robot_length = 0.2;
end;
vehicle = [-robot_length/2.0 -robot_width/2.0
    robot_length/2.0 -robot_width/2.0
    robot_length/2.0  robot_width/2.0
    -robot_length/2.0 robot_width/2.0];


if LDOG
    cellsize = 0.01;
    map = load('env3_10mm.txt');
    %x = load('sol_ldog_10mm_env1_subopt_0p5sec.txt');
    x = load('sol.txt');
else %WG
    cellsize = 0.025;
    %map = load('costmap--soffice1-msfl-r100-i0-c135-I225-d750-H3000-env.txt');
    map = load('cubicle-25mm-inflated-env.txt');
    %map = load('willow-25mm-noninflated.txt');
    x = load('sol.txt');
end;



figure(3);
%h = plot(x(:,1),x(:,2), 'k');
%h = plot(x(:,2),size(map,1)*cellsize-x(:,1), 'k');
h = plot(x(:,1),size(map,1)*cellsize-x(:,2), 'k');
set(h,'LineWidth',3);
hold on;

h = text(x(1,1), size(map,1)*cellsize-x(1,2), 'START');
set(h,'LineWidth',5);
h = text(x(size(x,1),1), size(map,1)*cellsize-x(size(x,1),2), 'GOAL');
set(h,'LineWidth',5);

%plot vehicle
%for pind = 1:ceil(length(x)/40):length(x)
for pind = 1:5:length(x)
    for i = 1:4
        xstartrot = vehicle(i,1)*cos(x(pind,3)) - vehicle(i,2)*sin(x(pind,3)) + x(pind,1);
        ystartrot = vehicle(i,1)*sin(x(pind,3)) + vehicle(i,2)*cos(x(pind,3)) + x(pind,2);
        if i < 4
            xendrot = vehicle(i+1,1)*cos(x(pind,3)) - vehicle(i+1,2)*sin(x(pind,3)) + x(pind,1);;
            yendrot = vehicle(i+1,1)*sin(x(pind,3)) + vehicle(i+1,2)*cos(x(pind,3)) + x(pind,2);;
        else
            xendrot = vehicle(1,1)*cos(x(pind,3)) - vehicle(1,2)*sin(x(pind,3)) + x(pind,1);;
            yendrot = vehicle(1,1)*sin(x(pind,3)) + vehicle(1,2)*cos(x(pind,3)) + x(pind,2);;
        end;
%%%        plot([xstartrot xendrot], [ystartrot yendrot]);
%%%        plot([ystartrot yendrot], [size(map,1)*cellsize-xstartrot size(map,1)*cellsize-xendrot]);
        plot([xstartrot xendrot], [size(map,1)*cellsize-ystartrot size(map,1)*cellsize-yendrot]);
    end;    
end;

if 1
for row = 1:size(map,1)
    for col = 1:size(map,2)
        if(map(row,col) >= obsthresh)
%            plot(col*cellsize,row*cellsize,'x');
            plot(col*cellsize, (size(map,1)-row)*cellsize,'x');
        elseif(map(row,col) > 0)
            plot(col*cellsize, (size(map,1)-row)*cellsize,'.');
        end;
    end;
end;
end;

%%fplot('2',[6 40 0 5]); 
%%fplot('18',[6 40 0 5]); 
%%plot(6*ones(length([2:18]),1), [2:18]);

%axis([min(x(:,1))-1 max(x(:,1))+1 min(x(:,2))-1 max(x(:,2))+1]);
%axis([-1 5 -1 5]);
