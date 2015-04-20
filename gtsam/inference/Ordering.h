/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    Ordering.h
 * @author  Richard Roberts
 * @author  Andrew Melim
 * @date    Sep 2, 2010
 */

#pragma once

#include <algorithm>
#include <vector>
#include <boost/assign/list_inserter.hpp>

#include <gtsam/base/FastSet.h>
#include <gtsam/inference/Key.h>
#include <gtsam/inference/VariableIndex.h>
#include <gtsam/inference/MetisIndex.h>
#include <gtsam/inference/FactorGraph.h>

namespace gtsam {

  class Ordering : public std::vector<Key> {
  protected:
    typedef std::vector<Key> Base;

  public:

    /// Type of ordering to use
    enum OrderingType {
      COLAMD, METIS, CUSTOM
    };

    typedef Ordering This; ///< Typedef to this class
    typedef boost::shared_ptr<This> shared_ptr; ///< shared_ptr to this class

    /// Create an empty ordering
    GTSAM_EXPORT Ordering() {}

    /// Create from a container
    template<typename KEYS>
    explicit Ordering(const KEYS& keys) : Base(keys.begin(), keys.end()) {}

    /// Create an ordering using iterators over keys
    template<typename ITERATOR>
    Ordering(ITERATOR firstKey, ITERATOR lastKey) : Base(firstKey, lastKey) {}

    /// Add new variables to the ordering as ordering += key1, key2, ...  Equivalent to calling
    /// push_back.
    boost::assign::list_inserter<boost::assign_detail::call_push_back<This> >
      operator+=(Key key) {
        return boost::assign::make_list_inserter(boost::assign_detail::call_push_back<This>(*this))(key);
    }

    /// Invert (not reverse) the ordering - returns a map from key to order position
    FastMap<Key, size_t> invert() const;

    /// @name Fill-reducing Orderings @{

    /// Compute a fill-reducing ordering using COLAMD from a factor graph (see details for note on
    /// performance). This internally builds a VariableIndex so if you already have a VariableIndex,
    /// it is faster to use COLAMD(const VariableIndex&)
    template<class FACTOR>
    static Ordering colamd(const FactorGraph<FACTOR>& graph) {
      return colamd(VariableIndex(graph)); }

    /// Compute a fill-reducing ordering using COLAMD from a VariableIndex.
    static GTSAM_EXPORT Ordering colamd(const VariableIndex& variableIndex);

    /// Compute a fill-reducing ordering using constrained COLAMD from a factor graph (see details
    /// for note on performance).  This internally builds a VariableIndex so if you already have a
    /// VariableIndex, it is faster to use COLAMD(const VariableIndex&).  This function constrains
    /// the variables in \c constrainLast to the end of the ordering, and orders all other variables
    /// before in a fill-reducing ordering.  If \c forceOrder is true, the variables in \c
    /// constrainLast will be ordered in the same order specified in the vector<Key> \c
    /// constrainLast.   If \c forceOrder is false, the variables in \c constrainLast will be
    /// ordered after all the others, but will be rearranged by CCOLAMD to reduce fill-in as well.
    template<class FACTOR>
    static Ordering colamdConstrainedLast(const FactorGraph<FACTOR>& graph,
      const std::vector<Key>& constrainLast, bool forceOrder = false) {
        return colamdConstrainedLast(VariableIndex(graph), constrainLast, forceOrder); }

    /// Compute a fill-reducing ordering using constrained COLAMD from a VariableIndex.  This
    /// function constrains the variables in \c constrainLast to the end of the ordering, and orders
    /// all other variables before in a fill-reducing ordering.  If \c forceOrder is true, the
    /// variables in \c constrainLast will be ordered in the same order specified in the vector<Key>
    /// \c constrainLast.   If \c forceOrder is false, the variables in \c constrainLast will be
    /// ordered after all the others, but will be rearranged by CCOLAMD to reduce fill-in as well.
    static GTSAM_EXPORT Ordering colamdConstrainedLast(const VariableIndex& variableIndex,
      const std::vector<Key>& constrainLast, bool forceOrder = false);

