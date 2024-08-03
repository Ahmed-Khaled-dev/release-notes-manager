/**
 * @file Config.cpp
 * @author Ahmed Khaled
 * @brief This file contains the implementation of the Config class
 */

#include <string>
#include <fstream>

#include <json.hpp>

#include "Config.h"

using namespace std;
using namespace nlohmann;

void Config::load(const string& configFileName) {
    ifstream externalConfigFile(configFileName);
    if (!externalConfigFile.is_open()) {
        throw runtime_error("Unable to open " + configFileName + ", please ensure that it exists in the same directory as the script");
    }

    json externalConfigData;
    try {
        externalConfigFile >> externalConfigData;
    }
    catch (json::parse_error& e) {
        throw runtime_error("JSON parsing error: " + string(e.what()));
    }

    if (externalConfigData.contains("markdownOutputFileName")) {
        markdownOutputFileName = externalConfigData["markdownOutputFileName"];

        if (markdownOutputFileName.find(".md") == string::npos) {
            throw invalid_argument("Key 'markdownOutputFileName' doesn't contain a correct value, enter a correct file name that ends in .md in "
                + configFileName);
        }
    }
    else {
        throw runtime_error("Key 'markdownOutputFileName' not found in " + configFileName);
    }

    if (externalConfigData.contains("htmlOutputFileName")) {
        htmlOutputFileName = externalConfigData["htmlOutputFileName"];

        if (htmlOutputFileName.find(".html") == string::npos) {
            throw invalid_argument("Key 'htmlOutputFileName' doesn't contain a correct value, enter a correct file name that ends in .html in "
                + configFileName);
        }
    }
    else {
        throw runtime_error("Key 'htmlOutputFileName' not found in " + configFileName);
    }

    if (externalConfigData.contains("githubReposApiUrl")) {
        githubReposApiUrl = externalConfigData["githubReposApiUrl"];
    }
    else {
        throw runtime_error("Key 'githubReposApiUrl' not found in " + configFileName);
    }

    if (externalConfigData.contains("githubMarkdownApiUrl")) {
        githubMarkdownApiUrl = externalConfigData["githubMarkdownApiUrl"];
    }
    else {
        throw runtime_error("Key 'githubMarkdownApiUrl' not found in " + configFileName);
    }

    if (externalConfigData.contains("githubUrl")) {
        githubUrl = externalConfigData["githubUrl"];
    }
    else {
        throw runtime_error("Key 'githubUrl' not found in " + configFileName);
    }

    if (externalConfigData.contains("commitTypesCount")) {
        commitTypesCount = externalConfigData["commitTypesCount"];

        if (commitTypesCount < 1) {
            throw invalid_argument("Key 'commitTypesCount' must contain a value bigger than 0 in " + configFileName);
        }
    }
    else {
        throw runtime_error("Key 'commitTypesCount' not found in " + configFileName);
    }

    if (externalConfigData.contains("commitMessagesSourceCliInputName")) {
        commitMessagesSourceCliInputName = externalConfigData["commitMessagesSourceCliInputName"];
    }
    else {
        throw runtime_error("Key 'commitMessagesSourceCliInputName' not found in " + configFileName);
    }

    if (externalConfigData.contains("commitMessagesSourceGithubActionsInputName")) {
        commitMessagesSourceGithubActionsInputName = externalConfigData["commitMessagesSourceGithubActionsInputName"];
    }
    else {
        throw runtime_error("Key 'commitMessagesSourceGithubActionsInputName' not found in " + configFileName);
    }

    if (externalConfigData.contains("pullRequestsSourceCliInputName")) {
        pullRequestsSourceCliInputName = externalConfigData["pullRequestsSourceCliInputName"];
    }
    else {
        throw runtime_error("Key 'pullRequestsSourceCliInputName' not found in " + configFileName);
    }

    if (externalConfigData.contains("pullRequestsSourceGithubActionsInputName")) {
        pullRequestsSourceGithubActionsInputName = externalConfigData["pullRequestsSourceGithubActionsInputName"];
    }
    else {
        throw runtime_error("Key 'pullRequestsSourceGithubActionsInputName' not found in " + configFileName);
    }

    if (externalConfigData.contains("shortModeCliInputName")) {
        shortModeCliInputName = externalConfigData["shortModeCliInputName"];
    }
    else {
        throw runtime_error("Key 'shortModeCliInputName' not found in " + configFileName);
    }

    if (externalConfigData.contains("shortModeGithubActionsInputName")) {
        shortModeGithubActionsInputName = externalConfigData["shortModeGithubActionsInputName"];
    }
    else {
        throw runtime_error("Key 'shortModeGithubActionsInputName' not found in " + configFileName);
    }

    if (externalConfigData.contains("fullModeCliInputName")) {
        fullModeCliInputName = externalConfigData["fullModeCliInputName"];
    }
    else {
        throw runtime_error("Key 'fullModeCliInputName' not found in " + configFileName);
    }

    if (externalConfigData.contains("fullModeGithubActionsInputName")) {
        fullModeGithubActionsInputName = externalConfigData["fullModeGithubActionsInputName"];
    }
    else {
        throw runtime_error("Key 'fullModeGithubActionsInputName' not found in " + configFileName);
    }

    if (externalConfigData.contains("singlePullRequestSourceCliInputName")) {
        singlePullRequestSourceCliInputName = externalConfigData["singlePullRequestSourceCliInputName"];  
    }
    else {
        throw runtime_error("Key 'singlePullRequestSourceCliInputName' not found in " + configFileName);
    }

    if (!externalConfigData.contains("commitTypes") || !externalConfigData["commitTypes"].is_array()) {
        throw runtime_error("Key 'commitTypes' not found or is not an array in " + configFileName);
    }

    auto& commitTypesArray = externalConfigData["commitTypes"];

    if (commitTypesCount != commitTypesArray.size()) {
        throw runtime_error("'commitTypesCount' does not match the size of the 'commitTypes' array in " + configFileName);
    }

    for (int i = 0; i < commitTypesCount; i++)
    {
        if (!commitTypesArray[i].contains("conventionalType")) {
            throw runtime_error("Missing 'conventionalType' in commitTypes array at index " + to_string(i) + " (0-based) in " + configFileName);
        }
        else if (!commitTypesArray[i].contains("markdownTitle")) {
            throw runtime_error("Missing 'markdownTitle' in commitTypes array at index " + to_string(i) + " (0-based) in " + configFileName);
        }

        commitTypes[i][0] = commitTypesArray[i]["conventionalType"];
        commitTypes[i][1] = commitTypesArray[i]["markdownTitle"];
    }

    if (externalConfigData.contains("markdownReleaseNotePrefix")) {
        markdownReleaseNotePrefix = externalConfigData["markdownReleaseNotePrefix"];
    }
    else {
        throw runtime_error("Key 'markdownReleaseNotePrefix' not found in " + configFileName);
    }

    if (externalConfigData.contains("markdownFullModeReleaseNotePrefix")) {
        markdownFullModeReleaseNotePrefix = externalConfigData["markdownFullModeReleaseNotePrefix"];
    }
    else {
        throw runtime_error("Key 'markdownFullModeReleaseNotePrefix' not found in " + configFileName);
    }

    if (externalConfigData.contains("outputMessages")) {
        auto& outputMessages = externalConfigData["outputMessages"];

        if (outputMessages.contains("noReleaseNotesSourceError")) {
            noReleaseNotesSourceError = outputMessages["noReleaseNotesSourceError"];
        }
        else {
            throw runtime_error("Key 'noReleaseNotesSourceError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("incorrectReleaseNotesSourceError")) {
            incorrectReleaseNotesSourceError = outputMessages["incorrectReleaseNotesSourceError"];
        }
        else {
            throw runtime_error("Key 'incorrectReleaseNotesSourceError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("noReleaseNotesModeError")) {
            noReleaseNotesModeError = outputMessages["noReleaseNotesModeError"];
        }
        else {
            throw runtime_error("Key 'noReleaseNotesModeError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("incorrectReleaseNotesModeError")) {
            incorrectReleaseNotesModeError = outputMessages["incorrectReleaseNotesModeError"];
        }
        else {
            throw runtime_error("Key 'incorrectReleaseNotesModeError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("noGithubTokenError")) {
            noGithubTokenError = outputMessages["noGithubTokenError"];
        }
        else {
            throw runtime_error("Key 'noGithubTokenError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("noReleaseStartReferenceError")) {
            noReleaseStartReferenceError = outputMessages["noReleaseStartReferenceError"];
        }
        else {
            throw runtime_error("Key 'noReleaseStartReferenceError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("noReleaseEndReferenceError")) {
            noReleaseEndReferenceError = outputMessages["noReleaseEndReferenceError"];
        }
        else {
            throw runtime_error("Key 'noReleaseEndReferenceError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("noPullRequestNumberError")) {
            noPullRequestNumberError = outputMessages["noPullRequestNumberError"];
        }
        else {
            throw runtime_error("Key 'noPullRequestNumberError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("noGithubRepositoryError")) {
            noGithubRepositoryError = outputMessages["noGithubRepositoryError"];
        }
        else {
            throw runtime_error("Key 'noGithubRepositoryError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("expectedSyntaxMessage")) {
            expectedSyntaxMessage = outputMessages["expectedSyntaxMessage"];
        }
        else {
            throw runtime_error("Key 'expectedSyntaxMessage' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("githubApiRateLimitExceededError")) {
            githubApiRateLimitExceededError = outputMessages["githubApiRateLimitExceededError"];
        }
        else {
            throw runtime_error("Key 'githubApiRateLimitExceededError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("githubApiUnauthorizedAccessError")) {
            githubApiUnauthorizedAccessError = outputMessages["githubApiUnauthorizedAccessError"];
        }
        else {
            throw runtime_error("Key 'githubApiUnauthorizedAccessError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("githubApiBadRequestError")) {
            githubApiBadRequestError = outputMessages["githubApiBadRequestError"];
        }
        else {
            throw runtime_error("Key 'githubApiBadRequestError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("githubApiUnableToMakeRequestError")) {
            githubApiUnableToMakeRequestError = outputMessages["githubApiUnableToMakeRequestError"];
        }
        else {
            throw runtime_error("Key 'githubApiUnableToMakeRequestError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("githubApiLibcurlError")) {
            githubApiLibcurlError = outputMessages["githubApiLibcurlError"];
        }
        else {
            throw runtime_error("Key 'githubApiLibcurlError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("gitLogError")) {
            gitLogError = outputMessages["gitLogError"];
        }
        else {
            throw runtime_error("Key 'gitLogError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("generatingReleaseNotesMessage")) {
            generatingReleaseNotesMessage = outputMessages["generatingReleaseNotesMessage"];
        }
        else {
            throw runtime_error("Key 'generatingReleaseNotesMessage' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("failedToGenerateReleaseNotesMessage")) {
            failedToGenerateReleaseNotesMessage = outputMessages["failedToGenerateReleaseNotesMessage"];
        }
        else {
            throw runtime_error("Key 'failedToGenerateReleaseNotesMessage' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("markdownFileError")) {
            markdownFileError = outputMessages["markdownFileError"];
        }
        else {
            throw runtime_error("Key 'markdownFileError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("htmlFileError")) {
            htmlFileError = outputMessages["htmlFileError"];
        }
        else {
            throw runtime_error("Key 'htmlFileError' not found in the 'outputMessages' category in " + configFileName);
        }

        if (outputMessages.contains("emptyReleaseNotesMessage")) {
            emptyReleaseNotesMessage = outputMessages["emptyReleaseNotesMessage"];
        }
        else {
            throw runtime_error("Key 'emptyReleaseNotesMessage' not found in the 'outputMessages' category in " + configFileName);
        }
    }
    else {
        throw runtime_error("Category 'outputMessages' not found in " + configFileName);
    }
}