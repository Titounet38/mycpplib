#define _CRT_SECURE_NO_WARNINGS

#include "ExcelAddIns.h"
#include "MyUtils.h"

#include <stdexcept>

namespace {

	void BusyExcelPrompt(const wchar_t * title) {
		auto ans = MessageBoxW(NULL, L"Excel is busy.\n\nRetry ?", title, MB_ICONEXCLAMATION & MB_OKCANCEL);
		if (ans != IDOK) throw std::runtime_error("Excel is busy");
	}

	BOOL CALLBACK EnumFuncFindExcel(HWND hWnd, LPARAM param) {

		auto&& handles = *(aVect<HWND>*)param;

		char className[50];

		if (RealGetWindowClassA(hWnd, className, sizeof(className))) {
			if (strcmp("XLMAIN", className) == 0) handles.Push(hWnd);
		}

		return TRUE;
	}

	void RunMacro(Excel::_ApplicationPtr& app, const wchar_t * wbName, const wchar_t * macro, HRESULT okFailure) {

		if (!macro) return;

		auto macroName = xFormat(L"'%s'!%s", wbName, macro);
		try {
			app->Run(macroName.GetDataPtr());
		}
		catch (const _com_error & e) {
			if (e.Error() != okFailure) {
				throw e;
			}
		}
	}

	template <class T>
	bool RemoveOrAddExcelAddIn_Workbooks_Core(
		Excel::_ApplicationPtr& app,
		bool remove,
		const wchar_t * add_addinPath,
		aVect<aVect<wchar_t>> & addremove_workbooks,
		aVect<wchar_t> remove_addinName,
		const wchar_t * remove_VBAmacroToRunOnce,
		HRESULT remove_VBAmacroToRunOnce_okFailure,
		T&& log,
		const wchar_t * dialogTitle,
		bool * foundAddin)
	{

		log(xFormat(L"=> RemoveOrAddExcelAddIn_Workbooks_Core: remove=%d\n", remove));

		auto&& workbooks = app->Workbooks;

		bool macroHasRun = false;

		aVect<wchar_t> remove_addinName_with_ext = remove_addinName;

		for (auto&& c : Reverse(remove_addinName)) {
			if (c == '.') {
				c = 0;
				break;
			}
		}

		aVect<aVect<wchar_t>> openWorkbooks;

		log(xFormat(L"app->AddIns2->Count = %d\n", app->AddIns2->Count));

		for (int i = 1; i <= app->AddIns2->Count; ++i) {
			auto addin_i = app->AddIns2->Item[i];
			if (addin_i->IsOpen) openWorkbooks.Push(addin_i->Name.GetBSTR());
		}

		log(xFormat(L"workbooks->Count = %d\n", workbooks->Count));

		for (int i = 1; i <= workbooks->Count; ++i) {
			auto&& wb = workbooks->Item[i];
			aVect<wchar_t> wbName = wb->Name.GetBSTR();
			openWorkbooks.Push(wbName);
		}

		log(xFormat(L"%d open workbook(s)\n", openWorkbooks.Count()));

		for (auto&& wbName : openWorkbooks) {

			log(xFormat(L"Checking workbook \"%s\".\n", wbName));

			//log(xFormat(L"Fixing links in Excel instance %d...\n", addremove_info.openedApps.Index(openedApp)));
			DWORD PID;
			GetWindowThreadProcessId((HWND)app->Hwnd, &PID);
			log(xFormat(L"Fixing links in Excel instance PID=%d...\n", PID));
			FixWorkbooks(app, add_addinPath, log);
			log(L"Done.\n");

			wchar_t * wbExt = nullptr;
			for (auto&& c : Reverse(wbName)) {
				if (c == L'.') {
					wbExt = &c;
					break;
				}
			}

			if (wbExt && _wcsicmp(wbExt, L".dll") == 0) continue;
			if (wbExt && _wcsicmp(wbExt, L".xll") == 0) continue;

			//_com_ptr_t<_com_IIID<Excel::_Workbook, &__uuidof(Excel::_Workbook)>> wb;
			Excel::_WorkbookPtr wb;

			try {
				wb = workbooks->Item[wbName.GetDataPtr()];
			}
			catch (const _com_error & e) {
				if (e.Error() == 0x8002000b) {
					log(xFormat(L"Unable to get VBProject for %s.\n", wbName));
					continue;
				} else {
					throw e;
				}
			}

			//auto&& refs = wb->VBProject->References;
			VBIDE::_ReferencesPtr refs;
			
			try {
				refs = wb->VBProject->References;
			}
			catch (const _com_error & e) {
				if (e.Error() == 0x800A03EC) {
					log(xFormat(L"Unable to access VBProject for %s, Excel option \"VB project trusted access\" is not probably enabled.\n", wbName));
					throw std::runtime_error("\n\nUnable to access VB project.\n\nPlease ensure Excel option \"VB project trusted access\" is enabled.");
				} else {
					log(xFormat(L"Unable to access VBProject for %s.\n", wbName));
					throw e;
				}
			}

			if (remove) {
				for (int j = 1; j <= refs->Count; ++j) {
					auto refName = refs->Item(j)->Name.GetBSTR();
					if (refName && remove_addinName && _wcsicmp(refName, remove_addinName) == 0) {

						*foundAddin = true;

						log(xFormat(L"Removing ref to %s in workbook %s...\n", remove_addinName, wbName));
						if (!macroHasRun && remove_VBAmacroToRunOnce) {
							log(xFormat(L"    Running macro %s...\n", remove_VBAmacroToRunOnce));
							RunMacro(app, wbName, remove_VBAmacroToRunOnce, remove_VBAmacroToRunOnce_okFailure);
							log(L"    Done.\n");
							macroHasRun = true;
						}

						//aVect<wchar_t> fullPath = refs->Item(j)->FullPath.GetBSTR();

						refs->Remove(refs->Item(j));
						
						//aVect<wchar_t> name_and_ext;
						//SplitPathW(fullPath, nullptr, nullptr, &name_and_ext, nullptr);

						//if (name_and_ext && remove_addinName_with_ext && _wcsicmp(name_and_ext, remove_addinName_with_ext) == 0) {
							addremove_workbooks.Push(wbName);
						//}
						
						log(L"Done.\n");
					}
				}
			} else {
				for (auto&& adwb : Reverse(addremove_workbooks)) {
					if (adwb && wbName && _wcsicmp(adwb, wbName) == 0) {
						log(xFormat(L"Adding ref to %s in workbook %s...\n", add_addinPath, adwb));
						refs->AddFromFile(add_addinPath);
						log(L"Done.\n");
						addremove_workbooks.RemoveElement(adwb);
						for (int j = 1; j <= wb->Worksheets->Count; ++j) {
							//((Excel::_WorksheetPtr)wb->Worksheets->Item[j])->EnableCalculation = false;
							//((Excel::_WorksheetPtr)wb->Worksheets->Item[j])->EnableCalculation = true;
							((Excel::_WorksheetPtr)wb->Worksheets->Item[j])->Calculate();
						}
					}
				}
			}
		}

		log(xFormat(L"<= RemoveOrAddExcelAddIn_Workbooks_Core\n"));
		 
		return macroHasRun;
	}

