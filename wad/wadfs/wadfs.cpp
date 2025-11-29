// Simple FUSE daemon that exposes a WAD as a filesystem using the provided libWad
// Implements callbacks: getattr, mknod, mkdir, readdir, read, write

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <sys/stat.h>
#include <errno.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>

#include "../libWad/Wad.h"

using std::string;
using std::vector;

static Wad *g_wad = nullptr;

static int wfs_getattr(const char *path, struct stat *stbuf)
{
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

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
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

static int wfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    (void) mode; (void) rdev;
    if (!g_wad) return -EIO;

    if (g_wad->isContent(path) || g_wad->isDirectory(path)) return -EEXIST;

    g_wad->createFile(path);

    if (g_wad->isContent(path)) return 0;
    return -EIO;
}

static int wfs_mkdir(const char *path, mode_t mode)
{
    (void) mode;
    if (!g_wad) return -EIO;

    if (g_wad->isContent(path) || g_wad->isDirectory(path)) return -EEXIST;

    g_wad->createDirectory(path);

    if (g_wad->isDirectory(path)) return 0;
    return -EIO;
}

static int wfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    (void) fi;
    if (!g_wad) return -EIO;

    int r = g_wad->getContents(path, buf, static_cast<int>(size), static_cast<int>(offset));
    if (r < 0) return -EIO;
    return r;
}

static int wfs_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    (void) fi;
    if (!g_wad) return -EIO;

    int r = g_wad->writeToFile(path, buf, static_cast<int>(size), static_cast<int>(offset));
    if (r < 0) return -EIO;
    return r;
}

// The assignment requires these exact callback function names:
// get_attr, mknod, mkdir, read, write, and readdir
// Provide thin wrappers that call our internal implementations so the
// function names match the spec exactly.
static int get_attr(const char *path, struct stat *stbuf) {
    return wfs_getattr(path, stbuf);
}

static int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi) {
    return wfs_readdir(path, buf, filler, offset, fi);
}

int mknod(const char *path, mode_t mode, dev_t rdev) {
    return wfs_mknod(path, mode, rdev);
}

int mkdir(const char *path, mode_t mode) {
    return wfs_mkdir(path, mode);
}

static int read(const char *path, char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi) {
    return wfs_read(path, buf, size, offset, fi);
}

static int write(const char *path, const char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi) {
    return wfs_write(path, buf, size, offset, fi);
}

static struct fuse_operations wfs_oper;

static void print_usage()
{
    std::cerr << "Usage: wadfs [-s] <WAD_FILE> <MOUNTPOINT>\n";
    std::cerr << "  -s : pass single-threaded flag to FUSE (optional)\n";
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        print_usage();
        return 1;
    }

    bool pass_single = false;
    int argi = 1;
    if (strcmp(argv[argi], "-s") == 0) {
        pass_single = true;
        argi++;
    }

    if (argi + 1 >= argc) {
        print_usage();
        return 1;
    }

    string wadPath = argv[argi++];
    char *mountpoint = argv[argi++];

    // Diagnostic: print what we're about to pass to Wad::loadWad
    std::cerr << "wadfs: argc=" << argc << "\n";
    for (int i = 0; i < argc; ++i) {
        std::cerr << "argv[" << i << "]='" << argv[i] << "'\n";
    }
    std::cerr << "Resolved wadPath='" << wadPath << "'\n";
    std::cerr << "Resolved mountpoint='" << mountpoint << "'\n";

    g_wad = Wad::loadWad(wadPath);
    if (!g_wad) {
        std::cerr << "Failed to load WAD: " << wadPath << std::endl;
        return 1;
    }

    std::vector<char*> fuse_argv;
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
