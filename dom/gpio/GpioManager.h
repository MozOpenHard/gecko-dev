/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_gpio_GpioManager_h
#define mozilla_dom_gpio_GpioManager_h

#include "mozilla/Attributes.h"
#include "nsISupports.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

//class Date;

namespace gpio {

class GpioManager final : public nsISupports
                        , public nsWrapperCache
{
public:
  static bool PrefEnabled(JSContext* aCx, JSObject* aGlobal)
  {
//#ifdef MOZ_GPIO_MANAGER
    return true;
//#else
    //return false;
//#endif
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(GpioManager)

  explicit GpioManager(nsPIDOMWindow* aWindow)
    : mWindow(aWindow)
  {
    SetIsDOMBinding();
  }

  nsPIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }
  JSObject* WrapObject(JSContext* aCx);

  void Export(uint32_t aPinNo);
  void Unexport(uint32_t aPinNo);
  void SetDirection(uint32_t aPinNo, bool aOut);
  bool GetDirection(uint32_t aPinNo);
  void SetValue(uint32_t aPinNo, uint32_t aValue);
  uint32_t GetValue(uint32_t aPinNo);

private:
  ~GpioManager() {}

  nsCOMPtr<nsPIDOMWindow> mWindow;
};

} // namespace gpio
} // namespace dom
} // namespace mozilla

#endif //mozilla_dom_gpio_GpioManager_h
