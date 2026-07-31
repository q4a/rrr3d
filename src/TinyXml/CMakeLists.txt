project(TinyXml)

# Vendored upstream, unmodified. See tools/vendor-tinyxml.py -- re-run it rather
# than editing anything in this directory.

add_library(${PROJECT_NAME} STATIC
    tinystr.cpp
    tinyxml.cpp
    tinyxmlerror.cpp
    tinyxmlparser.cpp
    tinystr.h
    tinyxml.h
)

target_include_directories(${PROJECT_NAME} PUBLIC
    .
)

# PUBLIC, and it has to be. TIXML_USE_STL is not a convenience switch: it
# changes TiXmlNode's signatures from TiXmlString to std::string, so a library
# built without it and a caller built with it disagree about the ABI and link
# cleanly anyway. That is why upstream ships tinyxml_STL.lib as a separate
# binary from tinyxml.lib, and it is the variant lslSerialFileXml.cpp -- which
# #defines TIXML_USE_STL itself before including the header -- has always used.
target_compile_definitions(${PROJECT_NAME} PUBLIC
    TIXML_USE_STL
)

# 2007 source. These are upstream's to fix and it is not going to; the point of
# vendoring unmodified is that tools/vendor-tinyxml.py stays a verbatim copy.
if (NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wno-implicit-fallthrough
        -Wno-unused-parameter
    )
endif()
