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

  enum VAR_TYPE  {
    VAR_INT = 0,
    VAR_BOOL = 1,
    VAR_FLOAT = 2,
    VAR_SET_OF_VAR = 3
};

  class GECODE_FLATZINC_EXPORT GenericHeuristic : public LnsHeuristic {
    bool depCur;
  protected:
    std::vector<std::pair<VAR_TYPE, int>> vars;
    [[nodiscard]] unsigned int numVars() const;
    [[nodiscard]] unsigned int numVars(const FlatZincSpace& space) const;
    [[nodiscard]] unsigned int domainSize(const FlatZincSpace &next, int index) const;
    [[nodiscard]] std::vector<int> createIndices(const FlatZincSpace &next) const;
    [[nodiscard]] bool isIntVar(const FlatZincSpace &next, int index) const;
    [[nodiscard]] bool isBoolVar(const FlatZincSpace& next, int index) const;
    [[nodiscard]] int varIndex(const FlatZincSpace& next, int index) const;
    [[nodiscard]] bool isAssigned(const FlatZincSpace& next, int index) const;
    void freeze(const FlatZincSpace& incumbent, FlatZincSpace &next, int index) const;
  public:
    GenericHeuristic(const FlatZincSpace& space, bool dependencyCuration);
    void shrinkArrays(const std::map<int, int> &iv_new, const std::map<int, int> &bv_new, const std::map<int, int> &fv_new, const std::map<int, int> &sv_new) override;
    [[nodiscard]] std::shared_ptr<LnsHeuristic> clone() const override = 0;
    bool heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next, const MetaInfo &mi, bool foundNewSolution) override = 0;
    [[nodiscard]] bool applicable() const override;
  };

  class GECODE_FLATZINC_EXPORT NaiveRandom : public GenericHeuristic {
    static constexpr unsigned int queue_size{10};
  public:
    NaiveRandom(const FlatZincSpace& space, bool dependencyCuration);
    [[nodiscard]] std::shared_ptr<LnsHeuristic> clone() const override;
    bool heuristic(const FlatZincSpace& incumbent, FlatZincSpace& next, const MetaInfo &mi, bool foundNewSolution) override;
  };


  class GECODE_FLATZINC_EXPORT PropagationGuided : public GenericHeuristic {
    static constexpr unsigned int queue_size{10};
  public:
    PropagationGuided(const FlatZincSpace& space, bool dependencyCuration);
    [[nodiscard]] std::shared_ptr<LnsHeuristic> clone() const override;
    bool heuristic(const FlatZincSpace& incumbent, FlatZincSpace& next, const MetaInfo &mi, bool foundNewSolution) override;
  };

  class GECODE_FLATZINC_EXPORT ReversePropagationGuided : public GenericHeuristic {
    static constexpr unsigned int queue_size{10};
  public:
    ReversePropagationGuided(const FlatZincSpace& space, bool dependencyCuration);
    [[nodiscard]] std::shared_ptr<LnsHeuristic> clone() const override;
    bool heuristic(const FlatZincSpace& incumbent, FlatZincSpace& next, const MetaInfo &mi, bool foundNewSolution) override;
  };

  class GECODE_FLATZINC_EXPORT CostImpactGuided : public GenericHeuristic {
    struct CIGInfo {
      std::vector<double> bound_differences{};
      std::vector<double> scores{};
      double bound_diff_sum{0.0};
      double r{0.0};
    } info;
    static constexpr unsigned int dives{2};
    static constexpr double alpha{0.5};
  public:
    CostImpactGuided(const FlatZincSpace& space, bool dependencyCuration);
    [[nodiscard]] bool requires_cloning() const override;
    [[nodiscard]] std::shared_ptr<LnsHeuristic> clone() const override;
    bool heuristic(const FlatZincSpace& incumbent, FlatZincSpace& next, const MetaInfo &mi, bool foundNewSolution) override;
  };

  class GECODE_FLATZINC_EXPORT StaticVariableRelationGuided : public GenericHeuristic {
    int iv_size;
    std::vector<std::vector<int>> constraint_arguments;
    std::vector<std::vector<double>> variable_relations;
    std::vector<int> variable_impacts;
    bool isIntVar(int index) const;
    bool isBoolVar(int index) const;
    int varIndex(int index) const;
    void compute_variable_relations(unsigned int numVars);
    int selectRandomBestIndex(FlatZincSpace &next, std::vector<int> &indices, int n) const;

    int selectRandomRelatedIndex(FlatZincSpace &next, std::vector<int> &indices,
                                 unsigned int best_var_index, int n) const;

  public:
    StaticVariableRelationGuided(const FlatZincSpace &space, bool dependencyCuration,
                                 const std::vector<ConExpr*> &constraints);
    void shrinkArrays(const std::map<int, int> &iv_new, const std::map<int, int> &bv_new, const std::map<int, int> &fv_new, const std::map<int, int> &sv_new) override;
    [[nodiscard]] bool requires_cloning() const override;
    [[nodiscard]] std::shared_ptr<LnsHeuristic> clone() const override;
    bool heuristic(const FlatZincSpace& incumbent, FlatZincSpace& next, const MetaInfo &mi, bool foundNewSolution) override;
  };

  class GECODE_FLATZINC_EXPORT ObjectiveRelaxationGuided : public LnsHeuristic {
    std::shared_ptr<std::vector<int>> indices;
  public:
    ObjectiveRelaxationGuided(const FlatZincSpace &space, const std::vector<ConExpr*> &constraints);
    [[nodiscard]] std::shared_ptr<LnsHeuristic> clone() const override;
    void shrinkArrays(const std::map<int, int> &iv_new, const std::map<int, int> &bv_new, const std::map<int, int> &fv_new, const std::map<int, int> &sv_new) override;
    bool heuristic(const FlatZincSpace& incumbent, FlatZincSpace& next, const MetaInfo &mi, bool foundNewSolution) override;
    [[nodiscard]] bool applicable() const override;
  };

  class GECODE_FLATZINC_EXPORT LnsHeuristicCombinator : public LnsHeuristic {
    std::vector<std::shared_ptr<LnsHeuristic>> neighborhoods;
  public:
    explicit LnsHeuristicCombinator(std::vector<std::shared_ptr<LnsHeuristic>>&&);
    void shrinkArrays(const std::map<int, int> &iv_new, const std::map<int, int> &bv_new, const std::map<int, int> &fv_new, const std::map<int, int> &sv_new) override;
    [[nodiscard]] bool requires_cloning() const override;
    [[nodiscard]] std::shared_ptr<LnsHeuristic> clone() const override;
    bool heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next, const MetaInfo &mi, bool foundNewSolution) override;
    [[nodiscard]] bool applicable() const override;
  };

  class GECODE_FLATZINC_EXPORT Circuit : public LnsHeuristic {
      int offset;
      std::vector<int> vars;
    public:
      Circuit(int o, std::vector<int>&& v);
      void shrinkArrays(const std::map<int, int> &iv_new, const std::map<int, int> &bv_new, const std::map<int, int> &fv_new, const std::map<int, int> &sv_new) override;
      [[nodiscard]] std::shared_ptr<LnsHeuristic> clone() const override;
      bool heuristic(const FlatZincSpace& incumbent, FlatZincSpace& next, const MetaInfo &mi, bool foundNewSolution) override;
      [[nodiscard]] bool applicable() const override;
  };

  class GECODE_FLATZINC_EXPORT ScheduleUnary : public LnsHeuristic {
    std::vector<int> vars;
    std::vector<int> durations;
  public:
    ScheduleUnary(std::vector<int>&& tasks, std::vector<int>&& durs);
    void shrinkArrays(const std::map<int, int> &iv_new, const std::map<int, int> &bv_new, const std::map<int, int> &fv_new, const std::map<int, int> &sv_new) override;
    [[nodiscard]] std::shared_ptr<LnsHeuristic> clone() const override;
    bool heuristic(const FlatZincSpace& incumbent, FlatZincSpace& next, const MetaInfo &mi, bool foundNewSolution) override;
    [[nodiscard]] bool applicable() const override;
  };

}}

#endif

// STATISTICS: flatzinc-any
