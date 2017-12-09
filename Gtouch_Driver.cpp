/*
期望功能:
1.左键点击
2.左键长按并滑动进行拖动
3.双指点击模拟右键
4.双指按住并滑动模拟滚轮进行页面滚动
5.双指长按双击

*/

//TODO
//add try catch while try reading data?
//add adjust procdure 
//graph intereface
//time based action judge
#include "stdafx.h"
#include <ctype.h>
#include <iostream>
#include <Windows.h>
#include <string>
#include <msclr\marshal_cppstd.h>  
#using <System.dll>

#define DEBUG
//#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

using namespace System;
using namespace System::IO::Ports;
using std::cout;
using std::endl;
using std::string;
using namespace msclr::interop;

//const int img_width = 640;
//const int img_height = 480;
const double fScreenWidth = 1920 - 1;
const double fScreenHeight = 1080 - 1;

//the four corner point of the screen in image as a trapezoid
//used to convert to rectangle
double X_L_U = 12;
double Y_L_U = 119;

double X_R_U = 622;
double Y_R_U = 112;

double X_L_D = 88;
double Y_L_D = 375;

double X_R_D = 562;
double Y_R_D = 367;

//used to convert the sixangle Hexagon to fourangle trapezoid to enahance the accuracy
double X_M_U = 308;
double Y_M_U = 88;

double X_M_D = 313;
double Y_M_D = 383;

//used as the center for cordinates propotion
double X_Center = 324;
double Y_Center = 210;


/*
因为激光面高于屏幕,故反射点高于碰触点,在摄像头看来碰触点下移,故这里采用坐标整体拉伸的办法来解决,其需要实现找到中心点,并相对于此根据系数进行基准屏幕四点坐标的变换
也可以通过手指触控来确定屏幕四点的坐标

*/
bool cordinatesExpansionEnable = true;

double proportion_X_L = 1.12;
double proportion_X_R = 1.15;
double proportion_Y_U = 1;
double proportion_Y_D = 1;

const int newActionInterval = 400;
const int singleClickMaxInterval = 500;
const int wheelSensitivity = 15;
SYSTEMTIME sys_time;
SYSTEMTIME last_receive_time;
SYSTEMTIME last_triggered_time;

POINT center[10];
int counts = 0;

bool isNewDataAvailable = false;
bool singleClickToDrag = false;
bool DoubleTouchActions = false;
bool verticalWheelAction = false;
bool tripleClickAction = false;
bool multiClickAction = false;

bool isClicked = false;
bool isDoubleClicked = false;
bool isLongClicked = false;
bool ismultiClicked = false;
bool isHolding = false;

void cordinatesExpansion();
void actionJudge();
void initiallize();
void receive();
void LeftClick();
void RightClick();
void MouseMove(int x, int y);
POINT map(double x, double y);
inline long dtime(SYSTEMTIME now_time, SYSTEMTIME previous_time);

enum MouseKeyStatus
{
	Pressed,
	Released,
	ToClick,
	ToDoubleClick
}LeftKeyStatus, RightKeyStatus;

//串口通信的一个类
public ref class PortChat
{
public:
	static SerialPort^ port;
	static void PortSet()
	{
		port = gcnew SerialPort();
		array<System::String^>^ PORTS = nullptr;
		PORTS = SerialPort::GetPortNames();
		Console::WriteLine("Available Ports : ");
		// Display each port name to the console.
		for each(System::String^ ports in PORTS)
		{
			Console::WriteLine(ports);
		}
		System::String^ name;
		Console::Write("Port Name: ");
		name = Console::ReadLine();
		if (name == "")
			name = "COM3";

		port->PortName = name;
		port->BaudRate = 115200;

		try
		{
			port->Open();
		}
		catch (System::ArgumentException^ error)
		{
			Console::WriteLine(error->Message);
			PortChat::PortSet();
		}
		catch (System::UnauthorizedAccessException^ error)
		{
			Console::WriteLine(error->Message);
		}
		catch (System::IO::IOException^ error)
		{
			Console::WriteLine(error->Message);
		}
		catch (System::InvalidOperationException^ error)
		{
			Console::WriteLine(error->Message);
		}
		if (port->IsOpen)
			Console::WriteLine(name + " Selected");
	}
};

#pragma region mouse action

