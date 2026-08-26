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
#include <random>
#include <gecode/flatzinc.hh>

namespace Gecode { namespace FlatZinc {
LnsHeuristicCombinator::LnsHeuristicCombinator(std::vector<std::shared_ptr<LnsHeuristic>>&& n)
    : neighborhoods(std::move(n)) {}

void LnsHeuristicCombinator::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next) {
    for (int i = 0; i < neighborhoods.size(); ++i) {
        neighborhoods[i]->heuristic(incumbent, next);
    }
}

bool LnsHeuristicCombinator::applicable(const FlatZincSpace &incumbent) const {
    return !neighborhoods.empty();
}

Circuit::Circuit(const int o, std::vector<int>&& v) :
    offset(o),
    vars(std::move(v)){}

void Circuit::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next) {
    const int average  = (vars.size() * next.freezePercent()) / 100.0;
    std::poisson_distribution<int> distribution(average);

    std::mt19937 gen(next.random(std::numeric_limits<int>::max()));
    const int numThawed = std::clamp<int>(distribution(gen), 2, vars.size());
    const int startIndex = next.random(vars.size() - numThawed);
    int remaining = 0;

    int index = 0;
    for (int iterations = 0; iterations < vars.size(); ++iterations) {
        if (index == startIndex) {
            remaining = numThawed;
        }
        const int successorVal = incumbent.iv[vars[index]].val();
        if (remaining <= 0) {
            rel(next, next.iv[vars[index]], IRT_EQ, successorVal);
        } else {
            --remaining;
        }
        index = successorVal - offset;
    }
}

bool Circuit::applicable(const FlatZincSpace &incumbent) const {
    return true;
}

ScheduleUnary::ScheduleUnary(std::vector<int>&& tasks, std::vector<int>&& durs) :
    vars(std::move(tasks)),
    durations(std::move(durs)) {}

void ScheduleUnary::heuristic(const FlatZincSpace &incumbent, FlatZincSpace &next) {
    const int average  = (vars.size() * next.freezePercent()) / 100.0;
    std::poisson_distribution<int> distribution(average);

    std::vector<int> chronological(vars.size());
    std::iota(chronological.begin(), chronological.end(), 0);

    std::sort(chronological.begin(), chronological.end(), [&](const int i, const int j) {
        return incumbent.iv[vars[i]].val() < incumbent.iv[vars[j]].val();
    });

    std::mt19937 gen(next.random(std::numeric_limits<int>::max()));
    const int numThawed = std::clamp<int>(distribution(gen), 2, vars.size());
    const int begin = next.random(vars.size() - numThawed);
    const int end = begin + numThawed;

    // Fix chronological order for tasks 0..begin
    for (int i = 0; i + 1 < begin; ++i) {
        const IntVar endTime = expr(next, next.iv[vars[chronological[i]]] + durations[chronological[i]]);
        rel(next, endTime, IRT_LQ, next.iv[vars[chronological[i + 1]]]);
    }
    // Tasks in begin..end must start after chronological task begin-1 ends:
    if (begin > 0 && begin + 1 < end) {
        const IntVar preEndTime = expr(next, next.iv[vars[chronological[begin - 1]]] + durations[chronological[begin - 1]]);
        for (int i = begin; i < end; ++i) {
            rel(next, preEndTime, IRT_LQ, next.iv[vars[chronological[i]]]);
        }
    }
    // Tasks in begin..end must end before chronological task end ends:
    if (end < vars.size()) {
        for (int i = begin; i < end; ++i) {
            const IntVar endTime = expr(next, next.iv[vars[chronological[i]]] + durations[chronological[i]]);
            rel(next, endTime, IRT_LQ, next.iv[vars[chronological[end]]]);
        }
    }
    // Fix chronological order for tasks end..vars.size()
    for (int i = end; i + 1 < vars.size(); ++i) {
        const IntVar endTime = expr(next, next.iv[vars[chronological[i]]] + durations[chronological[i]]);
        rel(next, endTime, IRT_LQ, next.iv[vars[chronological[i + 1]]]);
    }
}

bool ScheduleUnary::applicable(const FlatZincSpace &incumbent) const {
    return true;
}
}
}

// STATISTICS: flatzinc-any
