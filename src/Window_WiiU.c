#include "Core.h"
#if defined CC_BUILD_WIIU
#include "Window.h"
#include "Platform.h"
#include "Input.h"
#include "Event.h"
#include "String.h"
#include "Funcs.h"
#include "Bitmap.h"
#include "Errors.h"
#include "ExtMath.h"
#include "Graphics.h"
#include "Launcher.h"
#include <coreinit/memheap.h>
#include <coreinit/cache.h>
#include <coreinit/memfrmheap.h>
#include <coreinit/memory.h>
#include <coreinit/screen.h>
#include <proc_ui/procui.h>
#include <gx2/display.h>
#include <vpad/input.h>
#include <whb/proc.h>
#include <padscore/kpad.h>

static cc_bool launcherMode;
struct _DisplayData DisplayInfo;
struct _WindowData WindowInfo;
struct _WindowData Window_Alt;
cc_bool launcherTop;

void Window_Init(void) {
	switch(GX2GetSystemTVScanMode())
	{
	case GX2_TV_SCAN_MODE_480I:
	case GX2_TV_SCAN_MODE_480P:
		DisplayInfo.Width  = 854;
		DisplayInfo.Height = 480;
		break;
	case GX2_TV_SCAN_MODE_1080I:
	case GX2_TV_SCAN_MODE_1080P:
		DisplayInfo.Width  = 1920;
		DisplayInfo.Height = 1080;
		break;
	case GX2_TV_SCAN_MODE_720P:
	default:
		DisplayInfo.Width  = 1280;
		DisplayInfo.Height =  720;
	break;
	}
	
	DisplayInfo.ScaleX = 1;
	DisplayInfo.ScaleY = 1;
	
	Window_Main.Width   = DisplayInfo.Width;
	Window_Main.Height  = DisplayInfo.Height;
	Window_Main.Focused = true;
	Window_Main.Exists  = true;

	Input.Sources = INPUT_SOURCE_GAMEPAD;
	DisplayInfo.ContentOffsetX = 10;
	DisplayInfo.ContentOffsetY = 10;

	Window_Main.SoftKeyboard = SOFT_KEYBOARD_RESIZE;
	Input_SetTouchMode(true);
	
	Window_Alt.Width  = 854;
	Window_Alt.Height = 480;
		
	KPADInit();
	VPADInit();
}

void Window_Free(void) { }

void Window_Create2D(int width, int height) {
	// well this works in CEMU at least
	Window_Main.Width   = 1280;
	Window_Main.Height  =  720;
	launcherMode  = true;  
}

void Window_Create3D(int width, int height) { 
	Window_Main.Width   = DisplayInfo.Width;
	Window_Main.Height  = DisplayInfo.Height;
	launcherMode = false; 
}

void Window_RequestClose(void) {
	Event_RaiseVoid(&WindowEvents.Closing);
}


static void ProcessKPAD(double delta) {
	KPADStatus kpad = { 0 };
	int res = KPADRead(0, &kpad, 1);
	if (res != KPAD_ERROR_OK) return;
	
	switch (kpad.extensionType)
	{
	
	}
}


#define AXIS_SCALE 4.0f
static void ProcessVpadStick(int axis, float x, float y, double delta) {
	// May not be exactly 0 on actual hardware
	if (Math_AbsF(x) <= 0.1f) x = 0;
	if (Math_AbsF(y) <= 0.1f) y = 0;
	
	Gamepad_SetAxis(axis, x * AXIS_SCALE, -y * AXIS_SCALE, delta);
}
   
static void ProcessVpadButtons(int mods) {
	Gamepad_SetButton(CCPAD_L,  mods & VPAD_BUTTON_L);
	Gamepad_SetButton(CCPAD_R,  mods & VPAD_BUTTON_R);
	Gamepad_SetButton(CCPAD_ZL, mods & VPAD_BUTTON_ZL);
	Gamepad_SetButton(CCPAD_ZR, mods & VPAD_BUTTON_ZR);
      
	Gamepad_SetButton(CCPAD_A, mods & VPAD_BUTTON_A);
	Gamepad_SetButton(CCPAD_B, mods & VPAD_BUTTON_B);
	Gamepad_SetButton(CCPAD_X, mods & VPAD_BUTTON_X);
	Gamepad_SetButton(CCPAD_Y, mods & VPAD_BUTTON_Y);
      
	Gamepad_SetButton(CCPAD_START,  mods & VPAD_BUTTON_PLUS);
	Gamepad_SetButton(CCPAD_SELECT, mods & VPAD_BUTTON_MINUS);

	Gamepad_SetButton(CCPAD_LEFT,   mods & VPAD_BUTTON_LEFT);
	Gamepad_SetButton(CCPAD_RIGHT,  mods & VPAD_BUTTON_RIGHT);
	Gamepad_SetButton(CCPAD_UP,     mods & VPAD_BUTTON_UP);
	Gamepad_SetButton(CCPAD_DOWN,   mods & VPAD_BUTTON_DOWN);
	
}

