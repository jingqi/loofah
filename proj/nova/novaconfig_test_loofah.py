
import platform

from os.path import realpath, dirname, join, splitext
from subprocess import Popen

from nova.util import file_utils
from nova.builtin import compile_c, file_op


CWD = dirname(realpath(__file__))

ns = globals()['namespace']
ns.set_name('test_loofah')

ns.get_app().import_namespace(join(CWD, 'novaconfig_loofah.py'))

## Vars
src_root = join(CWD, '../../src/test_loofah')
out_dir = platform.system().lower() + '-' + ('debug' if ns['DEBUG'] == '1' else 'release')
out_root = join(CWD, out_dir)
obj_root = join(out_root, 'obj/test_loofah')
header_root = join(out_root, 'include/test_loofah')
nut_proj_root = join(ns.getenv('NUT_PATH', join(CWD, '../../lib/nut.git')), 'proj/nova')

## Flags
ns.append_env_flags('CPPFLAGS',
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
    ns.append_env_flags('LDFLAGS', '-lpthread', '-latomic')

ns.append_env_flags('LDFLAGS', '-L' + out_root, '-lnut', '-lloofah')

# NOTE 这些 win 网络库必须放在最后，否则会出错
#      See http://stackoverflow.com/questions/2033608/mingw-linker-error-winsock
if platform.system() == 'Windows':
    ns.append_env_flags('LDFLAGS', '-lwininet', '-lws2_32', '-lwsock32')

## Dependencies

# Generate headers
for src in file_utils.iterfiles(src_root, '.h'):
    h = src
    ih = file_utils.chproot(src, src_root, header_root)
    ns.set_recipe(ih, file_op.copyfile)
    ns.add_chained_deps('@headers', ih, h)

program = join(out_root, 'test_loofah' + ns['PROGRAM_SUFFIX'])
ns.set_recipe('@read_deps', compile_c.read_deps)
for src in file_utils.iterfiles(src_root, '.c', '.cpp'):
    c = src
    o = splitext(file_utils.chproot(src, src_root, obj_root))[0] + '.o'
    d = o + '.d'
    ns.add_dep(d, '@headers')
    ns.add_chained_deps(o, '@read_deps', d, c)
    ns.add_chained_deps(program, o, c)

ns.add_dep(program, join(out_root, 'libloofah' + ns['SHARED_LIB_SUFFIX']))
ns.set_recipe(program, compile_c.link_program)
ns.set_default_target(program)

# run
def run(target):
    p = Popen([program], cwd=dirname(program))
    p.wait()
ns.set_recipe('@run', run)
ns.add_dep('@run', program)

def valgrind(target):
    p = Popen(['valgrind', '-v', '--leak-check=full', '--show-leak-kinds=all', program], cwd=dirname(program))
    p.wait()
ns.set_recipe('@valgrind', valgrind)
ns.add_dep('@valgrind', program)

# clean
def clean(target):
    file_utils.remove_any(program, obj_root, header_root)
ns.set_recipe('@clean', clean)
