#include <string>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <ctype.h>
#include <algorithm>
#include <cctype>
#include <stack>
#include <regex>
#include "gtest/gtest.h"

#include "Wad.h"




const std::string setupWorkspace(){

        const std::string wad_path = "./testfiles/sample1.wad";
        const std::string test_wad_path = "./testfiles/sample1_copy.wad";

        std::string command = "cp " + wad_path + " " + test_wad_path;
        int returnCode = system(command.c_str());

        if(returnCode == EXIT_FAILURE){
                throw("Copy failure");
        }

        return test_wad_path;
}


TEST(LibReadTests, getMagic){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);
        ASSERT_EQ(testWad->getMagic(), "IWAD");

        delete testWad;
}

TEST(LibReadTests, isContent){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);
        
        //Testing valid content
        ASSERT_TRUE(testWad->isContent("/E1M0/01.txt"));
        ASSERT_TRUE(testWad->isContent("/E1M0/05.txt"));
        ASSERT_TRUE(testWad->isContent("/Gl/ad/os/cake.jpg"));
        ASSERT_TRUE(testWad->isContent("/mp.txt"));

        //Testing invalid content (directories)
        ASSERT_FALSE(testWad->isContent("/E1M0"));
        ASSERT_FALSE(testWad->isContent("/Gl/ad"));
        ASSERT_FALSE(testWad->isContent("/Gl/ad/os"));
        ASSERT_FALSE(testWad->isContent("/"));

        //Testing invalid content (nonexistent files/directories)
        ASSERT_FALSE(testWad->isContent("/Fake/"));
        ASSERT_FALSE(testWad->isContent("/notreal/"));
        ASSERT_FALSE(testWad->isContent("/NotAFile"));
        ASSERT_FALSE(testWad->isContent("/fake.jpg"));
        ASSERT_FALSE(testWad->isContent("/not/a/real/file/text.txt"));
        ASSERT_FALSE(testWad->isContent("/file.mp3"));
        ASSERT_FALSE(testWad->isContent(""));
        
        delete testWad;
}

TEST(LibReadTests, isDirectory){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);
        
        //Testing valid directories
        ASSERT_TRUE(testWad->isDirectory("/E1M0"));
        ASSERT_TRUE(testWad->isDirectory("/Gl/ad"));
        ASSERT_TRUE(testWad->isDirectory("/Gl/ad/os"));
        ASSERT_TRUE(testWad->isDirectory("/"));

        //Testing invalid directories (content))
        ASSERT_FALSE(testWad->isDirectory("/E1M0/01.txt"));
        ASSERT_FALSE(testWad->isDirectory("/E1M0/05.txt"));
        ASSERT_FALSE(testWad->isDirectory("/Gl/ad/os/cake.jpg"));
        ASSERT_FALSE(testWad->isDirectory("/mp.txt"));

        //Testing invalid content (nonexistent files/directories)
        ASSERT_FALSE(testWad->isDirectory("/Fa/"));
        ASSERT_FALSE(testWad->isDirectory("/Dr/"));
        ASSERT_FALSE(testWad->isDirectory("/NotAFile"));
        ASSERT_FALSE(testWad->isDirectory("/fake.jpg"));
        ASSERT_FALSE(testWad->isDirectory("/no/t/ar/ea/l/di/r/text.txt"));
        ASSERT_FALSE(testWad->isDirectory("/file.mp3"));
        ASSERT_FALSE(testWad->isDirectory(""));

        delete testWad;
}

TEST(LibReadTests, getSize){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);
        
        //Testing valid content
        ASSERT_EQ(testWad->getSize("/E1M0/01.txt"), 17);
        ASSERT_EQ(testWad->getSize("/E1M0/05.txt"), 12);
        ASSERT_EQ(testWad->getSize("/E1M0/09.txt"), 10);
        ASSERT_EQ(testWad->getSize("/E1M0/10.txt"), 13);
        ASSERT_EQ(testWad->getSize("/Gl/ad/os/cake.jpg"), 29869);
        ASSERT_EQ(testWad->getSize("/mp.txt"), 398);

        //Testing invalid content (directories)
        ASSERT_EQ(testWad->getSize("/E1M0"), -1);
        ASSERT_EQ(testWad->getSize("/Gl/ad/"), -1);
        ASSERT_EQ(testWad->getSize("/Gl/ad"), -1);
        ASSERT_EQ(testWad->getSize("/Gl/ad/os/"), -1);

        //Testing invalid content (nonexistent files/directories)
        ASSERT_EQ(testWad->getSize("/E1M1"), -1);
        ASSERT_EQ(testWad->getSize("/Ex/"), -1);
        ASSERT_EQ(testWad->getSize("/Fi/le"), -1);
        ASSERT_EQ(testWad->getSize("/file.jpg"), -1);
        ASSERT_EQ(testWad->getSize(""), -1);

        delete testWad;
}

