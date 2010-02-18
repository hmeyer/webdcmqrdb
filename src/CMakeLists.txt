SET(WT_PROJECT_SOURCE 
main.cpp
index.cpp sender.cpp dicomsender.cpp dicomconfig.cpp
)

SET(WT_PROJECT_TARGET dcmqrdb.wt)
SET(WT_HOME ~/devel/wt)
SET(DCMTK_HOME ~/devel/dcmtk-3.5.4)


LINK_DIRECTORIES( ${WT_HOME}/build/src ${WT_HOME}/build/src/http ${WT_HOME}/build/src/Wt/Ext
  ${DCMTK_HOME}/dcmqrdb/libsrc 
  ${DCMTK_HOME}/ofstd/libsrc 
  ${DCMTK_HOME}/dcmdata/libsrc 
  ${DCMTK_HOME}/dcmnet/libsrc)

ADD_EXECUTABLE(${WT_PROJECT_TARGET} ${WT_PROJECT_SOURCE})

INCLUDE_DIRECTORIES(${WT_HOME}/src)

INCLUDE_DIRECTORIES(${WT_HOME}/src)
INCLUDE_DIRECTORIES(${WT_HOME}/build)

INCLUDE_DIRECTORIES(${DCMTK_HOME}/config/include)
INCLUDE_DIRECTORIES(${DCMTK_HOME}/ofstd/include)
INCLUDE_DIRECTORIES(${DCMTK_HOME}/dcmdata/include)
INCLUDE_DIRECTORIES(${DCMTK_HOME}/dcmnet/include)
INCLUDE_DIRECTORIES(${DCMTK_HOME}/dcmqrdb/include)

ADD_DEFINITIONS(-DHAVE_CONFIG_H)


TARGET_LINK_LIBRARIES(${WT_PROJECT_TARGET} ${WT_CONNECTOR} wt wtext dcmqrdb dcmnet dcmdata clucene-core ofstd)
