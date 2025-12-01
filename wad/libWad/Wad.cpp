#include "Wad.h"
#include <unistd.h>
#include <fcntl.h>
#include <functional>
#include <cstring>

using namespace std;

// Static Constructor
Wad* Wad::loadWad(const string &path) {

    Wad* wad = new Wad(path);

    int descriptor = open(path.c_str(), O_RDONLY);
    if (descriptor < 0) {
        delete wad;
        return nullptr;
    }

    wad->fileDescriptor = descriptor;

    wad->loadHeader();
    wad->loadDescriptors();
    wad->buildTree();
    wad->loadFileData();

    // wad->printTree(); // debug

    return wad;
}

// Private Constructor 
Wad::Wad(const string &path)
        : fileDescriptor(-1), wadPath(path), magic(""), descriptorCount(0),
      descriptorOffset(0), root(nullptr) {
}

// Destructor
Wad::~Wad() {
    saveWad();
    close(fileDescriptor);   
}

// Getters
string Wad::getMagic() const {
    return magic;
}

bool Wad::isContent(const string &path) const {
    Node* node = lookupNode(path);
    if (!node) return false;
    return !node->isDirectory;
}

bool Wad::isDirectory(const string &path) const {
    if (path == "") return false;
    Node* node = lookupNode(path);
    if (!node) return false;
    return node->isDirectory;
}

int Wad::getSize(const string &path) const {
    Node* node = lookupNode(path);
    if (!node) return -1;
    if (node->isDirectory) return -1;
    return static_cast<int>(node->length); //length is uint32
}

int Wad::getContents(const string &path, char *buffer, int length, int offset) {
    if (!buffer || length <= 0) return -1;

    Node* node = lookupNode(path);
    if (!node || node->isDirectory) return -1; // must be a file

    // Out-of-range offset means no bytes available
    if (offset < 0 || offset >= static_cast<int>(node->data.size()))
        return 0;

    // Compute how many bytes we can copy
    int available = static_cast<int>(node->data.size()) - offset;
    int toCopy = min(length, available);

    if (toCopy > 0)
        copy(node->data.begin() + offset,
                  node->data.begin() + offset + toCopy,
                  buffer);

    return toCopy;
}

int Wad::getDirectory(const string &path, vector<string> *directory) {
    if (!directory || path == "") return -1;
    Node* node = lookupNode(path);
    if (!node || !node->isDirectory) return -1;

    directory->clear();

    for (Node* child : node->children) {
        directory->push_back(this->cleanName(child->name));
    }
    return static_cast<int>(directory->size());
}




// Setters
void Wad::createDirectory(const string &path) {
    // Normalize the path 
    string cleaned = path;

    while (!cleaned.empty() && cleaned.front() == '/')
        cleaned.erase(cleaned.begin());
    while (!cleaned.empty() && cleaned.back() == '/')
        cleaned.pop_back();

    if (cleaned.empty())
        return; // "/", "//", or ""

    //  2. Tokenize 
    vector<string> parts = tokenize(cleaned);
    if (parts.empty())
        return;

    // Check *parents* first
    {
        Node* temp = root;

        for (size_t i = 0; i < parts.size() - 1; i++) {
            const string &comp = parts[i];
            bool found = false;

            // Walk children
            for (Node* c : temp->children) {
                string name = c->name;

                // Strip "_START"
                if (name.size() > 6 && name.substr(name.size() - 6) == "_START")
                    name = name.substr(0, name.size() - 6);

                if (c->isDirectory && name == comp) {
                    found = true;
                    temp = c;
                    break;
                }
            }

            if (!found) {
                return;
            }

            // Cannot create inside a map directory
            if (isMapMarker(comp)) {
                return;
            }
        }
    }

    // The directory we want to create
    const string &last = parts.back();

    // Cannot create inside a map directory
    if (isMapMarker(last)) {
        return;
    }

    // check size
    if (last.size() > 2) {
        return;
    }

    // Finally, we create directory
    Node* curr = root;
    string absPath;

    for (const string &component : parts) {
        absPath += "/";
        absPath += component;

        Node* next = nullptr;

        // Find existing directory
        for (Node* c : curr->children) {
            string name = c->name;

            // Strip "_START"
            if (name.size() > 6 && name.substr(name.size() - 6) == "_START")
                name = name.substr(0, name.size() - 6);

            if (c->isDirectory && name == component) {
                next = c;
                break;
            }
        }

        // Create if missing
        if (!next) {
            string nodeName = component + "_START";

            next = new Node(nodeName, true);
            next->parent = curr;
            curr->children.push_back(next);

            next->offset = 0;
            pathMap[absPath] = next;
        }

        curr = next;
    }

    // printTree(); // Debug
}

