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
#

## Main module for visual odometry.
# authors: jamesb

import vop
import votools as VO
from timer import Timer

import os

import Image
from math import *
import numpy
import numpy.linalg

scratch = " " * (640 * 480)

import transformations

class Pose:
  def __init__(self, R=None, S=None):
    if R == None:
      R = numpy.identity(3)
    if S == None:
      S = ( 0, 0, 0)
    self.M = numpy.mat(numpy.vstack((numpy.hstack((R, numpy.array(S).reshape((3,1)))), numpy.array([0, 0, 0, 1]))))

  def fromlist(self, L):
    a = numpy.array(L)
    a.shape = 4,4
    self.M = numpy.mat(a)

  def tolist(self):
    return sum(self.M.tolist(), [])

  def invert(self):
    r = Pose()
    r.M = numpy.linalg.inv(self.M)
    return r

  def __mul__(self, other):
    r = Pose()
    r.M = self.M * other.M
    return r

  def __invert__(self):
    r = Pose()
    #r.M = self.M.T
    r.M = numpy.linalg.inv(self.M)
    return r

  def concatenate(self, P):
    r = Pose()
    r.M = numpy.dot(self.M, P.M)
    return r

  def xform(self, x, y, z):
    if 0:
      x,y,z,w = tuple(numpy.array(numpy.dot(self.M, numpy.array([x, y, z, 1]) ))[0])
      assert w == 1.0
      return (x,y,z)
    else:
      nx = float(self.M[0,0]) * x + float(self.M[0,1]) * y + float(self.M[0,2]) * z + float(self.M[0,3])
      ny = float(self.M[1,0]) * x + float(self.M[1,1]) * y + float(self.M[1,2]) * z + float(self.M[1,3])
      nz = float(self.M[2,0]) * x + float(self.M[2,1]) * y + float(self.M[2,2]) * z + float(self.M[2,3])
      return (nx, ny, nz)

  def quaternion(self):
    return transformations.quaternion_from_rotation_matrix(self.M)

  def euler(self):
    return transformations.euler_from_rotation_matrix(self.M)

  def comparison(self, other):
    p0 = self.xform(0, 0, 0)
    p1 = other.xform(0, 0, 0)
    return "dist=%f" % (sqrt(sum([(a-b)**2 for a,b in zip(p0,p1)])))

  def compare(self, other):
    p0 = numpy.array(self.xform(0, 0, 0))
    p1 = numpy.array(other.xform(0, 0, 0))
    eu0 = self.euler()
    eu1 = other.euler()
    d = p0 - p1
    eud = [abs(eu0[i] - eu1[i]) for i in [0,1,2]]
    return (sqrt(numpy.dot(d, d.conj())), eud[0], eud[1], eud[2])

  def qangle(self, other):
    (wx,wy,wz,ww) = self.quaternion()
    yaw1 = atan2(2*(wx*wy + ww*wz), wx*wx + ww*ww - wy*wy - wz*wz)
    (wx,wy,wz,ww) = other.quaternion()
    yaw2 = atan2(2*(wx*wy + ww*wz), wx*wx + ww*ww - wy*wy - wz*wz)
    return yaw2 - yaw1

  def angle(self, other):
    p0 = self.xform(0,0,0)
    d0 = self.xform(0,0,1)
    p1 = other.xform(0,0,0)
    d1 = other.xform(0,0,1)
    v0 = [ (b - a) for (a,b) in zip(p0,d0) ]
    v1 = [ (b - a) for (a,b) in zip(p1,d1) ]
    dot = sum([(a*b) for a,b in zip(v0,v1)])
    if dot >= 1.0:
      th = 0.0
    else:
      th = acos(dot)
    return th

  def further_than(self, other, pos_d, ori_d):
    p0 = self.xform(0,0,0)
    p1 = other.xform(0,0,0)
    th = self.qangle(other)
    d = sqrt(sum([(a - b)**2 for (a,b) in zip(p0, p1)]))
    return (d > pos_d) or (abs(th) > ori_d)

  def d(self, other):
    p0 = self.xform(0,0,0)
    p1 = other.xform(0,0,0)
    return sqrt(sum([(a - b)**2 for (a,b) in zip(p0, p1)]))

  def distance(self):
    x,y,z = self.xform(0,0,0)
    return sqrt(x * x + y * y + z * z)

  def assert_sane(self):
    rot = self.M[0:3,0:3]
    #assert numpy.alltrue(numpy.abs(((rot * rot.T) - numpy.identity(3))) < 1.0e-3)

