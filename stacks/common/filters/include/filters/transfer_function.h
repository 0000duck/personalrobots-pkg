/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// Original version: Melonee Wise <mwise@willowgarage.com>

#ifndef FILTERS_TRANSFER_FUNCTION_H_
#define FILTERS_TRANSFER_FUNCTION_H_

#include <stdint.h>
#include <math.h>
#include <vector>
#include <string>

#include <boost/scoped_ptr.hpp>

#include "filters/filter_base.h"
#include "filters/realtime_circular_buffer.h"

namespace filters
{
/***************************************************/
/*! \class TransferFunctionFilter
    \brief One-dimensional digital filter class.

    This class calculates the output for \f$N\f$ one-dimensional
    digital filters. Where the input, \f$x\f$, is a (\f$N\f$ x 1) vector
    of inputs and the output, \f$y\f$, is a (\f$N\f$ x 1) vector of outputs.
    The filter is described by vectors \f$a\f$ and \f$b\f$ and
    implemented using the standard difference equation:<br>

    \f{eqnarray*}
    a[0]*y[n] = b[0]*x[n] &+& b[1]*x[n-1]+ ... + b[n_b]*x[n-n_b]\\
                          &-& a[1]*y[n-1]- ... - a[n_a]*y[n-n_a]
     \f}<br>


    If \f$a[0]\f$ is not equal to 1, the coefficients are normalized by \f$a[0]\f$.

    Example xml config:<br>

    <filter type="TransferFunctionFilter" name="filter_name"><br>
        <params a="1.0 0.5" b="0.2 0.2"/><br>
    </filter><br>

*/
/***************************************************/
template <typename T>
class TransferFunctionFilter: public FilterBase <T>
{
public:
  /**
   * \brief Construct the filter
   */
  TransferFunctionFilter() ;

  /** \brief Destructor to clean up
   */
  ~TransferFunctionFilter();

  /** \brief Configure the filter with the correct number of channels and params.
   * \param number_of_channels The number of inputs filtered.
   * \param config The xml that is parsed to configure the filter.
   */
  virtual bool configure();

  /** \brief Update the filter and return the data seperately
   * \param data_in vector<T> with number_of_channels elements
   * \param data_out vector<T> with number_of_channels elements
   */
  virtual bool update(const T & data_in, T& data_out) ;
  virtual bool update(const std::vector<T> & data_in, std::vector<T>& data_out) ;



protected:

  boost::scoped_ptr<RealtimeCircularBuffer<std::vector<T> > > input_buffer_; //The input sample history.
  boost::scoped_ptr<RealtimeCircularBuffer<std::vector<T> > > output_buffer_; //The output sample history.

  std::vector<T>  temp; //used for storage and preallocation

  std::vector<double> a_;   //Transfer functon coefficients (output).
  std::vector<double> b_;   //Transfer functon coefficients (input).

};

template <typename T>
TransferFunctionFilter<T>::TransferFunctionFilter()
{
}

template <typename T>
TransferFunctionFilter<T>::~TransferFunctionFilter()
{
};

template <typename T>
bool TransferFunctionFilter<T>::configure()
{
  std::vector<double> temp_default;
  // Parse a and b into a std::vector<double>.
  if (!FilterBase<T>::getDoubleVectorParam("a", a_, temp_default))
  {
    ROS_ERROR("TransferFunctionFilter, \"%s\", params has no attribute a.", FilterBase<T>::getName().c_str());
    return false;
  }///\todo check length


  if (!FilterBase<T>::getDoubleVectorParam("b", b_, temp_default))
  {
    ROS_ERROR("TransferFunctionFilter, \"%s\", params has no attribute b.", FilterBase<T>::getName().c_str());
    return false;
  }///\todo check length

  // Create the input and output buffers of the correct size.
  temp.resize(this->number_of_channels_);
  input_buffer_.reset(new RealtimeCircularBuffer<std::vector<T> >(b_.size()-1, temp));
  output_buffer_.reset(new RealtimeCircularBuffer<std::vector<T> >(a_.size()-1, temp));

  // Prevent divide by zero while normalizing coeffs.
  if ( a_[0] == 0)
  {
    ROS_ERROR("a[0] can not equal 0.");
    return false;
  }

  // Normalize the coeffs by a[0].
  if(a_[0] != 1)
  {
    for(uint32_t i = 0; i < b_.size(); i++)
    {
      b_[i] = (b_[i] / a_[0]);
    }
    for(uint32_t i = 1; i < a_.size(); i++)
    {
      a_[i] = (a_[i] / a_[0]);
    }
    a_[0] = (a_[0] / a_[0]);
  }

  return true;
};


template <typename T>
bool TransferFunctionFilter<T>::update(const T & data_in, T& data_out)
{
  // Ensure the correct number of inputs
  if (this->number_of_channels_ != 1)
  {
    ROS_ERROR("Number of channels is %d, to use non vector constructor it must be 1", this->number_of_channels_);
    return false;
  }
  // Copy data to prevent mutation if in and out are the same ptr
  temp[0] = data_in;
  std::vector<T> temp_in(1);
  temp_in[0] = data_in;
  std::vector<T> temp_out(1);

  for (uint32_t i = 0; i < temp_in.size(); i++)
  {
    temp_out[i]=(b_[0]) * (temp_in[i]);

    for (uint32_t row = 1; row <= input_buffer_->size(); row++)
    {
      (temp_out)[i] += b_[row] * (*input_buffer_)[row-1][i];
    }
    for (uint32_t row = 1; row <= output_buffer_->size(); row++)
    {
      (temp_out)[i] -= a_[row] * (*output_buffer_)[row-1][i];
    }
  }
  input_buffer_->push_front(temp_in);
  output_buffer_->push_front(temp_out);

  data_out = temp_out[0];
  return true;
}

template <typename T>
bool TransferFunctionFilter<T>::update(const std::vector<T>  & data_in, std::vector<T> & data_out)
{

  // Ensure the correct number of inputs
  if (data_in.size() != this->number_of_channels_ || data_out.size() != this->number_of_channels_ )
  {
    ROS_ERROR("Number of channels is %d, but data_in.size() = %d and data_out.size() = %d.  They must match", this->number_of_channels_, data_in.size(), data_out.size());
    return false;
  }
  // Copy data to prevent mutation if in and out are the same ptr
  temp = data_in;

  for (uint32_t i = 0; i < temp.size(); i++)
  {
    data_out[i]=b_[0] * temp[i];

    for (uint32_t row = 1; row <= input_buffer_->size(); row++)
    {
      (data_out)[i] += b_[row] * (*input_buffer_)[row-1][i];
    }
    for (uint32_t row = 1; row <= output_buffer_->size(); row++)
    {
      (data_out)[i] -= a_[row] * (*output_buffer_)[row-1][i];
    }
  }
  input_buffer_->push_front(temp);
  output_buffer_->push_front(data_out);

  return true;
}

}

#endif //#ifndef FILTERS_TRANSFER_FUNCTION_H_
