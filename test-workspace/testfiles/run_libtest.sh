setup(){
    sudo apt-get install cmake -y > /dev/null
    sudo apt-get install libfuse-dev fuse -y > /dev/null
    sudo chmod 666 /dev/fuse > /dev/null
    if [ ! -d ./test-workspace ]; then
        mkdir ./test-workspace
    fi

    cd test-workspace

    if [ ! -d ./googletest ]; then
        echo "Installing googletest..."
        git -c advice.detachedHead=false clone https://github.com/google/googletest.git -b v1.13.0 > /dev/null
        cd googletest 
        mkdir build
        cd build
        cmake .. -DBUILD_GMOCK=OFF > /dev/null
        make > /dev/null
        sudo make install > /dev/null
        cd ../../
        pwd
    fi

    cd ../
    echo "Entering libWad directory and running makefile..."
    cd ./libWad
    if ! make ; then
        echo "Library did not compile. Exiting."
        exit
    fi
    cd ..
    cd ./test-workspace

    echo "Compiling tests..."
    if ! g++ -g -o libtest.out ../libtest.cpp -lgtest -lgtest_main -lpthread -L ../libWad -lWad ; then
        echo "Tests did not compile. Exiting."
        exit
    fi
}

run(){
    echo "Running tests..."
    ./libtest.out
}

main(){
    setup
    run
}

RUN_PARAM="$1"

main