#include <Windows.h>
#include <process.h>
#include <string>
#include <atlimage.h>
#include <vector>
#include <map>
#include <mmsystem.h>
#include <opencv.hpp>
#include <fstream>
#include <codecvt> 
#include <iomanip>

#pragma comment(lib, "winmm.lib") 

using namespace std;
using namespace cv;

namespace config{
	wstring WindowClass;
	wstring WindowName;
	wstring ChildName;
	Vec3b colorEdge;
	Vec3b colorDark;
	Vec3b colorBright;
	Vec3b colorGrid;
	Vec3b colorFrame;

	wstring Trim(wstring str)
	{
		auto itor = remove_if(str.begin(), str.end(), [](char c) {return c == L' '; });
		str.erase(itor, str.end());
		return str;
	}
	vector<wstring> split(wstring str,char delimiters) {
		vector<wstring>res;
		for (int l = 0,r = 0;r<=str.size(); ++r) {
			if (r  ==  str.size() || str[r] == delimiters) {
				res.push_back(str.substr(l, r - l));
				l = r+1;
			}
		}
		return res;
	}

	void Parse(wstring line) {
		line = Trim(line).c_str();
		wstring key, value;
		vector<wstring>lsp = split(line,'=');
		if (lsp.size() == 2) {
			key = lsp[0];
			value = lsp[1];
		}
		if (key == L"WindowClass") {
			WindowClass = value;
		}
		else if (key == L"WindowName") {
			WindowName = value;
		}
		else if (key == L"ChildName") {
			ChildName = value;
		}
		else if (key.substr(0,5) == L"Color") {
			Vec3b color;
			for(int i=0;i<3;++i)
				color[i] = _wtoi(split(value,',')[i].c_str());
			if (key == L"ColorEdge")
				colorEdge = color;
			if (key == L"ColorDark")
				colorDark = color;
			if (key == L"ColorBright")
				colorBright = color;
			if (key == L"ColorGrid")
				colorGrid = color;
			if (key == L"ColorFrame")
				colorFrame = color;
		}
		else {
			wcout << L"Unknow config parameter: " << key << endl;
		}
	}

	void ReadConfig() {
		wfstream fs(L"./config.cfg", ios::in);
		if (fs.is_open()) {
			wstring line;
			for (; getline(fs, line);) {
				Parse(line);
			}
		}
	}
};

HWND EnumHWnd = 0;   //用来保存CPU使用记录窗口的句柄
wstring ClassNameToEnum;
BOOL CALLBACK EnumChildWindowsProc(HWND hWnd, LPARAM lParam) //寻找CPU使用记录界面的子窗口ID
{
	wchar_t WndClassName[256];
	GetClassName(hWnd, WndClassName, 256);
	if (WndClassName == ClassNameToEnum && (EnumHWnd == 0 || [&hWnd]() {
			RECT cRect,tRect;
			GetWindowRect(hWnd, &tRect);
			GetWindowRect(EnumHWnd, &cRect);
			int tW = (tRect.right - tRect.left),
				tH = (tRect.bottom - tRect.top),
				cW = (cRect.right - cRect.left),
				cH = (cRect.bottom - cRect.top);
			return cW * cH < tW* tH;
		}())) {
		EnumHWnd = hWnd;
	}
	return true;
}

void FindCPUWnd() 
{
	HWND TaskmgrHwnd = FindWindow(config::WindowClass.c_str(), L"任务管理器");
	ClassNameToEnum = config::ChildName;
	EnumChildWindows(TaskmgrHwnd, EnumChildWindowsProc, NULL);
}


void Binarylize(Mat& src) {
	Mat bin, edge;
	cvtColor(src, bin, COLOR_BGR2GRAY);
	inRange(bin, Scalar(128, 128, 128), Scalar(255, 255, 255), bin);

	Canny(bin, edge, 80, 130);
	int gridHeight = src.cols / 10.0; 
	int gridWidth = src.rows / 8.0;
	int gridOffset = clock() / 1000 * 10;
	for (int r = 0; r < src.rows; ++r) {
		for (int c = 0; c < src.cols; ++c) {
			src.at<Vec3b>(r, c) = ((bin.at<uchar>(r, c) == 255) ? config::colorBright : config::colorDark);
			if (r % gridHeight == 0 || (c + gridOffset) % gridWidth == 0) src.at<Vec3b>(r, c) = config::colorGrid;
			if (edge.at<uchar>(r, c) == 255) src.at<Vec3b>(r, c) = config::colorEdge;
		}
	}
	rectangle(src, Rect{ 0,0,src.cols,src.rows }, config::colorFrame, 0.1);
}

void VideoFinder(string& video) {
	WIN32_FIND_DATAA wfd;
	HANDLE hFile = FindFirstFileA("*.*", &wfd);
	for (; hFile != INVALID_HANDLE_VALUE;)
	{
		FindNextFileA(hFile, &wfd);
		string fileName = wfd.cFileName;
		string type = fileName.substr(fileName.find_last_of('.') + 1);
		if (type == "flv" || type == "mp4" || type == "avi") {
			system(("ffmpeg -i " + fileName + " audio.wav -y").c_str());
			video = fileName;
			cout << "find " << fileName << endl;
			return;
		}
	}
}

int main() {
	config::ReadConfig();
	string videoName;
	VideoFinder(videoName);
	if (videoName.empty())
		return 0;
	FindCPUWnd(); //寻找CPU使用记录的窗口,  函数会把窗口句柄保存在全局变量EnumHWnd
	string wndName = "innerPlayer";
	namedWindow(wndName, WINDOW_NORMAL);
	HWND playerWnd = FindWindowA("Main HighGUI class", wndName.c_str());
	SetWindowLong(playerWnd, GWL_STYLE, GetWindowLong(playerWnd, GWL_STYLE) & (~(WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX)));
	SetWindowLong(playerWnd, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_LAYERED);
	RECT rect;
	GetWindowRect(EnumHWnd, &rect);
	SetParent(playerWnd, GetParent(EnumHWnd));
	SetWindowPos(playerWnd, HWND_TOPMOST, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_SHOWWINDOW);
	
	VideoCapture video(videoName);
	double frameTime = 1000.0 / video.get(CAP_PROP_FPS);
	PlaySound(L"audio.wav", NULL, SND_FILENAME | SND_ASYNC);
	clock_t startTime = clock();
	int frameCount = 0;
	
	for (Mat frame; video.read(frame); ++frameCount) {
		GetWindowRect(EnumHWnd, &rect);
		int w = rect.right - rect.left;
		int h = rect.bottom - rect.top;
		MoveWindow(playerWnd, 0, 0, w, h, true);
		//resize(frame, frame, Size(w,h),0,0, INTER_NEAREST);
		Binarylize(frame);
		imshow(wndName, frame);
		cout << fixed <<setprecision(3)<< "Frame: " << frameCount << " at " << frameCount * frameTime/1000.0 << endl;
		for (; frameCount * frameTime > clock();)waitKey(1);
	}
}