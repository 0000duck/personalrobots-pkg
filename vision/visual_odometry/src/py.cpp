#include <Python.h>

#include <iostream>
#include <vector>
#include <queue>

#include <boost/foreach.hpp>

#include <opencv/cxcore.h>
#include <opencv/cv.h>
#include <opencv/cvwimage.h>
#include <opencv/highgui.h>
// the google wrapper
#include <opencv/cvwimage.h>

#include <opencv/cxcore.hpp>

// WG Ext of OpenCV
#include <CvPoseEstErrMeasDisp.h>
#include "PoseEstimateDisp.h"
#include "PoseEstimate.h"
#include "CvMatUtils.h"
#include "CvMat3X3.h"
#include "CvTestTimer.h"
#include <PathRecon.h>
#include <VOSparseBundleAdj.h>

#include <VisOdom.h>

using namespace cv::willow;

using namespace cv;
using namespace std;

#define JDC_DEBUG 0

/************************************************************************/

//
// FramePose
//

typedef struct {
  PyObject_HEAD
  FramePose *fp;
} frame_pose_t;

static void
frame_pose_dealloc(PyObject *self)
{
  FramePose *fp = ((frame_pose_t*)self)->fp;
  delete fp;
  PyObject_Del(self);
}

/* Method table */
static PyMethodDef frame_pose_methods[] = {
  {NULL, NULL},
};

static PyObject *
frame_pose_GetAttr(PyObject *self, char *attrname)
{
    if (strcmp(attrname, "id") == 0) {
      FramePose *fp = ((frame_pose_t*)self)->fp;
      return PyInt_FromLong(fp->mIndex);
    } else if (strcmp(attrname, "M") == 0) {
      FramePose *fp = ((frame_pose_t*)self)->fp;
      PyObject *r = PyList_New(16);
      for (int i = 0; i < 16; i++)
        PyList_SetItem(r, i, PyFloat_FromDouble(fp->transf_local_to_global_data_[i]));
      return r;
    } else {
      return Py_FindMethod(frame_pose_methods, self, attrname);
    }
}

static PyTypeObject frame_pose_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "frame_pose",
    sizeof(frame_pose_t),
    0,
    (destructor)frame_pose_dealloc,
    0,
    (getattrfunc)frame_pose_GetAttr,
    0,
    0,
    0, // repr
    0,
    0,
    0,

    0,
    0,
    0,
    0,
    0,

    0,

    Py_TPFLAGS_CHECKTYPES,

    0,
    0,
    0,
    0

    /* the rest are NULLs */
};

PyObject *frame_pose(PyObject *self, PyObject *args)
{
  frame_pose_t *object = PyObject_NEW(frame_pose_t, &frame_pose_Type);

  int i;
  double M[16];
  if (!PyArg_ParseTuple(args, "i(dddddddddddddddd)", &i,
        &M[0],
        &M[1],
        &M[2],
        &M[3],
        &M[4],
        &M[5],
        &M[6],
        &M[7],
        &M[8],
        &M[9],
        &M[10],
        &M[11],
        &M[12],
        &M[13],
        &M[14],
        &M[15])) return NULL;
  CvMat m = cvMat(4, 4, CV_64FC1, M);
  object->fp = new FramePose(i);
  cvCopy(&m, &object->fp->transf_local_to_global_);

  return (PyObject*)object;
}


#if 0
/************************************************************************/

//
// PointTrack
//

typedef struct {
  PyObject_HEAD
  PointTrack *pt;
} point_track_t;

static void
point_track_dealloc(PyObject *self)
{
  PointTrack *pt = ((point_track_t*)self)->pt;
  delete pt;
  PyObject_Del(self);
}

PyObject *extend(PyObject *self, PyObject *args)
{
  PointTrack *pt = ((point_track_t*)self)->pt;
  int f0;
  CvPoint3D64f o0;
  if (!PyArg_ParseTuple(args, "i(ddd)", &f0, &o0.x, &o0.y, &o0.z)) return NULL;
  PointTrackObserv *pto0 = new PointTrackObserv(f0, o0, 0);
  pt->extend(pto0);
  Py_RETURN_NONE;
}

