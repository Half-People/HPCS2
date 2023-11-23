//#include "offset/offsets.hpp"
#include "offset/client.dll.hpp"
#include <comdef.h>
#include <Windows.h>
#include <stdio.h>
#include <TlHelp32.h>
#include <iostream>
#include <thread>
#include <cmath>
#include <wtypes.h>
#include <numbers>
#include <map>
#include "memory.hpp"
#include "api/ini.h"
#include <fstream>
#include <nlohmann/json.hpp>
using namespace nlohmann;
int render_distance = -1;
bool show_extra = true;
bool ShowViewAngle = false;
bool ShowAimBotFov = true;
int flag_render_distance = 70;
int ShowBoneDistance = 1000;
int TextSize = 15;
int BoomSize = 100;
#define BonesColor RGB(200, 200, 200)
float FOV = 15;//2
bool DrawESP = true;
bool brecoilControl = true;
bool AimBot = true;
bool AimBotHot = true;
bool AimTeam = false;
bool bTriggerbot = false;
bool bBunnyHop = false;
bool OnlyAttackVisible = true;
bool DrawESPLine = true;
bool MenuDrawBorder = false;
int TriggerDelay_ms = 90;
float AimSmoothing = 2;
int AimBones = 5;

HANDLE cs2_process_handle;
ProcessModule cs2_module_client;
ProcessModule base_engine;

std::shared_ptr<pProcess> process;
struct C_UTL_VECTOR
{
	DWORD_PTR count = 0;
	DWORD_PTR data = 0;
};
namespace render
{
	void DrawBorderBox(HDC hdc, int x, int y, int w, int h, COLORREF borderColor)
	{
		HBRUSH hBorderBrush = CreateSolidBrush(borderColor);
		HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBorderBrush);

		RECT rect = { x, y, x + w, y + h };
		FrameRect(hdc, &rect, hBorderBrush);

		SelectObject(hdc, hOldBrush); // Restore the original brush
		DeleteObject(hBorderBrush);	  // Delete the temporary brush
	}

	void DrawCircle(HDC hdc, int centerX, int centerY, int radius, COLORREF color) {
		HPEN hPen = CreatePen(PS_DASH, 2, color); // 使用红色、宽度为1像素，实线样式的画笔
		SelectObject(hdc, hPen);
		Ellipse(hdc, centerX - radius, centerY - radius, centerX + radius, centerY + radius);
		DeleteObject(hPen);
	}

	void DrawFilledBox(HDC hdc, int x, int y, int width, int height, COLORREF color)
	{
		HBRUSH hBrush = CreateSolidBrush(color);
		RECT rect = { x, y, x + width, y + height };
		FillRect(hdc, &rect, hBrush);
		DeleteObject(hBrush);
	}

	void SetTextSize(HDC hdc, int textSize)
	{
		LOGFONT lf;
		HFONT hFont, hOldFont;

		// Initialize the LOGFONT structure
		ZeroMemory(&lf, sizeof(LOGFONT));
		lf.lfHeight = -textSize;			// Set the desired text height (negative for height)
		lf.lfWeight = FW_NORMAL;			// Set the font weight (e.g., FW_NORMAL for normal)
		lf.lfQuality = ANTIALIASED_QUALITY; // Enable anti-aliasing

		// Create a new font based on the LOGFONT structure
		hFont = CreateFontIndirect(&lf);

		// Select the new font into the device context and save the old font
		hOldFont = (HFONT)SelectObject(hdc, hFont);

		// Clean up the old font (when done using it)
		DeleteObject(hOldFont);
	}

	void RenderLine(HDC hdc, float p1x, float p1y, float p2x, float p2y, COLORREF borderColor)
	{
		HPEN hPen = CreatePen(PS_DASH, 2, borderColor); // 使用红色、宽度为1像素，实线样式的画笔
		SelectObject(hdc, hPen);
		MoveToEx(hdc, (int)p1x, (int)p1y, (LPPOINT)NULL);
		LineTo(hdc, (int)p2x, (int)p2y);
		DeleteObject(hPen);
	}

	void RenderText(HDC hdc, int x, int y, const char* text, COLORREF textColor, int textSize)
	{
		SetTextSize(hdc, textSize);
		SetTextColor(hdc, textColor);

		int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
		wchar_t* wide_text = new wchar_t[len];
		MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, len);

		TextOutW(hdc, x, y, wide_text, len - 1);

		delete[] wide_text;
	}
}
namespace g {
	inline HDC hdcBuffer = NULL;
	inline HBITMAP hbmBuffer = NULL;

	RECT gameBounds;
}
enum bones : int {
	head = 6,
	neck = 5,
	chest = 4,
	shoulderRight = 8,
	shoulderLeft = 13,
	elbowRight = 9,
	elbowLeft = 14,
	handRight = 11,
	handLeft = 16,
	crotch = 0,
	kneeRight = 23,
	kneeLeft = 26,
	ankleRight = 24,
	ankleLeft = 27,
};

inline namespace boneGroups {
	inline std::vector<int> mid = { bones::head,bones::neck,bones::chest,bones::crotch };
	inline std::vector<int> leftArm = { bones::neck,bones::shoulderLeft,bones::elbowLeft,bones::handLeft };
	inline std::vector<int> righttArm = { bones::neck,bones::shoulderRight,bones::elbowRight,bones::handRight };
	inline std::vector<int> leftLeg = { bones::crotch,bones::kneeLeft,bones::ankleLeft };
	inline std::vector<int> rightLeg = { bones::crotch,bones::kneeRight,bones::ankleRight };
	inline std::map<int, std::string> mid_Name = { {bones::head,"head"}, {bones::neck,"neck"}, {bones::chest,"chest"}, {bones::crotch,"crotch"} };

	inline std::vector<std::vector<int>> allGroups = { mid,leftArm,righttArm,leftLeg,rightLeg };
}
void loop();

void FunctionT();
void USETLOOP()
{
	while (true)
		FunctionT();
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		g::hdcBuffer = CreateCompatibleDC(NULL);
		g::hbmBuffer = CreateCompatibleBitmap(GetDC(hWnd), g::gameBounds.right, g::gameBounds.bottom);
		SelectObject(g::hdcBuffer, g::hbmBuffer);

		SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);

		SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 0, LWA_COLORKEY);

		//std::cout << "[overlay] Window created successfully" << std::endl;
		Beep(500, 100);
		break;
	}
	case WM_ERASEBKGND: // We handle this message to avoid flickering
		return TRUE;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		//DOUBLE BUFFERING
		FillRect(g::hdcBuffer, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));

		if (GetForegroundWindow() == process->hwnd_) {
			//render::RenderText(g::hdcBuffer, 10, 10, "cs2 | ESP", RGB(75, 175, 175), 15);
			loop();
		}

		BitBlt(hdc, 0, 0, g::gameBounds.right, g::gameBounds.bottom, g::hdcBuffer, 0, 0, SRCCOPY);

		EndPaint(hWnd, &ps);
		InvalidateRect(hWnd, NULL, TRUE);
		break;
	}
	case WM_DESTROY:
		DeleteDC(g::hdcBuffer);
		DeleteObject(g::hbmBuffer);

		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

struct view_matrix_t {
	float* operator[ ](int index) {
		return matrix[index];
	}

	float matrix[4][4];
};

struct Vector3
{
	// constructor
	constexpr Vector3(
		const float x = 0.f,
		const float y = 0.f,
		const float z = 0.f) noexcept :
		x(x), y(y), z(z) { }

	// operator overloads
	constexpr const Vector3& operator-(const Vector3& other) const noexcept
	{
		return Vector3{ x - other.x, y - other.y, z - other.z };
	}

