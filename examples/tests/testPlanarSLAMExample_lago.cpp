/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 *  @file  testPlanarSLAMExample_lago.cpp
 *  @brief Unit tests for planar SLAM example using the initialization technique
 *  LAGO (Linear Approximation for Graph Optimization)
 *
 *  @author Luca Carlone
 *  @author Frank Dellaert
 *  @date   May 14, 2014
 */

// As this is a planar SLAM example, we will use Pose2 variables (x, y, theta) to represent
// the robot positions and Point2 variables (x, y) to represent the landmark coordinates.
#include <gtsam/geometry/Pose2.h>

#include <gtsam/linear/GaussianFactorGraph.h>
#include <gtsam/linear/VectorValues.h>

// Each variable in the system (poses and landmarks) must be identified with a unique key.
// We can either use simple integer keys (1, 2, 3, ...) or symbols (X1, X2, L1).
// Here we will use Symbols
#include <gtsam/inference/Symbol.h>

// In GTSAM, measurement functions are represented as 'factors'. Several common factors
// have been provided with the library for solving robotics/SLAM/Bundle Adjustment problems.
// Here we will use a RangeBearing factor for the range-bearing measurements to identified
// landmarks, and Between factors for the relative motion described by odometry measurements.
// Also, we will initialize the robot at the origin using a Prior factor.
#include <gtsam/slam/PriorFactor.h>
#include <gtsam/slam/BetweenFactor.h>

// When the factors are created, we will add them to a Factor Graph. As the factors we are using
// are nonlinear factors, we will need a Nonlinear Factor Graph.
#include <gtsam/nonlinear/NonlinearFactorGraph.h>

#include <gtsam/base/TestableAssertions.h>
#include <CppUnitLite/TestHarness.h>
#include <boost/math/constants/constants.hpp>
#include <cmath>

using namespace std;
using namespace gtsam;
using namespace boost::assign;

Symbol x0('x', 0), x1('x', 1), x2('x', 2), x3('x', 3);
static SharedNoiseModel model(noiseModel::Isotropic::Sigma(3, 0.1));
static const double PI = boost::math::constants::pi<double>();

#include <gtsam/inference/graph.h>
/**
 *  @brief Initialization technique for planar pose SLAM using
 *  LAGO (Linear Approximation for Graph Optimization). see papers:
 *
 *  L. Carlone, R. Aragues, J. Castellanos, and B. Bona, A fast and accurate
 *  approximation for planar pose graph optimization, IJRR, 2014.
 *
 *  L. Carlone, R. Aragues, J.A. Castellanos, and B. Bona, A linear approximation
 *  for graph-based simultaneous localization and mapping, RSS, 2011.
 *
 *  @param graph: nonlinear factor graph including between (Pose2) measurements
 *  @return Values: initial guess including orientation estimate from LAGO
 */

/*
 *  This function computes the cumulative orientation wrt the root (without wrapping)
 *  for a node (without wrapping). The function starts at the nodes and moves towards the root
 *  summing up the (directed) rotation measurements. The root is assumed to have orientation zero
 */
typedef map<Key,double> key2doubleMap;
const Key keyAnchor = symbol('A',0);

double computeThetaToRoot(const Key nodeKey, const PredecessorMap<Key>& tree,
    const key2doubleMap& deltaThetaMap, key2doubleMap& thetaFromRootMap) {

  double nodeTheta = 0;
  Key key_child = nodeKey; // the node
  Key key_parent = 0; // the initialization does not matter
  while(1){
    // We check if we reached the root
    if(tree.at(key_child)==key_child) // if we reached the root
      break;
    // we sum the delta theta corresponding to the edge parent->child
    nodeTheta += deltaThetaMap.at(key_child);
    // we get the parent
    key_parent = tree.at(key_child); // the parent
    // we check if we connected to some part of the tree we know
    if(thetaFromRootMap.find(key_parent)!=thetaFromRootMap.end()){
      nodeTheta += thetaFromRootMap[key_parent];
      break;
    }
    key_child = key_parent; // we move upwards in the tree
  }
  return nodeTheta;
}

