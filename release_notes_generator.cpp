/**
 * @file release_notes_generator.cpp
 * @author Ahmed Khaled
 * @brief A C++ script, done while working at Synfig during Google Summer of Code 2024, 
 * the script automatically generates nice-looking markdown/HTML release notes from conventional git commits
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <regex>
#include <curl/curl.h> // Used to make API requests
#include <json.hpp> // Used to parse json files returned by the GitHub API

// To allow for code compatibility between different compilers/OS (Microsoft's Visual C++ compiler and GCC)
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

using namespace std;
using namespace nlohmann;

const int MAX_DISPLAYED_COMMITS_PER_TYPE = 10;
const int COMMIT_TYPES_COUNT = 10;
const string MARKDOWN_OUTPUT_FILE_NAME = "release_notes.md";
const string HTML_OUTPUT_FILE_NAME = "release_notes.html";

/**
 * @brief GitHub API URL to retrieve Synfig's commits' pull request info
 */
const string GITHUB_PULL_REQUESTS_API_URL = "https://api.github.com/repos/synfig/synfig/pulls/";

/**
 * @brief GitHub API URL to convert markdown to HTML
 */
const string GITHUB_MARKDOWN_API_URL = "https://api.github.com/markdown";

//const string GITHUB_API_TOKEN = "";

const string SYNFIG_ISSUES_URL = "https://github.com/synfig/synfig/issues/";
const string SYNFIG_COMMITS_URL = "https://github.com/synfig/synfig/commit/";

enum class ReleaseNoteSources{
    CommitMessages,
    PullRequests
};

enum class ReleaseNoteModes {
    Short,
    Full
};

enum class InputErrors{
    IncorrectReleaseNotesSource,
    NoReleaseNotesSource,
    IncorrectReleaseNotesMode,
    NoReleaseNotesMode,
};

/**
 * @brief Enumeration for all the info related to commit types
 */
enum class CommitTypeInfo{
    ConventionalName, /**< Conventional name of the commit type (Ex: fix, feat, refactor, etc.)*/
    MarkdownTitle /**< Markdown title to be displayed in the release notes section of this commit type (Ex: 🐛 Bug Fixes)*/
};

/**
 * @brief Enumeration for the result of matching a commit type (e.g., fix: or fix(GUI):) against any commit type (fix, feat, etc.)
 */
enum class CommitTypeMatchResults {
    MatchWithoutSubCategory, /**< when for example "fix:" is matched against "fix", they match and "fix:" doesn't have a subcategory "()"*/
    MatchWithSubCategory, /**< when for example "fix(GUI):" is matched against "fix", they match and "fix(GUI):" has a subcategory which is "GUI"*/
    NoMatch /**< when for example "fix:" or "fix(GUI):" is matched against "feat", they don't match*/
};

/**
 * @brief 2d array storing conventional commit types and their corresponding markdown titles
 * mostly retrieved from https://github.com/pvdlg/conventional-changelog-metahub?tab=readme-ov-file#commit-types but I edited some emojis/titles
 */
string commitTypes[COMMIT_TYPES_COUNT][2] = {
    {"feat", "## ✨ New Features"},
    {"fix", "## 🐞 Bug Fixes"},
    {"docs", "## 📚 Documentation"},
    {"style", "## 💎 Styles"},
    {"refactor", "## ♻️ Code Refactoring"},
    {"perf", "## 🚀 Performance Improvements"},
    {"test", "## ✔️ Tests"},
    {"build", "## 📦 Builds"},
    {"ci", "## ⚙️ Continuous Integrations"},
    {"chore", "## 🔧 Chores"}};

void printInputError(InputErrors inputError);
string indentAllLinesInString(string s);
string replaceHashIdsWithLinks(string pullRequestBody);
string replaceCommitShasWithLinks(string pullRequestBody);
string removeExtraNewLines(string pullRequestBody);
string formatPullRequestBody(string pullRequestBody);
string addPullRequestInfoInNotes(json pullRequestInfo, string releaseNotesFromPullRequests, ReleaseNoteModes releaseNotesMode);
size_t handleApiCallBack(char* data, size_t size, size_t numOfBytes, string* buffer);
string getPullRequestInfo(string pullRequestUrl);
CommitTypeMatchResults checkCommitTypeMatch(string commitMessage, int commitTypeIndex);
string getCommitsNotesFromPullRequests(int commitTypeIndex, ReleaseNoteModes releaseNotesMode = ReleaseNoteModes::Short);
string getCommitsNotesFromCommitMessages(int commitTypeIndex);
string convertMarkdownToHtml(string markdownText);
void generateReleaseNotes(ReleaseNoteSources releaseNoteSource, ReleaseNoteModes releaseNoteMode = ReleaseNoteModes::Short);

