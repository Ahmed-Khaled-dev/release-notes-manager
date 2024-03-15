/**
 * @file release_notes_generator.cpp
 * @author Ahmed Khaled
 * @brief A C++ script, to demonstrate the ability to work on the Synfig GSoC 2024 Automated release notes generator project, 
 * the script generates markdown release notes from conventional git commits
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <curl/curl.h> // Used to make api requests
#include <json.hpp> // Used to parse json files returned by the GitHub API

// To allow for code compatibility between different compilers/OS (Microsoft's Visual C++ compiler and GCC)
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

using namespace std;
using namespace nlohmann;

const int MAX_DISPLAYED_COMMITS_PER_TYPE = 3;
const int COMMIT_TYPES_COUNT = 3;
const string MARKDOWN_OUTPUT_FILE_NAME = "release_notes.md";
/**
 * @brief GitHub API URL to retrieve merge commits' pull request info
 */
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

/**
 * @brief Enumeration for all the info related to commit types
 */
enum class CommitTypeInfo{
    ConventionalName, /**< Conventional name of the commit type (Ex: fix, feat, refactor, etc.)*/
    MarkdownTitle /**< Markdown title to be displayed in the release notes section of this commit type (Ex: 🐛 Bug Fixes)*/
};

enum class ReleaseNoteModes{
    Short,
    Full
};

/**
 * @brief 2d array storing conventional commit types and their corresponding markdown titles, can easily later on modify these types
 */
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
        cout << e.what() << endl;
        return 0;
    }

    cout << "Release notes generated successfully, check " + MARKDOWN_OUTPUT_FILE_NAME + " in the current directory" << endl;
    return 0;
}

/**
 * @brief Prints error messages when user runs the script with incorrect parameters/input
 * @param inputError The type of input error
 */
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
    cout << "2 - release_notes_generator m [s/f] (Merge Commits) (short or full release notes)" << endl;
}

/**
 * @brief Indents (puts 4 spaces) before all lines in a string
 * @param s The input string
 * @return The indented string
 */
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

/**
 * @brief Adds merge commits' pull request information (title, body) in the release notes, based on the release notes mode, using GitHub API
 * @param pullRequestInfo JSON object containing pull request information
 * @param mergeCommitsNotes Existing release notes generated from merge commits
 * @param releaseNotesMode The release notes mode that will decide if the Pull request body will be included or not
 * @return The updated release notes generated from merge commits
 */
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

/**
 * @brief Callback function that is requried to handle API response in libcurl
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
 * @brief Retrieves merge commit's Pull request info from the GitHub API using libcurl
 * @param pullRequestUrl The GitHub API URL of the merge commit's pull request
 * @return The Pull request info in JSON
 */
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

/**
 * @brief Checks if the commit message is of the expected conventional commit type
 * @param commitMessage The commit message
 * @param commitTypeIndex Index of the commit type in the commit types 2d array
 * @return True if the commit is of expected type, otherwise false
 */
bool commitIsCorrectType(string commitMessage, int commitTypeIndex) {
    return commitMessage.substr(0, commitMessage.find(":")) == commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName];
}

/**
 * @brief Retrieves release notes from merge commits based on the given conventional commit type and release notes mode
 * @param commitTypeIndex Index of the commit type in the commit types 2d array, to only generate release notes from the given commit type (fix, feat, etc.)
 * @param releaseNotesMode The release notes mode
 * @return The generated release notes from merge commits
 */
string getMergeCommitsNotes(int commitTypeIndex, ReleaseNoteModes releaseNotesMode) {
    string commandToRetrieveCommitsMessages = "git log --max-count " + to_string(MAX_DISPLAYED_COMMITS_PER_TYPE) +
        " --merges --oneline --format=\"%s\" --grep=\"^" + commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName] + ": \"";

    FILE* pipe = popen(commandToRetrieveCommitsMessages.c_str(), "r");
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
    pclose(pipe);
    return mergeCommitsNotes;
}

/**
 * @brief Retrieves release notes from normal commits based on the given conventional commit type
 * @param commitTypeIndex Index of the commit type in the commit types 2d array, to only generate release notes from the given commit type (fix, feat, etc.)
 * @return The generated release notes from normal commits
 */
string getNormalCommitsNotes(int commitTypeIndex) {
    string commandToRetrieveCommitsMessages = "git log --max-count " + to_string(MAX_DISPLAYED_COMMITS_PER_TYPE) +
        " --no-merges --oneline --format=\"%s\" --grep=\"^" + commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName] + ": \"";

    FILE* pipe = popen(commandToRetrieveCommitsMessages.c_str(), "r");
    if (!pipe) {
        throw runtime_error("Unable to open pipe to read git log commmand output");
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

    pclose(pipe);
    return normalCommitsNotes;
}

/**
 * @brief Generates release notes based on the given commit type, output type, and release notes mode
 * @param commit The type of commits to generate release notes from (Normal, Merge)
 * @param outputType The document/output type that the release notes will be generated in (Markdown, HTMl, etc.)
 * @param releaseNoteMode The release notes mode when the commit type is merge
 */
void generateReleaseNotes(Commits commit, OutputTypes outputType, ReleaseNoteModes releaseNoteMode) {
    ofstream outputFile;
    if (outputType == OutputTypes::Markdown) {
        outputFile.open(MARKDOWN_OUTPUT_FILE_NAME);

        if (!outputFile.is_open()) {
            throw runtime_error("Unable to open markdown release notes file");
        }
    }

    cout << "Generating release notes......." << endl;

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