void Wad::createFile(const string &path) {
    if (path.empty()) return;

    // Normalize
    string cleaned = path;
    while (!cleaned.empty() && cleaned.front() == '/') cleaned.erase(cleaned.begin());
    while (!cleaned.empty() && cleaned.back() == '/') cleaned.pop_back();

    if (cleaned.empty()) return; // path was "/" or similar

    // Tokenize components
    vector<string> parts = tokenize(cleaned);
    if (parts.empty()) return;

    // Filename is last component
    string filename = parts.back();
    parts.pop_back();

    // Build parent path
    string parentPath = "/";
    if (!parts.empty()) {
        parentPath.clear();
        for (const auto &p : parts) {
            parentPath += "/";
            parentPath += p;
        }
    }

    // Lookup parent node
    Node* parent = lookupNode(parentPath);
    if (!parent) {
        // parent does not exist
        return;
    }
    if (!parent->isDirectory) {
        // parent is not a directory
        return;
    }

    // Cant create stuff in E#M# directories
    string parentNameStripped = cleanName(parent->name);
    if (isMapMarker(parentNameStripped)) return;

    for (Node* c : parent->children) {
        if (!c) continue;
        if (!c->isDirectory && c->name == filename) {
            // file already exists
            return;
        }
        // Also reject if a directory exists with same base name (e.g., "foo" and "foo_START")
        if (c->isDirectory) {
            string childBase = cleanName(c->name);
            if (childBase == filename) return;
        }
    }

    // cant make map markers
    if (isMapMarker(filename))
        return;

    // WAD rule checks

    // Enforce maximum lump name length (8 chars)
    if (filename.size() > 8)
        return;



    Node* fileNode = new Node(filename, false);
    fileNode->parent = parent;
    fileNode->offset = 0; 
    fileNode->length = 0;

    parent->children.push_back(fileNode);

    string fullPath;
    if (parentPath == "/") fullPath = "/" + filename;
    else fullPath = parentPath + "/" + filename;
    pathMap[fullPath] = fileNode;

    // printTree(); // Debug
}

int Wad::writeToFile(const string &path, const char *buffer, int length, int offset) {
    // validation
    if (path.empty()) return -1;
    if (!buffer && length > 0) return -1; // nothing to copy
    if (length < 0) return -1;
    if (offset < 0) return -1;

    Node* node = lookupNode(path);
    if (!node) return -1;
    if (node->isDirectory) return -1;

    // file not empty, cannot write
    if (node->length > 0) return 0;

    // nothing to write
    if (length == 0) return 0;

    // fix size of file data buffer if necessary
    size_t requiredSize = static_cast<size_t>(offset) + static_cast<size_t>(length);
    if (node->data.size() < requiredSize) {
        node->data.resize(requiredSize);
    }

    // Copy bytes from buffer into node->data
    memcpy(node->data.data() + offset, buffer, static_cast<size_t>(length));
    // Update node length
    node->length = static_cast<uint32_t>(node->data.size());

    return length;
}




// Helpers
void Wad::loadHeader() {
    if (fileDescriptor < 0) return;
    
    // Header layout: 4-byte magic, 4-byte = uint32 descriptor count, 4-byte = uint32 descriptor offset
    lseek(fileDescriptor, 0, SEEK_SET);

    char magicBuf[4];
    ssize_t r = read(fileDescriptor, magicBuf, 4);
    if (r != 4) return;
    magic.assign(magicBuf, 4);

    uint32_t count = 0;
    uint32_t offset = 0;
    r = read(fileDescriptor, &count, sizeof(count));
    if (r != (ssize_t)sizeof(count)) return;
    r = read(fileDescriptor, &offset, sizeof(offset));
    if (r != (ssize_t)sizeof(offset)) return;

    descriptorCount = count;
    descriptorOffset = offset;
}

void Wad::loadDescriptors() {
    if (fileDescriptor < 0 || descriptorCount == 0) return;

    lseek(fileDescriptor, static_cast<off_t>(descriptorOffset), SEEK_SET);

    descriptors.clear();
    descriptors.reserve(descriptorCount);

    for (uint32_t i = 0; i < descriptorCount; ++i) {
        Descriptor d;
        ssize_t r = read(fileDescriptor, &d.offset, sizeof(d.offset));
        if (r != (ssize_t)sizeof(d.offset)) break;
        r = read(fileDescriptor, &d.length, sizeof(d.length));
        if (r != (ssize_t)sizeof(d.length)) break;

        char nameBytes[8];
        r = read(fileDescriptor, nameBytes, 8);
        if (r != 8) break;

        int nlen = 8;
        while (nlen > 0 && (nameBytes[nlen - 1] == '\0' || nameBytes[nlen - 1] == ' '))
            --nlen;

        bool hasSlash = (nlen > 0 && nameBytes[nlen - 1] == '/');
        if (hasSlash) --nlen;

        d.name.assign(nameBytes, nameBytes + nlen);
        if (hasSlash) d.name += '/';

        descriptors.push_back(d);
    }
}