	void ExcelConnectionFromHandle(
		HWND h,
		const wchar_t * dialogTitle,
		ExcelAddInManager & addremove_info,
		int& createdProcessID)
	{

		auto hDesk = FindWindowExA(h, NULL, "XLDESK", NULL);
		auto hXl = FindWindowExA(hDesk, NULL, "EXCEL7", NULL);

		if (!hXl) return;

		IDispatchPtr xlPtr;
		auto ridd = __uuidof(IDispatch);
		auto hr = AccessibleObjectFromWindow(hXl, OBJID_NATIVEOM, ridd, (void**)&xlPtr);

		if (FAILED(hr)) throw std::runtime_error("Unable to connect to Excel");

		BSTR name = L"Application";
		DISPID dispid;
		DISPPARAMS dispparamsNoArgs = { NULL, NULL, 0, 0 };
		VARIANT xlApp;

		VariantInit(&xlApp);

		PerformanceChronometer chrono;

		for (;;) {
			hr = xlPtr->GetIDsOfNames(IID_NULL, &name, 1, NULL, &dispid);
			if (hr == RPC_E_CALL_REJECTED) {
				if (chrono.GetSeconds() > 10) {
					//throw std::runtime_error("Excel is busy");
					BusyExcelPrompt(dialogTitle);
					chrono.Restart();
				}
				continue;
			}
			if (FAILED(hr)) throw std::runtime_error("Unable to connect to Excel");
			break;
		}

		chrono.Restart();

		for (;;) {
			hr = xlPtr->Invoke(dispid, IID_NULL, NULL, DISPATCH_PROPERTYGET, &dispparamsNoArgs, &xlApp, NULL, NULL);
			if (hr == RPC_E_CALL_REJECTED) {
				if (chrono.GetSeconds() > 10) {
					//throw std::runtime_error("Excel is busy");
					BusyExcelPrompt(dialogTitle);
					chrono.Restart();
				}
				continue;
			}
			if (FAILED(hr)) throw;
			break;
		}

		if (xlApp.vt != VT_DISPATCH) throw std::runtime_error("Unable to connect to Excel");

		auto&& openedApp = addremove_info.openedApps.Push().Last();
		auto&& app = openedApp.app;

		app.Attach((Excel::_ApplicationPtr)xlApp.pdispVal, true);
		openedApp.hWnd = (HWND)app->Hwnd;
		DWORD processId;

		if (GetWindowThreadProcessId((HWND)app->Hwnd, &processId)) {
			if (processId == createdProcessID) {
				addremove_info.createdApp = app;
			}
		}

		VariantClear(&xlApp);
	}

