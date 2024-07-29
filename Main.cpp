/**
 * @file Main.cpp
 * @author Ahmed Khaled
 * @brief This file contains the main functions related to generating release notes from commit messages or pull requests
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <regex>

#include <curl/curl.h> // Used to make API requests
#include <json.hpp>

#include "Config.h"
#include "Enums.h"
#include "Utils.h"
#include "Format.h"

// To allow for code compatibility between different compilers/OS (Microsoft's Visual C++ compiler and GCC)
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

using namespace std;
using namespace nlohmann;

void addPullRequestInfoInNotes(json pullRequestInfo, string &pullRequestsReleaseNotes, ReleaseNoteModes releaseNotesMode, 
                            int commitTypeIndex);
string getPullRequestInfo(string pullRequestUrl, string githubToken);
string getCommitsNotesFromPullRequests(int commitTypeIndex, string releaseStartRef, string releaseEndRef,
                                       string githubToken, ReleaseNoteModes releaseNotesMode);
string getCommitsNotesFromCommitMessages(int commitTypeIndex, string releaseStartRef, string releaseEndRef);
void generateReleaseNotes(ReleaseNoteSources releaseNoteSource, string releaseStartRef, string releaseEndRef, 
                          string githubToken, ReleaseNoteModes releaseNoteMode = ReleaseNoteModes::Short);
void generatePullRequestChangeNote(string pullRequestNumber, string githubToken);

Config config;

int main(int argc, char* argv[]){

    // Reading values from the external configuration file
    const string releaseNotesConfigFileName = "release_notes_config.json";
    try {
        config.load(releaseNotesConfigFileName);
    }
    catch (const exception& e) {
        cerr << e.what() << endl;
        return 1;
    }

    if (argc <= 1) {
        printInputError(InputErrors::NoReleaseNotesSource);
        return 1;
    }

    try {
        if (strcmp(argv[1], config.singlePullRequestSourceCliInputName.c_str()) == 0) {
            if (argc <= 2) {
                printInputError(InputErrors::NoPullRequestNumber);
                return 1;
            }
            else if (argc <= 3) {
                printInputError(InputErrors::NoGithubToken);
                return 1;
            }
            else if (argc <= 4) {
                printInputError(InputErrors::NoGithubRepository);
                return 1;
            }

            config.repoCommitsUrl = config.githubUrl + argv[4] + "/commit/";
            config.repoIssuesUrl = config.githubUrl + argv[4] + "/issues/";
            config.repoPullRequestsApiUrl = config.githubReposApiUrl + argv[4] + "/pulls/";

            generatePullRequestChangeNote(argv[2], argv[3]);
        }
        else {
            if (argc <= 2) {
                printInputError(InputErrors::NoReleaseStartReference);
                return 1;
            }
            else if (argc <= 3) {
                printInputError(InputErrors::NoReleaseEndReference);
                return 1;
            }
            else if (argc <= 4) {
                printInputError(InputErrors::NoGithubToken);
                return 1;
            }

            if (strcmp(argv[1], config.commitMessagesSourceCliInputName.c_str()) == 0
                || strcmp(argv[1], config.commitMessagesSourceGithubActionsInputName.c_str()) == 0) {
                generateReleaseNotes(ReleaseNoteSources::CommitMessages, argv[2], argv[3], argv[4]);
            }
            else if (strcmp(argv[1], config.pullRequestsSourceCliInputName.c_str()) == 0
                || strcmp(argv[1], config.pullRequestsSourceGithubActionsInputName.c_str()) == 0) {
                if (argc <= 5) {
                    printInputError(InputErrors::NoReleaseNotesMode);
                    return 1;
                }
                else if(argc <= 6) {
                    printInputError(InputErrors::NoGithubRepository);
                    return 1;
                }

                config.repoCommitsUrl = config.githubUrl + argv[6] + "/commit/";
                config.repoIssuesUrl = config.githubUrl + argv[6] + "/issues/";
                config.repoPullRequestsApiUrl = config.githubReposApiUrl + argv[6] + "/pulls/";

                if (strcmp(argv[5], config.fullModeCliInputName.c_str()) == 0
                    || strcmp(argv[5], config.fullModeGithubActionsInputName.c_str()) == 0) {
                    generateReleaseNotes(ReleaseNoteSources::PullRequests, argv[2], argv[3], argv[4], ReleaseNoteModes::Full);
                }
                else if (strcmp(argv[5], config.shortModeCliInputName.c_str()) == 0
                    || strcmp(argv[5], config.shortModeGithubActionsInputName.c_str()) == 0) {
                    generateReleaseNotes(ReleaseNoteSources::PullRequests, argv[2], argv[3], argv[4], ReleaseNoteModes::Short);
                }
                else {
                    printInputError(InputErrors::IncorrectReleaseNotesMode);
                    return 1;
                }
            }
            else {
                printInputError(InputErrors::IncorrectReleaseNotesSource);
                return 1;
            }
        }
    }
    catch (const exception& e) {
        cerr << config.failedToGenerateReleaseNotesMessage << endl;
        cerr << e.what() << endl;
        return 1;
    }

    return 0;
}

/**
 * @brief Formats given pull request information (title, body) into a nice looking markdown format,
 * then adds them in the given current release notes, based on the release notes mode
 * @param pullRequestInfo JSON object containing raw pull request information
 * @param pullRequestsReleaseNotes Existing release notes generated from pull requests
 * @param releaseNotesMode The release notes mode that will decide if the pull request body will be included or not
 * @param commitTypeIndex Index of the commit type in the commit types 2d array that this pull request belongs to
 */