void Wad::buildTree() {
    // Reset state
    root = new Node("", true);
    root->parent = nullptr;
    pathMap.clear();
    pathMap["/"] = root;

    vector<Node*> stack;
    stack.push_back(root);

    auto addPath = [&](Node* n) {
        string abs = "";
        Node* cur = n;
        vector<string> parts;
        while (cur && cur != root) {
            string cleaned = this->cleanName(cur->name);
            if (!cleaned.empty())  // skip empty names
                parts.push_back(cleaned);
            cur = cur->parent;
        }
        for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
            if (!abs.empty())
                abs += "/";
            abs += *it;
        }
        if (abs.empty()) abs = "/";
        else abs = "/" + abs;  // ensure leading slash
        pathMap[abs] = n;
    };

    auto isNamespaceStart = [](const string& nm) {
        return nm.size() > 6 && nm.compare(nm.size() - 6, 6, "_START") == 0;
    };
    auto isNamespaceEnd = [](const string& nm) {
        return nm.size() > 4 && nm.compare(nm.size() - 4, 4, "_END") == 0;
    };

    auto isEMDirectory = [&](size_t i) {
        if (!isMapMarker(descriptors[i].name)) return false;
        if (i + 1 >= descriptors.size()) return true; // last descriptor = directory
        const Descriptor &cur  = descriptors[i];
        const Descriptor &next = descriptors[i + 1];
        bool nextIsNamespaceStart = isNamespaceStart(next.name);
        bool offsetMismatch = (next.offset != cur.offset + cur.length);
        bool zeroLengthMarker = (cur.length == 0);
        return nextIsNamespaceStart || offsetMismatch || zeroLengthMarker;
    };

    for (size_t i = 0; i < descriptors.size(); ++i) {
        const Descriptor &d = descriptors[i];
        const string &name = d.name;

        // Close EM directories if necessary
        while (stack.size() > 1) {
            Node* top = stack.back();
            if (!(top->isDirectory && isMapMarker(top->name))) break;
            if (isNamespaceStart(name)) { stack.pop_back(); continue; }
            if (top->children.empty()) break;
            Node* lastChild = top->children.back();
            if (!lastChild->isDirectory && d.offset != lastChild->offset + lastChild->length) {
                stack.pop_back();
                continue;
            }
            break;
        }

        // Namespace start → directory
        if (isNamespaceStart(name)) {
            Node* dir = new Node(name, true);
            dir->parent = stack.back();
            stack.back()->children.push_back(dir);
            stack.push_back(dir);
            addPath(dir);
            continue;
        }

        // Namespace end → pop matching start
        if (isNamespaceEnd(name)) {
            string target = name.substr(0, name.size()-4); // remove _END

            while (stack.size() > 1) { // don't pop root
                Node* top = stack.back();
                string topNameClean = this->cleanName(top->name);
                if (topNameClean == target) {
                    stack.pop_back();
                    break;
                } else {
                    stack.pop_back();
                }
            }
            continue;
        }

        // EM directories
        if (isEMDirectory(i)) {
            Node* dir = new Node(name, true);
            dir->parent = stack.back();
            stack.back()->children.push_back(dir);
            stack.push_back(dir);
            addPath(dir);
            continue;
        }

        // Regular file
        Node* file = new Node(name, false);
        file->parent = stack.back();
        file->offset = d.offset;
        file->length = d.length;
        stack.back()->children.push_back(file);
        addPath(file);
    }
}

