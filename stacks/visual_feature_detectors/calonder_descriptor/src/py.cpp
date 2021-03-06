#include "Python.h"

#include "calonder_descriptor/matcher.h"
#include "calonder_descriptor/rtree_classifier.h"
#include "calonder_descriptor/patch_generator.h"

#include <boost/foreach.hpp>
#include <highgui.h>
#include <cvwimage.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>

using namespace features;

//typedef float SigType;
typedef uint8_t SigType;
typedef Promote<SigType>::type DistanceType;

typedef struct {
  PyObject_HEAD
  BruteForceMatcher <SigType, int> *c;
  std::vector<PyObject*> *sigs;
} wrapped_BruteForceMatcher_t;

static void
wrapped_BruteForceMatcher_dealloc(PyObject *self)
{
  wrapped_BruteForceMatcher_t *pc = (wrapped_BruteForceMatcher_t*)self;
  delete pc->c;
  BOOST_FOREACH(PyObject *sig, *pc->sigs)
    Py_DECREF(sig);
  delete pc->sigs;
  PyObject_Del(self);
}

PyObject *wrapped_BruteForceMatcher_addSignature(PyObject *self, PyObject *args);
PyObject *wrapped_BruteForceMatcher_findMatch(PyObject *self, PyObject *args);

/* Method table */
static PyMethodDef wrapped_BruteForceMatcher_methods[] = {
{"addSignature", wrapped_BruteForceMatcher_addSignature, METH_VARARGS},
{"findMatch", wrapped_BruteForceMatcher_findMatch, METH_VARARGS},
{NULL, NULL}
};

static PyObject *
wrapped_BruteForceMatcher_GetAttr(PyObject *self, char *attrname)
{
  return Py_FindMethod(wrapped_BruteForceMatcher_methods, self, attrname);
}

static PyTypeObject wrapped_BruteForceMatcher_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "BruteForceMatcher",
    sizeof(wrapped_BruteForceMatcher_t),
    0,
    (destructor)wrapped_BruteForceMatcher_dealloc,
    0,
    (getattrfunc)wrapped_BruteForceMatcher_GetAttr,
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

PyObject *make_wrapped_BruteForceMatcher(PyObject *self, PyObject *args)
{
  wrapped_BruteForceMatcher_t *object = PyObject_NEW(wrapped_BruteForceMatcher_t, &wrapped_BruteForceMatcher_Type);
  int dimension;
  if (!PyArg_ParseTuple(args, "i", &dimension))
    return NULL;
  object->c = new BruteForceMatcher <SigType, int>(dimension);
  object->sigs = new std::vector<PyObject*>;
  return (PyObject*)object;
}

typedef struct {
  PyObject_HEAD
  SigType *data;
  int size;
} signature_t;

static void
signature_dealloc(PyObject *self)
{
  signature_t *ps = (signature_t*)self;
  free(ps->data);
  PyObject_Del(self);
}

PyObject *signature_reduce(PyObject *self, PyObject *args)
{
  signature_t* ps = (signature_t*)self;
  return Py_BuildValue("(O(s#))", self->ob_type, (char*)ps->data, (int)(ps->size * sizeof(SigType)));
}

static int signature_init(PyObject *self, PyObject *args, PyObject *kwds)
{
  signature_t *object = (signature_t*)self;
  char *s;
  int len;

  if (!PyArg_ParseTuple(args, "s#", &s, &len))
    return -1;
  object->size = len;
  posix_memalign((void**)&object->data, 16, object->size * sizeof(SigType));
  memcpy(object->data, s, object->size * sizeof(SigType));
  return 0;
}

static char doc_signature_MODULE[] = "doc for signature\n";

static struct PyMethodDef signature_methods[] =
{
  {"__reduce__", (PyCFunction)signature_reduce, 0, NULL},
  {NULL,          NULL}
};

static PyObject *signature_repr(PyObject *self)
{
  signature_t* ps = (signature_t*)self;
  char str[100 + 2 * ps->size];
  sprintf(str, "<signature(%d ", ps->size);
  char *d = str + strlen(str);
  size_t i;
  for (i = 0; i < (size_t)ps->size; i++) {
    sprintf(d, "%02x", ps->data[i]);
    d += 2;
  }
  sprintf(d, ")>");
  return PyString_FromString(str);
}

