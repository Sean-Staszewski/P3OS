#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

using namespace std;

class Wad {

public:
    // -------- Static Constructor --------
    static Wad* loadWad(const string &path);

    // -------- Accessor Methods --------
    string getMagic() const;

    bool isContent(const string &path) const;
    bool isDirectory(const string &path) const;

    int getSize(const string &path) const;

    int getContents(const string &path, char *buffer, int length, int offset = 0);

    int getDirectory(const string &path, vector<string> *directory);

    // -------- Mutating Methods --------
    void createDirectory(const string &path);
    void createFile(const string &path);

    int writeToFile(const string &path, const char *buffer, int length, int offset = 0);

private:
    // Private constructor: only loadWad() can call it
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
    Node* root;

    // A map from absolute paths ("/F/F1/file.txt") to Node*
    unordered_map<string, Node*> pathMap;

    // -------- Internal Helper Functions --------
    void loadHeader();
    void loadDescriptors();
    void buildTree();

    Node* lookupNode(const string &path) const;

    bool isMapMarker(const string &name) const;
    bool isNamespaceStart(const string &name) const;
    bool isNamespaceEnd(const string &name) const;

    string trimDirectoryMarker(const string &name) const; // "F1_START" → "F1"

    vector<string> tokenize(const string &path) const;

    // Helpers for modifying descriptor list later
    void shiftDescriptorList(int bytes);
    void shiftLumpData(int bytes);

    string buildAbsolutePath(Node* node) const;
};
