/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.dev>
 *
 *  Contributing authors:
 *     Mikael Zayenz Lagerkvist <lagerkvist@gmail.com>
 *
 *  Copyright:
 *     Guido Tack, 2007
 *     Mikael Zayenz Lagerkvist, 2009
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

#include <random>
#include <gecode/flatzinc/lnsheuristicregistry.hh>
#include <gecode/flatzinc/lnsheuristics/lnsheuristics.hh>

#ifdef GECODE_HAS_SET_VARS
#include <gecode/set.hh>
#endif
#ifdef GECODE_HAS_FLOAT_VARS
#include <gecode/float.hh>
#endif
#include <deque>
#include <filesystem>
#include <random>
#include <unordered_set>
#include <gecode/flatzinc.hh>

namespace Gecode { namespace FlatZinc {

void freezeBool(const FlatZincSpace& incumbent, FlatZincSpace& next, const int index) {
    rel(next,
      next.bv_lns[index],
      IRT_EQ,
      incumbent.bv_lns[index].val());
}

void freezeInt(const FlatZincSpace& incumbent, FlatZincSpace& next, const int index) {
    rel(next,
      next.iv_lns[index],
      IRT_EQ,
      incumbent.iv_lns[index].val());
}

unsigned int GenericHeuristic::numVars(const FlatZincSpace &space) const {
    return _dependencyCuration ? space.iv_source.size() + space.bv_source.size() :space.iv_lns.size() + space.bv_lns.size();
}

unsigned int GenericHeuristic::domainSize(const FlatZincSpace& next, const int index) const {
    if (isBoolVar(next, index)) {
        return next.bv_lns[varIndex(next, index)].size();
    }
    return next.iv_lns[varIndex(next, index)].size();
}

void GenericHeuristic::freeze(const FlatZincSpace& incumbent, FlatZincSpace& next, const int index) const {
    if (isBoolVar(next, index)) {
        return freezeBool(incumbent, next, varIndex(next, index));
    }
    return freezeInt(incumbent, next, varIndex(next, index));
}

int randInInterval(const int lowInc, const int upExc, Rnd& random) {
    return lowInc + random(upExc - lowInc);
}

int lexBound(const FlatZincSpace& s) {
    assert(s.optVar() >= 0);
    assert(s.optVarIsInt());
    return s.method() == FlatZincSpace::MAX ? s.iv_lns[s.optVar()].min() : -s.iv_lns[s.optVar()].max();
}

std::vector<int> GenericHeuristic::createIndices(const FlatZincSpace &next) const {
    std::vector<int> indices(numVars(next));
    std::iota(indices.begin(), indices.end(), 0);
    return indices;
}

bool GenericHeuristic::isIntVar(const FlatZincSpace &next, const int index) const {
    return _dependencyCuration ? (0 <= index && index < next.iv_source.size()) : (0 <= index && index < next.iv_lns.size());
}

bool GenericHeuristic::isBoolVar(const FlatZincSpace& next, const int index) const {
    return _dependencyCuration ? (0 <= index - next.iv_source.size() && index - next.iv_source.size() < next.bv_source.size()) : (next.iv_lns.size() <= index && index - next.iv_lns.size() < next.bv_lns.size());
}

int GenericHeuristic::varIndex(const FlatZincSpace &next, const int index) const {
    if (_dependencyCuration) {
        return isIntVar(next, index) ? index : index - next.iv_source.size();
    }
    return isIntVar(next, index) ? index : index - next.iv_lns.size();
}

bool GenericHeuristic::isAssigned(const FlatZincSpace &next, const int index) const {
    if (_dependencyCuration) {
        return isIntVar(next, index) ? next.iv_source[varIndex(next, index)].assigned() : next.bv_source[varIndex(next, index)].assigned();
    }
    return isIntVar(next, index) ? next.iv_lns[varIndex(next, index)].assigned() : next.bv_lns[varIndex(next, index)].assigned();
}

GenericHeuristic::GenericHeuristic(const FlatZincSpace& space, const bool dependencyCuration) : _dependencyCuration(dependencyCuration) {
    if (dependencyCuration) {
        const unsigned int num_vars = space.iv_lns.size() + space.bv_lns.size() - (space.optVarIsInt() && space.optVar() > 0 ? 1 : 0);
        const unsigned int num_source = space.iv_source.size() + space.bv_source.size();
        if (num_vars == num_source) {
            _applicable = false;
            return;
        }
    }
    _applicable = true;
}

bool GenericHeuristic::dependencyCuration() const {
    return _dependencyCuration;
}

bool GenericHeuristic::applicable() const {
    return _applicable;
}

NaiveRandom::NaiveRandom(const FlatZincSpace &space, const bool dependencyCuration) : GenericHeuristic(space, dependencyCuration) {}

std::shared_ptr<LnsHeuristic> NaiveRandom::clone() const {return std::make_shared<NaiveRandom>(*this);}

bool NaiveRandom::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next, const MetaInfo &mi, bool) {
    unsigned int frozen = 0;
    for (int i = 0; i < numVars(next); ++i) {
        if (next.random(99U) <= next.freezePercent()) {
            freeze(incumbent, next, i);
            ++frozen;
        }
    }
    return false;
}


PropagationGuided::PropagationGuided(const FlatZincSpace &space, const bool dependencyCuration) : GenericHeuristic(space, dependencyCuration){
}

std::shared_ptr<LnsHeuristic> PropagationGuided::clone() const {
    return std::make_shared<PropagationGuided>(*this);
}

bool PropagationGuided::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next, const MetaInfo &mi, bool foundNewSolution) {
  std::vector<int> indices = createIndices(next);
  const size_t limit = floor(static_cast<double>(indices.size()) * (static_cast<double>(next.freezePercent()) / 100.0));
  size_t vars_frozen = 0;
  // Set up the variables for the propagation guided LNS.
  std::deque<unsigned int> queue;
  std::vector<bool> inQueue(indices.size(), false);
  std::vector<int> domainDifferences(indices.size());

  while (!indices.empty() && (vars_frozen < limit || (indices.size() + vars_frozen < indices.size()))) {
    const unsigned int index_pos = queue.empty()
      ? next.random(static_cast<int>(indices.size()))
      : queue.front();
    const int indexFreezeVar = indices[index_pos];
    std::swap(indices[index_pos], indices.back());
    indices.pop_back();
    if (!queue.empty()) {
      inQueue[queue.front()] = false;
      queue.pop_front();
    }
    if (isAssigned(next, indexFreezeVar)) {
      continue;
    }
    // Get the domain size before the propagation
    for (const int index : indices) {
      assert(indexFreezeVar != index);
      domainDifferences[index] = static_cast<int>(domainSize(next, index));
    }
    // Force value accordingly, and propagate.
    freeze(incumbent, next, indexFreezeVar);
    ++vars_frozen;
    next.status();

    // Add the variables that were propagated to pglns_info.
    for (const int index : indices) {
      domainDifferences[index] -= static_cast<int>(domainSize(next, index));
      assert(indexFreezeVar != index);
      if (!inQueue[index] && domainDifferences[index] > 0 && queue.size() < queue_size && !isAssigned(next, index)) {
        queue.push_back(index);
        inQueue[index] = true;
      }
    }

    // Sort the variables and indices in non_fzn_introduced_vars according to the difference in domain size in pglns_info.
    std::sort(queue.begin(), queue.end(), [&domainDifferences](const unsigned int& i1, const unsigned int& i2) {
      return domainDifferences[i1] > domainDifferences[i2];
    });
  }
    return false;
}