/*
 *  This function computes the cumulative orientation (without wrapping)
 *  for all node wrt the root (root has zero orientation)
 */
key2doubleMap computeThetasToRoot(const key2doubleMap& deltaThetaMap,
    const PredecessorMap<Key>& tree) {

  key2doubleMap thetaToRootMap;
  key2doubleMap::const_iterator it;
  // for all nodes in the tree
  for(it = deltaThetaMap.begin(); it != deltaThetaMap.end(); ++it )
  {
    // compute the orientation wrt root
    Key nodeKey = it->first;
    double nodeTheta = computeThetaToRoot(nodeKey, tree, deltaThetaMap,
        thetaToRootMap);
    thetaToRootMap.insert(std::pair<Key, double>(nodeKey, nodeTheta));
  }
  return thetaToRootMap;
}

/*
 *  Given a factor graph "g", and a spanning tree "tree", the function selects the nodes belonging to the tree and to g,
 *  and stores the factor slots corresponding to edges in the tree and to chordsIds wrt this tree
 *  Also it computes deltaThetaMap which is a fast way to encode relative orientations along the tree:
 *  for a node key2, s.t. tree[key2]=key1, the values deltaThetaMap[key2] is the relative orientation theta[key2]-theta[key1]
 */
void getSymbolicGraph(
    /*OUTPUTS*/ vector<size_t>& spanningTreeIds, vector<size_t>& chordsIds, key2doubleMap& deltaThetaMap,
    /*INPUTS*/ const PredecessorMap<Key>& tree, const NonlinearFactorGraph& g){

  // Get keys for which you want the orientation
  size_t id=0;
  // Loop over the factors
  BOOST_FOREACH(const boost::shared_ptr<NonlinearFactor>& factor, g){
    if (factor->keys().size() == 2){
      Key key1 = factor->keys()[0];
      Key key2 = factor->keys()[1];

      // recast to a between
      boost::shared_ptr< BetweenFactor<Pose2> > pose2Between = boost::dynamic_pointer_cast< BetweenFactor<Pose2> >(factor);
      if (!pose2Between) continue;

      // get the orientation - measured().theta();
      double deltaTheta = pose2Between->measured().theta();

      // insert (directed) orientations in the map "deltaThetaMap"
      bool inTree=false;
      if(tree.at(key1)==key2){
        deltaThetaMap.insert(std::pair<Key, double>(key1, -deltaTheta));
        inTree = true;
      } else if(tree.at(key2)==key1){
        deltaThetaMap.insert(std::pair<Key, double>(key2,  deltaTheta));
        inTree = true;
      }

      // store factor slot, distinguishing spanning tree edges from chordsIds
      if(inTree == true)
        spanningTreeIds.push_back(id);
      else // it's a chord!
        chordsIds.push_back(id);
    }
    id++;
  }
}

// Retrieves the deltaTheta and the corresponding noise model from a BetweenFactor<Pose2>
void getDeltaThetaAndNoise(NonlinearFactor::shared_ptr factor,
    Vector& deltaTheta, noiseModel::Diagonal::shared_ptr& model_deltaTheta) {

  boost::shared_ptr<BetweenFactor<Pose2> > pose2Between =
      boost::dynamic_pointer_cast<BetweenFactor<Pose2> >(factor);
  if (!pose2Between)
    throw std::invalid_argument(
        "buildOrientationGraph: invalid between factor!");
  deltaTheta = (Vector(1) << pose2Between->measured().theta());
  // Retrieve noise model
  SharedNoiseModel model = pose2Between->get_noiseModel();
  boost::shared_ptr<noiseModel::Diagonal> diagonalModel =
      boost::dynamic_pointer_cast<noiseModel::Diagonal>(model);
  if (!diagonalModel)
    throw std::invalid_argument("buildOrientationGraph: invalid noise model (current version assumes diagonal noise model)!");
  Vector std_deltaTheta = (Vector(1) << diagonalModel->sigma(2) ); // std on the angular measurement
  model_deltaTheta = noiseModel::Diagonal::Sigmas(std_deltaTheta);
}

