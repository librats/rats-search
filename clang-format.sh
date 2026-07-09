find src tests \( -name '*.cpp' -o -name '*.h' \) -not -path 'src/librats/*' -print0 \
  | xargs -0 clang-format -i