	constexpr const Vector3& operator+(const Vector3& other) const noexcept
	{
		return Vector3{ x + other.x, y + other.y, z + other.z };
	}

	constexpr const Vector3& operator/(const float factor) const noexcept
	{
		return Vector3{ x / factor, y / factor, z / factor };
	}

	constexpr const Vector3& operator*(const float factor) const noexcept
	{
		return Vector3{ x * factor, y * factor, z * factor };
	}

	constexpr const bool operator>(const Vector3& other) const noexcept {
		return x > other.x && y > other.y && z > other.z;
	}

	constexpr const bool operator>=(const Vector3& other) const noexcept {
		return x >= other.x && y >= other.y && z >= other.z;
	}

	constexpr const bool operator<(const Vector3& other) const noexcept {
		return x < other.x&& y < other.y&& z < other.z;
	}

	constexpr const bool operator<=(const Vector3& other) const noexcept {
		return x <= other.x && y <= other.y && z <= other.z;
	}

	constexpr const Vector3& ToAngle() const noexcept
	{
		return Vector3{
			std::atan2(-z, std::hypot(x, y)) * (180.0f / std::numbers::pi_v<float>),
			std::atan2(y, x) * (180.0f / std::numbers::pi_v<float>),
			0.0f
		};
	}

	void Print()const
	{
		std::cout << "\n x : " << x << "   y :" << y << "z :" << z;
		return;
	}

	constexpr const bool IsZero() const noexcept
	{
		return x == 0.f && y == 0.f && z == 0.f;
	}

	float length() const {
		return std::sqrt(x * x + y * y + z * z);
	}

	float length2d() const {
		return std::sqrt(x * x + y * y);
	}

	Vector3 limit(float limit)
	{
		Vector3 T;
		if (x > limit)//|| In.x < -limit)
		{
			T.x = limit;
		}
		else
		{
			if (x < 0 - limit)
			{
				T.x = -limit;
			}
			else
			{
				T.x = x;
			}
		}

		if (y > limit)//|| In.y < -limit)
		{
			T.y = limit;
		}
		else
		{
			if (y < -limit)
			{
				T.y = 0 - limit;
			}
			else
			{
				T.y = y;
			}
		}

		T.z = z;
		return T;
	}

	Vector3 world_to_screen(view_matrix_t matrix) const {
		float _x = matrix[0][0] * x + matrix[0][1] * y + matrix[0][2] * z + matrix[0][3];
		float _y = matrix[1][0] * x + matrix[1][1] * y + matrix[1][2] * z + matrix[1][3];
		float w = matrix[3][0] * x + matrix[3][1] * y + matrix[3][2] * z + matrix[3][3];

		float inv_w = 1.f / w;
		_x *= inv_w;
		_y *= inv_w;

		float x = g::gameBounds.right * .5f;
		float y = g::gameBounds.bottom * .5f;

		x += 0.5f * _x * g::gameBounds.right + 0.5f;
		y -= 0.5f * _y * g::gameBounds.bottom + 0.5f;

		return { x, y, w };
	}

	float calculate_distance(const Vector3& point) const {
		float dx = point.x - x;
		float dy = point.y - y;
		float dz = point.z - z;

		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	// struct data
	float x, y, z;
};

inline constexpr Vector3 CalculateAngle(
	const Vector3& localPosition,
	const Vector3& enemyPosition,
	const Vector3& viewAngles) noexcept
{
	return ((enemyPosition - localPosition).ToAngle() - viewAngles);
};

inline Vector3 clampAngles(Vector3 angles) {
	if (angles.x > 89.0f && angles.x <= 180.0f) {
		angles.x = 89.0f;
	}

	if (angles.x > 180.0f) {
		angles.x -= 360.0f;
	}

	if (angles.x < -89.0f) {
		angles.x = -89.0f;
	}

	if (angles.y > 180.0f) {
		angles.y -= 360.0f;
	}

	if (angles.y < -180.0f) {
		angles.y += 360.0f;
	}
	angles.z = 0;

	return angles;
};

inline Vector3 normalizeAngles(Vector3 angle) {
	while (angle.x > 180.f)
		angle.x -= 360.0f;

	while (angle.x < -180.0f)
		angle.x += 360.0f;

	while (angle.y > 180.0f)
		angle.y -= 360.0f;

	while (angle.y < -180.0f)
		angle.y += 360.0f;

	return angle;
};

inline Vector3 calculateBestAngle(Vector3 angle, float& configFov) {
	Vector3 newAngle = 0;

	float fov = std::hypot(angle.x, angle.y);

	if (fov <= configFov) {
		configFov = fov;
		newAngle = angle;
	}
	return newAngle;
}

uintptr_t TraceAddress(uintptr_t BaseAddress, std::vector<unsigned long> Offsets)
{
	uintptr_t Address = 0;

	if (Offsets.size() == 0)
		return BaseAddress;
	Address = process->read<uintptr_t>(BaseAddress);
	if (!Address)
		return 0;

	for (int i = 0; i < Offsets.size() - 1; i++)
	{
		Address = process->read<uintptr_t>(Address + Offsets[i]);
		if (!Address)
			return 0;
	}
	return Address == 0 ? 0 : Address + Offsets[Offsets.size() - 1];
}

int localTeam;
json Offset;

void Triggerbot()
{
	uintptr_t  player = process->read<uintptr_t>(cs2_module_client.base + Offset["client_dll"]["data"]["dwLocalPlayerPawn"]["value"]);//client_dll::dwLocalPlayerPawn);
	unsigned long   iIDEntIndex = process->read<unsigned long >(player + C_CSPlayerPawnBase::m_iIDEntIndex);
	//std::cout << "\n " << iIDEntIndex;
	if (iIDEntIndex == -1)
		return;

	const uintptr_t ListEntry = TraceAddress(cs2_module_client.base +/*client_dll::dwEntityList*/ Offset["client_dll"]["data"]["dwEntityList"]["value"], { 0x8 * (iIDEntIndex >> 9) + 0x10,0x0 });
	if (ListEntry == 0)
		return;

	uintptr_t PawnAddress = process->read<uintptr_t>(ListEntry + 0x78 * (iIDEntIndex & 0x1FF));
	if (!PawnAddress)
		return;

	bool AllowShoot = false;

	//std::cout << "\n " << process->read<int>(PawnAddress + C_BaseEntity::m_iHealth);

	if (!AimTeam)
		AllowShoot = process->read<int>(PawnAddress + C_BaseEntity::m_iTeamNum) != localTeam && process->read<int>(PawnAddress + C_BaseEntity::m_iHealth) > 0;
	else
		AllowShoot = process->read<int>(PawnAddress + C_BaseEntity::m_iHealth) > 0;

	if (!AllowShoot)
		return;

	static std::chrono::time_point LastTimePoint = std::chrono::steady_clock::now();
	auto CurTimePoint = std::chrono::steady_clock::now();
	if (CurTimePoint - LastTimePoint >= std::chrono::milliseconds(TriggerDelay_ms/*ms TriggerDelay*/))
	{
		const bool isAlreadyShooting = GetAsyncKeyState(VK_LBUTTON) < 0;
		if (!isAlreadyShooting)
		{
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
		}

		LastTimePoint = CurTimePoint;
	}

	//mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
	//mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
	return;
}

void Bunnyhop(uintptr_t player)
{
	const bool hasFlagInAir = process->read<int>(player + C_BaseEntity::m_fFlags) & (1 << 0);
	if (GetAsyncKeyState(VK_SPACE) && hasFlagInAir)
	{
		process->write(cs2_module_client.base + Offset["client_dll"]["data"]["dwForceJump"]["value"] /*client_dll::dwForceJump*/, 65537);
	}
	else if (GetAsyncKeyState(VK_SPACE) && !hasFlagInAir)
	{
		process->write(cs2_module_client.base + Offset["client_dll"]["data"]["dwForceJump"]["value"], 256);
		process->write(cs2_module_client.base + Offset["client_dll"]["data"]["dwForceJump"]["value"], 65537);
		process->write(cs2_module_client.base + Offset["client_dll"]["data"]["dwForceJump"]["value"], 256);
	}
	else
	{
		process->write(cs2_module_client.base + Offset["client_dll"]["data"]["dwForceJump"]["value"], 256);
	}
}

Vector3 aimBot(C_UTL_VECTOR aimPunchCache, int getShotsFired, Vector3& cameraPos, float& fov, bool isHot, Vector3 baseViewAngles, uintptr_t boneArray) {
	Vector3 aimPos;
	Vector3 newAngle;

	aimPos = process->read<Vector3>(boneArray + AimBones/*head*/ * 32);
	//aimPos.Print();
	const Vector3 angle = CalculateAngle(cameraPos, aimPos, baseViewAngles);

	newAngle = calculateBestAngle(angle, fov/*fov*/);

	if (newAngle.IsZero()) {
		return 0;
	}

	if (isHot) {
		if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
			if (brecoilControl)
			{
				Vector3 aimPunchAngle = process->read<Vector3>(aimPunchCache.data + (aimPunchCache.count - 1) * sizeof(Vector3));

				if (getShotsFired > 1) {
					newAngle.x = newAngle.x - aimPunchAngle.x * 2.f;
					newAngle.y = newAngle.y - aimPunchAngle.y * 2.f;
				}
			}

			newAngle = clampAngles(newAngle);
			//if (AimSmoothing != 0)
			//	newAngle = newAngle.limit(AimSmoothing);

			if (AimSmoothing != 0)
				newAngle = newAngle / AimSmoothing;

			if (!newAngle.IsZero())
				return  newAngle + baseViewAngles;//baseViewAngles;
			//Triggerbot();
		}
	}
	else {
		if (getShotsFired > 1) {
			Vector3 aimPunchAngle = process->read<Vector3>(aimPunchCache.data + (aimPunchCache.count - 1) * sizeof(Vector3));
			newAngle.x = newAngle.x - aimPunchAngle.x * 2.f;
			newAngle.y = newAngle.y - aimPunchAngle.y * 2.f;
			//newAngle = clampAngles(newAngle);

			//newAngle.x = newAngle.x / AimSmoothing;//smoothing
			//newAngle.y = newAngle.y / AimSmoothing;//smoothing

			if (AimSmoothing != 0)
				newAngle = newAngle.limit(AimSmoothing);

			return newAngle + baseViewAngles;
		}
	}
	return 0;
}

