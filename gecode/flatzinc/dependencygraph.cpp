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

#include <gecode/flatzinc/dependencygraph.hh>

#ifdef GECODE_HAS_SET_VARS
#include <gecode/set.hh>
#endif
#ifdef GECODE_HAS_FLOAT_VARS
#include <gecode/float.hh>
#endif
#include <numeric>
#include <gecode/flatzinc.hh>
#include <gecode/flatzinc/blackbox.hh>

namespace Gecode { namespace FlatZinc {

bool intersects(const std::set<AST::Node*>& a, const std::set<AST::Node*>& b) {
  std::set<AST::Node*>::const_iterator aIter = a.begin();
  std::set<AST::Node*>::const_iterator bIter = b.begin();
  while (aIter != a.end() && bIter != b.end()) {
    if (*aIter < *bIter) {
      ++aIter;
    } else if (*bIter < *aIter) {
      ++bIter;
    } else {
      return true;
    }
  }
  return false;
}

void
DependencyGraph::post(const FlatZincSpace& s, ConExpr const* ce) {
  for (unsigned i = 0; i < ce->size(); i++) {
    if (!s.sourcevars((*ce)[0])) {
      return;
    }
  }
  sources.emplace(ce);
  std::map<std::string,NEIGHBORHOOD_CONSTRAINT>::iterator i = r.find(ce->id);
  if (i != r.end()) {
    neighborhoods.emplace_back(ce);
  }
}

bool
DependencyGraph::isSource(ConExpr const* ce) const {
  return sources.find(ce) != sources.end();
}

void
DependencyGraph::add(const std::string& id, const NEIGHBORHOOD_CONSTRAINT p) {
  r[id] = p;
  r["gecode_" + id] = p;
  r["fzn_" + id] = p;
}

std::vector<ConExpr const*>
DependencyGraph::neighborhoodConstraints() {

  // Create scopes for all source constraints
  std::vector<std::set<AST::Node*>> scopes(neighborhoods.size());
  for (int n = 0; n < neighborhoods.size(); n++) {
    for (int i = 0; i < neighborhoods[n]->size(); i++) {
      AST::Node* arg = (*neighborhoods[n])[i];
      if (arg->isBoolVar() || arg->isIntVar()) {
        scopes[n].emplace(arg);
      } else if (arg->isArray()) {
        AST::Array* arr = arg->getArray();
        for (int j = 0; j < arr->a.size(); j++) {
          if (arr->a[j]->isBoolVar() || arr->a[j]->isIntVar()) {
            scopes[n].emplace(arr->a[j]);
          }
        }
      }
    }
  }

  std::vector<int> indices(neighborhoods.size());
  std::iota(indices.begin(), indices.end(), 0);

  // sort neighborhood constraints (indices) based on semantics and constraint scopes
  std::sort(indices.begin(), indices.end(), [&](const int index1, const int index2) {
    const int c = r[neighborhoods[index1]->id] - r[neighborhoods[index2]->id];
    if (c != 0) {
      return c > 0;
    }
    return scopes[index1].size() > scopes[index2].size();
  });

  std::set<AST::Node*> vars;
  std::vector<ConExpr const*> n;
  n.reserve(neighborhoods.size());
  for (int i = 0; i < indices.size(); i++) {
    const int index = indices[i];
    if (!intersects(vars, scopes[index])) {
      n.emplace_back(neighborhoods[index]);
      for (std::set<AST::Node*>::iterator iter = scopes[index].begin(); iter != scopes[index].end(); ++iter) {
        vars.emplace(*iter);
      }
    }
  }
  return n;
}

DependencyGraph::DependencyGraph() {
  add("gecode_circuit", NEIGHBORHOOD_CIRCUIT);
  add("gecode_schedule_unary", NEIGHBORHOOD_DISJUNCTIVE);
}

}}

// STATISTICS: flatzinc-any