void addPullRequestInfoInNotes(json pullRequestInfo, string &pullRequestsReleaseNotes, ReleaseNoteModes releaseNotesMode,
                                int commitTypeIndex) {
    if (!pullRequestInfo["title"].is_null()) {
        string title = pullRequestInfo["title"];

        CommitTypeMatchResults matchResult = checkCommitTypeMatch(title, commitTypeIndex);

        if(releaseNotesMode == ReleaseNoteModes::Full)
            pullRequestsReleaseNotes += convertConventionalCommitTitleToReleaseNoteTitle(title, matchResult, 
            config.markdownFullModeReleaseNotePrefix);
        else
            pullRequestsReleaseNotes += convertConventionalCommitTitleToReleaseNoteTitle(title, matchResult, 
            config.markdownReleaseNotePrefix);
    }

    if (releaseNotesMode == ReleaseNoteModes::Full && !pullRequestInfo["body"].is_null()) {
        string body = pullRequestInfo["body"];

        // Capitalizing the first letter of the body
        body[0] = toupper(body[0]);
        body = formatPullRequestBody(body);

        pullRequestsReleaseNotes += indentAllLinesInString(body) + "\n";
    }

    pullRequestsReleaseNotes += "\n";
}

/**
 * @brief Retrieves pull request info from the GitHub API using libcurl
 * @param pullRequestUrl The GitHub API URL of the pull request
 * @param githubToken The GitHub token used to make authenticated requests to the GitHub API
 * @return The pull request info in JSON 
 */
string getPullRequestInfo(string pullRequestUrl, string githubToken) {
    // Initializing libcurl
    CURL* curl = curl_easy_init();
    CURLcode resultCode;
    string jsonResponse;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: token " + githubToken).c_str());

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, pullRequestUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handleApiCallBack);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &jsonResponse);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Ahmed-Khaled-dev");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        resultCode = curl_easy_perform(curl);

        if (resultCode == CURLE_OK) {
            // Get the HTTP response code
            long httpCode;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            // All info obtained from https://docs.github.com/en/rest/using-the-rest-api/troubleshooting-the-rest-api?apiVersion=2022-11-28
            // and https://docs.github.com/en/rest/pulls/pulls?apiVersion=2022-11-28#get-a-pull-request
            if (httpCode == 200) {
                return jsonResponse;
            }
            else if (httpCode == 503 || httpCode == 500 || httpCode == 422 || httpCode == 406) {
                throw runtime_error("GitHub API request could not be processed to retrieve pull request " + pullRequestUrl
                    + " Additional information : " + jsonResponse);
            }
            else if (httpCode == 404) {
                throw runtime_error("Pull request " + pullRequestUrl + " not found "
                    + "or you are accessing a private repository and the GitHub token used doesn't have permissions to access pull requests info. "
                    +  "Additional information : " + jsonResponse);
            }
            else {
                handleGithubApiErrorCodes(httpCode, jsonResponse);
            }
        }
        else {
            throw runtime_error(config.githubApiUnableToMakeRequestError);
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    else {
        throw runtime_error(config.githubApiLibcurlError);
    }

    return jsonResponse;
}

