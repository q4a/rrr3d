project(TinyXml)

# Vendored TinyXML 2.5.3, zlib licensed -- see LICENSE.tinyxml.
#
# extern.7z supplies this as a prebuilt MSVC static lib and vendors only the
# headers, so there is nothing to build against off Windows. The headers here
# are byte-identical to extern/tinyxml/include apart from line endings, and the
# sources come from the same 2.5.3 release, so both platforms compile the same
# parser. (The upstream release notes say 2.6.2; the headers say 2.5.3, and the
# headers win, since they are what the project has always compiled against.)
#
# Tracked rather than dropped into extern/, which is gitignored -- CI and fresh
# checkouts need these.

add_library(${PROJECT_NAME} STATIC
    source/tinyxml.cpp
    source/tinyxmlparser.cpp
    source/tinyxmlerror.cpp
    source/tinystr.cpp
    header/tinyxml.h
    header/tinystr.h
)

target_include_directories(${PROJECT_NAME} PUBLIC header)

# LexStd uses the std::string overloads, which are the tinyxml_STL build.
target_compile_definitions(${PROJECT_NAME} PUBLIC TIXML_USE_STL)