TEST(LibReadTests, getContentsTest1){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getContents Test 1, reading full text file
        std::string testPath = "/E1M0/01.txt";
        char expectedString[] = "He loves to sing\n";
        char buffer[1024];
        memset(buffer, 0, 1024);

        int sizeOfFile = testWad->getSize(testPath);
        ASSERT_EQ(sizeOfFile, 17);

        int retValue = testWad->getContents(testPath, buffer, sizeOfFile);
        ASSERT_EQ(retValue, sizeOfFile);

        ASSERT_EQ(memcmp(expectedString, buffer, sizeOfFile), 0)
        << "Expected string: " << expectedString
        << "Returned string: " << buffer;

        delete testWad;
}

TEST(LibReadTests, getContentsTest2){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getContents Test 2, reading middle of text file using offset
        std::string testPath = "/mp.txt";
        char expectedString[] = "airspeed velocity";
        char buffer[1024];
        memset(buffer, 0, 1024);

        int expectedStringLen = 17;
        int offset = 117;

        int retValue = testWad->getContents(testPath, buffer, expectedStringLen, offset);
        ASSERT_EQ(retValue, expectedStringLen);

        ASSERT_EQ(memcmp(expectedString, buffer, expectedStringLen), 0)
        << "Expected string: " << expectedString << std::endl
        << "Returned string: " << buffer << std::endl;

        delete testWad;
}

TEST(LibReadTests, getContentsTest3){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getContents Test 3, trying to read past end of file (length is too large)
        std::string testPath = "/E1M0/03.txt";
        char expectedString[] = "song\n";
        char buffer[1024];
        memset(buffer, 0, 1024);

        int expectedStringLen = 5;
        int lengthTest = 1000;
        int offset = 7;

        int retValue = testWad->getContents(testPath, buffer, lengthTest, offset);
        ASSERT_EQ(retValue, expectedStringLen);

        ASSERT_EQ(memcmp(expectedString, buffer, expectedStringLen), 0);

        delete testWad;
}

TEST(LibReadTests, getContentsTest4){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getContents Test 4, trying to read past end of file (offset is too large)
        std::string testPath = "/E1M0/03.txt";
        char buffer[1024];
        memset(buffer, 0, 1024);

        int expectedStringLen = 5;
        int lengthTest = 5;
        int offset = 1000;

        int retValue = testWad->getContents(testPath, buffer, lengthTest, offset);
        ASSERT_EQ(retValue, 0);

        delete testWad;
}


TEST(LibReadTests, getContentsTest5){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getContents Test 5, passing getContents a bad path
        std::string testPath = "/ba/dfile";
        char buffer[1024];
        memset(buffer, 0, 1024);

        int retValue = testWad->getContents(testPath, buffer, 5);
        ASSERT_EQ(retValue, -1);

        delete testWad;
}

TEST(LibReadTests, getContentsTest6){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getContents Test 6, passing getContents a directory
        std::string testPath = "/Gl/";
        char buffer[1024];
        memset(buffer, 0, 1024);

        int retValue = testWad->getContents(testPath, buffer, 5);
        ASSERT_EQ(retValue, -1);

        delete testWad;
}

TEST(LibReadTests, getContentsTest7){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getContents Test 7, passing getContents an empty path
        std::string testPath = "";
        char buffer[1024];
        memset(buffer, 0, 1024);

        int retValue = testWad->getContents(testPath, buffer, 5);
        ASSERT_EQ(retValue, -1);

        delete testWad;
}