void recoilControl(Vector3& ViewAngle, C_UTL_VECTOR aimPunchCache, int getShotsFired, DWORD_PTR baseViewAnglesAddy) {
	static Vector3 oldPunch;
	if (getShotsFired > 1) {
		Vector3 aimPunchAngle = process->read<Vector3>(aimPunchCache.data + (aimPunchCache.count - 1) * sizeof(Vector3));
		Vector3 recoilVector = {
			ViewAngle.x + oldPunch.x - aimPunchAngle.x * 2.f,
			ViewAngle.y + oldPunch.y - aimPunchAngle.y * 2.f
		};
		recoilVector = clampAngles(recoilVector);

		oldPunch = aimPunchAngle * 2.f;
		process->write<Vector3>(baseViewAnglesAddy, recoilVector);
	}
	else
		oldPunch = 0;
}

int main(int argc, char* argv[])
{
	SetConsoleTitleW(L"cs2-HalfPeople-esp");

	process = std::make_shared<pProcess>();

	while (!process->AttachProcessHj("cs2.exe"))
		std::this_thread::sleep_for(std::chrono::seconds(1));

	do {
		cs2_module_client = process->GetModule("client.dll");
		base_engine = process->GetModule("engine2.dll");
		if (cs2_module_client.base == 0 || base_engine.base == 0) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			std::cout << " Failed to find module client.dll/engine2.dll, waiting for the game to load it..." << std::endl;
		}
	} while (cs2_module_client.base == 0 || base_engine.base == 0);

	system("call cs2-dumper.exe -b .json --output offsets -o -v");

	std::ifstream rfileOffset;
	rfileOffset.open("offsets\\offsets.json");
	if (rfileOffset.good())
	{
		std::cout << "\n Read Offsets.json";
		rfileOffset >> Offset;
	}
	else
		std::cout << "\n Read Offsets.json Error";

	rfileOffset.close();

	const uintptr_t buildNumber = process->read<uintptr_t>(base_engine.base + Offset["engine2_dll"]["data"]["dwBuildNumber"]["value"] /*engine2_dll::dwBuildNumber*/);
	std::cout << "\n build number : " << buildNumber;

	std::cout << " Make sure your game is in \"Full Screen Windowed\"" << std::endl;
	while (GetForegroundWindow() != process->hwnd_) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		process->UpdateHWND();
		ShowWindow(process->hwnd_, TRUE);
	}
	std::cout << "Creating window overlay" << std::endl;

	WNDCLASSEXA wc = { 0 };
	wc.cbSize = sizeof(WNDCLASSEXA);
	wc.lpfnWndProc = WndProc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hbrBackground = WHITE_BRUSH;
	wc.hInstance = reinterpret_cast<HINSTANCE>(GetWindowLongA(process->hwnd_, (-6))); // GWL_HINSTANCE));
	wc.lpszMenuName = " ";
	wc.lpszClassName = " ";

	RegisterClassExA(&wc);

	GetClientRect(process->hwnd_, &g::gameBounds);

	// Create the window
	HINSTANCE hInstance = NULL;
	HWND hWnd = CreateWindowExA(WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW, " ", "cs2-HalfPeople-esp", WS_POPUP,
		g::gameBounds.left, g::gameBounds.top, g::gameBounds.right - g::gameBounds.left, g::gameBounds.bottom + g::gameBounds.left, NULL, NULL, hInstance, NULL); // NULL, NULL);

	if (hWnd == NULL)
		return 0;

	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	ShowWindow(hWnd, TRUE);
	//SetActiveWindow(hack::process->hwnd_);
	std::thread T(USETLOOP);
	// Message loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		//std::thread T(FunctionT);
		if (GetAsyncKeyState(VK_END) & 0x8000) break;

		TranslateMessage(&msg);
		DispatchMessage(&msg);

		//T.join();
		//std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	DeleteDC(g::hdcBuffer);
	DeleteObject(g::hbmBuffer);

	DestroyWindow(hWnd);

	process->Close();

	system("pause");

	return 0;
}
uintptr_t entity_list;
uintptr_t localpCSPlayerPawn;
uintptr_t localPlayerController;

//Vector3 TEST_CAMEPOS, TEST_CAMEANGLE, TEST_VIEWANGLE;

#define MenuOffset 100
#define MenuItemSelectColor RGB(100,200,200)
#define MenuItemColor RGB(200,200,200)
#define MenuItemMaxIndex 22
#define MenuTitleColor RGB(195,125,255)
int ApplayItem = false;
bool isDrawMenu = false;