	//void ExcelConnection(
	//	const wchar_t * dialogTitle,
	//	ExcelAddInManager & addremove_info,
	//	int& createdProcessID)
	//{
	//	aVect<HWND> handles;
	//	EnumWindows(EnumFuncFindExcel, (LPARAM)&handles);

	//	for (auto&& h : handles) {
	//		ExcelConnectionFromHandle(h, dialogTitle, addremove_info, createdProcessID);
	//	}
	//}

	aVect<wchar_t> GetUnusedFileName(const wchar_t * fileName) {

		aVect<wchar_t> unusedFileName;

		for (int i = 0; i < 10000; ++i) {
			unusedFileName = fileName;
			unusedFileName.Append(L"_old%d", i);
			if (!FileExists(unusedFileName)) break;
			unusedFileName.Erase();
		}

		return unusedFileName;
	}

	template <class T>
	void RemoveOrAddExcelAddIn_Workbooks(
		RemoveOrAddAddinInfo& openedApp,
		bool remove,
		const wchar_t * add_addinPath,
		aVect<wchar_t> remove_addinName,
		const wchar_t * remove_VBAmacroToRunOnce,
		HRESULT remove_VBAmacroToRunOnce_okFailure,
		T&& log,
		const wchar_t * dialogTitle = nullptr)
	{

		PerformanceChronometer chrono;

		for (;;) {
			if (chrono.GetSeconds() > 10) {
				//throw std::runtime_error("Excel is busy");
				log(L"Excel is busy\n");
				BusyExcelPrompt(dialogTitle);
				chrono.Restart();
			}
			try {
				bool foundAddin;
				bool macroHasRun = RemoveOrAddExcelAddIn_Workbooks_Core(openedApp.app, remove, add_addinPath, openedApp.workbooks,
					remove_addinName, remove_VBAmacroToRunOnce, remove_VBAmacroToRunOnce_okFailure, log, dialogTitle, &foundAddin);

				if (macroHasRun) openedApp.macroHasRun = true;
				if (foundAddin) openedApp.foundAddin = true;
				break;
			}
			catch (const _com_error & e) {
				if (e.Error() != RPC_E_CALL_REJECTED) throw e;
			}
		}
	}

	template <class T>
	void BackupRename(const wchar_t * fileName, T&& log) {

		if (!FileExists(fileName)) {
			log(xFormat(L"BackupRename: file doesn't exist (\"%s\").\n", fileName));
			return;
		}

		auto newFileName = GetUnusedFileName(fileName);

		if (!newFileName) throw std::runtime_error(xFormat("Unable to find unused file name for \"%S\"", fileName));
		if (!IsFileWritable(newFileName)) throw std::runtime_error(xFormat("Unable to create file \"%S\"", newFileName));

		log(xFormat(L"    Renaming \"%s\" to \"%s\".\n", fileName, newFileName));
		auto r = MoveFileW(fileName, newFileName);
		if (!r) throw std::runtime_error(xFormat("MoveFileW failed:%S", GetWin32ErrorDescription(r)));
		log(L"    Done.\n");
	}

	template <class T>
	void FixWorkbook(Excel::_WorkbookPtr & wb, const wchar_t * newPath, T&& log) {

		auto sources = wb->LinkSources();

		if (sources.vt == VT_EMPTY) return;
		aVect<wchar_t> sName, fName;

		if (sources.vt == (VT_ARRAY | VT_VARIANT)) {
			auto I = sources.parray->rgsabound[0].cElements;
			auto ptr = (VARIANT*)sources.parray->pvData;
			for (ULONG i = 0; i < I; ++i) {
				if (ptr[i].vt != VT_BSTR) MY_ERROR("BSTR expected");
				auto str = ptr[i].bstrVal;
				if (str && newPath && _wcsicmp(str, newPath) != 0) {
					SplitPathW(str, nullptr, nullptr, &sName, nullptr);
					SplitPathW(newPath, nullptr, nullptr, &fName, nullptr);
					if (sName && fName && _wcsicmp(sName, fName) == 0) {
						log(xFormat(L"        Changing link from %s to %s...\n", str, newPath));
						wb->ChangeLink(str, newPath, Excel::XlLinkType::xlLinkTypeExcelLinks);
						log(L"        Done.\n");
						break;
					}
				}
			}
		}
		else {
			MY_ERROR("SAFEARRAY expected");
		}
	}

