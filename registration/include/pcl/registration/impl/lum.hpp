/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2011, Willow Garage, Inc.
 *
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
 *   * Neither the name of Willow Garage, Inc. nor the names of its
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
 *
 * $Id$
 *
 */

#ifndef PCL_REGISTRATION_IMPL_LUM_H_
#define PCL_REGISTRATION_IMPL_LUM_H_

#include <list>
#include <algorithm>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>

#include <Eigen/Geometry>

#include <pcl/common/transforms.h>
#include <pcl/pcl_base.h>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  inline typename pcl::registration::LUM<PointT>::Vertex
  pcl::registration::LUM<PointT>::addPointCloud (PointCloudPtr cloud)
  {
    Vertex v = add_vertex (*slam_graph_);
    (*slam_graph_)[v].cloud_ = cloud;
    (*slam_graph_)[v].pose_ = Vector6f::Zero ();
    return v;
  }

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  inline typename pcl::registration::LUM<PointT>::Vertex
  pcl::registration::LUM<PointT>::addPointCloud (PointCloudPtr cloud, Vector6f pose)
  {
    Vertex v = add_vertex (*slam_graph_);
    (*slam_graph_)[v].cloud_ = cloud;
    if (v == 0)
    {
      PCL_WARN("[pcl::registration::LUM::addPointCloud] The pose estimate is ignored for the first cloud in the graph since that will become the reference pose.\n");
      (*slam_graph_)[v].pose_ = Vector6f::Zero ();
      return 0;
    }
    (*slam_graph_)[v].pose_ = pose;
    return v;
  }

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  inline void
  pcl::registration::LUM<PointT>::setPose (Vertex vertex, Vector6f pose)
  {
    if (vertex == 0)
    {
      PCL_WARN("[pcl::registration::LUM::setPose] The pose estimate is ignored for the first cloud in the graph since that will become the reference pose.\n");
      return;
    }
    if (vertex < 0 || vertex >= num_vertices (*slam_graph_))
    {
      PCL_WARN("[pcl::registration::LUM::setPose] You are attempting to set a pose estimate to a non-existing graph vertex.\n");
      return;
    }
    (*slam_graph_)[vertex].pose_ = pose;
    typename SLAMGraph::out_edge_iterator out_edge, out_edge_end;
    for (tie (out_edge, out_edge_end) = out_edges (vertex, *slam_graph_); out_edge != out_edge_end; ++out_edge)
      (*slam_graph_)[*out_edge].computed_ = false;
    typename SLAMGraph::in_edge_iterator in_edge, in_edge_end;
    for (tie (in_edge, in_edge_end) = in_edges (vertex, *slam_graph_); in_edge != in_edge_end; ++in_edge)
      (*slam_graph_)[*in_edge].computed_ = false;
  }

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  inline typename pcl::registration::LUM<PointT>::Vector6f
  pcl::registration::LUM<PointT>::getPose (Vertex vertex)
  {
    if (vertex < 0 || vertex >= num_vertices (*slam_graph_))
    {
      PCL_WARN("[pcl::registration::LUM::getPose] You are attempting to get a pose estimate from a non-existing graph vertex.\n");
      return Vector6f::Zero ();
    }
    return (*slam_graph_)[vertex].pose_;
  }

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  inline void
  pcl::registration::LUM<PointT>::setCorrespondences (Vertex source_vertex, Vertex target_vertex, CorrespondencesPtr corrs)
  {
    if (corrs->size () < 3)
    {
      PCL_WARN("[pcl::registration::LUM::setCorrespondences] A set of correspondences needs to contain at least 3 different correspondences.\n");
      return;
    }
    if (source_vertex < 0 || source_vertex >= num_vertices (*slam_graph_) || target_vertex < 0 || target_vertex >= num_vertices (*slam_graph_) || source_vertex == target_vertex)
    {
      PCL_WARN("[pcl::registration::LUM::setCorrespondences] You are attempting to set a set of correspondences between non-existing or identical graph vertices.\n");
      return;
    }
    Edge e;
    bool present;
    tie (e, present) = edge (source_vertex, target_vertex, *slam_graph_);
    if (!present)
      tie (e, present) = add_edge (source_vertex, target_vertex, *slam_graph_);
    (*slam_graph_)[e].corrs_ = corrs;
    (*slam_graph_)[e].computed_ = false;
  }

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  inline pcl::CorrespondencesPtr
  pcl::registration::LUM<PointT>::getCorrespondences (Vertex source_vertex, Vertex target_vertex)
  {
    if (source_vertex < 0 || source_vertex >= num_vertices (*slam_graph_) || target_vertex < 0 || target_vertex >= num_vertices (*slam_graph_))
    {
      PCL_WARN("[pcl::registration::LUM::getCorrespondences] You are attempting to get a set of correspondences between non-existing graph vertices.\n");
      return pcl::CorrespondencesPtr ();
    }
    Edge e;
    bool present;
    tie (e, present) = edge (source_vertex, target_vertex, *slam_graph_);
    if (!present)
    {
      PCL_WARN("[pcl::registration::LUM::getCorrespondences] You are attempting to get a set of correspondences from a non-existing graph edge.\n");
      return pcl::CorrespondencesPtr ();
    }
    return (*slam_graph_)[e].corrs_;
  }

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  void
  pcl::registration::LUM<PointT>::compute ()
  {
    // Get graph size
    size_t n = num_vertices (*slam_graph_);
    if (n < 2)
    {
      PCL_WARN("[pcl::registration::LUM::compute] The internal SLAM graph needs to contain at least 2 vertices.\n");
      return;
    }

    for (size_t i = 0; i < max_iterations_; ++i)
    {
      // Linearized computation of C^-1 and C^-1*D for all edges in the graph (results stored in slam_graph_)
      typename SLAMGraph::edge_iterator e, e_end;
      for (tie (e, e_end) = edges (*slam_graph_); e != e_end; ++e)
        computeEdge (*e);

      // Declare matrices G and B
      Eigen::MatrixXf G = Eigen::MatrixXf::Zero (6 * (n - 1), 6 * (n - 1));
      Eigen::VectorXf B = Eigen::VectorXf::Zero (6 * (n - 1));

      // Start at 1 because 0 is the reference pose
      for (size_t vi = 1; vi != n; ++vi)
      {
        for (size_t vj = 0; vj != n; ++vj)
        {
          // Attempt to use the forward edge, otherwise use backward edge, otherwise there was no edge
          Edge e;
          bool present1, present2;
          tie (e, present1) = edge (vi, vj, *slam_graph_);
          if (!present1)
          {
            tie (e, present2) = edge (vj, vi, *slam_graph_);
            if (!present2)
              continue;
          }

          // Fill in elements of G and B
          // TODO matrixwise += may be risky/time consuming
          if (vj > 0)
            G.block (6 * (vi - 1), 6 * (vj - 1), 6, 6) = -(*slam_graph_)[e].cinv_;
          G.block (6 * (vi - 1), 6 * (vi - 1), 6, 6) += (*slam_graph_)[e].cinv_;
          B.segment (6 * (vi - 1), 6) += (present1 ? 1 : -1) * (*slam_graph_)[e].cinvd_;
        }
      }

      // Computation of the linear equation system: GX = B
      // TODO investigate accuracy vs. speed tradeoff and find the best solving method for our type of linear equation (sparse)
      Eigen::VectorXf X = G.colPivHouseholderQr ().solve (B);

      // Update the poses
      for (size_t vi = 1; vi != n; ++vi)
        setPose (vi, getPose (vi) - incidenceCorrection (getPose (vi)).inverse () * X.segment (6 * (vi - 1), 6));
    }
  }

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  typename pcl::registration::LUM<PointT>::PointCloudPtr
  pcl::registration::LUM<PointT>::getConcatenatedCloud ()
  {
    PointCloudPtr out (new PointCloud);
    typename SLAMGraph::vertex_iterator v, v_end;
    for (tie (v, v_end) = vertices (*slam_graph_); v != v_end; ++v)
    {
      PointCloud temp;
      pcl::transformPointCloud (*(*slam_graph_)[*v].cloud_, temp, poseToTransform ((*slam_graph_)[*v].pose_));
      *out += temp;
    }
    return out;
  }

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  void
  pcl::registration::LUM<PointT>::computeEdge (Edge e)
  {
    if ((*slam_graph_)[e].computed_)
      return;

    // Get correspondences and size
    CorrespondencesPtr corrs = (*slam_graph_)[e].corrs_;
    size_t m = corrs->size ();

    // Get compounded point clouds
    // TODO use the linearized compounding operation and apply it only to the correspondence points instead of all points
    PointCloud source_cloud, target_cloud;
    pcl::transformPointCloud (*(*slam_graph_)[source (e, *slam_graph_)].cloud_, source_cloud, poseToTransform ((*slam_graph_)[source (e, *slam_graph_)].pose_));
    pcl::transformPointCloud (*(*slam_graph_)[target (e, *slam_graph_)].cloud_, target_cloud, poseToTransform ((*slam_graph_)[target (e, *slam_graph_)].pose_));

    // Declare matrices M'M and M'Z
    Matrix6f MM = Matrix6f::Identity ();
    Vector6f MZ = Vector6f::Zero ();

    for (size_t k = 0; k != m; ++k)
    {
      // Declare temporary variables
      float x, y, z, dx, dy, dz;

      // Get average of point pair k and difference of point pair k
      x = (source_cloud.points[(*corrs)[k].index_query].x + target_cloud.points[(*corrs)[k].index_match].x) / 2.0;
      y = (source_cloud.points[(*corrs)[k].index_query].y + target_cloud.points[(*corrs)[k].index_match].y) / 2.0;
      z = (source_cloud.points[(*corrs)[k].index_query].z + target_cloud.points[(*corrs)[k].index_match].z) / 2.0;
      dx = source_cloud.points[(*corrs)[k].index_query].x - target_cloud.points[(*corrs)[k].index_match].x;
      dy = source_cloud.points[(*corrs)[k].index_query].y - target_cloud.points[(*corrs)[k].index_match].y;
      dz = source_cloud.points[(*corrs)[k].index_query].z - target_cloud.points[(*corrs)[k].index_match].z;

      // Fast computation of summation elements of M'M
      MM (0, 4) -= y;
      MM (0, 5) += z;
      MM (1, 3) -= z;
      MM (1, 4) += x;
      MM (2, 3) += y;
      MM (2, 5) -= x;
      MM (3, 4) -= x * z;
      MM (3, 5) -= x * y;
      MM (4, 5) -= y * z;
      MM (3, 3) += y * y + z * z;
      MM (4, 4) += x * x + y * y;
      MM (5, 5) += x * x + z * z;

      // Fast computation of M'Z
      MZ (0) += dx;
      MZ (1) += dy;
      MZ (2) += dz;
      MZ (3) += y * dz - z * dy;
      MZ (4) += x * dy - y * dx;
      MZ (5) += z * dx - x * dz;
    }

    // Remaining elements of M'M
    MM (0, 0) = MM (1, 1) = MM (2, 2) = m;
    MM (4, 0) = MM (0, 4);
    MM (5, 0) = MM (0, 5);
    MM (3, 1) = MM (1, 3);
    MM (4, 1) = MM (1, 4);
    MM (3, 2) = MM (2, 3);
    MM (5, 2) = MM (2, 5);
    MM (4, 3) = MM (3, 4);
    MM (5, 3) = MM (3, 5);
    MM (5, 4) = MM (4, 5);

    // Pose difference estimation
    Vector6f D = MM.inverse () * MZ;

    // Declare s^2
    float ss = 0;
    for (size_t k = 0; k != m; ++k)
    {
      // Declare temporary variables
      float x, y, z, dx, dy, dz;

      // Get average of point pair k and difference of point pair k
      x = (source_cloud.points[(*corrs)[k].index_query].x + target_cloud.points[(*corrs)[k].index_match].x) / 2.0;
      y = (source_cloud.points[(*corrs)[k].index_query].y + target_cloud.points[(*corrs)[k].index_match].y) / 2.0;
      z = (source_cloud.points[(*corrs)[k].index_query].z + target_cloud.points[(*corrs)[k].index_match].z) / 2.0;
      dx = source_cloud.points[(*corrs)[k].index_query].x - target_cloud.points[(*corrs)[k].index_match].x;
      dy = source_cloud.points[(*corrs)[k].index_query].y - target_cloud.points[(*corrs)[k].index_match].y;
      dz = source_cloud.points[(*corrs)[k].index_query].z - target_cloud.points[(*corrs)[k].index_match].z;

      // Computation of s^2
      // TODO if s^2 becomes very small, GX = B is in risk of not being computable
      ss += pow (dx - (D (0) + z * D (5) - y * D (4)), 2) + pow (dy - (D (1) + x * D (4) - z * D (3)), 2) + pow (dz - (D (2) + y * D (3) - x * D (5)), 2);
    }

    // Output results
    (*slam_graph_)[e].cinv_ = MM * (1 / ss);
    (*slam_graph_)[e].cinvd_ = MZ * (1 / ss);
    (*slam_graph_)[e].computed_ = true;
  }

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  inline Eigen::Vector3f
  pcl::registration::LUM<PointT>::linearizedCompound (Vector6f pose, Eigen::Vector3f point)
  {
    Eigen::Vector3f out;
    out (0) = pose (0) - point (2) * sin (pose (4)) + cos (pose (4)) * (point (0) * cos (pose (5)) - point (1) * sin (pose (5)));
    out (1) = pose (1) + point (2) * cos (pose (4)) * sin (pose (3)) + cos (pose (3)) * (point (1) * cos (pose (5)) + point (0) * sin (pose (5)))
        + sin (pose (3)) * sin (pose (4)) * (point (0) * cos (pose (5)) - point (1) * sin (pose (5)));
    out (2) = pose (2) - sin (pose (3)) * (point (1) * cos (pose (5)) + point (0) * sin (pose (5)))
        + cos (pose (3)) * (point (2) * cos (pose (4)) + sin (pose (4)) * (point (0) * cos (pose (5)) - point (1) * sin (pose (5))));
    return out;
  }

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  inline typename pcl::registration::LUM<PointT>::Matrix6f
  pcl::registration::LUM<PointT>::incidenceCorrection (Vector6f pose)
  {
    Matrix6f out = Matrix6f::Identity ();
    float cx = cosf (pose (3)), sx = sinf (pose (3)), cy = cosf (pose (4)), sy = sinf (pose (4));
    out (0, 4) = pose (1) * sx - pose (2) * cx;
    out (0, 5) = pose (1) * cx * cy + pose (2) * sx * cy;
    out (1, 3) = pose (2);
    out (1, 4) = -pose (0) * sx;
    out (1, 5) = -pose (0) * cx * cy + pose (2) * sy;
    out (2, 3) = -pose (1);
    out (2, 4) = pose (0) * cx;
    out (2, 5) = -pose (0) * sx * cy - pose (1) * sy;
    out (3, 5) = sy;
    out (4, 4) = sx;
    out (4, 5) = cx * cy;
    out (5, 4) = cx;
    out (5, 5) = -sx * cy;
    return out;
  }

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
  inline Eigen::Affine3f
  pcl::registration::LUM<PointT>::poseToTransform (Vector6f pose)
  {
    Eigen::Affine3f t;
    float A = cosf (pose (5)), B = sinf (pose (5)), C = cosf (pose (4)), D = sinf (pose (4)), E = cosf (pose (3)), F = sinf (pose (3)), DE = D * E, DF = D * F;
    t (0, 0) = A * C;
    t (0, 1) = A * DF - B * E;
    t (0, 2) = B * F + A * DE;
    t (0, 3) = pose (0);
    t (1, 0) = B * C;
    t (1, 1) = A * E + B * DF;
    t (1, 2) = B * DE - A * F;
    t (1, 3) = pose (1);
    t (2, 0) = -D;
    t (2, 1) = C * F;
    t (2, 2) = C * E;
    t (2, 3) = pose (2);
    t (3, 0) = 0;
    t (3, 1) = 0;
    t (3, 2) = 0;
    t (3, 3) = 1;
    return t;
  }

#endif // PCL_REGISTRATION_IMPL_LUM_H_