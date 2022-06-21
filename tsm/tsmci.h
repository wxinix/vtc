/*!
  C++ Virtual Traffic Cabinet Framework
  TransModeler Hardware-in-Loop Simulation Controller Interface
  Copyright (C) 2022  Wuping Xin

  MPL 1.1/GPL 2.0/LGPL 2.1 tri-license
*/

/*!
  TransModeler must have been installed on the target computer, otherwise this
  file won't compile.
*/

#ifndef VTC_HILS_TSMCI_H
#define VTC_HILS_TSMCI_H

#if _WIN32 && _MSC_VER && !__INTEL_COMPILER

#include <atlbase.h>

/*!
  For usage of #import directive, refer to
  https://docs.microsoft.com/en-us/cpp/preprocessor/hash-import-directive-cpp?view=msvc-170

  TsmApi type lib is imported here using lib id. If TransModeler has
  not been installed, this file won't compile.
*/
#import "libid:1DA9E83D-B7FF-49D2-B3FC-49AE2CEE10F7"

/*!
  tsmapi.tlh will be auto-generated and imported into the intermediate output
  directory of the project.

  For IntelliSense to work during development time, "#include <tsmapi.tlh>" needs
  to be specified, and intermediate output dir needs to be set to be part of
  the CMake INCLUDE_DIR.

  If IntelliSense is not needed, "#include <tsmapi.tlh>" can be removed.

  If "#include <tsmapi.tlh>" is specified, each time the project is being built,
  MSVC compiler will look into the intermediate output dir as if tsmapi.tlh and
  tsmapi.tli are automatically "included" from there.
 */
#include <tsmapi.tlh>
#include <vtc/vtc.hpp>

namespace vtc::hils::transmodeler {

#pragma pack(push, 8)
#define STDCALLBACK(name) STDMETHODCALLTYPE raw_ ## name

using namespace TsmApi;

/*!
 * TransModeler Controller Interface.
 */
class TsmCI : public _ISimulationEvents, HilsCI
{
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID a_iid, void **a_ppv_obj) final
  {
    return match(a_iid)(
        pattern | or_(_uuidof(_ISimulationEvents), IID_IUnknown) = [&]() {
          AddRef();
          *a_ppv_obj = (void *) this;
          return S_OK;
        },
        pattern | _ = [&]() {
          *a_ppv_obj = nullptr;
          return E_NOINTERFACE;
        });
  }

  ULONG STDMETHODCALLTYPE AddRef() final
  {
    return ++refcount_;
  }

  ULONG STDMETHODCALLTYPE Release() final
  {
    --refcount_;
    if (0 == refcount_) delete this;
    return refcount_;
  }

  virtual HRESULT STDCALLBACK(OpenProject)(BSTR) override
  {
    return S_OK;
  }

  virtual HRESULT STDCALLBACK(StartSimulation)(SHORT, TsmRunType, VARIANT_BOOL) override
  {
    return S_OK;
  }

  virtual HRESULT STDCALLBACK(SimulationStarted)() override
  {
    // logger should not be set at DllEntry point, c.f. https://github.com/gabime/spdlog/issues/1530
    vtc::setup_logger(dir_, "tsmci");
    return (initok_ = load_config(dir_) && enable_sdlc()) ? S_OK : E_ABORT;
  }

  virtual HRESULT STDCALLBACK(Advance)(DOUBLE a_time, DOUBLE *a_next) override
  {
    static auto process_loadswitch_wiring = [&](const auto ch, const auto state, const auto approach, const auto turn) {
      const auto signal = tsmapp_->Network->Signal[approach];
      signal->TurnSignalState[static_cast<short>(turn)] = static_cast<TsmSignalState>(state);
    };

    static auto process_detector_wiring = [&](const auto ch, const auto sid) {
      return tsmapp_->Network->Sensor[sid]->IsActivated;
    };

    if (initok_) {
      process_wirings(process_loadswitch_wiring, process_detector_wiring);
      *a_next = a_time + tsmapp_->StepSize;
      return S_OK;
    } else {
      return E_ABORT;
    }
  }

  virtual HRESULT STDCALLBACK(SimulationStopped)(TsmState) override
  {
    if (initok_) disable_sdlc();
    return S_OK;
  }