void MenuTitle(int& MenuTitleIndexOffset, int index, const char* Text)
{
	render::RenderText(g::hdcBuffer, 0, MenuOffset + 5 + MenuTitleIndexOffset + TextSize * index, Text, MenuTitleColor, TextSize + 5);
	MenuTitleIndexOffset += (TextSize + 15);
}
bool MenuItem(int Offset, int& SelectIndex, int& index, const char* text)
{
	bool R = SelectIndex == index;
	render::RenderText(g::hdcBuffer, 0, MenuOffset + 5 + Offset + TextSize * index, text, (R ? MenuItemSelectColor : MenuItemColor), TextSize);
	index++;
	return R && ApplayItem > 0;
}

void DrawMenu()
{
	int MenuTitleIndexOffset = 0;
	static int MenuSelectIndex = 0;
	int index = 0;
	static int MenuHight = 0;
	if (GetKeyState(VK_UP) & 0x8000)
	{
		if (MenuSelectIndex - 1 < 0)
			MenuSelectIndex = MenuItemMaxIndex;
		MenuSelectIndex--;
		Beep(500, 90);
	}

	if (GetKeyState(VK_DOWN) & 0x8000)
	{
		if (MenuSelectIndex + 1 > MenuItemMaxIndex)
			MenuSelectIndex = 0;
		MenuSelectIndex++;
		Beep(500, 90);
	}
	ApplayItem = 0;
	if (GetKeyState(VK_RIGHT) & 0x8000)
	{
		ApplayItem = 1;

		Beep(600, 90);
	}

	if (GetKeyState(VK_LEFT) & 0x8000)
	{
		ApplayItem = 2;

		Beep(600, 90);
	}

	render::DrawFilledBox(g::hdcBuffer, 0, MenuOffset, TextSize * 15, MenuHight, RGB(0, 0, 0));

	SetBkColor(g::hdcBuffer, RGB(0, 0, 0));

	MenuTitle(MenuTitleIndexOffset, index, "F1 - Hide Menu");
	MenuTitle(MenuTitleIndexOffset, index, "-----Function-----");

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("RecoilControl -> ").append((brecoilControl ? "true" : "false")).c_str()))
	{
		brecoilControl = !brecoilControl;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("AimBot -> ").append((AimBot ? "true" : "false")).c_str()))
	{
		AimBot = !AimBot;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("Triggerbot -> ").append((bTriggerbot ? "true" : "false")).c_str()))
	{
		bTriggerbot = !bTriggerbot;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("BunnyHop -> ").append((bBunnyHop ? "true" : "false")).c_str()))
	{
		bBunnyHop = !bBunnyHop;
	}

	MenuTitle(MenuTitleIndexOffset, index, "-------ESP-------");

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("Draw ESP -> ").append((DrawESP ? "true" : "false")).c_str()))
	{
		DrawESP = !DrawESP;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("Draw ESP Line -> ").append((DrawESPLine ? "true" : "false")).c_str()))
	{
		DrawESPLine = !DrawESPLine;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("ShowViewAngle -> ").append((ShowViewAngle ? "true" : "false")).c_str()))
	{
		ShowViewAngle = !ShowViewAngle;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("show_extra -> ").append((show_extra ? "true" : "false")).c_str()))
	{
		show_extra = !show_extra;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("TextSize -> ").append(std::to_string(TextSize)).c_str()))
	{
		MenuHight = 0;
		if (ApplayItem == 1)
		{
			TextSize++;
		}
		else
			if (TextSize - 1 > 0)
				TextSize--;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("ShowBoneDistance -> ").append(std::to_string(ShowBoneDistance)).c_str()))
	{
		if (ApplayItem == 1)
		{
			ShowBoneDistance += 10;
		}
		else
			if (ShowBoneDistance - 10 > 0)
				ShowBoneDistance -= 10;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("flag_render_distance -> ").append(std::to_string(flag_render_distance)).c_str()))
	{
		if (ApplayItem == 1)
		{
			flag_render_distance += 10;
		}
		else
			if (flag_render_distance - 10 > 0)
				flag_render_distance -= 10;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("render_distance -> ").append(std::to_string(render_distance)).c_str()))
	{
		if (ApplayItem == 1)
		{
			render_distance += 10;
		}
		else
			if (render_distance - 10 > -1)
				render_distance -= 10;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("Draw Boom MaxSize -> ").append(std::to_string(BoomSize)).c_str()))
	{
		if (ApplayItem == 1)
		{
			BoomSize += 10;
		}
		else
			if (BoomSize - 10 > -1)
				BoomSize -= 10;

		render::DrawFilledBox(
			g::hdcBuffer,
			25,
			25,
			BoomSize,
			BoomSize * 1.4,
			RGB(175, 75, 75)
		);
	}

	MenuTitle(MenuTitleIndexOffset, index, "-----AimBot-----");

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("AimTeam -> ").append((AimTeam ? "true" : "false")).c_str()))
	{
		AimTeam = !AimTeam;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("AimBotHot -> ").append((AimBotHot ? "true" : "false")).c_str()))
	{
		AimBotHot = !AimBotHot;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("OnlyAttackVisible -> ").append((OnlyAttackVisible ? "true" : "false")).c_str()))
	{
		OnlyAttackVisible = !OnlyAttackVisible;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("AimBot FOV -> ").append(std::to_string(FOV)).c_str()))
	{
		if (ApplayItem == 1)
		{
			FOV++;
		}
		else
			if (FOV - 1 > 0)
				FOV -= 1;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("AimBot Smoothing -> ").append(std::to_string(AimSmoothing)).c_str()))
	{
		if (ApplayItem == 1)
		{
			AimSmoothing += 0.5;
		}
		else
			if (AimSmoothing - 0.5 > 0.5)
				AimSmoothing -= 0.5;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("AimBot AimBone -> ").append(boneGroups::mid_Name.find(AimBones)->second).c_str()))
	{
		if (ApplayItem == 1)
		{
			for (size_t i = 0; i < boneGroups::mid.size(); i++)
			{
				if (boneGroups::mid[i] == AimBones)
				{
					if (i + 1 < boneGroups::mid.size())
					{
						AimBones = boneGroups::mid.at(i + 1);
					}
					else
						AimBones = boneGroups::mid.front();

					i = boneGroups::mid.size();
				}
			}
		}
		else
			if (AimSmoothing - 1 > -1)
				for (size_t i = 0; i < boneGroups::mid.size(); i++)
				{
					if (boneGroups::mid[i] == AimBones)
					{
						if (i - 1 > 0)
						{
							AimBones = boneGroups::mid.at(i - 1);
						}
						else
							AimBones = boneGroups::mid.back();

						i = boneGroups::mid.size();
					}
				}
	}

	MenuTitle(MenuTitleIndexOffset, index, "-----Misc-----");

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("Menu Draw Border -> ").append((MenuDrawBorder ? "true" : "false")).c_str()))
	{
		MenuDrawBorder = !MenuDrawBorder;
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, std::string("TriggerDelay -> ").append(std::to_string(TriggerDelay_ms)).append(" ms").c_str()))
	{
		if (ApplayItem == 1)
		{
			TriggerDelay_ms++;
		}
		else
			if (TriggerDelay_ms - 1 > 0)
				TriggerDelay_ms -= 1;
	} // <<

	MenuTitle(MenuTitleIndexOffset, index, "-----File-----");

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, "Save Setting"))
	{
		// first, create a file instance
		mINI::INIFile file("Configuration.ini");
		mINI::INIStructure ini;
		file.read(ini);
		ini["Function"]["brecoilControl"] = std::to_string(brecoilControl);
		ini["Function"]["AimBot"] = std::to_string(AimBot);
		ini["Function"]["DrawESP"] = std::to_string(DrawESP);
		ini["Function"]["bTriggerbot"] = std::to_string(bTriggerbot);
		ini["Function"]["bBunnyHop"] = std::to_string(bBunnyHop);

		ini["AimBot"]["AimBotHot"] = std::to_string(AimBotHot);
		ini["AimBot"]["AimTeam"] = std::to_string(AimTeam);
		ini["AimBot"]["AimBone"] = std::to_string(AimBones);
		ini["AimBot"]["FOV"] = std::to_string(FOV);
		ini["AimBot"]["Smoothing"] = std::to_string((int)AimSmoothing);
		ini["AimBot"]["OnlyAttackVisible"] = std::to_string(OnlyAttackVisible);

		ini["TriggerBot"]["TriggerDelay"] = std::to_string(TriggerDelay_ms);
		ini["Menu"]["MenuDrawBorder"] = std::to_string(MenuDrawBorder);

		ini["ESP"]["show_extra"] = std::to_string(show_extra);
		ini["ESP"]["ShowViewAngle"] = std::to_string(ShowViewAngle);
		ini["ESP"]["TextSize"] = std::to_string(TextSize);
		ini["ESP"]["ShowBoneDistance"] = std::to_string(ShowBoneDistance);
		ini["ESP"]["flag_render_distance"] = std::to_string(flag_render_distance);
		ini["ESP"]["render_distance"] = std::to_string(render_distance);
		ini["ESP"]["BoomSize"] = std::to_string(BoomSize);
		ini["ESP"]["DrawESPLine"] = std::to_string(DrawESPLine);
		std::cout << "\n Save :" << file.write(ini);
	}

	if (MenuItem(MenuTitleIndexOffset, MenuSelectIndex, index, "Load Setting"))
	{
		mINI::INIFile file("Configuration.ini");
		mINI::INIStructure ini;
		std::cout << "\n Load :" << file.read(ini);
		brecoilControl = atoi(ini["Function"]["brecoilControl"].c_str());
		AimBot = atoi(ini["Function"]["AimBot"].c_str());
		DrawESP = atoi(ini["Function"]["DrawESP"].c_str());
		bTriggerbot = atoi(ini["Function"]["bTriggerbot"].c_str());
		bBunnyHop = atoi(ini["Function"]["bBunnyHop"].c_str());

		AimTeam = atoi(ini["AimBot"]["AimTeam"].c_str());
		AimBotHot = atoi(ini["AimBot"]["AimBotHot"].c_str());
		FOV = atof(ini["AimBot"]["FOV"].c_str());
		AimSmoothing = atoi(ini["AimBot"]["Smoothing"].c_str());
		AimBones = atoi(ini["AimBot"]["AimBone"].c_str());
		OnlyAttackVisible = atoi(ini["AimBot"]["OnlyAttackVisible"].c_str());

		TriggerDelay_ms = atoi(ini["TriggerBot"]["TriggerDelay"].c_str());

		MenuDrawBorder = atoi(ini["Menu"]["MenuDrawBorder"].c_str());

		show_extra = atoi(ini["ESP"]["show_extra"].c_str());
		TextSize = atoi(ini["ESP"]["TextSize"].c_str());
		ShowViewAngle = atoi(ini["ESP"]["ShowViewAngle"].c_str());
		ShowBoneDistance = atoi(ini["ESP"]["ShowBoneDistance"].c_str());
		flag_render_distance = atoi(ini["ESP"]["flag_render_distance"].c_str());
		render_distance = atoi(ini["ESP"]["render_distance"].c_str());
		BoomSize = atoi(ini["ESP"]["BoomSize"].c_str());
		DrawESPLine = atoi(ini["ESP"]["DrawESPLine"].c_str());
	}

	if (MenuHight == 0)
		MenuHight = MenuTitleIndexOffset + (index + 1) * TextSize;

	SetBkColor(g::hdcBuffer, RGB(255, 255, 255));
}

