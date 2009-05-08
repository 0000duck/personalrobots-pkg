import numpy
from math import *

import Image as Image
import ImageDraw as ImageDraw

# Fx, Fy, Tx, Clx, Crx, Cy
class Camera:
  """ See http://www.videredesign.com/docs/calibrate_4.4d.pdf
      But note that the reprojection matrix Q ought to have (1/Tx),
      according to Kurt.
  """
   
  def __init__(self, params):
    self.params = params
    (Fx, Fy, Tx, Clx, Crx, Cy) = params

    # Left Projection matrix
    self.Pl = numpy.array([
      [ Fx, 0,  Clx, 0 ],
      [ 0,  Fy, Cy,  0 ],
      [ 0,  0,  1,   0 ]
    ])

    # Right Projection matrix
    self.Pr = numpy.array([
      [ Fx, 0,  Crx, Fx*-Tx ],
      [ 0,  Fy, Cy,  0      ],
      [ 0,  0,  1,   0      ]
    ])

    self.Q = numpy.array([
      [ 1, 0,  0, -Clx ],
      [ 0, 1,  0, -Cy ],
      [ 0, 0,  0,  Fx ],
      [ 0, 0, 1 / Tx, (Crx-Clx)/Tx ]
    ])

  def __cmp__(self, other):
    return cmp(self.params, other.params)

  # Used as input to the LevMarq optimizer.
  def cart_to_disp(self):
    (Fx, Fy, Tx, Clx, Crx, Cy) = self.params
    return [
      Fx,   0,    Clx,              0,
      0,    Fy,   Cy,               0,
      0,    0,    (Clx-Crx),        Fx*Tx,
      0,    0,    1,                0
    ]

  # Used as input to the LevMarq optimizer.
  def disp_to_cart(self):
    (Fx, Fy, Tx, Clx, Crx, Cy) = self.params
    return [
      1./Fx,    0,          0,           -Clx/Fx,
      0,   	1./Fy,        0,           -Cy/Fy,
      0,         0,          0,                1,
      0,         0,      1./(Tx*Fx),    -(Clx-Crx)/(Fx*Tx)
    ]

  def pix2cam(self, u, v, d):
    """ takes pixel space u,v,d and returns camera space X,Y,Z """
    if 0:
      res = numpy.dot(self.Q, numpy.array( [ [u], [v], [d], [1] ])).transpose()[0]
      (x, y, z, w) = res
      return (x / w, y / w, z / w)
    else:
      (Fx, Fy, Tx, Clx, Crx, Cy) = self.params
      x = u - Clx
      y = v - Cy
      z = Fx
      w = (d + (Crx-Clx)) / Tx
      return (x / w, y / w, z / w)

  def cam2pixLR(self, X, Y, Z):
    """ takes camera space (X,Y,Z) and returns the pixel space (u,v) for both cameras """
    if 0:
      def xform(P, pt):
        (x,y,w) = numpy.dot(P, pt).transpose()[0]
        return (x/w, y/w)
      campt = numpy.array([ [X], [Y], [Z], [1] ])
      (xl,yl) = xform(self.Pl, campt)
      (xr,yr) = xform(self.Pr, campt)
      assert yl == yr
      return ((xl,yl), (xr,yr))
    else:
      (Fx, Fy, Tx, Clx, Crx, Cy) = self.params
      xl = Fx * X + Clx * Z
      y = Fy * Y + Cy * Z
      w = Z

      xr = Fx * X + Crx * Z + (Fx * -Tx)
      rw = 1.0 / w
      y_rw = y * rw
      return ((xl * rw, y_rw), (xr * rw, y_rw))

  def cam2pix(self, X, Y, Z):
    """ takes camera space (X,Y,Z) and returns the pixel space (x,y,d) for left camera"""
    ((xl,yl), (xr,yr)) = self.cam2pixLR(X, Y, Z)
    return (xl, yl, xl - xr)

class VidereCamera(Camera):
  def __init__(self, config_str):
    section = ""
    in_proj = 0
    matrix = []
    for l in config_str.split('\n'):
      if len(l) > 0 and l[0] == '[':
        section = l.strip('[]')
      ws = l.split()
      if ws != []:
        if section == "right camera" and ws[0].isalpha():
          in_proj = (ws[0] == 'proj')
        elif in_proj:
          matrix.append([ float(s) for s in l.split() ])
    Fx = matrix[0][0]
    Fy = matrix[1][1]
    Cx = matrix[0][2]
    Cy = matrix[1][2]
    Tx = -matrix[0][3] / Fx
    Tx *= 1e-3
    Camera.__init__(self, (Fx, Fy, Tx, Cx, Cx, Cy))

class StereoCamera(Camera):
  def __init__(self, right_cam_info_msg):
    matrix = numpy.array(right_cam_info_msg.P).reshape((3,4))
    Fx = float(matrix[0][0])
    Fy = float(matrix[1][1])
    Cx = float(matrix[0][2])
    Cy = float(matrix[1][2])
    Tx = float(-matrix[0][3] / Fx)
    Camera.__init__(self, (Fx, Fy, Tx, Cx, Cx, Cy))

class DictCamera(Camera):
  def __init__(self, pd):

    Fx = float(pd['Fx'])
    Fy = float(pd['Fy'])
    Clx = float(pd['Clx'])
    Crx = float(pd['Crx'])
    Cy = float(pd['Cy'])
    Tx = float(pd['Tx'])

    Camera.__init__(self, (Fx, Fy, Tx, Clx, Crx, Cy))