// Sequence protocol for signature
Py_ssize_t signature_length(PyObject *self)
{
  signature_t *object = (signature_t*)self;
  return object->size;
}

PyObject* signature_item(PyObject *self, Py_ssize_t i)
{
  signature_t *object = (signature_t*)self;
  if (i >= object->size)
    return NULL;
  return PyInt_FromLong(object->data[i]);
}

static PySequenceMethods signature_SequenceMethods =
{
  signature_length,   /*length*/
  NULL,               /*concat*/
  NULL,               /*repeat*/
  signature_item,     /*item*/
  NULL,               /*ass_item*/
  NULL,               /*contains*/
  NULL,               /*inplace_concat*/
  NULL                /*inplace_repeat*/
};

static PyTypeObject signature_Type = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,                                      /*size*/
  "calonder.signature",                   /*name*/
  sizeof(signature_t),                    /*basicsize*/
  0,                                      /*itemsize*/

  /* methods */
  (destructor)signature_dealloc,          /*dealloc*/
  (printfunc)NULL,                        /*print*/
  NULL,                                   /*getattr*/
  NULL,                                   /*setattr*/
  NULL,                                   /*compare*/
  NULL,                                   /*repr*/
  NULL,                                   /*as_number*/
  &signature_SequenceMethods,             /*as_sequence*/
  NULL,                                   /*as_mapping*/
  (hashfunc)NULL,                         /*hash*/
  (ternaryfunc)NULL,                      /*call*/
  (reprfunc)signature_repr,               /*str*/

  /* Space for future expansion */
  0L,0L,0L,

  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
  doc_signature_MODULE,                   /* Documentation string */
  0,                                      /* tp_traverse */
  0,                                      /* tp_clear */
  0,                                      /* tp_richcompare */
  0,                                      /* tp_weaklistoffset */
  0,                                      /* tp_iter */
  0,                                      /* tp_iternext */
  signature_methods,                      /* tp_methods */
  0,                                      /* tp_members */
  0,                                      /* tp_getset */
  0,                                      /* tp_base */
  0,                                      /* tp_dict */
  0,                                      /* tp_descr_get */
  0,                                      /* tp_descr_set */
  0,                                      /* tp_dictoffset */
  (initproc)signature_init,               /* tp_init */
  PyType_GenericAlloc,                    /* tp_alloc */
  0                                       /* tp_new */
};

typedef struct {
  PyObject_HEAD
  RTreeClassifier *classifier;
} classifier_t;

static void
classifier_dealloc(PyObject *self)
{
  classifier_t *pc = (classifier_t*)self;
  delete pc->classifier;
  PyObject_Del(self);
}

// TODO: keyword arguments for training parameters
//       allow training on multiple images
PyObject *train(PyObject *self, PyObject *args)
{
  classifier_t *pc = (classifier_t*)self;

  IplImage *input;
  PyObject *kp;
  int num_trees = RTreeClassifier::DEFAULT_TREES;
  int depth = RandomizedTree::DEFAULT_DEPTH;
  int views = RandomizedTree::DEFAULT_VIEWS;
  int dimension = RandomizedTree::DEFAULT_REDUCED_NUM_DIM;
  int num_quant_bits = 0;

  {
    char *imgdata;
    int imgdata_size, x, y;
    if (!PyArg_ParseTuple(args, "s#iiO|iiiii", &imgdata, &imgdata_size,
                          &x, &y, &kp, &num_trees, &depth, &views,
                          &dimension, &num_quant_bits))
      return NULL;
    dimension = std::min(dimension, (int)PyList_Size(kp));
    input = cvCreateImageHeader(cvSize(x, y), IPL_DEPTH_8U, 1);
    cvSetData(input, imgdata, x);
  }
  std::vector<BaseKeypoint> base_set;
  for (int i = 0; i < PyList_Size(kp); i++) {
    int x, y;
    PyArg_ParseTuple(PyList_GetItem(kp, i), "ii", &x, &y);
    base_set.push_back( BaseKeypoint(x+16, y+16, input) );
  }

  Rng rng( 0 );
  //Rng rng( std::time(0) );
  pc->classifier->train(base_set, rng, num_trees, depth,
                        views, dimension, num_quant_bits);

  Py_RETURN_NONE;
}

