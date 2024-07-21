/**
 * @file Enums.h
 * @author Ahmed Khaled
 * @brief This file contains all the enums used in the whole program
 */

#pragma once

enum class ReleaseNoteSources {
    CommitMessages,
    PullRequests
};

enum class ReleaseNoteModes {
    Short,
    Full
};

enum class InputErrors {
    IncorrectReleaseNotesSource,
    NoReleaseNotesSource,
    IncorrectReleaseNotesMode,
    NoReleaseNotesMode,
    NoGithubToken,
    NoReleaseStartReference,
    NoReleaseEndReference,
    NoPullRequestNumber
};

/**
 * @brief Enumeration for all the info related to commit types
 */
enum class CommitTypeInfo {
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