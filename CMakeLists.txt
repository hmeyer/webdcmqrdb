CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(Webdcmqrdb)

SET (WT_CONNECTOR "wtfcgi" CACHE STRING "Connector used (wthttp or wtfcgi)")

ADD_SUBDIRECTORY(src)