TEST(LibReadTests, getContentsTest8){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getContents Test 8, reading large image file
        std::string testPath = "/Gl/ad/os/cake.jpg";
        int size = testWad->getSize(testPath);
        ASSERT_EQ(size, 29869);

        char* expectedContents = new char[30000];
        char* buffer = new char[30000];

        int file_fd = open("./testfiles/cake.jpg", O_RDONLY, 0777);
        read(file_fd, expectedContents, size);

        int ret = testWad->getContents(testPath, buffer, 29869);
        ASSERT_EQ(ret, 29869);
        ASSERT_EQ(memcmp(expectedContents, buffer, 29869), 0);

        delete[] expectedContents;
        delete[] buffer;
        delete testWad;
}



TEST(LibReadTests, getDirectoryTest1){
        // Disable capture so your cout prints
        ::testing::GTEST_FLAG(catch_exceptions) = false;
        ::testing::GTEST_FLAG(output) = "stdout";
        ::testing::GTEST_FLAG(print_time) = true;

        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getDirectoryTest 1, E1M0
        std::string testPath = "/E1M0/";

        std::vector<std::string> expectedVector = {
                "01.txt",
                "02.txt",
                "03.txt",
                "04.txt",
                "05.txt",
                "06.txt",
                "07.txt",
                "08.txt",
                "09.txt",
                "10.txt"
        };

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, expectedVector.size());
        ASSERT_EQ(testVector, expectedVector);

        delete testWad;
}

TEST(LibReadTests, getDirectoryTest2){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getDirectoryTest 2, '/Gl/ad'
        std::string testPath = "/Gl/ad";

        std::vector<std::string> expectedVector = {
                "os"
        };

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, expectedVector.size());
        ASSERT_EQ(testVector, expectedVector);

        delete testWad;
}

TEST(LibReadTests, getDirectoryTest3){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getDirectoryTest 3, rootDir
        std::string testPath = "/";

        std::vector<std::string> expectedVector = {
                "E1M0",
                "Gl",
                "mp.txt"
        };

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, expectedVector.size());
        ASSERT_EQ(testVector, expectedVector);

        delete testWad;
}

TEST(LibReadTests, getDirectoryTest4){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getDirectoryTest 4, invalid path
        std::string testPath = "/no/t/re/al";

        std::vector<std::string> expectedVector;
        std::vector<std::string> testVector;

        int ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, -1);
        ASSERT_EQ(testVector.size(), 0);
        ASSERT_EQ(testVector, expectedVector);

        delete testWad;
}

TEST(LibReadTests, getDirectoryTest5){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getDirectoryTest 5, path to content
        std::string testPath = "/mp.txt";

        std::vector<std::string> expectedVector;
        std::vector<std::string> testVector;

        int ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, -1);
        ASSERT_EQ(testVector.size(), 0);
        ASSERT_EQ(testVector, expectedVector);

        delete testWad;
}

TEST(LibReadTests, getDirectoryTest6){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //getDirectoryTest 6, empty path
        std::string testPath = "";

        std::vector<std::string> expectedVector;
        std::vector<std::string> testVector;

        int ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, -1);
        ASSERT_EQ(testVector.size(), 0);
        ASSERT_EQ(testVector, expectedVector);

        delete testWad;
}

TEST(LibWriteTests, createDirectoryTest1){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createDirectory Test 1, create dir at root
        std::string testPath = "/ex/";

        testWad->createDirectory(testPath);
        
        ASSERT_TRUE(testWad->isDirectory(testPath));
        ASSERT_FALSE(testWad->isContent(testPath));

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(testVector.size(), 0);

        testVector.clear();
        ret = testWad->getDirectory("/", &testVector);
        ASSERT_EQ(ret, 4);
        ASSERT_EQ(testVector.size(), 4);

        std::vector<std::string> expectedVector = {
                "E1M0",
                "Gl",
                "mp.txt",
                "ex",
        };

        ASSERT_EQ(expectedVector, testVector);

        //Deleting and reinitiating object
        delete testWad;
        testWad = Wad::loadWad(wad_path);

        ASSERT_TRUE(testWad->isDirectory(testPath));
        ASSERT_FALSE(testWad->isContent(testPath));

        testVector.clear();
        ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(testVector.size(), 0);

        testVector.clear();
        ret = testWad->getDirectory("/", &testVector);
        ASSERT_EQ(ret, 4);
        ASSERT_EQ(testVector.size(), 4);

        ASSERT_EQ(expectedVector, testVector);

        delete testWad;
}

