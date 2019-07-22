
import platform

from os.path import dirname, join

from nova.util import file_utils


CWD = dirname(__file__)

ns = globals()['namespace']
ns.set_name('loofah-wks')

app = ns.get_app()
app.import_namespace(join(CWD, 'novaconfig-loofah.py'))
app.import_namespace(join(CWD, 'novaconfig-test-loofah.py'))
app.import_namespace(join(CWD, 'novaconfig-test-pingpong.py'))

out_dir = platform.system().lower() + '-' + ('debug' if ns['DEBUG'] == '1' else 'release')
ns.set_default_target(join(CWD, out_dir, 'test-loofah' + ns['PROGRAM_SUFFIX']))

# run
ns.add_dep('@run', 'test-loofah|@run')
ns.add_dep('@valgrind', 'test-loofah|@valgrind')

# clean
def clean(target):
    file_utils.remove_any(join(CWD, out_dir))
ns.set_recipe('@clean', clean)
ns.add_dep('@clean', 'nut|@clean')
