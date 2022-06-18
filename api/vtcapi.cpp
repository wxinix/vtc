/*!
  C++ Virtual Traffic Cabinet Framework
  Virtual Traffic Cabinet Dynamic Link Library
  Copyright (C) 2022  Wuping Xin

  MPL 1.1/GPL 2.0/LGPL 2.1 tri-license
*/

#include "vtcapi.h"

#if _WIN32
#include <Windows.h>

BOOL APIENTRY DllMain(HANDLE handle, DWORD ulReason, LPVOID lpReserved)
{
  switch (ulReason) {
    case DLL_PROCESS_ATTACH: //
      return true;

    case DLL_PROCESS_DETACH: //
      return true;

    case DLL_THREAD_ATTACH:  //
      return true;

    case DLL_THREAD_DETACH: //
      return true;
  }
}

#endif

constexpr auto version = "1.0.0-r20220526";

VTC_API const char *VTC_Version(void)
{
  return version;
}