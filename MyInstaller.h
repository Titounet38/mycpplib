#ifndef __INSTALLER__
#define __INSTALLER__


#include "WinUtils.h"
#include "Axis.h"
#include <functional>
#include <stdexcept>
#include <comdef.h>

struct InstallFile {
	aVect<wchar_t> sourceName;
	aVect<wchar_t> destName;
	bool erase;

	InstallFile(const wchar_t * sourceName, const wchar_t * destName, bool erase = true)
		: sourceName(sourceName), destName(destName), erase(erase) {}

	InstallFile(const wchar_t * name, bool erase = true)
		: sourceName(name), destName(name), erase(erase) {}

	InstallFile(const char * sourceName, const char * destName, bool erase = true)
		: sourceName(sourceName), destName(destName), erase(erase) {}

	InstallFile(const char * name, bool erase = true)
		: sourceName(name), destName(name), erase(erase) {}

	//virtual ~InstallFile() {}

	//virtual void PreCopyTask() {}
	//virtual void PostCopyTask() {}
	
	std::function<void()> preCopyTask;
	std::function<void()> postCopyTask;

};

inline void MyCreateDirectory(const aVect<wchar_t> & folder) {
	
	if (!folder) MY_ERROR("Unable to create destination path");

	if (!CreateDirectoryW(folder, NULL)) {
		auto lastErr = GetLastError();
		if (lastErr == ERROR_PATH_NOT_FOUND) {
			aVect<wchar_t> subFolder;
			SplitPathW(folder, nullptr, &subFolder, nullptr, nullptr);
			MyCreateDirectory(subFolder);
			SAFE_CALL(CreateDirectoryW(folder, NULL));
		}
		else if (lastErr == ERROR_ALREADY_EXISTS) {
			return; /* ok */
		}
	}
}

