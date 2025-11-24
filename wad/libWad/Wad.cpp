#include "Wad.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

using namespace std;

// -------- Static Constructor --------
Wad* Wad::loadWad(const string &path) {
    Wad* wad = new Wad(path);

    int descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor < 0) {
        delete wad;
        return nullptr;
    }

    wad->fileDescriptor = descriptor;

    // Load WAD internals (header, descriptors, tree).
    // Individual loaders are responsible for handling errors.
    wad->loadHeader();
    wad->loadDescriptors();
    wad->buildTree();

    return wad;
}

// -------- Private Constructor --------
Wad::Wad(const string &path)
        : fileDescriptor(-1), wadPath(path), magic(""), descriptorCount(0),
      descriptorOffset(0), root(nullptr) {
}

// -------- Accessor Methods --------
string Wad::getMagic() const {
    return magic;
}

bool Wad::isContent(const string &path) const {
    Node* node = lookupNode(path);
    if (!node) return false;
    return !node->isDirectory;
}

bool Wad::isDirectory(const string &path) const {
    Node* node = lookupNode(path);
    if (!node) return false;
    return node->isDirectory;
}

int Wad::getSize(const string &path) const {
    Node* node = lookupNode(path);
    if (!node) return -1;
    if (node->isDirectory) return -1;
    // length is stored as uint32_t in Node; return as int (truncate if too large)
    return static_cast<int>(node->length);
}

int Wad::getContents(const string &path, char *buffer, int length, int offset) {
    // Determine available size using public API.
    int availInt = getSize(path);
    if (availInt < 0) return -1; // path not content or doesn't exist

    if (offset < 0) return -1;
    if (length == 0) return 0;
    if (length < 0) return -1;

    uint32_t avail = static_cast<uint32_t>(availInt);
    if (static_cast<uint32_t>(offset) >= avail) return 0;

    size_t toRead = static_cast<size_t>(length);
    if (toRead > static_cast<size_t>(avail - offset))
        toRead = static_cast<size_t>(avail - offset);

    if (toRead == 0) return 0;
    if (!buffer) return -1; // buffer must be provided

    Node* node = lookupNode(path);
    if (!node) return -1;

    size_t totalRead = 0;
    while (totalRead < toRead) {
        off_t readOffset = static_cast<off_t>(node->offset) + static_cast<off_t>(offset + static_cast<int>(totalRead));
        ssize_t r = ::pread(fileDescriptor, buffer + totalRead, toRead - totalRead, readOffset);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break; // EOF
        totalRead += static_cast<size_t>(r);
    }

    return static_cast<int>(totalRead);
}

int Wad::getDirectory(const string &path, vector<string> *directory) {
    if (!directory) return -1;
    Node* node = lookupNode(path);
    if (!node) return -1;
    if (!node->isDirectory) return -1;

    directory->clear();
    for (Node* child : node->children) {
        directory->push_back(child->name);
    }
    return static_cast<int>(directory->size());
}






// -------- Mutating Methods --------
void Wad::createDirectory(const string &path) {
}

void Wad::createFile(const string &path) {
}

int Wad::writeToFile(const string &path, const char *buffer, int length, int offset) {
    return -1;
}







// -------- Internal Helpers --------
void Wad::loadHeader() {
    if (fileDescriptor < 0) return;

    // Header layout (classic WAD): 4-byte magic, uint32 descriptor count, uint32 descriptor offset
    ::lseek(fileDescriptor, 0, SEEK_SET);

    char magicBuf[4];
    ssize_t r = ::read(fileDescriptor, magicBuf, 4);
    if (r != 4) return;
    magic.assign(magicBuf, 4);

    uint32_t count = 0;
    uint32_t offset = 0;
    r = ::read(fileDescriptor, &count, sizeof(count));
    if (r != (ssize_t)sizeof(count)) return;
    r = ::read(fileDescriptor, &offset, sizeof(offset));
    if (r != (ssize_t)sizeof(offset)) return;

    descriptorCount = count;
    descriptorOffset = offset;

}