ReversePropagationGuided::ReversePropagationGuided(const FlatZincSpace &space, bool dependencyCuration) : GenericHeuristic(space, dependencyCuration) {
}

std::shared_ptr<LnsHeuristic> ReversePropagationGuided::clone() const {
    return std::make_shared<ReversePropagationGuided>(*this);
}

bool ReversePropagationGuided::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next, const MetaInfo &mi, bool foundNewSolution) {
  std::vector<int> indices = createIndices(next);
  const int limit = floor(static_cast<double>(indices.size()) * (static_cast<double>(next.freezePercent()) / 100.0));
  size_t vars_frozen = 0;
  // Set up the variables for the propagation guided LNS.
  std::deque<unsigned int> queue;
  std::vector<bool> inQueue(indices.size(), false);
  std::vector<int> domainDifferences(indices.size());
  std::iota(indices.begin(), indices.end(), 0);

  while (!indices.empty() && (vars_frozen < limit || (indices.size() + vars_frozen < indices.size()))) {
        const unsigned int index_pos = queue.empty()
              ? next.random(static_cast<int>(indices.size()))
              : queue.front();
        const int indexFreezeVar = indices[index_pos];

        std::swap(indices[index_pos], indices.back());
        indices.pop_back();
        if (!queue.empty()) {
              inQueue[queue.front()] = false;
              queue.pop_front();
        }
        if (isAssigned(next, indexFreezeVar)) {
            continue;
        }
        // Get the domain size before the propagation
        for (const int index : indices) {
            domainDifferences[index] = static_cast<int>(domainSize(next, index));
        }
        // Force value accordingly, and propagate.
        freeze(incumbent, next, indexFreezeVar);
        vars_frozen++;
        next.status();

        // Add the variables that were propagated to the queue.
        for (const int index : indices) {
              domainDifferences[index] -= static_cast<int>(domainSize(next, index));
              // avg_propagation /= indices.size();
              if (!inQueue[index] && domainDifferences[index] > 0 && queue.size() < queue_size && !isAssigned(next, index)) {
                    queue.emplace_back(index);
                    inQueue[index] = true;
              }
        }

        // Sort the variables and indexes in non_fzn_introduced_vars according to the difference in domain size in pglns_info.
        std::sort(queue.begin(), queue.end(), [&domainDifferences](const unsigned int& i1, const unsigned int& i2) {
            return domainDifferences[i1] < domainDifferences[i2];
        });
  }
  return false;
}

