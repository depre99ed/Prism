#pragma once
#include <windows.h>

// A painted dialog has its own text field and Tab order. Native dialog mnemonic
// handling would consume its characters, including keys addressed to hidden controls.
inline void DispatchDialogInput(HWND root,MSG& message,HACCEL accelerators,bool painted)
{
    if(painted && message.message>=WM_KEYFIRST && message.message<=WM_KEYLAST &&
        (message.hwnd==root || IsChild(root,message.hwnd)))message.hwnd=root;
    if(accelerators && TranslateAcceleratorW(root,accelerators,&message))return;
    if(painted || !IsDialogMessageW(root,&message))
    {TranslateMessage(&message);DispatchMessageW(&message);}
}