	template <class T>
	void FixWorkbooks(Excel::_ApplicationPtr & app, const wchar_t * newPath, T&& log) {

		auto I = app->Workbooks->Count;

		for (long i = 1; i <= I; ++i) {
			log(xFormat(L"    Fixing links in Workbook %s...\n", app->Workbooks->Item[i]->Name.GetBSTR()));
			FixWorkbook(app->Workbooks->Item[i], newPath, log);
			log(L"    Done.\n");
		}

	}

	template <class T>
	struct HackThreadProcParams {

		HackThreadProcParams(T logFunc) : logFunc(logFunc), success(NULL, false, false, NULL) {}

		WinEvent success;
		int addinCount;
		HWND hWndApp;
		T logFunc;
	};

	void PressKey(HWND hWnd, WPARAM keyCode) {
		SendMessageW(hWnd, WM_KEYDOWN, keyCode, 0);
		Sleep(10);
		SendMessageW(hWnd, WM_KEYUP, keyCode, 0);
		Sleep(10);
	}

	template <class T>
	DWORD WINAPI HackThreadProc(LPVOID lpThreadParameter) {

		auto params = (HackThreadProcParams<T>*)lpThreadParameter;
		auto addinCount = params->addinCount;
		auto hWndApp = params->hWndApp;
		auto&& log = params->logFunc;
			
		for (;;) {

			if (!SetForegroundWindow(hWndApp)) {
				if (IsDebuggerPresent()) __debugbreak();
				log(L"    SetForegroundWindow to Excel window failed.\n");
			}

			if (params->success.IsSignaled()) return 0;

			auto hWnd = GetLastActivePopup(hWndApp);
			char className[50];

			if (RealGetWindowClassA(hWnd, className, sizeof(className))) {
				//printf("Foreground window: %s\n", className);
				if (_strcmpi(className, "XLMAIN") != 0) {

					for (int i = 0; i < addinCount; ++i) {
						PressKey(hWnd, VK_DOWN);
					}
					for (int i = 0; i < addinCount; ++i) {
						PressKey(hWnd, VK_UP);
					}
					PressKey(hWnd, VK_RETURN);

					if (params->success.AutoWaitFor(1000)) return 0;
				}
			}
		}
	}

	template <class T>
	void SaveWorkbook(Excel::_ApplicationPtr & app, const wchar_t * name, T&& log) {

		log(xFormat(L"Trying to save Worbook \"%s\".\n", name));

		try {
			auto wb = app->Workbooks->GetItem(name);
			if (wb->ReadOnly == VARIANT_TRUE) {
				log(xFormat(L"Worbook \"%s\" is opened as read-only, skipping save.\n", name));
			} else {
				wb->Save();
				log(L"Done.\n");
			}
		}
		catch (const _com_error & e) {
			if (e.Error() == 0x8002000b) {
				log(xFormat(L"Addin %s not currently open.\n", name));
			} else {
				throw e;
			}
		}
	}

