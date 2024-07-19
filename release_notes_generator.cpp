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

    void load(const string& configFileName) {
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

        if (externalConfigData.contains("githubRepoPullRequestsApiUrl")) {
            githubRepoPullRequestsApiUrl = externalConfigData["githubRepoPullRequestsApiUrl"];
        }
        else {
            throw runtime_error("Key 'githubRepoPullRequestsApiUrl' not found in " + configFileName);
        }

        if (externalConfigData.contains("githubMarkdownApiUrl")) {
            githubMarkdownApiUrl = externalConfigData["githubMarkdownApiUrl"];
        }
        else {
            throw runtime_error("Key 'githubMarkdownApiUrl' not found in " + configFileName);
        }

        if (externalConfigData.contains("repoIssuesUrl")) {
            repoIssuesUrl = externalConfigData["repoIssuesUrl"];
        }
        else {
            throw runtime_error("Key 'repoIssuesUrl' not found in " + configFileName);
        }

        if (externalConfigData.contains("repoCommitsUrl")) {
            repoCommitsUrl = externalConfigData["repoCommitsUrl"];
        }
        else {
            throw runtime_error("Key 'repoCommitsUrl' not found in " + configFileName);
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
        }
        else {
            throw runtime_error("Category 'outputMessages' not found in " + configFileName);
        }
    }
};

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
void handleGithubApiErrorCodes(long errorCode, string apiResponse);
string getPullRequestInfo(string pullRequestUrl, string githubToken);
CommitTypeMatchResults checkCommitTypeMatch(string commitMessage, int commitTypeIndex);
string getCommitsNotesFromPullRequests(int commitTypeIndex, string releaseStartRef, string releaseEndRef,
                                       string githubToken, ReleaseNoteModes releaseNotesMode);
string getCommitsNotesFromCommitMessages(int commitTypeIndex, string releaseStartRef, string releaseEndRef);
string convertMarkdownToHtml(string markdownText, string githubToken);
void generateReleaseNotes(ReleaseNoteSources releaseNoteSource, string releaseStartRef, string releaseEndRef, 
                          string githubToken, ReleaseNoteModes releaseNoteMode = ReleaseNoteModes::Short);

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
    catch (const exception& e) {
        cerr << config.failedToGenerateReleaseNotesMessage << endl;
        cerr << e.what() << endl;
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
        result.insert(currentHashIdMatch.position() + numberOfNewCharactersAdded, "[#" + currentNumericId + "](" + config.repoIssuesUrl + currentNumericId + ")");
        // Regex smatch.position() was assigned before we replaced hash ids with urls
        // So we must account for that by counting number of new characters we have added, "4" is for the characters "[]()"
        numberOfNewCharactersAdded += 4 + config.repoIssuesUrl.length() + currentNumericId.length();
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
        result.insert(currentShaMatch.position() + numberOfNewCharactersAdded, " [" + currentSha.substr(0, 6) + "](" + config.repoCommitsUrl + currentSha + ") ");
        numberOfNewCharactersAdded += 4 + config.repoCommitsUrl.length() + 6;
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
        string::size_type colonPosition = title.find(":");
        if (colonPosition != string::npos) {
            title = title.substr(colonPosition + 2);
        }
        title[0] = toupper(title[0]);

        if(releaseNotesMode == ReleaseNoteModes::Full)
            releaseNotesFromPullRequests += config.markdownFullModeReleaseNotePrefix + title + "\n";
        else
            releaseNotesFromPullRequests += config.markdownReleaseNotePrefix + title + "\n";
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

                string jsonResponse = getPullRequestInfo(config.githubRepoPullRequestsApiUrl + commitPullRequestNumber, githubToken);
                json pullRequestInfo = json::parse(jsonResponse);

                releaseNotesFromPullRequests = addPullRequestInfoInNotes(pullRequestInfo, releaseNotesFromPullRequests, releaseNotesMode) + "\n";
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
        subCategoryText = "";

        CommitTypeMatchResults matchResult = checkCommitTypeMatch(commitMessage, commitTypeIndex);

        if (matchResult != CommitTypeMatchResults::NoMatch) {
            if (matchResult == CommitTypeMatchResults::MatchWithSubCategory) {
                size_t startPos = commitMessage.find("(") + 1;
                // Adding the sub category title
                subCategoryText = " (" + commitMessage.substr(startPos, commitMessage.find(")") - startPos) + " Related) ";
            }
            
            commitMessage = config.markdownReleaseNotePrefix + commitMessage.substr(commitMessage.find(":") + 2) + "\n";

            // Capitalizing the first letter in the commit message
            commitMessage[config.markdownReleaseNotePrefix.size()] = toupper(commitMessage[config.markdownReleaseNotePrefix.size()]);

            // Inserting the commit type subcategory (will be empty if there is no subcategory)
            commitMessage.insert(config.markdownReleaseNotePrefix.size(), subCategoryText);

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
 * @param githubToken The GitHub token used to make authenticated requests to the GitHub API
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

    ofstream markdownFileOutput(config.markdownOutputFileName);

    if (!markdownFileOutput.is_open()) {
        throw runtime_error(config.markdownFileError);
    }

    markdownFileOutput << markdownReleaseNotes;
    markdownFileOutput.close();

    ofstream htmlFileOutput(config.htmlOutputFileName);

    if (!htmlFileOutput.is_open()) {
        throw runtime_error(config.htmlFileError);
    }

    htmlFileOutput << convertMarkdownToHtml(markdownReleaseNotes, githubToken);

    cout << "Release notes generated successfully, check " + config.markdownOutputFileName + " and " + config.htmlOutputFileName + " in the current directory" << endl;
}