// fzn-pbs.cpp

// Includes
#include <iostream>
#include <fstream>
#include <gecode/flatzinc.hh>
#include <gecode/flatzinc/plugin.hh>
#include <gecode/flatzinc/branch.hh>
#include <gecode/search.hh>
#include <gecode/flatzinc/portfolio.hh>
#include <gecode/flatzinc/searchengine.hh>

#include <array>
#include <vector>
#include <string>
#include <limits>
#include <bits/random.h>
#include <random>

#include <gecode/flatzinc/incumbent.hh>

namespace Gecode { namespace FlatZinc {

SearchController::SearchController(FlatZinc::FlatZincSpace* flatZincSpace, std::ostream& out, const Printer& printer, const FlatZincOptionsStruct& flatZincOptions, Support::Timer& timerTotal)
    : _flatZincSpace(flatZincSpace),
      _ostream(out),
      _printer(printer),
      _flatZincOptions(flatZincOptions),
      _timerTotal(timerTotal),
      _method(flatZincSpace->method()),
      _runningThreads(std::atomic<int>{0}),
      _allBestSolutions(std::make_shared<std::vector<std::shared_ptr<Space>>>()) {
}

SearchController::~SearchController() = default;

void SearchController::thread_done() {
    if (_runningThreads.fetch_sub(1) == 1) {
        _executionDoneEvent.signal();
    }
}

bool SearchController::updateBestSolution(const std::shared_ptr<FlatZincSpace> &sol,
                                          const unsigned int asset_id) {
    // If the optimum was found, then stop there is no need to update the best solution.
    for (const auto& intVar : sol->iv) {
        assert(intVar.assigned());
    }

    _solutionMutex.lock();
    if (_optimumFound->load()) {
        _solutionMutex.unlock();
        return false;
    }

    const auto expected = _flatZincSpace->_incumbentSolution->load();
    const int sol_comp = expected == nullptr ? -1 : sol->compareObj(*expected);
    if (sol_comp <= 0) {
        // Critical Section
        const bool success = sol_comp < 0
            ? _flatZincSpace->_incumbentSolution->compare_replace_strong(expected, sol)
            : _flatZincSpace->_incumbentSolution->compare_enqueue_strong(expected, sol);

        if (!success)
        {
            _solutionMutex.unlock();
            return false;
        }

        if (sol_comp < 0) {
            _allBestSolutions->emplace_back(std::dynamic_pointer_cast<Gecode::Space>(sol));
            if (_flatZincOptions.allSolutions()) {
                sol->print(_ostream, _printer);
                _ostream << "----------" << std::endl;
            }
            if (sol->method() == FlatZincSpace::SAT) {
                _optimumFound->store(true);
            }
            if (asset_id < _assets.size()) {
                _finishedAsset = asset_id;
            }
        }
        if (sol_comp < 0) {
            if (sol->optVarIsInt() && sol->optVar() >= 0) {
                _ostream << "%% objective: " << sol->iv[sol->optVar()] << std::endl;
            }
        }
        if (asset_id < _assets.size()) {
            _assets[asset_id]->incrSolutions(1);
        }
    }
    _solutionMutex.unlock();

    return sol_comp < 0;
}

void SearchController::awaitRunnersCompleted() {
    _executionDoneEvent.wait();
}

// Print the statistics of the search.
void SearchController::solutionStatistics(BaseAsset* asset, Support::Timer& t_total, unsigned int finishedAsset) {
    // Space failed before assets was created and search started.
    StatusStatistics statusStatistics = asset->statusStatistics();
    const double t_solve = asset->solveTime();
    const double totalTime = (t_total.stop() / 1000.0);
    const double solveTime = (t_solve / 1000.0);
    const double initTime = totalTime - solveTime;
    if (finishedAsset == -1) {
        _ostream << std::endl
            << "%%%mzn-stat: initTime=" << initTime
            << std::endl;
        _ostream << "%%%mzn-stat: solveTime=" << solveTime
            << std::endl;
        _ostream << "%%%mzn-stat: solutions=" << _allBestSolutions->size()
            << std::endl;
        _ostream << "%%%mzn-stat: finished asset="
            << asset->assetTypeStr() << std::endl;
        _ostream << "%%%mzn-stat: variables="
            << (_flatZincSpace->iv.size() + _flatZincSpace->bv.size() + _flatZincSpace->sv.size()) << std::endl
            << "%%%mzn-stat: propagators=" << 0 << std::endl
            << "%%%mzn-stat: propagations=" << statusStatistics.propagate << std::endl
            << "%%%mzn-stat: nodes=" << 0 << std::endl
            << "%%%mzn-stat: failures=" << 1 << std::endl
            << "%%%mzn-stat: restarts=" << 0 << std::endl
            << "%%%mzn-stat: peakDepth=" << 0 << std::endl
            << "%%%mzn-stat-end" << std::endl
            << std::endl;
        return;
    }

    // Search was not unsatisfiable: Print statistics.
    unsigned int numPropagators = asset->numPropagators();
    Gecode::Search::Statistics stat = asset->engine()->statistics();
    if (_flatZincOptions.stat()) {
        const FlatZincSpace& fzs = asset->flatZincSpace();
        _ostream << std::endl
            << "%%%mzn-stat: initTime=" << initTime
            << std::endl;
        _ostream << "%%%mzn-stat: solveTime=" << solveTime
            << std::endl;
        _ostream << "%%%mzn-stat: solutions=" << _allBestSolutions->size()
            << std::endl;
        _ostream << "%%%mzn-stat: finished asset="
            << asset->assetTypeStr() << std::endl;
        _ostream << "%%%mzn-stat: variables="
            << (_flatZincSpace->iv.size() + _flatZincSpace->bv.size() + _flatZincSpace->sv.size()) << std::endl
            << "%%%mzn-stat: propagators=" << numPropagators << std::endl
            << "%%%mzn-stat: propagations=" << statusStatistics.propagate+stat.propagate << std::endl
            << "%%%mzn-stat: nodes=" << stat.node << std::endl
            << "%%%mzn-stat: failures=" << stat.fail << std::endl
            << "%%%mzn-stat: restarts=" << stat.restart << std::endl
            << "%%%mzn-stat: peakDepth=" << stat.depth << std::endl
            << "%%%mzn-stat-end" << std::endl
            << std::endl;

        for (long unsigned int a = 0; a < _assets.size(); a++) {
            if (static_cast<int>(a) == finishedAsset) {
                continue;
            }
            numPropagators = _assets[a]->numPropagators();
            if (_assets[a]->assetType() == AssetType::SHAVING) {
                _ostream << "%%%mzn-stat: unfinished asset="
                    << _assets[a]->assetTypeStr() << std::endl;
                _ostream << "%%%mzn-stat: propagators=" << numPropagators << std::endl
                    << "%%%mzn-stat: propagations=" << statusStatistics.propagate+stat.propagate << std::endl
                    << "%%%mzn-stat: foundFailures=" << _forbiddenLiterals.size() << std::endl
                    << "%%%mzn-stat-end" << std::endl
                    << std::endl;
                continue;
            }
            stat = _assets[a]->engine()->statistics();
            numPropagators = _assets[a]->numPropagators();
            _ostream << "%%%mzn-stat: unfinished asset="
                << _assets[a]->assetTypeStr() << std::endl;
            _ostream << "%%%mzn-stat: solutions=" << _assets[a]->numSolutions() << std::endl
                << "%%%mzn-stat: propagators=" << numPropagators << std::endl
                << "%%%mzn-stat: propagations=" << statusStatistics.propagate+stat.propagate << std::endl
                << "%%%mzn-stat: nodes=" << stat.node << std::endl
                << "%%%mzn-stat: failures=" << stat.fail << std::endl
                << "%%%mzn-stat: restarts=" << stat.restart << std::endl
                << "%%%mzn-stat: peakDepth=" << stat.depth << std::endl
                << "%%%mzn-stat-end" << std::endl
                << std::endl;
        }
    }
    else{
        _ostream << std::endl
            << "%%%mzn-stat: initTime=" << initTime
            << std::endl;
        _ostream << "%%%mzn-stat: solveTime=" << solveTime
            << std::endl;
        _ostream << "%%%mzn-stat: solutions=" << _allBestSolutions->size()
            << std::endl;
        _ostream << "%%%mzn-stat: finished asset="
            << asset->assetTypeStr() << std::endl;
        _ostream << "%%%mzn-stat: variables="
            << (_flatZincSpace->iv.size() + _flatZincSpace->bv.size() + _flatZincSpace->sv.size()) << std::endl
            << "%%%mzn-stat: propagators=" << numPropagators << std::endl
            << "%%%mzn-stat: propagations=" << statusStatistics.propagate+stat.propagate << std::endl
            << "%%%mzn-stat: nodes=" << stat.node << std::endl
            << "%%%mzn-stat: failures=" << stat.fail << std::endl
            << "%%%mzn-stat: restarts=" << stat.restart << std::endl
            << "%%%mzn-stat: peakDepth=" << stat.depth << std::endl
            << "%%%mzn-stat-end" << std::endl
            << std::endl;
    }
}

void SearchController::createBanditArmAsset(unsigned int assetId) {
    _assets[assetId] = (std::make_unique<BanditArmAsset>(*this, *_flatZincSpace, _flatZincOptions, assetId));
    _assets[assetId]->setAssetTypeStr("bandit arm asset");
}

void SearchController::createLnsAsset(unsigned int assetId, int lnsId) {
    _assets[assetId] = std::make_unique<LNSAsset>(*this, *_flatZincSpace, _flatZincOptions, assetId, lnsId);
    _assets[assetId]->setAssetTypeStr("LNS asset");
}

void SearchController::createAsset(AssetType asset, unsigned int assetId) {
    switch (asset)
    {
        case AssetType::SHAVING:
            _assets[assetId] = (std::make_unique<ShavingAsset>(*this, *_flatZincSpace, _flatZincOptions, assetId, 20, true, new LargestAFCVariableSorter()));
            _assets[assetId]->setAssetTypeStr("shaving asset");
            break;
        case AssetType::SYSTEMATIC_SEARCH:
            _assets[assetId] = (std::make_unique<DFSAsset>(*this, *_flatZincSpace, _flatZincOptions, assetId, 1));
            _assets[assetId]->setAssetTypeStr("bab asset");
            break;
        default:
            break;
    }
}

void SearchController::updateMultiArmedBandit() {
    _banditMutex.lock();
    ++_banditTimestamp;
    _bandit = std::make_unique<Bandit>(_flatZincSpace->numLnsHeuristics());
    _banditMutex.unlock();
}

void SearchController::createAssets(double initTime) {
    // Since the BAB asset that uses non failing propagators will finish almost immediately, an extra asset is created.
    const unsigned int numAssets = true ? _flatZincOptions.threads() : (_flatZincOptions.threads() <= 1 ? 2 : (_flatZincOptions.threads() + 1));

    // Vector of asset type and the number of threads to use for that asset type.
    std::array<std::pair<AssetType, bool>, 1> defaultCompleteTypes{std::pair<AssetType, bool>{AssetType::SYSTEMATIC_SEARCH, false}};

    const int numCompleteAssets = _flatZincOptions.systematic() ? 1 : 0;
    const int numLnsAssets = static_cast<int>(numAssets) - numCompleteAssets;
    const bool useShaving = false && numAssets - numCompleteAssets - numLnsAssets > 0;

    // Set array sizes indexed by the assets id.
    _assets.resize(numAssets);
    _assetSwappedEngine.resize(numAssets, false);
    _runningThreads = numAssets;

    updateMultiArmedBandit();

    // Create complete assets:
    int assetId = 0;
    for (const auto [completeAsset, useNonFailingPropagators] : defaultCompleteTypes) {
        if (assetId < numCompleteAssets || useNonFailingPropagators)
        {
            createAsset(completeAsset, assetId);
            ++assetId;
        }
    }
    for (int i = 0; i < numLnsAssets; ++i) {
        if (_flatZincOptions.mab()) {
            createBanditArmAsset(assetId);
        } else {
            createLnsAsset(assetId, i);
        }
        ++assetId;
    }
    if (useShaving) {
        createAsset(AssetType::SHAVING, assetId);
        ++assetId;
    }
    for (auto& asset : _assets) {
        asset->increaseSolveTime(initTime);
        asset->setStatusStatistics(_statusStatistics);
    }
}

// The controller that creates the workers and controls the searches.
void SearchController::run() {
    // run the assets.
    for (auto &asset : _assets) {
        asset->run();
    }
    awaitRunnersCompleted();

    // If the shaving asset finished, the problem is unsatisfiable.
    if (_finishedAsset < _assets.size() && _assets[_finishedAsset]->assetType() == AssetType::SHAVING) {
        _ostream << "=====UNSATISFIABLE=====" << std::endl;
    } else {
        // Print the best or final solution:
        const auto sol = _flatZincSpace->_incumbentSolution->load();
        // Not a guarantee that a solution is found and finished asset it set.
        // Use default user asset in case no solution was found.
        BaseEngine* se = _assets[_finishedAsset > _assets.size() ? 0 : _finishedAsset]->engine();

        if (sol != nullptr) {
            sol->print(_ostream, _printer);
            _ostream << "----------" << std::endl;
        }
        if (se && !se->stopped()) {
            if (sol) {
                _ostream << "==========" << std::endl;
            } else {
                _ostream << "=====UNSATISFIABLE=====" << std::endl;
            }
        }
        else if (sol == nullptr) {
            _ostream << "=====UNKNOWN=====" << std::endl;
        }
    }
    // If print Statistics:
    if (_finishedAsset < _assets.size() && _flatZincOptions.mode() == SM_STAT) {
        solutionStatistics(_assets[_finishedAsset].get(), _timerTotal, _finishedAsset);
    }
}
// ########################################################################
//                         Multi Armed Bandit below.
// ########################################################################
Bandit::Bandit(const unsigned int numArms, const double temperature) :
    _numArms(numArms),
    _weights(numArms, 1.0),
    _probabilities(numArms, 1.0/static_cast<double>(numArms)),
    _gen(),
    _temperature(temperature),
    _initialRoundRobin(numArms) {
    std::iota(_initialRoundRobin.begin(), _initialRoundRobin.end(), 0);
    std::shuffle(_initialRoundRobin.begin(), _initialRoundRobin.end(), _gen);
}

unsigned int Bandit::getArm() {
    if (!_initialRoundRobin.empty()) {
        const int arm = _initialRoundRobin.back();
        _initialRoundRobin.pop_back();
        return arm;
    }
    auto distro = std::discrete_distribution<int>(_probabilities.begin(), _probabilities.end());
    return distro(_gen);
}

void Bandit::updateReward(const unsigned int arm, const unsigned int wins) {
    assert(arm < _numArms);
    const double reward = std::tanh(wins); // sigmoid, maps to [0,1)

    for (size_t j = 0; j < _numArms; j++) {
        const double estimated_reward = j == arm ? reward/_probabilities[j] : 0;
        _weights[j] = _weights[j] * std::exp(_temperature * estimated_reward /static_cast<double>(_numArms));
    }

    double sum_weights = 0.0;
    for (size_t j = 0; j < _numArms; j++) {
        sum_weights += _weights[j];
    }
    for (size_t i = 0; i < _numArms; i++) {
        _probabilities[i] = (1.0-_temperature)*(_weights[i]/sum_weights) + _temperature/static_cast<double>(_numArms);
    }
}

// ########################################################################
//                         AssetExecutor below.
// ########################################################################
void AssetExecutor::runSearch() {

    StatusStatistics statusStatistics = asset->statusStatistics();
    // Start the search timer.
    Support::Timer t_solve;
    t_solve.start();
    if (asset->flatZincSpace().status(statusStatistics) == SS_FAILED) {
        control.thread_done();
        return;
    }
    asset->setNumPropagators(PropagatorGroup::all.size(asset->flatZincSpace()));
    asset->setStatusStatistics(statusStatistics);

    size_t round = 0;

    do {
        asset->updateBanditArm();
        // update engine with new timeout
        if (round > 0) {
            asset->updateTimeout();
        }
        BaseEngine* engine = asset->engine();

        // Run the search engine.
        assert(engine != nullptr);
        std::shared_ptr<FlatZincSpace> sol;
        bool solWasBestSol = false;

        while (auto nextSol = std::shared_ptr<FlatZincSpace>(engine->next())) {
            if (control._optimumFound->load()) {
                break;
            }
            // If last solution was not the current best solution, delete it.
            if (!solWasBestSol && sol != nullptr) {
                sol = nullptr;
            }
            sol = nextSol;

            solWasBestSol = control.updateBestSolution(sol, asset_id);
            if (solWasBestSol && sol->method() == FlatZincSpace::SAT) {
                break;
            }

            // Apply nq constraints to make asset take advantage of shaving.
            assert(control.get_forbidden_literals().empty());
            if (false) {
                std::vector<Literal> local_forbidden_literals = control.get_forbidden_literals();
                const size_t size = local_forbidden_literals.size();
                if (size > asset->shavingStart()) {
                    for (size_t i = asset->shavingStart(); i < size; i++) {
                        local_forbidden_literals[i].var.nq(&(asset->flatZincSpace()), local_forbidden_literals[i].value);
                    }
                }
            }

            /*
            if (asset->assetType() == AssetType::SYSTEMATIC_SEARCH && asset->useSelfSubsumingPropagators()) {
                asset->increaseSolveTime(t_solve.stop());
                control.thread_done();
                return;
            }
            */
        }
        ++round;
    } while (asset->runNextRound());
    // Stop the search timer.
    const double t = t_solve.stop();
    if (!asset->engine()->stopped() && asset->assetType() != AssetType::LOCAL_SEARCH) {
        control._optimumFound->store(true);
    }
    asset->increaseSolveTime(t);
    control.thread_done();
}

// Go through and run each asset in the round-robin for some fixed amount of restarts. Store the number of sols for each asset, best asset keeps on running until search finishes.
void RoundRobinLNSAsset::run() {
    int curBestObj = control._method == FlatZincSpace::MAX ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();

    const int optVar = _roundRobinAssets[0]->flatZincSpace().optVar();
    Support::Timer t_solve;

    for (long unsigned int i = 0; i < _roundRobinAssets.size(); i++) {
        // Do one run of the asset and decide which asset is the best.
        BaseEngine *se = _roundRobinAssets[i]->engine();
        t_solve.start();
        auto sol = std::shared_ptr<FlatZincSpace>(se->next());
        _roundRobinAssets[i]->increaseSolveTime(t_solve.stop());

        control.updateBestSolution(sol, _assetId);

        const int curObj = sol->iv[optVar].val();
        const bool isBetterSolution =
            control._method == FlatZincSpace::MAX ? curObj > curBestObj : curObj < curBestObj;
        if (isBetterSolution) {
            curBestObj = curObj;
            best_asset = std::move(_roundRobinAssets[i]);
        }

        if (_flatZincOptions.mode() == SM_STAT) {
            switch (best_asset->assetType())
            {
                case AssetType::SYSTEMATIC_SEARCH:
                    best_asset->setAssetTypeStr("round robin asset systematic search");
                    break;
                case AssetType::LOCAL_SEARCH:
                    best_asset->setAssetTypeStr("round robin asset lns");
                    break;
                case AssetType::SHAVING:
                    best_asset->setAssetTypeStr("round robin asset shaving");
                    break;
                case AssetType::DUMMY:
                    best_asset->setAssetTypeStr("round robin asset");
                    break;
            }
        }

        // If search is finished (solution has been found, then return (since we are done))
        if (control._optimumFound->load()) {
            // Select any asset as solution has already been found.
            best_asset = std::move(_roundRobinAssets[i]);
            control.thread_done();
            return;
        }
    }
    // Unless search has finished, use the best engine and perform actual search:
    if (!control._optimumFound->load()) {
        // If no solution was found.
        if (best_asset != nullptr) {
            double prev_solveTime = best_asset->solveTime();
            best_asset->increaseSolveTime(prev_solveTime);
            best_asset->run();
        } else {
            best_asset = std::move(_roundRobinAssets[0]);
            control.thread_done();
        }
    }
}

void AssetExecutor::runShaving() {
    // Cast asset to be a ShavingAsset.
    auto* shaving_asset = dynamic_cast<ShavingAsset*>(asset);

    StatusStatistics status_stat;
    const CloneStatistics clone_stat;

    bool has_reported_literal = false;



    Support::Timer t_solve;
    t_solve.start();
    if (asset->flatZincSpace().status(status_stat) != SS_FAILED) {
        asset->setNumPropagators(PropagatorGroup::all.size(asset->flatZincSpace()));
        asset->setStatusStatistics(status_stat);
    }

    // Shave bounds
    if (shaving_asset->doBoundsShaving()) {
        shaving_asset->runShavingPass(control, status_stat, clone_stat, has_reported_literal, [](VarDescription& vd, FlatZincSpace* s) {
            return vd.bounds_literals(s);
        });
    }
    else {
        shaving_asset->runShavingPass(control, status_stat, clone_stat, has_reported_literal, [shaving_asset](VarDescription& vd, FlatZincSpace* s) {
            if (vd.size(s) > static_cast<unsigned int>(shaving_asset->getMaxDomShavingSize())) {
                return std::vector<Literal>{};
            }
            return vd.domain_literals(s);
        });
    }

    asset->increaseSolveTime(t_solve.stop());
    asset->setStatusStatistics(status_stat);

    control.thread_done();
}

AssetExecutor::AssetExecutor(SearchController &control, BaseAsset *asset, const FlatZincOptionsStruct &fopt, unsigned int asset_id,
    bool do_search): control(control), asset(asset), fopt(fopt), p(control._printer), asset_id(asset_id), do_search(do_search) {}

// ########################################################################
//                         Assets Below.
// ########################################################################

BaseAsset::BaseAsset(FlatZincSpace &flatZincSpace, FlatZincSpace *curFlatZincSpace, const FlatZincOptionsStruct &flatZincOptions,
    unsigned int assetId, AssetType assetType):
_originalFlatZincSpace(flatZincSpace),
_curFlatZincSpace(curFlatZincSpace),
_flatZincOptions(flatZincOptions),
_assetType(assetType),
_assetId(assetId) {
    if (_curFlatZincSpace != nullptr) {
        _curFlatZincSpace->populateLnsVariables();
    }
}

std::shared_ptr<Search::Options> BaseAsset::generateSearchOptions(FlatZincSpace& originalFlatZincSpace, Search::Stop* stop) const {
    auto searchOptions = std::make_shared<Search::Options>();
    searchOptions->stop = stop;
    searchOptions->c_d = _flatZincOptions.c_d();
    searchOptions->a_d = _flatZincOptions.a_d();

#ifdef GECODE_HAS_FLOAT_VARS
    originalFlatZincSpace.step = _flatZincOptions.step();
#endif

    searchOptions->threads = numThreads();
    searchOptions->nogoods_limit = _flatZincOptions.nogoods() ? _flatZincOptions.nogoods_limit() : 0;

    auto* fznCutoff = Driver::createCutoff(_flatZincOptions);
    assert(searchOptions->cutoff == nullptr);
    if (fznCutoff != nullptr) {
        searchOptions->cutoff = new Search::CutoffAppend(new Search::CutoffConstant(0), 1, fznCutoff);
    } else {
        searchOptions->cutoff = new Search::CutoffConstant(3000);
    }

    if (_flatZincOptions.interrupt()) {
        Driver::CombinedStop::installCtrlHandler(true);
    }

    return searchOptions;
}

bool BaseAsset::runNextRound() {
    return false;
}

DFSAsset::DFSAsset(SearchController &searchController, FlatZincSpace& fg, const FlatZincOptionsStruct &fopt,
                   unsigned int assetId, unsigned int numThreads) :
BaseAsset(fg, fg.deepClone(), fopt, assetId, AssetType::SYSTEMATIC_SEARCH),
_searchController(searchController),
_numThreads(numThreads),
executor(new AssetExecutor(searchController, this, fopt, assetId, true)) {
    assert(_searchOptions == nullptr);
    _searchOptions = std::make_shared<Search::Options>();
    _searchOptions->c_d = _searchOptions->c_d;
    _searchOptions->a_d = _searchOptions->a_d;
    _searchOptions->threads = _numThreads;
    _searchOptions->stop = Driver::CombinedStop::create(
        0,
        0,
        _flatZincOptions.time(),
        0,
        true,
        _searchController._optimumFound);

    if (_flatZincOptions.interrupt()) {
        Driver::CombinedStop::installCtrlHandler(true);
    }

    if (searchController._method == FlatZincSpace::SAT)
    {
        _engine = new DFSEngine(_curFlatZincSpace, *_searchOptions);
    }
    else
    {
        _engine = new BABEngine(_curFlatZincSpace, *_searchOptions);
    }
}

LNSAsset::LNSAsset(SearchController &searchController, FlatZincSpace& fg, const FlatZincOptionsStruct &fopt,
    unsigned int assetId, unsigned int lnsNeighborhoodIndex, RestartMode restartMode, double restartBase, int restartScale)
: BaseAsset(fg, fg.deepClone(), fopt, assetId, AssetType::LOCAL_SEARCH),
_searchController(searchController),
_lnsNeighborhoodIndex(lnsNeighborhoodIndex),
_restartMode(restartMode),
_restartBase(restartBase),
_restartScale(restartScale),
executor(new AssetExecutor(searchController, this, fopt, assetId, true)) {

    if (_flatZincOptions.interrupt()) {
        Driver::CombinedStop::installCtrlHandler(true);
    }

    // if asset uses restart-based search:
    FlatZincOptionsStruct brancherOptions(_flatZincOptions);
    if (brancherOptions.restart() == RM_NONE) {
        brancherOptions.restart(_restartMode);
        brancherOptions.restart_base(_restartBase);
        brancherOptions.restart_scale(_restartScale);
    }

    assert(_searchOptions == nullptr);
    _searchOptions = generateSearchOptions(
        _originalFlatZincSpace,
        Driver::CombinedStop::create(
            brancherOptions.node(),
            brancherOptions.fail(), // this should be constant 3000
            brancherOptions.time(),
            brancherOptions.restart_limit(), // this should be 0
            true,
            searchController._optimumFound));

    _curFlatZincSpace->cloneLnsHeuristics();

    _engine = new RBSEngine(_curFlatZincSpace, *_searchOptions, searchController._optimumFound, searchController._allBestSolutions);
}

BanditArmAsset::BanditArmAsset(SearchController &searchController, FlatZincSpace& fg, const FlatZincOptionsStruct &fopt,
    unsigned int assetId, RestartMode restartMode, double restartBase, int restartScale)
: BaseAsset(fg, fg.deepClone(), fopt, assetId, AssetType::LOCAL_SEARCH),
_searchController(searchController),
_restartMode(restartMode),
_restartBase(restartBase),
_restartScale(restartScale),
executor(new AssetExecutor(searchController, this, fopt, assetId, true)),
_banditArm(std::numeric_limits<int>::max()),
heuristic(std::make_shared<int>(_banditArm)) {
    _timeout.start();

    if (_flatZincOptions.interrupt()) {
        Driver::CombinedStop::installCtrlHandler(true);
    }

    if (_flatZincOptions.restart() == RM_NONE) {
        _flatZincOptions.restart(_restartMode);
        _flatZincOptions.restart_base(_restartBase);
        _flatZincOptions.restart_scale(_restartScale);
    }

    const double timeout = std::min(defaultTime,  _flatZincOptions.time() - _timeout.stop());
    assert(_searchOptions == nullptr);
    _searchOptions = generateSearchOptions(
        _originalFlatZincSpace,
        Driver::CombinedStop::create(
            _flatZincOptions.node(),
            _flatZincOptions.fail(),
            timeout,
            _flatZincOptions.restart_limit(),
            true,
            _searchController._optimumFound));
    _curFlatZincSpace->cloneLnsHeuristics();
    _curFlatZincSpace->heuristic = heuristic;

    _engine = new RBSEngine(_curFlatZincSpace, *_searchOptions, _searchController._optimumFound, _searchController._allBestSolutions);
}

void BanditArmAsset::updateBanditArm() {
    // lock
    _searchController._banditMutex.lock();

    // update reward if bandit has not changed.
    if (_searchController.banditTimestamp() == _banditTimestamp) {
        _searchController._bandit->updateReward(_banditArm, _numCurSolutions);
    }
    // get new arm
    _banditArm = static_cast<int>(_searchController._bandit->getArm());
    // update local parameters
    _banditTimestamp = _searchController.banditTimestamp();
    // unlock
    _searchController._banditMutex.unlock();

    _numCurSolutions = 0;

    // update current FlatZincSpace:
    *heuristic = _banditArm;
}

bool BanditArmAsset::runNextRound() {
    if (auto const* s = dynamic_cast<Driver::CombinedStop*>(_searchOptions->stop)) {
        if (!s->done()) {
            return false;
        }
    }
    return !_searchController._optimumFound->load() && _timeout.stop() < _flatZincOptions.time();
}

void BanditArmAsset::updateTimeout() {
    assert(_searchOptions != nullptr);
    assert(_searchOptions->stop != nullptr);
    const double timeout = std::min(defaultTime,  _flatZincOptions.time() - _timeout.stop());
    if (auto* s = dynamic_cast<Driver::CombinedStop*>(_searchOptions->stop)) {
        s->update_time(timeout);
    }
}

RoundRobinLNSAsset::RoundRobinLNSAsset(SearchController &control, FlatZincSpace& fg, const FlatZincOptionsStruct &fopt,
                                       unsigned int asset_id) :
BaseAsset(fg, nullptr, fopt, asset_id, AssetType::DUMMY),
best_asset(nullptr), control(control) {
    // Fill the round_robin_assets vector with all types of LNS assets available.
    // Ordering of assets can help (Now following thesis results).
    for (int i = 0; i < fg.numLnsHeuristics(); ++i) {
        _roundRobinAssets.emplace_back(std::make_unique<LNSAsset>(control, _originalFlatZincSpace, _flatZincOptions,
            asset_id, i));
    }
}

ShavingAsset::ShavingAsset(SearchController &control, FlatZincSpace& fg, const FlatZincOptionsStruct &fopt,
    unsigned int assetId, int maxDomShavingSize,
    bool do_bounds_shaving, VariableSorter *sorter): BaseAsset(fg, fg.deepClone(), fopt, assetId, AssetType::SHAVING),
                                                     control(control),
                                                     _executor(new AssetExecutor(control, this, fopt, assetId, false)),
                                                     _maxDomShavingSize(maxDomShavingSize),
                                                     _doBoundsShaving(do_bounds_shaving),
                                                     _sorter(sorter) {
    std::reverse(_variables.begin(), _variables.end());
    for (int i = 0; i < _curFlatZincSpace->iv.size(); i++) {
        if (_curFlatZincSpace->iv[i].assigned()) {
            continue;
        }
        _variables.emplace_back(VarType::Int, FlatZincVarArray::iv, i);
    }
    for (int i = 0; i < _curFlatZincSpace->bv.size(); i++) {
        if (_curFlatZincSpace->bv[i].assigned()) {
            continue;
        }
        _variables.emplace_back(VarType::Bool, FlatZincVarArray::bv, i);
    }
}

void ShavingAsset::runShavingPass(SearchController& ctrl, StatusStatistics statisStatistics,
    CloneStatistics cloneStatistics, bool& hasReportedLiteral,
    const std::function<std::vector<Literal> (VarDescription&, FlatZincSpace*)> &literalExtractor) const {
    std::vector queue(_variables);
    _sorter->sort_variables(queue, _curFlatZincSpace);
    while (!queue.empty()) {
        if (ctrl._optimumFound->load()) {
            ctrl.thread_done();
            return;
        }
        auto vd = queue.back();
        queue.pop_back();

        for (auto literal : literalExtractor(vd, _curFlatZincSpace)) {
            if (ctrl._optimumFound->load()) {
                return;
            }
            const auto clone = dynamic_cast<FlatZincSpace*>(_curFlatZincSpace->clone(cloneStatistics));
            literal.var.eq(clone, literal.value);
            const auto status = clone->status(statisStatistics);
            delete clone;
            if (status != SS_FAILED) {
                continue;
            }
            ctrl.report_forbidden_literal(literal);
            hasReportedLiteral = true;
            literal.var.nq(_curFlatZincSpace, literal.value);
            auto root_status = _curFlatZincSpace->status(statisStatistics);
            // If variable can neither be equal or not equal, then the problem is unsatisfiable and we are done.
            if (root_status == SS_FAILED) {
                // The only way the non-search Shaving Asset can actually finish first is iff the problem is unsatisfiable and it is found.
                if (!ctrl._optimumFound->exchange(true)) {
                    ctrl._finishedAsset = _assetId;
                }
                return;
            }
        }

        _sorter->sort_variables(queue, _curFlatZincSpace);
    }
}
}}
