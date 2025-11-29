#include "Wad.h"
#include <unistd.h>
#include <fcntl.h>
#include <functional> 
#include <cstring>   
// #include <iostream> // for debug printTree

using namespace std;

// -------- Static Constructor --------
Wad* Wad::loadWad(const string &path) {

    Wad* wad = new Wad(path);

    int descriptor = open(path.c_str(), O_RDONLY);
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
    wad->loadFileData();

    // wad->printTree(); // debug


    return wad;
}

// -------- Private Constructor --------
Wad::Wad(const string &path)
        : fileDescriptor(-1), wadPath(path), magic(""), descriptorCount(0),
      descriptorOffset(0), root(nullptr) {
}

// Destructor
Wad::~Wad() {
    saveWad();
    close(fileDescriptor);   
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
    if (path == "") return false;
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






// -------- Mutating Methods --------
void Wad::createDirectory(const string &path) {
    // ---------- 1. Normalize the path ----------
    string cleaned = path;

    while (!cleaned.empty() && cleaned.front() == '/')
        cleaned.erase(cleaned.begin());
    while (!cleaned.empty() && cleaned.back() == '/')
        cleaned.pop_back();

    if (cleaned.empty())
        return; // "/", "//", or ""

    // ---------- 2. Tokenize ----------
    vector<string> parts = tokenize(cleaned);
    if (parts.empty())
        return;

    // ========== 3. PRECHECK RULES FOR TEST CASES ==========
    // We check *parents* first, before doing any creation.
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
                // Test 5: parent directory does NOT exist → abort completely
                return;
            }

            // Test 7: cannot create inside a map directory
            if (isMapMarker(comp)) {
                return;
            }
        }
    }

    // The directory we want to create
    const string &last = parts.back();

    // Test 6: Cannot create top-level map directory (E1M1, E2M3, etc.)
    if (isMapMarker(last)) {
        return;
    }

    // Test 4: Directory name too long (>2 chars)
    // Example: "/Gl/exam/" → "exam" is invalid
    if (last.size() > 2) {
        return;
    }

    // ---------- 4. Actual Creation ----------
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

            next->offset = 0;        // saveWad will fill properly
            pathMap[absPath] = next; // update map
        }

        curr = next;
    }

    // printTree(); // Debug
}

void Wad::createFile(const string &path) {
    if (path.empty()) return;

    // Normalize: remove leading/trailing slashes
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

    // Build parent path; if no parent components, parent is root ("/")
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
        // parent doesn't exist
        return;
    }
    if (!parent->isDirectory) {
        // parent is not a directory
        return;
    }

    // Helper: strip _START/_END suffix when checking map markers
    auto stripStartEnd = [](const string &n) -> string {
        if (n.size() > 6 && n.compare(n.size()-6, 6, "_START") == 0)
            return n.substr(0, n.size()-6);
        if (n.size() > 4 && n.compare(n.size()-4, 4, "_END") == 0)
            return n.substr(0, n.size()-4);
        return n;
    };

    // Reject creating files inside map marker directories
    string parentNameStripped = stripStartEnd(parent->name);
    if (isMapMarker(parentNameStripped)) return;

    // Check duplicate name in parent (compare exact names for files; directories may have _START)
    for (Node* c : parent->children) {
        if (!c) continue;
        if (!c->isDirectory && c->name == filename) {
            // file already exists
            return;
        }
        // Also reject if a directory exists with same base name (e.g., "foo" and "foo_START")
        if (c->isDirectory) {
            string childBase = stripStartEnd(c->name);
            if (childBase == filename) return;
        }
    }

    // 2. Do not allow creating a file whose name matches a map marker
    if (isMapMarker(filename))
        return;

    // ---- Validate filename according to WAD lump rules ----

    // Enforce maximum lump name length (8 chars)
    if (filename.size() > 8)
        return;



    // Create new file node (virtual only)
    Node* fileNode = new Node(filename, false);
    fileNode->parent = parent;
    fileNode->offset = 0; // will be assigned by saveWad()
    fileNode->length = 0;

    // Insert as last child so it will be written before any _END marker
    parent->children.push_back(fileNode);

    // Add to pathMap with absolute path (root case handled)
    string fullPath;
    if (parentPath == "/") fullPath = "/" + filename;
    else fullPath = parentPath + "/" + filename;
    pathMap[fullPath] = fileNode;

    // printTree(); // Debug
}

