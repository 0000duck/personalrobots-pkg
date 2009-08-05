/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Daniel Munoz
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

#include <functional_m3n/m3n_model.h>

// --------------------------------------------------------------
/*! See function definition */
// --------------------------------------------------------------
int M3NModel::train(const vector<const RandomField*>& training_rfs, const M3NParams& m3n_params)
{
  M3NLogger logger;
  time_t start_timer, end_timer;

  // -------------------------------------------
  // Extract the labels to train on
  set<unsigned int> invalid_training_indices;
  if (extractVerifyLabelsFeatures(training_rfs, invalid_training_indices) < 0)
  {
    return -1;
  }

  // -------------------------------------------
  // All feature information and labels now defined.
  // Now define the dimension information
  // (This must be called after extractVerifyLabelsFeatures())
  initStackedFeatureIndices();

  // -------------------------------------------
  // Extract training parameters
  // INVARIANT: parametersDefined() ensures parameters are valid
  if (m3n_params.parametersDefined() == false)
  {
    ROS_ERROR("Training parameters are not all defined");
    return -1;
  }
  double learning_rate = m3n_params.getLearningRate();
  unsigned int nbr_iterations = m3n_params.getNumberOfIterations();
  const vector<float>& robust_potts_params = m3n_params.getRobustPottsParams();
  if (robust_potts_params.size() != clique_set_feature_dims_.size())
  {
    ROS_ERROR("The robust potts params do not match the number of clique sets");
    return -1;
  }

  // -------------------------------------------
  // Ensure the Robust Potts truncation parameters are the same if doing online learning
  if (trained_)
  {
    for (unsigned int i = 0 ; i < robust_potts_params.size() ; i++)
    {
      if (fabs(robust_potts_params[i] - robust_potts_params_[i]) > 1e-5)
      {
        ROS_ERROR("Robust Potts truncation parameters are different");
        return -1;
      }
    }
  }
  robust_potts_params_ = robust_potts_params;

  // -------------------------------------------
  // Learn with loss-augmented inference to act as a margin
  // (currently using hamming loss only)
  loss_augmented_inference_ = true;

  // -------------------------------------------
  // Train for the specified number of iterations
  for (unsigned int t = 0 ; t < nbr_iterations ; t++)
  {
    ROS_INFO("-------- Starting iteration %u --------", t);
    // ---------------------------------------------------
    // Instantiate new regressor
    RegressorWrapper* curr_regressor = instantiateRegressor(m3n_params);
    if (curr_regressor == NULL)
    {
      ROS_ERROR("Could not create new regressor at iteration %u", t);
      return -1;
    }

    // ---------------------------------------------------
    // Iterate over each RandomField
    double iteration_inference_time = 0.0;
    for (unsigned int i = 0 ; i < training_rfs.size() ; i++)
    {
      if (invalid_training_indices.count(i) != 0)
      {
        continue;
      }

      const RandomField* curr_rf = training_rfs[i];
      const map<unsigned int, RandomField::Node*>& nodes = curr_rf->getNodesRandomFieldIDs();
      const vector<map<unsigned int, RandomField::Clique*> >& clique_sets = curr_rf->getCliqueSets();

      // ---------------------------------------------------
      // Perform inference with the current model.
      // If first iteration, generate a random labeling,
      // otherwise, perform inference with the current model.
      map<unsigned int, unsigned int> curr_inferred_labeling;
      time(&start_timer);
      if (t == 0)
      {
        // Generate random labeling
        unsigned int nbr_labels = training_labels_.size();
        unsigned int random_label = 0;
        for (map<unsigned int, RandomField::Node*>::const_iterator iter_nodes = nodes.begin() ; iter_nodes
            != nodes.end() ; iter_nodes++)
        {
          random_label = training_labels_[rand() % nbr_labels];
          curr_inferred_labeling[iter_nodes->first] = random_label;
        }
      }
      else
      {
        inferPrivate(*curr_rf, curr_inferred_labeling);
      }
      time(&end_timer);
      iteration_inference_time += difftime(end_timer, start_timer);
      logger.printClassificationRates(nodes, curr_inferred_labeling, training_labels_);

      // ---------------------------------------------------
      // Create training set for the new regressor from node and clique features.
      // When classification is wrong, do +1/-1 with features with ground truth/inferred label

      // ------------------------
      // Node features
      for (map<unsigned int, RandomField::Node*>::const_iterator iter_nodes = nodes.begin() ; iter_nodes
          != nodes.end() ; iter_nodes++)
      {
        unsigned int curr_node_id = iter_nodes->first;
        unsigned int curr_node_gt_label = iter_nodes->second->getLabel();
        unsigned int curr_node_infer_label = curr_inferred_labeling[curr_node_id];
        if (curr_node_gt_label != curr_node_infer_label)
        {
          // +1 features with ground truth label
          if (curr_regressor->addTrainingSample(iter_nodes->second->getFeatureVals(), node_feature_dim_,
              node_stacked_feature_start_idx_[curr_node_gt_label], 1.0) < 0)
          {
            abort();
          }

          // -1 features with wrong inferred label
          if (curr_regressor->addTrainingSample(iter_nodes->second->getFeatureVals(), node_feature_dim_,
              node_stacked_feature_start_idx_[curr_node_infer_label], -1.0) < 0)
          {
            abort();
          }
        }
      }

      // ------------------------
      // Iterate over clique sets to get clique features
      for (unsigned int clique_set_idx = 0 ; clique_set_idx < clique_sets.size() ; clique_set_idx++)
      {
        // ------------------------
        // Iterate over cliques
        const map<unsigned int, RandomField::Clique*>& curr_cliques = clique_sets[clique_set_idx];
        for (map<unsigned int, RandomField::Clique*>::const_iterator iter_cliques = curr_cliques.begin() ; iter_cliques
            != curr_cliques.end() ; iter_cliques++)
        {
          unsigned int curr_clique_gt_mode1_label = 0;
          unsigned int curr_clique_gt_mode1_count = 0;
          unsigned int curr_clique_gt_mode2_label = 0; // unused
          unsigned int curr_clique_gt_mode2_count = 0; // unused
          if (iter_cliques->second->getModeLabels(curr_clique_gt_mode1_label, curr_clique_gt_mode1_count,
              curr_clique_gt_mode2_label, curr_clique_gt_mode2_count) < 0)
          {
            return -1;
          }

          unsigned int curr_clique_infer_mode1_label = 0;
          unsigned int curr_clique_infer_mode1_count = 0;
          unsigned int curr_clique_infer_mode2_label = 0; // unused
          unsigned int curr_clique_infer_mode2_count = 0; // unused
          if (iter_cliques->second->getModeLabels(curr_clique_infer_mode1_label,
              curr_clique_infer_mode1_count, curr_clique_infer_mode2_label, curr_clique_infer_mode2_count,
              NULL, &curr_inferred_labeling) < 0)
          {
            return -1;
          }

          // ------------------------
          // Compute functional gradient for current clique
          // If mode labels of ground truth and inferred labelings agree, then it will not affect it
          // Otherwise, need to determine if the labeling has non-zero potential based on interaction model
          // (all labels must agree if using Potts model), this is done in calcFuncGradResidual().
          // Only contribute if the residual is non-zero.
          if (curr_clique_gt_mode1_label != curr_clique_infer_mode1_label)
          {
            // ------------------------
            // Compute functional gradient residual from ground truth label (gt_residual)
            float gt_residual = calcFuncGradResidual(robust_potts_params_[clique_set_idx],
                iter_cliques->second->getOrder(), curr_clique_gt_mode1_count);

            if (gt_residual > 1e-5)
            {
              // +1 features with ground truth label
              if (curr_regressor->addTrainingSample(iter_cliques->second->getFeatureVals(),
                  clique_set_feature_dims_[clique_set_idx],
                  clique_set_stacked_feature_start_idx_[clique_set_idx][curr_clique_gt_mode1_label],
                  gt_residual) < 0)
              {
                abort();
              }
            }

            // ------------------------
            // Compute functional gradient residual from inferred label (gt_infer)
            float infer_residual = calcFuncGradResidual(robust_potts_params_[clique_set_idx],
                iter_cliques->second->getOrder(), curr_clique_infer_mode1_count);

            if (infer_residual > 1e-5)
            {
              // -1 features with wrong inferred label
              if (curr_regressor->addTrainingSample(iter_cliques->second->getFeatureVals(),
                  clique_set_feature_dims_[clique_set_idx],
                  clique_set_stacked_feature_start_idx_[clique_set_idx][curr_clique_infer_mode1_label],
                  -infer_residual) < 0)
              {
                abort();
              }
            }
          }
        } // end iterate over cliques
      } // end iterate over clique sets
    } // end iterate over random fields

    // ---------------------------------------------------
    // Train and augment regressor to the model
    time(&start_timer);
    if (curr_regressor->train() < 0)
    {
      // TODO: free previous regressors? set trained_ true? return 0?
      return -1;
    }
    const double curr_step_size = learning_rate / sqrt(static_cast<double> (t + 1));
    regressors_.push_back(pair<double, RegressorWrapper*> (curr_step_size, curr_regressor));
    time(&end_timer);
    double iteration_regressor_time = difftime(end_timer, start_timer);

    logger.addTimingInference(iteration_inference_time);
    logger.addTimingRegressors(iteration_regressor_time);
  } // end training iterations

  trained_ = true;
  return 0;
}