TEST(LibWriteTests, createDirectoryTest2){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createDirectory Test 2, create dir at existing directory
        std::string testPath = "/Gl/ex";

        testWad->createDirectory(testPath);
        
        ASSERT_TRUE(testWad->isDirectory(testPath));
        ASSERT_FALSE(testWad->isContent(testPath));

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(testVector.size(), 0);

        testVector.clear();
        ret = testWad->getDirectory("/Gl", &testVector);
        ASSERT_EQ(ret, 2);
        ASSERT_EQ(testVector.size(), 2);

        std::vector<std::string> expectedVector = {
                "ad",
                "ex",
        };

        ASSERT_EQ(expectedVector, testVector);

        //Deleting and reinitiating object
        delete testWad;
        testWad = Wad::loadWad(wad_path);

        ASSERT_TRUE(testWad->isDirectory(testPath));
        ASSERT_FALSE(testWad->isContent(testPath));

        testVector.clear();
        ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(testVector.size(), 0);

        testVector.clear();
        ret = testWad->getDirectory("/Gl", &testVector);
        ASSERT_EQ(ret, 2);
        ASSERT_EQ(testVector.size(), 2);

        ASSERT_EQ(expectedVector, testVector);

        delete testWad;
}

TEST(LibWriteTests, createDirectoryTest3){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createDirectory Test 3, create nested directories back to back
        std::string testPath = "/ex/";
        std::string testPath2 = "/ex/am/";

        //Creating dir '/ex/'
        testWad->createDirectory(testPath);
        
        ASSERT_TRUE(testWad->isDirectory(testPath));
        ASSERT_FALSE(testWad->isContent(testPath));

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(testVector.size(), 0);

        testVector.clear();
        ret = testWad->getDirectory("/", &testVector);
        ASSERT_EQ(ret, 4);
        ASSERT_EQ(testVector.size(), 4);

        std::vector<std::string> expectedVector = {
                "E1M0",
                "Gl",
                "mp.txt",
                "ex",
        };

        ASSERT_EQ(expectedVector, testVector);

        //Creating dir '/ex/am/'
        testWad->createDirectory(testPath2);

        ASSERT_TRUE(testWad->isDirectory(testPath2));
        ASSERT_FALSE(testWad->isContent(testPath2));

        testVector.clear();
        ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, 1);
        ASSERT_EQ(testVector.size(), 1);

        expectedVector.clear();
        expectedVector = {
                "am"
        };

        ASSERT_EQ(testVector, expectedVector);

        testVector.clear();
        ret = testWad->getDirectory(testPath2, &testVector);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(testVector.size(), 0);

        //Deleting and reinitiating object
        delete testWad;
        testWad = Wad::loadWad(wad_path);

        ASSERT_TRUE(testWad->isDirectory(testPath));
        ASSERT_FALSE(testWad->isContent(testPath));

        ASSERT_TRUE(testWad->isDirectory(testPath2));
        ASSERT_FALSE(testWad->isContent(testPath2));

        testVector.clear();
        ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, 1);
        ASSERT_EQ(testVector.size(), 1);

        expectedVector.clear();
        expectedVector = {
                "am"
        };

        ASSERT_EQ(testVector, expectedVector);

        testVector.clear();
        ret = testWad->getDirectory(testPath2, &testVector);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(testVector.size(), 0);

        delete testWad;
}

TEST(LibWriteTests, createDirectoryTest4){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createDirectory Test 4, create dir with too long name
        std::string testPath = "/Gl/exam/";

        testWad->createDirectory(testPath);
        
        ASSERT_FALSE(testWad->isDirectory(testPath));
        ASSERT_FALSE(testWad->isContent(testPath));

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, -1);
        ASSERT_EQ(testVector.size(), 0);

        testVector.clear();
        ret = testWad->getDirectory("/Gl", &testVector);
        ASSERT_EQ(ret, 1);
        ASSERT_EQ(testVector.size(), 1);
        ASSERT_EQ(testVector[0], "ad");

        //Deleting and reinitiating object
        delete testWad;
        testWad = Wad::loadWad(wad_path);

        ASSERT_FALSE(testWad->isDirectory(testPath));
        ASSERT_FALSE(testWad->isContent(testPath));

        testVector.clear();
        ret = testWad->getDirectory(testPath, &testVector);
        ASSERT_EQ(ret, -1);
        ASSERT_EQ(testVector.size(), 0);

        testVector.clear();
        ret = testWad->getDirectory("/Gl", &testVector);
        ASSERT_EQ(ret, 1);
        ASSERT_EQ(testVector.size(), 1);
        ASSERT_EQ(testVector[0], "ad");

        delete testWad;
}