//坐标映射的函数,根据实际情况进行了调整,建议另外重新写
POINT map(double x, double y)
{
#ifdef DEBUG
	//Console::WriteLine("map called");
#endif // DEBUG

	//六边形转梯形
	double Y_UP = (Y_L_U + Y_R_U) / 2;
	double Y_D = (Y_L_D + Y_R_D) / 2;

	double Y_M = (Y_M_U + Y_M_D) / 2.0;
	double y_rate_u = (Y_UP - Y_M) / (Y_M_U - Y_M);
	double y_rate_d = (Y_D - Y_M) / (Y_M_D - Y_M);
	
	if (y > Y_M)
	{
		y = Y_M - (Y_M - y)*y_rate_u;
	}
	if (y < Y_M)
	{
		y = Y_M - (Y_M - y)*y_rate_d;
	}

	
	//梯形转换为屏幕坐标(1920x1080)
	POINT point;
	double L1 = X_R_U - X_L_U;
	double L2 = X_R_D - X_L_D;
	double X_Mid_U = (X_R_U + X_L_U) / 2;
	double X_Mid_D = (X_R_D + X_L_D) / 2;
	double dy = (Y_L_D + Y_R_D - Y_L_U - Y_R_U) / 2;
	double zoom_rate = 1 - ((L1 - L2)*(y - Y_UP) / (dy*L1));
	double L3 = zoom_rate*L1;
	double y_rate = (y - Y_UP) / dy;

	double X_Mid = X_Mid_U - y_rate*(X_Mid_U - X_Mid_D);
	//double x_rate = (X_Mid - X_L_U - (X_Mid - x) / zoom_rate) / (X_Mid - X_L_U);
	double x_rate = (L3 / 2 - (X_Mid - x)) / L3;

	if (x_rate < 0)
		point.x = 0;
	else if (x_rate < 1)
		point.x = long(x_rate*fScreenWidth);
	if (y_rate < 0)
		point.y = 0;
	else if (y_rate < 1)
		point.y = long(y_rate*fScreenHeight);
	
	//边缘优化
	point.x = point.x < 50 ? 20 : (point.x > 1870 ? 1900 : point.x);
	point.y = point.y < 50 ? 20 : (point.y > 1030 ? 1060 : point.y);
	
	//右上角关闭按钮优化
	if (point.x > 1800 && point.y < 100)
	{
		point.x = 1910;
		point.y = 10;
	}

	//以抛物线关系校准坐标(消除鱼眼?)
	if (1)
	{
		double y1 = 225;
		double y2 = 890;
		double A = 150 / (((y1 + y2) / 2)*((y1 + y2) / 2));
		if (y1 < point.y&&point.y < y2)
		{
			//	后面的为负数,向上校准,故加
			point.y += long(A*(point.y - y1)*(point.y - y2));
		}
	}
	

	return point;
}

void MouseMove(int x, int y)
{
#ifdef DEBUG
	Console::WriteLine("mouse move called");
	cout << "X:" << x << endl;
	cout << "Y:" << y << endl;

#endif // DEBUG
	//double fScreenWidth = ::GetSystemMetrics(SM_CXSCREEN) - 1;
	//double fScreenHeight = ::GetSystemMetrics(SM_CYSCREEN) - 1;

	double fx = x*(65535.0f / fScreenWidth);
	double fy = y*(65535.0f / fScreenHeight);
	INPUT  Input = { 0 };
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
	Input.mi.dx = (long)fx;
	Input.mi.dy = (long)fy;
	::SendInput(1, &Input, sizeof(INPUT));
}

//TODO
void MousePen()
{
#ifdef DEBUG
	Console::WriteLine("mouse pen called");
#endif // DEBUG

	/*
	typedef struct tagTOUCHINPUT {
    LONG x;
    LONG y;
    HANDLE hSource;
    DWORD dwID;
    DWORD dwFlags;
    DWORD dwMask;
    DWORD dwTime;
    ULONG_PTR dwExtraInfo;
    DWORD cxContact;
    DWORD cyContact;
} TOUCHINPUT, *PTOUCHINPUT;
typedef TOUCHINPUT const * PCTOUCHINPUT;

	*/
	TOUCHINPUT Input = { 0 };

	//Input.type = INPUT_MOUSE;
	Input.dwFlags = TOUCHEVENTF_PEN;
	Input.dwMask = TOUCHINPUTMASKF_TIMEFROMSYSTEM;
//	Input.mi.mouseData = 0;
	Input.dwTime = 0;
	HANDLE hProcess0 = GetStdHandle(STD_OUTPUT_HANDLE);

	//GetTouchInputInfo((HTOUCHINPUT)lParam, 1, &Input, sizeof(TOUCHINPUT));
	

}

