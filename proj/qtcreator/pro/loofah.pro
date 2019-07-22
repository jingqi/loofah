
TEMPLATE = subdirs

SUBDIRS += \
    nut \
    loofah \
    test-loofah \
    test-pingpong

loofah.depends = nut
test-loofah.depends = nut loofah
test-pingpong.depends = nut loofah
