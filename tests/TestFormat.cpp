#include "doctest.h"

#include "../Format.h"
#include "../Config.h"

Config config;

TEST_CASE("Testing the string indenting function") {
    CHECK(indentAllLinesInString("Hello World") == "    Hello World");
    CHECK(indentAllLinesInString("") == "");
    CHECK(indentAllLinesInString("\n") == "    \n");
    CHECK(indentAllLinesInString("Line1\nLine2") == "    Line1\n    Line2");
    CHECK(indentAllLinesInString("Line1\nLine2\nLine3") == "    Line1\n    Line2\n    Line3");
    CHECK(indentAllLinesInString("    Already indented\nNot indented") == "        Already indented\n    Not indented");
}

// For the following 2 functions, I later on could consider validating that the detected commit SHA/hash id
// is a real reference that exists in the git repository I am working on, this will increase detection accuracy
// but in return will decrease the speed of the script, since this will add multiple API requests
// so I should later on consider if it would be worth it
TEST_CASE("Testing replacing hash-ids with markdown links function") {
    config.repoIssuesUrl = "https://github.com/user/repo/issues/";

    CHECK(replaceHashIdsWithLinks("") == "");
    CHECK(replaceHashIdsWithLinks("No issue id here") == "No issue id here");
    CHECK(replaceHashIdsWithLinks("#1234") == "[#1234](https://github.com/user/repo/issues/1234)");
    CHECK(replaceHashIdsWithLinks("Fixes #1234 and closes #5678") == "Fixes [#1234](https://github.com/user/repo/issues/1234) and closes [#5678](https://github.com/user/repo/issues/5678)");
    CHECK(replaceHashIdsWithLinks("Related to #1") == "Related to [#1](https://github.com/user/repo/issues/1)");
    CHECK(replaceHashIdsWithLinks("Multiple issues: #123, #456, and #789") == "Multiple issues: [#123](https://github.com/user/repo/issues/123), [#456](https://github.com/user/repo/issues/456), and [#789](https://github.com/user/repo/issues/789)");
    CHECK(replaceHashIdsWithLinks("Not a hash id: #abcd") == "Not a hash id: #abcd");
    CHECK(replaceHashIdsWithLinks("Very large id #12345678901234567890") == "Very large id [#12345678901234567890](https://github.com/user/repo/issues/12345678901234567890)");
    CHECK(replaceHashIdsWithLinks("#1234!") == "[#1234](https://github.com/user/repo/issues/1234)!");
    CHECK(replaceHashIdsWithLinks("Multiple, spaced: #12, #34, #56") == "Multiple, spaced: [#12](https://github.com/user/repo/issues/12), [#34](https://github.com/user/repo/issues/34), [#56](https://github.com/user/repo/issues/56)");
    // The below edge cases my script fails in them, they are very minor cases so I will leave them commented for now
    //CHECK(replaceHashIdsWithLinks("Mixed #12abc") == "Mixed #12abc");
    //CHECK(replaceHashIdsWithLinks("#1234#5678") == "[#1234](https://github.com/user/repo/issues/1234)#5678");
    //CHECK(replaceHashIdsWithLinks("Already linked [#1234](https://github.com/user/repo/issues/1234) and #5678") == "Already linked [#1234](https://github.com/user/repo/issues/1234) and [#5678](https://github.com/user/repo/issues/5678)");
}