void MouseWheel(int dWheel)
{
#ifdef DEBUG
	Console::WriteLine("mouse wheel called");
#endif // DEBUG
	INPUT Input = { 0 };

	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_WHEEL;
	Input.mi.mouseData = dWheel*wheelSensitivity;
	Input.mi.time = 0;
	SendInput(1, &Input, sizeof(INPUT));
}

void MiddleClick()
{
#ifdef DEBUG
	Console::WriteLine("middle click called");
#endif // DEBUG

	INPUT    Input = { 0 };
	// 设置滚轮量
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_WHEEL;
	Input.mi.mouseData = 500;
	::SendInput(1, &Input, sizeof(INPUT));
}

void LeftPress()
{
#ifdef DEBUG
	Console::WriteLine("left key press called");
#endif // DEBUG
	INPUT Input = { 0 };
	// left down 
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	::SendInput(1, &Input, sizeof(INPUT));
}

void LeftRelease()
{
#ifdef DEBUG
	Console::WriteLine("left key release called");
#endif // DEBUG
	INPUT Input = { 0 };
	// left up
	//::ZeroMemory(&Input, sizeof(INPUT));
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	::SendInput(1, &Input, sizeof(INPUT));
}

void LeftClick()
{
#ifdef DEBUG
	Console::WriteLine("left key click called");
#endif // DEBUG

	INPUT Input = { 0 };
	// left down 
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	::SendInput(1, &Input, sizeof(INPUT));

	// left up
	::ZeroMemory(&Input, sizeof(INPUT));
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	::SendInput(1, &Input, sizeof(INPUT));
}

void RightPress()
{
#ifdef DEBUG
	Console::WriteLine("right key press called");
#endif // DEBUG
	INPUT Input = { 0 };
	// right down
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
	::SendInput(1, &Input, sizeof(INPUT));
}

void RightRelease()
{
#ifdef DEBUG
	Console::WriteLine("right key release called");
#endif // DEBUG
	INPUT Input = { 0 };
	// right up
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
	::SendInput(1, &Input, sizeof(INPUT));
}

void RightClick()
{
#ifdef DEBUG
	Console::WriteLine("right key click called");
#endif // DEBUG
	INPUT Input = { 0 };
	// right down
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
	::SendInput(1, &Input, sizeof(INPUT));

	// right up
	::ZeroMemory(&Input, sizeof(INPUT));
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
	::SendInput(1, &Input, sizeof(INPUT));
}

#pragma endregion

int main(array<System::String ^> ^args)
{
	//Console::WriteLine("main called");

	initiallize();
	while (1)
	{
		GetLocalTime(&sys_time);
		receive();
		actionJudge();

	}

	return 0;
}