//template <class UserData = EmptyClass>
//struct MyInstaller_Old {
//
//	enum CopyCallbackReturnValue { retry, next, abort };
//
//	LogFile logFile;
//	UserData data;
//	bool interruptible = true;
//	bool allowNetworkInstallationDirectory = false;
//
//	aVect<InstallFile> files;
//
//	aVect<wchar_t> regPath;
//	aVect<wchar_t> regValueName;
//
//	aVect<wchar_t> sourcePath;
//	aVect<wchar_t> destPath;
//
//	aVect<wchar_t> title;
//	aVect<wchar_t> appName;
//
//	ProgressBar progressBar;
//
//	virtual ~MyInstaller_Old() {}
//
//	virtual CopyCallbackReturnValue CopyCallback(const InstallFile & file, DWORD errCode, const wchar_t * errMsg) {
//
//		if (errCode == ERROR_SUCCESS) {
//			return CopyCallbackReturnValue::next;
//		} else {
//			auto answer = MessageBoxW(NULL, errMsg, L"Error", MB_ICONERROR | MB_ABORTRETRYIGNORE);
//
//			if (answer == IDABORT) {
//				return CopyCallbackReturnValue::abort;
//			}
//			else if (answer == IDIGNORE) {
//				if (this->logFile) this->logFile.xPrintf("File NOT copied:\nFrom:\n%s\nTo:\n%s\n\n", sourcePath, destPath);
//				return CopyCallbackReturnValue::next;
//			}
//			else if (answer == IDRETRY) {
//				return CopyCallbackReturnValue::retry;
//			}
//			else {
//				MY_ERROR("???");
//			}
//		}
//
//		return CopyCallbackReturnValue::retry;
//	}
//
//	virtual int Abort(const wchar_t * prependMsg = nullptr) {
//
//		aVect<wchar_t> msg = "Installation aborted.";
//
//		if (prependMsg) msg.Prepend(msg);
//
//		if (this->logFile) this->logFile.xPrintf(L"%s\n", msg);
//		MessageBoxW(NULL, msg, L"Error", MB_ICONINFORMATION);
//		return 0;
//	}
//
//	virtual bool ValidateDestinationPath() {
//		for (;;) {
//
//			if (!this->allowNetworkInstallationDirectory && IsNetworkPath(this->destPath)) {
//				auto msg = xFormat(L"Install directory must not be on a network drive:\n\n%s\n", this->destPath);
//				MessageBoxW(NULL, msg, this->title ? this->title : L"", MB_ICONERROR);
//				this->destPath.Erase();
//			}
//
//			if (this->destPath) {
//
//				auto msg = xFormat(L"Install directory is:\n\n%s\n\nUse this directory?", this->destPath);
//				auto answer = MessageBoxW(NULL, msg, this->title ? this->title : L"", MB_ICONINFORMATION | MB_YESNOCANCEL);
//				if (answer == IDYES) {
//					break;
//				} else if (answer == IDCANCEL) {
//					return false;
//				} else {
//					this->destPath.Erase();
//				}
//			}
//
//			if (!this->destPath) {
//				auto msg = this->appName ? xFormat(L"Please select the directory where %s will be installed", this->appName) : L"Please select the installation directory";
//				auto answer = MessageBoxW(NULL, msg, this->title ? this->title : L"", MB_ICONINFORMATION | MB_OKCANCEL);
//				if (answer == IDCANCEL) {
//					return false;
//				}
//				this->destPath = BrowseForFolder(this->destPath, L"Installation directory");
//				if (this->destPath) {
//					if (!WriteRegistry(HKEY_CURRENT_USER, regPath, L"InstallFolder", this->destPath)) MY_ERROR("reg error");
//				}
//			}
//		}
//
//		return true;
//	}
//
//	virtual void Success() {
//		MessageBoxW(NULL, L"Installation finished.", this->title ? this->title : "", MB_ICONINFORMATION);
//		_wsystem(xFormat(L"explorer \"%s\"", this->destPath));
//	}
//
//	int Proceed(bool logHeader = true) {
//
//		if (this->regPath && this->regValueName) {
//			if (this->destPath) MY_ERROR("destPath and regPath both set");
//			this->destPath = GetRegistry(HKEY_CURRENT_USER, this->regPath, this->regValueName);
//			if (!this->allowNetworkInstallationDirectory && IsNetworkPath(this->regPath)) this->regPath.Erase();
//		} else {
//			if (this->regPath || this->regValueName) MY_ERROR("regPath or regValueName not set");
//		}
//		
//		if (!IsDirectory(this->destPath)) this->destPath.Free();
//
//		if (!this->ValidateDestinationPath()) {
//			return this->Abort();
//		}
//
//		if (this->logFile) {
//			this->logFile.EnableDisplay();
//			if (logHeader) {
//				this->logFile.NoTime().xPrintf(
//					"===================================================================\n"
//					"===================================================================\n"
//					"===================================================================\n");
//				this->logFile.PrintDate().xPrintf("[%s]\n", getenv("USERNAME"));
//			}
//			this->logFile.xPrintf(L"Source folder: \"%s\"\n", this->sourcePath);
//			this->logFile.xPrintf(L"Destination folder: \"%s\"\n\n", this->destPath);
//		}
//
//		this->progressBar.SetTitle(this->title ? this->title : "Installation");
//		this->progressBar.SetInterruptible(this->interruptible);
//
//		aVect_for(this->files, i) {
//
//			auto&& f = this->files[i];
//
//			auto sourcePath = xFormat(L"%s\\%s", this->sourcePath, f.sourceName);
//
//			if (auto ptr = StrChrW(sourcePath, L'*')) {
//				if (ptr[1] != 0) MY_ERROR("TODO");
//				if (ptr[-1] != L'\\') MY_ERROR("TODO");
//
//				ptr[0] = 0;
//				ptr[-1] = 0;
//
//				auto sourceFolder = f.sourceName;
//				auto destFolder = f.destName;
//				
//				ptr = StrChrW(sourceFolder, L'*');
//				ptr[0] = 0;
//				ptr[-1] = 0;
//
//				this->files.Remove(i);
//
//				RecursiveFileEnumerator fileEnumerator;
//
//				fileEnumerator.SetPath(sourcePath);
//
//				for (;;) {
//					auto data = fileEnumerator.GetCurrentData();
//					if (!IsDirectory(data->dwFileAttributes)) {
//
//						auto path = fileEnumerator.GetCurrentFileSubPath();
//						this->files.Insert(i, xFormat(L"%s\\%s", sourceFolder, path), xFormat(L"%s\\%s", destFolder, path));
//					}
//					if (!fileEnumerator.Next()) break;
//				}
//			}
//		}
//
//		for (auto&& f : this->files) {
//
//			progressBar.SetProgress((double)this->files.Index(f) / this->files.Count());
//			if (!progressBar) return Abort();
//
//			auto sourcePath = xFormat(L"%s\\%s", this->sourcePath, f.sourceName);
//			auto destPath = xFormat(L"%s\\%s", this->destPath, f.destName);
//
//			for (;;) {
//				DWORD errCode = ERROR_SUCCESS;
//				aVect<wchar_t> errMsg;
//				aVect<wchar_t> destPathFolder;
//
//				
//				SplitPathW(destPath, nullptr, &destPathFolder, nullptr, nullptr);
//				if (!IsDirectory(destPathFolder)) {
//					MyCreateDirectory(destPathFolder);
//				}
//
//				if (CopyFileW(sourcePath, destPath, !f.erase)) {
//					if (this->logFile) this->logFile.xPrintf(L"File copied:\nFrom:\n%s\nTo:\n%s\n\n", sourcePath, destPath);
//				} else {
//					errCode = GetLastError();
//					if (errCode == ERROR_FILE_EXISTS) {
//						if (this->logFile) this->logFile.xPrintf(L"File NOT copied:\nFrom:\n%s\nTo:\n%s\n\n", sourcePath, destPath);
//						errCode = ERROR_SUCCESS;
//					} else {
//						auto err = GetErrorDescription(errCode, false);
//						errMsg = xFormat(L"Copy error:\n\n%S\n\nFrom:\n%s\n\nTo:\n%s\n\n", err, sourcePath, destPath);
//						if (this->logFile) this->logFile.xPrintf(L"%s\n", errMsg);
//					}
//				}
//
//				auto retVal = this->CopyCallback(f, errCode, errMsg);
//
//				if (retVal == CopyCallbackReturnValue::abort) return this->Abort();
//				else if (retVal == CopyCallbackReturnValue::retry) continue;	/* loop */
//				else if (retVal == CopyCallbackReturnValue::next) break;		/* continue with next file */
//				else MY_ERROR("???");
//			}
//		}
//
//		this->progressBar.Close();
//
//		this->Success();
//
//		return 1;
//	}
//};