PyObject *Cwrite(PyObject *self, PyObject *args)
{
  classifier_t *pc = (classifier_t*)self;
  char *filename;
  if (!PyArg_ParseTuple(args, "s", &filename)) return NULL;
  pc->classifier->write(filename);
  Py_RETURN_NONE;
}

PyObject *Cread(PyObject *self, PyObject *args)
{
  classifier_t *pc = (classifier_t*)self;
  char *filename;
  if (!PyArg_ParseTuple(args, "s", &filename))
    return NULL;
  pc->classifier->read(filename);
  Py_RETURN_NONE;
}

PyObject *dimension(PyObject *self, PyObject *args)
{
  classifier_t *pc = (classifier_t*)self;
  return Py_BuildValue("i", pc->classifier->classes());
}

PyObject *getSignature(PyObject *self, PyObject *args)
{
  IplImage *input;
  {
    char *imgdata;
    int imgdata_size, x, y;
    if (!PyArg_ParseTuple(args, "s#ii", &imgdata, &imgdata_size, &x, &y))
      return NULL;
    input = cvCreateImageHeader(cvSize(x, y), IPL_DEPTH_8U, 1);
    cvSetData(input, imgdata, x);
  }
  classifier_t *pc = (classifier_t*)self;
  signature_t *object = PyObject_NEW(signature_t, &signature_Type);
  object->size = pc->classifier->classes();
  posix_memalign((void**)&object->data, 16, object->size * sizeof(SigType));
  pc->classifier->getSignature(input, object->data);
  cvReleaseImageHeader(&input);
  return (PyObject*)object;
}

static PyObject *im2arr(CvArr **dst, PyObject *src)
{
  int width, height;
  if (!PyObject_HasAttrString(src, "size") ||
      !PyObject_HasAttrString(src, "mode") ||
      !PyObject_HasAttrString(src, "tostring"))
    return NULL;

  {
    PyObject *ob_size = PyObject_GetAttrString(src, "size");
    if (!PyArg_ParseTuple(ob_size, "ii", &width, &height))
      return NULL;
    Py_DECREF(ob_size);
  }

  int depth, nchannels, bps;
  {
    PyObject *ob_mode = PyObject_GetAttrString(src, "mode");
    char *mode = PyString_AsString(ob_mode);

    nchannels = (int)strlen(mode);
    if (strcmp(mode, "F") == 0) {
      depth = IPL_DEPTH_32F;
      bps = 4;
    } else {
      depth = IPL_DEPTH_8U;
      bps = 1;
    }

    Py_DECREF(ob_mode);
    mode = NULL;
  }

  PyObject *ob_tostring = PyObject_GetAttrString(src, "tostring");
  PyObject *ob_string = PyObject_CallObject(ob_tostring, NULL);
  Py_DECREF(ob_tostring);

  char *string;
  Py_ssize_t length;
  PyString_AsStringAndSize(ob_string, &string, &length);

  *dst = cvCreateImageHeader(cvSize(width, height), depth, nchannels);
  cvSetData(*dst, (void*)string, nchannels * bps * width);

  return ob_string;
}

