# Using Zyphie

Zyphie creates .zyph archives containing regular files and folders. It is an
independent format: ZIP, RAR and 7z archives cannot be opened by this version.

## Install and launch

Extract the complete Windows x64 portable ZIP to a new folder. Keep its DLLs and
plugins beside the application. Open zyphie.exe for the desktop or use zyph.exe
in a terminal. No administrator access, account, service, registry changes or
network connection is needed. To uninstall, remove the application folder after
closing the app; your archives and original data are separate. The package is
unsigned; no code-signing identity is claimed.

## Create

Choose Add files or Add folder (or drop them into the window). Select Fast,
Balanced, Maximum or Store. Balanced with one worker is the default; more workers
can increase memory use and do not always improve speed. Choose New next to the
archive path, enter a new filename, then Create archive. Sources remain intact.
The final success message means the archive was written and fully verified.
Progress reaching 100% alone does not mean publication succeeded.

## Browse, verify and restore

Choose Browse next to Archive file, then Open archive. Expand folders in the
browser. Listing validates metadata but does not verify payloads. Verify archive
checks all file/block hashes. To restore, enter a NEW destination folder beneath
an existing parent (Choose parent proposes a name). Choose **Extract all** to
restore the entire archive, even if a file or folder is highlighted in the browser.
To restore a subset, select entries and choose **Extract selected**; its count is
the number of selected rows, and each selected folder includes all descendants.
Extract selected is disabled with no selection and never falls back to extracting
everything. Unselected file payloads are not checked during selective extraction;
run Verify archive when you need complete integrity verification.

If a previous extraction contains only one folder, it may have been a selective
restore in the original V1 desktop. Open the archive and use Extract all with a
new destination to restore everything; the earlier output does not need deleting.

An existing destination is refused. Choose another name; Zyphie never merges or
replaces existing data. Errors are shown in the window/dialog and no success is
reported for skipped files. Cancel requests a safe stop; an active compression
block finishes first. Closing during a job requests cancellation and asks you to
close again when it finishes. No settings or recent-file history are persisted.

Light/dark themes, keyboard focus navigation and labeled controls are available.
Elapsed time and bytes are measured; remaining-time estimates apply to the current
phase only and appear after enough progress. Scan totals are unknown.

## CLI examples

```powershell
./zyph.exe pack "C:/Data/Project" -o project.zyph --preset balanced --threads 2 --progress
./zyph.exe list project.zyph
./zyph.exe verify project.zyph
./zyph.exe extract project.zyph -o restored --select "Project/readme.txt"
```

## Supported data and limitations

Regular files/directories, UTF-8 portable names, empty entries, file bytes and
whole-second timestamps are supported. Links, reparse points, devices and other
special objects are rejected. ACLs, owner IDs, alternate streams and subsecond
timestamps are not represented. Choose inputs accordingly; source trees must not
change during packing. Do not place output archives within their own source tree.

There is no encryption, password protection, recovery record or authentication.
A hash detects corruption, not deliberate replacement by an attacker. Resources
are bounded by policy, not a hard process-memory limit. Power-loss durability and
automatic abandoned-staging recovery are not promised. Keep independent backups.
Windows 10/11 x64 is the target; the release validation report states tested OSes.