struct MyInstaller {

	enum CopyCallbackReturnValue { retry, next, abort };

	LogFile * pLogFile = nullptr;
	bool interruptible = true;
	bool allowNetworkInstallationDirectory = false;
	bool aborted = false;

	aVect<InstallFile> files;

	aVect<wchar_t> regPath;
	aVect<wchar_t> regValueName;

	aVect<wchar_t> sourcePath;
	aVect<wchar_t> destPath;

	aVect<wchar_t> title;
	aVect<wchar_t> appName;

	ProgressBar progressBar;

	virtual ~MyInstaller() {}

	//virtual ~MyInstaller() {
	//	for (auto&& f : this->files) {
	//		delete f;
	//	}
	//}
	 
	virtual CopyCallbackReturnValue CopyCallback(InstallFile & file, DWORD errCode, const wchar_t * errMsg) {

		if (errCode == ERROR_SUCCESS) {
			return CopyCallbackReturnValue::next;
		}
		else {
			auto answer = MessageBoxW(NULL, errMsg, L"Error", MB_SERVICE_NOTIFICATION | MB_ICONERROR | MB_ABORTRETRYIGNORE);

			if (answer == IDABORT) {
				return CopyCallbackReturnValue::abort;
			}
			else if (answer == IDIGNORE) {
				this->Log(L"File NOT copied:\nFrom:\n%s\nTo:\n%s\n\n", 
					xFormat(L"%s\\%s", sourcePath, file.sourceName) ,
					xFormat(L"%s\\%s", destPath, file.destName));
				return CopyCallbackReturnValue::next;
			}
			else if (answer == IDRETRY) {
				return CopyCallbackReturnValue::retry;
			}
			else {
				MY_ERROR("???");
			}
		}

		return CopyCallbackReturnValue::retry;
	}

	template <class... Args>
	void Log(Args&&... args) {
		if (this->pLogFile && *this->pLogFile) {
			//this->pLogFile->Printf("coucou %S", L"dkgsl");
			//this->pLogFile->Printf(std::forward<Args>(args)...);
			this->pLogFile->xPrintf(args...);
		}
	}

	virtual int Abort(const wchar_t * prependMsg = nullptr) {

		aVect<wchar_t> msg = "Installation aborted.";

		if (prependMsg) msg.Prepend(msg);
		this->aborted = true;
		this->Log(L"%s\n", msg);
		MessageBoxW(NULL, msg, L"Error", MB_ICONINFORMATION | MB_SERVICE_NOTIFICATION);
		return 0;
	}

	bool WriteRegistry(HKEY key, const wchar_t * path, const wchar_t * valueName, const wchar_t * value) {

		this->Log(L"Reg Write: \"%s\" to \"%s\\%s\"...\n", value, path, valueName);

		LSTATUS status;

		auto retVal = ::WriteRegistry(key, path, valueName, value, &status);

		if (retVal) {
			auto check = this->GetRegistry(HKEY_CURRENT_USER, path, valueName, false);

			if (check && this->destPath) {
				if (_wcsicmp(check, this->destPath) != 0) {
					this->Log(L"????  Reg Read: \"%s\" from \"%s\\%s\"\n", value, path, valueName);
				} else {
					this->Log(L"Done.\n");
				}
			}
		} else {
			this->Log(L"Reg error: unable to write to \"%s\\%s\", error 0x%p: \"%s\"", path, valueName, status, GetWin32ErrorDescription(status, false));
		}

		return retVal;
	}

