/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "I2cManager.h"

#include "mozilla/dom/MozI2cManagerBinding.h"
#include "nsII2cService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
namespace i2c {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(I2cManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(I2cManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(I2cManager)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(I2cManager, mWindow)

JSObject*
I2cManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozI2cManagerBinding::Wrap(aCx, this, aGivenProto);
}

void
I2cManager::Open(uint8_t aDeviceNo)
{
  nsCOMPtr<nsII2cService> i2cService = do_GetService(I2CSERVICE_CONTRACTID);
  if (i2cService) {
    i2cService->Open(aDeviceNo);
  }
}

void
I2cManager::SetDeviceAddress(uint8_t aDeviceNo, uint8_t aDeviceAddress)
{
  nsCOMPtr<nsII2cService> i2cService = do_GetService(I2CSERVICE_CONTRACTID);
  if (i2cService) {
    i2cService->SetDeviceAddress(aDeviceNo, aDeviceAddress);
  }
}

void
I2cManager::Write(uint8_t aDeviceNo, uint8_t aCommand, uint16_t aValue, bool aIsOctet)
{
  nsCOMPtr<nsII2cService> i2cService = do_GetService(I2CSERVICE_CONTRACTID);
  if (i2cService) {
    i2cService->Write(aDeviceNo, aCommand, aValue, aIsOctet);
  }
}

uint16_t
I2cManager::Read(uint8_t aDeviceNo, uint8_t aCommand, bool aIsOctet)
{
  uint16_t aValue;

  nsCOMPtr<nsII2cService> i2cService = do_GetService(I2CSERVICE_CONTRACTID);
  if (i2cService) {
    i2cService->Read(aDeviceNo, aCommand, aIsOctet, &aValue);
    return aValue;
  }
  return 12345;
}

} // namespace i2c
} // namespace dom
} // namespace mozilla