// --------------------------------------------------------------
/*!
 * See function definition
 * Invariant: truncation_params are valid
 */
// --------------------------------------------------------------
float M3NModel::calcFuncGradResidual(const float truncation_param,
                                     const unsigned int clique_order,
                                     const unsigned int nbr_mode_label)
{
  // If using Robust Potts, determine if allowed number of disagreeing nodes is allowable
  if (truncation_param > 0.0)
  {
    float float_nbr_mode_label = static_cast<float> (nbr_mode_label);
    float float_clique_order = static_cast<float> (clique_order);
    float Q = truncation_param * float_clique_order;
    float residual = ((float_nbr_mode_label - float_clique_order) / Q) + 1.0;
    if (residual > 0.0)
    {
      //ROS_INFO("nbr_model_label: %u,  order: %u,  Q: %f, ==> residual: %f", nbr_mode_label, clique_order, Q, residual);
      return residual;
    }
    else
    {
      return 0.0;
    }
  }
  // Otherwise, a truncation param of 0.0 indicates to use Potts model
  else
  {
    return static_cast<float> (clique_order == nbr_mode_label);
  }
}

// --------------------------------------------------------------
/*! See function definition */
// --------------------------------------------------------------
RegressorWrapper* M3NModel::instantiateRegressor(const M3NParams& m3n_params)
{
  RegressorWrapper* created_regressor = NULL;

  RegressorWrapper::algorithm_t regressor_algorithm = m3n_params.getRegressorAlgorithm();
  if (regressor_algorithm == RegressorWrapper::REGRESSION_TREE)
  {
    created_regressor = new RegressionTreeWrapper(total_stack_feature_dim_,
        m3n_params.getRegressionTreeParams());
  }

  return created_regressor;
}