void DrawAimBotFOV(float Fov)
{
	float Radius = tan(FOV / 180.f * std::numbers::pi_v<float> / 2.f) / tan(Fov / 180.f * std::numbers::pi_v<float> / 2.f) * g::gameBounds.right;
	render::DrawCircle(g::hdcBuffer, g::gameBounds.right / 2, g::gameBounds.bottom / 2, Radius, RGB(134, 133, 218));

	float Length;
	float radian;
	Vector3 LineEndPoint[2];
	Vector3 Pos;// = Gui.Window.Size / 2;
	Pos.x = g::gameBounds.right / 2;
	Pos.y = g::gameBounds.bottom / 2;

	radian = (Fov / 2) * std::numbers::pi_v<float> / 180;

	LineEndPoint[0].y = Pos.y - 60/*FovLineSize */;
	LineEndPoint[1].y = LineEndPoint[0].y;

	Length = 60/*FovLineSize */ * tan(radian);

	LineEndPoint[0].x = Pos.x - Length;
	LineEndPoint[1].x = Pos.x + Length;

	//Gui.Line(Pos, LineEndPoint[0], Color, 1.5);
	//Gui.Line(Pos, LineEndPoint[1], Color, 1.5);

	render::RenderLine(g::hdcBuffer, Pos.x, Pos.y, LineEndPoint[0].x, LineEndPoint[0].y, RGB(134, 133, 218));
	render::RenderLine(g::hdcBuffer, Pos.x, Pos.y, LineEndPoint[1].x, LineEndPoint[1].y, RGB(134, 133, 218));
}