PyObject *getSignatures(PyObject *self, PyObject *args)
{
  classifier_t *pc = (classifier_t*)self;

  PyObject *coords;

  char *imgdata;
  int imgdata_size, w, h;
  if (!PyArg_ParseTuple(args, "(ii)s#O", &w, &h, &imgdata, &imgdata_size, &coords))
    return NULL;

  IplImage *cva = cvCreateImageHeader(cvSize(w, h), IPL_DEPTH_8U, 1);
  cvSetData(cva, imgdata, w);
  IplImage *input = cvCreateImage(cvSize(32, 32), IPL_DEPTH_8U, 1);

  PyObject *fi = PySequence_Fast(coords, "coords");
  if (fi == NULL)
    return NULL;
  PyObject *r = PyList_New(PySequence_Fast_GET_SIZE(fi));
  for (Py_ssize_t i = 0; i < PySequence_Fast_GET_SIZE(fi); i++) {
    PyObject *t = PySequence_Fast_GET_ITEM(fi, i);
    if (!PyTuple_Check(t)) {
      PyErr_SetString(PyExc_TypeError, "Expected tuple");
      return NULL;
    }
    int x, y;
    x = PyInt_AS_LONG(PyTuple_GET_ITEM(t, 0));
    y = PyInt_AS_LONG(PyTuple_GET_ITEM(t, 1));

    cvSetImageROI(cva, cvRect(x - 16, y - 16, 32, 32));
    cvCopy(cva, input);

    signature_t *object = PyObject_NEW(signature_t, &signature_Type);
    object->size = pc->classifier->classes();
    posix_memalign((void**)&object->data, 16, object->size * sizeof(SigType));
    pc->classifier->getSignature(input, object->data);
    PyList_SET_ITEM(r, i, (PyObject*)object);
  }
  Py_DECREF(fi);
  cvReleaseImage(&input);
  cvReleaseImageHeader(&cva);
  return r;
}

/* Method table */
static PyMethodDef classifier_methods[] = {
  {"train", train, METH_VARARGS},
  {"write", Cwrite, METH_VARARGS},
  {"read", Cread, METH_VARARGS},
  {"dimension", dimension, METH_VARARGS},
  {"getSignature", getSignature, METH_VARARGS},
  {"getSignatures", getSignatures, METH_VARARGS},
  {NULL, NULL},
};

static PyObject *
classifier_GetAttr(PyObject *self, char *attrname)
{
    return Py_FindMethod(classifier_methods, self, attrname);
}

PyObject *wrapped_BruteForceMatcher_addSignature(PyObject *self, PyObject *args)
{
  wrapped_BruteForceMatcher_t *pm = (wrapped_BruteForceMatcher_t*)self;

  PyObject *sig;
  if (!PyArg_ParseTuple(args, "O", &sig))
    return NULL;

  signature_t *ps = (signature_t*)sig;
  pm->c->addSignature(ps->data, 0);
  // claim reference to ensure sig isn't deleted prematurely!
  Py_INCREF(sig);
  pm->sigs->push_back(sig);
  Py_RETURN_NONE;
}

PyObject *wrapped_BruteForceMatcher_findMatch(PyObject *self, PyObject *args)
{
  wrapped_BruteForceMatcher_t *pm = (wrapped_BruteForceMatcher_t*)self;

  PyObject *sig;
  int predicates_size;
  char *predicates = NULL;

  if (!PyArg_ParseTuple(args, "O|s#", &sig, &predicates, &predicates_size))
    return NULL;
  signature_t *ps = (signature_t*)sig;

  DistanceType distance;
  int index;
  if (predicates == NULL)
    index = pm->c->findMatch(ps->data, &distance);
  else {
    index = pm->c->findMatchPredicated(ps->data, predicates, &distance);
  }

  if (index == -1)
    Py_RETURN_NONE;
  else
    return Py_BuildValue("id", index, distance);
}

static PyTypeObject classifier_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "classifier",
    sizeof(classifier_t),
    0,
    (destructor)classifier_dealloc,
    0,
    (getattrfunc)classifier_GetAttr,
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

PyObject *classifier(PyObject *self, PyObject *args)
{
    classifier_t *object = PyObject_NEW(classifier_t, &classifier_Type);
    //object->classifier = new RTreeClassifier(true);
    object->classifier = new RTreeClassifier();
    return (PyObject*)object;
}

static PyMethodDef methods[] = {
  {"classifier", classifier, METH_VARARGS},
  {"BruteForceMatcher", make_wrapped_BruteForceMatcher, METH_VARARGS},
  {NULL, NULL},
};

extern "C" void initcalonder()
{
  PyObject *m, *d;

  signature_Type.tp_new = PyType_GenericNew;
  if (PyType_Ready(&signature_Type) < 0)
    return;
  m = Py_InitModule("calonder", methods);
  PyModule_AddObject(m, "signature", (PyObject *)&signature_Type);
  d = PyModule_GetDict(m);
}