	aVect<wchar_t> GetRegistry(HKEY key, const wchar_t * path, const wchar_t * valueName, bool log = true) {
		aVect<wchar_t> retVal;
		auto r = ::GetRegistry(retVal, key, path, valueName);
		if (r != ERROR_SUCCESS) {
			this->Log(L"Reg error: unable to read from \"%s\\%s\", error 0x%p: \"%s\"\n", path, valueName, r, GetWin32ErrorDescription(r, false));
		} else {
			if (log) this->Log(L"Reg Read: \"%s\" from \"%s\\%s\"\n", retVal, path, valueName);
		}

		return retVal;
	}

	virtual bool ValidateDestinationPath() {
		for (;;) {

			if (!this->allowNetworkInstallationDirectory && IsNetworkPath(this->destPath)) {
				auto msg = xFormat(L"Install directory must not be on a network drive:\n\n%s\n", this->destPath);
				MessageBoxW(NULL, msg, this->title ? this->title : L"", MB_ICONERROR | MB_SERVICE_NOTIFICATION);
				this->destPath.Erase();
			}

			if (this->destPath) {

				auto msg = xFormat(L"Install directory is:\n\n%s\n\nUse this directory?", this->destPath);
				auto answer = MessageBoxW(NULL, msg, this->title ? this->title : L"", MB_ICONINFORMATION | MB_YESNOCANCEL | MB_SERVICE_NOTIFICATION);
				if (answer == IDYES) {
					break;
				} else if (answer == IDCANCEL) {
					return false;
				} else {
					this->destPath.Erase();
				}
			}

			if (!this->destPath) {
				auto msg = this->appName ? xFormat(L"Please select the directory where %s will be installed.", this->appName) : L"Please select the installation directory.";
				auto answer = MessageBoxW(NULL, msg, this->title ? this->title : L"", MB_ICONINFORMATION | MB_OKCANCEL | MB_SERVICE_NOTIFICATION);
				if (answer == IDCANCEL) {
					return false;
				}
				this->destPath = BrowseForFolder(this->destPath, L"Installation directory");
				if (this->destPath) {
					if (!this->WriteRegistry(HKEY_CURRENT_USER, regPath, this->regValueName, this->destPath)) throw std::runtime_error("Unable to write to registry");
				}
			}
		}

		return true;
	}

	virtual void Success(bool showInstallFolder = true, bool messageBox = true) {
		if (messageBox) MessageBoxW(NULL, L"Installation successful.", this->title ? this->title : "", MB_ICONINFORMATION | MB_SERVICE_NOTIFICATION);
		if (showInstallFolder) _wsystem(xFormat(L"explorer \"%s\"", this->destPath));
	}