int main(int argc, char* argv[]){
    if (argc <= 1) {
        printInputError(InputErrors::NoReleaseNotesSource);
        return 0;
    }

    try {
        if (strcmp(argv[1], "message") == 0) {
            generateReleaseNotes(ReleaseNoteSources::CommitMessages);
        }
        else if (strcmp(argv[1], "pr") == 0) {
            if (argc <= 2) {
                printInputError(InputErrors::NoReleaseNotesMode);
                return 0;
            }

            if (strcmp(argv[2], "full") == 0) {
                generateReleaseNotes(ReleaseNoteSources::PullRequests, ReleaseNoteModes::Full);
            }
            else if (strcmp(argv[2], "short") == 0) {
                generateReleaseNotes(ReleaseNoteSources::PullRequests, ReleaseNoteModes::Short);
            }
            else {
                printInputError(InputErrors::IncorrectReleaseNotesMode);
            }
        }
        else {
            printInputError(InputErrors::IncorrectReleaseNotesSource);
        }
    }
    catch (const exception& e) {
        cout << e.what() << endl;
        return 0;
    }

    return 0;
}

/**
 * @brief Prints error messages when user runs the script with incorrect parameters/input
 * @param inputError The type of input error
 */
void printInputError(InputErrors inputError) {
    if (inputError == InputErrors::NoReleaseNotesSource) {
        cout << "Please enter the source you which to use to generate release notes (message or pr)" << endl;
    }
    else if (inputError == InputErrors::IncorrectReleaseNotesSource) {
        cout << "Please enter a valid release notes source" << endl;
    }
    else if (inputError == InputErrors::NoReleaseNotesMode) {
        cout << "Please enter which release notes mode you want for PRs (short or full)" << endl;
    }
    else if (inputError == InputErrors::IncorrectReleaseNotesMode) {
        cout << "Please enter a valid release notes mode" << endl;
    }
    cout << "Expected Syntax:" << endl;
    cout << "1 - release_notes_generator message" << endl;
    cout << "2 - release_notes_generator pr short/full" << endl;
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
 * @brief Replaces all plain text hash ids (issue ids and pull request ids (#2777)) with links to these issues/pull requests on GitHub
 * @param pullRequestBody The original body/description of the pull request to do the replacements on
 * @return Pull request body/description after performing the replacements
 */
string replaceHashIdsWithLinks(string pullRequestBody) {
    // The first brackets are not considered a capture group, they are a must when adding "R" to define this as a *raw string literal*
    // We add this "R" to write regex patterns easier and increase readability by not needing to escape back slashes
    regex hashIdPattern(R"(#(\d+))");
    
    string result = pullRequestBody;
    size_t numberOfNewCharactersAdded = 0;
    sregex_iterator firstHashId(pullRequestBody.begin(), pullRequestBody.end(), hashIdPattern);
    // This is an end of sequence iterator
    sregex_iterator lastHashId;

    for (sregex_iterator i = firstHashId; i != lastHashId; i++) {
        smatch currentHashIdMatch = *i;
        // I get from my current match the first capture group (brackets) of my pattern (\d+) which only contains the numerical id in the hash id
        string currentNumericId = currentHashIdMatch.str(1);
        // I remove the old hash id and create a new markdown link using it and insert the new link in its place
        result.erase(currentHashIdMatch.position() + numberOfNewCharactersAdded, currentHashIdMatch.length());
        result.insert(currentHashIdMatch.position() + numberOfNewCharactersAdded, "[#" + currentNumericId + "](" + SYNFIG_ISSUES_URL + currentNumericId + ")");
        // Regex smatch.position() was assigned before we replaced hash ids with urls
        // So we must account for that by counting number of new characters we have added, "4" is for the characters "[]()"
        numberOfNewCharactersAdded += 4 + SYNFIG_ISSUES_URL.length() + currentNumericId.length();
    }

    return result;
};

/**
 * @brief Replaces all plain text commit SHAs (e.g., 219c2149) with links to these commits on GitHub
 * @param pullRequestBody The original body/description of the pull request to do the replacements on
 * @return Pull request body/description after performing the replacements
 */
string replaceCommitShasWithLinks(string pullRequestBody) {
    regex commitShaPattern(R"([ (]([0-9a-f]{6,40})[ )])");

    string result = pullRequestBody;
    size_t numberOfNewCharactersAdded = 0;
    sregex_iterator firstSha(pullRequestBody.begin(), pullRequestBody.end(), commitShaPattern);
    sregex_iterator lastSha;

    for (sregex_iterator i = firstSha; i != lastSha; i++)
    {
        smatch currentShaMatch = *i;
        string currentSha = currentShaMatch.str(1);
        result.erase(currentShaMatch.position() + numberOfNewCharactersAdded, currentShaMatch.length());
        result.insert(currentShaMatch.position() + numberOfNewCharactersAdded, " [" + currentSha.substr(0, 6) + "](" + SYNFIG_COMMITS_URL + currentSha + ") ");
        numberOfNewCharactersAdded += 4 + SYNFIG_COMMITS_URL.length() + 6;
    }

    return result;
}

/**
 * @brief Remove extra new lines in retrieved PR description to make it look identical to the PR description on GitHub
 * @param pullRequestBody The original body/description of the retrieved PR
 * @return PR body/description after removing extra new lines
 */
string removeExtraNewLines(string pullRequestBody) {
    // New lines in the retrieved PR description are represented as "\r\n" and there is no problem with that
    // BUT after the script writes the retrieved PR description in the markdown file and I observed the contents of the markdown file
    // using a hexadecimal editor, I found out that for some reason an extra "\r" was added when the markdown was written, so new lines were "\r\r\n"
    // for some reason that created extra new lines, so when I tried removing this extra "\r" and I added 2 spaces before the new line "  \r\n"
    // (these 2 spaces in markdown specify that a new line should occur), it worked!
    regex pattern(R"(\r)");
    pullRequestBody = regex_replace(pullRequestBody, pattern, "  ");

    return pullRequestBody;
}

/**
 * @brief Makes the formatting of the retrieved PR body look like the PR on GitHub
 * @param pullRequestBody The original body/description of the retrieved PR
 * @return PR body/description after formatting it
 */
string formatPullRequestBody(string pullRequestBody) {
    pullRequestBody = replaceHashIdsWithLinks(pullRequestBody);
    pullRequestBody = replaceCommitShasWithLinks(pullRequestBody);
    pullRequestBody = removeExtraNewLines(pullRequestBody);

    return pullRequestBody;
}

/**
 * @brief Adds given pull request information (title, body) in the release notes, based on the release notes mode, using the GitHub API
 * @param pullRequestInfo JSON object containing pull request information
 * @param releaseNotesFromPullRequests Existing release notes generated from pull requests
 * @param releaseNotesMode The release notes mode that will decide if the pull request body will be included or not
 * @return The updated release notes after adding the given pull request info, based on the release notes mode
 */
string addPullRequestInfoInNotes(json pullRequestInfo, string releaseNotesFromPullRequests, ReleaseNoteModes releaseNotesMode) {
    if (!pullRequestInfo["title"].is_null()) {
        string title = pullRequestInfo["title"];

        // Removing the commit type from the title and capitalizing the first letter
        title = title.substr(title.find(":") + 2);
        title[0] = toupper(title[0]);

        if(releaseNotesMode == ReleaseNoteModes::Full)
            releaseNotesFromPullRequests += "- ### " + title + "\n";
        else
            releaseNotesFromPullRequests += "- " + title + "\n";
    }

    if (releaseNotesMode == ReleaseNoteModes::Full && !pullRequestInfo["body"].is_null()) {
        string body = pullRequestInfo["body"];

        // Capitalizing the first letter of the body
        body[0] = toupper(body[0]);
        body = formatPullRequestBody(body);

        releaseNotesFromPullRequests += indentAllLinesInString(body) + "\n";
    }

    return releaseNotesFromPullRequests;
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
 * @brief Retrieves pull request info from the GitHub API using libcurl
 * @param pullRequestUrl The GitHub API URL of the pull request
 * @return The pull request info in JSON
 */
string getPullRequestInfo(string pullRequestUrl) {
    // Initializing libcurl
    CURL* curl = curl_easy_init();
    CURLcode resultCode;
    string jsonResponse;
    struct curl_slist* headers = NULL;
    //headers = curl_slist_append(headers, ("Authorization: token " + GITHUB_API_TOKEN).c_str());

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, pullRequestUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handleApiCallBack);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &jsonResponse);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Ahmed-Khaled-dev");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

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
            throw runtime_error("Unable to make request to the GitHub API. Additional information: " + jsonResponse);
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    else {
        throw runtime_error("Error initializing libcurl to make requests to the GitHub API");
    }
}

/**
 * @brief Checks how the given commit message matches the expected conventional commit type
 * @param commitMessage The commit message
 * @param commitTypeIndex Index of the commit type to check against in the commit types 2d array
 * @return The type of match that happened between the two commit types
 */
CommitTypeMatchResults checkCommitTypeMatch(string commitMessage, int commitTypeIndex) {
    string correctCommitType = commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName];

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
 * @brief Retrieves release notes from each commit's *pull request* based on the given conventional commit type and release notes mode
 * @param commitTypeIndex Index of the commit type in the commit types 2d array, to only generate release notes from the given commit type (fix, feat, etc.)
 * @param releaseNotesMode The release notes mode
 * @return The generated release notes from each commit's pull request based on the given conventional commit type and release notes mode
 */
string getCommitsNotesFromPullRequests(int commitTypeIndex, ReleaseNoteModes releaseNotesMode) {
    string commandToRetrieveCommitsMessages = "git log --max-count " + to_string(MAX_DISPLAYED_COMMITS_PER_TYPE) +
        " --oneline --format=\"%s\" --grep=\"^" + commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName] + "[:(]\"" +
        " --grep=\"#[0-9]\" --all-match";

    FILE* pipe = popen(commandToRetrieveCommitsMessages.c_str(), "r");
    if (!pipe) {
        throw runtime_error("Unable to open pipe to read git log command output");
    }

    char buffer[150];
    string commitMessage, commitPullRequestNumber, releaseNotesFromPullRequests = "";

    // Reading commit messages line-by-line from the output of the git log command
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        commitMessage = buffer;

        CommitTypeMatchResults matchResult = checkCommitTypeMatch(commitMessage, commitTypeIndex);

        if (matchResult != CommitTypeMatchResults::NoMatch) {
            size_t hashtagPositionInCommitMessage = commitMessage.find("#");

            // Validating that a hashtag exists
            if (hashtagPositionInCommitMessage != string::npos)
            {
                // Extracting the PR number associated with the commit
                commitPullRequestNumber = commitMessage.substr(hashtagPositionInCommitMessage + 1, 4);

                try
                {
                    string jsonResponse = getPullRequestInfo(GITHUB_PULL_REQUESTS_API_URL + commitPullRequestNumber);
                    json pullRequestInfo = json::parse(jsonResponse);

                    releaseNotesFromPullRequests = addPullRequestInfoInNotes(pullRequestInfo, releaseNotesFromPullRequests, releaseNotesMode) + "\n";
                }
                catch (const exception& e)
                {
                    return e.what();
                }
            }
        }
    }
    pclose(pipe);
    return releaseNotesFromPullRequests;
}