	template <class T>
	void RemoveOrAddExcelAddIn_Application(
		RemoveOrAddAddinInfo& openedApp,
		bool remove,
		const wchar_t * add_addinPath,
		bool add_install,
		const wchar_t * remove_addinName,
		const wchar_t * remove_VBAmacroToRunOnce,
		HRESULT remove_VBAmacroToRunOnce_okFailure,
		T&& log,
		ExcelAddInManager & addremove_info,
		const wchar_t * dialogTitle)
	{

		log(xFormat(L"=> RemoveOrAddExcelAddIn_Application: remove=%d\n", remove));

		auto&& app = openedApp.app;

		PerformanceChronometer chrono;

		for (;;) {
			if (chrono.GetSeconds() > 10) {
				//throw std::runtime_error("Excel is busy");
				log(L"Excel is busy\n");
				BusyExcelPrompt(dialogTitle);
				chrono.Restart();
			}
			try {
				if (remove) {

					log(xFormat(L"app->AddIns->Count=%d\n", app->AddIns->Count));

					for (int i = 1; i <= app->AddIns->Count; ++i) {
						auto&& ref = app->AddIns->Item[i];
						aVect<wchar_t> refName = ref->Name.GetBSTR();

						if (refName && remove_addinName && _wcsicmp(refName, remove_addinName) == 0) {
							openedApp.foundAddin = true;
							if (ref->Installed) {
								SaveWorkbook(app, remove_addinName, log);
								log(xFormat(L"Setting \"Installed\" to false on addin %s\n", remove_addinName));
								ref->Installed = false;
								break;
							}
						}
					}

					log(xFormat(L"Trying to close addin %s...\n", remove_addinName));

					try {
						auto wb = app->Workbooks->GetItem(remove_addinName);
						if (remove_VBAmacroToRunOnce && !openedApp.macroHasRun) RunMacro(app, remove_addinName, remove_VBAmacroToRunOnce, remove_VBAmacroToRunOnce_okFailure);

						try {
							wb->Close(true);
							log(L"Done.\n");
						}
						catch (const _com_error & e) {
							if (e.Error() == 0x800a03ec) {
								log(xFormat(L"    Addin %s is \"referenced by another workbook and cannot be closed\" (Excel bug).\n", remove_addinName));
							} else {
								log(xFormat(L"    Addin %s cannot be closed (Excel bug?).\n", remove_addinName));
							}
							throw std::runtime_error(xFormat("Unable to close \"%S\"", remove_addinName));
						}
					}
					catch (const _com_error & e) {
						if (e.Error() == 0x8002000b) {
							log(xFormat(L"Addin %s not currently open.\n", remove_addinName));
						} else {
							throw e;
						}
					}
				} else {
					if (add_install) {

						log(xFormat(L"Adding addin %s...\n", add_addinPath));

						auto ref = app->AddIns->Add(add_addinPath, false);

						aVect<wchar_t> curName = ref->FullName.GetBSTR();

						if (_wcsicmp(curName, add_addinPath) == 0) {
							log(L"Done.\n");
						} else {

							ref->Installed = false;

							log(xFormat(L"Path \"%s\" must be changed to \"%s\"...\n", curName, add_addinPath));

							//aVect<wchar_t> onlyPath;;
							//SplitPathW(curName, nullptr, &onlyPath, nullptr, nullptr);

							//WCHAR tmpFileName[MAX_PATH];
							//auto r = GetTempFileNameW(onlyPath, L"", 0, tmpFileName);
							//if (!r) throw std::runtime_error(xFormat("GetTempFileNameW failed:%s", GetErrorDescription(r)));

							//DeleteFileW(tmpFileName);

							//auto tmpFileName = GetUnusedFileName(curName);

							//if (!tmpFileName) throw std::runtime_error(xFormat("Unable to find unused file name for \"%s\"", curName));

							//if (FileExists(curName)) {
							//	log(xFormat(L"    Renaming \"%s\" to \"%s\".\n", curName, tmpFileName));
							//	auto r = MoveFileW(curName, tmpFileName);
							//	if (!r) throw std::runtime_error(xFormat("MoveFileW failed:%s", GetErrorDescription(r)));
							//	log(L"    Done.\n");
							//}

							BackupRename(curName, log);

							app->PutDisplayAlerts(0, VARIANT_FALSE);

							log(L"    Trying \"force Excel to forget about old path\" method...\n");

							for (;;) {
								int addinCount = app->AddIns->Count;

								//auto GoUpandDown = xFormat(L"{Up %d}{DOWN %d}{Up %d}~", addinCount, addinCount, addinCount);
								//app->SendKeys(GoUpandDown.GetDataPtr(), false);
								//Sleep(1000);

								HackThreadProcParams<T> params(log);
								params.addinCount = addinCount;
								params.hWndApp = (HWND)app->Hwnd;
								
								auto h = CreateThread(NULL, 0, HackThreadProc<T>, (LPVOID)&params, 0, NULL);
								app->Dialogs->Item[Excel::xlDialogAddinManager]->Show();
								params.success.Set();
								if (WaitForSingleObject(h, INFINITE) != WAIT_OBJECT_0) throw std::runtime_error("Unable to send commands to Excel addin dialog");
								CloseHandle(h);

								//if (addinCount == app->AddIns->Count) break;
								break;
							}

							app->PutDisplayAlerts(0, VARIANT_TRUE);

							log(L"    Done.\n");

							//log(xFormat(L"    Renaming \"%s\" to back \"%s\".\n", tmpFileName, curName));
							//r = MoveFileW(tmpFileName, curName);
							//if (!r) throw std::runtime_error(xFormat("MoveFileW failed:%s", GetErrorDescription(r)));
							//log(L"    Done.\n");

							log(xFormat(L"    Adding addin %s again...\n", add_addinPath));
							ref = app->AddIns->Add(add_addinPath, false);
							curName = ref->FullName.GetBSTR();

							if (_wcsicmp(curName, add_addinPath) == 0) {
								log(L"    Now the path is right.\n");
							} else {
								if (IsDebuggerPresent()) __debugbreak();
								throw std::runtime_error("Unable to set the correct addin path");
							}

							log(L"Done.\n");
						}

						ref->Installed = false;

						log(xFormat(L"Setting \"Installed\" to true on addin %s.\n", add_addinPath));

						ref->Installed = true;

						//log(xFormat(L"Fixing links in Excel instance %d...\n", addremove_info.openedApps.Index(openedApp)));
						//FixWorkbooks(app, add_addinPath, log);
						//log(L"Done.\n");
					}
				}
				break;
			}
			catch (const _com_error & e) {
				if (e.Error() != RPC_E_CALL_REJECTED) throw e;
			}
		}

		log(xFormat(L"<= RemoveOrAddExcelAddIn_Application\n"));
	}

