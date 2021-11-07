
//-----LIBRARIES----------
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <fstream>
#include "resource.h"

//-----dEFINITIONS----------
#define WINDOW_CLASS_NAME L"MultiThreaded Loader Tool"
const unsigned int _kuiWINDOWWIDTH = 1200;
const unsigned int _kuiWINDOWHEIGHT = 1200;
#define MAX_FILES_TO_OPEN 50
#define MAX_CHARACTERS_IN_FILENAME 25

//-----GLOBAL VARIABLES----------
bool g_bIsFileLoaded = false;

LPCWSTR ImageLoadTime;
LPCWSTR errorMessage;

std::vector<std::thread> threads;
std::vector<HBITMAP> g_vecLoadedImages;
std::vector<std::wstring> g_vecImageFileNames;

std::mutex g_lock;

int userDefinedThreadCount = 1;


// Function loads path names of images into a vector.
// Returns TRUE or FALSE depending on successfull execution   
bool ChooseImageFilesToLoad(HWND _hwnd)
{
	OPENFILENAME ofn;
	SecureZeroMemory(&ofn, sizeof(OPENFILENAME)); 
	wchar_t wsFileNames[MAX_FILES_TO_OPEN * MAX_CHARACTERS_IN_FILENAME + MAX_PATH];
	wchar_t _wsPathName[MAX_PATH + 1];
	wchar_t _wstempFile[MAX_PATH + MAX_CHARACTERS_IN_FILENAME]; 
	wchar_t _wsFileToOpen[MAX_PATH + MAX_CHARACTERS_IN_FILENAME];
	ZeroMemory(wsFileNames, sizeof(wsFileNames));
	ZeroMemory(_wsPathName, sizeof(_wsPathName));
	ZeroMemory(_wstempFile, sizeof(_wstempFile));

	
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = _hwnd;
	ofn.lpstrFile = wsFileNames;
	ofn.nMaxFile = MAX_FILES_TO_OPEN * 20 + MAX_PATH; 
	ofn.lpstrFile[0] = '\0';
	ofn.lpstrFilter = L"Bitmap Images(.bmp)\0*.bmp\0"; 
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;

	
	if (GetOpenFileName(&ofn) != 0) 
	{
		wcsncpy_s(_wsPathName, wsFileNames, ofn.nFileOffset);
		int i = ofn.nFileOffset;
		int j = 0;

		while (true)
		{
			if (*(wsFileNames + i) == '\0')
			{
				_wstempFile[j] = *(wsFileNames + i);
				wcscpy_s(_wsFileToOpen, _wsPathName);
				wcscat_s(_wsFileToOpen, L"\\");
				wcscat_s(_wsFileToOpen, _wstempFile);
				g_vecImageFileNames.push_back(_wsFileToOpen);
				j = 0;
			}
			else
			{
				_wstempFile[j] = *(wsFileNames + i);
				j++;
			}
			if (*(wsFileNames + i) == '\0' && *(wsFileNames + i + 1) == '\0')
			{
				break;
			}
			else
			{
				i++;
			}
		}

		g_bIsFileLoaded = true;
		return true;
	}
	else 
	{
		return false;
	}
}


// Function takes the vector of image path names and a starting index and ending index.
// Loops through a section of the vector of images determined by the start index and end index.
// Uses a mutex lock to ensure that there are no conflicts between threads and type casts the result as a HBITMAP object.
// Stores each HBITMAP object into a vector also protected by mutex locking.
void loadImageRange(std::vector<std::wstring> files, int start, int end)
{
	for (int i = start; i < end; i++)
	{
		g_lock.lock();
		HBITMAP loadedImage = (HBITMAP)LoadImageW(NULL, files[i].c_str(), IMAGE_BITMAP, 100, 100, LR_LOADFROMFILE);
		g_lock.unlock();

		bool loadFail = loadedImage == NULL;
		if (loadFail)
			throw "Unable to load image.";

		g_lock.lock();
		g_vecLoadedImages.push_back(loadedImage);
		g_lock.unlock();
	}
}

// Loads a single image from the path given and stores it in a HBITMAP vector
void loadImage(std::wstring file, int start, int end)
{
	
	g_lock.lock();
	HBITMAP loadedImage = (HBITMAP)LoadImageW(NULL, file.c_str(), IMAGE_BITMAP, 100, 100, LR_LOADFROMFILE);
	g_lock.unlock();

	bool loadFail = loadedImage == NULL;
	if (loadFail)
		throw "Unable to load image.";

	g_lock.lock();
	g_vecLoadedImages.push_back(loadedImage);
	g_lock.unlock();
	
}