TEST(LibWriteTests, createDirectoryTest5){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createDirectory Test 5, create dir in nonexistent dir
        std::string testPath = "/Ex/am/";
        std::string parentDir = "/Ex/";

        testWad->createDirectory(testPath);
        
        ASSERT_FALSE(testWad->isDirectory(testPath));
        ASSERT_FALSE(testWad->isContent(testPath));
        ASSERT_FALSE(testWad->isDirectory(parentDir));
        ASSERT_FALSE(testWad->isContent(parentDir));

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory("/", &testVector);
        ASSERT_EQ(ret, 3);
        ASSERT_EQ(testVector.size(), 3);

        std::vector<std::string> expectedVector = {
                "E1M0",
                "Gl",
                "mp.txt",
        };

        ASSERT_EQ(expectedVector, testVector);

        delete testWad;
}

TEST(LibWriteTests, createDirectoryTest6){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createDirectory Test 6, creating map dir
        std::string testPath = "/E1M1/";

        testWad->createDirectory(testPath);
        
        ASSERT_FALSE(testWad->isDirectory(testPath));
        ASSERT_FALSE(testWad->isContent(testPath));

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory("/", &testVector);
        ASSERT_EQ(ret, 3);
        ASSERT_EQ(testVector.size(), 3);

        std::vector<std::string> expectedVector = {
                "E1M0",
                "Gl",
                "mp.txt",
        };

        ASSERT_EQ(expectedVector, testVector);

        delete testWad;
}

TEST(LibWriteTests, createDirectoryTest7){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createDirectory Test 7, creating valid dir in map dir
        std::string testPath = "/E1M0/E1M1/";

        testWad->createDirectory(testPath);
        
        ASSERT_FALSE(testWad->isDirectory(testPath));
        ASSERT_FALSE(testWad->isContent(testPath));

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory("/E1M0/", &testVector);
        ASSERT_EQ(ret, 10);
        ASSERT_EQ(testVector.size(), 10);

        std::vector<std::string> expectedVector = {
                "01.txt",
                "02.txt",
                "03.txt",
                "04.txt",
                "05.txt",
                "06.txt",
                "07.txt",
                "08.txt",
                "09.txt",
                "10.txt"
        };

        ASSERT_EQ(expectedVector, testVector);

        delete testWad;
}

TEST(LibWriteTests, createFileTest1){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createFile Test 1, creating file in root
        std::string testPath = "/file.txt";

        testWad->createFile(testPath);

        ASSERT_TRUE(testWad->isContent(testPath));
        ASSERT_FALSE(testWad->isDirectory(testPath));

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory("/", &testVector);
        ASSERT_EQ(ret, 4);
        ASSERT_EQ(testVector.size(), 4);

        std::vector<std::string> expectedVector = {
                "E1M0",
                "Gl",
                "mp.txt",
                "file.txt"
        };

        ASSERT_EQ(expectedVector, testVector);

        //Reinstantiating object and rerunning tests
        delete testWad;
        testWad = Wad::loadWad(wad_path);

        ASSERT_TRUE(testWad->isContent(testPath));
        ASSERT_FALSE(testWad->isDirectory(testPath));

        testVector.clear();
        ret = testWad->getDirectory("/", &testVector);
        ASSERT_EQ(ret, 4);
        ASSERT_EQ(testVector.size(), 4);

        ASSERT_EQ(expectedVector, testVector);

        delete testWad;
}

TEST(LibWriteTests, createFileTest2){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createFile Test 2, creating file in directory
        std::string testPath = "/Gl/ad/example";

        testWad->createFile(testPath);

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory("/Gl/ad/", &testVector);

        ASSERT_TRUE(testWad->isContent(testPath));
        ASSERT_FALSE(testWad->isDirectory(testPath));

        ASSERT_EQ(ret, 2);
        ASSERT_EQ(testVector.size(), 2);

        std::vector<std::string> expectedVector = {
                "os",
                "example"
        };

        ASSERT_EQ(expectedVector, testVector);

        //Reinstantiating object and rerunning tests
        delete testWad;
        testWad = Wad::loadWad(wad_path);

        ASSERT_TRUE(testWad->isContent(testPath));
        ASSERT_FALSE(testWad->isDirectory(testPath));

        testVector.clear();
        ret = testWad->getDirectory("/Gl/ad/", &testVector);
        ASSERT_EQ(ret, 2);
        ASSERT_EQ(testVector.size(), 2);

        ASSERT_EQ(expectedVector, testVector);

        delete testWad;
}