bool CostImpactGuided::requires_cloning() const {
    return true;
}

std::shared_ptr<LnsHeuristic> CostImpactGuided::clone() const {
    return std::make_shared<CostImpactGuided>(*this);
}

CostImpactGuided::CostImpactGuided(const FlatZincSpace& space, const bool dependencyCuration) : GenericHeuristic(space, dependencyCuration) {

}

bool CostImpactGuided::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next, const MetaInfo &mi, bool foundNewSolution) {
    // Use a vector of indices, select variables from it.
    std::vector<int> indices = createIndices(next);

    // Note that we are constructing the set of variables that are relaxed, not frozen.

    // Update scores and r every 10th restart or every time a better solution is found.
    if (info.bound_differences.size() != indices.size() || info.scores.size() != indices.size() || mi.restart() % 10 == 0 || foundNewSolution) {
        info.bound_differences.clear();
        info.bound_differences.resize(indices.size(), 0.0);
        info.scores.clear();
        info.scores.resize(indices.size());
        info.bound_diff_sum = 0.0;
        info.r = 0.0;

        for (unsigned int dive = 0; dive < dives; dive++){
              // Clone the space to make a dive possible.
              auto* fzs_clone = dynamic_cast<FlatZinc::FlatZincSpace*>(next.clone());
              // Create uniformly randomized permutations of the variables.

              // Depending on opt method, calculate the bound differences after fixing the variables.
              for (int i = 0; i < static_cast<int>(indices.size()); ++i){
                    std::swap(indices[i], indices[i + next.random(static_cast<int>(indices.size()) - i)]);

                    const int oldBound = lexBound(*fzs_clone);
                    // The variables stored in vars.intVar are those variables found in iv_lns_default.
                    const int index = indices[i];
                    freeze(incumbent, *fzs_clone, index);
                    fzs_clone->status();

                    const int newBound = lexBound(*fzs_clone);
                    // Corresponds to (3) in the paper:
                    // if minimising, then newBound >= oldBound. If newBound is high, then impact is high
                    // else, maximising and oldBound >= newBound. If newBound is low, then impact is high
                    const int impact = std::abs(newBound - oldBound);

                    info.bound_differences[index] += static_cast<double>(impact);
                    info.bound_diff_sum += static_cast<double>(impact);
              }
              delete fzs_clone;
        }
        // Divide each element in bound differences by dives.
        for (double & bound_difference : info.bound_differences){
              // corresponds to (6) in the paper:
              bound_difference /= dives;
        }
        // corresponds to (6) in the paper:
        info.bound_diff_sum /= dives;

        // Compute the score for each variable.
        for (size_t i = 0; i < info.bound_differences.size(); ++i){
              // corresponds to (7) in the paper:
              const double score = (alpha * info.bound_differences[i]) +
                                   ((1 - alpha) * static_cast<double>(info.bound_differences.size()) * info.bound_diff_sum);
              info.r += score;
              info.scores[i] = score;
        }
      }

      const double relaxFactor = static_cast<double>(100 - next.freezePercent()) / 100.0;
      const size_t numVarsToRelax = std::max<size_t>(1,
            ceil(relaxFactor * static_cast<double>(indices.size())));

      // The following it based on Algorithm 1 from the paper:
      // Select the variables to relax.
      double r_local = info.r;
      size_t numRelaxed = 0;

      for (numRelaxed = 0; !indices.empty() && numRelaxed < numVarsToRelax; ++numRelaxed) {
            double v = r_local <= 0
                     ? 0
                     : next.random(static_cast<int>(floor(r_local)));

            int best_index = -1;
            double best_score = 0;
            for (int i = 0; i < static_cast<int>(indices.size()); ++i) {
                  std::swap(indices[i], indices[i + next.random(static_cast<int>(indices.size()) - i)]);
                  const double score = info.scores[indices[i]];
                  v -= score;
                  if (v <= 0 || best_index < 0 || best_score < score) {
                        best_index = i;
                        best_score = score;
                        if (v <= 0) {
                        break;
                        }
                  }
            }
            r_local -= best_score;
            // More efficient than erasing the element, because vectors.
            std::swap(indices[best_index], indices.back());
            indices.pop_back();
        }

      // freeze the chosen variables.
      for (const int index : indices) {
            freeze(incumbent, next, index);
      }
      // Only return false if variables were relaxed.
      return false;
}

