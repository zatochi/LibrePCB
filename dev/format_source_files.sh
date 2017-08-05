#!/usr/bin/env bash

find ./apps          -iname *.h -o -iname *.cpp | xargs clang-format -i -style=file
find ./libs/librepcb -iname *.h -o -iname *.cpp | xargs clang-format -i -style=file
echo "DONE!"

