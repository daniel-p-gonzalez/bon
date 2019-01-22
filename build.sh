#!/bin/bash
if ! type "clang++" > /dev/null; then
    mkdir -p build && cd build && cmake ../src && make -j8 install && cd .. && mv build/bon .
else
    mkdir -p build && cd build && CXX=clang++ CC=clang cmake ../src && make -j8 install && cd .. && mv build/bon .
fi

BON_VERSION=$(grep -Po "set\(BON_VERSION \"\K([0-9]+\.[0-9]+\.[0-9]+)" src/CMakeLists.txt)
BON_PATH=~/.bon/${BON_VERSION}/bin

case ":$PATH:" in
  *:$HOME/.bon/${BON_VERSION}/bin:*) echo "~/.bon/${BON_VERSION}/bin already in PATH";;
  *) echo "Adding ~/.bon/${BON_VERSION}/bin to PATH"
    echo "export PATH=\$HOME/.bon/${BON_VERSION}/bin:\$PATH" >> ~/.bashrc
    echo "export PATH=\$HOME/.bon/${BON_VERSION}/bin:\$PATH" >> ~/.profile
    echo "export PATH=\$HOME/.bon/${BON_VERSION}/bin:\$PATH" >> ~/.bash_profile
    echo "Run \"source ~/.bashrc\" to refresh your environment"
  ;;
esac

if [ -z "$BON_STDLIB_PATH" ]
then
    echo "export BON_STDLIB_PATH=\$HOME/.bon/${BON_VERSION}/stdlib" >> ~/.bashrc
    echo "export BON_STDLIB_PATH=\$HOME/.bon/${BON_VERSION}/stdlib" >> ~/.profile
    echo "export BON_STDLIB_PATH=\$HOME/.bon/${BON_VERSION}/stdlib" >> ~/.bash_profile
fi