/*
 *  Linear factor graph with regularized orientation measurements
 */
GaussianFactorGraph buildOrientationGraph(const vector<size_t>& spanningTreeIds, const vector<size_t>& chordsIds,
    const NonlinearFactorGraph& g, const key2doubleMap& orientationsToRoot, const PredecessorMap<Key>& tree){

  GaussianFactorGraph lagoGraph;
  Vector deltaTheta;
  noiseModel::Diagonal::shared_ptr model_deltaTheta;

  Matrix I = eye(1);
  // put original measurements in the spanning tree
  BOOST_FOREACH(const size_t& factorId, spanningTreeIds){
    const FastVector<Key>& keys = g[factorId]->keys();
    Key key1 = keys[0], key2 = keys[1];
    getDeltaThetaAndNoise(g[factorId], deltaTheta, model_deltaTheta);
    lagoGraph.add(JacobianFactor(key1, -I, key2, I, deltaTheta, model_deltaTheta));
  }
  // put regularized measurements in the chordsIds
  BOOST_FOREACH(const size_t& factorId, chordsIds){
    const FastVector<Key>& keys = g[factorId]->keys();
    Key key1 = keys[0], key2 = keys[1];
    getDeltaThetaAndNoise(g[factorId], deltaTheta, model_deltaTheta);
    double key1_DeltaTheta_key2 = deltaTheta(0);
    double k2pi_noise = key1_DeltaTheta_key2 + orientationsToRoot.at(key1) - orientationsToRoot.at(key2); // this coincides to summing up measurements along the cycle induced by the chord
    double k = round(k2pi_noise/(2*PI));
    Vector deltaThetaRegularized = (Vector(1) << key1_DeltaTheta_key2 - 2*k*PI);
    lagoGraph.add(JacobianFactor(key1, -I, key2, I, deltaThetaRegularized, model_deltaTheta));
  }
  // prior on some orientation (anchor)
  noiseModel::Diagonal::shared_ptr model_anchor = noiseModel::Diagonal::Variances((Vector(1) << 1e-8));
  lagoGraph.add(JacobianFactor(tree.begin()->first, I, (Vector(1) << 0.0), model_anchor));
  return lagoGraph;
}

