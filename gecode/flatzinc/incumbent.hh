// fzn-pbs.hh

#ifndef GECODE_FLATZINC_INCUMBENT
#define GECODE_FLATZINC_INCUMBENT

// Includes
#include <gecode/flatzinc.hh>
#include <gecode/flatzinc/branch.hh>

#include <memory>
#include <deque>
#include <optional>
#include <mutex>

namespace Gecode { namespace FlatZinc {
class GECODE_FLATZINC_EXPORT IncumbentSolution {
  std::mutex _mutex;
  static constexpr size_t _maxSize = 3;
  std::deque<std::shared_ptr<const FlatZincSpace>> _spaces;

public:
  IncumbentSolution() = default;

  std::shared_ptr<const FlatZincSpace> load_random();

  std::optional<TupleSet> load_tuple_sets();

  std::shared_ptr<const FlatZincSpace> load();

  bool compare_replace_strong(const std::shared_ptr<const FlatZincSpace>& expected_front, const std::shared_ptr<FlatZincSpace> &desired);

  bool compare_enqueue_strong(const std::shared_ptr<const FlatZincSpace>& expected_front, const std::shared_ptr<FlatZincSpace>& desired);

  void enqueue(const std::shared_ptr<FlatZincSpace>& desired);

  void replace(const std::shared_ptr<FlatZincSpace>& desired);

  [[nodiscard]] bool hasValue() const;
};

}}
#endif