void Wad::loadFileData()
{
    for (auto &pair : pathMap)
    {
        Node* node = pair.second;
        if (!node->isDirectory && node->length > 0)
        {
            node->data.resize(node->length);
            off_t off = static_cast<off_t>(node->offset);

            ssize_t r = pread(fileDescriptor, node->data.data(), node->length, off);
            if (r < 0) {
                perror("pread");
                node->data.clear();
            }
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
    // If your WAD uses E#M# markers as maps:
    if (name.size() == 4 && name[0] == 'E' && isdigit(name[1]) &&
        name[2] == 'M' && isdigit(name[3])) {
        return true;
    }
    return false;
}

vector<string> Wad::tokenize(const string &path) const {
    vector<string> result;
    if (path.empty()) return result;

    size_t i = 0;
    size_t n = path.size();

    while (i < n && path[i] == '/')
        i++;

    while (i < n) {
        size_t start = i;

        while (i < n && path[i] != '/')
            i++;

        if (i > start) {
            string part = path.substr(start, i - start);

            if (part != "." && part != "..")
                result.push_back(part);
        }

        while (i < n && path[i] == '/')
            i++;
    }

    return result;
}

// Debug Method: Print tree structure

// void Wad::printTree() const {
//     function<void(const Node*, int)> rec;

//     rec = [&](const Node* n, int depth) {
//         string indent(depth * 2, ' ');
//         if (n->isDirectory) {
//             string printName = n->name;
//             // Optionally strip _START/_END for nicer output
//             if (printName.size() > 6 && printName.compare(printName.size()-6,6,"_START")==0)
//                 printName = printName.substr(0, printName.size()-6);
//             else if (printName.size() > 4 && printName.compare(printName.size()-4,4,"_END")==0)
//                 printName = printName.substr(0, printName.size()-4);
//             cout << indent << printName << "/\n";
//         } else {
//             cout << indent << n->name << "\n";
//         }

//         for (const Node* c : n->children)
//             rec(c, depth + 1);
//     };

//     rec(root, 0);
// }

void Wad::saveWad() {
    if (!root) return;

    vector<Descriptor> newDescriptors;
    vector<uint8_t> newLumpData;

    const uint32_t headerSize = 12;

    // Recursive writer
    function<void(Node*)> writeNode = [&](Node* node) {
        if (!node) return;

        // DIRECTORY
        if (node->isDirectory) {
            Descriptor startDesc;
            // store absolute offset: headerSize + current lump-data size
            startDesc.offset = headerSize + static_cast<uint32_t>(newLumpData.size());
            startDesc.length = 0;
            startDesc.name = node->name;
            newDescriptors.push_back(startDesc);

            for (Node* c : node->children)
                writeNode(c);

            if (node->name.size() > 6 &&
                node->name.compare(node->name.size() - 6, 6, "_START") == 0) {
                Descriptor endDesc;
                endDesc.offset = headerSize + static_cast<uint32_t>(newLumpData.size());
                endDesc.length = 0;
                endDesc.name = node->name.substr(0, node->name.size() - 6) + "_END";
                newDescriptors.push_back(endDesc);
            }

            return;
        }

        // FILE (LUMP)
        Descriptor fileDesc;
        // absolute offset into final file:
        fileDesc.offset = headerSize + static_cast<uint32_t>(newLumpData.size());
        fileDesc.length = node->length;
        fileDesc.name = node->name;
        newDescriptors.push_back(fileDesc);

        if (node->length > 0) {
            size_t base = newLumpData.size();
            newLumpData.resize(base + node->length);

            // Prefer in-memory data if present and sized correctly
            if (!node->data.empty() &&
                node->data.size() == static_cast<size_t>(node->length)) {
                memcpy(&newLumpData[base], node->data.data(), node->length);
            } else {
                // fallback: read from original file using node->offset (which is absolute)
                lseek(fileDescriptor, static_cast<off_t>(node->offset), SEEK_SET);
                ssize_t r = read(fileDescriptor, &newLumpData[base], node->length);
                if (r < 0) {
                    // read error: zero-fill to be safe
                    fill(&newLumpData[base], &newLumpData[base + node->length], 0);
                } else if (static_cast<size_t>(r) < static_cast<size_t>(node->length)) {
                    // partial read: zero the remainder
                    fill(&newLumpData[base + r], &newLumpData[base + node->length], 0);
                }
            }
        }
    };

    // start recursive write from root's children
    for (Node* n : root->children)
        writeNode(n);

    // Now write the WAD file
    int fd = open(wadPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) {
        return;
    }

    // Header: magic (4), descriptor count (4), descriptor offset (4)
    write(fd, magic.data(), 4);
    uint32_t descriptorCount = static_cast<uint32_t>(newDescriptors.size());
    // descriptorOffset = header size + lump-data size
    uint32_t descriptorOffset = headerSize + static_cast<uint32_t>(newLumpData.size());
    write(fd, &descriptorCount, sizeof(descriptorCount));
    write(fd, &descriptorOffset, sizeof(descriptorOffset));

    // Lump data block
    if (!newLumpData.empty()) {
        ssize_t w = write(fd, newLumpData.data(), newLumpData.size());
        if (w < 0) {
            close(fd);
            return;
        }
    }

    // Descriptor table (each descriptor: offset (4), length (4), name (8))
    for (auto &desc : newDescriptors) {
        write(fd, &desc.offset, sizeof(desc.offset));
        write(fd, &desc.length, sizeof(desc.length));

        char nameBytes[8] = {};
        size_t copyLen = min<size_t>(desc.name.size(), 8);
        copy(desc.name.begin(), desc.name.begin() + copyLen, nameBytes);
        write(fd, nameBytes, 8);
    }

    close(fd);
}

string Wad::cleanName(const string &name) const {
    if (name.size() > 6 && name.compare(name.size() - 6, 6, "_START") == 0)
        return name.substr(0, name.size() - 6);

    if (name.size() > 4 && name.compare(name.size() - 4, 4, "_END") == 0)
        return name.substr(0, name.size() - 4);

    return name;
}