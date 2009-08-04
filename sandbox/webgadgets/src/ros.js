////////////////////////////////////////////////////////////////////////////////
// Message Pump class. Handles sending and receiving AJAX messages
var MessagePump = Class.create({
  emptyFunction: function() {},

  initialize: function()
  {
    this.response = 'no message';
    this.responseFunc = this.emptyFunction;
    this.responseObj = null;
  },
  
  sendAJAX: function (request, _responseObj, _responseFunc)
  {
    if (_responseFunc != null)
    {
      this.responseFunc = _responseFunc;
      this.responseObj = _responseObj;
    }

    new Ajax.Request(request, 
        {method: 'get', 
         onSuccess: function(transport) 
         { this.response = transport.responseText; }.bind(this),
         onFailure: function()
         { this.response = 'error'; }.bind(this),
         onComplete: function()
         { this.completeFunction(); }.bind(this)
        });
  },
     
  completeFunction: function()
  {
    this.responseFunc(this.responseObj, this);
  }
  
});


////////////////////////////////////////////////////////////////////////////////
// ROS TF class. 
var ROSTF = Class.create({
  initialize: function(_tf)
  {
    this.tf = _tf
    this.pump = new MessagePump();
    this.objCB = null;
    this.funcCB = null;
    this.timeout = 100;
  },


  subscribe: function (obj, callback)
  {
    this.pump.sendAJAX('/ros/tfsub' + this.tf, obj, callback);
  },

  getMessage: function(obj, callback)
  {
    this.pump.sendAJAX('/ros/tfget' + this.tf, obj, callback);
  },

  setCallback: function(obj, callback, _timeout)
  {
    this.objCB = obj;
    this.funcCB  = callback;
    this.timeout = _timeout;
    this.subscribe(this, this.subscribed);
  },

  subscribed: function(myself, pump)
  {
    myself.getMessage(myself, myself.gotMessage);
  },

  gotMessage: function(myself, pump)
  {
    if (myself.funcCB != null)
    {
      myself.funcCB(myself.objCB, pump);
    }

    setTimeout( function () 
        {myself.getMessage(myself, myself.gotMessage);} , myself.timeout);

  },

});


////////////////////////////////////////////////////////////////////////////////
// ROS Topic class, a simple interface to ros topics
var ROSTopic = Class.create({
  initialize: function(_topic)
  {
    this.objCB = null;
    this.funcCB = null;
    this.topic = _topic
    this.pump = new MessagePump();
  },

  setCallback: function(obj, callback)
  {
    this.objCB = obj;
    this.funcCB  = callback;
    this.subscribe(this, this.subscribed);
  },

  subscribed: function(myself, pump)
  {
    myself.getMessage(myself, myself.gotMessage);
  },

  subscribe: function (obj, callback)
  {
    this.pump.sendAJAX('/ros/subscribe' + this.topic, obj, callback);
  },

  announce: function (obj, callback)
  {
    this.pump.sendAJAX('/ros/announce' + this.topic, obj, callback);
  },

  unsubscribe: function (obj, callback)
  {
    this.pump.sendAJAX('/ros/unsubscribe' + this.topic, obj, callback);
    this.funcCB = null;
    this.objCB = null;
  },

  getMessage: function(obj, callback)
  {
    this.pump.sendAJAX('/ros/get' + this.topic, obj, callback);
  },

  gotMessage: function(myself, pump)
  {
    if (myself.funcCB != null)
    {
      myself.funcCB(myself.objCB, pump);
      myself.getMessage(myself, myself.gotMessage);
    }
  },

  publish: function(msg, obj, callback)
  {
    this.pump.sendAJAX('/ros/pub' + this.topic + msg, obj, callback);
  },

});


