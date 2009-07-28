#ifndef __D3D_SPIN_IMAGE_H__
#define __D3D_SPIN_IMAGE_H__
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <opencv/cv.h>
#include <opencv/cxcore.h>
#include <opencv/cvaux.hpp>

#include <point_cloud_mapping/geometry/nearest.h>

#include <descriptors_3d/spectral_analysis.h>

using namespace std;

// --------------------------------------------------------------
//* SpinImage
/*!
 * \brief A SpinImage descriptor implements the feature described in
 *        [1] Johnson and Hebert, "Using Spin-Images for Efficient Object
 *        Recognition in Cluttered 3-D Scenes", PAMI 1999.
 */
// --------------------------------------------------------------
class SpinImage: public SpectralAnalysis
{
  public:
    // --------------------------------------------------------------
    /*!
     * \brief Type representing the spinning axis (Beta axis as described in [1])
     */
    // --------------------------------------------------------------
    typedef enum spin_axis
    {
      UNDEFINED = 0, NORMAL = 1, TANGENT = 2, CUSTOM = 3
    } spin_axis_type_t;

    // --------------------------------------------------------------
    /*!
     * \brief Instantiates the SpinImage descriptor
     *
     * The feature vector format is: [row_1 ... row_n]
     */
    // --------------------------------------------------------------
    SpinImage() :
      spin_axis_type_(SpinImage::UNDEFINED), use_interest_region_only_(false), dimensions_defined_(false),
          row_res_(-1.0), col_res_(-1.0), nbr_rows_(0), nbr_cols_(0)
    {
      result_size_ = 0;
    }

    // ===================================================================
    /*! \name Required settings  */
    // ===================================================================
    //@{
    // --------------------------------------------------------------
    /*!
     * \brief Sets the dimensions of the spin image
     *
     * Example spin image definition with 3 rows and 4 cols, using notation
     * described in [1]: \n
     *   beta                 \n
     *    ^                   \n
     *    |_ _ _ _            \n
     *    |_|_|_|_|           \n
     *    x_|_|_|_|           \n
     *    |_|_|_|_|           \n
     *    -----------> alpha  \n
     * (x = center point of spin image)
     *
     * \param row_res The cell resolution along the beta axis
     * \param col_res The cell resolution along the alpha axis
     * \param nbr_rows The number of cells along the beta axis
     * \param nbr_cols The number of cells along the alpha axis
     *
     * \warning nbr_rows must be odd
     *
     * \return 0 on success, otherwise negative value on error
     */
    // --------------------------------------------------------------
    int setImageDimensions(double row_res, double col_res, unsigned int nbr_rows, unsigned int nbr_cols);
    //@}

    // ===================================================================
    /*! \name Optional settings */
    // ===================================================================
    //@{
    // --------------------------------------------------------------
    /*!
     * \brief Sets the spin axis (Beta axis as defined in [1]) to be the
     *        locally extracted normal
     *
     * \warning setSpectralRadius() or useSpectralInformation() must also be called before compute()
     */
    // --------------------------------------------------------------
    inline void setAxisNormal()
    {
      spin_axis_type_ = SpinImage::NORMAL;
    }

    // --------------------------------------------------------------
    /*!
     * \brief Sets the spin axis (Beta axis as defined in [1]) to be the
     *        locally extracted tangent
     *
     * \warning setSpectralRadius() or useSpectralInformation() must also be called before compute()
     */
    // --------------------------------------------------------------
    inline void setAxisTangent()
    {
      spin_axis_type_ = SpinImage::TANGENT;
    }

    // --------------------------------------------------------------
    /*!
     * \brief Sets the spin axis (Beta axis as defined in [1]) to be an
     *        arbitrary vector.  (E.g. the vertical [0,0,1])
     *
     * \param ref_x The x-coordinate of the spinning axis
     * \param ref_y The y-coordinate of the spinning axis
     * \param ref_z The z-coordinate of the spinning axis
     */
    // --------------------------------------------------------------
    void setAxisCustom(double ref_x, double ref_y, double ref_z);

    // --------------------------------------------------------------
    /*!
     * \brief Indicates to compute the spin image with only points contained
     *        within given interest regions
     *
     * \warning This method is only relevant when calling compute() with interest regions
     */
    // --------------------------------------------------------------
    inline void useInterestRegionOnly()
    {
      use_interest_region_only_ = true;
    }
    //@}

    // --------------------------------------------------------------
    /*!
     * \brief Computes the spin image descriptor, centered at each interest point
     *
     * \warning setImageDimensions and setAxisNormal/setAxisTangent/setAxisCustom must be called first
     *
     * \see Descriptor3D::compute
     */
    // --------------------------------------------------------------
    virtual void compute(const sensor_msgs::PointCloud& data,
                         cloud_kdtree::KdTree& data_kdtree,
                         const cv::Vector<const geometry_msgs::Point32*>& interest_pts,
                         cv::Vector<cv::Vector<float> >& results);

    // --------------------------------------------------------------
    /*!
     * \brief Computes the spin image descriptor, centered at the centroid
     *        of each interest region
     *
     * \warning setImageDimensions and setAxisNormal/setAxisTangent/setAxisCustom must be called first
     *
     * \see Descriptor3D::compute
     */
    // --------------------------------------------------------------
    virtual void compute(const sensor_msgs::PointCloud& data,
                         cloud_kdtree::KdTree& data_kdtree,
                         const cv::Vector<const vector<int>*>& interest_region_indices,
                         cv::Vector<cv::Vector<float> >& results);

  private:
    // --------------------------------------------------------------
    /*!
     * \brief Computes a spin image of the neighboring points around center_pt
     *
     * \param data The overall point cloud
     * \param neighbor_indices The indices in data to compute the spin image with
     * \param center_pt The center point of the spin image
     * \param spin_image The resulting spin image feature
     */
    // --------------------------------------------------------------
    void computeSpinImage(const sensor_msgs::PointCloud& data,
                          const vector<int>& neighbor_indices,
                          const Eigen::Vector3d& center_pt,
                          cv::Vector<float>& spin_image);

    /*! \brief The axis that the spin image revolves around */
    Eigen::Vector3d spin_axis_;
    /*! \brief The type of the spinning axis */
    spin_axis_type_t spin_axis_type_;

    /*! \brief Flag if useInterestRegionOnly() has been called */
    bool use_interest_region_only_;

    /*! \brief Flag if setImageDimensions() has been called */
    bool dimensions_defined_;
    /*! \brief The cell resolution along the beta axis */
    double row_res_;
    /*! \brief The cell resolution along the alpha axis */
    double col_res_;
    /*! \brief The number of cells along the beta axis */
    unsigned int nbr_rows_;
    /*! \brief The number of cells along the alpha axis */
    unsigned int nbr_cols_;
};

#endif