void loop()
{
	static int indirectUpdate = 0;
	static uintptr_t localPlayer;
	if (GetKeyState(VK_F1) & 0x8000)
	{
		Sleep(100);
		isDrawMenu = !isDrawMenu;
	}
	if (isDrawMenu)
		DrawMenu();
	else
		render::RenderText(g::hdcBuffer, 0, MenuOffset, "F1 - Show Menu", RGB(200, 200, 200), TextSize);

	//render::RenderText(g::hdcBuffer, 0, MenuOffset + 500, std::string("TEST_CAMEPOS : ").append(std::to_string(TEST_CAMEPOS.x)).append(",").append(std::to_string(TEST_CAMEPOS.y)).append(",").append(std::to_string(TEST_CAMEPOS.z)).c_str(), RGB(200, 0, 0), TextSize);
	//render::RenderText(g::hdcBuffer, 0, MenuOffset + 500 + TextSize, std::string("TEST_CAMEANGLE : ").append(std::to_string(TEST_CAMEANGLE.x)).append(",").append(std::to_string(TEST_CAMEANGLE.y)).append(",").append(std::to_string(TEST_CAMEANGLE.z)).c_str(), RGB(200, 0, 0), TextSize);
	//render::RenderText(g::hdcBuffer, 0, MenuOffset + 500 + TextSize * 2, std::string("TEST_VIEWANGLE : ").append(std::to_string(TEST_VIEWANGLE.x)).append(",").append(std::to_string(TEST_VIEWANGLE.y)).append(",").append(std::to_string(TEST_VIEWANGLE.z)).c_str(), RGB(200, 0, 0), TextSize);
	//localPlayerController = process->read<uintptr_t>(cs2_module_client.base + client_dll::dwLocalPlayerController);

	static float PlayerFov;
	if (indirectUpdate == 0)
	{
		indirectUpdate++;
		entity_list = process->read<uintptr_t>(cs2_module_client.base + Offset["client_dll"]["data"]["dwEntityList"]["value"] /*client_dll::dwEntityList*/);

		//localPlayer = process->read<uintptr_t>(localPlayerController + CCSPlayerController::m_hPlayerPawn);
		localPlayer = process->read<uintptr_t>(cs2_module_client.base + Offset["client_dll"]["data"]["dwLocalPlayerPawn"]["value"] /*client_dll::dwLocalPlayerPawn*/);
		if (!localPlayer)
			return;

		localTeam = process->read<int>(localPlayer + C_BaseEntity::m_iTeamNum);

		if (ShowAimBotFov)
		{
			const DWORD64 CameraServices = process->read<DWORD64>(localPlayer + C_BasePlayerPawn::m_pCameraServices);
			if (CameraServices)
				PlayerFov = process->read<int>(CameraServices + CCSPlayerBase_CameraServices::m_iFOVStart);
		}
	}
	else if (indirectUpdate > 10)
	{
		indirectUpdate = 0;
	}
	else
	{
		indirectUpdate++;
	}

	if (ShowAimBotFov)
		DrawAimBotFOV(PlayerFov);

	if (!DrawESP)
		return;
	const view_matrix_t view_matrix = process->read<view_matrix_t>(cs2_module_client.base + Offset["client_dll"]["data"]["dwViewMatrix"]["value"] /*client_dll::dwViewMatrix*/);

	int playerIndex = 0;
	uintptr_t list_entry;

	static uintptr_t localCGameSceneNode;
	static bool c4IsPlanted;
	if (indirectUpdate == 0)
	{
		localCGameSceneNode = process->read<uintptr_t>(localPlayer + C_BaseEntity::m_pGameSceneNode);
		c4IsPlanted = process->read<bool>(cs2_module_client.base + Offset["client_dll"]["data"]["dwPlantedC4"]["value"] /*client_dll::dwPlantedC4*/ - 0x8);
	}

	const Vector3 localOrigin = process->read<Vector3>(localCGameSceneNode + CGameSceneNode::m_vecOrigin);

	render::RenderText(g::hdcBuffer, 50, 50, "HalfPeople CSGO 2 GameHack Test", RGB(200, 200, 200), 30);
	if (c4IsPlanted)
	{
		const uintptr_t planted_c4 = process->read<uintptr_t>(process->read<uintptr_t>(cs2_module_client.base + Offset["client_dll"]["data"]["dwPlantedC4"]["value"] /*client_dll::dwPlantedC4*/));

		const uintptr_t c4Node = process->read<uintptr_t>(planted_c4 + C_BaseEntity::m_pGameSceneNode);

		const Vector3 c4Origin = process->read<Vector3>(c4Node + CGameSceneNode::m_vecAbsOrigin);

		const Vector3 c4ScreenPos = c4Origin.world_to_screen(view_matrix);

		if (c4ScreenPos.z >= 0.01f) {
			float c4Distance = localOrigin.calculate_distance(c4Origin);
			float c4RoundedDistance = std::round(c4Distance / 500.f);

			float height = BoomSize - c4RoundedDistance;
			float width = height * 1.4f;

			if (DrawESPLine)
				render::RenderLine(g::hdcBuffer, 0, 0, c4ScreenPos.x, c4ScreenPos.y, RGB(175, 75, 75));

			render::DrawFilledBox(
				g::hdcBuffer,
				c4ScreenPos.x - (width / 2),
				c4ScreenPos.y - (height / 2),
				width,
				height,
				RGB(175, 75, 75)
			);

			render::RenderText(
				g::hdcBuffer,
				c4ScreenPos.x + (width / 2 + 5),
				c4ScreenPos.y,
				"C4",
				RGB(75, 75, 175),
				TextSize
			);
		}
	}
	while (true)
	{
		playerIndex++;
		list_entry = process->read<uintptr_t>(entity_list + (8 * (playerIndex & 0x7FFF) >> 9) + 16);
		if (!list_entry)
			break;

		const uintptr_t player = process->read<uintptr_t>(list_entry + 120 * (playerIndex & 0x1FF));
		if (!player)
			continue;

		const int playerTeam = process->read<int>(player + C_BaseEntity::m_iTeamNum);

		const std::uint32_t playerPawn = process->read<std::uint32_t>(player + CCSPlayerController::m_hPlayerPawn);

		const uintptr_t list_entry2 = process->read<uintptr_t>(entity_list + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);
		if (!list_entry2)
			continue;

		const uintptr_t pCSPlayerPawn = process->read<uintptr_t>(list_entry2 + 120 * (playerPawn & 0x1FF));

		const int playerArmor = process->read<int>(pCSPlayerPawn + C_CSPlayerPawnBase::m_ArmorValue);
		const int playerHealth = process->read<int>(pCSPlayerPawn + C_BaseEntity::m_iHealth);
		if (playerHealth <= 0 || playerHealth > 100)
			continue;

		if (pCSPlayerPawn == localPlayer)
			continue;

		std::string playerName = "Invalid Name";
		const DWORD64 playerNameAddress = process->read<DWORD64>(player + CCSPlayerController::m_sSanitizedPlayerName);
		if (playerNameAddress) {
			char buf[256];
			process->read_raw(playerNameAddress, buf, sizeof(buf));
			playerName = std::string(buf);
		}

		const uintptr_t CGameSceneNode = process->read<uintptr_t>(pCSPlayerPawn + C_BaseEntity::m_pGameSceneNode);
		const Vector3 origin = process->read<Vector3>(CGameSceneNode + CGameSceneNode::m_vecOrigin);
		const Vector3 head = { origin.x, origin.y, origin.z + 75.f };

		if (render_distance != -1 && (localOrigin - origin).length2d() > render_distance)
			continue;

		const Vector3 screenPos = origin.world_to_screen(view_matrix);
		if (screenPos.z < 0)
			continue;
		const Vector3 screenHead = head.world_to_screen(view_matrix);

		if (screenPos.z >= 0.01f) {
			const float height = screenPos.y - screenHead.y;
			const float width = height / 2.4f;
			const float head_height = (screenPos.y - screenHead.y) / 8;
			const float head_width = (height / 2.4f) / 4;

			float distance = localOrigin.calculate_distance(origin);
			int roundedDistance = std::round(distance / 10.f);

			render::DrawBorderBox(
				g::hdcBuffer,
				screenHead.x - width / 2,
				screenHead.y,
				width,
				height,
				(localTeam == playerTeam ? RGB(75, 175, 75) : RGB(175, 75, 75))
			);
			if (DrawESPLine)
				render::RenderLine(g::hdcBuffer, screenHead.x, screenHead.y + height, g::gameBounds.right / 2, g::gameBounds.bottom * 0.8, (localTeam == playerTeam ? RGB(75, 175, 75) : RGB(175, 75, 75)));

			render::DrawBorderBox(
				g::hdcBuffer,
				screenHead.x - (width / 2 + 10),
				screenHead.y + (height * (100 - playerArmor) / 100),
				2,
				height - (height * (100 - playerArmor) / 100),
				RGB(0, 185, 255)
			);

			render::DrawBorderBox(
				g::hdcBuffer,
				screenHead.x - (width / 2 + 5),
				screenHead.y + (height * (100 - playerHealth) / 100),
				2,
				height - (height * (100 - playerHealth) / 100),
				RGB(
					(255 - playerHealth),
					(55 + playerHealth * 2),
					75
				)
			);

			render::RenderText(
				g::hdcBuffer,
				screenHead.x + (width / 2 + 5),
				screenHead.y,
				playerName.c_str(),
				RGB(75, 75, 175),
				TextSize
			);

			/**
			* I know is not the best way but a simple way to not saturate the screen with a ton of information
			*/
			if (roundedDistance > flag_render_distance)
				continue;

			render::RenderText(
				g::hdcBuffer,
				screenHead.x + (width / 2 + 5),
				screenHead.y + TextSize,
				(std::to_string(playerHealth) + "hp").c_str(),
				RGB(
					(255 - playerHealth),
					(55 + playerHealth * 2),
					75
				),
				TextSize
			);

			render::RenderText(
				g::hdcBuffer,
				screenHead.x + (width / 2 + 5),
				screenHead.y + TextSize * 2,
				(std::to_string(playerArmor) + "armor").c_str(),
				RGB(
					(255 - playerArmor),
					(55 + playerArmor * 2),
					75
				),
				TextSize
			);

			render::RenderText(
				g::hdcBuffer,
				screenHead.x + (width / 2 + 5),
				screenHead.y + TextSize * 8,
				(std::to_string((int)distance) + "cm").c_str(),
				RGB(
					(255 - playerArmor),
					(55 + playerArmor * 2),
					75
				),
				TextSize
			);

			if (show_extra)
			{
				/*
				* Reading values for extra flags is now seperated from the other reads
				* This removes unnecessary memory reads, improving performance when not showing extra flags
				*/
				const bool isDefusing = process->read<bool>(pCSPlayerPawn + C_CSPlayerPawnBase::m_bIsDefusing);
				const uintptr_t playerMoneyServices = process->read<uintptr_t>(player + CCSPlayerController::m_pInGameMoneyServices);
				const int32_t money = process->read<int32_t>(playerMoneyServices + CCSPlayerController_InGameMoneyServices::m_iAccount);
				const float flashAlpha = process->read<float>(pCSPlayerPawn + C_CSPlayerPawnBase::m_flFlashOverlayAlpha);

				const auto clippingWeapon = process->read<std::uint64_t>(pCSPlayerPawn + C_CSPlayerPawnBase::m_pClippingWeapon);
				const auto weaponData = process->read<std::uint64_t>(clippingWeapon + 0x360);
				const auto weaponNameAddress = process->read<std::uint64_t>(weaponData + CCSWeaponBaseVData::m_szName);
				std::string weaponName = "Invalid Weapon Name";

				if (!weaponNameAddress) {
					weaponName = "Invalid Weapon Name";
				}
				else {
					char buf[32];
					process->read_raw(weaponNameAddress, buf, sizeof(buf));
					weaponName = std::string(buf);
					if (weaponName.compare(0, 7, "weapon_") == 0)
						weaponName = weaponName.substr(7, weaponName.length()); // Remove weapon_ prefix
				}

				render::RenderText(
					g::hdcBuffer,
					screenHead.x + (width / 2 + 5),
					screenHead.y + TextSize * 3,
					weaponName.c_str(),
					RGB(75, 75, 175),
					TextSize
				);

				render::RenderText(
					g::hdcBuffer,
					screenHead.x + (width / 2 + 5),
					screenHead.y + TextSize * 4,
					(std::to_string(roundedDistance) + "m away").c_str(),
					RGB(75, 75, 175),
					TextSize
				);

				render::RenderText(
					g::hdcBuffer,
					screenHead.x + (width / 2 + 5),
					screenHead.y + TextSize * 5,
					("$" + std::to_string(money)).c_str(),
					RGB(0, 125, 0),
					TextSize
				);

				if (flashAlpha > 100)
				{
					render::RenderText(
						g::hdcBuffer,
						screenHead.x + (width / 2 + 5),
						screenHead.y + TextSize * 6,
						"Player is flashed",
						RGB(75, 75, 175),
						TextSize
					);
				}

				if (isDefusing)
				{
					const std::string defuText = "Player is defusing";
					render::RenderText(
						g::hdcBuffer,
						screenHead.x + (width / 2 + 5),
						screenHead.y + 60,
						defuText.c_str(),
						RGB(75, 75, 175),
						TextSize
					);
				}
			}

			if (distance > ShowBoneDistance)
			{
				continue;
			}

			// const uintptr_t CGameSceneNode = process->read<uintptr_t>( pCSPlayerPawn + C_BaseEntity::m_pGameSceneNode);
			const uintptr_t boneArray = process->read<uintptr_t>(CGameSceneNode + CSkeletonInstance::m_modelState + CGameSceneNode::m_vecOrigin);

			Vector3 head_ = process->read<Vector3>(boneArray + 6 * 32);
			Vector3 cou = process->read<Vector3>(boneArray + 5 * 32);
			Vector3 shoulderR = process->read<Vector3>(boneArray + 8 * 32);
			Vector3 shoulderL = process->read<Vector3>(boneArray + 13 * 32);
			Vector3 brasR = process->read<Vector3>(boneArray + 9 * 32);
			Vector3 brasL = process->read<Vector3>(boneArray + 14 * 32);
			Vector3 handR = process->read<Vector3>(boneArray + 11 * 32);
			Vector3 handL = process->read<Vector3>(boneArray + 16 * 32);
			Vector3 cock = process->read<Vector3>(boneArray + 0 * 32);
			Vector3 kneesR = process->read<Vector3>(boneArray + 23 * 32);
			Vector3 kneesL = process->read<Vector3>(boneArray + 26 * 32);
			Vector3 feetR = process->read<Vector3>(boneArray + 24 * 32);
			Vector3 feetL = process->read<Vector3>(boneArray + 27 * 32);

			Vector3 Ahead;
			Vector3 Acou;
			Vector3 AshoulderR;
			Vector3 AshoulderL;
			Vector3 AbrasR;
			Vector3 AbrasL;
			Vector3 AhandR;
			Vector3 AhandL;
			Vector3 Acock;
			Vector3 AkneesR;
			Vector3 AkneesL;
			Vector3 AfeetR;
			Vector3 AfeetL;

			Ahead = head_.world_to_screen(view_matrix);
			if (Ahead.z < 0)
				continue;

			if (ShowViewAngle)
			{
				Vector3 EyeAngle = process->read<Vector3>(player + C_CSPlayerPawnBase::m_angEyeAngles);
				//Vec2 StartPoint, EndPoint;
				Vector3 Temp;

				//StartPoint = head_;

				float LineLength = cos(EyeAngle.x * std::numbers::pi_v<float> / 180) * 25/*Length*/;

				Temp.x = head_.x + cos(EyeAngle.y * std::numbers::pi_v<float> / 180) * LineLength;
				Temp.y = head_.y + sin(EyeAngle.y * std::numbers::pi_v<float> / 180) * LineLength;
				Temp.z = head_.z - sin(EyeAngle.x * std::numbers::pi_v<float> / 180) * 25/*Length*/;

				Temp = Temp.world_to_screen(view_matrix);
				if (Temp.z > 0)
					render::RenderLine(g::hdcBuffer, Ahead.x, Ahead.y, Temp.x, Temp.y, RGB(0, 255, 0));
			}

			Acou = cou.world_to_screen(view_matrix);
			if (Acou.z < 0)
				continue;
			AshoulderR = shoulderR.world_to_screen(view_matrix);
			if (AshoulderR.z < 0)
				continue;
			AshoulderL = shoulderL.world_to_screen(view_matrix);
			if (AshoulderL.z < 0)
				continue;
			AbrasR = brasR.world_to_screen(view_matrix);
			if (AbrasR.z < 0)
				continue;
			AbrasL = brasL.world_to_screen(view_matrix);
			if (AbrasL.z < 0)
				continue;
			AhandL = handL.world_to_screen(view_matrix);
			if (AhandL.z < 0)
				continue;
			AhandR = handR.world_to_screen(view_matrix);
			if (AhandR.z < 0)
				continue;
			Acock = cock.world_to_screen(view_matrix);
			if (Acock.z < 0)
				continue;
			AkneesR = kneesR.world_to_screen(view_matrix);
			if (AkneesR.z < 0)
				continue;
			AkneesL = kneesL.world_to_screen(view_matrix);
			if (AkneesL.z < 0)
				continue;
			AfeetR = feetR.world_to_screen(view_matrix);
			if (AfeetR.z < 0)
				continue;
			AfeetL = feetL.world_to_screen(view_matrix);
			if (AfeetL.z < 0)
				continue;

			render::RenderLine(g::hdcBuffer, Ahead.x, Ahead.y, Acou.x, Acou.y, BonesColor);
			render::RenderLine(g::hdcBuffer, Acou.x, Acou.y, AshoulderR.x, AshoulderR.y, BonesColor);
			render::RenderLine(g::hdcBuffer, Acou.x, Acou.y, AshoulderL.x, AshoulderL.y, BonesColor);
			render::RenderLine(g::hdcBuffer, AbrasL.x, AbrasL.y, AshoulderL.x, AshoulderL.y, BonesColor);
			render::RenderLine(g::hdcBuffer, AbrasR.x, AbrasR.y, AshoulderR.x, AshoulderR.y, BonesColor);
			render::RenderLine(g::hdcBuffer, AbrasR.x, AbrasR.y, AhandR.x, AhandR.y, BonesColor);
			render::RenderLine(g::hdcBuffer, AbrasL.x, AbrasL.y, AhandL.x, AhandL.y, BonesColor);
			render::RenderLine(g::hdcBuffer, Acou.x, Acou.y, Acock.x, Acock.y, BonesColor);
			render::RenderLine(g::hdcBuffer, AkneesR.x, AkneesR.y, Acock.x, Acock.y, BonesColor);
			render::RenderLine(g::hdcBuffer, AkneesL.x, AkneesL.y, Acock.x, Acock.y, BonesColor);
			render::RenderLine(g::hdcBuffer, AkneesL.x, AkneesL.y, AfeetL.x, AfeetL.y, BonesColor);
			render::RenderLine(g::hdcBuffer, AkneesR.x, AkneesR.y, AfeetR.x, AfeetR.y, BonesColor);

			// Vector3 previous, current;
			//
			//
			// for (std::vector<int> currentGroup : boneGroups::allGroups) {
			//     previous = { 0,0,0 };
			//
			//     for (int currentBone : currentGroup) {
			//         current = process->read<Vector3>(boneArray + currentBone * 32);
			//
			//         if (previous.IsZero()) {
			//             previous = current;
			//             continue;
			//         }
			//
			//         Vector3 currentScreenPos = current.world_to_screen(view_matrix);
			//         Vector3 previousScreenPos = previous.world_to_screen(view_matrix);
			//
			//
			//        render::RenderLine(g::hdcBuffer, previousScreenPos.x, previousScreenPos.y, currentScreenPos.x, currentScreenPos.y, BonesColor);
			//
			//         previous = current;
			//     }
			// }
		}
	}
}