def from_xyz_euler(xyz, euler):
  R = transformations.rotation_matrix_from_euler(euler[0], euler[1], euler[2], 'sxyz')
  return Pose(R[:3,:3], xyz)

import fast

class FeatureDetector:

  def name(self):
    return self.__class__.__name__

  def __init__(self):
    self.thresh = self.default_thresh
    self.cold = True

  def detect(self, frame, target_points):

    features = self.get_features(frame, target_points)
    if len(features) < (target_points * 0.5) or len(features) > (target_points * 2.0):
        (lo,hi) = self.threshrange
        for i in range(7):
          self.thresh = 0.5 * (lo + hi)
          features = self.get_features(frame, target_points)
          if len(features) < target_points:
            hi = self.thresh
          if len(features) > target_points:
            lo = self.thresh
        self.thresh = 0.5 * (lo + hi)

    # Try to be a bit adaptive for next time
    if len(features) > (target_points * 1.1):
        self.thresh *= 1.05
    if len(features) < (target_points * 0.9):
        self.thresh *= 0.95
    return features

# Feature detectors that return features in order (i.e. strongest first)
# can be simpler.  Just always keep the threshold high enough to give
# too many responses, and take the top N.

class FeatureDetectorOrdered:

  def name(self):
    return self.__class__.__name__

  def __init__(self):
    self.thresh = self.default_thresh
    self.cold = True

  def detect(self, frame, target_points):

    features = self.get_features(frame, target_points)
    # Too few features, so lower threshold
    while (len(features) < target_points) and (self.thresh > self.threshrange[0]):
      self.thresh = float(max(self.threshrange[0], self.thresh / 2))
      features = self.get_features(frame, target_points)

    # Try to be a bit adaptive for next time
    if len(features) > (target_points * 2):
      self.thresh *= 2
    if len(features) < (target_points * 1.25):
      self.thresh *= 0.95
    return features[:target_points]

def FAST(imdata, xsize, ysize, thresh):
  kp = fast.fast(imdata, xsize, ysize, 9, thresh)
  return sorted(fast.nonmax(kp), key = lambda x:(x[2],x[0],x[1]), reverse = True)

class FeatureDetectorFast(FeatureDetectorOrdered):

  default_thresh = 10
  threshrange = (0.5,300)

  def get_features(self, frame, target_points):
    assert len(frame.rawdata) == (frame.size[0] * frame.size[1])
    feat = FAST(frame.rawdata, frame.size[0], frame.size[1], self.thresh)
    return [ (x,y) for (x,y,r) in feat if (16 <= x and x <= (640-16) and (16 <= y) and y < (480-16)) ]

class FeatureDetector4x4:

  def __init__(self, fd):
    self.fds = [ fd() for i in range(16) ]

  def name(self):
    return "4x4 " + self.fds[0].__class__.__name__

  def detect(self, frame, target_points):
    master = Image.fromstring("L", frame.size, frame.rawdata)
    allpts = []
    xbase = 16
    ybase = 16
    w = frame.size[0] - 32
    h = frame.size[1] - 32
    for x in range(4):
      for y in range(4):
        xleft = xbase + x * (w/4)
        ytop = ybase + y * (h/4)
        subimage = master.crop((xleft, ytop, xleft + (w/4), ytop + (h/4)))
        assert subimage.size == ((w/4), (h/4))

        class FrameAdapter:
          def __init__(self, im):
            self.size = im.size
            self.rawdata = im.tostring()

        subpts = self.fds[4 * x + y].detect(FrameAdapter(subimage), target_points / 16)
        allpts += [(xleft + xp, ytop + yp) for (xp,yp) in subpts]
    return allpts

