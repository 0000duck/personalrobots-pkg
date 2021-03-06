# Software License Agreement (BSD License)
#
# Copyright (c) 2008, Willow Garage, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided
#    with the distribution.
#  * Neither the name of the Willow Garage nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

import roslib
roslib.load_manifest('pr2_dashboard')

import wx
import wx.aui
from wx import xrc
import motor
import ocean_battery_driver
from ocean_battery_driver.ibps_panel import *
import pr2_power_board
from pr2_power_board.pr2_power_board_panel import *


PKG='pr2_dashboard'

class HardwarePanel(wx.Panel):
    def __init__(self, parent):
        wx.Panel.__init__(self, parent)
        
        self._notebook = wx.aui.AuiNotebook(self, wx.ID_ANY, wx.DefaultPosition, wx.DefaultSize, 
                                            wx.aui.AUI_NB_TAB_SPLIT | wx.aui.AUI_NB_TAB_MOVE | wx.aui.AUI_NB_TOP | wx.aui.AUI_NB_SCROLL_BUTTONS)
        
        sizer = wx.BoxSizer(wx.VERTICAL)
        sizer.Add(self._notebook, 1, wx.EXPAND)
        self.SetSizer(sizer)
        
        self._motor_panel = motor.MotorPanel(self)
        self._battery_panel = BatteryPanel(self)
        self._powerboard_panel = PowerBoardPanel(self)
        self._notebook.AddPage(self._powerboard_panel, "Power Board", True)
        self._notebook.AddPage(self._motor_panel, "Motors")
        self._notebook.AddPage(self._battery_panel, "Battery")
        
        
            
