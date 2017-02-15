/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_i2c_I2cService_h
#define mozilla_dom_i2c_I2cService_h

#include "mozilla/StaticPtr.h"
#include "nsII2cService.h"

namespace mozilla {
namespace dom {
namespace i2c {

union I2cValue {
  uint8_t byte;
  uint16_t word;
};

/**
 * This class implements a service which lets us use the I2C ports.
 */
class I2cService : public nsII2cService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSII2CSERVICE

  static already_AddRefed<I2cService> GetInstance();

private:
  virtual ~I2cService() {};

  static StaticRefPtr<I2cService> sSingleton;

  std::map<uint8_t, int> mFdMap;
  std::map<uint8_t, uint8_t> mDeviceAddressMap;

  int32_t Execute(int aFile, char aReadWrite, uint8_t aCommand,
                           int aValueSize, union I2cValue* aValue);
  int32_t ReadByte(int aFile, uint8_t aCommand);
  int32_t WriteByte(int aFile, uint8_t aCommand, uint8_t aValue);
  int32_t ReadWord(int aFile, uint8_t aCommand);
  int32_t WriteWord(int aFile, uint8_t aCommand, uint16_t aValue);
};

} // namespace i2c
} // namespace dom
} // namespace mozilla

#endif //mozilla_dom_i2c_I2cService_h
