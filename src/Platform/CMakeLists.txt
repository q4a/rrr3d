
SET(Platform_src
#        exodus.cpp
#        d3dx9.cpp
        d3dx9math.cpp
#        ${pevents_SOURCE_DIR}/src/pevents.cpp
)

ADD_LIBRARY(Exodus STATIC ${Platform_src})