void Wad::loadDescriptors() {
    if (fileDescriptor < 0) return;
    if (descriptorCount == 0) return;

    // Seek to the descriptor table
    ::lseek(fileDescriptor, static_cast<off_t>(descriptorOffset), SEEK_SET);

    descriptors.clear();
    descriptors.reserve(descriptorCount);

    for (uint32_t i = 0; i < descriptorCount; ++i) {
        Descriptor d;
        // Read offset and length (uint32 each)
        ssize_t r = ::read(fileDescriptor, &d.offset, sizeof(d.offset));
        if (r != (ssize_t)sizeof(d.offset)) break;
        r = ::read(fileDescriptor, &d.length, sizeof(d.length));
        if (r != (ssize_t)sizeof(d.length)) break;

        // Read name (8 bytes)
        char nameBytes[8];
        r = ::read(fileDescriptor, nameBytes, 8);
        if (r != 8) break;

        // Trim trailing NULs and spaces
        int nlen = 8;
        while (nlen > 0 && (nameBytes[nlen-1] == '\0' || nameBytes[nlen-1] == ' ')) --nlen;
        d.name.assign(nameBytes, nameBytes + nlen);

        descriptors.push_back(d);
    }

}

void Wad::buildTree() {
    // Create root node and clear any previous state
    root = new Node("", true);
    root->parent = nullptr;
    pathMap.clear();
    pathMap["/"] = root;

    // Use a stack to manage nested namespaces
    vector<Node*> stack;
    stack.push_back(root);

    for (const Descriptor &d : descriptors) {
        const string &name = d.name;

        // detect namespace start (suffix "_START")
        bool isStart = (name.size() > 6 && name.compare(name.size() - 6, 6, "_START") == 0);
        bool isEnd = (name.size() > 4 && name.compare(name.size() - 4, 4, "_END") == 0);

        if (isStart) {
            string dirName = name.substr(0, name.size() - 6);
            Node* dir = new Node(dirName, true);
            dir->parent = stack.back();
            stack.back()->children.push_back(dir);
            stack.push_back(dir);

            // compute absolute path for this dir
            string abs;
            Node* cur = dir;
            vector<string> parts;
            while (cur && cur != root) {
                parts.push_back(cur->name);
                cur = cur->parent;
            }
            for (auto it = parts.rbegin(); it != parts.rend(); ++it) abs += "/" + *it;
            if (abs.empty()) abs = "/";
            pathMap[abs] = dir;

        } else if (isEnd) {
            if (stack.size() > 1) stack.pop_back();

        } else {
            // Regular lump -> file node
            Node* file = new Node(name, false);
            file->parent = stack.back();
            file->offset = d.offset;
            file->length = d.length;
            stack.back()->children.push_back(file);

            // compute absolute path for file
            string abs;
            Node* cur = file;
            vector<string> parts;
            while (cur && cur != root) {
                parts.push_back(cur->name);
                cur = cur->parent;
            }
            for (auto it = parts.rbegin(); it != parts.rend(); ++it) abs += "/" + *it;
            if (abs.empty()) abs = "/";
            pathMap[abs] = file;
        }
    }

}

Wad::Node* Wad::lookupNode(const string &path) const {
    // Normalize: ensure leading '/', remove trailing '/' (except root)
    string p = path;
    if (p.empty()) p = "/";
    if (p[0] != '/') p = string("/") + p;
    if (p.size() > 1 && p.back() == '/') p.pop_back();

    auto it = pathMap.find(p);
    if (it == pathMap.end()) return nullptr;
    return it->second;
}

bool Wad::isMapMarker(const string &name) const {
    return false;
}

bool Wad::isNamespaceStart(const string &name) const {
    return false;
}

bool Wad::isNamespaceEnd(const string &name) const {
    return false;
}

string Wad::trimDirectoryMarker(const string &name) const {
    return "";
}

vector<string> Wad::tokenize(const string &path) const {
    return {};
}

void Wad::shiftDescriptorList(int bytes) {
}

void Wad::shiftLumpData(int bytes) {
}

string Wad::buildAbsolutePath(Node* node) const {
    return "";
}
