"""Validate a trusted locally produced ZIP after relocation, without SDK paths."""
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import zipfile

archive=Path(sys.argv[1]).resolve()
repo=Path(__file__).resolve().parents[1]
base=repo/'build'/'package-smoke'
base.mkdir(exist_ok=True)
root=Path(tempfile.mkdtemp(prefix='installed-',dir=base))
with zipfile.ZipFile(archive) as z:
    for info in z.infolist():
        target=(root/info.filename).resolve()
        if not target.is_relative_to(root): raise RuntimeError('Unsafe package path')
    z.extractall(root)
pkg=next(p for p in root.iterdir() if p.is_dir())
manifest=json.loads((pkg/'manifest.json').read_text(encoding='utf-8-sig'))
for entry in manifest['files']:
    path=pkg/entry['path']
    assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256'], path
assert not (pkg/'Qt6Test.dll').exists()
assert (pkg/'sources'/'qtbase-6.8.3.zip').is_file()
env=dict(os.environ)
for key in list(env):
    if key.startswith(('QT_','QML','VCPKG')): env.pop(key)
env['PATH']=str(pkg)+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32')
def run(*args):
    return subprocess.run([str(pkg/'zyph.exe'),*map(str,args)],env=env,cwd=root,check=True,capture_output=True,text=True,timeout=30)
assert '1.0.0' in run('--version').stdout
source=root/'original';source.mkdir();(source/'empty').mkdir()
(source/'data.txt').write_bytes(b'original bytes\x00\xff'*1000)
run('pack',source,'-o',root/'test.zyph','--threads','2')
run('verify',root/'test.zyph')
run('extract',root/'test.zyph','-o',root/'restored')
assert (root/'restored'/'original'/'data.txt').read_bytes()==(source/'data.txt').read_bytes()
assert (root/'restored'/'original'/'empty').is_dir()
for fixture in (repo/'tests'/'fixtures').glob('*.zyph'):run('verify',fixture)
# Launch the production GUI, check its real window responds, and close normally.
user=ctypes.WinDLL('user32',use_last_error=True)
user.IsHungAppWindow.argtypes=[wintypes.HWND];user.IsHungAppWindow.restype=wintypes.BOOL
user.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
user.PostMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
process=subprocess.Popen([str(pkg/'zyphie.exe')],env=env,cwd=root)
windows=[]
@callback
def visit(hwnd,_):
    pid=wintypes.DWORD();user.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
    if pid.value==process.pid and user.IsWindowVisible(hwnd):windows.append(hwnd)
    return True
try:
    deadline=time.monotonic()+15
    while time.monotonic()<deadline:
        assert process.poll() is None, 'GUI exited unexpectedly'
        user.EnumWindows(visit,0)
        if windows:break
        time.sleep(.1)
    assert windows,'No GUI window'
    assert not user.IsHungAppWindow(windows[0]),'GUI not responsive'
    user.PostMessageW(windows[0],0x0010,0,0)
    assert process.wait(timeout=10)==0
finally:
    if process.poll() is None:process.kill();process.wait()
report={'package':archive.name,'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'manifest_files':len(manifest['files']),'clean_path':True,'golden_verified':True,'roundtrip_verified':True,'gui_window_responsive':True,'gui_normal_exit':True}
(root/'report.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2));print('Installed test location:',root)