	struct KeyReleaser {
		HKEY hKey;
		bool initialized = false;

		~KeyReleaser() {
			if (this->initialized) {
				auto status = RegCloseKey(this->hKey);
				if (status != ERROR_SUCCESS) MY_ERROR("Unable to close registry key.");
			}
		}
	};

	template <class T>
	void RemoveOrAddExcelAddIn(
		bool remove,
		const wchar_t * add_addinPath,
		bool add_install,
		ExcelAddInManager & addremove_info,
		const wchar_t * remove_addinName,
		const wchar_t * remove_VBAmacroToRunOnce,
		HRESULT remove_VBAmacroToRunOnce_okFailure,
		T&& log,
		const wchar_t * dialogTitle = nullptr)
	{

		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

		if (remove) {

			int createdProcessID = -1;

			ExcelConnection(dialogTitle, addremove_info, createdProcessID);

			//if (!addremove_info.openedApps) {

			//	{
			//		log(L"Excel instance not found, launching new instance...\n");

			//		Excel::_ApplicationPtr app;

			//		app.CreateInstance(__uuidof(Excel::Application));

			//		auto commandLine = xFormat(L"%s\\Excel.exe", app->Path.GetBSTR());

			//		WinProcess excelProcess;
			//		excelProcess.CreateFree(NULL, commandLine, NULL, NULL, FALSE, 0, NULL, NULL);
			//		createdProcessID = excelProcess.dwProcessId;
			//		if (WaitForInputIdle(excelProcess.GetHandle(), INFINITE) != 0) {
			//			throw std::runtime_error("Unable to wait for new Excel process");
			//		}

			//		log(L"Done.\n");
			//	}

			//	ExcelConnection(dialogTitle, addremove_info, createdProcessID);

			//	if (!addremove_info.openedApps) {
			//		throw std::runtime_error("Unable to start/find Excel process");
			//	}
			//}
		}

		log(xFormat(L"%d Excel instance(s) running.\n", addremove_info.openedApps.Count()));

		for (auto&& openedApp : Reverse(addremove_info.openedApps)) {
			
			DWORD PID;
			GetWindowThreadProcessId((HWND)openedApp.app->Hwnd, &PID);

			auto func_Workbooks = [&] {

				if (remove) {
					log(xFormat(L"\nRemoving \"%s\" from Excel instance %d (PID %d) workbooks.\n\n", remove_addinName, addremove_info.openedApps.Index(openedApp), PID));
				} else {
					log(xFormat(L"\nAdding \"%s\" to Excel instance %d (PID %d) workbooks.\n\n", add_addinPath, addremove_info.openedApps.Index(openedApp), PID));
				}

				RemoveOrAddExcelAddIn_Workbooks(openedApp, remove, add_addinPath, remove_addinName,
					remove_VBAmacroToRunOnce, remove_VBAmacroToRunOnce_okFailure, log, dialogTitle);

				if (openedApp.foundAddin) addremove_info.foundAddin = true;
			};

			auto func_Application = [&] {

				if (remove) {
					log(xFormat(L"\nRemoving \"%s\" from Excel instance %d (PID %d) application.\n\n", remove_addinName, addremove_info.openedApps.Index(openedApp), PID));
				} else {
					log(xFormat(L"\nAdding \"%s\" to Excel instance %d (PID %d) application.\n\n", add_addinPath, addremove_info.openedApps.Index(openedApp), PID));
				}

				RemoveOrAddExcelAddIn_Application(openedApp, remove, add_addinPath, add_install, remove_addinName,
					remove_VBAmacroToRunOnce, remove_VBAmacroToRunOnce_okFailure, log, addremove_info, dialogTitle);
				if (openedApp.foundAddin) addremove_info.foundAddin = true;
			};

			if (remove) {
				log(xFormat(L"\nRemoving \"%s\" from Excel instance %d (PID %d).\n\n", remove_addinName, addremove_info.openedApps.Index(openedApp), PID));
				func_Workbooks();
				func_Application();
			} else {
				log(xFormat(L"\nAdding \"%s\" to Excel instance %d (PID %d).\n\n", add_addinPath, addremove_info.openedApps.Index(openedApp), PID));
				func_Application();
				func_Workbooks();
				addremove_info.openedApps.RemoveElement(openedApp);
			}
		}

		if (!remove) {

			log(L"Fixing Excel registry entries...\n");

			aVect<wchar_t> version;

			{
				Excel::_ApplicationPtr app;
				auto hr = app.CreateInstance(__uuidof(Excel::Application));
				if (FAILED(hr)) {
					throw std::runtime_error(xFormat("Unable to create excel Instance, hr = 0x%p", hr));
				}
				version = app->Version.GetBSTR();
			}

			log(xFormat(L"    Excel version is \"%s\"\n", version));
			auto keyBase = xFormat(L"Software\\Microsoft\\Office\\%s\\Excel\\Options", version);
			aVect<wchar_t> key;
			for (int i = 0;; ++i) {
				key = xFormat("OPEN");
				if (i > 0) key.Append(L"%d", i);
				if (auto value = GetRegistry(HKEY_CURRENT_USER, keyBase, key)) {
					aVect<wchar_t> regName, addinName;
					auto missingQuote = value && value.First() != L'"';
					value.Trim<L'"'>();
					SplitPathW(value, nullptr, nullptr, &regName, nullptr);
					SplitPathW(add_addinPath, nullptr, nullptr, &addinName, nullptr);
					if (regName && addinName && _wcsicmp(regName, addinName) == 0) {
						log(xFormat(L"    found \"%s\" at key \"%s\\%s\".\n", addinName, keyBase, key));
						if (value && add_addinPath && _wcsicmp(value, add_addinPath) == 0) {
							log(xFormat(L"    path is already right.\n", addinName, keyBase, key));
							if (!missingQuote) key.Erase();
						} else {
							log(xFormat(L"    path must be changed from \"%s\" to \"%s\".\n", value, add_addinPath));
							BackupRename(value, log);
						}
						break;
					}
				} else {
					log(xFormat(L"    key \"%s\\%s\" not found.\n", keyBase, key));
					break;
				}
			}

			if (key) {
				log(xFormat(L"    Writing \"%s\" to key \"%s\\%s\".\n", add_addinPath, keyBase, key));
				auto toWrite = xFormat(L"\"%s\"", add_addinPath);
				if (!WriteRegistry(HKEY_CURRENT_USER, keyBase, key, toWrite)) {
					throw std::runtime_error(xFormat("Unable to write to registry key \"%S\\%S\"", keyBase, key));
				}
			}

			try {
				keyBase = xFormat(L"Software\\Microsoft\\Office\\%s\\Excel\\Add-in Manager", version);

				HKEY hKey;
				if (ERROR_SUCCESS != RegOpenKeyExW(HKEY_CURRENT_USER, keyBase, 0, KEY_ALL_ACCESS, &hKey)) {
					throw std::runtime_error(xFormat("Unable to open registry key \"%S\".", keyBase));
				}

				KeyReleaser guard;
				guard.hKey = hKey;
				guard.initialized = true;

				DWORD lpcMaxValueNameLen;
				if (ERROR_SUCCESS != RegQueryInfoKeyW(hKey, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &lpcMaxValueNameLen, NULL, NULL, NULL)) {
					throw std::runtime_error(xFormat("Unable to get registry key \"%S\" information.", keyBase));
				}

				aVect<wchar_t> buffer(lpcMaxValueNameLen + 1);

				for (DWORD index = 0; ; ++index) {

					for (;;) {
						auto bufSize = (DWORD)buffer.Count();

						auto status = RegEnumValueW(hKey, index, buffer, &bufSize, NULL, NULL, NULL, NULL);
						if (status == ERROR_NO_MORE_ITEMS) {
							buffer.Erase();
							break;
						}
						else if (status == ERROR_MORE_DATA) {
							buffer.Redim(2 * buffer.Count());
							continue;
						}
						if (status == ERROR_SUCCESS) {
							break;
						}

						throw std::runtime_error(xFormat("Error while enumerating registry key \"%S\".", keyBase));
					}

					if (!buffer) break;

					aVect<wchar_t> name, addinName;
					SplitPathW(buffer, nullptr, nullptr, &name, nullptr);
					SplitPathW(add_addinPath, nullptr, nullptr, &addinName, nullptr);

					if (name && addinName && _wcsicmp(name, addinName) == 0) {
						log(xFormat(L"    Deleting a reference to \"%s\" in key \"%s\"...\n", addinName, keyBase));
						auto status = RegDeleteValueW(hKey, buffer);
						if (ERROR_SUCCESS != status) {
							throw std::runtime_error(xFormat("Error while enumerating registry key \"%S\".", keyBase));
						}
						log(L"    Done.\n");
						break;
					}
				}

				log(L"Done.\n");
			}
			catch (const std::runtime_error & e) {
				log(xFormat(L"    Error occured while inspecting key Software\\Microsoft\\Office\\%s\\Excel\\Add-in Manager:\n", version));
				log(xFormat(L"    %S\n", e.what()));
				log(L"    Error is not critical, installation can carry on.\n");
			}
		}
	}
}

