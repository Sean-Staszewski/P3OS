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
    ~Wad(); // saves any changes and closes file

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
    // Private Constructor: only loadWad() calls it
    Wad(const string &path);

    // Helper Structures
    struct Descriptor {
        uint32_t offset;
        uint32_t length;
        string name;         // decoded from 8 bytes
    };

    struct Node { // represents a file or directory in the WAD tree
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

    Node* root;
    unordered_map<string, Node*> pathMap;
    vector<Descriptor> descriptors;
    unordered_map<string, vector<char>> virtualFileData;


    // WAD attributes
    int fileDescriptor;                // POSIX file descriptor
    string wadPath;               // real filesystem path

    string magic;
    uint32_t descriptorCount;
    uint32_t descriptorOffset;



    // helpers
    void loadHeader();
    void loadDescriptors();
    void buildTree();
    void loadFileData();

    Node* lookupNode(const string &path) const;

    bool isMapMarker(const string &name) const; // E#M# checker

    vector<string> tokenize(const string &path) const; // cuts up path into its parts

    // void printTree() const; // for debugging

    void saveWad(); // saves all data stored virtually back into WAD file

    string cleanName(const std::string &name) const; // cleans _START and _END markers directory names
};