/**
 * @brief Retrieves release notes from each commit's *pull request* between the start reference and the end reference
 * based on the given conventional commit type, release notes mode, and using the given GitHub token
 * @param commitTypeIndex Index of the commit type in the commit types 2d array, to only generate release notes from the given commit type (fix, feat, etc.)
 * @param releaseStartRef The git reference (commit SHA or tag name) that references the commit directly before the 
 * commit that starts the commit messages of the release, for example, the tag name of the previous release
 * @param releaseEndRef The git reference (commit SHA or tag name) that references the end of the commit messages of the release
 * @param releaseNotesMode The release notes mode
 * @param githubToken The GitHub token used to make authenticated requests to the GitHub API
 * @return The generated release notes
 */
string getCommitsNotesFromPullRequests(int commitTypeIndex, string releaseStartRef, string releaseEndRef, 
                                       string githubToken, ReleaseNoteModes releaseNotesMode) {
    string commandToRetrieveCommitsMessages = "git log " + releaseStartRef + ".." + releaseEndRef +
            " --oneline --format=\"%s\" --grep=\"^" + config.commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName] + "[:(]\"" +
            " --grep=\"#[0-9]\" --all-match";

    FILE* pipe = popen(commandToRetrieveCommitsMessages.c_str(), "r");
    if (!pipe) {
        throw runtime_error(config.gitLogError);
    }

    char buffer[150];
    string commitMessage, commitPullRequestNumber, releaseNotesFromPullRequests = "";

    // Add the title of this commit type section in the release notes
    releaseNotesFromPullRequests += "\n" + config.commitTypes[commitTypeIndex][(int)CommitTypeInfo::MarkdownTitle] + "\n";

    bool commitTypeContainsReleaseNotes = 0;

    // Reading commit messages line-by-line from the output of the git log command
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        commitTypeContainsReleaseNotes = 1;
        commitMessage = buffer;

        CommitTypeMatchResults matchResult = checkCommitTypeMatch(commitMessage, commitTypeIndex);

        if (matchResult != CommitTypeMatchResults::NoMatch) {
            // Regular expression to match # followed by one or more digits
            regex prRegex(R"(#(\d+))");
            smatch match;

            // Validating that a hashtag exists
            if (regex_search(commitMessage, match, prRegex))
            {
                // Extracting the PR number associated with the commit from the first capture group
                commitPullRequestNumber = match.str(1);

                string jsonResponse = getPullRequestInfo(config.repoPullRequestsApiUrl + commitPullRequestNumber, githubToken);
                json pullRequestInfo = json::parse(jsonResponse);

                addPullRequestInfoInNotes(pullRequestInfo, releaseNotesFromPullRequests, releaseNotesMode, commitTypeIndex);
            }
        }
    }

    // Remove the title of this commit type section if it doesn't contain any release notes
    if (!commitTypeContainsReleaseNotes) {
        releaseNotesFromPullRequests = "";
    }

    pclose(pipe);
    return releaseNotesFromPullRequests;
}

/**
 * @brief Retrieves release notes from each commit's *message* between the start reference and the end reference
 * based on the given conventional commit type
 * @param commitTypeIndex Index of the commit type in the commit types 2d array, to only generate release notes from the given commit type (fix, feat, etc.)
 * @param releaseStartRef The git reference (commit SHA or tag name) that references the commit directly before the 
 * commit that starts the commit messages of the release, for example, the tag name of the previous release
 * @param releaseEndRef The git reference (commit SHA or tag name) that references the end of the commit messages of the release
 * @return The generated release notes
 */
