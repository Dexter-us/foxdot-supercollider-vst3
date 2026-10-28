Dexter U.S. FoxDot - SuperCollider UGen modules

This is a separate one-time companion pack for the Windows VST3. Extract its Portable folder beside
the dexter_sc_tone.vst3 bundle so the layout is:

  <VST3 folder>/
    dexter_sc_tone.vst3/
    Portable/SCPlugins/*.scx

The plugin scans this sibling folder and older in-bundle locations at startup. Keep this Portable
folder when replacing the .vst3 bundle; download the UGen pack again only when the SuperCollider
engine/module set changes. Restart the DAW after adding or changing a module.

The pack contains the selected self-contained SuperCollider modules built from the pinned source
revision. It excludes DiskIO_UGens and libsndfile. The module filenames shown by the editor only
confirm that files were found; they do not guarantee every module loaded successfully.

Only add native modules from sources you trust. A .scx file is executable native code and must match
this plugin's pinned SuperCollider API/build. Check its license and any required DLLs yourself;
the loader cannot validate either. Third-party or duplicate modules may prevent UGens from registering.
The bundled UIUGens module provides global desktop mouse input; no sample assets or FoxDot sample-player
controls are included.
