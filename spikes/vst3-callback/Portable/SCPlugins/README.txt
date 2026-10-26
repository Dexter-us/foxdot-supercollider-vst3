Dexter U.S. FoxDot - SuperCollider UGen modules

The VST bundles official SuperCollider UGen modules under Contents/Resources/SCPlugins.
This folder is scanned too: place compatible Windows x64 SuperCollider .scx modules here.
The embedded engine loads modules at startup; restart the DAW after adding or changing a module.
The editor status shows the .scx filenames it found, not a guarantee that each module loaded successfully.

Only add native modules from sources you trust. A .scx file is executable native code and must match
this plugin's pinned SuperCollider API/build. Check its license and any required DLLs yourself;
the loader cannot validate either. Duplicate modules or missing dependencies may prevent UGens from
registering. The core bundle excludes DiskIO_UGens and does not include sample files.
