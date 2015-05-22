/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GpioManager.h"

#include "mozilla/dom/MozGpioManagerBinding.h"
#include "nsIGpioService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
namespace gpio {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GpioManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(GpioManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GpioManager)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GpioManager, mWindow)

JSObject*
GpioManager::WrapObject(JSContext* aCx)
{
  return MozGpioManagerBinding::Wrap(aCx, this);
}

void
GpioManager::Export(uint32_t aPinNo)
{
  nsCOMPtr<nsIGpioService> gpioService = do_GetService(GPIOSERVICE_CONTRACTID);
  if (gpioService) {
    gpioService->Export(aPinNo);
  }
}

void
GpioManager::Unexport(uint32_t aPinNo)
{
  nsCOMPtr<nsIGpioService> gpioService = do_GetService(GPIOSERVICE_CONTRACTID);
  if (gpioService) {
    gpioService->Unexport(aPinNo);
  }
}

void
GpioManager::SetDirection(uint32_t aPinNo, bool aOut)
{
  nsCOMPtr<nsIGpioService> gpioService = do_GetService(GPIOSERVICE_CONTRACTID);
  if (gpioService) {
    gpioService->SetDirection(aPinNo, aOut);
  }
}

bool
GpioManager::GetDirection(uint32_t aPinNo)
{
  bool aOut;

  nsCOMPtr<nsIGpioService> gpioService = do_GetService(GPIOSERVICE_CONTRACTID);
  if (gpioService) {
    gpioService->GetDirection(aPinNo, &aOut);
    return aOut;
  }
  return false;
}

void
GpioManager::SetValue(uint32_t aPinNo, uint32_t aValue)
{
  nsCOMPtr<nsIGpioService> gpioService = do_GetService(GPIOSERVICE_CONTRACTID);
  if (gpioService) {
    gpioService->SetValue(aPinNo, aValue);
  }
}

uint32_t
GpioManager::GetValue(uint32_t aPinNo)
{
  uint32_t aValue;

  nsCOMPtr<nsIGpioService> gpioService = do_GetService(GPIOSERVICE_CONTRACTID);
  if (gpioService) {
    gpioService->GetValue(aPinNo, &aValue);
    return aValue;
  }
  return 12345;
}

} // namespace gpio
} // namespace dom
} // namespace mozilla