StaticVariableRelationGuided::StaticVariableRelationGuided(const FlatZincSpace &space, const bool dependencyCuration, const std::vector<ConExpr*>& constraints) :
    GenericHeuristic(space, dependencyCuration) {
    constexpr size_t num_types = 2;
    // For var[i] = <t, j>, create var_indices[t][j] = i
    std::array<std::unordered_map<int,int>,num_types> inverse_index;
    if (dependencyCuration) {
        var_indices.reserve(space.iv_source.size() + space.bv_source.size());
        for (auto j = 0; j < space.iv_lns.size(); ++j) {
            if ((!space.optVarIsInt() || space.optVar() != j) && !space.intIsFuncDep(j)) {
                inverse_index[0].emplace(j, var_indices.size());
                var_indices.emplace_back(j);
            }
        }
        for (auto j = 0; j < space.bv_lns.size(); ++j) {
            if (!space.boolIsFuncDep(j)) {
                inverse_index[1].emplace(j, var_indices.size());
                var_indices.emplace_back(j);
            }
        }
    } else {
        var_indices.resize(space.iv_lns.size() + space.bv_lns.size());
        std::iota(var_indices.begin(), var_indices.end(), 0);
        for (auto j = 0; j < space.iv_lns.size(); ++j) {
            inverse_index[0].emplace(j, j);
        }
        for (auto j = 0; j < space.bv_lns.size(); ++j) {
            inverse_index[1].emplace(j, space.iv_lns.size() + j);
        }
    }

    // constraint_input_vars[i] = the indices of variables in constraint i
    auto addConstraintInput = [&](AST::Node *node) {
        if (node->isIntVar()) {
            const int var_index = node->getIntVar();
            const auto iter = inverse_index[0].find(var_index);
            if (!space.iv_lns[var_index].assigned() && iter != inverse_index[1].end()) {
                constraint_arguments.back().emplace_back(iter->second);
            }
        } else if (node->isBoolVar()) {
            const int var_index = node->getBoolVar();
            const auto iter = inverse_index[1].find(var_index);
            if (!space.bv_lns[var_index].assigned() && iter != inverse_index[0].end()) {
                constraint_arguments.back().emplace_back(iter->second);
            }
        }
    };
    constraint_arguments.clear();
    constraint_arguments.reserve(constraints.size());
    for (const ConExpr* ce : constraints) {
        constraint_arguments.emplace_back();
        for (size_t i = 0; i < ce->args->a.size(); ++i) {
            if (ce->args->a[i]->isArray()) {
                auto arr = ce->args->a[i]->getArray();
                for (const auto & node : arr->a) {
                    addConstraintInput(node);
                }
            } else {
                addConstraintInput(ce->args->a[i]);
            }
        }
        if (constraint_arguments.back().empty()) {
            constraint_arguments.pop_back();
        } else {
            std::sort(constraint_arguments.back().begin(), constraint_arguments.back().end());
            constraint_arguments.back().erase(std::unique(constraint_arguments.back().begin(), constraint_arguments.back().end()), constraint_arguments.back().end());
        }
    }
    compute_variable_relations();
}

