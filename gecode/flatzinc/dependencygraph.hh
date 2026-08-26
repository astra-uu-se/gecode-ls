/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.dev>
 *
 *  Copyright:
 *     Guido Tack, 2007
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.dev
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef GECODE_FLATZINC_DEPENDENCYGRAPH_HH
#define GECODE_FLATZINC_DEPENDENCYGRAPH_HH

#include <gecode/flatzinc.hh>
#include <string>
#include <map>
#include <set>

namespace Gecode { namespace FlatZinc {

  enum NEIGHBORHOOD_CONSTRAINT : int {
    NEIGHBORHOOD_NONE = 0,
    NEIGHBORHOOD_CIRCUIT = 10000,
    NEIGHBORHOOD_DISJUNCTIVE = 20000
  };

  /// Map from constraint identifier to constraint posting functions
  class GECODE_FLATZINC_EXPORT DependencyGraph {
  public:
    DependencyGraph();

    ~DependencyGraph() = default;

    /// Add neighborhood function \a p with identifier \a id
    void add(const std::string& id, NEIGHBORHOOD_CONSTRAINT p);

    void post(const FlatZincSpace&, ConExpr const* ce);

    /// Post constraint specified by \a ce
    std::vector<ConExpr const*> neighborhoodConstraints();

    bool isSource(ConExpr const* ce) const;

  private:
    /// The actual neighborhood registry
    std::map<std::string,NEIGHBORHOOD_CONSTRAINT> r;

    std::set<ConExpr const*> sources;

    std::vector<ConExpr const*> neighborhoods;
  };

}}

#endif

// STATISTICS: flatzinc-any