template<class T>
ExcelAddInManager RemoveExcelAddIn(
	const wchar_t * addinName,
	T&& log,
	const wchar_t * VBAmacroToRunOnce = nullptr,
	HRESULT VBAmacroToRunOnce_okFailure = 0, 
	const wchar_t * dialogTitle = nullptr)
{
	ExcelAddInManager info;
	RemoveOrAddExcelAddIn(true, nullptr, false, info, addinName, VBAmacroToRunOnce, VBAmacroToRunOnce_okFailure, log, dialogTitle);
	return std::move(info);
}

template<class T>
void AddExcelAddIn(
	ExcelAddInManager & info, 
	const wchar_t * addinPath, 
	bool install,
	T&& log,
	const wchar_t * dialogTitle = nullptr)
{
	RemoveOrAddExcelAddIn(false, addinPath, install, info, nullptr, nullptr, 0, log);
}

void ExcelConnection(
	const wchar_t * dialogTitle,
	ExcelAddInManager & addremove_info,
	int& createdProcessID)
{
	aVect<HWND> handles;
	EnumWindows(EnumFuncFindExcel, (LPARAM)&handles);

	for (auto&& h : handles) {
		ExcelConnectionFromHandle(h, dialogTitle, addremove_info, createdProcessID);
	}
}

