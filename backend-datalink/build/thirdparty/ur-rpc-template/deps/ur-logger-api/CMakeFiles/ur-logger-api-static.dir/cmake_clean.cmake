file(REMOVE_RECURSE
  "libur-logger-api.a"
  "libur-logger-api.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/ur-logger-api-static.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