////////////////////////////////////////////////////////////////////////////////
// A ROS Gadget. This will load a ros gadget
var ROSGadget = Class.create({
  create: function(titleName, width)
  {
    var maxY = 70;
    allGadgets = document.getElementById('maincontent').getElementsByTagName('div');

    for (i=0; i<allGadgets.length; i++)
    {
      if (allGadgets[i].id == 'gadget')
      {
        y = parseInt(allGadgets[i].style.top);
        h = allGadgets[i].clientHeight;

        if (y + h > maxY)
        {
          maxY = y+h;
        }
      }
    }

    var d = new Date();

    this.mainDiv = document.createElement('div');
    this.mainDiv.id = 'gadget';
    this.mainDiv.name = titleName + d.getTime();
    this.mainDiv.style.width = width + 'px';
    this.mainDiv.style.left = "10px";
    this.mainDiv.style.top = maxY + "px";

    this.headerDiv = document.createElement('div');
    this.headerDiv.id = 'header';

    this.contentDiv = document.createElement('div');
    this.contentDiv.id = 'content';
    
    this.titleSpan = document.createElement("span");
    title = document.createElement("b");
    title.style.id = "gadgetTitle";
    title.style.cssFloat = "left";
    title.style.margin= "0 0 0 10px";
    title.innerHTML = titleName;

    this.closeButton = document.createElement('a');
    this.closeButton.id = 'closeButton';
    this.closeButton.href='#';
    this.closeButton.id ='xbutton';
    this.closeButton.innerHTML = "<img src='images/xicon.png' style='margin-top:2px;' height='20px'>";

    this.helpButton = document.createElement('a');
    this.helpButton.href='#';
    this.helpButton.id ='xbutton';
    this.helpButton.innerHTML = "<img src='images/questionicon.png' style='margin-top:2px;'  height='20px'>";

    this.titleSpan.appendChild(this.closeButton);
    this.titleSpan.appendChild(this.helpButton);
    this.titleSpan.appendChild(title);

    this.headerDiv.appendChild(this.titleSpan);

    this.mainDiv.appendChild(this.headerDiv);
    this.mainDiv.appendChild(this.contentDiv);
    document.getElementById('maincontent').appendChild(this.mainDiv);


    this.dragging = false;
    this.divDragStart = {'x': 0, 'y':0};
    this.mainDivStart = {'x': 0, 'y':0};

    this.headerDiv.observe('mousedown', this.dragDivStart.bind(this));
    this.closeButton.observe('click', this.closeGadget.bind(this) );

    this.helpButton.observe('click', this.showHelp.bind(this) );

    this.help = new HelpGadget(this.mainDiv, titleName);
  },

  setHelpText : function(txt)
  {
    this.help.setContent(txt);
  },

  closeGadget : function(e)
  {
    this.mainDiv.remove();
  },

  showHelp : function(e)
  {
    this.help.showHelp();
  },

  dragDivStart : function(e)
  {
    this.dragging = true;

    this.divDragStart.x = e.pageX;
    this.divDragStart.y = e.pageY;

    this.mainDivStart.x = parseInt(this.mainDiv.style.left);
    this.mainDivStart.y = parseInt(this.mainDiv.style.top);

    document.observe('mousemove', this.dragDivMove.bind(this));
    document.observe('mouseup', this.dragDivStop.bind(this));
  },

  dragDivStop : function(e)
  {
    this.dragging = false;
    document.observe('mousemove', Event.stop);
    document.observe('mouseup', Event.stop);
  },

  dragDivMove: function (e)
  {
    if (this.dragging)
    {
      delta = {'x' : e.pageX - this.divDragStart.x, 
               'y' : e.pageY - this.divDragStart.y};

      newX = delta.x + this.mainDivStart.x;
      newY = delta.y + this.mainDivStart.y;

      width = this.mainDiv.clientWidth;
      height = this.mainDiv.clientHeight;

      headerHeight = document.getElementById('mainheader').clientHeight + 10;

      newX = Math.max(10, newX);
      newY = Math.max(headerHeight, newY);

      /// This will lock the widgets to the viewable area.
      /*if (newX + width > document.body.clientWidth-10)
        newX = document.body.clientWidth - width - 10;
      
      if (newY + height > document.body.clientHeight-headerHeight)
        newY = document.body.clientHeight - height - headerHeight;
        */


      this.mainDiv.style.left = newX + "px";
      this.mainDiv.style.top = newY + "px";
    }
  }

});

