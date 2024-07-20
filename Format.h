/**
 * @file Format.h
 * @author Ahmed Khaled
 * @brief This file defines functions related to text/markdown formatting
 */

#pragma once

#include <string>

using namespace std;

string indentAllLinesInString(string s);
string replaceHashIdsWithLinks(string pullRequestBody);
string replaceCommitShasWithLinks(string pullRequestBody);
string removeExtraNewLines(string pullRequestBody);
string formatPullRequestBody(string pullRequestBody);