static void ProcessVpadTouch(VPADTouchData* data) {
	static int was_touched;

	// TODO rescale to main screen size
	if (data->touched) {
		int x = data->x, y = data->y;
		Platform_Log2("TOUCH: %i, %i", &x, &y);
		Input_AddTouch(0,    data->x,       data->y);
	} else if (was_touched) {
		Input_RemoveTouch(0, Pointers[0].x, Pointers[0].y);
	}
	was_touched = data->touched;
}

static void ProcessVPAD(double delta) {
	VPADStatus vpadStatus;
	VPADReadError error = VPAD_READ_SUCCESS;
	VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &error);
	if (error != VPAD_READ_SUCCESS) return;
	
	VPADGetTPCalibratedPoint(VPAD_CHAN_0, &vpadStatus.tpNormal, &vpadStatus.tpNormal);
	ProcessVpadButtons(vpadStatus.hold);
	ProcessVpadTouch(&vpadStatus.tpNormal);
	
	ProcessVpadStick(PAD_AXIS_LEFT,  vpadStatus.leftStick.x,  vpadStatus.leftStick.y,  delta);
	ProcessVpadStick(PAD_AXIS_RIGHT, vpadStatus.rightStick.x, vpadStatus.rightStick.y, delta);
}


void Window_ProcessEvents(double delta) {
	Input.JoystickMovement = false;
	
	if (!WHBProcIsRunning()) {
		Window_Main.Exists = false;
		Window_RequestClose();
		return;
	}
	
	ProcessVPAD(delta);
	ProcessKPAD(delta);
}

void Window_UpdateRawMouse(void) { }

void Cursor_SetPosition(int x, int y) { }
void Window_EnableRawMouse(void)  { Input.RawMode = true;  }
void Window_DisableRawMouse(void) { Input.RawMode = false; }


/*########################################################################################################################*
*------------------------------------------------------Framebuffer--------------------------------------------------------*
*#########################################################################################################################*/
void Window_AllocFramebuffer(struct Bitmap* bmp) {
	WHBLogConsoleInit();  	
	bmp->scan0 = (BitmapCol*)Mem_Alloc(bmp->width * bmp->height, 4, "window pixels");
}

void Window_DrawFramebuffer(Rect2D r, struct Bitmap* bmp) {
	if (launcherTop) return; // TODO: Draw on DRC properly
   
	/*for (int y = r.y; y < r.y + r.height; y++) 
	{
		cc_uint32* src = bmp->scan0 + y * bmp->width;
		
		for (int x = r.x; x < r.x + r.width; x++) {
			OSScreenPutPixelEx(SCREEN_TV, x, y, src[x]);
		}
	}*/
	for (int y = 0; y < bmp->height; y++) 
	{
		cc_uint32* src = bmp->scan0 + y * bmp->width;
		
		for (int x = 0; x < bmp->width; x++) {
			OSScreenPutPixelEx(SCREEN_TV, x, y, src[x]);
		}
	}

	//DCFlushRange(sBufferTV, sBufferSizeTV);
	OSScreenFlipBuffersEx(SCREEN_TV);
	
	OSScreenClearBufferEx(SCREEN_DRC, Launcher_Theme.BackgroundColor);
	OSScreenFlipBuffersEx(SCREEN_DRC);
}

void Window_FreeFramebuffer(struct Bitmap* bmp) {
	Mem_Free(bmp->scan0);
	WHBLogConsoleFree();
}


/*########################################################################################################################*
*-------------------------------------------------------Misc/Other--------------------------------------------------------*
*#########################################################################################################################*/
void Window_SetTitle(const cc_string* title)   { }
void Clipboard_GetText(cc_string* value)       { }
void Clipboard_SetText(const cc_string* value) { }

int Window_GetWindowState(void) { return WINDOW_STATE_FULLSCREEN; }
cc_result Window_EnterFullscreen(void) { return 0; }
cc_result Window_ExitFullscreen(void)  { return 0; }
int Window_IsObscured(void)            { return 0; }

void Window_Show(void) { }
void Window_SetSize(int width, int height) { }


void OnscreenKeyboard_Open(struct OpenKeyboardArgs* args) { /* TODO implement */ }
void OnscreenKeyboard_SetText(const cc_string* text) { }
void OnscreenKeyboard_Draw2D(Rect2D* r, struct Bitmap* bmp) { }
void OnscreenKeyboard_Draw3D(void) { }
void OnscreenKeyboard_Close(void) { /* TODO implement */ }

void Window_ShowDialog(const char* title, const char* msg) {
	/* TODO implement */
	Platform_LogConst(title);
	Platform_LogConst(msg);
}

cc_result Window_OpenFileDialog(const struct OpenFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}

cc_result Window_SaveFileDialog(const struct SaveFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}
#endif