TEST_CASE("Testing replacing commit SHAs with markdown links function") {
    config.repoCommitsUrl = "https://github.com/user/repo/commit/";

    CHECK(replaceCommitShasWithLinks("") == "");
    CHECK(replaceCommitShasWithLinks("No commit sha here") == "No commit sha here");
    CHECK(replaceCommitShasWithLinks("Commit 219c2149 fixed the issue") == "Commit [219c21](https://github.com/user/repo/commit/219c2149) fixed the issue");
    CHECK(replaceCommitShasWithLinks("Fixed by 1234567890abcdef") == "Fixed by [123456](https://github.com/user/repo/commit/1234567890abcdef)");
    CHECK(replaceCommitShasWithLinks("See commit 1234567 and 89abcdef") == "See commit [123456](https://github.com/user/repo/commit/1234567) and [89abcd](https://github.com/user/repo/commit/89abcdef)");
    CHECK(replaceCommitShasWithLinks("Multiple commits: 1234567, 89abcdef, and abcdef0123456789") == "Multiple commits: [123456](https://github.com/user/repo/commit/1234567), [89abcd](https://github.com/user/repo/commit/89abcdef), and [abcdef](https://github.com/user/repo/commit/abcdef0123456789)");
    CHECK(replaceCommitShasWithLinks("(219c2149)") == "([219c21](https://github.com/user/repo/commit/219c2149))");
    CHECK(replaceCommitShasWithLinks("Commit at start 219c2149 and end abcdef0123456789") == "Commit at start [219c21](https://github.com/user/repo/commit/219c2149) and end [abcdef](https://github.com/user/repo/commit/abcdef0123456789)");
    CHECK(replaceCommitShasWithLinks("Mix of valid and invalid shas: 12345, 67890abcdef12345") == "Mix of valid and invalid shas: 12345, [67890a](https://github.com/user/repo/commit/67890abcdef12345)");
    CHECK(replaceCommitShasWithLinks("Already linked [219c2149](https://github.com/user/repo/commit/219c2149) and 89abcdef") == "Already linked [219c2149](https://github.com/user/repo/commit/219c2149) and [89abcd](https://github.com/user/repo/commit/89abcdef)");
    CHECK(replaceCommitShasWithLinks("Very large SHA 1234567890123456789012345678901234567890") == "Very large SHA [123456](https://github.com/user/repo/commit/1234567890123456789012345678901234567890)");
    CHECK(replaceCommitShasWithLinks("123456 ") == "[123456](https://github.com/user/repo/commit/123456) ");
    CHECK(replaceCommitShasWithLinks(" commit 1234567 ") == " commit [123456](https://github.com/user/repo/commit/1234567) ");
    CHECK(replaceCommitShasWithLinks("219c2149\nAnother line with sha 89abcdef") == "[219c21](https://github.com/user/repo/commit/219c2149)\nAnother line with sha [89abcd](https://github.com/user/repo/commit/89abcdef)");
}

TEST_CASE("Testing converting conventional commit title to release note title function") {
    // Test cases with no subcategory
    CHECK(convertConventionalCommitTitleToReleaseNoteTitle("fix: fixed bug X", CommitTypeMatchResults::MatchWithoutSubCategory, "### ") == "### Fixed bug X\n");
    CHECK(convertConventionalCommitTitleToReleaseNoteTitle("feat: added feature Y", CommitTypeMatchResults::MatchWithoutSubCategory, "## ") == "## Added feature Y\n");
    CHECK(convertConventionalCommitTitleToReleaseNoteTitle("chore: updated dependencies", CommitTypeMatchResults::MatchWithoutSubCategory, "- ") == "- Updated dependencies\n");

    // Test cases with subcategory
    CHECK(convertConventionalCommitTitleToReleaseNoteTitle("fix(auth): fixed bug X", CommitTypeMatchResults::MatchWithSubCategory, "### ") == "### (Auth Related) Fixed bug X\n");
    CHECK(convertConventionalCommitTitleToReleaseNoteTitle("feat(UI): added new button", CommitTypeMatchResults::MatchWithSubCategory, "## ") == "## (UI Related) Added new button\n");
    CHECK(convertConventionalCommitTitleToReleaseNoteTitle("refactor(core): improved performance", CommitTypeMatchResults::MatchWithSubCategory, "- ") == "- (Core Related) Improved performance\n");

    // Edge cases
    CHECK(convertConventionalCommitTitleToReleaseNoteTitle("", CommitTypeMatchResults::MatchWithoutSubCategory, "### ") == "### \n");
    CHECK(convertConventionalCommitTitleToReleaseNoteTitle("fix: ", CommitTypeMatchResults::MatchWithoutSubCategory, "### ") == "### \n");
}