//根据检测的坐标
void actionJudge()
{
#ifdef DEBUG
	//Console::WriteLine("actionJudge called");
#endif // DEBUG

	static POINT lastPoint = center[0];
	static bool isdoubleClickActionDone = false;
	static bool isLastPointAvailable = false;
	static bool LeftKeyToClick = false;
	static bool RightKeyToClick = false;
	static SYSTEMTIME doubleTouchActionEnterTime = sys_time;

	//LeftKeyStatus::ToClick
	//LeftKeyStatus= MouseKeyStatus::
	if (counts == 0)
	{
		isdoubleClickActionDone = false;
		if (LeftKeyToClick)
		{
			LeftClick();
			LeftKeyToClick = false;
		}
		if (RightKeyToClick)
		{
			RightClick();
			RightKeyToClick = false;
		}

		if (RightKeyStatus == MouseKeyStatus::Pressed)
		{
			RightRelease();
			RightKeyStatus = MouseKeyStatus::Released;
		}	
		if (LeftKeyStatus == MouseKeyStatus::Pressed)
		{
			LeftRelease();
			LeftKeyStatus = MouseKeyStatus::Released;
		}
		singleClickToDrag = DoubleTouchActions = verticalWheelAction = tripleClickAction = multiClickAction = false;
	}

	if (isNewDataAvailable)
	{
		isNewDataAvailable = false;
		POINT point = map(center[0].x, center[0].y);
		MouseMove(point.x, point.y);

		if (isLastPointAvailable)
		{
			int dx = center[0].x - lastPoint.x;
			int dy = center[0].y - lastPoint.y;
/*

			//if none was set
			//判断动作类型
			if (!singleClickToDrag & !DoubleTouchActions & !verticalWheelAction & !tripleClickAction & !multiClickAction)
			{
				switch (counts)
				{
				case 0:
					singleClickToDrag = DoubleTouchActions = verticalWheelAction = tripleClickAction = multiClickAction = false;
					break;
				case 1:
					singleClickToDrag = true;
					break;
				case 2:

					DoubleTouchActions = true;
					break;
				case 3:
					tripleClickAction = true;
					break;
				case 4:
				case 5:
				case 6:
					multiClickAction = true;
					break;

				default:
					multiClickAction = true;
					break;
				}
			}
*/
			if (DoubleTouchActions && !isdoubleClickActionDone)
			{

				if (verticalWheelAction)
					MouseWheel(dy);
				else if (abs(dx) + abs(dy) > 10)//有进入此模式的最小移动阀值,之后锁定
				{
					if (abs(dy) / (double)abs(dx) > 2)  //主要是y轴方向的移动
					{
						RightKeyToClick = false;
						verticalWheelAction = true;
						MouseWheel(dy);
					}
				}
				else if (dtime(sys_time, doubleTouchActionEnterTime) > newActionInterval)
				{
					//double click action
					if (RightKeyStatus == MouseKeyStatus::Pressed)
					{
						RightRelease();
						RightKeyStatus = MouseKeyStatus::Released;
						//TODO 
						//after one right click ,double left click would fail so the mouse should move a little upleft then do the double click
						//but normally it's no necessary to do so 
						//as we seperate each action and execute at the end of the touch action
					}
					RightKeyToClick = false;
					LeftClick();
					Sleep(1);
					LeftClick();
					//once double cliclk done, do nothing and skip all judge till next touch action
					isdoubleClickActionDone = true;
				}
				else
				{
					RightKeyToClick = true;
				}
			}
			else if (counts == 1)
			{

				//LeftKeyToClick = true;

				if (LeftKeyStatus == MouseKeyStatus::Released)
				{
					LeftPress();
					LeftKeyStatus = MouseKeyStatus::Pressed;
				}
			}
			else if (counts == 2)
			{
				GetLocalTime(&doubleTouchActionEnterTime);

				LeftKeyToClick = false;
				if (LeftKeyStatus == MouseKeyStatus::Pressed)
				{
					LeftRelease();
					LeftKeyStatus = MouseKeyStatus::Released;
				}
				DoubleTouchActions = true;
			}
			lastPoint = center[0];
		}
		else
		{
			isLastPointAvailable = true;
			lastPoint = center[0];
		}
	}
}

void Expansion(double &x, double X0, double p)
{
	x = X0 - p*(X0 - x);
}

void cordinatesExpansion()
{
	//Expansion(X_L_U, X_Center, 1 / proportion_X_L);
	//Expansion(X_L_D, X_Center, 1 / proportion_X_L);
	Expansion(X_L_U, X_Center, proportion_X_L);
	Expansion(X_L_D, X_Center, proportion_X_L);
	Expansion(X_R_U, X_Center, proportion_X_R);
	Expansion(X_R_D, X_Center, proportion_X_R);

	//Expansion(Y_L_U, Y_Center, 1 / proportion_Y_U);
	//Expansion(Y_R_U, Y_Center, 1 / proportion_Y_U);
	Expansion(Y_L_U, Y_Center, proportion_Y_U);
	Expansion(Y_R_U, Y_Center, proportion_Y_U);
	Expansion(Y_L_D, Y_Center, proportion_Y_D);
	Expansion(Y_R_D, Y_Center, proportion_Y_D);

}

void initiallize()
{
	//将变量值设置为本地时间
	GetLocalTime(&sys_time);
	//system("time");

	LeftKeyStatus = MouseKeyStatus::Released;
	RightKeyStatus = MouseKeyStatus::Released;
	if (cordinatesExpansionEnable)
		cordinatesExpansion();
	//port initiallization
	PortChat::PortSet();
	PortChat::port->ReadTimeout = 200;
	PortChat::port->ReadExisting();
	HANDLE hProcess0 = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hProcess0, FOREGROUND_GREEN);//设置字体颜色为绿色

	//TODO
	//point coordinates initiallization
	//automatically get the corner points cordinations
    //automatically adjust the paraments value based on current method

}