PyObject *set3d(PyObject *self, PyObject *args)
{
  PointTrack *pt = ((point_track_t*)self)->pt;
  CvPoint3D64f p0;
  if (!PyArg_ParseTuple(args, "ddd", &p0.x, &p0.y, &p0.z)) return NULL;
  cvCopy(&p0, &pt->coordinates_);
  Py_RETURN_NONE;
}

/* Method table */
static PyMethodDef point_track_methods[] = {
  { "extend", extend, METH_VARARGS },
  { "set3d", set3d, METH_VARARGS },
  { NULL, NULL },
};

static PyObject *
point_track_GetAttr(PyObject *self, char *attrname)
{
    return Py_FindMethod(point_track_methods, self, attrname);
}

static PyTypeObject point_track_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "point_track",
    sizeof(point_track_t),
    0,
    (destructor)point_track_dealloc,
    0,
    (getattrfunc)point_track_GetAttr,
    0,
    0,
    0, // repr
    0,
    0,
    0,

    0,
    0,
    0,
    0,
    0,

    0,

    Py_TPFLAGS_CHECKTYPES,

    0,
    0,
    0,
    0

    /* the rest are NULLs */
};

PyObject *point_track(PyObject *self, PyObject *args)
{
  point_track_t *object = PyObject_NEW(point_track_t, &point_track_Type);

  int f0, f1, trackid;
  CvPoint3D64f o0, o1, p;
  if (!PyArg_ParseTuple(args, "i(ddd)i(ddd)(ddd)i", &f0, &o0.x, &o0.y, &o0.z, &f1, &o1.x, &o1.y, &o1.z, &p.x, &p.y, &p.z, &trackid)) return NULL;
  PointTrackObserv *pto0 = new PointTrackObserv(f0, o0, 0);
  PointTrackObserv *pto1 = new PointTrackObserv(f1, o1, 0);
  object->pt = new PointTrack(pto0, pto1, p, trackid);

  return (PyObject*)object;
}
#endif

/************************************************************************/

#if 0
//
// Pose Estimator
//

typedef struct {
  PyObject_HEAD
  PoseEstimateStereo *pe;
#if JDC_DEBUG==1 // jdc
  LevMarqSparseBundleAdj *sba_;
  SBAVisualizer          *vis_;
  boost::unordered_map<int, FramePose*>* map_index_to_FramePose_;
#endif
} pose_estimator_t;

static void
pose_estimator_dealloc(PyObject *self)
{
  PoseEstimateStereo *pe = ((pose_estimator_t*)self)->pe;
  delete pe;
#if JDC_DEBUG==1
  pose_estimator_t* obj = (pose_estimator_t*)self;
  delete obj->sba_;
  delete obj->vis_;
  delete obj->map_index_to_FramePose_;
#endif
  PyObject_Del(self);
}