  virtual HRESULT STDCALLBACK(EndSimulation)(TsmState) override
  {
    return S_OK;
  }

  virtual HRESULT STDCALLBACK(CloseProject)() override
  {
    return S_OK;
  }

  virtual HRESULT STDCALLBACK(ExitApplication)() override
  {
    return S_OK;
  }

  TsmCI(const TsmCI &) = delete;
  TsmCI(TsmCI &&) = delete;
  TsmCI &operator=(TsmCI &) = delete;
  TsmCI &operator=(TsmCI &&) = delete;

  /*!
   * Init will be called when the Dll module is being loaded.
   * @param a_module
   * @return
   */
  bool init(HMODULE a_module) noexcept
  {
    auto buf = std::array<char, _MAX_PATH + 1>{0};
    return match(GetModuleFileName(a_module, buf.data(), _MAX_PATH))(
        pattern | or_(_MAX_PATH, 0) = expr(false),
        pattern | _ = [&]() { return (dir_ = fs::path{buf.data()}.parent_path(), enable_events_sink()); }
    );
  }

  /*!
   * Finalize will be called when the Dll module is being unloaded.
   */
  void finalize() noexcept
  {
    disable_events_sink();
  }

  static TsmCI &instance() noexcept
  {
    static auto the_instance = TsmCI{};
    return the_instance;
  }

  static ITsmApplication *create_tsmapp() noexcept
  {
    CComPtr<ITsmApplication> tsmapp;
    HRESULT hr = tsmapp.CoCreateInstance(L"TsmApi.TsmApplication");
    return SUCCEEDED(hr) ? tsmapp.Detach() : nullptr;
  }

private:
  TsmCI() = default;

  ~TsmCI()
  {
    finalize();
  };

  bool load_config(const fs::path &a_path) noexcept
  {
    auto verify_loadswitch_wiring = [&](const auto ch, const auto approach, const auto turn) {
      const auto signal = tsmapp_->Network->Signal[approach];
      return (signal) || (vtc::logger()->error("Loadswitch {} wired to non-existing tsm signal {}", ch, approach), false);
    };

    auto verify_detector_wiring = [&](const auto ch, const auto sid) {
      const auto sensor = tsmapp_->Network->Sensor[sid];
      return (sensor) || (vtc::logger()->error("Detector {} wired to non-existing tsm sensor {}", ch, sid), false);
    };

    auto verify_simstep = [&](const double val) {
      const auto step = tsmapp_->StepSize;
      return (std::lround((step - val) * 100) == 0)
          || (vtc::logger()->error("Simstep {} detected, expected {}.", step, val), false);
    };

    return HilsCI::load_config(a_path / "tsmci.config.xml", {verify_simstep, verify_loadswitch_wiring, verify_detector_wiring});
  }

  bool enable_events_sink() noexcept
  {
    tsmapp_ = create_tsmapp();

    return (tsmapp_) && [&]() {
      IConnectionPointContainer *l_pCPC = nullptr;
      HRESULT hr = tsmapp_->QueryInterface(IID_IConnectionPointContainer, (void **) &l_pCPC);

      if (SUCCEEDED(hr)) {
        hr = l_pCPC->FindConnectionPoint(__uuidof(_ISimulationEvents), &pCP_);
        if (SUCCEEDED(hr)) hr = pCP_->Advise(this, &cookie_);
        l_pCPC->Release();
      }

      return SUCCEEDED(hr);
    }();
  }

  void disable_events_sink() noexcept
  {
    if (pCP_) {
      pCP_->Unadvise(cookie_);
      pCP_ = nullptr;
    }

    tsmapp_ = nullptr;
    cookie_ = 0;
  }

private:
  ULONG refcount_{1};
  IConnectionPointPtr pCP_{nullptr};
  DWORD cookie_{0};
  ITsmApplicationPtr tsmapp_{nullptr};
  fs::path dir_{};
  bool initok_{false};
};

#pragma pack(pop)
}

#else
#error This project must be built on Windows using MSVC compiler.
#endif // _WIN32 && _MSC_VER && !__INTEL_COMPILER
#endif // VTC_HILS_TSMCI_H