bool StaticVariableRelationGuided::requires_cloning() const {
    return true;
}

std::shared_ptr<LnsHeuristic> StaticVariableRelationGuided::clone() const {
    return std::make_shared<StaticVariableRelationGuided>(*this);
}

void StaticVariableRelationGuided::compute_variable_relations() {
    // variable_relations[i][j] = the relation between variables vars[i] and vars[j]
    variable_relations.clear();
    variable_relations.resize(var_indices.size(), std::vector<double>(var_indices.size(), 0.0));

    // The variable_relations matrix is used for Static Variable Dependency LNS asset
    // and contain the relations between the variables given the weights defined for each constraint.
    for (auto & arguments : constraint_arguments) {
        for (int i = 0; i + 1 < arguments.size(); ++i) {
            for (int j = i + 1; j < arguments.size(); ++j) {
                variable_relations.at(arguments[i]).at(arguments[j]) += 1.0 / static_cast<double>(arguments.size());
                variable_relations.at(arguments[j]).at(arguments[i]) += 1.0 / static_cast<double>(arguments.size());
            }
        }
    }

    // num_constraints[i] = number of non-unary constraints vars[i] occurs in:
    std::vector<int> num_constraints(var_indices.size(), 0);
    for (const auto & arguments : constraint_arguments) {
        if (arguments.size() <= 1) {
            continue;
        }
        for (const int var_index : arguments) {
            ++num_constraints[var_index];
        }
    }

    for (int i = 0; i < num_constraints.size(); ++i) {
        if (num_constraints[i] == 0) {
            continue;
        }
        for (int j = 0; j < num_constraints.size(); ++j) {
            variable_relations.at(i).at(j) /= static_cast<double>(num_constraints[i]);
        }
    }
    variable_impacts.clear();
    variable_impacts.resize(num_constraints.size(), 0);
}

int StaticVariableRelationGuided::selectRandomBestIndex(FlatZincSpace& next, std::vector<int>& indices, const int n) const {
    int best_impact = -1;
    int best_indices_index = -1;

    for (int i = 0; i < std::min<int>(n, static_cast<int>(indices.size())); ++i) {
        std::swap(indices[i], indices[i + next.random(static_cast<int>(indices.size()) - i)]);
        const int impact = variable_impacts.at(i);
        if (best_indices_index < 0 || best_impact < impact) {
            best_impact = impact;
            best_indices_index = i;
        }
    }

    return best_indices_index;
}

