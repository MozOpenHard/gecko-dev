/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_i2c_I2cManager_h
#define mozilla_dom_i2c_I2cManager_h

#include "mozilla/Attributes.h"
#include "nsISupports.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

//class Date;

namespace i2c {

class I2cManager final : public nsISupports
                       , public nsWrapperCache
{
public:
  static bool PrefEnabled(JSContext* aCx, JSObject* aGlobal)
  {
#ifdef MOZ_I2C_MANAGER
    return true;
#else
    return false;
#endif
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(I2cManager)

  explicit I2cManager(nsPIDOMWindow* aWindow)
    : mWindow(aWindow)
  {
  }

  nsPIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void Open(uint8_t aDeviceNo);
  void SetDeviceAddress(uint8_t aDeviceNo, uint8_t aDeviceAddress);
  void Write(uint8_t aDeviceNo, uint8_t aCommand, uint16_t aValue, bool aIsOctet);
  uint16_t Read(uint8_t aDeviceNo, uint8_t aCommand, bool aIsOctet);

private:
  ~I2cManager() {}

  nsCOMPtr<nsPIDOMWindow> mWindow;
};

} // namespace i2c
} // namespace dom
} // namespace mozilla

#endif //mozilla_dom_i2c_I2cManager_h