PyObject *estimate(PyObject *self, PyObject *args)
{
  PoseEstimateStereo *pe = ((pose_estimator_t*)self)->pe;
  vector <Keypoint> kpa;
  vector <Keypoint> kpb;
  vector <pair<int, int> > pairs;
  int polishing = 1;

  PyObject *p_kpa, *p_kpb, *p_pairs;
  if (!PyArg_ParseTuple(args, "OOO|i", &p_kpa, &p_kpb, &p_pairs, &polishing)) return NULL;

  double x, y, z;
  for (int i = 0; i < PyList_Size(p_kpa); i++) {
    PyArg_ParseTuple(PyList_GetItem(p_kpa, i), "ddd", &x, &y, &z);
    kpa.push_back(Keypoint(x, y, z, 0., 0., NULL));
  }
  for (int i = 0; i < PyList_Size(p_kpb); i++) {
    PyArg_ParseTuple(PyList_GetItem(p_kpb, i), "ddd", &x, &y, &z);
    kpb.push_back(Keypoint(x, y, z, 0., 0., NULL));
  }
  for (int i = 0; i < PyList_Size(p_pairs); i++) {
    int a, b;
    PyArg_ParseTuple(PyList_GetItem(p_pairs, i), "ii", &a, &b);
    pairs.push_back(make_pair(a, b));
  }

  double _mRot[9];
  double _mShift[3];
  CvMat rot   = cvMat(3, 3, CV_64FC1, _mRot);
  CvMat shift = cvMat(3, 1, CV_64FC1, _mShift);

  int inliers = pe->estimate(kpa, kpb, pairs, rot, shift, polishing);

  return Py_BuildValue("(i,(ddddddddd),(ddd))",
                       inliers,
                       _mRot[0], _mRot[1], _mRot[2],_mRot[3], _mRot[4], _mRot[5],_mRot[6], _mRot[7], _mRot[8],
                       _mShift[0], _mShift[1], _mShift[2]);
}

PyObject *inliers(PyObject *self, PyObject *args)
{
  PoseEstimateStereo *pe = ((pose_estimator_t*)self)->pe;

  CvMat *inliers0 = NULL, *inliers1 = NULL;
  pe->getInliers(inliers0, inliers1);

  PyObject *r;
  if (inliers0 != NULL && inliers1 != NULL) {
    r = PyList_New(inliers0->rows);
    for (int i = 0; i < inliers0->rows; i++) {
      PyList_SetItem(r, i, Py_BuildValue("((ddd),(ddd))",
          cvGetReal2D(inliers0, i, 0),
          cvGetReal2D(inliers0, i, 1),
          cvGetReal2D(inliers0, i, 2),
          cvGetReal2D(inliers1, i, 0),
          cvGetReal2D(inliers1, i, 1),
          cvGetReal2D(inliers1, i, 2)
          ));
    }
  } else {
    r = PyList_New(0);
  }

  return r;
}

PyObject *setInlierErrorThreshold(PyObject *self, PyObject *args)
{
  PoseEstimateStereo *pe = ((pose_estimator_t*)self)->pe;
  double thresh;
  if (!PyArg_ParseTuple(args, "d", &thresh))
    return NULL;
  pe->setInlierErrorThreshold(thresh);
  Py_RETURN_NONE;
}

PyObject *setNumRansacIterations(PyObject *self, PyObject *args)
{
  PoseEstimateStereo *pe = ((pose_estimator_t*)self)->pe;
  int numIterations;
  if (!PyArg_ParseTuple(args, "i", &numIterations))
    return NULL;

  pe->setNumRansacIterations(numIterations);
  Py_RETURN_NONE;
}

static vector<FramePose*> fpl_p2c(PyObject *o)
{
  vector<FramePose*> r;
  for (Py_ssize_t i = 0; i < PyList_Size(o); i++) {
    PyObject *oi = PyList_GetItem(o, i);
    r.push_back(((frame_pose_t*)oi)->fp);
  }
  return r;
}

