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

#include <gecode/flatzinc/lnsheuristicregistry.hh>
#include <gecode/flatzinc/lnsheuristics/lnsheuristics.hh>
#include <gecode/kernel.hh>
#include <gecode/int.hh>

#ifdef GECODE_HAS_SET_VARS
#include <gecode/set.hh>
#endif
#ifdef GECODE_HAS_FLOAT_VARS
#include <gecode/float.hh>
#endif
#include <gecode/flatzinc.hh>

namespace Gecode { namespace FlatZinc {

  LnsHeuristicRegistry& lnsHeuristicRegistry(void) {
    static LnsHeuristicRegistry r;
    return r;
  }

  std::shared_ptr<LnsHeuristic>
  LnsHeuristicRegistry::post(FlatZincSpace& s, const ConExpr& ce) {
    std::map<std::string,poster>::iterator i = r.find(ce.id);
    if (i == r.end()) {
      throw FlatZinc::Error("LnsHeuristic",
        std::string("Constraint ")+ce.id+" not found", ce.ann);
    }
    return i->second(s, ce, ce.ann);
  }

  void
  LnsHeuristicRegistry::add(const std::string& id, poster p) {
    r[id] = p;
    r["gecode_" + id] = p;
    r["fzn_" + id] = p;
  }

  namespace {

    std::shared_ptr<LnsHeuristic> p_schedule_unary(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      return std::make_shared<ScheduleUnary>(s.arg2intindices(ce[0]), s.arg2intvector(ce[1]));
    }

    std::shared_ptr<LnsHeuristic> p_circuit(FlatZincSpace& s, const ConExpr& ce, AST::Node*) {
      return std::make_shared<Circuit>(ce[0]->getInt(), s.arg2intindices(ce[1]));
    }

    class IntPoster {
    public:
      IntPoster(void) {

#ifndef GECODE_HAS_SET_VARS

#endif
        lnsHeuristicRegistry().add("gecode_schedule_unary", &p_schedule_unary);
        lnsHeuristicRegistry().add("gecode_circuit", &p_circuit);
      }
    };
    IntPoster __int_poster;

#ifdef GECODE_HAS_SET_VARS

    class SetPoster {
    public:
      SetPoster(void) {

      }
    };
    SetPoster __set_poster;
#endif

#ifdef GECODE_HAS_FLOAT_VARS

#ifdef GECODE_HAS_MPFR

#endif

    class FloatPoster {
    public:
      FloatPoster(void) {


#ifdef GECODE_HAS_MPFR

#endif
      }
    } __float_poster;
#endif

  }
}}

// STATISTICS: flatzinc-any
