#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "../libWad/Wad.h"

using namespace std;

static Wad *g_wad = nullptr;

static int get_attr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (!g_wad) return -EIO;

    if (string(path) == "/" || g_wad->isDirectory(path)) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (g_wad->isContent(path)) {
        stbuf->st_mode = S_IFREG | 0777;
        stbuf->st_nlink = 1;
        stbuf->st_size = static_cast<off_t>(g_wad->getSize(path));
        return 0;
    }

    return -ENOENT;
}

static int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    if (!g_wad) return -EIO;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    vector<string> entries;
    int rc = g_wad->getDirectory(path, &entries);
    if (rc < 0) return -ENOENT;

    for (const auto &name : entries) {
        filler(buf, name.c_str(), NULL, 0);
    }

    return 0;
}

int mknod(const char *path, mode_t mode, dev_t rdev) {
    (void) mode; (void) rdev;
    if (!g_wad) return -EIO;

    if (g_wad->isContent(path) || g_wad->isDirectory(path)) return -EEXIST;

    g_wad->createFile(path);

    if (g_wad->isContent(path)) return 0;
    return -EIO;
}

int mkdir(const char *path, mode_t mode) {
    (void) mode;
    if (!g_wad) return -EIO;

    if (g_wad->isContent(path) || g_wad->isDirectory(path)) return -EEXIST;

    g_wad->createDirectory(path);

    if (g_wad->isDirectory(path)) return 0;
    return -EIO;
}

static int read(const char *path, char *buf, size_t size, off_t offset, 
                struct fuse_file_info *fi) {
    (void) fi;
    if (!g_wad) return -EIO;

    int r = g_wad->getContents(path, buf, static_cast<int>(size), static_cast<int>(offset));
    if (r < 0) return -EIO;
    return r;
}

static int write(const char *path, const char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi) {
    (void) fi;
    if (!g_wad) return -EIO;

    int r = g_wad->writeToFile(path, buf, static_cast<int>(size), static_cast<int>(offset));
    if (r < 0) return -EIO;
    return r;
}

static struct fuse_operations wfs_oper;

int main(int argc, char *argv[])
{
    if (argc < 3) {
        return 1;
    }

    bool pass_single = false;
    int argi = 1;
    if (strcmp(argv[argi], "-s") == 0) {
        pass_single = true;
        argi++;
    }

    if (argi + 1 >= argc) {
        return 1;
    }

    string wadPath = argv[argi++];
    char *mountpoint = argv[argi++];

    g_wad = Wad::loadWad(wadPath);
    if (!g_wad) {
        return 1;
    }

    vector<char*> fuse_argv;
    fuse_argv.push_back(argv[0]);
    if (pass_single) fuse_argv.push_back((char*)"-s");
    fuse_argv.push_back(mountpoint);

    int fuse_argc = static_cast<int>(fuse_argv.size());

     /* Initialize operations struct and assign callbacks explicitly
         to avoid designated-initializer ordering issues in C++. */
    memset(&wfs_oper, 0, sizeof(wfs_oper));
    wfs_oper.getattr = get_attr;
    wfs_oper.readdir = readdir;
    wfs_oper.mknod = mknod;
    wfs_oper.mkdir = mkdir;
    wfs_oper.read = read;
    wfs_oper.write = write;

    int ret = fuse_main(fuse_argc, fuse_argv.data(), &wfs_oper, g_wad);

    delete g_wad;
    return ret;
}
