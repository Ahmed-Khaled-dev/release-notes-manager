// Author: Ahmed Khaled
// Description: This is a script made as an exercise to demonstrate the ability to work on the Synfig GSoC 24
// "Automated release notes generator" project, the script generates markdown release notes from conventional git commits
// Synfig : https://www.synfig.org/
// Project : https://synfig-docs-dev.readthedocs.io/en/latest/gsoc/2024/ideas.html#projects-ideas

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

using namespace std;

const int MAX_DISPLAYED_COMMITS_PER_TYPE = 3;
const int COMMIT_TYPES_COUNT = 3;
const int COMMIT_TYPE_INFO_COUNT = 2;
const string MARKDOWN_OUTPUT_FILE_NAME = "release_notes.md";

enum class Commits{
    Normal,
    Merge
};

// Can later on add HTMl
enum class OutputTypes{
    Markdown
};

enum class ErrorTypes{
    IncorrectCommitType,
    NoCommitType,
};

enum class CommitTypeInfo{
    ConventionalName,
    MarkdownTitle
};

enum class ReleaseNoteModes{
    Short,
    Full
};

string commitTypes[COMMIT_TYPES_COUNT][COMMIT_TYPE_INFO_COUNT] = {
    {"fix", "## 🐛 Bug Fixes"}, 
    {"feat", "## ✨ New Features"},
    {"refactor", "## ♻️ Code Refactoring"}};

void printErrorMessage(ErrorTypes errorType){
    if(errorType == ErrorTypes::IncorrectCommitType){
        cout << "Incorrect commit type!" << endl;
    }
    else if(errorType == ErrorTypes::NoCommitType){
        cout << "Please enter the type of commits you which to use to generate release notes" << endl;
    }
    cout << "Please choose either n (Normal Commits) or m (Merged Commits)" << endl;
    cout << "Expected Syntax:" << endl;
    cout << "1 - release_notes_generator n" << endl;
    cout << "2 - release_notes_generator m [s/f] (short or full release notes - by default short)";
}

string getNormalCommitsNotes(int commitTypeIndex){
    string commandToRetrieveCommitsMessages = "git log --max-count " + to_string(MAX_DISPLAYED_COMMITS_PER_TYPE) + 
    " --no-merges --oneline --format=\"%s\" --grep=\"^" + commitTypes[commitTypeIndex][(int) CommitTypeInfo::ConventionalName] + ": \"";

    FILE* pipe = popen(commandToRetrieveCommitsMessages.c_str(), "r");
    if(!pipe){
        return "Error: Unable to open pipe to read git log commmand output";
    }

    char buffer[150];
    string currentCommitMessage, normalCommitsNotes;
    // Reading commit messages line-by-line from the output of the git log command
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        currentCommitMessage = buffer;
        // Validating again that the commit message actually starts with the wanted type
        if(currentCommitMessage.substr(0, currentCommitMessage.find(":")) == commitTypes[commitTypeIndex][(int) CommitTypeInfo::ConventionalName])
            currentCommitMessage = "- " + currentCommitMessage.substr(currentCommitMessage.find(":") + 2) + "\n";
        
        // Capitalizing the first letter in the commit message
        currentCommitMessage[2] = toupper(currentCommitMessage[2]);

        normalCommitsNotes += currentCommitMessage;
    }

    pclose(pipe);
    return normalCommitsNotes;
}

void generateReleaseNotes(Commits commit, OutputTypes outputType, ReleaseNoteModes releaseNotesMode = ReleaseNoteModes::Short){
    ofstream outputFile;
    if(outputType == OutputTypes::Markdown){
        outputFile.open(MARKDOWN_OUTPUT_FILE_NAME);
        
        if (!outputFile.is_open()){
            cout << "Error: Unable to open markdown release notes file" << endl;
        }
    }
        
    string commandToRetrieveCommitsMessages;
    string currentCommitMessage;
    for (int i = 0; i < COMMIT_TYPES_COUNT; i++)
    {
        // Output the title of this commit type section in the release notes
        outputFile << "\n" << commitTypes[i][(int) CommitTypeInfo::MarkdownTitle] << "\n";

        if(commit == Commits::Normal){
            outputFile << getNormalCommitsNotes(i);
        }
        else if(commit == Commits::Merge){
            
        }
    }
}

int main(int argc, char* argv[]){
    if(argc > 1){
        if(strcmp(argv[1], "n") == 0){
            generateReleaseNotes(Commits::Normal, OutputTypes::Markdown);
        }
        else if(strcmp(argv[1], "m") == 0){
            if(argc > 2 && strcmp(argv[2], "f") == 0){
                generateReleaseNotes(Commits::Merge, OutputTypes::Markdown, ReleaseNoteModes::Full);
            }
            else{
                generateReleaseNotes(Commits::Merge, OutputTypes::Markdown, ReleaseNoteModes::Short);
            }
        }
        else{
            printErrorMessage(ErrorTypes::IncorrectCommitType);
        }
    }
    else{
        printErrorMessage(ErrorTypes::NoCommitType);
    }

    return 0;
}