PyObject *sba(PyObject *self, PyObject *args)
{
  PyObject *ofixed, *ofree, *otracks;
  int max_num_iters = 5;
  if (!PyArg_ParseTuple(args, "OOOi", &ofixed, &ofree, &otracks, &max_num_iters)) return NULL;

#if JDC_DEBUG==1  // commented out by jdc
#else
  PoseEstimateStereo *pe = ((pose_estimator_t*)self)->pe;
  CvMat cartToDisp;
  CvMat dispToCart;
  pe->getProjectionMatrices(&cartToDisp, &dispToCart);

  int full_free_window_size  = PyList_Size(ofree);
  int full_fixed_window_size = PyList_Size(ofixed);

  double epsilon = DBL_EPSILON;
  CvTermCriteria term_criteria = cvTermCriteria(CV_TERMCRIT_EPS+CV_TERMCRIT_ITER,max_num_iters,epsilon);
  LevMarqSparseBundleAdj sba(&dispToCart, &cartToDisp, full_fixed_window_size, full_free_window_size, term_criteria);

#endif

  vector<FramePose*> fixed_frames = fpl_p2c(ofixed);
  vector<FramePose*> free_frames = fpl_p2c(ofree);
  PointTracks tracks;
  tracks.current_frame_index_ = 0;
  tracks.oldest_frame_index_in_tracks_ = 0;
  for (Py_ssize_t i = 0; i < PyList_Size(otracks); i++) {
    PyObject *oi = PyList_GetItem(otracks, i);
    if (!PyObject_IsInstance(oi, (PyObject*)&point_track_Type)) {
      PyErr_SetString(PyExc_TypeError, "expecting list of point_track");
      return NULL;
    }
    PointTrack *pt = ((point_track_t*)oi)->pt;
    tracks.tracks_.push_back(pt);
  }

#if JDC_DEBUG==1 // jdc
  LevMarqSparseBundleAdj* sba = ((pose_estimator_t*)self)->sba_;
  LevMarqSparseBundleAdj::ErrorCode err_code = sba->optimize(&fixed_frames, &free_frames, &tracks);
  if (err_code == LevMarqSparseBundleAdj::InputError) {
    cout << "Input error: either fixed win or free win is empty" << endl;
  }
  cout << "initial cost: " << sba->initial_cost_ << " final cost: " << sba->accepted_cost_ << endl;
  cout << "# good updates: " << sba->num_good_updates_ << " # retractions: " << sba->num_retractions_ << endl;

  if (free_frames.size()>0 && fixed_frames.size()>0) {
    SBAVisualizer* vis = ((pose_estimator_t*)self)->vis_;
    // set frame poses, point tracks and maps from index to frame poses
    vector<FramePose* > frame_poses;
    printf("fixed frames [");
    BOOST_FOREACH(FramePose *fp, fixed_frames) {
      frame_poses.push_back(fp);
      vis->map_index_to_FramePose_->insert(make_pair(fp->mIndex, fp));;
      printf("%d ", fp->mIndex);
    }
    printf("]\n");
    printf("free frames [");
    BOOST_FOREACH(FramePose *fp, free_frames) {
      frame_poses.push_back(fp);
      vis->map_index_to_FramePose_->insert(make_pair(fp->mIndex,fp));
      printf("%d ", fp->mIndex);
    }
    printf("]\n");
    vis->framePoses = &frame_poses;
    vis->tracks = &tracks;
    int current_frame_index = free_frames.back()->mIndex;

    IplImage* im = cvLoadImage("/tmp/mkplot-left.png");
    if (im) {
      vis->canvasTracking.SetIpl(im);
    } else {
      // make sure the image buffers is allocated to the right sizes
      vis->canvasTracking.Allocate(640, 480);
      // clear the image
      cvSet(vis->canvasTracking.Ipl(), cvScalar(0,0,0));
    }
    { // annotation on the canvas
      char info[256];
      CvPoint org = cvPoint(0, 475);
      CvFont font;
      cvInitFont( &font, CV_FONT_HERSHEY_SIMPLEX, .5, .4);
      sprintf(info, "%04d, nTrcks=%d",
          current_frame_index, tracks.tracks_.size());

      cvPutText(vis->canvasTracking.Ipl(), info, org, &font, CvMatUtils::yellow);
    }
    cout << "All The Tracks"<<endl;
    tracks.print();
    sprintf(vis->poseEstFilename,  "%s/poseEst-%04d.png", vis->outputDirname.c_str(),
        current_frame_index);
    vis->recordFrameIds(&fixed_frames, &free_frames);
    vis->drawTrackTrajectories(current_frame_index);
    vis->show();
    vis->save();
    vis->reset();
  }
#else
  LevMarqSparseBundleAdj::ErrorCode err_code = sba.optimize(&fixed_frames, &free_frames, &tracks);
  if (err_code == LevMarqSparseBundleAdj::InputError) {
    cout << "Input error: either fixed win or free win is empty" << endl;
  }
  cout << "initial cost: " << sba.initial_cost_ << " final cost: " << sba.accepted_cost_ << endl;
  cout << "# good updates: " << sba.num_good_updates_ << " # retractions: " << sba.num_retractions_ << endl;
#endif

  Py_RETURN_NONE;
}