/**
 * @brief Retrieves release notes from each commit's *message* based on the given conventional commit type
 * @param commitTypeIndex Index of the commit type in the commit types 2d array, to only generate release notes from the given commit type (fix, feat, etc.)
 * @return The generated release notes from each commit's message based on the given conventional commit type
 */
string getCommitsNotesFromCommitMessages(int commitTypeIndex) {
    string commandToRetrieveCommitsMessages = "git log --max-count " + to_string(MAX_DISPLAYED_COMMITS_PER_TYPE) +
        " --oneline --format=\"%s\" --grep=\"^" + commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName] + "[:(]\"";

    FILE* pipe = popen(commandToRetrieveCommitsMessages.c_str(), "r");
    if (!pipe) {
        throw runtime_error("Unable to open pipe to read git log command output");
    }

    char buffer[150];
    string commitMessage, releaseNotesFromCommitMessages, subCategoryText;

    // Reading commit messages line-by-line from the output of the git log command
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        commitMessage = buffer;
        subCategoryText = "";

        CommitTypeMatchResults matchResult = checkCommitTypeMatch(commitMessage, commitTypeIndex);

        if (matchResult != CommitTypeMatchResults::NoMatch) {
            if (matchResult == CommitTypeMatchResults::MatchWithSubCategory) {
                size_t startPos = commitMessage.find("(") + 1;
                // Adding the sub category title
                subCategoryText = " (" + commitMessage.substr(startPos, commitMessage.find(")") - startPos) + " Related) ";
            }
            
            commitMessage = "- " + commitMessage.substr(commitMessage.find(":") + 2) + "\n";

            // Capitalizing the first letter in the commit message
            commitMessage[2] = toupper(commitMessage[2]);

            // Inserting the commit type subcategory (will be empty if there is no subcategory)
            commitMessage.insert(2, subCategoryText);

            releaseNotesFromCommitMessages += commitMessage;
        }
    }

    pclose(pipe);
    return releaseNotesFromCommitMessages;
}

