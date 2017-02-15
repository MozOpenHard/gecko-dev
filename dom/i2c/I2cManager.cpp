/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "I2cManager.h"

#include "mozilla/dom/MozI2cManagerBinding.h"
#include "nsII2cService.h"
#include "nsServiceManagerUtils.h"
#include "nsError.h"

namespace mozilla {
namespace dom {
namespace i2c {

NS_IMPL_CYCLE_COLLECTION_CLASS(I2cManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(I2cManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(I2cManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(I2cManager)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(I2cManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(I2cManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(I2cManager)
  
JSObject*
I2cManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozI2cManagerBinding_workers::Wrap(aCx, this, aGivenProto);
}

void
I2cManager::Open(uint8_t aDeviceNo, ErrorResult& aRv)
{
  nsresult aRes;

  nsCOMPtr<nsII2cService> i2cService = do_GetService(I2CSERVICE_CONTRACTID);
  if (!i2cService) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  aRes = i2cService->Open(aDeviceNo);
  if(aRes != NS_OK){
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  }
}

void
I2cManager::SetDeviceAddress(uint8_t aDeviceNo, uint8_t aDeviceAddress, ErrorResult& aRv)
{
  nsresult aRes;

  nsCOMPtr<nsII2cService> i2cService = do_GetService(I2CSERVICE_CONTRACTID);
  if (!i2cService) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  aRes = i2cService->SetDeviceAddress(aDeviceNo, aDeviceAddress);
  if(aRes != NS_OK){
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  }
}

void
I2cManager::Write(uint8_t aDeviceNo, uint8_t aDeviceAddress, uint8_t aCommand, uint16_t aValue, bool aIsOctet, ErrorResult& aRv)
{
  nsresult aRes;

  nsCOMPtr<nsII2cService> i2cService = do_GetService(I2CSERVICE_CONTRACTID);
  if (!i2cService) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  aRes = i2cService->Write(aDeviceNo, aDeviceAddress, aCommand, aValue, aIsOctet);
  if(aRes != NS_OK){
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  }
}

uint16_t
I2cManager::Read(uint8_t aDeviceNo, uint8_t aDeviceAddress, uint8_t aCommand, bool aIsOctet, ErrorResult& aRv)
{
  uint16_t aValue;
  nsresult aRes;

  nsCOMPtr<nsII2cService> i2cService = do_GetService(I2CSERVICE_CONTRACTID);
  if (!i2cService) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return 0xFFFF;
  }
  
  aRes = i2cService->Read(aDeviceNo, aDeviceAddress, aCommand, aIsOctet, &aValue);
  if(aRes != NS_OK){
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return 0xFFFF;
  }
  
  return aValue;
}


} // namespace i2c
} // namespace dom
} // namespace mozilla