/* Method table */
static PyMethodDef pose_estimator_methods[] = {
  {"estimate", estimate, METH_VARARGS},
  {"inliers", inliers, METH_VARARGS},
  {"sba", sba, METH_VARARGS},
  {"setInlierErrorThreshold", setInlierErrorThreshold, METH_VARARGS},
  {"setNumRansacIterations",  setNumRansacIterations,  METH_VARARGS},
  {NULL, NULL},
};

static PyObject *
pose_estimator_GetAttr(PyObject *self, char *attrname)
{
    return Py_FindMethod(pose_estimator_methods, self, attrname);
}

static PyTypeObject pose_estimator_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "pose_estimator",
    sizeof(pose_estimator_t),
    0,
    (destructor)pose_estimator_dealloc,
    0,
    (getattrfunc)pose_estimator_GetAttr,
    0,
    0,
    0, // repr
    0,
    0,
    0,

    0,
    0,
    0,
    0,
    0,

    0,

    Py_TPFLAGS_CHECKTYPES,

    0,
    0,
    0,
    0

    /* the rest are NULLs */
};

PyObject *pose_estimator(PyObject *self, PyObject *args)
{
#if 0
  pose_estimator_t *object = PyObject_NEW(pose_estimator_t, &pose_estimator_Type);
  object->pe = new PoseEstimateStereo();

  double Fx, Fy, Tx, Clx, Crx, Cy;
  if (!PyArg_ParseTuple(args, "dddddd", &Fx, &Fy, &Tx, &Clx, &Crx, &Cy)) return NULL;
  object->pe->setCameraParams(Fx, Fy, Tx, Clx, Crx, Cy);
  object->pe->setInlierErrorThreshold(3.0);

#if JDC_DEBUG==1 // jdc
  CvMat cartToDisp;
  CvMat dispToCart;
  object->pe->getProjectionMatrices(&cartToDisp, &dispToCart);

  object->pe->setNumRansacIterations(100);

  // set the window size large enough to accommodate. Although not efficient.
  int full_free_window_size  = 300;
  int full_fixed_window_size = 300;
  int max_num_iters = 50;

  double epsilon = DBL_EPSILON;
  CvTermCriteria term_criteria = cvTermCriteria(CV_TERMCRIT_EPS+CV_TERMCRIT_ITER,max_num_iters,epsilon);
  object->sba_ = new LevMarqSparseBundleAdj(&dispToCart, &cartToDisp, full_fixed_window_size, full_free_window_size, term_criteria);

  object->vis_ = new SBAVisualizer((PoseEstimateDisp&)*object->pe, NULL, NULL, NULL);
  object->map_index_to_FramePose_ =
    new boost::unordered_map<int, FramePose*>();
  // note that SBAVisualizer by design does not own map_index_to_FramePose
  object->vis_->map_index_to_FramePose_ = object->map_index_to_FramePose_;
  object->vis_->outputDirname = string("/tmp/deleteme");
#endif

  return (PyObject*)object;
#endif
}
#endif

/************************************************************************/

