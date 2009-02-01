#ifndef FEATURES_RTREE_CLASSIFIER_H
#define FEATURES_RTREE_CLASSIFIER_H

#include "calonder_descriptor/randomized_tree.h"
#include "calonder_descriptor/basic_math.h"

class RTTester;

namespace features {

class RTreeClassifier
{   
public:
  friend class ::RTTester;
  static const int DEFAULT_TREES = 48;
  static const size_t DEFAULT_NUM_QUANT_BITS = 4;  
    
  RTreeClassifier();

  void train(std::vector<BaseKeypoint> const& base_set, 
             Rng &rng,
             int num_trees = RTreeClassifier::DEFAULT_TREES,
             int depth = RandomizedTree::DEFAULT_DEPTH,
             int views = RandomizedTree::DEFAULT_VIEWS,
             size_t reduced_num_dim = RandomizedTree::DEFAULT_REDUCED_NUM_DIM,
             int num_quant_bits = DEFAULT_NUM_QUANT_BITS);
  void train(std::vector<BaseKeypoint> const& base_set,
             Rng &rng, 
             PatchGenerator &make_patch,
             int num_trees = RTreeClassifier::DEFAULT_TREES,
             int depth = RandomizedTree::DEFAULT_DEPTH,
             int views = RandomizedTree::DEFAULT_VIEWS,
             size_t reduced_num_dim = RandomizedTree::DEFAULT_REDUCED_NUM_DIM,
             int num_quant_bits = DEFAULT_NUM_QUANT_BITS);

  // Caller is responsible for calling free() on returned signature
  //float* getSignature(IplImage* patch);
  
  // sig must point to a memory block of at least classes()*sizeof(float|uint8_t) bytes
  void getSignature(IplImage *patch, uint8_t *sig);
  void getSignature(IplImage *patch, float *sig);
  void getSparseSignature(IplImage *patch, float *sig, float thresh);
  // TODO: deprecated in favor of getSignature overload, remove
  void getFloatSignature(IplImage *patch, float *sig) { getSignature(patch, sig); }

  static int countNonZeroElements(float *vec, int n, double tol=1e-10);
  static inline void safeSignatureAlloc(uint8_t **sig, int num_sig=1, int sig_len=176);
  static inline uint8_t* safeSignatureAlloc(int num_sig=1, int sig_len=176);  
    
  inline int classes() { return classes_; }
  inline int original_num_classes() { return original_num_classes_; }
  
  void setQuantization(int num_quant_bits);
  void discardFloatPosteriors();
  
  void read(const char* file_name);
  void read(std::istream &is);
  void write(const char* file_name) const;
  void write(std::ostream &os) const;

  // experimental and debug
  void saveAllFloatPosteriors(std::string file_url);
  void saveAllBytePosteriors(std::string file_url);
  void setFloatPosteriorsFromTextfile_176(std::string url);
  float countZeroElements();
   
  std::vector<RandomizedTree> trees_;

private:    
  int classes_;
  int num_quant_bits_;
  uint8_t **posteriors_;
  uint16_t *ptemp_;
  int original_num_classes_;  
  bool keep_floats_;
};

// Returns 16-byte aligned signatures that can be passed to getSignature().
// Release by calling free() - NOT delete!
//
// note: 1) for num_sig>1 all signatures will still be 16-byte aligned, as
//          long as sig_len%16 == 0 holds.
//       2) casting necessary, otherwise it breaks gcc's strict aliasing rules
inline void RTreeClassifier::safeSignatureAlloc(uint8_t **sig, int num_sig, int sig_len)
{
   assert(sig_len == 176);
   void *p_sig;
   posix_memalign(&p_sig, 16, num_sig*sig_len*sizeof(uint8_t));
   *sig = reinterpret_cast<uint8_t*>(p_sig);
}

inline uint8_t* RTreeClassifier::safeSignatureAlloc(int num_sig, int sig_len)
{
   uint8_t *sig;
   safeSignatureAlloc(&sig, num_sig, sig_len);
   return sig;
}


} // namespace features

#endif