string getCommitsNotesFromCommitMessages(int commitTypeIndex, string releaseStartRef, string releaseEndRef) {
    string commandToRetrieveCommitsMessages = "git log " + releaseStartRef + ".." + releaseEndRef +
        " --oneline --format=\"%s\" --grep=\"^" + config.commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName] + "[:(]\"";

    FILE* pipe = popen(commandToRetrieveCommitsMessages.c_str(), "r");
    if (!pipe) {
        throw runtime_error(config.gitLogError);
    }

    char buffer[150];
    string commitMessage, releaseNotesFromCommitMessages, subCategoryText;

    // Add the title of this commit type section in the release notes
    releaseNotesFromCommitMessages += "\n" + config.commitTypes[commitTypeIndex][(int)CommitTypeInfo::MarkdownTitle] + "\n";

    bool commitTypeContainsReleaseNotes = 0;

    // Reading commit messages line-by-line from the output of the git log command
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        commitTypeContainsReleaseNotes = 1;
        commitMessage = buffer;

        CommitTypeMatchResults matchResult = checkCommitTypeMatch(commitMessage, commitTypeIndex);

        if (matchResult != CommitTypeMatchResults::NoMatch) {
            releaseNotesFromCommitMessages += convertConventionalCommitTitleToReleaseNoteTitle(commitMessage, matchResult, 
            config.markdownReleaseNotePrefix);
        }
    }

    // Remove the title of this commit type section if it doesn't contain any release notes
    if (!commitTypeContainsReleaseNotes) {
        releaseNotesFromCommitMessages = "";
    }

    pclose(pipe);
    return releaseNotesFromCommitMessages;
}

/**
 * @brief Generates release notes using commit messages between the start reference and the end reference
 * using the given release notes source and if the source is pull requests then generates them based on the release note mode 
 * and using the given GitHub token
 * @param releaseNoteSource The source to generate release notes from (commit messages or pull requests)
 * @param releaseStartRef The git reference (commit SHA or tag name) that references the commit directly before the 
 * commit that starts the commit messages of the release, for example, the tag name of the previous release
 * @param releaseEndRef The git reference (commit SHA or tag name) that references the end of the commit messages of the release
 * @param releaseNoteMode The release notes mode when the source is pull requests
 * @param githubToken The GitHub token used to make authenticated requests to the GitHub API when source is pull requests
 */
void generateReleaseNotes(ReleaseNoteSources releaseNoteSource, string releaseStartRef, string releaseEndRef, 
                          string githubToken, ReleaseNoteModes releaseNoteMode) {
    cout << config.generatingReleaseNotesMessage << endl;

    string commandToRetrieveCommitsMessages;
    string currentCommitMessage;
    string markdownReleaseNotes = "";
    for (int i = 0; i < config.commitTypesCount; i++)
    {
        if (releaseNoteSource == ReleaseNoteSources::CommitMessages) {
            markdownReleaseNotes += getCommitsNotesFromCommitMessages(i, releaseStartRef, releaseEndRef);
        }
        else if (releaseNoteSource == ReleaseNoteSources::PullRequests) {
            markdownReleaseNotes += getCommitsNotesFromPullRequests(i, releaseStartRef, releaseEndRef, githubToken, releaseNoteMode);
        }
    }

    writeGeneratedNotesInFiles(markdownReleaseNotes, githubToken);

    cout << "Release notes generated successfully, check " + config.markdownOutputFileName + " and " + config.htmlOutputFileName + " in the current directory" << endl;
}

/**
 * @brief Generates a single change note with it's conventional commit type category
 * for a single pull request using the GitHub API (Not using commit messages at all)
 * @param pullRequestNumber The number of the pull request to generate change note for (e.g., 13, 144, 3722, etc.)
 * @param githubToken The GitHub token used to make authenticated requests to the GitHub API
 */
void generatePullRequestChangeNote(string pullRequestNumber, string githubToken) {
    cout << config.generatingReleaseNotesMessage << endl;

    string jsonResponse = getPullRequestInfo(config.repoPullRequestsApiUrl + pullRequestNumber, githubToken);
    json pullRequestInfo = json::parse(jsonResponse);

    string pullRequestChangeNote = "";
    for (int commitTypeIndex = 0; commitTypeIndex < config.commitTypesCount; commitTypeIndex++)
    {
        if (checkCommitTypeMatch(pullRequestInfo["title"], commitTypeIndex) != CommitTypeMatchResults::NoMatch) {
            pullRequestChangeNote += "\n" + config.commitTypes[commitTypeIndex][(int)CommitTypeInfo::MarkdownTitle] + "\n";
            addPullRequestInfoInNotes(pullRequestInfo, pullRequestChangeNote, ReleaseNoteModes::Full, commitTypeIndex);
            break;
        }
    }

    writeGeneratedNotesInFiles(pullRequestChangeNote, githubToken);

    cout << "Pull request change note generated successfully, check " + config.markdownOutputFileName + " and " + config.htmlOutputFileName + " in the current directory" << endl;
}