bool estimateLeastSquareInCol(double *_P0, double *_P1, double *_R, double *_T)
{
  CvMat P0  = cvMat(3, 3, CV_64F, _P0);
  CvMat P1  = cvMat(3, 3, CV_64F, _P1);
  CvMat R = cvMat(3, 3, CV_64FC1, _R);
  CvMat T = cvMat(3, 1, CV_64FC1, _T);
  bool status = true;
  double _Q[9], _W[9], _Ut[9], _Vt[9], _C0[3], _C1[3];
  CvMat Q  = cvMat(3, 3, CV_64F, _Q); // Q = P1 * transpose(P0)
  CvMat W  = cvMat(3, 3, CV_64F, _W); // Eigen matrix of Q. Q = U * W * transpose(V)
  CvMat Ut = cvMat(3, 3, CV_64F, _Ut); // transpose(U)
  CvMat Vt = cvMat(3, 3, CV_64F, _Vt); // transpose(V)
  CvMat C0 = cvMat(3, 1, CV_64F, _C0); // centroid of the 3 points in P0
  CvMat C1 = cvMat(3, 1, CV_64F, _C1); // centroid of the 3 points in P1
  // compute the centroids of these 3 ponints for the two positions
  //cvCVAPI(void)  cvReduce( const CvArr* src, CvArr* dst, int dim CV_DEFAULT(-1),
  //               int op CV_DEFAULT(CV_REDUCE_SUM) );
  cvReduce(&P0, &C0, -1, CV_REDUCE_AVG);
  // compute the relative vectors of the two groups of points w.r.t. their centroids.
  double _Temp[3*P0.cols];
  CvMat Temp = cvMat(3, P0.cols, CV_64F, _Temp);
  cvRepeat(&C0, &Temp);
  cvSub(&P0, &Temp, &P0);

  cvReduce(&P1, &C1, -1, CV_REDUCE_AVG);
  cvRepeat(&C1, &Temp);
  cvSub(&P1, &Temp, &P1);

  // Q = P1 * P0^T
  cvGEMM(&P1, &P0, 1, NULL, 0, &Q, CV_GEMM_B_T);

  // do a SVD on Q. Q = U*W*transpose(V)
  // according to the documentation, specifying the flags speeds up the processing
  cvSVD(&Q, &W, &Ut, &Vt, CV_SVD_MODIFY_A|CV_SVD_U_T|CV_SVD_V_T);

  // R = U * S * V^T
  double _S[] = {
                  1.,0.,0.,
                  0.,1.,0.,
                  0.,0.,1.
  };
  CvMat S;
  cvInitMatHeader(&S, 3, 3, CV_64FC1, _S);
  if (cvDet(&Ut)*cvDet(&Vt)<0) {
    // Ut*V is not a rotation matrix, although either of them are unitary
    // we shall set the last diag entry of S to be -1
    // so that Ut*S*V is a rotational matrix
    // setting S[2,2] = -1
    _S[8] = -1.;
    double _UxS[9];
    CvMat UxS;
    cvInitMatHeader(&UxS, 3, 3, CV_64FC1, _UxS);
    cvGEMM(&Ut, &S, 1, NULL, 0, &UxS, CV_GEMM_A_T);
    cvGEMM(&UxS, &Vt, 1, NULL, 0, &R, 0);
  } else {
    cvGEMM(&Ut, &Vt, 1, NULL, 0, &R, CV_GEMM_A_T);
  }

  // t = centroid1 - R*centroid0
  cvGEMM(&R, &C0, -1, &C1, 1, &T, 0);
  return status;
}

#include <Eigen/Core>
#include <Eigen/Array>
#include <Eigen/SVD>

static double det(Eigen::Matrix3f m)
{
  double
    a = m(0,0),
    b = m(0,1),
    c = m(0,2),
    d = m(1,0),
    e = m(1,1),
    f = m(1,2),
    g = m(2,0),
    h = m(2,1),
    i = m(2,2);
  return a*e*i - a*f*h - b*d*i + b*f*g + c*d*h - c*e*g;
}