int Wad::writeToFile(const string &path, const char *buffer, int length, int offset) {
    // Basic validation
    if (path.empty()) return -1;
    if (!buffer && length > 0) return -1; // nothing to copy
    if (length < 0) return -1;
    if (offset < 0) return -1;

    // Lookup node
    Node* node = lookupNode(path);
    if (!node) return -1;

    // Path must represent content, not directory
    if (node->isDirectory) return -1;

    // If the file already has content (non-zero length) we must not overwrite it:
    // per spec, return 0 to indicate valid path but write failed because file exists.
    if (node->length > 0) return 0;

    // If requested write length is zero, nothing to do, return 0 bytes written.
    if (length == 0) return 0;

    // Ensure node->data exists and is large enough to hold offset + length bytes.
    // (Assumes Node has: vector<uint8_t> data;)
    size_t requiredSize = static_cast<size_t>(offset) + static_cast<size_t>(length);
    if (node->data.size() < requiredSize) {
        node->data.resize(requiredSize);
    }

    // Copy bytes from user buffer into node->data at given offset
    memcpy(node->data.data() + offset, buffer, static_cast<size_t>(length));

    // Update node length to reflect actual stored data size
    node->length = static_cast<uint32_t>(node->data.size());

    // If node->offset currently 0 and this node was a virtual/created file (previously length==0),
    // you may want to mark its offset for saveWad to rewrite. We'll leave offset alone here:
    // saveWad() should compute and assign final offsets for all lumps.
    //
    // If you prefer a temporary offset marker for in-memory-only lumps, set it here (optional):
    // if (node->offset == 0) node->offset = 0; // no-op / placeholder

    // Return number of bytes copied
    return length;
}









// -------- Internal Helpers --------
void Wad::loadHeader() {
    if (fileDescriptor < 0) return;
    
    // Header layout (classic WAD): 4-byte magic, uint32 descriptor count, uint32 descriptor offset
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

        // Trim trailing NULs and spaces
        int nlen = 8;
        while (nlen > 0 && (nameBytes[nlen - 1] == '\0' || nameBytes[nlen - 1] == ' '))
            --nlen;

        bool hasSlash = (nlen > 0 && nameBytes[nlen - 1] == '/');
        if (hasSlash) --nlen;

        d.name.assign(nameBytes, nameBytes + nlen);
        if (hasSlash) d.name += '/'; // preserve trailing slash for WAD descriptor

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

        // Namespace end → pop
        // Namespace end → pop matching start
        if (isNamespaceEnd(name)) {
            string target = name.substr(0, name.size()-4); // remove _END

            // Pop until we find the matching start
            while (stack.size() > 1) { // don't pop root
                Node* top = stack.back();
                string topNameClean = this->cleanName(top->name);
                if (topNameClean == target) {
                    stack.pop_back(); // found matching start
                    break;
                } else {
                    stack.pop_back(); // intermediate node, keep popping
                }
            }
            continue; // move to next descriptor
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

        // **New: manually created directories (length=0, trailing '/')**
        if (d.length == 0 && !name.empty() && name.back() == '/') {
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

    // Skip leading slashes
    while (i < n && path[i] == '/')
        i++;

    while (i < n) {
        size_t start = i;

        // Find next slash
        while (i < n && path[i] != '/')
            i++;

        // Extract component
        if (i > start) {
            string part = path.substr(start, i - start);

            // ignore "." and ".." because WAD has no parent dirs
            if (part != "." && part != "..")
                result.push_back(part);
        }

        // Move past repeated slashes
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

    // Header size: magic (4) + descriptor count (4) + descriptor offset (4)
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

    // Serialize tree (skip root itself)
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