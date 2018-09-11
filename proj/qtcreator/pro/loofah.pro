
TEMPLATE = subdirs

SUBDIRS += \
    nut \
    loofah \
    test_loofah \
    test_pingpong

loofah.depends = nut
test_loofah.depends = nut loofah
test_pingpong.depends = nut loofah