/* ************************************************************************* */
// Selects the subgraph composed by between factors and transforms priors into between wrt a fictitious node
NonlinearFactorGraph buildPose2graph(const NonlinearFactorGraph& graph){
  NonlinearFactorGraph pose2Graph;

  BOOST_FOREACH(const boost::shared_ptr<NonlinearFactor>& factor, graph){

    // recast to a between on Pose2
    boost::shared_ptr< BetweenFactor<Pose2> > pose2Between =
        boost::dynamic_pointer_cast< BetweenFactor<Pose2> >(factor);
    if (pose2Between)
      pose2Graph.add(pose2Between);

    // recast to a between on Rot2
    boost::shared_ptr< BetweenFactor<Rot2> > rot2Between =
        boost::dynamic_pointer_cast< BetweenFactor<Rot2> >(factor);
    if (rot2Between)
      pose2Graph.add(rot2Between);

    // recast to a prior on Pose2
    boost::shared_ptr< PriorFactor<Pose2> > pose2Prior =
        boost::dynamic_pointer_cast< PriorFactor<Pose2> >(factor);
    if (pose2Prior)
      pose2Graph.add(BetweenFactor<Pose2>(keyAnchor, pose2Prior->keys()[0],
          pose2Prior->prior(), pose2Prior->get_noiseModel()));

    // recast to a prior on Rot2
    boost::shared_ptr< PriorFactor<Rot2> > rot2Prior =
        boost::dynamic_pointer_cast< PriorFactor<Rot2> >(factor);
    if (rot2Prior)
      pose2Graph.add(BetweenFactor<Rot2>(keyAnchor, rot2Prior->keys()[0],
          rot2Prior->prior(), rot2Prior->get_noiseModel()));
  }
  return pose2Graph;
}
/* ************************************************************************* */
// returns the orientations of the Pose2 in the connected sub-graph defined by BetweenFactor<Pose2>
VectorValues initializeLago(const NonlinearFactorGraph& graph) {

  // We "extract" the Pose2 subgraph of the original graph: this
  // is done to properly model priors and avoiding operating on a larger graph
  NonlinearFactorGraph pose2Graph = buildPose2graph(graph);

  // Find a minimum spanning tree
  PredecessorMap<Key> tree = findMinimumSpanningTree<NonlinearFactorGraph, Key, BetweenFactor<Pose2> >(pose2Graph);

  // Create a linear factor graph (LFG) of scalars
  key2doubleMap deltaThetaMap;
  vector<size_t> spanningTreeIds; // ids of between factors forming the spanning tree T
  vector<size_t> chordsIds;       // ids of between factors corresponding to chordsIds wrt T
  getSymbolicGraph(spanningTreeIds, chordsIds, deltaThetaMap, tree, pose2Graph);

  // temporary structure to correct wraparounds along loops
  key2doubleMap orientationsToRoot = computeThetasToRoot(deltaThetaMap, tree);

  // regularize measurements and plug everything in a factor graph
  GaussianFactorGraph lagoGraph = buildOrientationGraph(spanningTreeIds, chordsIds, pose2Graph, orientationsToRoot, tree);

  // Solve the LFG
  VectorValues estimateLago = lagoGraph.optimize();

  return estimateLago;
}

/* ************************************************************************* */
// Only correct the orientation part in initialGuess
Values initializeLago(const NonlinearFactorGraph& graph, const Values& initialGuess) {
  Values initialGuessLago;

  // get the orientation estimates from LAGO
  VectorValues orientations = initializeLago(graph);

  // for all nodes in the tree
  for(VectorValues::const_iterator it = orientations.begin(); it != orientations.end(); ++it ){
    Key key = it->first;
    if (key != keyAnchor){
      Pose2 pose = initialGuess.at<Pose2>(key);
      Vector orientation = orientations.at(key);
      Pose2 poseLago = Pose2(pose.x(),pose.y(),orientation(0));
      initialGuessLago.insert(key, poseLago);
    }
  }
  return initialGuessLago;
}

/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */


namespace simple {
// We consider a small graph:
//                            symbolic FG
//               x2               0  1
//             / | \              1  2
//            /  |  \             2  3
//          x3   |   x1           2  0
//           \   |   /            0  3
//            \  |  /
//               x0
//

Pose2 pose0 = Pose2(0.000000, 0.000000, 0.000000);
Pose2 pose1 = Pose2(1.000000, 1.000000, 1.570796);
Pose2 pose2 = Pose2(0.000000, 2.000000, 3.141593);
Pose2 pose3 = Pose2(-1.000000, 1.000000, 4.712389);

NonlinearFactorGraph graph() {
  NonlinearFactorGraph g;
  g.add(BetweenFactor<Pose2>(x0, x1, pose0.between(pose1), model));
  g.add(BetweenFactor<Pose2>(x1, x2, pose1.between(pose2), model));
  g.add(BetweenFactor<Pose2>(x2, x3, pose2.between(pose3), model));
  g.add(BetweenFactor<Pose2>(x2, x0, pose2.between(pose0), model));
  g.add(BetweenFactor<Pose2>(x0, x3, pose0.between(pose3), model));
  g.add(PriorFactor<Pose2>(x0, pose0, model));
  return g;
}
}