TEST(LibWriteTests, createFileTest3){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createFile Test 3, passing invalid name, too long
        std::string testPath = "/Gl/ad/example.txt";

        testWad->createFile(testPath);

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory("/Gl/ad/", &testVector);

        ASSERT_FALSE(testWad->isContent(testPath));
        ASSERT_FALSE(testWad->isDirectory(testPath));

        ASSERT_EQ(ret, 1);
        ASSERT_EQ(testVector.size(), 1);

        std::vector<std::string> expectedVector = {
                "os"
        };

        ASSERT_EQ(expectedVector, testVector);

        delete testWad;
}

TEST(LibWriteTests, createFileTest4){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createFile Test 4, passing invalid path, nonexistent directory
        std::string testPath = "/Gl/ex/example.txt";

        testWad->createFile(testPath);

        ASSERT_FALSE(testWad->isContent(testPath));
        ASSERT_FALSE(testWad->isDirectory(testPath));

        ASSERT_FALSE(testWad->isContent("/Gl/ex"));
        ASSERT_FALSE(testWad->isDirectory("/Gl/ex"));

        delete testWad;
}

TEST(LibWriteTests, createFileTest5){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createFile Test 5, valid filename in E#M# directory
        std::string testPath = "/E1M0/file.txt";

        testWad->createFile(testPath);

        ASSERT_FALSE(testWad->isContent(testPath));
        ASSERT_FALSE(testWad->isDirectory(testPath));

        std::vector<std::string> expectedVector = {
                "01.txt",
                "02.txt",
                "03.txt",
                "04.txt",
                "05.txt",
                "06.txt",
                "07.txt",
                "08.txt",
                "09.txt",
                "10.txt"
        };

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory("/E1M0/", &testVector);
        ASSERT_EQ(ret, expectedVector.size());
        ASSERT_EQ(testVector, expectedVector);

        delete testWad;
}

TEST(LibWriteTests, createFileTest6){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //createFile Test 6, passing invalid name in E#M# format
        std::string testPath = "/E1M1";

        testWad->createFile(testPath);

        ASSERT_FALSE(testWad->isContent(testPath));
        ASSERT_FALSE(testWad->isDirectory(testPath));

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory("/", &testVector);
        ASSERT_EQ(ret, 3);
        ASSERT_EQ(testVector.size(), 3);

        std::vector<std::string> expectedVector = {
                "E1M0",
                "Gl",
                "mp.txt",
        };

        ASSERT_EQ(testVector, expectedVector);

        delete testWad;
}

TEST(LibWriteTests, writeToFileTest1){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //writeToFile Test 1, creating and writing to a text file
        std::string testPath = "/file.txt";

        //testing file creation

        testWad->createFile(testPath);

        ASSERT_TRUE(testWad->isContent(testPath));
        ASSERT_FALSE(testWad->isDirectory(testPath));

        std::vector<std::string> testVector;
        int ret = testWad->getDirectory("/", &testVector);
        ASSERT_EQ(ret, 4);
        ASSERT_EQ(testVector.size(), 4);

        std::vector<std::string> expectedVector = {
                "E1M0",
                "Gl",
                "mp.txt",
                "file.txt"
        };

        ASSERT_EQ(expectedVector, testVector);

        //testing file writing
        const char expectedFileContents[] = "Hello! This is a test to make sure I can write to files.\n";
        int expectedSizeOfFile = 57;

        ret = testWad->writeToFile(testPath, expectedFileContents, expectedSizeOfFile);
        ASSERT_EQ(ret, expectedSizeOfFile);
        ASSERT_EQ(testWad->getSize(testPath), expectedSizeOfFile);

        char buffer[100];
        memset(buffer, 0, 100);
        ret = testWad->getContents(testPath, buffer, expectedSizeOfFile);
        ASSERT_EQ(ret, expectedSizeOfFile);
        ASSERT_EQ(memcmp(buffer, expectedFileContents, expectedSizeOfFile), 0)
        << "Expected string: " << buffer
        << "Returned string: " << expectedFileContents;        

        //Reinstantiating object and rerunning tests
        delete testWad;
        testWad = Wad::loadWad(wad_path);

        ASSERT_EQ(testWad->getSize(testPath), expectedSizeOfFile);

        memset(buffer, 0, 100);
        ret = testWad->getContents(testPath, buffer, expectedSizeOfFile);
        ASSERT_EQ(ret, expectedSizeOfFile);
        ASSERT_EQ(memcmp(buffer, expectedFileContents, expectedSizeOfFile), 0)
        << "Expected string: " << buffer
        << "Returned string: " << expectedFileContents;

        //Attemping to write to the file again

        ret = testWad->writeToFile(testPath, expectedFileContents, expectedSizeOfFile);
        //path is technically valid, but the file already exists.
        //function should return 0, to signify that the path is valid
        //but the the write should still fail.
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(testWad->getSize(testPath), expectedSizeOfFile);

        memset(buffer, 0, 100);
        ret = testWad->getContents(testPath, buffer, expectedSizeOfFile);
        ASSERT_EQ(ret, expectedSizeOfFile);
        ASSERT_EQ(memcmp(buffer, expectedFileContents, expectedSizeOfFile), 0)
        << "Expected string: " << buffer
        << "Returned string: " << expectedFileContents;

        delete testWad;      
}