class FeatureDetectorHarris(FeatureDetector):

  default_thresh = 1e-3
  threshrange = (1e-4,1e-2)

  def get_features(self, frame, target_points):
    return VO.harris(frame.rawdata, frame.size[0], frame.size[1], int(target_points * 1.2), self.thresh, 2.0)

import starfeature

class FeatureDetectorStar(FeatureDetector):

  default_thresh = 30.0
  threshrange = (1,64)
  line_thresh = 10.0

  def get_features(self, frame, target_points):
    sd = starfeature.star_detector(frame.size[0], frame.size[1], 5, self.thresh, self.line_thresh)
    return [ (x,y) for (x,y,s,r) in sd.detect(frame.rawdata) ]

########################################################################

class DescriptorScheme:

  def name(self):
    return self.__class__.__name__

  def match(self, af0, af1):
    if af0.kp == [] or af1.kp == []:
      return []
    Xs = vop.array([k[0] for k in af1.kp])
    Ys = vop.array([k[1] for k in af1.kp])
    pairs = []
    for (i,(ki,di)) in enumerate(zip(af0.kp,af0.descriptors)):
      predX = (abs(Xs - ki[0]) < 64)
      predY = (abs(Ys - ki[1]) < 32)
      hits = vop.where(predX & predY, 1, 0).tostring()
      best = self.search(di, af1, hits)
      if best != None:
        pairs.append((i, best[0], best[1]))
    return pairs

class DescriptorSchemeSAD(DescriptorScheme):

  def collect(self, frame):
    if not hasattr(frame, "lgrad"):
      frame.lgrad = " " * (frame.size[0] * frame.size[1])
      VO.ost_do_prefilter_norm(frame.rawdata, frame.lgrad, frame.size[0], frame.size[1], 31, scratch)
    frame.descriptors = [ VO.grab_16x16(frame.lgrad, frame.size[0], p[0]-7, p[1]-7) for p in frame.kp ]
    frame.matcher = None

  def search(self, di, af1, hits):
      i = VO.sad_search(di, af1.descriptors, hits)
      if i == None:
        return None
      else:
        return (i, 0)

import calonder

class DescriptorSchemeCalonder(DescriptorScheme):
  def __init__(self):
    self.cl = calonder.classifier()
    #self.cl.setThreshold(0.0)
    filename = '/u/prdata/calonder_trees/current.rtc'
    assert os.access(filename, os.R_OK)
    self.cl.read(filename)

  def collect(self, frame):
    im = Image.fromstring("L", frame.size, frame.rawdata)
    frame.matcher = calonder.BruteForceMatcher(self.cl.dimension())
    if 0:
      frame.descriptors = []
      for (x,y,d) in frame.kp:
        patch = im.crop((x-16,y-16,x+16,y+16))
        sig = self.cl.getSignature(patch.tostring(), patch.size[0], patch.size[1])
        frame.descriptors.append(sig)
        frame.matcher.addSignature(sig)
    else:
      frame.descriptors = self.cl.getSignatures(im, [ (x,y) for (x,y,d) in frame.kp ])
      for sig in frame.descriptors:
        frame.matcher.addSignature(sig)

  def search(self, di, af1, hits):
    match = af1.matcher.findMatch(di, hits)
    return match

uniq_track_id = 100

