/*!
  C++ Virtual Traffic Cabinet Framework
  TransModeler Hardware-in-Loop Simulation Controller Interface
  Copyright (C) 2022  Wuping Xin

  MPL 1.1/GPL 2.0/LGPL 2.1 tri-license
*/

#include "tsmci.h"

BOOL APIENTRY DllMain(HANDLE handle, DWORD ulReason, LPVOID lpReserved)
{
  switch (ulReason) {
    case DLL_PROCESS_ATTACH: //
      return vtc::hils::transmodeler::TsmCI::instance().init(static_cast<HMODULE>(handle));

    case DLL_PROCESS_DETACH: //
      return vtc::hils::transmodeler::TsmCI::instance().finalize(), true;

    case DLL_THREAD_ATTACH: //
      return true;

    case DLL_THREAD_DETACH: //
      return true;
  }
}