int StaticVariableRelationGuided::selectRandomRelatedIndex(FlatZincSpace& next, std::vector<int>& indices, const unsigned int best_var_index, const int n) const {
    // Given indices to relations, select variable with the best relations.
    int best_indices_index = -1;
    double best_relation = -1;
    for (int i = 0; i < std::min<int>(n, static_cast<int>(indices.size())); i++){
        std::swap(indices[i], indices[i + next.random(static_cast<int>(indices.size()) - i)]);
        const int other_var_index = indices[i];
        if (best_indices_index < 0 || variable_relations.at(best_var_index).at(other_var_index) > best_relation) {
            best_relation = variable_relations.at(best_var_index).at(other_var_index);
            best_indices_index = i;
        }
    }

    return best_indices_index;
}

void StaticVariableRelationGuided::freeze(const FlatZincSpace &incumbent, FlatZincSpace &next, const int index) const {
    if (index < next.iv_lns.size()) {
        rel(next, next.iv_lns[var_indices[index]], IRT_EQ, incumbent.iv_lns[var_indices[index]]);
    } else {
        rel(next, next.bv_lns[var_indices[index]], IRT_EQ, incumbent.bv_lns[var_indices[index]]);
    }
}

bool StaticVariableRelationGuided::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next, const MetaInfo &mi, bool foundNewSolution) {


    if (foundNewSolution || variable_impacts.size() < var_indices.size()) {
        variable_impacts.resize(var_indices.size());

        const int oldBound = lexBound(incumbent);
        for (int i = 0; i < var_indices.size(); ++i) {
            auto* fzs_clone = dynamic_cast<FlatZincSpace*>(next.clone());


            freeze(incumbent, *fzs_clone, i);
            fzs_clone->status();

            const int newBound = lexBound(*fzs_clone);
            const int impact = std::abs(newBound - oldBound);
            variable_impacts.at(i) = impact;
            delete fzs_clone;
        }
    }


    const double relaxFactor = static_cast<double>(100 - next.freezePercent()) / 100.0;
    const size_t numVarsToRelax = std::max<size_t>(1, static_cast<size_t>(ceil(relaxFactor * static_cast<double>(var_indices.size()))));
    const int n = 10 - static_cast<int>(round(5.0 * relaxFactor));

    std::vector<int> indices(var_indices.size(), 0);
    std::iota(indices.begin(), indices.end(), 0);
    int best_var_index = -1;
    for (int i = 0; i < numVarsToRelax && !indices.empty(); ++i) {
        // Select variable to relax:
        const int best_indices_index = i % 2 == 0
                                 ? selectRandomBestIndex(next, indices, n)
                                 : selectRandomRelatedIndex(next, indices, best_var_index, n);
        best_var_index = indices[best_indices_index];
        // remove best_index from indices
        indices[best_indices_index] = indices.back();
        indices.pop_back();
    }
    // freeze all non-relaxed variables:
    for (const int index : indices) {
        freeze(incumbent, next, index);
    }

    return false;
}