LRESULT CALLBACK WindowProc(HWND _hwnd, UINT _uiMsg, WPARAM _wparam, LPARAM _lparam)
{
	PAINTSTRUCT ps;
	HDC _hWindowDC;
	//RECT rect;
	switch (_uiMsg)
	{
	case WM_KEYDOWN:
	{
		switch (_wparam)
		{
		case VK_ESCAPE:
		{
			SendMessage(_hwnd, WM_CLOSE, 0, 0);
			return(0);
		}
		break;
		default:
			break;
		}
	}
	break;
	case WM_PAINT:
	{

		_hWindowDC = BeginPaint(_hwnd, &ps);

		EndPaint(_hwnd, &ps);
		return (0);
	}
	break;
	case WM_COMMAND:
	{
		switch (LOWORD(_wparam))
		{

		case ID_FILE_LOADIMAGE:
		{
			if (ChooseImageFilesToLoad(_hwnd))
			{
				// Set the start time
				auto ImageLoadTimeStart = std::chrono::high_resolution_clock::now();
				
				int start, end, step;

				// If there are more threads, or the same amount of threads defined than images, each image is threaded.
				if (userDefinedThreadCount >= g_vecImageFileNames.size())
				{
					for (int i = 0; i < g_vecImageFileNames.size(); i++)
					{
						threads.push_back(std::thread(loadImage, g_vecImageFileNames[i], start, end));
					}
				}

				// If there are more images than threads, the images are dived into the defined amount of threads.
				else
				{
					start = 0;
					step = g_vecImageFileNames.size() / userDefinedThreadCount;
					end = (g_vecImageFileNames.size() / userDefinedThreadCount) + (g_vecImageFileNames.size() % userDefinedThreadCount);

					for (int i = 0; i < userDefinedThreadCount; i++)
					{
						threads.push_back(std::thread(loadImageRange, g_vecImageFileNames, start, end));

						start = end;
						end += step;
					}
				}
					
				// Returns the threads to the main function when they have completed execution
				for (int i = 0; i < threads.size(); i++)
				{
					if (threads[i].joinable())
						threads[i].join();
				}

				// *** THIS CODE WAS TAKEN FROM THE LESSON HELD BY JAMES KNIGHT ***
				// Renders the images to the window
				for (int i = 0; i < g_vecLoadedImages.size(); i++)
				{
					HBITMAP loadedImage = g_vecLoadedImages[i];

					HDC hdc = GetDC(_hwnd);
					HBRUSH brush = CreatePatternBrush(loadedImage);
					RECT rect;
					SetRect(&rect, 100* i, 0, 100 * i + 100, 100);
					FillRect(hdc, &rect, brush);
					DeleteObject(brush);
					ReleaseDC(_hwnd, hdc);
				}
				// *** THIS IS THE END OF THE CODE GIVEN BY JAMES KNIGHT ***

				// Ends and calculates the time of the threading and outputs the time to the window
				auto ImageLoadTimeStop = std::chrono::high_resolution_clock::now();
				auto ImageLoadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(ImageLoadTimeStop - ImageLoadTimeStart);

				std::wstring Out = std::to_wstring(ImageLoadDuration.count());
				Out += L" ms to load images. ";
				ImageLoadTime = Out.c_str();
				_hwnd = CreateWindow(L"STATIC", ImageLoadTime, WS_VISIBLE | WS_CHILD | WS_BORDER, 0, 0, 300, 25, _hwnd, NULL, NULL, NULL);
				
			}
			else
			{
				MessageBox(_hwnd, L"No Image File selected", L"Error Message", MB_ICONWARNING);
			}

			return (0);
		}
		break;
		
		case ID_EXIT:
		{
			SendMessage(_hwnd, WM_CLOSE, 0, 0);
			return (0);
		}
		break;
		default:
			break;
		}
	}
	break;
	case WM_CLOSE:
	{
		PostQuitMessage(0);
	}
	break;
	default:
		break;
	}
	return (DefWindowProc(_hwnd, _uiMsg, _wparam, _lparam));
}

HWND CreateAndRegisterWindow(HINSTANCE _hInstance)
{
	WNDCLASSEX winclass; // This will hold the class we create.
	HWND hwnd;           // Generic window handle.

						 // First fill in the window class structure.
	winclass.cbSize = sizeof(WNDCLASSEX);
	winclass.style = CS_DBLCLKS | CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	winclass.lpfnWndProc = WindowProc;
	winclass.cbClsExtra = 0;
	winclass.cbWndExtra = 0;
	winclass.hInstance = _hInstance;
	winclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	winclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	winclass.hbrBackground =
		static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
	winclass.lpszMenuName = NULL;
	winclass.lpszClassName = WINDOW_CLASS_NAME;
	winclass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	// register the window class
	if (!RegisterClassEx(&winclass))
	{
		return (0);
	}

	HMENU _hMenu = LoadMenu(_hInstance, MAKEINTRESOURCE(IDR_MENU1));

	// create the window
	hwnd = CreateWindowEx(NULL, // Extended style.
		WINDOW_CLASS_NAME,      // Class.
		L"MultiThreaded Loader Tool",   // Title.
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		10, 10,                    // Initial x,y.
		_kuiWINDOWWIDTH, _kuiWINDOWHEIGHT,                // Initial width, height.
		NULL,                   // Handle to parent.
		_hMenu,                   // Handle to menu.
		_hInstance,             // Instance of this application.
		NULL);                  // Extra creation parameters.

	return hwnd;
}


int WINAPI WinMain(HINSTANCE _hInstance,
	HINSTANCE _hPrevInstance,
	LPSTR _lpCmdLine,
	int _nCmdShow)
{

	MSG msg;  //Generic Message

	HWND _hwnd = CreateAndRegisterWindow(_hInstance);

	std::string line;

	// Opening a file & retrieving its contents and converting from string to integer
	std::fstream myFile;

	myFile.open("inputFile.txt", std::fstream::in);

	if (myFile.is_open())
	{
		getline(myFile, line);
		userDefinedThreadCount = stoi(line);
		myFile.close();
	}
	
	if (userDefinedThreadCount < 1)
	{
		std::wstring OutMessage;
		OutMessage += L" Error! Please input a number into the file greater than zero. ";
		errorMessage = OutMessage.c_str();
		_hwnd = CreateWindow(L"STATIC", errorMessage, WS_VISIBLE | WS_CHILD | WS_BORDER, 200, 200, 300, 100, _hwnd, NULL, NULL, NULL);
	}
	
	if (!(_hwnd))
	{
		return (0);
	}

	while (true)
	{
		
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			
			if (msg.message == WM_QUIT)
			{
				break;
			}

			TranslateMessage(&msg);
			
			DispatchMessage(&msg);
		}

	}
	
	return (static_cast<int>(msg.wParam));
}