class Track:
  def __init__(self, p0, p0id, p1, p1id, p1pose, cam):
    global uniq_track_id
    self.p = [p0, p1]
    self.id = [p0id, p1id]
    self.lastpt = p1
    self.alive = True
    #print p0id,p0,p1id,p1,p1pose.xform(*cam.pix2cam(*p1)),uniq_track_id
    self.sba_track = VO.point_track(p0id, p0, p1id, p1, p1pose.xform(*cam.pix2cam(*p1)), uniq_track_id)
    self.uniq_track_id = uniq_track_id
    uniq_track_id += 1
  def kill(self):
    self.alive = False
  def extend(self, p1, p1id):
    self.p.append(p1)
    self.id.append(p1id)
    self.lastpt = p1
    self.sba_track.extend(p1id, p1)

class VisualOdometer:

  def __init__(self, cam, **kwargs):
    self.cam = cam
    self.timer = {}
    for t in ['feature', 'disparity', 'descriptor_collection', 'temporal_match', 'solve', 'tracks', 'sba' ]:
      self.timer[t] = Timer()
    self.pe = VO.pose_estimator(*self.cam.params)
    self.prev_frame = None
    self.pose = Pose()
    self.inl = 0
    self.outl = 0
    self.num_frames  = 0
    self.tot_inliers = 0
    self.tot_matches = 0
    self.tot_points  = 0
    self.ext_frames = 1000000
    self.keyframe = None
    self.log_keyframes = []
    self.log_keyposes = []
    self.pairs = []
    self.tracks = set()
    self.all_tracks = set()
    self.posechain = []

    self.position_thresh = kwargs.get('position_keypoint_thresh', 0.5)
    self.angle_thresh = kwargs.get('angle_keypoint_thresh', (2 * pi) / (5. / 360))
    self.inlier_thresh = kwargs.get('inlier_thresh', 175)
    self.feature_detector = kwargs.get('feature_detector', FeatureDetectorFast())
    self.descriptor_scheme = kwargs.get('descriptor_scheme', DescriptorSchemeSAD())
    self.inlier_error_threshold = kwargs.get('inlier_error_threshold', 3.0)
    self.scavenge = kwargs.get('scavenge', False)
    self.sba = kwargs.get('sba', None)
    self.targetkp = kwargs.get('targetkp', 300)
    self.num_ransac_iters = kwargs.get('ransac_iters', 100)

    self.pe.setInlierErrorThreshold(self.inlier_error_threshold)
    self.pe.setNumRansacIterations(self.num_ransac_iters)

  def name(self):
    return "VisualOdometer (%s %s iet=%.1f sba=%s)" % (self.feature_detector.name(), self.descriptor_scheme.name(), self.inlier_error_threshold, str(self.sba))

  def reset_timers(self):
    for n,t in self.timer.items():
      t.reset();

  def average_time_per_frame(self):
    niter = self.num_frames
    return 1e3 * sum([t.sum for t in self.timer.values()]) / niter

  def summarize_timers(self):
    niter = self.num_frames
    print ""
    print "total number of keyframes/frames:", len(self.log_keyframes), "/", niter
    print "average number of points  per frame: ", self.tot_points/niter
    print "average number of matches per frame: ", self.tot_matches/niter
    print "average number of inliers per frame: ", self.tot_inliers/niter
    print ""
    if niter != 0:
      for n,t in self.timer.items():
        print "%-20s %fms" % (n, 1e3 * t.sum / niter)
      print "%-20s %fms" % ("TOTAL", self.average_time_per_frame())


  def find_keypoints(self, frame):
    self.timer['feature'].start()
    frame.kp2d = self.feature_detector.detect(frame, self.targetkp)
    self.timer['feature'].stop()

  def find_disparities(self, frame):
    self.timer['disparity'].start()
    disparities = [frame.lookup_disparity(x,y) for (x,y) in frame.kp2d]
    frame.kp = [ (x,y,z) for ((x,y),z) in zip(frame.kp2d, disparities) if z]
    #print "disparities", len(frame.kp2d), len(frame.kp)

    if 0:
      import pylab
      pylab.imshow(numpy.fromstring(frame.lf.tostring(), numpy.uint8).reshape(480,640), cmap=pylab.cm.gray)
      pylab.scatter([x for (x,y) in frame.kp2d], [y for (x,y) in frame.kp2d], label = '2d', c = 'red')
      pylab.scatter([x for (x,y,d) in frame.kp], [y for (x,y,d) in frame.kp], label = 'disparities', c = 'green')
      pylab.legend()
      pylab.show()

    self.timer['disparity'].stop()

  lgrad = " " * (640 * 480)
  def collect_descriptors(self, frame):
    self.timer['descriptor_collection'].start()
    self.descriptor_scheme.collect(frame)
    self.timer['descriptor_collection'].stop()

  def temporal_match(self, af0, af1, want_distances = False):
    """ Match features between two frames.  Returns a list of pairs of indices into the features in the two frames, and optionally a distance value for each pair, if want_distances in True.  """
    self.timer['temporal_match'].start()
    pairs = self.descriptor_scheme.match(af0, af1)
    if not want_distances:
      pairs = [(a,b) for (a,b,d) in pairs]
    self.timer['temporal_match'].stop()
    return pairs

  def solve(self, k0, k1, pairs, polish = True):
    self.timer['solve'].start()
    if pairs != []:
      #r = self.pe.estimate(k0, k1, pairs)
      r = self.pe.estimate(k1, k0, [ (b,a) for (a,b) in pairs ], polish)
    else:
      r = None
    self.timer['solve'].stop()

    return r

  def mkpose(self, rot, shift):
    r33 = numpy.mat(numpy.array(rot).reshape(3,3))
    return Pose(r33, numpy.array(shift))

    #pr = Pose(r33, numpy.array([0,0,0]))
    #ps = Pose(numpy.mat([[1,0,0],[0,1,0],[0,0,1]]), numpy.array(shift))
    #return pr * ps

  def show_pairs(self, pairs, f0, f1):
    print "*** SHOWING PAIRS FOR FRAMES ", f0.id, f1.id, "***"
    print f0.id, "has", len(f0.kp), "keypoints"
    print f1.id, "has", len(f1.kp), "keypoints"
    print "There are", len(pairs), "pairs"
    import pylab
    for (a,b) in pairs:
      pylab.plot([ f0.kp[a][0], f1.kp[b][0] ], [ f0.kp[a][1], f1.kp[b][1] ])
    pylab.imshow(numpy.fromstring(f0.lf.tostring(), numpy.uint8).reshape(480,640), cmap=pylab.cm.gray)
    pylab.scatter([x for (x,y,d) in f0.kp], [y for (x,y,d) in f0.kp], label = '%d kp' % f0.id, c = 'red')
    pylab.scatter([x for (x,y,d) in f1.kp], [y for (x,y,d) in f1.kp], label = '%d kp' % f1.id, c = 'green')
    pylab.legend()
    pylab.show()

  def proximity(self, f0, f1, scavenger = False):
    """Given frames f0, f1, returns (inliers, pose) where pose is the transform that maps f1's frame to f0's frame.)"""
    self.num_frames += 1

    pairs = self.temporal_match(f0, f1)
    #self.show_pairs(pairs, f0, f1)
    if len(pairs) > 10:
      solution = self.solve(f0.kp, f1.kp, pairs, True)
      if scavenger and solution and solution[0] > 10:
        (inl, rot, shift) = solution
        pose = self.mkpose(rot, shift)
        solution = self.scavenger(pose, f0, f1)
      if solution:
        (inl, rot, shift) = solution
      else:
        return (0, None)
      #print "...and", inl, "inliers"
      pose = self.mkpose(rot, shift)
      return (inl, pose)
    else:
      return (0, None)

  def scavenger(self, diff_pose, af0, af1):
    Xs = vop.array([k[0] for k in af1.kp])
    Ys = vop.array([k[1] for k in af1.kp])
    pairs = []
    fwd_pose = ~diff_pose
    for (i,(ki,di)) in enumerate(zip(af0.kp,af0.descriptors)):
      (x,y,d) = self.cam.cam2pix(*fwd_pose.xform(*self.cam.pix2cam(*ki)))
      predX = (abs(Xs - x) < 4)
      predY = (abs(Ys - y) < 4)
      hits = vop.where(predX & predY, 1, 0).tostring()
      best = self.descriptor_scheme.search(di, af1, hits)
      if best != None:
        pairs.append((i, best[0], best[1]))
    self.pairs = [(i0,i1) for (i0,i1,d) in pairs]
    if False:
      import pylab
      f0,f1 = af0,af1
      for (a,b) in self.pairs:
        pylab.plot([ f0.kp[a][0], f1.kp[b][0] ], [ f0.kp[a][1], f1.kp[b][1] ])
      pylab.imshow(numpy.fromstring(af0.lf.tostring(), numpy.uint8).reshape(480,640), cmap=pylab.cm.gray)
      pylab.scatter([x for (x,y,d) in f0.kp], [y for (x,y,d) in f0.kp], label = '%d kp' % f0.id, c = 'red')
      pylab.scatter([x for (x,y,d) in f1.kp], [y for (x,y,d) in f1.kp], label = '%d kp' % f1.id, c = 'green')
      pylab.legend()
      pylab.show()
    solution = self.solve(af0.kp, af1.kp, self.pairs)
    return solution

  def maintain_tracks(self, f0, f1):
    self.timer['tracks'].start()
    pairs = self.temporal_match(f0, f1)
    solution = self.solve(f0.kp, f1.kp, pairs)
    if solution and solution[0] > 5:
      (inl, rot, shift) = solution
      diff_pose = self.mkpose(rot, shift)
      if self.scavenge:
        (inl, rot, shift) = self.scavenger(diff_pose, f0, f1)
    # for points x in f0, pairmap[x] is the corresponding point in f1
    pairmap = dict([(p1,p0) for (p0,p1) in self.pe.inliers()])

    for t in self.tracks:
      if not (t.lastpt in pairmap):
        t.kill()

    if self.posechain != []:
      # Only keep tracks that have a recent frame
      (fix_sz, free_sz, niter) = self.sba
      nfr = fix_sz + free_sz
      print "posechain length", len(self.posechain)
      print "posechain ", (self.posechain)
      if len(self.posechain) > nfr:
        oldest_useful_frame = self.posechain[-nfr][0].id
      else:
        oldest_useful_frame = self.posechain[0][0].id
      #print "oldest_useful_frame", oldest_useful_frame

      def age(t):
        return max([ i for i in t.id if i < 1000000])
      self.tracks = set([ t for t in self.tracks if oldest_useful_frame <= age(t)])

    oldtails = set([t.lastpt for t in self.tracks])
    for t in self.tracks:
      if t.alive:
        t.extend(pairmap[t.lastpt], f1.id)

    #print len(self.tracks), [ len(t.p) for t in self.tracks ]
    for p0 in set(pairmap) - oldtails:
      p1 = pairmap[p0]
      newtrack = Track(p0, f0.id, p1, f1.id, f1.pose, self.cam)
      self.tracks.add(newtrack)
      self.all_tracks.add(newtrack)

    if 0:
      import pylab
      print "Inliers:", len(self.pe.inliers())

      f1in = [ a for (a,b) in self.pe.inliers()]
      pylab.scatter([x for (x,y,d) in f1in], [y for (x,y,d) in f1in])

      print "There are", len(self.tracks)
      for t in self.tracks:
        pylab.plot([x for (x,y,d) in t.p], [y for (x,y,d) in t.p])
      pylab.xlim((0, 640))
      pylab.ylim((0, 480))
      pylab.show()
    
    # Run through all tracks.  If two tracks share an endpoint, delete the shorter track
    by_lastpt = [e[1] for e in sorted([(t.lastpt, t) for t in self.tracks if t.alive])]
    tocull = set()
    for (t0,t1) in zip(by_lastpt, by_lastpt[1:]):
      if t0.lastpt == t1.lastpt:
        if 0:
          print "Detected shared point", t0.uniq_track_id, t1.uniq_track_id
          print t0.lastpt, t0.p
          print t1.lastpt, t1.p
          print
        tocull.add(t0)
        tocull.add(t1)
        if len(t0.p) > len(t1.p):
          tocull.add(t0)
        else:
          tocull.add(t1)
    #print "Killing tracks because of shared point:", [t.id for t in tocull]
    self.tracks -= tocull

    for t in self.tracks:
      for (x,y,d) in t.p:
        assert d != 0

    self.timer['tracks'].stop()

  def sba_add_frame(self, frame):
    self.posechain.append((frame,VO.frame_pose(frame.id, frame.pose.tolist())))
    
  def sba_handle_frame(self, frame):
    self.timer['sba'].start()

    # ids is the list of frameids for which we have observations
    ids = set()
    for t in self.tracks:
      #print len(t.p), t.p
      ids |= set(t.id)

    self.sba_add_frame(frame)
    # Only allow frames in the posechain if there exists an observation for that frame
    self.posechain = [ (f,fp) for (f,fp) in self.posechain if f.id in ids ]

    (fix_sz, free_sz, niter) = self.sba
    if len(self.posechain) <= (1 + free_sz):
      fix_sz = 1
      free_sz = len(self.posechain) - 1
    fixed = self.posechain[-(fix_sz + free_sz):-(free_sz)]
    free = self.posechain[-(free_sz):]
    externals = sum([ f.externals for (f,_) in (fixed + free)], [])
    print "SBA:", "fixed", [f.id for (f,_) in fixed], "free", [f.id for (f,_) in free]
    if externals != []:
      fixed += externals
      print "external SBA:", "fixed", [f.id for (f,_) in fixed], "free", [f.id for (f,_) in free]
    self.pe.sba([fp for (_,fp) in fixed], [fp for (_,fp) in free], [ t.sba_track for t in self.tracks ], niter)
    if 0:
      for (f,fp) in free:
        p = Pose()
        p.fromlist(fp.M)
        print f.id, p.xform(0,0,0)
    # copy these corrected poses back into the VO's key and current frames
    to_correct = [ self.keyframe, frame ]
    if self.prev_frame:
      to_correct.append(self.prev_frame)
    #print "Copying poses", [ fp.id for fp in self.posechain ], "to", [ f.id for f in to_correct ]
    for (_,fp) in self.posechain[-(fix_sz + free_sz):]:
      for dst in set(to_correct):
        if fp.id == dst.id:
          dst.pose.fromlist(fp.M)
          print "SBA corrected pose of frame", dst.id, "to", dst.pose.M
    self.timer['sba'].stop()

  def process_frame(self, frame):
    self.find_keypoints(frame)
    self.find_disparities(frame)
    self.collect_descriptors(frame)
    frame.id = self.num_frames

  def add_external_frame(self, frame, fext):
    self.find_keypoints(fext)
    self.find_disparities(fext)
    self.collect_descriptors(fext)
    fext.id = self.ext_frames
    self.ext_frames += 1
    frame.externals.append((fext, VO.frame_pose(fext.id, fext.pose.tolist())))

  # just set up the frame with descriptors, no VO processing
  def setup_frame(self, frame):
    self.find_keypoints(frame)
    self.find_disparities(frame)
    self.collect_descriptors(frame)
    frame.id = self.num_frames
    frame.ref_frame_id = None

  # return inliers from a match
  def check_inliers(self, frame1, frame2):
    self.pairs = self.temporal_match(frame1, frame2)
    solution = self.solve(frame1.kp, frame2.kp, self.pairs)
    self.inl = solution[0]

  def change_keyframe(self, newkey):
    print "Change keyframe from", self.keyframe.id, "to", newkey.id
    self.log_keyframes.append(newkey.id)
    self.log_keyposes.append(newkey.pose)
    oldkey = self.keyframe
    self.keyframe = newkey

    if self.sba:
      self.maintain_tracks(oldkey, newkey)
      self.sba_handle_frame(newkey)

  def handle_frame(self, frame):
    self.find_keypoints(frame)
    self.find_disparities(frame)
    self.collect_descriptors(frame)
    frame.id = self.num_frames
    return self.handle_frame_0(frame)

  def handle_frame_0(self, frame):
    if self.prev_frame:
      # If the key->current is good, use it
      # Otherwise, prev frame becomes the new key

      ref = self.keyframe
      self.pairs = self.temporal_match(ref, frame)
      #if frame.id == 202 and ref.id == 199: self.show_pairs(self.pairs, ref, frame)
      solution = self.solve(ref.kp, frame.kp, self.pairs)
      if solution and solution[0] > 5:
        (inl, rot, shift) = solution
        diff_pose = self.mkpose(rot, shift)
        if self.scavenge:
          solution = self.scavenger(diff_pose, ref, frame)
          if solution and solution[0] > 5:
            (inl, rot, shift) = solution
            diff_pose = self.mkpose(rot, shift)
          else:
            inl = 0
            diff_pose = Pose()
      else:
        inl = 0
        diff_pose = Pose()
      diff_pose.assert_sane()
      self.inl = inl
      self.outl = len(self.pairs) - inl
      frame.diff_pose = diff_pose
