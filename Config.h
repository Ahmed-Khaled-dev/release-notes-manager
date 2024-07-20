/**
 * @file Config.h
 * @author Ahmed Khaled
 * @brief This file defines the Config class which is used for loading and validating the JSON external configuration values
 */
#pragma once

#include <string>

using namespace std;

/**
 * @brief A class for loading and validating the JSON external configuration values
 * This class allows the script to be easily customizable without opening any source code files
 */
class Config {
public:
    string markdownOutputFileName;
    string htmlOutputFileName;
    string githubRepoPullRequestsApiUrl;
    string githubMarkdownApiUrl;
    string repoIssuesUrl;
    string repoCommitsUrl;
    int commitTypesCount;
    /**
     * @brief 2d array storing conventional commit types and their corresponding markdown titles
     * The first dimension is 50 to give it enough space to store as many types as the user enters in the release_config.json
     */
    string commitTypes[50][2];
    // Variables that determine the syntax of running the script
    // To generate release notes directly from the CLI using commit messages as source
    // ./release_notes_generator commitMessagesSourceCliInputName release_start_reference release_end_reference github_token
    // To generate release notes from the inputs of the GitHub Actions workflow using commit messages as source
    // ./release_notes_generator commitMessagesSourceGithubActionsInputName release_start_reference release_end_reference github_token
    // Same thing with editing the syntax of running the script with pull requests as source and the syntax of the 2 modes related to it (short/full)
    string commitMessagesSourceCliInputName;
    string commitMessagesSourceGithubActionsInputName;
    string pullRequestsSourceCliInputName;
    string pullRequestsSourceGithubActionsInputName;
    string shortModeCliInputName;
    string shortModeGithubActionsInputName;
    string fullModeCliInputName;
    string fullModeGithubActionsInputName;

    // Variables that determine the looks of the release notes
    string markdownReleaseNotePrefix;
    string markdownFullModeReleaseNotePrefix;

    // Variables that control the output messages that are shown to the user
    string noReleaseNotesSourceError;
    string incorrectReleaseNotesSourceError;
    string noReleaseNotesModeError;
    string incorrectReleaseNotesModeError;
    string noGithubTokenError;
    string noReleaseStartReferenceError;
    string noReleaseEndReferenceError;
    string githubApiRateLimitExceededError;
    string githubApiUnauthorizedAccessError;
    string githubApiBadRequestError;
    string githubApiUnableToMakeRequestError;
    string githubApiLibcurlError;
    string gitLogError;
    string markdownFileError;
    string htmlFileError;
    string expectedSyntaxMessage;
    string generatingReleaseNotesMessage;
    string failedToGenerateReleaseNotesMessage;
    string emptyReleaseNotesMessage;

    void load(const string& configFileName);
};