/**
 * @file Format.cpp
 * @author Ahmed Khaled
 * @brief This file implements functions in Format.h
 */

#include <string>
#include <regex>

#include "Format.h"
#include "Config.h"

using namespace std;

extern Config config;

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
