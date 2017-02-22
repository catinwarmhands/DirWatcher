//  #DirWatcher
//  
//  DirWatcher is a header-only c++ library
//  for watching changes in folder using WinAPI.
//  Process will run in background thread.
//  
//  ###Dependencies:
//  1. `<thread>` for threading
//  2. `<windows.h>` for WinAPI calls
//  
//  ###Copyright:
//  (c) 2017 by CIWH
//  
//  ###License
//  public domain
//  
//  ##Basic usage:
//  
//  0. (Optional) You can define some actions before including: 
//     `DIRWATCHER_FAILED_WATCH_DIR_ACTION` will be called if setting handle is failed, 
//     `DIRWATCHER_FAILED_CLOSE_HANDLE_ACTION` will be called if closing hanle failed, 
//     `DIRWATCHER_DEFAULT_CALLBACK_MESSAGE` will be inserted as code for default callback,
//     `DIRWATCHER_MESSAGE_BUFFER_SIZE` is message buffer size (default is 1024 bytes)
//  1. `#include "path/to/this/file/dirwatcher.hpp"`
//  2. Create object: `ciwh::DirWatcher watcher`; 
//     Defaults: dir is `.`, non-recursive (dont watch subfolders)
//  3. Set callback:  `
//     watcher.setCallback([](ciwh::FileActionType type, const char* filename) {
//     	   /*your code here*/
//     });
//  `
//  4. Run: `watcher.start();`
//  5. (Optional, will be called in destructor) Stop: `watcher.stop();`
//  
//  You can chande dir via `setDir(const char* path)` method, set recursive mode via `setRecursive(bool b)` method. If wather was running, it will restart.
//  
//  Getters are: `bool isRecursive()`, `bool isRunning()`, `const char* const getDir()`


#ifndef DIRWATCHER_HPP_
#define DIRWATCHER_HPP_

#if !defined(_WIN32) || !defined(_WIN64)
	#error DirWatcher only works on windows for now...
#endif

#if !defined(DIRWATCHER_FAILED_WATCH_DIR_ACTION) || !defined(DIRWATCHER_FAILED_CLOSE_HANDLE_ACTION) || !defined(DIRWATCHER_DEFAULT_CALLBACK_MESSAGE)
	#include <cstdio> //for printing default messages
#endif

#ifndef DIRWATCHER_FAILED_WATCH_DIR_ACTION
	#define DIRWATCHER_FAILED_WATCH_DIR_ACTION   {printf("Failed to watch dir '%s', error code '%lu', exiting...", dir, GetLastError()); std::exit(0);}
#endif

#ifndef DIRWATCHER_FAILED_CLOSE_HANDLE_ACTION
	#define DIRWATCHER_FAILED_CLOSE_HANDLE_ACTION printf("Failed to close directory watching handle, error code '%lu'", GetLastError())
#endif

#ifndef DIRWATCHER_DEFAULT_CALLBACK_MESSAGE
	#define DIRWATCHER_DEFAULT_CALLBACK_MESSAGE   printf("Default DirWatcher callback: action '%d', filename '%s'", (int)type, filename)
#endif

#ifndef DIRWATCHER_MESSAGE_BUFFER_SIZE
 	#define DIRWATCHER_MESSAGE_BUFFER_SIZE 1024
#endif

#include <thread>
#include <windows.h>

namespace ciwh {

enum class FileActionType {
	ADDED,
	REMOVED,
	MODIFIED,
	RENAMED_OLD_NAME,
	RENAMED_NEW_NAME,
};

class DirWatcher {
private:
	HANDLE hDir;
	std::thread th;
	const char* dir = ".";
	bool recursive = true;
	bool isactive = false;
	void (*callback)(FileActionType, const char* filename);
public:
	DirWatcher(const DirWatcher& other) = delete;
	auto operator=(const DirWatcher& other) = delete;

	DirWatcher() {
		//setting default callback
		callback = [](FileActionType type, const char* filename) {
			DIRWATCHER_DEFAULT_CALLBACK_MESSAGE;
		};
	}