// --------------------------------------------------------------
/*! See function definition */
// --------------------------------------------------------------
int M3NModel::extractVerifyLabelsFeatures(const vector<const RandomField*>& training_rfs,
                                          set<unsigned int>& invalid_training_indices)
{
  invalid_training_indices.clear();

  // ---------------------------------------------------
  // Extract and verify the feature dimensions and labels for each random field
  for (unsigned int i = 0 ; i < training_rfs.size() ; i++)
  {
    // ---------------------------------------------------
    // Check for NULL pointers to random fields
    if (training_rfs[i] == NULL)
    {
      ROS_ERROR("M3NModel::extractVerifyLabelsFeatures() passed NULL random field %u", i);
      return -1;
    }

    // ---------------------------------------------------
    // Flag indicating if its the first time encountering node/clique in random field/clique-set
    bool node_dimensions_defined = false;

    // ---------------------------------------------------
    // Nodes: extract/verify feature dimension and labels
    const map<unsigned int, RandomField::Node*>& nodes = (training_rfs[i])->getNodesRandomFieldIDs();
    for (map<unsigned int, RandomField::Node*>::const_iterator iter_nodes = nodes.begin() ; iter_nodes
        != nodes.end() ; iter_nodes++)
    {
      const unsigned int curr_label = iter_nodes->second->getLabel();
      const unsigned int curr_feature_dim = iter_nodes->second->getNumberFeatureVals();

      // --------------------------
      // Extract or verify the node feature dimension to be used by this model
      if (!trained_ && !node_dimensions_defined)
      {
        node_feature_dim_ = curr_feature_dim;
        node_dimensions_defined = true;
      }
      else if (curr_feature_dim != node_feature_dim_)
      {
        ROS_ERROR("Node features dimensions are NOT the same: %u and %u (node %u, random field %u)",
            node_feature_dim_, curr_feature_dim, iter_nodes->first, i);
        return -1;
      }

      // --------------------------
      // If model not already trained, extract label and feature dimension information
      if (!trained_)
      {
        // Ensure the random field is fully labeled
        if (curr_label == RandomField::UNKNOWN_LABEL)
        {
          ROS_ERROR("RandomField %u contains unlabeled node with id %u", i, iter_nodes->first);
          return -1;
        }
        else
        {
          // Add the label if not encountered it before
          if (find(training_labels_.begin(), training_labels_.end(), curr_label) == training_labels_.end())
          {
            training_labels_.push_back(curr_label);
          }
        }
      }
      // Otherwise, the model is already trained so verify it can handle all labels in the random field
      else
      {
        if (find(training_labels_.begin(), training_labels_.end(), curr_label) == training_labels_.end())
        {
          {
            ROS_ERROR("Trained model encountered new label %u in RandomField %u", curr_label, i);
            return -1;
          }
        }
      }
    }// end iteration over nodes

    // ---------------------------------------------------
    // Ensure the node feature dimensions are defined by the presence of one node
    if (node_feature_dim_ == 0)
    {
      ROS_WARN("Did NOT find any nodes from the RandomField %u  (%u)", i, nodes.size());
      invalid_training_indices.insert(i);
      continue;
    }

    // ---------------------------------------------------
    // Allocate for the number clique sets if never trained before,
    // Otherwise ensure the number of clique sets are consistent
    const vector<map<unsigned int, RandomField::Clique*> >& clique_sets = (training_rfs[i])->getCliqueSets();
    if (!trained_)
    {
      clique_set_feature_dims_.assign(clique_sets.size(), 0);
    }
    else if (trained_ && clique_set_feature_dims_.size() != clique_sets.size())
    {
      ROS_ERROR("Number of clique sets do NOT match from RandomField %u", i);
      return -1;
    }

    // ---------------------------------------------------
    // Cliques: extract/verify feature dimensions for each clique-set
    for (unsigned int j = 0 ; j < clique_sets.size() ; j++)
    {
      // Reset flag for encountering first clique in the clique-set
      bool curr_cs_dimensions_defined = false;

      // --------------------------
      // Iterate over the cliques in the current clique-set
      for (map<unsigned int, RandomField::Clique*>::const_iterator iter_cliques = clique_sets[j].begin() ; iter_cliques
          != clique_sets[j].end() ; iter_cliques++)
      {
        unsigned int curr_feature_dim = iter_cliques->second->getNumberFeatureVals();

        // --------------------------
        // If never trained before and is the first clique in the clique set, then record the feature dimensions
        if (!trained_ && !curr_cs_dimensions_defined)
        {
          clique_set_feature_dims_[j] = curr_feature_dim;
          curr_cs_dimensions_defined = true;
        }
        // Otherwise, verify the feature dimensions are consistent
        else if (curr_feature_dim != clique_set_feature_dims_[j])
        {
          ROS_ERROR("Clique features are NOT the same: %u and %u (clique %u, cs idx %u, random field %u)",
              clique_set_feature_dims_[j], curr_feature_dim, iter_cliques->first, j, i);
          return -1;
        }
      } // end iteration over cliques

      // --------------------------
      // Ensure the clique set's feature dimensions are defined
      if (clique_set_feature_dims_[j] == 0)
      {
        ROS_WARN("Did not find any cliques in clique-set %u from RandomField %u; skipping.", j, i);
        invalid_training_indices.insert(i);
        break;
      }
    } // end iteration over clique sets
  } // end iteration over random fields

  // Cannot train model with 1 label
  if (training_labels_.size() == 1)
  {
    training_labels_.clear();
    node_feature_dim_ = 0;
    clique_set_feature_dims_.assign(clique_set_feature_dims_.size(), 0);
    ROS_ERROR("Cannot train with data containing only 1 label");
    return -1;
  }

  // Verify found at least 1 valid random field
  if (invalid_training_indices.size() == training_rfs.size())
  {
    if (!trained_)
    {
      training_labels_.clear();
      node_feature_dim_ = 0;
      clique_set_feature_dims_.assign(clique_set_feature_dims_.size(), 0);
    }
    ROS_ERROR("Could not find any valid random fields to train on");
    return -1;
  }

  // ---------------------------------------------------
  // Store labels in ascending order
  sort(training_labels_.begin(), training_labels_.end());
  return 0;
}