/**
 * @brief Converts markdown to HTML using the GitHub API markdown endpoint
 * @param markdownText The markdown text to be converted to HTML
 * @return The HTML text containing the exact same content as the given markdown
 */
string convertMarkdownToHtml(string markdownText) {
    // Initializing libcurl
    CURL* curl = curl_easy_init();
    CURLcode resultCode;
    string htmlText;
    struct curl_slist* headers = NULL;
    //headers = curl_slist_append(headers, ("Authorization: token " + GITHUB_API_TOKEN).c_str());
    headers = curl_slist_append(headers, ((const string) "Accept: application/vnd.github+json").c_str());

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, GITHUB_MARKDOWN_API_URL.c_str());
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
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            // Check if the response code indicates rate limit exceeded
            if (httpCode == 403) {
                throw runtime_error("Rate limit exceeded. Additional information: " + htmlText);
            }
            else {
                return htmlText;
            }
        }
        else {
            throw runtime_error("Unable to make request to the GitHub API. Additional information: " + htmlText);
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    else {
        throw runtime_error("Error initializing libcurl to make requests to the GitHub API");
    }
}

/**
 * @brief Generates release notes based on the given source and release notes mode
 * @param releaseNoteSource The source to generate release notes from (commit messages or pull requests)
 * @param releaseNoteMode The release notes mode when the source is pull requests
 */
