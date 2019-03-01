
import platform

from os.path import realpath, dirname, join, splitext

from nova.util import file_utils
from nova.builtin import compile_c, file_op


CWD = dirname(realpath(__file__))

ns = globals()['namespace']
ns.set_name('loofah')

nut_proj_root = join(ns.getenv('NUT_PATH', join(CWD, '../../lib/nut.git')), 'proj/nova')
ns.get_app().import_namespace(join(nut_proj_root, 'novaconfig_nut.py'))


## Vars
src_root = join(CWD, '../../src/loofah')
out_dir = platform.system().lower() + '-' + ('debug' if ns['DEBUG'] == '1' else 'release')
out_root = join(CWD, out_dir)
obj_root = join(out_root, 'obj/loofah')
header_root = join(out_root, 'include/loofah')


## Flags
ns.append_env_flags('CPPFLAGS', '-DBUILDING_LOOFAH',
                    '-I' + realpath(join(out_root, 'include')),
                    '-I' + realpath(join(nut_proj_root, out_dir, 'include')))
ns.append_env_flags('CFLAGS', '-std=c11')
ns.append_env_flags('CXXFLAGS', '-std=c++11')

if platform.system() == 'Darwin':
    ns.append_env_flags('CXXFLAGS', '-stdlib=libc++')
    ns.append_env_flags('LDFLAGS', '-lc++')
else:
    ns.append_env_flags('LDFLAGS', '-lstdc++')

if platform.system() == 'Linux':
    ns.append_env_flags('LDFLAGS', '-lpthread', '-ldl', '-latomic')

if platform.system() != 'Windows':
    ns.append_env_flags('CXXFLAGS', '-fPIC')

ns.append_env_flags('LDFLAGS', '-L' + out_root, '-lnut')

# NOTE 这些 win 网络库必须放在最后，否则会出错
#      See http://stackoverflow.com/questions/2033608/mingw-linker-error-winsock
if platform.system() == 'Windows':
    ns.append_env_flags('LDFLAGS', '-lwininet', '-lws2_32', '-lwsock32')

## Dependencies
so = join(out_root, 'libloofah' + ns['SHARED_LIB_SUFFIX'])

# Generate headers
for src in file_utils.iterfiles(src_root, '.h'):
    h = src
    ih = file_utils.chproot(src, src_root, header_root)
    ns.set_recipe(ih, file_op.copyfile)
    ns.add_chained_deps('@headers', ih, h)

ns.set_recipe('@read_deps', compile_c.read_deps)
for src in file_utils.iterfiles(src_root, '.c', '.cpp'):
    c = src
    o = splitext(file_utils.chproot(src, src_root, obj_root))[0] + '.o'
    d = o + '.d'
    ns.add_dep(d, '@headers')
    ns.add_chained_deps(o, '@read_deps', d, c)
    ns.add_chained_deps(so, o, c)

nut_dup = join(out_root, 'libnut' + ns['SHARED_LIB_SUFFIX'])
ns.set_recipe(nut_dup, file_op.copyfile)
ns.add_chained_deps(so, nut_dup, join(nut_proj_root, out_dir, 'libnut' + ns['SHARED_LIB_SUFFIX']))
ns.set_default_target(so)

# clean
def clean(target):
    file_utils.remove_any(so, obj_root, header_root)
ns.set_recipe('@clean', clean)