    /// Compute a fill-reducing ordering using constrained COLAMD from a factor graph (see details
    /// for note on performance).  This internally builds a VariableIndex so if you already have a
    /// VariableIndex, it is faster to use COLAMD(const VariableIndex&).  This function constrains
    /// the variables in \c constrainLast to the end of the ordering, and orders all other variables
    /// before in a fill-reducing ordering.  If \c forceOrder is true, the variables in \c
    /// constrainLast will be ordered in the same order specified in the vector<Key> \c
    /// constrainLast.   If \c forceOrder is false, the variables in \c constrainFirst will be
    /// ordered after all the others, but will be rearranged by CCOLAMD to reduce fill-in as well.
    template<class FACTOR>
    static Ordering colamdConstrainedFirst(const FactorGraph<FACTOR>& graph,
      const std::vector<Key>& constrainFirst, bool forceOrder = false) {
        return colamdConstrainedFirst(VariableIndex(graph), constrainFirst, forceOrder); }

    /// Compute a fill-reducing ordering using constrained COLAMD from a VariableIndex.  This
    /// function constrains the variables in \c constrainFirst to the front of the ordering, and
    /// orders all other variables after in a fill-reducing ordering.  If \c forceOrder is true, the
    /// variables in \c constrainFirst will be ordered in the same order specified in the
    /// vector<Key> \c constrainFirst.   If \c forceOrder is false, the variables in \c
    /// constrainFirst will be ordered after all the others, but will be rearranged by CCOLAMD to
    /// reduce fill-in as well.
    static GTSAM_EXPORT Ordering colamdConstrainedFirst(const VariableIndex& variableIndex,
      const std::vector<Key>& constrainFirst, bool forceOrder = false);

    /// Compute a fill-reducing ordering using constrained COLAMD from a factor graph (see details
    /// for note on performance).  This internally builds a VariableIndex so if you already have a
    /// VariableIndex, it is faster to use COLAMD(const VariableIndex&).  In this function, a group
    /// for each variable should be specified in \c groups, and each group of variables will appear
    /// in the ordering in group index order.  \c groups should be a map from Key to group index.
    /// The group indices used should be consecutive starting at 0, but may appear in \c groups in
    /// arbitrary order.  Any variables not present in \c groups will be assigned to group 0.  This
    /// function simply fills the \c cmember argument to CCOLAMD with the supplied indices, see the
    /// CCOLAMD documentation for more information.
    template<class FACTOR>
    static Ordering colamdConstrained(const FactorGraph<FACTOR>& graph,
      const FastMap<Key, int>& groups) {
        return colamdConstrained(VariableIndex(graph), groups); }

    /// Compute a fill-reducing ordering using constrained COLAMD from a VariableIndex.  In this
    /// function, a group for each variable should be specified in \c groups, and each group of
    /// variables will appear in the ordering in group index order.  \c groups should be a map from
    /// Key to group index. The group indices used should be consecutive starting at 0, but may
    /// appear in \c groups in arbitrary order.  Any variables not present in \c groups will be
    /// assigned to group 0.  This function simply fills the \c cmember argument to CCOLAMD with the
    /// supplied indices, see the CCOLAMD documentation for more information.
    static GTSAM_EXPORT Ordering colamdConstrained(const VariableIndex& variableIndex,
      const FastMap<Key, int>& groups);

    /// Return a natural Ordering. Typically used by iterative solvers
    template <class FACTOR>
    static Ordering Natural(const FactorGraph<FACTOR> &fg) {
      FastSet<Key> src = fg.keys();
      std::vector<Key> keys(src.begin(), src.end());
      std::stable_sort(keys.begin(), keys.end());
      return Ordering(keys);
    }

    /// METIS Formatting function
    template<class FACTOR>
    static GTSAM_EXPORT void CSRFormat(std::vector<int>& xadj, std::vector<int>& adj, const FactorGraph<FACTOR>& graph);

    /// Compute an ordering determined by METIS from a VariableIndex
    static GTSAM_EXPORT Ordering metis(const MetisIndex& met);

    template<class FACTOR>
    static Ordering metis(const FactorGraph<FACTOR>& graph)
    {
        return metis(MetisIndex(graph));
    }

    /// @}

    /// @name Testable @{

    GTSAM_EXPORT void print(const std::string& str = "", const KeyFormatter& keyFormatter = DefaultKeyFormatter) const;

    GTSAM_EXPORT bool equals(const Ordering& other, double tol = 1e-9) const;

    /// @}

  private:
    /// Internal COLAMD function
    static GTSAM_EXPORT Ordering colamdConstrained(
      const VariableIndex& variableIndex, std::vector<int>& cmember);


    /** Serialization function */
    friend class boost::serialization::access;
    template<class ARCHIVE>
    void serialize(ARCHIVE & ar, const unsigned int /*version*/) {
      ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Base);
    }
  };

  /// traits
  template<> struct traits<Ordering> : public Testable<Ordering> {};

}

