#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

using namespace std;

class Wad {

public:
    // Static Constructor & Destructor
    static Wad* loadWad(const string &path);
    ~Wad();

    // Getters
    string getMagic() const;

    bool isContent(const string &path) const;
    bool isDirectory(const string &path) const;

    int getSize(const string &path) const;

    int getContents(const string &path, char *buffer, int length, int offset = 0);

    int getDirectory(const string &path, vector<string> *directory);

    // Setters
    void createDirectory(const string &path);
    void createFile(const string &path);
    int writeToFile(const string &path, const char *buffer, int length, int offset = 0);

private:
    // Private Constructor: only loadWad() can call it
    Wad(const string &path);

    // -------- Internal Structures --------
    struct Descriptor {
        uint32_t offset;
        uint32_t length;
        string name;         // decoded from 8 bytes
    };

    struct Node {
        string name;
        bool isDirectory;
        uint32_t offset;          // only valid if content file
        uint32_t length;          // only valid if content file
        vector<char> data;
        vector<Node*> children;
        Node* parent;

        Node(string n, bool dir)
            : name(n), isDirectory(dir), offset(0), length(0), parent(nullptr) {}
    };

    // -------- Internal State --------
    int fileDescriptor;                // POSIX file descriptor
    string wadPath;               // real filesystem path

    string magic;
    uint32_t descriptorCount;
    uint32_t descriptorOffset;

    vector<Descriptor> descriptors;
    unordered_map<string, vector<char>> virtualFileData;

    Node* root;

    // A map from absolute paths ("/F/F1/file.txt") to Node*
    unordered_map<string, Node*> pathMap;

    // -------- Internal Helper Functions --------
    void loadHeader();
    void loadDescriptors();
    void buildTree();
    void loadFileData();

    Node* lookupNode(const string &path) const;

    bool isMapMarker(const string &name) const;

    vector<string> tokenize(const string &path) const;

    // void printTree() const; // for debugging

    void saveWad();

    string cleanName(const std::string &name) const;
};
