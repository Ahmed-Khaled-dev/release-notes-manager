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

string markdownOutputFileName;
string htmlOutputFileName;

/**
 * @brief GitHub API URL to retrieve Synfig's commits' pull request info
 */
string synfigGithubPullRequestsApiUrl;

/**
 * @brief GitHub API URL to convert markdown to HTML
 */
string githubMarkdownApiUrl;

string synfigIssuesUrl;
string synfigCommitsUrl;

int commitTypesCount;

/**
 * @brief 2d array storing conventional commit types and their corresponding markdown titles
 * The first dimension is 50 to give it enough space to store as many types as the user enters in the config.json
 */
string commitTypes[50][2];

enum class ReleaseNoteSources{
    CommitMessages,
    PullRequests
};

enum class ReleaseNoteModes{
    Short,
    Full
};

enum class InputErrors{
    IncorrectReleaseNotesSource,
    NoReleaseNotesSource,
    IncorrectReleaseNotesMode,
    NoReleaseNotesMode,
    NoGithubToken,
    NoReleaseStartReference,
    NoReleaseEndReference
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
enum class CommitTypeMatchResults{
    MatchWithoutSubCategory, /**< when for example "fix:" is matched against "fix", they match and "fix:" doesn't have a subcategory "()"*/
    MatchWithSubCategory, /**< when for example "fix(GUI):" is matched against "fix", they match and "fix(GUI):" has a subcategory which is "GUI"*/
    NoMatch /**< when for example "fix:" or "fix(GUI):" is matched against "feat", they don't match*/
};

void printInputError(InputErrors inputError);
string indentAllLinesInString(string s);
string replaceHashIdsWithLinks(string pullRequestBody);
string replaceCommitShasWithLinks(string pullRequestBody);
string removeExtraNewLines(string pullRequestBody);
string formatPullRequestBody(string pullRequestBody);
string addPullRequestInfoInNotes(json pullRequestInfo, string releaseNotesFromPullRequests, ReleaseNoteModes releaseNotesMode);
size_t handleApiCallBack(char* data, size_t size, size_t numOfBytes, string* buffer);
string getPullRequestInfo(string pullRequestUrl, string githubToken);
CommitTypeMatchResults checkCommitTypeMatch(string commitMessage, int commitTypeIndex);
string getCommitsNotesFromPullRequests(int commitTypeIndex, string releaseStartRef, string releaseEndRef,
                                       string githubToken, ReleaseNoteModes releaseNotesMode);
string getCommitsNotesFromCommitMessages(int commitTypeIndex, string releaseStartRef, string releaseEndRef);
string convertMarkdownToHtml(string markdownText, string githubToken);
void generateReleaseNotes(ReleaseNoteSources releaseNoteSource, string releaseStartRef, string releaseEndRef, 
                          string githubToken, ReleaseNoteModes releaseNoteMode = ReleaseNoteModes::Short);

int main(int argc, char* argv[]){
    
    // Reading values from the external configuration file
    const string configFileName = "release_config.json";
    ifstream externalConfigFile(configFileName);
    if (!externalConfigFile.is_open()) {
        cerr << "Unable to open " << configFileName << ", please ensure that it exists in the same directory as the script" << endl;
        return 1;
    }
    
    json externalConfigData;
    externalConfigFile >> externalConfigData;

    markdownOutputFileName = externalConfigData["markdownOutputFileName"];
    htmlOutputFileName = externalConfigData["htmlOutputFileName"];
    synfigGithubPullRequestsApiUrl = externalConfigData["synfigGithubPullRequestsApiUrl"];
    githubMarkdownApiUrl = externalConfigData["githubMarkdownApiUrl"];
    synfigIssuesUrl = externalConfigData["synfigIssuesUrl"];
    synfigCommitsUrl = externalConfigData["synfigCommitsUrl"];
    commitTypesCount = externalConfigData["commitTypesCount"];

    for (int i = 0; i < commitTypesCount; i++)
    {
        commitTypes[i][0] = externalConfigData["commitTypes"][i]["conventionalType"];
        commitTypes[i][1] = externalConfigData["commitTypes"][i]["markdownTitle"];
    }
    
    if (argc <= 1) {
        printInputError(InputErrors::NoReleaseNotesSource);
        return 1;
    }
    else if (argc <= 2) {
        printInputError(InputErrors::NoReleaseStartReference);
        return 1;
    }
    else if (argc <= 3) {
        printInputError(InputErrors::NoReleaseEndReference);
        return 1;
    }
    else if(argc <= 4){
        printInputError(InputErrors::NoGithubToken);
        return 1;
    }

    try {
        if (strcmp(argv[1], "message") == 0 || strcmp(argv[1], "Commit Messages") == 0) {
            generateReleaseNotes(ReleaseNoteSources::CommitMessages, argv[2], argv[3], argv[4]);
        }
        else if (strcmp(argv[1], "pr") == 0 || strcmp(argv[1], "Pull Requests") == 0) {
            if (argc <= 5) {
                printInputError(InputErrors::NoReleaseNotesMode);
                return 1;
            }

            if (strcmp(argv[5], "full") == 0 || strcmp(argv[5], "Full") == 0) {
                generateReleaseNotes(ReleaseNoteSources::PullRequests, argv[2], argv[3], argv[4], ReleaseNoteModes::Full);
            }
            else if (strcmp(argv[5], "short") == 0 || strcmp(argv[5], "Short") == 0) {
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
    catch (const exception& e) {
        cout << e.what() << endl;
        return 1;
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
    else if (inputError == InputErrors::NoGithubToken) {
        cout << "Please enter a GitHub token to be able to make authenticated requests to the GitHub API" << endl;
    }
    else if (inputError == InputErrors::NoReleaseStartReference) {
        cout << "Please enter a git reference (commit SHA or tag name) that references the commit directly before the commit that *starts* " 
             << "this release's commit messages, for example, the tag name of the previous release" << endl;
    }
    else if (inputError == InputErrors::NoReleaseEndReference) {
        cout << "Please enter a git reference (commit SHA or tag name) that references the commit that *ends* this release's commit messages" << endl;
    }
    cout << "Expected Syntax:" << endl;
    cout << "1 - release_notes_generator message release_start_reference release_end_reference github_token" << endl;
    cout << "2 - release_notes_generator pr release_start_reference release_end_reference github_token short/full" << endl;
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
        result.insert(currentHashIdMatch.position() + numberOfNewCharactersAdded, "[#" + currentNumericId + "](" + synfigIssuesUrl + currentNumericId + ")");
        // Regex smatch.position() was assigned before we replaced hash ids with urls
        // So we must account for that by counting number of new characters we have added, "4" is for the characters "[]()"
        numberOfNewCharactersAdded += 4 + synfigIssuesUrl.length() + currentNumericId.length();
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
        result.insert(currentShaMatch.position() + numberOfNewCharactersAdded, " [" + currentSha.substr(0, 6) + "](" + synfigCommitsUrl + currentSha + ") ");
        numberOfNewCharactersAdded += 4 + synfigCommitsUrl.length() + 6;
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
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            // Check if the response code indicates rate limit exceeded
            if (httpCode == 403) {
                throw runtime_error("Rate limit exceeded. Additional information: " + jsonResponse);
            }
            else if (httpCode == 401) {
                throw runtime_error("Unauthorized. Additional information: " + jsonResponse);
            }
            else if (httpCode == 400) {
                throw runtime_error("Bad request. Additional information: " + jsonResponse);
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
        " --oneline --format=\"%s\" --grep=\"^" + commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName] + "[:(]\"" +
        " --grep=\"#[0-9]\" --all-match";

    FILE* pipe = popen(commandToRetrieveCommitsMessages.c_str(), "r");
    if (!pipe) {
        throw runtime_error("Unable to open pipe to read git log command output");
    }

    char buffer[150];
    string commitMessage, commitPullRequestNumber, releaseNotesFromPullRequests = "";

    // Add the title of this commit type section in the release notes
    releaseNotesFromPullRequests += "\n" + commitTypes[commitTypeIndex][(int)CommitTypeInfo::MarkdownTitle] + "\n";

    bool commitTypeContainsReleaseNotes = 0;

    // Reading commit messages line-by-line from the output of the git log command
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        commitTypeContainsReleaseNotes = 1;
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
                    string jsonResponse = getPullRequestInfo(synfigGithubPullRequestsApiUrl + commitPullRequestNumber, githubToken);
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
        " --oneline --format=\"%s\" --grep=\"^" + commitTypes[commitTypeIndex][(int)CommitTypeInfo::ConventionalName] + "[:(]\"";

    FILE* pipe = popen(commandToRetrieveCommitsMessages.c_str(), "r");
    if (!pipe) {
        throw runtime_error("Unable to open pipe to read git log command output");
    }

    char buffer[150];
    string commitMessage, releaseNotesFromCommitMessages, subCategoryText;

    // Add the title of this commit type section in the release notes
    releaseNotesFromCommitMessages += "\n" + commitTypes[commitTypeIndex][(int)CommitTypeInfo::MarkdownTitle] + "\n";

    bool commitTypeContainsReleaseNotes = 0;

    // Reading commit messages line-by-line from the output of the git log command
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        commitTypeContainsReleaseNotes = 1;
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

    // Remove the title of this commit type section if it doesn't contain any release notes
    if (!commitTypeContainsReleaseNotes) {
        releaseNotesFromCommitMessages = "";
    }

    pclose(pipe);
    return releaseNotesFromCommitMessages;
}

/**
 * @brief Converts markdown to HTML using the GitHub API markdown endpoint
 * @param markdownText The markdown text to be converted to HTML
 * @return The HTML text containing the exact same content as the given markdown
 */
string convertMarkdownToHtml(string markdownText, string githubToken) {
    // Initializing libcurl
    CURL* curl = curl_easy_init();
    CURLcode resultCode;
    string htmlText;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ((const string) "Accept: application/vnd.github+json").c_str());
    headers = curl_slist_append(headers, ("Authorization: token " + githubToken).c_str());

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, githubMarkdownApiUrl.c_str());
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

    return htmlText;
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
    cout << "Generating release notes......." << endl;

    string commandToRetrieveCommitsMessages;
    string currentCommitMessage;
    string markdownReleaseNotes = "";
    for (int i = 0; i < commitTypesCount; i++)
    {
        if (releaseNoteSource == ReleaseNoteSources::CommitMessages) {
            try {
                markdownReleaseNotes += getCommitsNotesFromCommitMessages(i, releaseStartRef, releaseEndRef);
            }
            catch (const exception& e) {
                cout << e.what();
            }
        }
        else if (releaseNoteSource == ReleaseNoteSources::PullRequests) {
            try {
                markdownReleaseNotes += getCommitsNotesFromPullRequests(i, releaseStartRef, releaseEndRef, githubToken, releaseNoteMode);
            }
            catch (const exception& e) {
                cout << e.what();
            }
        }
    }

    ofstream markdownFileOutput(markdownOutputFileName);

    if (!markdownFileOutput.is_open()) {
        throw runtime_error("Unable to create/open markdown release notes file");
    }

    markdownFileOutput << markdownReleaseNotes;
    markdownFileOutput.close();

    ofstream htmlFileOutput(htmlOutputFileName);

    if (!htmlFileOutput.is_open()) {
        throw runtime_error("Unable to create/open HTML release notes file");
    }

    htmlFileOutput << convertMarkdownToHtml(markdownReleaseNotes, githubToken);

    cout << "Release notes generated successfully, check " + markdownOutputFileName + " and " + htmlOutputFileName + " in the current directory" << endl;
}