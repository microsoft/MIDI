// DesktopAppSdkInitializer.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "DesktopAppSdkInitializer.h"


// This is an example of an exported variable
DESKTOPAPPSDKINITIALIZER_API int nDesktopAppSdkInitializer=0;

// This is an example of an exported function.
DESKTOPAPPSDKINITIALIZER_API int fnDesktopAppSdkInitializer(void)
{
    return 0;
}

// This is the constructor of a class that has been exported.
CDesktopAppSdkInitializer::CDesktopAppSdkInitializer()
{
    return;
}