ObjectiveRelaxationGuided::ObjectiveRelaxationGuided(const FlatZincSpace &space, const std::vector<ConExpr *> &constraints) : indices(nullptr) {
    if (!space.optVarIsInt()) {
        return;
    }
    for (ConExpr const* ce : constraints) {
        // Ensure that the constraint is of type int_lin_eq and is defined var in compiled fzn file for the use of Objective Relaxation LNS.
        if (ce->id != "int_lin_eq" || ce->ann == nullptr || ce->ann->a.empty()) {
            continue;
        }
        bool defines_output = false;
        for (auto * annotation : ce->ann->a) {
            if (!annotation->isCall("defines_var")) {
                continue;
            }
            const auto& call = annotation->getCall("defines_var");
            const auto& varNode = call->args;
            defines_output = varNode != nullptr && varNode->isIntVar() && varNode->getIntVar() == space.optVar();
        }
        if (!defines_output) {
            return;
        }
        const auto& coef = ce->args->a[0]->getArray();
        const auto& vars = ce->args->a[1]->getArray();



        // Two different cases: All coefficients are similar or some coefficients are larger than others.
        // Loop starts at 1 since the first entry is the objective value itself, and freezing that variable breaks the point of the search.
        const double mean = std::accumulate(
            coef->a.begin()+1,
            coef->a.end(),
            0.0,
            [](const double acc, AST::Node* b) {
                return acc + std::abs(b->getInt());
                }
            ) / static_cast<double>(coef->a.size()-1);
        const double sq_sum = std::accumulate(
            coef->a.begin()+1,
            coef->a.end(),
            0.0,
            [](const double sum, AST::Node* b) {
                const int val = b->getInt();
                return sum + val * val;
            });
        const double stddev = std::sqrt((sq_sum / static_cast<double>(coef->a.size()-1)) - (mean * mean));

        // Case 1: coefficients are similar (a standard deviation smaller than 1)
        // Case 2: Some coefficients are larger than other, keep those non-fixed and make those with smaller mean freezeable, to relax the objective.
        indices = std::make_shared<std::vector<int>>();
        indices->reserve(vars->a.size() - 1);
        for (size_t i = 0; i < vars->a.size(); i++) {
            if (vars->a[i]->getIntVar() != space.optVar() && vars->a[i]->isIntVar() && !space.iv_lns[vars->a[i]->getIntVar()].assigned() && (stddev < 1 || coef->a[i]->getInt() < mean)) {
                indices->emplace_back(vars->a[i]->getIntVar());
            }
        }
    }
}

std::shared_ptr<LnsHeuristic> ObjectiveRelaxationGuided::clone() const {
    return std::make_shared<ObjectiveRelaxationGuided>(*this);
}

bool ObjectiveRelaxationGuided::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next, const MetaInfo &,
                                          bool) {
    for (const int index : *indices) {
        if (next.random(99U) <= next.freezePercent()) {
            freezeInt(incumbent, next, index);
        }
    }
    return false;
}

bool ObjectiveRelaxationGuided::applicable() const {
    return indices != nullptr && !indices->empty();
}

LnsHeuristicCombinator::LnsHeuristicCombinator(std::vector<std::shared_ptr<LnsHeuristic>>&& n)
    : neighborhoods(std::move(n)) {}

bool LnsHeuristicCombinator::requires_cloning() const {
    return std::any_of(neighborhoods.begin(), neighborhoods.end(), [&](const std::shared_ptr<LnsHeuristic>& neighbor) {
        return neighbor->requires_cloning();
    });
}

std::shared_ptr<LnsHeuristic> LnsHeuristicCombinator::clone() const {
    std::vector<std::shared_ptr<LnsHeuristic>> cloned(neighborhoods.size(), nullptr);
    for (size_t i = 0; i < neighborhoods.size(); ++i) {
        if (neighborhoods.at(i)->requires_cloning()) {
            cloned.at(i) = neighborhoods.at(i)->clone();
        } else {
            cloned.at(i) = neighborhoods.at(i);
        }
    }
    return std::make_shared<LnsHeuristicCombinator>(std::move(cloned));
}

bool LnsHeuristicCombinator::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next, const MetaInfo &mi, bool foundNewSolution) {
    bool allDone = true;
    for (const auto & neighborhood : neighborhoods) {
        allDone &= neighborhood->heuristic(incumbent, next, mi, foundNewSolution);
    }
    return allDone;
}

bool LnsHeuristicCombinator::applicable() const {
    return !neighborhoods.empty();
}

Circuit::Circuit(const int o, std::vector<int>&& v) :
    offset(o),
    source_vars(std::move(v)){}

std::shared_ptr<LnsHeuristic> Circuit::clone() const {
    return std::make_shared<Circuit>(*this);
}