void FunctionT()
{
	if (bTriggerbot)
		Triggerbot();

	static int indirectUpdate = 0;
	static uintptr_t localPlayer;
	//if (!(brecoilControl || AimBot || entity_list || localPlayer))
	//{
	//	std::cout << "\n brecoilControl : " << brecoilControl;
	//	std::cout << "\n AimBot : " << AimBot;
	//	std::cout << "\n entity_list : " << entity_list;
	//	std::cout << "\n localPlayer : " << localPlayer;
	//	return;
	//}

	int playerIndex = -1;
	uintptr_t list_entry;
	static uintptr_t baseViewAnglesAddy;
	if (indirectUpdate == 0)
	{
		indirectUpdate++;
		localPlayer = process->read<uintptr_t>(cs2_module_client.base + Offset["client_dll"]["data"]["dwLocalPlayerPawn"]["value"] /*client_dll::dwLocalPlayerPawn*/);
		//std::cout << "\n " << process->read<int>(localPlayer + C_BaseEntity::m_iHealth);

		baseViewAnglesAddy = cs2_module_client.base + Offset["client_dll"]["data"]["dwViewAngles"]["value"] /*client_dll::dwViewAngles*/;
	}
	else if (indirectUpdate > 20)
		indirectUpdate = 0;
	else
		indirectUpdate++;

	if (bBunnyHop)
		Bunnyhop(localPlayer);

	Vector3 cameraPos = process->read<Vector3>(localPlayer + C_CSPlayerPawnBase::m_vecLastClipCameraPos);

	Vector3 baseViewAngles = process->read<Vector3>(cs2_module_client.base + Offset["client_dll"]["data"]["dwViewAngles"]["value"] /*client_dll::dwViewAngles*/);

	C_UTL_VECTOR aimPunchCache = process->read<C_UTL_VECTOR>(localPlayer + C_CSPlayerPawn::m_aimPunchCache);

	int shotsFired = process->read<int>(localPlayer + C_CSPlayerPawnBase::m_iShotsFired);

	float fov = FOV;

	Vector3 AimTarget;
	while (true)
	{
		playerIndex++;

		list_entry = process->read<uintptr_t>(entity_list + (8 * (playerIndex & 0x7FFF) >> 9) + 16);
		if (!list_entry)
			break;

		const uintptr_t player = process->read<uintptr_t>(list_entry + 120 * (playerIndex & 0x1FF));
		if (!player)
			continue;

		const int playerTeam = process->read<int>(player + C_BaseEntity::m_iTeamNum);
		if ((!AimTeam) && playerTeam == localTeam)
		{
			continue;
		}

		const std::uint32_t playerPawn = process->read<std::uint32_t>(player + CCSPlayerController::m_hPlayerPawn);

		const uintptr_t list_entry2 = process->read<uintptr_t>(entity_list + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);
		if (!list_entry2)
			continue;

		const uintptr_t pCSPlayerPawn = process->read<uintptr_t>(list_entry2 + 120 * (playerPawn & 0x1FF));
		if ((!pCSPlayerPawn) || pCSPlayerPawn == localPlayer)
			continue;

		if (process->read<int>(player + C_BaseEntity::m_lifeState))
			continue;

		const uintptr_t CGameSceneNode = process->read<uintptr_t>(pCSPlayerPawn + C_BaseEntity::m_pGameSceneNode);
		if (process->read<bool>(CGameSceneNode + CGameSceneNode::m_bDormant))
			return;

		const int playerHealth = process->read<int>(pCSPlayerPawn + C_BaseEntity::m_iHealth);
		if (playerHealth <= 0 || playerHealth > 100)
			continue;

		if (OnlyAttackVisible)
		{
			const int EntitySpottedState = process->read<DWORD_PTR>(pCSPlayerPawn + C_CSPlayerPawnBase::m_entitySpottedState + EntitySpottedState_t::m_bSpottedByMask);
			if (!(EntitySpottedState & (1 << localPlayer)))
				continue;
		}

		const uintptr_t boneArray = process->read<uintptr_t>(CGameSceneNode + CSkeletonInstance::m_modelState + CGameSceneNode::m_vecOrigin);

		if (AimBot)
		{
			Vector3 AimTargteBuffer = aimBot(aimPunchCache, shotsFired, cameraPos, fov, AimBotHot, baseViewAngles, boneArray);

			if (!AimTargteBuffer.IsZero())
			{
				AimTarget = AimTargteBuffer;
			}
		}
	}
	if (brecoilControl && AimTarget.IsZero())
	{
		recoilControl(baseViewAngles, aimPunchCache, shotsFired, baseViewAnglesAddy);
	}
	else if (!AimTarget.IsZero())
		process->write(baseViewAnglesAddy, AimTarget);
}