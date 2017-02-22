#DirWatcher

DirWatcher is a header-only c++ library
for watching changes in folder using WinAPI.
Process will run in background thread.

###Dependencies:
1. `<thread>` for threading
2. `<windows.h>` for WinAPI calls

###Copyright:
(c) 2017 by CIWH

###License
Public domain

##Basic usage:

0. (Optional) You can define some actions before including: 
 * `DIRWATCHER_FAILED_WATCH_DIR_ACTION` will be called if setting handle is failed, 
 * `DIRWATCHER_FAILED_CLOSE_HANDLE_ACTION` will be called if closing hanle failed, 
 * `DIRWATCHER_DEFAULT_CALLBACK_MESSAGE` will be inserted as code for default callback,
 * `DIRWATCHER_MESSAGE_BUFFER_SIZE` is message buffer size (default is 1024 bytes)
1. `#include "path/to/this/file/dirwatcher.hpp"`
2. Create object: `ciwh::DirWatcher watcher`; 
 * Defaults: dir is `.`, non-recursive (dont watch subfolders)
3. Set callback:  `
watcher.setCallback([](ciwh::FileActionType type, const char* filename) {
	/*your code here*/
});
`
4. Run: `watcher.start();`
5. (Optional, will be called in destructor) Stop: `watcher.stop();`

You can chande dir via `setDir(const char* path)` method, set recursive mode via `setRecursive(bool b)` method. If wather was running, it will restart.

Getters are: `bool isRecursive()`, `bool isRunning()`, `const char* const getDir()`