	virtual int Proceed(bool logHeader = true) {

		this->aborted = false;

		if (this->pLogFile && *this->pLogFile) {
			this->pLogFile->EnableDisplay();
			if (logHeader) {
				this->pLogFile->NoTime().xPrintf(
					"===================================================================\n"
					"===================================================================\n"
					"===================================================================\n");
				this->pLogFile->PrintDate().xPrintf("[%s]\n", getenv("USERNAME"));
			}
		}

		if (this->regPath && this->regValueName && this->destPath) {
			if (!WriteRegistry(HKEY_CURRENT_USER, this->regPath, this->regValueName, this->destPath)) throw std::runtime_error("Unable to write to registry");
			this->GetRegistry(HKEY_CURRENT_USER, this->regPath, this->regValueName);
		} else {
			if (this->regPath && this->regValueName) {
				if (this->destPath) MY_ERROR("destPath and regPath both set");
				this->destPath = GetRegistry(HKEY_CURRENT_USER, this->regPath, this->regValueName);
				if (!this->allowNetworkInstallationDirectory && IsNetworkPath(this->destPath)) this->destPath.Erase();
			} else {
				if (this->regPath || this->regValueName) MY_ERROR("regPath or regValueName not set");
			}

			if (!IsDirectory(this->destPath)) this->destPath.Free();

			if (!this->ValidateDestinationPath()) {
				return this->Abort();
			}
		}

		if (logHeader) {
			this->Log(L"Source folder: \"%s\"\n", this->sourcePath);
			this->Log(L"Destination folder: \"%s\"\n\n", this->destPath);
		}

		this->progressBar.SetAlwaysOnTop(true);
		this->progressBar.SetTitle(this->title ? this->title : "Installation");
		this->progressBar.SetInterruptible(this->interruptible);

		aVect_for(this->files, i) {

			auto&& f = this->files[i];

			auto sourcePath = xFormat(L"%s\\%s", this->sourcePath, f.sourceName);

			if (auto ptr = StrChrW(sourcePath, L'*')) {
				if (ptr[1] != 0) MY_ERROR("TODO");
				if (ptr[-1] != L'\\') MY_ERROR("TODO");

				ptr[0] = 0;
				ptr[-1] = 0;

				auto sourceFolder = f.sourceName;
				auto destFolder = f.destName;

				ptr = StrChrW(sourceFolder, L'*');
				ptr[0] = 0;
				ptr[-1] = 0;

				this->files.Remove(i);

				RecursiveFileEnumerator fileEnumerator;

				fileEnumerator.SetPath(sourcePath);

				for (;;) {
					auto data = fileEnumerator.GetCurrentData();
					if (!IsDirectory(data->dwFileAttributes)) {

						auto path = fileEnumerator.GetCurrentFileSubPath();
						//auto iFile = new InstallFile(xFormat(L"%s\\%s", sourceFolder, path), xFormat(L"%s\\%s", destFolder, path));
						auto iFile = InstallFile(xFormat(L"%s\\%s", sourceFolder, path), xFormat(L"%s\\%s", destFolder, path));
						this->files.Insert(i, iFile);
					}
					if (!fileEnumerator.Next()) break;
				}
			}
		}

		for (auto&& f : this->files) {

			progressBar.SetProgress((double)this->files.Index(f) / this->files.Count());
			if (!progressBar) return Abort();

			auto sourcePath = xFormat(L"%s\\%s", this->sourcePath, f.sourceName);
			auto destPath = xFormat(L"%s\\%s", this->destPath, f.destName);

			for (;;) {
				DWORD errCode = ERROR_SUCCESS;
				aVect<wchar_t> errMsg;
				aVect<wchar_t> destPathFolder;


				SplitPathW(destPath, nullptr, &destPathFolder, nullptr, nullptr);
				if (!IsDirectory(destPathFolder)) {
					MyCreateDirectory(destPathFolder);
				}

				if (f.preCopyTask) f.preCopyTask();

				if (CopyFileW(sourcePath, destPath, !f.erase)) {
					this->Log(L"File copied:\nFrom:\n%s\nTo:\n%s\n\n", sourcePath, destPath);
				}
				else {
					errCode = GetLastError();
					if (errCode == ERROR_FILE_EXISTS) {
						this->Log(L"File NOT copied:\nFrom:\n%s\nTo:\n%s\n\n", sourcePath, destPath);
						errCode = ERROR_SUCCESS;
					}
					else {
						auto err = GetWin32ErrorDescription(errCode, false);
						errMsg = xFormat(L"Copy error:\n\n%s\n\nFrom:\n%s\n\nTo:\n%s\n\n", err, sourcePath, destPath);
						this->Log(L"%s\n", errMsg);
					}
				}

				if (f.postCopyTask) f.postCopyTask();

				auto retVal = this->CopyCallback(f, errCode, errMsg);

				if (retVal == CopyCallbackReturnValue::abort) return this->Abort();
				else if (retVal == CopyCallbackReturnValue::retry) continue;	/* loop */
				else if (retVal == CopyCallbackReturnValue::next) break;		/* continue with next file */
				else MY_ERROR("???");
			}
		}

		this->progressBar.Close();

		this->Success();

		return 1;
	}

	void ErrorHandler(std::exception_ptr & e) {

		try {
			std::rethrow_exception(e);
		}
		catch (const std::runtime_error & e) {
			auto msg = xFormat(L"Runtime error exception: %S\n\nInstallation aborted.\n", e.what());
			this->Log("%S", msg);
			MessageBoxW(NULL, msg, this->title, MB_ICONERROR | MB_SERVICE_NOTIFICATION);
		}
		catch (const _com_error & e) {
			aVect<wchar_t> msg;
			BSTR descBSTR;
			if (e.ErrorInfo() && e.ErrorInfo()->GetDescription(&descBSTR)) {
				_bstr_t desc;
				desc.Attach(descBSTR);
				msg = xFormat(L"COM error: %s\n\nInstallation aborted.\n", descBSTR);
			}
			else {
				msg = xFormat(L"COM error: %s\n\nInstallation aborted.\n", e.ErrorMessage());
			}
			this->Log("%S", msg);
			MessageBoxW(NULL, msg, this->title, MB_ICONERROR | MB_SERVICE_NOTIFICATION);
		}
	}
};



#endif
