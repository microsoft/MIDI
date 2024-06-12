// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the DESKTOPAPPSDKINITIALIZER_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// DESKTOPAPPSDKINITIALIZER_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef DESKTOPAPPSDKINITIALIZER_EXPORTS
#define DESKTOPAPPSDKINITIALIZER_API __declspec(dllexport)
#else
#define DESKTOPAPPSDKINITIALIZER_API __declspec(dllimport)
#endif

// This class is exported from the dll
class DESKTOPAPPSDKINITIALIZER_API CDesktopAppSdkInitializer {
public:
	CDesktopAppSdkInitializer(void);
	// TODO: add your methods here.
};

extern DESKTOPAPPSDKINITIALIZER_API int nDesktopAppSdkInitializer;

DESKTOPAPPSDKINITIALIZER_API int fnDesktopAppSdkInitializer(void);
