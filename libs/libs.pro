TEMPLATE = subdirs

SUBDIRS = \
    clipper \
    delaunay-triangulation \
    fontobene \
    hoedown \
    googletest \
    librepcb \
    parseagle \
    quazip \
    router \
    sexpresso

librepcb.depends = \
    clipper \
    delaunay-triangulation \
    fontobene \
    parseagle \
    hoedown \
    quazip \
    router \
    sexpresso \

