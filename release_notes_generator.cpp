// Author: Ahmed Khaled
// Description: This is a script made as an exercise to demonstrate the ability to work on the Synfig GSoC 24
// "Automated release notes generator" project, the script generates markdown release notes from conventional git commits
// Synfig : https://www.synfig.org/
// Project : https://synfig-docs-dev.readthedocs.io/en/latest/gsoc/2024/ideas.html#projects-ideas

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
// Used to make api requests
#include <curl/curl.h>
// Used to parse json files returned by the GitHub API
#include <nlohmann/json.hpp>

using namespace std;
using namespace nlohmann;

const int MAX_DISPLAYED_COMMITS_PER_TYPE = 3;
const int COMMIT_TYPES_COUNT = 3;
const string MARKDOWN_OUTPUT_FILE_NAME = "release_notes.md";
// Used to retrieve merge commits' pull request info
const string GITHUB_API_URL = "https://api.github.com/repos/synfig/synfig/pulls/";

enum class Commits{
    Normal,
    Merge
};

// Can later on add HTMl
enum class OutputTypes{
    Markdown
};

enum class InputErrors{
    IncorrectCommitType,
    NoCommitType,
    NoReleaseNotesMode,
    IncorrectReleaseNotesMode
};

enum class CommitTypeInfo{
    ConventionalName,
    MarkdownTitle
};

enum class ReleaseNoteModes{
    Short,
    Full
};

string commitTypes[COMMIT_TYPES_COUNT][2] = {
    {"fix", "## 🐛 Bug Fixes"}, 
    {"feat", "## ✨ New Features"},
    {"refactor", "## ♻️ Code Refactoring"}};

void printInputError(InputErrors inputError);
string indentAllLinesInString(string s);
string addPullRequestInfoInNotes(json pullRequestInfo, string mergeCommitsNotes, ReleaseNoteModes releaseNotesMode);
size_t handleApiCallBack(char* data, size_t size, size_t numOfBytes, string* buffer);
string getApiResponse(string pullRequestUrl);
bool commitIsCorrectType(string commitMessage, int commitTypeIndex);
string getMergeCommitsNotes(int commitTypeIndex, ReleaseNoteModes releaseNotesMode = ReleaseNoteModes::Short);
string getNormalCommitsNotes(int commitTypeIndex);
void generateReleaseNotes(Commits commit, OutputTypes outputType, ReleaseNoteModes releaseNoteMode = ReleaseNoteModes::Short);

int main(int argc, char* argv[]){
    if (argc <= 1) {
        printInputError(InputErrors::NoCommitType);
        return 0;
    }

    try {
        if (strcmp(argv[1], "n") == 0) {
            generateReleaseNotes(Commits::Normal, OutputTypes::Markdown);
        }
        else if (strcmp(argv[1], "m") == 0) {
            if (argc <= 2) {
                printInputError(InputErrors::NoReleaseNotesMode);
                return 0;
            }

            if (strcmp(argv[2], "f") == 0) {
                generateReleaseNotes(Commits::Merge, OutputTypes::Markdown, ReleaseNoteModes::Full);
            }
            else if (strcmp(argv[2], "s") == 0) {
                generateReleaseNotes(Commits::Merge, OutputTypes::Markdown, ReleaseNoteModes::Short);
            }
            else {
                printInputError(InputErrors::IncorrectReleaseNotesMode);
            }
        }
        else {
            printInputError(InputErrors::IncorrectCommitType);
        }
    }
    catch (const exception& e) {
        cout << e.what();
    }

    return 0;
}

void printInputError(InputErrors inputError) {
    if (inputError == InputErrors::IncorrectCommitType) {
        cout << "Incorrect commit type!" << endl;
    }
    else if (inputError == InputErrors::NoCommitType) {
        cout << "Please enter the type of commits you which to use to generate release notes" << endl;
    }
    else if (inputError == InputErrors::NoReleaseNotesMode) {
        cout << "Please enter which release notes mode you want for merge commits (short or full)" << endl;
    }
    else if (inputError == InputErrors::IncorrectReleaseNotesMode) {
        cout << "Please enter a valid release notes mode" << endl;
    }
    cout << "Expected Syntax:" << endl;
    cout << "1 - release_notes_generator n (Normal Commits)" << endl;
    cout << "2 - release_notes_generator m [s/f] (Merge Commits) (short or full release notes)";
}

string indentAllLinesInString(string s) {
    bool isNewLine = 1;
    string result;
    for (char c : s) {
        if (isNewLine) {
            result += "    ";
        }
        result += c;

        isNewLine = (c == '\n');
    }

    return result;
}

string addPullRequestInfoInNotes(json pullRequestInfo, string mergeCommitsNotes, ReleaseNoteModes releaseNotesMode) {
    if (!pullRequestInfo["title"].is_null()) {
        string title = pullRequestInfo["title"];

        // Removing the commit type from the title and capitalizing the first letter
        title = title.substr(title.find(":") + 2);
        title[0] = toupper(title[0]);

        if(releaseNotesMode == ReleaseNoteModes::Full)
            mergeCommitsNotes += "- ### " + title + "\n";
        else
            mergeCommitsNotes += "- " + title + "\n";
    }

    if (releaseNotesMode == ReleaseNoteModes::Full && !pullRequestInfo["body"].is_null()) {
        string body = pullRequestInfo["body"];

        // Capitalizing the first letter of the body
        body[0] = toupper(body[0]);
        mergeCommitsNotes += indentAllLinesInString(body) + "\n";
    }

    return mergeCommitsNotes;
}

