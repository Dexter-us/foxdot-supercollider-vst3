Dexter U.S. FoxDot - SuperCollider UGen modules

The VST bundles official SuperCollider UGen modules under Contents/Resources/SCPlugins.
This folder is scanned too: place compatible Windows x64 SuperCollider .scx modules here.
The embedded engine loads modules at startup; restart the DAW after adding or changing a module.
The editor status shows the .scx filenames it found, not a guarantee that each module loaded successfully.

Only add native modules from sources you trust. A .scx file is executable native code and must match
this plugin's pinned SuperCollider API/build. Check its license and any required DLLs yourself;
the loader cannot validate either. Duplicate modules or missing dependencies may prevent UGens from
registering. The core bundle includes ReverbUGens and UIUGens for global desktop mouse input.
DiskIO_UGens and shared libsndfile are a SEPARATE optional pack (VST 0.4.80+).
Keep its sndfile.dll beside DiskIO_UGens.scx; do not install duplicate DiskIO modules.
Examples 106-108 document the explicit VST disk-stream/recording extensions.
Normal play() sample playback still uses the separate native WAV/PlayBuf path.