TEST(LibWriteTests, writeToFileTest2){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //writeToFile Test 2, writing to an existing file
        std::string testPath = "/E1M0/01.txt";
        const char testWriteContent[] = "Example text"; 
        int sizeOfWrite = sizeof(testWriteContent);
        int ret = testWad->writeToFile(testPath, testWriteContent, sizeOfWrite);
        ASSERT_EQ(ret, 0);
        
        delete testWad;      
}

TEST(LibWriteTests, writeToFileTest3){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //writeToFile Test 3, writing to an existing directory
        std::string testPath = "/E1M0/";
        const char testWriteContent[] = "Example text"; 
        int sizeOfWrite = sizeof(testWriteContent);
        int ret = testWad->writeToFile(testPath, testWriteContent, sizeOfWrite);
        ASSERT_EQ(ret, -1);
        
        delete testWad;      
}

TEST(LibFunctionalityTests, bigTest){
        std::string wad_path = setupWorkspace();
        Wad* testWad = Wad::loadWad(wad_path);

        //Creating dir '/Ex'
        testWad->createDirectory("/Ex");
        std::vector<std::string> expectedVector = {"E1M0","Gl","mp.txt","Ex"};
        ASSERT_TRUE(testWad->isDirectory("/Ex"));
        std::vector<std::string> testVector;
        int ret = testWad->getDirectory("/", &testVector);
        ASSERT_EQ(ret, 4);
        ASSERT_EQ(testVector.size(), 4);
        ASSERT_EQ(testVector, expectedVector);

        //Creating dir '/Ex/ad/'
        testWad->createDirectory("/Ex/ad/");
        ASSERT_TRUE(testWad->isDirectory("/Ex/ad/"));
        testVector.clear();
        expectedVector.clear();
        expectedVector = {"ad"};
        ret = testWad->getDirectory("/Ex", &testVector);
        ASSERT_EQ(ret, 1);
        ASSERT_EQ(testVector.size(), 1);
        ASSERT_EQ(testVector, expectedVector);

        //Creating and writing to '/Ex/ad/test.txt'
        testWad->createFile("/Ex/ad/test.txt");
        ASSERT_TRUE(testWad->isContent("/Ex/ad/test.txt"));
        testVector.clear();
        expectedVector.clear();
        expectedVector = {"test.txt"};
        ret = testWad->getDirectory("/Ex/ad/", &testVector);
        ASSERT_EQ(ret, 1);
        ASSERT_EQ(testVector.size(), 1);
        ASSERT_EQ(testVector, expectedVector);

        const char inputText[] =
                "This text is pretty long, but will be "
                "concatenated into just a single string. "
                "The disadvantage is that you have to quote "
                "each part, and newlines must be literal as "
                "usual.";

        //Removing one from size to exclude null term
        int inputSize = 170;

        ret = testWad->writeToFile("/Ex/ad/test.txt", inputText, inputSize);
        ASSERT_EQ(ret, inputSize);

        char buffer[200];
        memset(buffer, 0 , 200);
        ret = testWad->getContents("/Ex/ad/test.txt", buffer, inputSize);
        ASSERT_EQ(ret, inputSize);
        ASSERT_EQ(testWad->getSize("/Ex/ad/test.txt"), inputSize);
        ASSERT_EQ(memcmp(buffer, inputText, inputSize), 0);

        //Creating dir '/Ex/ad/os/'
        testWad->createDirectory("/Ex/ad/os");
        ASSERT_TRUE(testWad->isDirectory("/Ex/ad/os"));
        testVector.clear();
        expectedVector.clear();
        expectedVector = {"test.txt", "os"};
        ret = testWad->getDirectory("/Ex/ad", &testVector);
        ASSERT_EQ(ret, 2);
        ASSERT_EQ(testVector.size(), 2);
        ASSERT_EQ(testVector, expectedVector);

        //Creating and writing to '/Ex/ad/os/test.txt'
        testWad->createFile("/Ex/ad/os/test.txt");
        ASSERT_TRUE(testWad->isContent("/Ex/ad/os/test.txt"));
        testVector.clear();
        expectedVector.clear();
        expectedVector = {"test.txt"};
        ret = testWad->getDirectory("/Ex/ad/os", &testVector);
        ASSERT_EQ(ret, 1);
        ASSERT_EQ(testVector.size(), 1);
        ASSERT_EQ(testVector, expectedVector);

        ret = testWad->writeToFile("/Ex/ad/os/test.txt", inputText, inputSize);
        ASSERT_EQ(ret, inputSize);

        memset(buffer, 0 , 200);
        ret = testWad->getContents("/Ex/ad/os/test.txt", buffer, inputSize);
        ASSERT_EQ(ret, inputSize);
        ASSERT_EQ(testWad->getSize("/Ex/ad/os/test.txt"), inputSize);
        ASSERT_EQ(memcmp(buffer, inputText, inputSize), 0);


        //Deleting and reinstantiating object
        delete testWad;      
        testWad = Wad::loadWad(wad_path);

        testVector.clear();
        expectedVector.clear();
        expectedVector = {"ad"};
        ret = testWad->getDirectory("/Ex/", &testVector);
        ASSERT_TRUE(testWad->isDirectory("/Ex/"));
        ASSERT_TRUE(testWad->isDirectory("/Ex/ad"));
        ASSERT_FALSE(testWad->isContent("/Ex/test.txt"));
        ASSERT_EQ(ret, 1);
        ASSERT_EQ(testVector.size(), 1);
        ASSERT_EQ(testVector, expectedVector);

        testVector.clear();
        expectedVector.clear();
        expectedVector = {"test.txt", "os"};
        ret = testWad->getDirectory("/Ex/ad", &testVector);
        ASSERT_EQ(ret, 2);
        ASSERT_EQ(testVector.size(), 2);
        ASSERT_EQ(testVector, expectedVector);

        testVector.clear();
        expectedVector.clear();
        expectedVector = {"test.txt"};
        ret = testWad->getDirectory("/Ex/ad/os", &testVector);
        ASSERT_EQ(ret, 1);
        ASSERT_EQ(testVector.size(), 1);
        ASSERT_EQ(testVector, expectedVector);

        ASSERT_TRUE(testWad->isContent("/Ex/ad/test.txt"));
        ASSERT_EQ(testWad->getSize("/Ex/ad/test.txt"), inputSize);
        memset(buffer, 0 , 200);
        ret = testWad->getContents("/Ex/ad/test.txt", buffer, inputSize);
        ASSERT_EQ(ret, inputSize);
        ASSERT_EQ(memcmp(buffer, inputText, inputSize), 0);

        ASSERT_TRUE(testWad->isContent("/Ex/ad/os/test.txt"));
        ASSERT_EQ(testWad->getSize("/Ex/ad/os/test.txt"), inputSize);
        memset(buffer, 0 , 200);
        ret = testWad->getContents("/Ex/ad/os/test.txt", buffer, inputSize);
        ASSERT_EQ(ret, inputSize);
        ASSERT_EQ(memcmp(buffer, inputText, inputSize), 0);

        delete testWad;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Disable capture so your cout prints
    ::testing::GTEST_FLAG(catch_exceptions) = false;
    ::testing::GTEST_FLAG(output) = "stdout";
    ::testing::GTEST_FLAG(print_time) = true;

    return RUN_ALL_TESTS();
}