//ms
bool isNewDataReady(int waitTime)
{
	GetLocalTime(&sys_time);
	SYSTEMTIME time_enter_receive = sys_time;
	while (PortChat::port->BytesToRead < 6)
	{
		if (dtime(sys_time, time_enter_receive) > waitTime)
			return false;//time out	
		GetLocalTime(&sys_time);
	}
	return true;
}

void receive()
{
#ifdef DEBUG
	//Console::WriteLine("receive called");
#endif // DEBUG
	//wait till there are data come or time out
	if (isNewDataReady(200))
	{
		counts = 0;

		GetLocalTime(&sys_time);
		if (sys_time.wSecond * 1000 + sys_time.wMilliseconds - last_receive_time.wSecond * 1000 - last_receive_time.wMilliseconds > newActionInterval)
		{
			last_triggered_time = sys_time;
		}
		last_receive_time = sys_time;
		

		System::String ^str_clr;
		str_clr = PortChat::port->ReadLine();

		while (str_clr != "EndCycle\r")
		{
			std::string str_std = msclr::interop::marshal_as<std::string>(str_clr);
			char buffer[20];
			strcpy_s(buffer, str_std.c_str());

			char *p;
			if (NULL != (p = strstr(buffer, "X:")))
			{
				center[counts].x = atoi(p + 2);
			}
			if (NULL != (p = strstr(buffer, "Y:")))
			{
				center[counts].y = atoi(p + 2);
				counts++;
				isNewDataAvailable = true;
			}

			if (isNewDataReady(200))
				str_clr = PortChat::port->ReadLine();
		}
		
		//initiallize for the next round
		for (int i = counts; i < 10; i++)
		{
			center[i].x = 0;
			center[i].y = 0;
		}
	}
	else
	{

	}

}

inline long dtime(SYSTEMTIME now_time, SYSTEMTIME previous_time)
{
	return 1000 * (60 * (60 * (24 * (now_time.wDay - previous_time.wDay) + now_time.wHour - previous_time.wHour) + now_time.wMinute - previous_time.wMinute) + now_time.wSecond - previous_time.wSecond) + now_time.wMilliseconds - previous_time.wMilliseconds;
}

/*
void receive_one()
{
	for (int i = 0; i < 10; i++)
	{
		System::String ^str_clr;
		str_clr = PortChat::port->ReadLine();

		std::string str_std = msclr::interop::marshal_as<std::string>(str_clr);
		char buffer[10];
		strcpy_s(buffer, str_std.c_str());

		char *p;
		if (NULL != (p = strstr(buffer, "X:")))
		{
			center[0].x = atoi(p + 2);
		}
		if (NULL != (p = strstr(buffer, "Y:")))
		{
			center[0].y = atoi(p + 2);
			return;
		}
	}

}

POINT map_WZY(double x, double y)
{
	double X1 = X_L_U, X2 = X_R_U, X3 = X_L_D, X4 = X_R_D;
	double Y1 = Y_L_U, Y2 = Y_R_U, Y3 = Y_L_D, Y4 = Y_R_D;
	double X11, X12;
	//double H, L;

	X11 = (X1*(y - Y1) / (x - X1) - X4*(Y3 - Y4) / (X3 - X4) + Y4 - Y1) / ((y - Y1) / (x - X1) - (Y3 - Y4) / (X3 - X4));
	X12 = (X1*(y - Y1) / (x - X1) - X4*(Y2 - Y4) / (X2 - X4) + Y4 - Y1) / ((y - Y1) / (x - X1) - (Y2 - Y4) / (X2 - X4));

	POINT point;
	point.x = (long)fScreenWidth * (x - X1) / (X12 - X1);
	point.y = (long)fScreenHeight * (x - X1) / (X11 - X1);
	return point;
}
*/

//convert std string to clr string
//System::String^ Xangle_S_CLR = marshal_as<System::String^>(std::to_string(Xangle));
//convert clr string to std string 
//std::string str_std = msclr::interop::marshal_as<std::string>(str_clr);
//convert std string to char buffer
//char buffer[10];
//strcpy(buffer, str_std.c_str());