	bool isRecursive() { return recursive; }
	bool isRunning()   { return isactive;  }
	const char* const getDir() {return dir;}

	void setRecursive(bool r) {
		bool wasRunning = isactive;
		stop();
		recursive = r;
		if (wasRunning) start();
	}

	void setDir(const char* d) {
		bool wasRunning = isactive;
		stop();
		dir = d;
		if (wasRunning) start();
	}

	void setCallback(void (*func)(FileActionType, const char* filename)) {
		bool wasRunning = isactive;
		stop();
		callback = func;
		if (wasRunning) start();
	}

	void stop() {
		if (th.joinable()) {
			th.join();
			BOOL success = FindCloseChangeNotification(hDir);
			if (success == FALSE) {
				DIRWATCHER_FAILED_CLOSE_HANDLE_ACTION;
			}
		}
		isactive = false;
	}

	void start() {
		hDir = CreateFile( 
			dir,                                // pointer to the file name
			FILE_LIST_DIRECTORY,                // access (read/write) mode
			// Share mode MUST be the following to avoid problems with renames via Explorer!
			FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, // share mode
			NULL,                               // security descriptor
			OPEN_EXISTING,                      // how to create
			FILE_FLAG_BACKUP_SEMANTICS,         // file attributes
			NULL                                // file with attributes to copy
		);

		if (hDir == INVALID_HANDLE_VALUE) {
			DIRWATCHER_FAILED_WATCH_DIR_ACTION;
		}

		th = std::thread([&](){
			TCHAR szBuffer[DIRWATCHER_MESSAGE_BUFFER_SIZE];
			DWORD BytesReturned;
			while(ReadDirectoryChangesW(
				hDir,                          // handle to directory
				&szBuffer,                     // read results buffer
				sizeof(szBuffer),              // length of buffer
				(recursive ? TRUE : FALSE),    // monitoring option
				FILE_NOTIFY_CHANGE_SECURITY |
				FILE_NOTIFY_CHANGE_CREATION |
				FILE_NOTIFY_CHANGE_LAST_WRITE |
				FILE_NOTIFY_CHANGE_SIZE |
				FILE_NOTIFY_CHANGE_ATTRIBUTES |
				FILE_NOTIFY_CHANGE_DIR_NAME |
				FILE_NOTIFY_CHANGE_FILE_NAME,  // filter conditions
				&BytesReturned,                // bytes returned
				NULL,                          // overlapped buffer
				NULL                           // completion routine
				)
			) {
				DWORD dwOffset = 0;
				FILE_NOTIFY_INFORMATION* pInfo = NULL;
				do {
					// Get a pointer to the first change record...
					pInfo = (FILE_NOTIFY_INFORMATION*)&szBuffer[dwOffset];
					FileActionType fat;
					switch (pInfo->Action) {
						case FILE_ACTION_ADDED:            fat = FileActionType::ADDED;            break; 
						case FILE_ACTION_REMOVED:          fat = FileActionType::REMOVED;          break; 
						case FILE_ACTION_MODIFIED:         fat = FileActionType::MODIFIED;         break; 
						case FILE_ACTION_RENAMED_OLD_NAME: fat = FileActionType::RENAMED_OLD_NAME; break; 
						case FILE_ACTION_RENAMED_NEW_NAME: fat = FileActionType::RENAMED_NEW_NAME; break;
					}

					// ReadDirectoryChangesW processes filenames in Unicode. We will convert them to a TCHAR format...
					TCHAR szFileName[MAX_PATH] = {0};
					WideCharToMultiByte(CP_ACP, NULL, pInfo->FileName, pInfo->FileNameLength, szFileName, sizeof(szFileName) / sizeof(TCHAR), NULL, NULL);
					szFileName[pInfo->FileNameLength / 2] = 0;

					callback(fat, szFileName);

					// More than one change may happen at the same time. Load the next change and continue...
					dwOffset += pInfo->NextEntryOffset;
				} while (pInfo->NextEntryOffset != 0);
			}
		});

		isactive = true;
	}

	~DirWatcher() {
		stop();
	}
};

} //namespace ciwh

#endif //DIRWATCHER_HPP_
