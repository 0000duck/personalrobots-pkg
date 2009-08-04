var ROSStartupGadget = Class.create(ROSGadget, {

  initialize: function()
  {
    this.create("Robot Status", 500);

    /*this.robotSelect = document.createElement('select');
    this.robotSelect.id = 'robotSelect';
    this.robotSelect.style.cssFloat = 'right';
    this.robotSelect.style.margin = '2px 0 0 0';
    this.robotSelect.add(new Option("Sim",""), null);
    this.robotSelect.add(new Option("PRF",""), null);
    this.robotSelect.add(new Option("PRG",""), null);
    */

    this.startButton = document.createElement('input');
    this.startButton.type = 'button';
    this.startButton.id = 'startbutton';
    this.startButton.value = 'Start';
    this.startButton.style.cssFloat = 'right';
    this.startButton.style.margin = '2px 0 0 0';
    this.startButton.observe('click', this.start.bind(this) );

    this.titleSpan.appendChild(this.startButton);
    //this.titleSpan.appendChild(this.robotSelect);

    label = document.createElement('label');
    label.style.cssFloat  = 'right';
    label.style.margin = '0 0 0 5px';
    label.innerHTML = "Robot:";

    this.titleSpan.appendChild (label);

    statusDiv = document.createElement('div');
    statusDiv.innerHTML = '<span style="height:25px;line-height:15px;vertical-align:middle;font-size:120%;"><img id="robotStatusIcon"src="images/redbutton.png" width="15" style="border:0;float:left; display:inline; margin-right: 10px;"/> <b>Robot</b>: <span id="robotstatus"></span> </span>';

    this.contentDiv.appendChild(statusDiv);

    this.robotStatusIcon = document.getElementById("robotStatusIcon");
    this.robotStatus = document.getElementById('robotstatus');
    this.robotStatus.innerHTML = 'stopped';

    if (this.pump != null)
      delete this.pump;

    this.pump = new MessagePump();

    var helpTxt="<dl><dt>Purpose:</dt><dd>This gadget is used to bring up the robot</dd><dt>Overview:</dt><dd>The <Start> button located in this gadget's title bar will start the ROS core, and run the selected robot launch file. The content of this gadget displays the current status of the robot. A Red light indicates that the robot is not running, a yellow light indicates the robot is starting up, and a green light inidicates the robot is running.</dd></dl>";
    this.setHelpText(helpTxt);

    var gaugeWidth = 100;
    var gaugeHeight = 100;

    this.gaugeDiv = document.createElement("div");
    this.gaugeDiv.style.border = "0px solid black";
    this.gaugeDiv.style.width = gaugeWidth + "px";
    this.gaugeDiv.style.height = gaugeHeight + "px";

    this.gauge = new google.visualization.Gauge(this.gaugeDiv);

    this.gaugeOptions = {width: gaugeWidth, height: gaugeHeight, 
                         redFrom: 0, redTo: 15, yellowFrom:15, 
                         yellowTo: 30, minorTicks: 5};

    this.gaugeData = new google.visualization.DataTable();
    this.gaugeData.addColumn('string', 'Label');
    this.gaugeData.addColumn('number', 'Value');
    this.gaugeData.addRows(1);
    this.gaugeData.setValue(0, 0, "% Battery");
    this.gaugeData.setValue(0, 1, 0);

    this.gauge.draw(this.gaugeData, this.gaugeOptions);

    this.contentDiv.appendChild(this.gaugeDiv);

  },

  batteryStateChange: function(myself, pump)
  {
    jsonData = eval('(' + pump.response + ')');

    remaining = parseFloat(jsonData.energy_remaining);
    capacity = parseFloat(jsonData.energy_capacity);

    percent = (remaining / capacity) * 100.0;
    myself.gaugeData.setValue(0,1,percent|0);
    myself.gauge.draw(myself.gaugeData, myself.gaugeOptions);
  },

  start: function()
  {
    this.run = true;
    this.pump.sendAJAX('/ros/startup', this, this.started);
    this.robotStatus.innerHTML = 'starting...';
    this.robotStatusIcon.src = 'images/yellowbutton.png';

    this.robotSelect.disabled = true;
  },
  
  stop: function()
  {
    this.pump.sendAJAX('/ros/shutdown', this, this.stopped);
    this.run = false;

    this.robotStatus.innerHTML = 'stopping...';
    this.robotStatusIcon.src = 'images/yellowbutton.png';
  },

  started: function(myself, pump)
  {
    myself.robotStatus.innerHTML = 'started';
    myself.robotStatusIcon.src = 'images/greenbutton.png';
    myself.startButton.value = 'Stop';

    //myself.startButton.observe('click', Event.stop );
    myself.startButton.observe('click', myself.stop.bind(myself) );

    myself.batteryTopic = new ROSTopic("/battery_state");
    myself.batteryTopic.setCallback(myself, myself.batteryStateChange);
  },
  
  stopped: function (myself, pump)
  {
    myself.robotStatus.innerHTML = 'stopped';
    myself.startButton.value = 'Start';
    myself.robotSelect.disabled = false;
    myself.robotStatusIcon.src = 'images/redbutton.png';

    myself.startButton.observe('click', myself.start.bind(myself) );
  },

});