void ExcelAddInManager::Remove(const wchar_t * name, const wchar_t * VBA_macroToRunOnce, HRESULT VBA_macroToRunOnce_okFailure) {
	auto pLogFile = this->pLogFile;
	auto log = [pLogFile](const wchar_t * str) {if (pLogFile) pLogFile->Printf(L"%s", str); };
	auto newAddinMgr = RemoveExcelAddIn(name, log, VBA_macroToRunOnce, VBA_macroToRunOnce_okFailure, this->dialogTitle);

	for (auto&& oa : this->openedApps) {
		//auto hInstance = oa.app->Hinstance;    /* Hinstance is not globally unique ?!?! yep */
		for (auto&& noa : newAddinMgr.openedApps) {
			//if (hInstance == noa.app->Hinstance) {
			if (oa.hWnd == noa.hWnd) {
				for (auto&& w : Reverse(oa.workbooks)) {
					noa.workbooks.Insert(0, w);
					oa.workbooks.RemoveElement(w);
				}
			}
		}
	}

	*this = std::move(newAddinMgr);
}

void ExcelAddInManager::Add(const wchar_t * path, bool install) {
	auto pLogFile = this->pLogFile;
	auto log = [pLogFile](const wchar_t * str) {if (pLogFile) pLogFile->Printf(L"%s", str); };
	AddExcelAddIn(*this, path, install, log, this->dialogTitle);
}

ExcelAddInManager& ExcelAddInManager::operator=(ExcelAddInManager&& that) {

	this->createdApp = std::move(that.createdApp);
	that.createdApp.Detach();
	this->openedApps = std::move(that.openedApps);
	return *this;
}

ExcelAddInManager::~ExcelAddInManager() {
	if (this->createdApp) this->createdApp->Quit();
}
