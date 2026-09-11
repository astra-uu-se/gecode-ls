#include <gecode/flatzinc/incumbent.hh>
#include <unordered_set>
#include <bits/random.h>
#include <random>

namespace Gecode { namespace FlatZinc {

    std::shared_ptr<const FlatZincSpace> IncumbentSolution::load_random() {
        _mutex.lock();
        if (_spaces.empty()) {
            return nullptr;
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> distribution(size_t{0}, _spaces.size() - 1);
        const size_t index = distribution(gen);
        auto space = _spaces[index];
        _mutex.unlock();
        return space;
    }

    std::optional<TupleSet> IncumbentSolution::load_tuple_sets() {
        _mutex.lock();
        if (_spaces.empty()) {
            _mutex.unlock();
            return {};
        }
        const int numVars = _spaces.front()->iv.size() + _spaces.front()->bv.size();
        TupleSet assignments(numVars);
        for (const auto & space : _spaces) {
            IntArgs t(numVars);
            for (int i = 0; i < space->iv.size(); ++i) {
                t[i] = space->iv[i].val();
            }
            for (int i = 0; i < space->bv.size(); ++i) {
                t[i + space->iv.size()] = space->bv[i].val();
            }
            assignments.add(t);
        }
        _mutex.unlock();
        assignments.finalize();
        return {assignments};
    }

    std::shared_ptr<const FlatZincSpace> IncumbentSolution::load() {
        _mutex.lock();
        auto space = _spaces.empty() ? nullptr : _spaces.front();
        _mutex.unlock();
        return space;
    }

    bool IncumbentSolution::compare_replace_strong(const std::shared_ptr<const FlatZincSpace> &expected_front,
        const std::shared_ptr<FlatZincSpace> &desired) {
        _mutex.lock();
        const bool ret = (_spaces.empty() && expected_front == nullptr) || _spaces.front() == expected_front;
        if (ret) {
            _spaces.clear();
            _spaces.emplace_back(desired);
        }
        _mutex.unlock();
        return ret;
    }

    bool IncumbentSolution::compare_enqueue_strong(const std::shared_ptr<const FlatZincSpace> &expected_front,
        const std::shared_ptr<FlatZincSpace> &desired) {
        _mutex.lock();
        bool in_spaces = true;
        for (const auto& space : _spaces)
        {
            for (int i = 0; i < std::min(space->bv.size(), desired->bv.size()); ++i)
            {
                if (space->bv[i].val() != desired->bv[i].val())
                {
                    in_spaces = false;
                    break;
                }
            }
            for (int i = 0; in_spaces && i < std::min(space->iv.size(), desired->iv.size()); ++i)
            {
                if (space->iv[i].val() != desired->iv[i].val())
                {
                    in_spaces = false;
                    break;
                }
            }
        }
        if (in_spaces)
        {
            _mutex.unlock();
            return false;
        }
        const bool ret = (_spaces.empty() && expected_front == nullptr) || _spaces.front() == expected_front;
        if (ret) {
            if (_spaces.size() >= _maxSize) {
                _spaces.pop_front();
            }
            _spaces.emplace_back(desired);
        }
        _mutex.unlock();
        return ret;
    }

    void IncumbentSolution::enqueue(const std::shared_ptr<FlatZincSpace> &desired) {
        _mutex.lock();
        if (_spaces.size() >= _maxSize) {
            _spaces.pop_front();
        }
        _spaces.emplace_back(desired);
        _mutex.unlock();
    }

    void IncumbentSolution::replace(const std::shared_ptr<FlatZincSpace> &desired) {
        _mutex.lock();
        _spaces.clear();
        _spaces.emplace_back(desired);
        _mutex.unlock();
    }

    bool IncumbentSolution::hasValue() const { return !_spaces.empty(); }
}}
