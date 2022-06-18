/*!
  C++ Virtual Traffic Cabinet Framework
  Virtual Traffic Cabinet Dynamic Link Library
  Copyright (C) 2022  Wuping Xin

  MPL 1.1/GPL 2.0/LGPL 2.1 tri-license
*/

#ifndef VTC_VTC_VTCAPI_H_
#define VTC_VTC_VTCAPI_H_

#include <vtc/vtc.hpp>

#ifdef VTCAPI_IMPORTS
#define VTC_API extern "C" __declspec(dllimport)
#else
#define VTC_API extern "C" __declspec(dllexport)
#endif

/*!
 * VTC_Version returns a string describing version information of the VTC API
 * library, which uses semantic versioning.
 */
VTC_API const char* VTC_Version(void);


#endif //VTC_VTC_VTCAPI_H_
