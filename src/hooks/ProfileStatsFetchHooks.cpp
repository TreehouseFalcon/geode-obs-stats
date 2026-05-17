#include "../stats/CompletedDifficultyCounts.hpp"
#include "../obs/ObsTextSourceUpdates.hpp"

#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/modify/GameLevelManager.hpp>

#include <vector>
#include <string>

using namespace geode::prelude;

namespace {
    bool hasRequestedProfileStats = false;
    bool isWaitingForProfileStats = false;
    int requestedAccountID = 0;
    std::string requestedTag;

    void requestLoggedInProfileStats() {
        if (hasRequestedProfileStats) {
            return;
        }

        GJAccountManager* accountManager = GJAccountManager::sharedState();
        if (accountManager == nullptr || accountManager->m_accountID <= 0) {
            return;
        }

        GameLevelManager* levelManager = GameLevelManager::sharedState();
        if (levelManager == nullptr) {
            return;
        }

        hasRequestedProfileStats = true;
        isWaitingForProfileStats = true;
        requestedAccountID = accountManager->m_accountID;
        requestedTag = "account_" + std::to_string(requestedAccountID);

        log::info("requesting logged-in user profile stats for account {}", requestedAccountID);
        levelManager->getGJUserInfo(requestedAccountID);
    }

    void handleProfileStatsResponse(gd::string const& response, gd::string const& tag) {
        if (!isWaitingForProfileStats || tag != requestedTag) {
            return;
        }

        isWaitingForProfileStats = false;

        if (!obs_stats::CompletedDifficultyCounts::updateCountsFromProfileResponse(response)) {
            log::warn("logged-in profile stats response did not include parseable completed difficulty counts");
            return;
        }

        std::vector<obs_stats::StatPlaceholder> completedDifficultyPlaceholders;
        for (obs_stats::StatPlaceholder const& placeholder : obs_stats::statPlaceholders()) {
            if (obs_stats::selectStrategy(placeholder) == obs_stats::SelectStrategy::CompletedDifficulty) {
                completedDifficultyPlaceholders.push_back(placeholder);
            }
        }

        obs_stats::updateApplicableObsTextSources(completedDifficultyPlaceholders);
    }
}

$on_game(Loaded) {
    requestLoggedInProfileStats();
};

class $modify(ProfileStatsGameLevelManagerHook, GameLevelManager) {
    void onGetGJUserInfoCompleted(gd::string response, gd::string tag) {
        handleProfileStatsResponse(response, tag);
        GameLevelManager::onGetGJUserInfoCompleted(response, tag);
    }
};