#      print "frame", frame.id, "key:", ref.id, "inliers:", inl, "angle_thresh", self.angle_thresh
      is_far = (self.inl < self.inlier_thresh) or Pose().further_than(diff_pose, self.position_thresh, self.angle_thresh)
      if (self.keyframe != self.prev_frame) and is_far: 
        self.change_keyframe(self.prev_frame)
        return self.handle_frame_0(frame)
      Tok = ref.pose
      Tkp = diff_pose
      Top = Tok * Tkp
      frame.pose = Top
      frame.inl = self.inl
      frame.ref_frame_id = ref.id
    else:
      frame.pose = Pose()
      self.keyframe = frame
      self.log_keyframes.append(self.keyframe.id)
      self.log_keyposes.append(self.keyframe.pose)
      frame.inl = 999
      if self.sba:
        self.sba_add_frame(frame)
      frame.ref_frame_id = None
    self.pose.assert_sane()

    self.pose = frame.pose
    self.prev_frame = frame

    self.num_frames  += 1
    self.tot_inliers += self.inl
    self.tot_matches += len(self.pairs)
    self.tot_points  += len(frame.kp2d)

    return self.pose

  def correct(self, corrmap, current_frame):
    p = corrmap(self.keyframe.id)
    if p:
      self.keyframe.pose = p
    print "current", current_frame.id, "prev", self.prev_frame.id, "key", self.keyframe.id
    for f in [ current_frame, self.prev_frame ]:
      if f and f.ref_frame_id:
        p = corrmap(f.ref_frame_id)
        if p:
          Tok = p
          Tkp = f.diff_pose
          Top = Tok * Tkp
          print "** CORRECTED by", f.pose.d(Top), "**", "key", f.ref_frame_id, "of frame", f.id
          f.pose = Top

  def report_frame(self, frame):
    import md5
    print "*** FRAME", "id", frame.id, "key", frame.ref_frame_id, "***"
    print "use_grad_img", frame.use_grad_img
    print "limage md5:", " ".join([ "%02x" % ord(x) for x in md5.new(frame.lf.tostring()).digest()])
    print "rimage md5:", " ".join([ "%02x" % ord(x) for x in md5.new(frame.rf.tostring()).digest()])
    print "lgrad md5:", " ".join([ "%02x" % ord(x) for x in md5.new(frame.lgrad).digest()])
    print "rimage md5:", " ".join([ "%02x" % ord(x) for x in md5.new(frame.rgrad).digest()])
    print "kp2d length", len(frame.kp2d)
    print "  ", frame.kp2d[:7]
    print "kp length", len(frame.kp)
    for k in frame.kp:
      print "    ", k
    print