size_t handleApiCallBack(char* data, size_t size, size_t numOfBytes, string* buffer) {
    size_t totalSize = size * numOfBytes;
    buffer->append(data, totalSize);
    return totalSize;
}

string getApiResponse(string pullRequestUrl) {
    // Initializing libcurl
    CURL* curl;
    CURLcode resultCode;
    curl = curl_easy_init();
    string jsonResponse;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, pullRequestUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handleApiCallBack);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &jsonResponse);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Ahmed-Khaled-dev");

        resultCode = curl_easy_perform(curl);

        if (resultCode == CURLE_OK) {
            // Get the HTTP response code
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            // Check if the response code indicates rate limit exceeded
            if (httpCode == 403) {
                throw runtime_error("Rate limit exceeded. Additional information: " + jsonResponse);
            }
            else {
                return jsonResponse;
            }
        }
        else {
            throw runtime_error("Unable to make request to the GitHub API");
        }

        curl_easy_cleanup(curl);
    }
    else {
        throw runtime_error("Error initializing libcurl to make requests to the GitHub API");
    }
}

bool commitIsCorrectType(string commitMessage, int commitTypeIndex) {
    return commitMessage.substr(0, commitMessage.find(":")) == commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName];
}

string getMergeCommitsNotes(int commitTypeIndex, ReleaseNoteModes releaseNotesMode) {
    string commandToRetrieveCommitsMessages = "git log --max-count " + to_string(MAX_DISPLAYED_COMMITS_PER_TYPE) +
        " --merges --oneline --format=\"%s\" --grep=\"^" + commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName] + ": \"";

    FILE* pipe = _popen(commandToRetrieveCommitsMessages.c_str(), "r");
    if (!pipe) {
        throw runtime_error("Unable to open pipe to read git log commmand output");
    }

    char buffer[150];
    string commitMessage, commitPullRequestNumber, mergeCommitsNotes = "";

    // Reading commit messages line-by-line from the output of the git log command
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        commitMessage = buffer;

        if (commitIsCorrectType(commitMessage, commitTypeIndex)) {
            commitPullRequestNumber = commitMessage.substr(commitMessage.find("#") + 1, 4);

            try
            {
                string jsonResponse = getApiResponse(GITHUB_API_URL + commitPullRequestNumber);
                json pullRequestInfo = json::parse(jsonResponse);

                mergeCommitsNotes = addPullRequestInfoInNotes(pullRequestInfo, mergeCommitsNotes, releaseNotesMode) + "\n";
            }
            catch (const exception& e)
            {
                return e.what();
            }
        }
    }
    _pclose(pipe);
    return mergeCommitsNotes;
}

string getNormalCommitsNotes(int commitTypeIndex) {
    string commandToRetrieveCommitsMessages = "git log --max-count " + to_string(MAX_DISPLAYED_COMMITS_PER_TYPE) +
        " --no-merges --oneline --format=\"%s\" --grep=\"^" + commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName] + ": \"";

    FILE* pipe = _popen(commandToRetrieveCommitsMessages.c_str(), "r");
    if (!pipe) {
        throw runtime_error("Error: Unable to open pipe to read git log commmand output");
    }

    char buffer[150];
    string commitMessage, normalCommitsNotes;

    // Reading commit messages line-by-line from the output of the git log command
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        commitMessage = buffer;

        if (commitIsCorrectType(commitMessage, commitTypeIndex)) {
            commitMessage = "- " + commitMessage.substr(commitMessage.find(":") + 2) + "\n";

            // Capitalizing the first letter in the commit message
            commitMessage[2] = toupper(commitMessage[2]);

            normalCommitsNotes += commitMessage;
        }
    }

    _pclose(pipe);
    return normalCommitsNotes;
}

void generateReleaseNotes(Commits commit, OutputTypes outputType, ReleaseNoteModes releaseNoteMode) {
    ofstream outputFile;
    if (outputType == OutputTypes::Markdown) {
        outputFile.open(MARKDOWN_OUTPUT_FILE_NAME);

        if (!outputFile.is_open()) {
            throw runtime_error("Unable to open markdown release notes file");
        }
    }

    string commandToRetrieveCommitsMessages;
    string currentCommitMessage;
    for (int i = 0; i < COMMIT_TYPES_COUNT; i++)
    {
        // Output the title of this commit type section in the release notes
        outputFile << "\n" << commitTypes[i][(int)CommitTypeInfo::MarkdownTitle] << "\n";

        if (commit == Commits::Normal) {
            try {
                outputFile << getNormalCommitsNotes(i);
            }
            catch (const exception& e) {
                cout << e.what();
            }
        }
        else if (commit == Commits::Merge) {
            try {
                outputFile << getMergeCommitsNotes(i, releaseNoteMode);
            }
            catch (const exception& e) {
                cout << e.what();
            }
        }
    }
}