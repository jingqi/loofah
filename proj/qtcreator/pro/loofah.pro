
TEMPLATE = subdirs

NUT_PROJ = ../../../lib/nut.git/proj/qtcreator/pro/nut/nut.pro
SUBDIRS += \
    $${NUT_PROJ} \
    loofah \
    test_loofah \
    test_pingpong

loofah.depends = $${NUT_PROJ}
test_loofah.depends = $${NUT_PROJ} loofah
test_pingpong.depends = $${NUT_PROJ} loofah
