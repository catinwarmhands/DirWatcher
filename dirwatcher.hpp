#if 0 //README
#DirWatcher

DirWatcher is a header-only c++ library
for watching changes in folder using WinAPI.
Process will run in background thread.

###Dependencies:
1. `<thread>` for threading
2. `<windows.h>` for WinAPI calls
3. `<function>` (optional, see below) for callback function

###Copyright:
(c) 2017 by CIWH

###License
Public domain

##Usage:
0. (Optional) You can define some actions before including: 
 * `DIRWATCHER_FAILED_WATCH_DIR_ACTION` will be called if setting handle is failed, 
 * `DIRWATCHER_FAILED_CLOSE_HANDLE_ACTION` will be called if closing hanle failed, 
 * `DIRWATCHER_DEFAULT_CALLBACK_MESSAGE` will be inserted as code for default callback,
 * `DIRWATCHER_MESSAGE_BUFFER_SIZE` is message buffer size (default is 1024 bytes)
 * `DIRWATCHER_USE_STD_FUNCTION` if defined - will use `std::function` instead of function pointer for callback
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

You can chande dir via `setDir(const char* path)` method, set recursive mode via `setRecursive(bool b)` method. 
If watcher was running, it will restart.

Getters are: `bool isRecursive()`, `bool isRunning()`, `const char* const getDir()`
#endif //README


#ifndef DIRWATCHER_HPP_
#define DIRWATCHER_HPP_

#if !defined(_WIN32) && !defined(_WIN64)
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
 	#define DIRWATCHER_MESSAGE_BUFFER_SIZE 65535
#endif

#include <thread>
#include <windows.h>

#ifdef DIRWATCHER_USE_STD_FUNCTION
	#include <functional>
#endif

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
	bool recursive = false;
	bool isrunning = false;
	HANDLE events[2]; //[0] - file changed, [1] - thread should be terminated;

	#ifdef DIRWATCHER_USE_STD_FUNCTION
		std::function<void(FileActionType, const char*)> callback;
	#else
		void (*callback)(FileActionType, const char*);
	#endif

public:
	DirWatcher(const DirWatcher& other) = delete;
	auto operator=(const DirWatcher& other) = delete;

	DirWatcher() {
		events[0] = CreateEvent(NULL, FALSE, FALSE, NULL);
		events[1] = CreateEvent(NULL, FALSE, FALSE, NULL);

		//setting default callback
		callback = [](FileActionType type, const char* filename) {
			DIRWATCHER_DEFAULT_CALLBACK_MESSAGE;
		};
	}

	bool isRecursive() { return recursive; }
	bool isRunning()   { return isrunning; }
	const char* const getDir() {return dir;}

	void setRecursive(bool r) {
		bool wasRunning = isrunning;
		stop();
		recursive = r;
		if (wasRunning) start();
	}

	void setDir(const char* d) {
		bool wasRunning = isrunning;
		stop();
		dir = d;
		if (wasRunning) start();
	}

	void setCallback(
		#ifdef DIRWATCHER_USE_STD_FUNCTION
			std::function<void(FileActionType, const char*)> func
		#else
			void (*func)(FileActionType, const char*)
		#endif
	) {
		bool wasRunning = isrunning;
		stop();
		callback = func;
		if (wasRunning) start();
	}

	void stop() {
		isrunning = false;
		if (th.joinable()) {
			SetEvent(events[1]);
			th.join();

			BOOL success = CloseHandle(hDir);
			if (success == FALSE) {
				DIRWATCHER_FAILED_CLOSE_HANDLE_ACTION;
			}
		}
	}

	void start() {
		if (isrunning) {
			stop();
		}
		isrunning = true;

		hDir = CreateFile( 
			dir,                                // pointer to the file name
			FILE_LIST_DIRECTORY,                // access (read/write) mode
			// Share mode MUST be the following to avoid problems with renames via Explorer!
			FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, // share mode
			NULL,                               // security descriptor
			OPEN_EXISTING,                      // how to create
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,         // file attributes
			NULL                                // file with attributes to copy
		);

		if (hDir == INVALID_HANDLE_VALUE) {
			DIRWATCHER_FAILED_WATCH_DIR_ACTION;
		}

		th = std::thread([&](){
			OVERLAPPED stOverlapped;
			ZeroMemory(&stOverlapped, sizeof(stOverlapped));

			stOverlapped.hEvent = events[0];

			TCHAR szBuffer[DIRWATCHER_MESSAGE_BUFFER_SIZE];
			DWORD BytesReturned;

			while (isrunning) {
				ReadDirectoryChangesW(
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
					&stOverlapped,                 // overlapped buffer
					NULL                           // completion routine
				);
				
				DWORD dwWaitRes = WaitForMultipleObjects(2, events, FALSE, INFINITE);

				//terminating event was raised
				if (dwWaitRes == WAIT_OBJECT_0 + 1) {
					return;
				}

				DWORD dwBytesRead = 0;
				PFILE_NOTIFY_INFORMATION pInfo = NULL;
				GetOverlappedResult(hDir, &stOverlapped, &dwBytesRead, FALSE);

				pInfo = (PFILE_NOTIFY_INFORMATION)(&szBuffer);

				DWORD fileNameLength = pInfo->FileNameLength / sizeof(WCHAR);

				char* szFileName = new char[fileNameLength+1];
				memset(szFileName, 0, fileNameLength+1);
				
				WideCharToMultiByte(CP_OEMCP, NULL, pInfo->FileName, pInfo->FileNameLength / sizeof(WCHAR), szFileName, fileNameLength / sizeof(TCHAR), NULL, NULL);

				FileActionType fat;
				switch (pInfo->Action) {
					case FILE_ACTION_ADDED:            fat = FileActionType::ADDED;            break; 
					case FILE_ACTION_REMOVED:          fat = FileActionType::REMOVED;          break; 
					case FILE_ACTION_MODIFIED:         fat = FileActionType::MODIFIED;         break; 
					case FILE_ACTION_RENAMED_OLD_NAME: fat = FileActionType::RENAMED_OLD_NAME; break; 
					case FILE_ACTION_RENAMED_NEW_NAME: fat = FileActionType::RENAMED_NEW_NAME; break;
				}

				callback(fat, szFileName);

				delete[] szFileName;
			}
		});
	}

	~DirWatcher() {
		stop();
	}
};

} //namespace ciwh

#endif //DIRWATCHER_HPP_