/* *************************************************************************** */
TEST( Lago, checkSTandChords ) {
  NonlinearFactorGraph g = simple::graph();
  PredecessorMap<Key> tree = findMinimumSpanningTree<NonlinearFactorGraph, Key,
      BetweenFactor<Pose2> >(g);

  key2doubleMap deltaThetaMap;
  vector<size_t> spanningTreeIds; // ids of between factors forming the spanning tree T
  vector<size_t> chordsIds; // ids of between factors corresponding to chordsIds wrt T
  getSymbolicGraph(spanningTreeIds, chordsIds, deltaThetaMap, tree, g);

  DOUBLES_EQUAL(spanningTreeIds[0], 0, 1e-6); // factor 0 is the first in the ST (0->1)
  DOUBLES_EQUAL(spanningTreeIds[1], 3, 1e-6); // factor 3 is the second in the ST(2->0)
  DOUBLES_EQUAL(spanningTreeIds[2], 4, 1e-6); // factor 4 is the third in the  ST(0->3)

}

/* *************************************************************************** */
TEST( Lago, orientationsOverSpanningTree ) {
  NonlinearFactorGraph g = simple::graph();
  PredecessorMap<Key> tree = findMinimumSpanningTree<NonlinearFactorGraph, Key,
      BetweenFactor<Pose2> >(g);

  // check the tree structure
  EXPECT_LONGS_EQUAL(tree[x0], x0);
  EXPECT_LONGS_EQUAL(tree[x1], x0);
  EXPECT_LONGS_EQUAL(tree[x2], x0);
  EXPECT_LONGS_EQUAL(tree[x3], x0);

  key2doubleMap expected;
  expected[x0]=  0;
  expected[x1]=  PI/2; // edge x0->x1 (consistent with edge (x0,x1))
  expected[x2]= -PI; // edge x0->x2 (traversed backwards wrt edge (x2,x0))
  expected[x3]= -PI/2;  // edge x0->x3 (consistent with edge (x0,x3))

  key2doubleMap deltaThetaMap;
  vector<size_t> spanningTreeIds; // ids of between factors forming the spanning tree T
  vector<size_t> chordsIds; // ids of between factors corresponding to chordsIds wrt T
  getSymbolicGraph(spanningTreeIds, chordsIds, deltaThetaMap, tree, g);

  key2doubleMap actual;
  actual = computeThetasToRoot(deltaThetaMap, tree);
  DOUBLES_EQUAL(expected[x0], actual[x0], 1e-6);
  DOUBLES_EQUAL(expected[x1], actual[x1], 1e-6);
  DOUBLES_EQUAL(expected[x2], actual[x2], 1e-6);
  DOUBLES_EQUAL(expected[x3], actual[x3], 1e-6);
}

/* *************************************************************************** */
TEST( Lago, regularizedMeasurements ) {
  NonlinearFactorGraph g = simple::graph();
  PredecessorMap<Key> tree = findMinimumSpanningTree<NonlinearFactorGraph, Key,
      BetweenFactor<Pose2> >(g);

  key2doubleMap deltaThetaMap;
  vector<size_t> spanningTreeIds; // ids of between factors forming the spanning tree T
  vector<size_t> chordsIds; // ids of between factors corresponding to chordsIds wrt T
  getSymbolicGraph(spanningTreeIds, chordsIds, deltaThetaMap, tree, g);

  key2doubleMap orientationsToRoot = computeThetasToRoot(deltaThetaMap, tree);

  GaussianFactorGraph lagoGraph = buildOrientationGraph(spanningTreeIds, chordsIds, g, orientationsToRoot, tree);
  std::pair<Matrix,Vector> actualAb = lagoGraph.jacobian();
  // jacobian corresponding to the orientation measurements (last entry is the prior on the anchor and is disregarded)
  Vector actual = (Vector(5) <<  actualAb.second(0),actualAb.second(1),actualAb.second(2),actualAb.second(3),actualAb.second(4));
  // this is the whitened error, so we multiply by the std to unwhiten
  actual = 0.1 * actual;
  // Expected regularized measurements (same for the spanning tree, corrected for the chordsIds)
  Vector expected = (Vector(5) << PI/2, PI, -PI/2, PI/2 - 2*PI , PI/2);

  EXPECT(assert_equal(expected, actual, 1e-6));
}