bool Circuit::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next, const MetaInfo &mi, bool foundNewSolution) {
    const int average  = static_cast<int>(static_cast<double>(source_vars.size() * next.freezePercent()) / 100.0);
    std::poisson_distribution<int> distribution(average);

    std::mt19937 gen(next.random(std::numeric_limits<int>::max()));
    const int numThawed = std::clamp<int>(distribution(gen), 2, static_cast<int>(source_vars.size()));
    const int startIndex = next.random(static_cast<int>(source_vars.size()) - numThawed);
    int remaining = 0;

    int index = 0;
    for (int iterations = 0; iterations < source_vars.size(); ++iterations) {
        assert(index >= 0);
        assert(index < source_vars.size());
        if (index == startIndex) {
            remaining = numThawed;
        }
        const int successorVal = incumbent.iv_source[source_vars[index]].val();
        assert(successorVal >= offset);
        assert(successorVal - offset < source_vars.size());
        if (remaining <= 0) {
            rel(next, next.iv_source[source_vars[index]], IRT_EQ, successorVal);
        } else {
            --remaining;
        }
        index = successorVal - offset;
    }
    return false;
}

bool Circuit::applicable() const {
    return true;
}

ScheduleUnary::ScheduleUnary(std::vector<int>&& tasks, std::vector<int>&& durs) :
    source_vars(std::move(tasks)),
    durations(std::move(durs)) {}

std::shared_ptr<LnsHeuristic> ScheduleUnary::clone() const {
    return std::make_shared<ScheduleUnary>(*this);
}

bool ScheduleUnary::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next, const MetaInfo &mi, bool foundNewSolution) {
    const int average  = static_cast<int>(static_cast<double>(source_vars.size() * next.freezePercent()) / 100.0);
    std::poisson_distribution<int> distribution(average);

    std::vector<int> chronological(source_vars.size());
    std::iota(chronological.begin(), chronological.end(), 0);

    std::sort(chronological.begin(), chronological.end(), [&](const int i, const int j) {
        return incumbent.iv_source[source_vars[i]].val() < incumbent.iv_source[source_vars[j]].val();
    });

    std::mt19937 gen(next.random(std::numeric_limits<int>::max()));
    const int numThawed = std::clamp<int>(distribution(gen), 2, static_cast<int>(source_vars.size()));
    const int begin = next.random(static_cast<int>(source_vars.size()) - numThawed);
    const int end = begin + numThawed;

    // Fix chronological order for tasks 0..begin
    for (int i = 0; i + 1 < begin; ++i) {
        const IntVar endTime = expr(next, next.iv_source[source_vars[chronological[i]]] + durations[chronological[i]]);
        rel(next, endTime, IRT_LQ, next.iv_source[source_vars[chronological[i + 1]]]);
    }
    // Fix chronological order for tasks end..vars.size()
    for (int i = end; i + 1 < source_vars.size(); ++i) {
        const IntVar endTime = expr(next, next.iv_source[source_vars[chronological[i]]] + durations[chronological[i]]);
        rel(next, endTime, IRT_LQ, next.iv_source[source_vars[chronological[i + 1]]]);
    }
    // Tasks in begin..end must start after chronological task begin-1 ends:
    if (begin > 0 && begin + 1 < end) {
        const IntVar preEndTime = expr(next, next.iv_source[source_vars[chronological[begin - 1]]] + durations[chronological[begin - 1]]);
        for (int i = begin; i < end; ++i) {
            rel(next, preEndTime, IRT_LQ, next.iv_source[source_vars[chronological[i]]]);
        }
    }
    // Tasks in begin..end must end before chronological task end ends:
    if (end < source_vars.size()) {
        for (int i = begin; i < end; ++i) {
            const IntVar endTime = expr(next, next.iv_source[source_vars[chronological[i]]] + durations[chronological[i]]);
            rel(next, endTime, IRT_LQ, next.iv_source[source_vars[chronological[end]]]);
        }
    }

    return false;
}

bool ScheduleUnary::applicable() const {
    return true;
}
}
}

// STATISTICS: flatzinc-any