bool estimateLeastSquareInColE(double *_P0, double *_P1, double *_R, double *_T)
{
  Eigen::Matrix3f P0, P1;
  P0 << _P0[0], _P0[1], _P0[2], _P0[3], _P0[4], _P0[5], _P0[6], _P0[7], _P0[8];
  P1 << _P1[0], _P1[1], _P1[2], _P1[3], _P1[4], _P1[5], _P1[6], _P1[7], _P1[8];

  Eigen::MatrixXf C0 = P0.rowwise().sum() / 3;
  Eigen::Matrix3f C03;
  C03.col(0) = C0;
  C03.col(1) = C0;
  C03.col(2) = C0;
  P0 -= C03;

  Eigen::MatrixXf C1 = P1.rowwise().sum() / 3;
  Eigen::Matrix3f C13;
  C13.col(0) = C1;
  C13.col(1) = C1;
  C13.col(2) = C1;
  P1 -= C13;

  // Q = P1 * P0^T
  Eigen::Matrix3f Q = P1 * P0.transpose();

  Eigen::SVD <Eigen::Matrix3f> svd(Q);

  // R = U * S * V^T
  // Eigen::MatrixXf R = svd.matrixU() * (svd.singularValues() * svd.matrixV().transpose());

  Eigen::Matrix3f S = Eigen::Matrix3f::Identity();
  S(2,2) = -1;
  Eigen::Matrix3f R = svd.matrixU() * S * svd.matrixV().transpose();

  // t = centroid1 - R*centroid0
  Eigen::MatrixXf T = C1 - R * C0;

  _R[0] = R(0,0);
  _R[1] = R(0,1);
  _R[2] = R(0,2);
  _R[3] = R(1,0);
  _R[4] = R(1,1);
  _R[5] = R(1,2);
  _R[6] = R(2,0);
  _R[7] = R(2,1);
  _R[8] = R(2,2);

  _T[0] = T(0);
  _T[1] = T(1);
  _T[2] = T(2);

  return 1;
}

static void unpackL(double *dst, PyObject *src)
{
  PyObject *sf = PySequence_Fast(src, "not iterable");
  Py_ssize_t i = 0;
  for (i = 0; i < PySequence_Fast_GET_SIZE(sf); i++) {
    dst[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(sf, i));
  }
  Py_DECREF(sf);
}

PyObject *SVD(PyObject *self, PyObject *args)
{
  PyObject *pP0, *pP1;
  if (!PyArg_ParseTuple(args, "OO", &pP0, &pP1))
    return NULL;

  double _P0[9], _P1[9];

  unpackL(_P0, pP0);
  unpackL(_P1, pP1);

  double _R[9], _T[3];
  estimateLeastSquareInCol(_P0, _P1, _R, _T);
  return Py_BuildValue("(ddddddddd)(ddd)(dddddddddddd)",
    _R[0],_R[1],_R[2],_R[3],_R[4],_R[5],_R[6],_R[7],_R[8],
    _T[0],_T[1],_T[2],
    _R[0],_R[1],_R[2],_T[0], _R[3],_R[4],_R[5],_T[1], _R[6],_R[7],_R[8],_T[2]
    );
}

PyObject *SVDe(PyObject *self, PyObject *args)
{
  PyObject *pP0, *pP1;
  if (!PyArg_ParseTuple(args, "OO", &pP0, &pP1))
    return NULL;

  double _P0[9], _P1[9];

  unpackL(_P0, pP0);
  unpackL(_P1, pP1);

  double _R[9], _T[3];
  estimateLeastSquareInColE(_P0, _P1, _R, _T);
  return Py_BuildValue("(ddddddddd)(ddd)(dddddddddddd)",
    _R[0],_R[1],_R[2],_R[3],_R[4],_R[5],_R[6],_R[7],_R[8],
    _T[0],_T[1],_T[2],
    _R[0],_R[1],_R[2],_T[0], _R[3],_R[4],_R[5],_T[1], _R[6],_R[7],_R[8],_T[2]
    );
}


