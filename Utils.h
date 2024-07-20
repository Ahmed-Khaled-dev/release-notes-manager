/**
 * @file Utils.h
 * @author Ahmed Khaled
 * @brief This file defines general utility functions, like error handling, checks, converting types, etc.
 */

#pragma once

#include <string>

#include "Enums.h"

using namespace std;

void printInputError(InputErrors inputError);
size_t handleApiCallBack(char* data, size_t size, size_t numOfBytes, string* buffer);
void handleGithubApiErrorCodes(long errorCode, string apiResponse);
CommitTypeMatchResults checkCommitTypeMatch(string commitMessage, int commitTypeIndex);
string convertMarkdownToHtml(string markdownText, string githubToken);