/* *************************************************************************** */
TEST( Lago, smallGraphVectorValues ) {

  VectorValues initialGuessLago = initializeLago(simple::graph());

  // comparison is up to PI, that's why we add some multiples of 2*PI
  EXPECT(assert_equal((Vector(1) << 0.0), initialGuessLago.at(x0), 1e-6));
  EXPECT(assert_equal((Vector(1) << 0.5 * PI), initialGuessLago.at(x1), 1e-6));
  EXPECT(assert_equal((Vector(1) << PI - 2*PI), initialGuessLago.at(x2), 1e-6));
  EXPECT(assert_equal((Vector(1) << 1.5 * PI - 2*PI), initialGuessLago.at(x3), 1e-6));
}

/* *************************************************************************** */
TEST( Lago, multiplePosePriors ) {
  NonlinearFactorGraph g = simple::graph();
  g.add(PriorFactor<Pose2>(x1, simple::pose1, model));
  VectorValues initialGuessLago = initializeLago(g);

  // comparison is up to PI, that's why we add some multiples of 2*PI
  EXPECT(assert_equal((Vector(1) << 0.0), initialGuessLago.at(x0), 1e-6));
  EXPECT(assert_equal((Vector(1) << 0.5 * PI), initialGuessLago.at(x1), 1e-6));
  EXPECT(assert_equal((Vector(1) << PI - 2*PI), initialGuessLago.at(x2), 1e-6));
  EXPECT(assert_equal((Vector(1) << 1.5 * PI - 2*PI), initialGuessLago.at(x3), 1e-6));
}

/* *************************************************************************** */
TEST( Lago, multiplePoseAndRotPriors ) {
  NonlinearFactorGraph g = simple::graph();
  g.add(PriorFactor<Rot2>(x1, simple::pose1.theta(), model));
  VectorValues initialGuessLago = initializeLago(g);

  // comparison is up to PI, that's why we add some multiples of 2*PI
  EXPECT(assert_equal((Vector(1) << 0.0), initialGuessLago.at(x0), 1e-6));
  EXPECT(assert_equal((Vector(1) << 0.5 * PI), initialGuessLago.at(x1), 1e-6));
  EXPECT(assert_equal((Vector(1) << PI - 2*PI), initialGuessLago.at(x2), 1e-6));
  EXPECT(assert_equal((Vector(1) << 1.5 * PI - 2*PI), initialGuessLago.at(x3), 1e-6));
}

/* *************************************************************************** */
TEST( Lago, smallGraphValues ) {

  // we set the orientations in the initial guess to zero
  Values initialGuess;
  initialGuess.insert(x0,Pose2(simple::pose0.x(),simple::pose0.y(),0.0));
  initialGuess.insert(x1,Pose2(simple::pose1.x(),simple::pose1.y(),0.0));
  initialGuess.insert(x2,Pose2(simple::pose2.x(),simple::pose2.y(),0.0));
  initialGuess.insert(x3,Pose2(simple::pose3.x(),simple::pose3.y(),0.0));

  // lago does not touch the Cartesian part and only fixed the orientations
  Values actual = initializeLago(simple::graph(), initialGuess);

  // we are in a noiseless case
  Values expected;
  expected.insert(x0,simple::pose0);
  expected.insert(x1,simple::pose1);
  expected.insert(x2,simple::pose2);
  expected.insert(x3,simple::pose3);

  EXPECT(assert_equal(expected, actual, 1e-6));
}

/* ************************************************************************* */
int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
/* ************************************************************************* */

