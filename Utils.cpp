/**
 * @file Utils.cpp
 * @author Ahmed Khaled
 * @brief This file implements functions in Utils.h
 */

#include <iostream>
#include <string>

#include <curl/curl.h> // Used to make API requests
#include <json.hpp>

#include "Utils.h"
#include "Enums.h"
#include "Config.h"

using namespace std;
using namespace nlohmann;

extern Config config;

/**
 * @brief Prints error messages when user runs the script with incorrect parameters/input
 * @param inputError The type of input error
 */
void printInputError(InputErrors inputError) {
    if (inputError == InputErrors::NoReleaseNotesSource) {
        cerr << config.noReleaseNotesSourceError << endl;
    }
    else if (inputError == InputErrors::IncorrectReleaseNotesSource) {
        cerr << config.incorrectReleaseNotesSourceError << endl;
    }
    else if (inputError == InputErrors::NoReleaseNotesMode) {
        cerr << config.noReleaseNotesModeError << endl;
    }
    else if (inputError == InputErrors::IncorrectReleaseNotesMode) {
        cerr << config.incorrectReleaseNotesModeError << endl;
    }
    else if (inputError == InputErrors::NoGithubToken) {
        cerr << config.noGithubTokenError << endl;
    }
    else if (inputError == InputErrors::NoReleaseStartReference) {
        cerr << config.noReleaseStartReferenceError << endl;
    }
    else if (inputError == InputErrors::NoReleaseEndReference) {
        cerr << config.noReleaseEndReferenceError << endl;
    }
    cerr << config.expectedSyntaxMessage << endl;
}

/**
 * @brief Callback function that is required to handle API response in libcurl
 * @param data Pointer to the data received
 * @param size Size of each data element
 * @param numOfBytes Number of bytes received
 * @param buffer Pointer to the buffer storing the response
 * @return Total size of the received data
 */
size_t handleApiCallBack(char* data, size_t size, size_t numOfBytes, string* buffer) {
    size_t totalSize = size * numOfBytes;
    buffer->append(data, totalSize);
    return totalSize;
}

/**
 * @brief Throws runtime exceptions with appropriate messages that describe the given GitHub API error code
 * All info obtained from https://docs.github.com/en/rest/using-the-rest-api/troubleshooting-the-rest-api?apiVersion=2022-11-28
 * @param errorCode The GitHub API error code that occurred
 * @param apiResponse The GitHub API response
 */
void handleGithubApiErrorCodes(long errorCode, string apiResponse) {
    if (errorCode == 429 || errorCode == 403) {
        throw runtime_error(config.githubApiRateLimitExceededError + apiResponse);
    }
    else if (errorCode == 401) {
        throw runtime_error(config.githubApiUnauthorizedAccessError + apiResponse);
    }
    else if (errorCode == 400) {
        throw runtime_error(config.githubApiBadRequestError + apiResponse);
    }
}

/**
 * @brief Checks how the given commit message matches the expected conventional commit type
 * @param commitMessage The commit message
 * @param commitTypeIndex Index of the commit type to check against in the commit types 2d array
 * @return The type of match that happened between the two commit types
 */
CommitTypeMatchResults checkCommitTypeMatch(string commitMessage, int commitTypeIndex) {
    string correctCommitType = config.commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName];

    if (commitMessage.substr(0, commitMessage.find(":")) == correctCommitType) {
        return CommitTypeMatchResults::MatchWithoutSubCategory;
    }
    else if (commitMessage.substr(0, commitMessage.find("(")) == correctCommitType) {
        return CommitTypeMatchResults::MatchWithSubCategory;
    }
    else {
        return CommitTypeMatchResults::NoMatch;
    }
}

/**
 * @brief Converts markdown to HTML using the GitHub API markdown endpoint
 * @param markdownText The markdown text to be converted to HTML
 * @param githubToken The GitHub token used to make authenticated requests to the GitHub API
 * @return The HTML text containing the exact same content as the given markdown
 */
string convertMarkdownToHtml(string markdownText, string githubToken) {
    // Initializing libcurl
    CURL* curl = curl_easy_init();
    CURLcode resultCode;
    string htmlText;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ((const string)"Accept: application/vnd.github+json").c_str());
    headers = curl_slist_append(headers, ("Authorization: token " + githubToken).c_str());

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, config.githubMarkdownApiUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handleApiCallBack);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &htmlText);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Ahmed-Khaled-dev");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        json postData;
        postData["text"] = markdownText;
        string postDataString = postData.dump();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postDataString.c_str());

        resultCode = curl_easy_perform(curl);

        if (resultCode == CURLE_OK) {
            // Get the HTTP response code
            long httpCode;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            if (httpCode == 200) {
                return htmlText;
            }
            else if (httpCode == 404) {
                throw runtime_error("Markdown API url not found");
            }
            else {
                handleGithubApiErrorCodes(httpCode, htmlText);
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

    return htmlText;
}