static bool doLevMarq(
    const CvMat& uvds0Inlier, const CvMat& uvds1Inlier,
    const CvMat& CartToDisp,  const CvMat& DispToCart,
    CvMat& rot, CvMat& shift) {

  // nonlinear optimization by Levenberg-Marquardt
  int numInliers = uvds0Inlier.rows;
  cv::willow::LevMarqTransformDispSpace levMarq(&DispToCart, &CartToDisp, numInliers);

  double param[6];

  //initialize the parameters
  levMarq.rotAndShiftMatsToParams(rot, shift, param);

  levMarq.optimize(&uvds0Inlier, &uvds1Inlier, param);

  levMarq.paramsToRotAndShiftMats(param, rot, shift);
  return true;
}

PyObject *polish(PyObject *self, PyObject *args)
{
  PyObject *pUvds0Inlier, *pUvds1Inlier, *pCartToDisp, *pDispToCart, *pR, *pS;
  if (!PyArg_ParseTuple(args, "OOOOOO", &pUvds0Inlier, &pUvds1Inlier, &pCartToDisp, &pDispToCart, &pR, &pS))
    return NULL;

  double _R[9], _T[3];
  unpackL(_R, pR);
  CvMat R = cvMat(3, 3, CV_64FC1, _R);
  unpackL(_T, pS);
  CvMat T = cvMat(3, 1, CV_64FC1, _T);
  double _pCartToDisp[16];
  unpackL(_pCartToDisp, pCartToDisp);
  CvMat CartToDisp = cvMat(4, 4, CV_64FC1, _pCartToDisp);
  double _pDispToCart[16];
  unpackL(_pDispToCart, pDispToCart);
  CvMat DispToCart = cvMat(4, 4, CV_64FC1, _pDispToCart);

  Py_ssize_t i;
  PyObject *sf;

  sf = PySequence_Fast(pUvds0Inlier, "uvds0Inlier");
  double _uvds0Inlier[3 * PySequence_Fast_GET_SIZE(sf)];
  for (i = 0; i < PySequence_Fast_GET_SIZE(sf); i++) {
    unpackL(_uvds0Inlier + 3 * i, PySequence_Fast_GET_ITEM(sf, i));
  }
  CvMat uvds0Inlier = cvMat(PySequence_Fast_GET_SIZE(sf), 3, CV_64FC1, _uvds0Inlier);
  Py_DECREF(sf);

  sf = PySequence_Fast(pUvds1Inlier, "uvds1Inlier");
  double _uvds1Inlier[3 * PySequence_Fast_GET_SIZE(sf)];
  for (i = 0; i < PySequence_Fast_GET_SIZE(sf); i++) {
    unpackL(_uvds1Inlier + 3 * i, PySequence_Fast_GET_ITEM(sf, i));
  }
  CvMat uvds1Inlier = cvMat(PySequence_Fast_GET_SIZE(sf), 3, CV_64FC1, _uvds1Inlier);
  Py_DECREF(sf);

  doLevMarq(uvds0Inlier, uvds1Inlier, CartToDisp, DispToCart, R, T);

  return Py_BuildValue("(ddddddddd)(ddd)",
    _R[0],_R[1],_R[2],_R[3],_R[4],_R[5],_R[6],_R[7],_R[8],
    _T[0],_T[1],_T[2]);
}

/************************************************************************/

static PyMethodDef methods[] = {
  // {"point_track", point_track, METH_VARARGS},
  {"frame_pose", frame_pose, METH_VARARGS},
  // {"imWindow", mkimWindow, METH_VARARGS},
  {"SVD", ::SVD, METH_VARARGS},
  {"SVDe", ::SVDe, METH_VARARGS},
  {"polish", polish, METH_VARARGS},

  // {"pose_estimator", pose_estimator, METH_VARARGS},
  {NULL, NULL, NULL},
};

extern "C" void init_visual_odometry_lowlevel()
{
    PyObject *m, *d;

    m = Py_InitModule("_visual_odometry_lowlevel", methods);
    d = PyModule_GetDict(m);
}