void generateReleaseNotes(ReleaseNoteSources releaseNoteSource, ReleaseNoteModes releaseNoteMode) {
    cout << "Generating release notes......." << endl;

    string commandToRetrieveCommitsMessages;
    string currentCommitMessage;
    string markdownReleaseNotes = "";
    for (int i = 0; i < COMMIT_TYPES_COUNT; i++)
    {
        // Output the title of this commit type section in the release notes
        markdownReleaseNotes += "\n" + commitTypes[i][(int)CommitTypeInfo::MarkdownTitle] + "\n";

        if (releaseNoteSource == ReleaseNoteSources::CommitMessages) {
            try {
                markdownReleaseNotes += getCommitsNotesFromCommitMessages(i);
            }
            catch (const exception& e) {
                cout << e.what();
            }
        }
        else if (releaseNoteSource == ReleaseNoteSources::PullRequests) {
            try {
                markdownReleaseNotes += getCommitsNotesFromPullRequests(i, releaseNoteMode);
            }
            catch (const exception& e) {
                cout << e.what();
            }
        }
    }

    ofstream markdownFileOutput(MARKDOWN_OUTPUT_FILE_NAME);

    if (!markdownFileOutput.is_open()) {
        throw runtime_error("Unable to create/open markdown release notes file");
    }

    markdownFileOutput << markdownReleaseNotes;
    markdownFileOutput.close();

    ofstream htmlFileOutput(HTML_OUTPUT_FILE_NAME);

    if (!htmlFileOutput.is_open()) {
        throw runtime_error("Unable to create/open HTML release notes file");
    }

    htmlFileOutput << convertMarkdownToHtml(markdownReleaseNotes);

    cout << "Release notes generated successfully, check " + MARKDOWN_OUTPUT_FILE_NAME + " and " + HTML_OUTPUT_FILE_NAME + " in the current directory" << endl;
}