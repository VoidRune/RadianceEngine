#!/bin/bash

pushd ..
Scripts/Premake/Linux/premake5.exe --cc=clang --file=Build.lua gmake2
popd