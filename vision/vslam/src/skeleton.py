import roslib
roslib.load_manifest('vslam')

import Image
from votools import TreeOptimizer3
import place_recognition
from visualodometer import VisualOdometer, Pose, DescriptorSchemeCalonder, DescriptorSchemeSAD, FeatureDetectorFast, FeatureDetector4x4, FeatureDetectorStar, FeatureDetectorHarris, from_xyz_euler
from stereo import SparseStereoFrame
from timer import Timer

import pylab, numpy
import random
import pickle
import math

class minimum_frame:
  def __init__(self, id, kp, descriptors, matcher):
    self.id = id
    self.kp = kp
    self.descriptors = descriptors
    self.matcher = matcher
    assert len(kp) == len(descriptors)

def mk_covar(xyz, rp, yaw):
  return (1.0 / math.sqrt(xyz),1.0 / math.sqrt(xyz), 1.0 / math.sqrt(xyz), 1.0 / math.sqrt(rp), 1.0 / math.sqrt(rp), 1.0 / math.sqrt(yaw))
#weak = mk_covar(0.01, 0.0002, 0.002)
weak = mk_covar(5,3,3)
strong = mk_covar(0.0001, 0.000002, 0.00002)

class Skeleton:
  def __init__(self, cam):
    self.nodes = set()
    self.edges = set()
    self.pg = TreeOptimizer3()
    self.pg.initializeOnlineOptimization()
    if 1:
      self.vt = place_recognition.load("/u/mihelich/images/holidays/holidays.tree")
    else:
      self.vt = None
    self.place_ids = []
    self.vo = VisualOdometer(cam, feature_detector = FeatureDetectorFast(), descriptor_scheme = DescriptorSchemeCalonder())
    #self.vo = VisualOdometer(cam)
    self.node_kp = {}
    self.node_descriptors = {}
    self.node_matcher = {}
    self.termcrit = lambda count, delta: ((count > 10) or (delta < 1e-1))
    self.pr_maximum = 15    # How many out of PR's places to consider for GCC
    self.node_vdist = 15                # how many frame to wait to put in a skeleton node

    self.timer = {}
    for t in ['toro add', 'toro opt', 'place recognition', 'gcc', 'descriptors']:
      self.timer[t] = Timer()

  def add(self, this, inl = 999):
    print "skel add with inliers", inl
    if len(self.nodes) == 0:
      self.nodes.add(this)
    elif not(this in self.nodes):
      byid = [ (f.id, f) for f in self.nodes ]
      prev = max(byid)[1]

      # Ignore the node if there are less than 15 frames since the previous node
      if (this.id - prev.id) < self.node_vdist:
        return

      if inl > 0:
        relpose = ~prev.pose * this.pose
        inf = strong
      else:
        relpose = Pose(numpy.identity(3), [ 5, 0, 0 ])
        inf = weak

      self.nodes.add(this)
      self.edges.add( (prev, this) )
      self.timer['toro add'].start()
      self.pg.addIncrementalEdge(prev.id, this.id, relpose.xform(0,0,0), relpose.euler(), inf)
      print "ADDED VO CONSTRAINT", prev.id, this.id, inf
      self.timer['toro add'].stop()
      #print "added node at", this.pose.xform(0,0,0), "in graph as", self.newpose(this.id).xform(0,0,0)

      self.memoize_node_kp_d(this)
      this_d = self.node_descriptors[this.id]
      if len(self.nodes) > 1:
        far = [ f for f in self.place_find(this.lf, this_d) if (not f.id in [this.id, prev.id])]
        self.add_links(this, far)

      self.place_ids.append(this)

  def addConstraint(self, prev, this, relpose):
    self.edges.add((prev, this))
    self.timer['toro add'].start()
    self.pg.addIncrementalEdge(prev.id, this.id, relpose.xform(0,0,0), relpose.euler(), strong)
    self.timer['toro add'].stop()

  def optimize(self):
    self.pg.initializeOnlineIterations()
    print "pg.error", self.pg.error()
    for j in range(5):
      for i in range(1000):
        #print "iter", i, "pg.error", self.pg.error()
        self.pg.iterate()
      print "pg.error", self.pg.error()
    #self.pg.recomputeAllTransformations()
    #self.pg.save("render5.graph")

  def newpose(self, id):
    xyz,euler = self.pg.vertex(id)
    return from_xyz_euler(xyz, euler)

  def place_find(self, lf, descriptors):
    if self.vt:
      self.timer['place recognition'].start()
      scores = self.vt.topN(lf, descriptors, len(self.place_ids), True)
      self.timer['place recognition'].stop()
      assert len(scores) == len(self.place_ids)+1
      return [id for (_,id) in sorted(zip(scores, self.place_ids))][:self.pr_maximum]
    else:
      return self.place_ids

  def add_links(self, this, far):
    self.timer['gcc'].start()
    coll = [ self.PE(this, f) + (f,) for f in far ]
    self.timer['gcc'].stop()
    id0 = this
    for inl,obs,id1 in coll:
      if 100 <= inl:
        old_error = self.pg.error()
        self.addConstraint(id0, id1, obs)
        print "ADDED CONSTRAINT", id0.id, id1.id, "error changed from", old_error, "to", self.pg.error()
          
    return
    t0 = self.timer['toro opt'].sum
    self.timer['toro opt'].start()
    self.pg.initializeOnlineIterations()
    count = 0
    self.pg.iterate()                   # error can go way up on first iterate
    prev_e = self.pg.error()
    self.pg.iterate()
    print
    print "Starting OPT loop, error ", self.pg.error(), " prev error ", prev_e
    while not self.termcrit(count, prev_e - self.pg.error()):
      prev_e = self.pg.error()
      self.pg.iterate()
      count += 1
    self.timer['toro opt'].stop()
    t1 = self.timer['toro opt'].sum
    td = t1 - t0
    if (td > 0.300):                    # too large, stretch frame additions
      self.node_vdist = 15 + (td - 0.4)*100
    else:
      self.node_vdist = 15
    print "OPT TIMER ", 1000.0*(t1-t0), "  ITERATIONS ", count, "  FRAMES ", self.node_vdist, "  ERROR ", self.pg.error()
    print

  def my_frame(self, id):
    return minimum_frame(id, self.node_kp[id], self.node_descriptors[id], self.node_matcher[id])

  def memoize_node_kp_d(self, af):
    self.timer['descriptors'].start()
    if not (af.id in self.node_kp):
      nf = SparseStereoFrame(af.lf, af.rf)
      self.vo.setup_frame(nf)
      self.node_kp[af.id] = nf.kp
      self.node_descriptors[af.id] = nf.descriptors
      self.node_matcher[af.id] = nf.matcher

      if 0:
        pylab.imshow(numpy.fromstring(af.lf.tostring(), numpy.uint8).reshape(480,640), cmap=pylab.cm.gray)
        pylab.scatter([x for (x,y,d) in nf.kp], [y for (x,y,d) in nf.kp], label = 'f0 kp', c = 'red')
        pylab.show()
        Image.fromstring("L", (640,480), af.lf.tostring()).save("star.png")
    self.timer['descriptors'].stop()

  def PE(self, af0, af1):
    if False and af0.id == 1241:
      (f0,f1) = (self.my_frame(af0.id), self.my_frame(af1.id))
      pairs = self.vo.temporal_match(f0, f1)
      for (a,b) in pairs:
        pylab.plot([ f0.kp[a][0], f1.kp[b][0] ], [ f0.kp[a][1], f1.kp[b][1] ])
      pylab.imshow(numpy.fromstring(af0.lf.tostring(), numpy.uint8).reshape(480,640), cmap=pylab.cm.gray)
      pylab.scatter([x for (x,y,d) in f0.kp], [y for (x,y,d) in f0.kp], label = '%d kp' % f0.id, c = 'red')
      pylab.scatter([x for (x,y,d) in f1.kp], [y for (x,y,d) in f1.kp], label = '%d kp' % f1.id, c = 'green')
      pylab.legend()
      pylab.show()
    return self.vo.proximity(self.my_frame(af0.id), self.my_frame(af1.id))

  def correct_frame_pose(self, id):
    if id in [ f.id for f in self.nodes ]:
      return self.newpose(id)
    else:
      return None

  def drawable(self):

    pts = dict([ (f,self.newpose(f.id).xform(0,0,0)) for f in self.nodes ])
    nodepts = pts.values()
    edges = []
    for (f0,f1) in self.edges:
      p0 = pts[f0]
      p1 = pts[f1]
      edges.append((p0, p1))
    return (nodepts, edges)

  def plot(self, color, annotate = False):
    pts = dict([ (f,self.newpose(f.id).xform(0,0,0)) for f in self.nodes ])
    nodepts = pts.values()
    pylab.scatter([x for (x,y,z) in nodepts], [z for (x,y,z) in nodepts], c = color, label = 'after SGD')
    if annotate:
      for (f,(x,y,z)) in pts.items():
        pylab.annotate('%d' % f.id, (float(x), float(z)))

    for (f0,f1) in self.edges:
      p0 = pts[f0]
      p1 = pts[f1]
      pylab.plot((p0[0], p1[0]), (p0[2], p1[2]), c = color)

  def summary(self):
    pts = dict([ (f.id,self.newpose(f.id).xform(0,0,0)) for f in self.nodes ])
    return (pts, [ (f0.id, f1.id) for (f0,f1) in self.edges ])

  def average_time_per_frame(self):
    niter = len(self.nodes)
    return 1e3 * sum([t.sum for t in self.timer.values()]) / niter

  def summarize_timers(self):
    print "Graph has", len(self.nodes), "nodes and", len(self.edges), "edges"
    print
    niter = len(self.nodes)
    if niter != 0:
      for n,t in self.timer.items():
        print "%-20s %fms" % (n, 1e3 * t.sum / niter)
      print "%-20s %fms" % ("TOTAL", self.average_time_per_frame())
    #self.vo.summarize_timers()

  def dump_timers(self, filename):
    f = open(filename, 'w')
    d = dict([ (nm, tm.log) for (nm, tm) in self.timer.items() ])
    pickle.dump(d, f)
    f.close()
