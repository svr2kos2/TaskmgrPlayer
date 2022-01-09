#include <Windows.h>
#include <process.h>
#include <string>
#include <atlimage.h>
#include <vector>
#include <mmsystem.h>
#include <opencv.hpp>

#pragma comment(lib, "winmm.lib") 

using namespace std;
using namespace cv;

HWND EnumHWnd = 0;   //用来保存CPU使用记录窗口的句柄
wstring ClassNameToEnum;
BOOL CALLBACK EnumChildWindowsProc(HWND hWnd, LPARAM lParam) //寻找CPU使用记录界面的子窗口ID
{
	wchar_t WndClassName[255] = { 0 };
	GetClassName(hWnd, WndClassName, 255);
	wstring str;
	str = WndClassName;
	if (str == ClassNameToEnum)
	{
		RECT rect;
		GetWindowRect(hWnd, &rect);
		if (str == L"CtrlNotifySink")
			if ((rect.bottom - rect.top) < 65 || (rect.right - rect.left) < 65)
				return true;
		EnumHWnd = hWnd;   //如果找到了CPU使用记录的窗口的话, 把他赋值给EnumHWnd
		return false;
	}

	return true;
}

void FindCPUWnd()    //要想适用其他版本的任务管理器 更改这个函数
{
	HWND h = FindWindow(L"TaskManagerWindow", L"任务管理器");
	ClassNameToEnum = L"NativeHWNDHost";     //一级子窗口
	EnumChildWindows(h, EnumChildWindowsProc, NULL);
	ClassNameToEnum = L"DirectUIHWND";		//二级子窗口
	EnumChildWindows(EnumHWnd, EnumChildWindowsProc, NULL);	//子窗口的名字适用Spy++获得
	ClassNameToEnum = L"CtrlNotifySink";		//三级子窗口
	EnumChildWindows(EnumHWnd, EnumChildWindowsProc, NULL);
	ClassNameToEnum = L"CvChartWindow";		//四级子窗口
	EnumChildWindows(EnumHWnd, EnumChildWindowsProc, NULL);
}

void mouseEvent(int event, int x, int y, int flags, void* param) {
	SetCursor(LoadCursor(NULL, IDC_ARROW));
}

void Binarylize(Mat& src) {
	static auto colorEdge = Vec3b(187, 125, 12);
	static auto colorDark = Vec3b(250, 246, 241);
	static auto colorBright = Vec3b(255, 255, 255);
	static auto colorGrid = Vec3b(244, 234, 217);
	Mat bin, edge;
	cvtColor(src, bin, COLOR_BGR2GRAY);
	inRange(bin, Scalar(128, 128, 128), Scalar(255, 255, 255), bin);

	Canny(bin, edge, 80, 130);
	int gridHeight = src.cols / 10.0;
	int gridWidth = src.rows / 8.0;
	int gridOffset = clock() / 1000 * 10;
	for (int r = 0; r < src.rows; ++r) {
		for (int c = 0; c < src.cols; ++c) {
			src.at<Vec3b>(r, c) = ((bin.at<uchar>(r, c) == 255) ? colorBright : colorDark);
			if (r % gridHeight == 0 || (c + gridOffset) % gridWidth == 0) src.at<Vec3b>(r, c) = colorGrid;
			if (edge.at<uchar>(r, c) == 255) src.at<Vec3b>(r, c) = colorEdge;
		}
	}
	rectangle(src, Rect{ 0,0,src.cols,src.rows }, colorEdge, 0.1);
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
	MoveWindow(FindWindowA("HighGUI class", 0), -100, -1, rect.right - rect.left, rect.bottom - rect.top, true);
	setMouseCallback(wndName, (MouseCallback)mouseEvent);
	SetParent(playerWnd, GetParent(EnumHWnd));
	SetWindowPos(playerWnd, HWND_TOPMOST, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_SHOWWINDOW);
	VideoCapture video(videoName);
	double frameTime = 1000.0 / video.get(CAP_PROP_FPS);
	PlaySound(L"audio.wav", NULL, SND_FILENAME | SND_ASYNC);
	clock_t startTime = clock();
	int frameCount = 0;
	for (Mat frame; video.read(frame); ++frameCount) {
		Binarylize(frame);
		GetWindowRect(EnumHWnd, &rect);
		MoveWindow(playerWnd, 0, 0, rect.right - rect.left, rect.bottom - rect.top, true);
		imshow(wndName, frame);
		for (; frameCount * frameTime > clock();)waitKey(1);
	}
	system("del audio.wav");
}