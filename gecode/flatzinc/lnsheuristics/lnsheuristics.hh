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

#ifndef GECODE_FLATZINC_NEIGHBORHOOD_NEIGHBORHOODS_HH
#define GECODE_FLATZINC_NEIGHBORHOOD_NEIGHBORHOODS_HH

#include <gecode/flatzinc.hh>
#include <gecode/flatzinc/lnsheuristicregistry.hh>

namespace Gecode { namespace FlatZinc {
  class GECODE_FLATZINC_EXPORT LnsHeuristicCombinator : public LnsHeuristic {
    std::vector<std::shared_ptr<LnsHeuristic>> neighborhoods;
  public:
    explicit LnsHeuristicCombinator(std::vector<std::shared_ptr<LnsHeuristic>>&&);
    void heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next) override;
    [[nodiscard]] bool applicable(const FlatZincSpace &incumbent) const override;
  };

  class GECODE_FLATZINC_EXPORT Circuit : public LnsHeuristic {
      int offset;
      std::vector<int> vars;
    public:
      Circuit(int o, std::vector<int>&& v);
      void heuristic(const FlatZincSpace& incumbent, FlatZincSpace& next) override;
      [[nodiscard]] bool applicable(const FlatZincSpace &incumbent) const override;
  };

  class GECODE_FLATZINC_EXPORT ScheduleUnary : public LnsHeuristic {
    std::vector<int> vars;
    std::vector<int> durations;
  public:
    ScheduleUnary(std::vector<int>&& tasks, std::vector<int>&& durs);
    void heuristic(const FlatZincSpace& incumbent, FlatZincSpace& next) override;
    [[nodiscard]] bool applicable(const FlatZincSpace &incumbent) const override;
  };

}}

#endif

// STATISTICS: flatzinc-any
