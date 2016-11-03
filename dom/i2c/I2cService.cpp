/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "jsapi.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Hal.h"
#include "I2cService.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "I2cService" , ## args)
#else
#define LOG(args...)
#endif

#define I2C_REQUEST 0x0720
#define I2C_SLAVE 0x0703
#define I2C_READ 1
#define I2C_WRITE 0
#define I2C_BYTE 2
#define I2C_WORD 3

namespace mozilla {
namespace dom {
namespace i2c {

struct IoctlData {
  uint8_t readWrite;
  uint8_t command;
  uint32_t valueSize;
  I2cValue* value;
};

NS_IMPL_ISUPPORTS(I2cService, nsII2cService)

StaticRefPtr<I2cService> I2cService::sSingleton;

already_AddRefed<I2cService>
I2cService::GetInstance()
{
  if (!sSingleton) {
    sSingleton = new I2cService();
    ClearOnShutdown(&sSingleton);
  }
  RefPtr<I2cService> service = sSingleton.get();
  return service.forget();
}

int32_t
I2cService::Execute(int aFile, char aReadWrite, uint8_t aCommand, 
                             int aValueSize, union I2cValue* aValue)
{
  struct IoctlData ioctlData;
  ioctlData.readWrite = aReadWrite;
  ioctlData.command = aCommand;
  ioctlData.valueSize = aValueSize;
  ioctlData.value = aValue;
  return ioctl(aFile, I2C_REQUEST, &ioctlData);
}

int32_t
I2cService::ReadByte(int aFile, uint8_t aCommand)
{
  union I2cValue value;
  if (Execute(aFile, I2C_READ, aCommand, I2C_BYTE, &value))
    return -1;
  else
    return 0x0FF & value.byte;
}

int32_t
I2cService::WriteByte(int aFile, uint8_t aCommand, uint8_t aValue)
{
  union I2cValue value;
  value.byte = aValue;
  return Execute(aFile, I2C_WRITE, aCommand, I2C_BYTE, &value);
}

int32_t
I2cService::ReadWord(int aFile, uint8_t aCommand)
{
  union I2cValue value;
  if (Execute(aFile, I2C_READ, aCommand, I2C_WORD, &value))
    return -1;
  else
    return 0x0FFFF & value.word;
}

int32_t
I2cService::WriteWord(int aFile, uint8_t aCommand, uint16_t aValue)
{
  union I2cValue value;
  value.word = aValue;
  return Execute(aFile, I2C_WRITE, aCommand, I2C_WORD, &value);
}

NS_IMETHODIMP
I2cService::Open(uint8_t aDeviceNo) {
  int fd;
  char deviceName[16];

  LOG("i2c Open(%d) called\n", aDeviceNo);

  if (mFdMap.find(aDeviceNo) != mFdMap.end()) {
    LOG("I2C device%d already opened\n", aDeviceNo);
    return NS_OK;
  }

  snprintf(deviceName, sizeof(deviceName), "/dev/i2c-%d", aDeviceNo);
  fd = open(deviceName, O_RDWR);
  if (fd < 0) {
    LOG("%s: open(%s) failed: %s(%d)\n", __func__, deviceName, strerror(errno), errno);
    return NS_ERROR_FAILURE;
  }

  mFdMap[aDeviceNo] = fd;

  return NS_OK;
}

NS_IMETHODIMP
I2cService::SetDeviceAddress(uint8_t aDeviceNo, uint8_t aDeviceAddress) {

  LOG("i2c SetDeviceAddress(%d, %d) called\n", aDeviceNo, aDeviceAddress);

  if (mFdMap.find(aDeviceNo) == mFdMap.end()) {
    LOG("I2C device%d is not opened yet\n", aDeviceNo);
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (ioctl(mFdMap[aDeviceNo], I2C_SLAVE, aDeviceAddress) < 0) {
    LOG("%s: ioctl(I2C_SLAVE) for %02x failed: %s(%d)\n", __func__,
        aDeviceAddress, strerror(errno), errno);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
I2cService::Write(uint8_t aDeviceNo, uint8_t aCommand, uint16_t aValue, bool aIsOctet) {

  LOG("i2c Write(%d,%d,%d) called\n", aDeviceNo, aCommand, aValue);

  if (mFdMap.find(aDeviceNo) == mFdMap.end()) {
    LOG("I2C device%d is not opened yet\n", aDeviceNo);
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (aIsOctet) {
    if (WriteByte(mFdMap[aDeviceNo], aCommand, (uint8_t)aValue) < 0) {
      LOG("%s: WriteByte(%d,%d,%d) failed: %s(%d)\n", __func__,
          aDeviceNo, aCommand, aValue, strerror(errno), errno);
      return NS_ERROR_FAILURE;
    }
  } else {
    if (WriteWord(mFdMap[aDeviceNo], aCommand, aValue) < 0) {
      LOG("%s: WriteWord(%d,%d,%d) failed: %s(%d)\n", __func__,
          aDeviceNo, aCommand, aValue, strerror(errno), errno);
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
I2cService::Read(uint8_t aDeviceNo, uint8_t aCommand, bool aIsOctet, uint16_t *aValue) {

  LOG("i2c Read(%d,%d,%d) called\n", aDeviceNo, aCommand, aIsOctet);

  if (mFdMap.find(aDeviceNo) == mFdMap.end()) {
    LOG("I2C device%d is not opened yet\n", aDeviceNo);
    return NS_ERROR_NOT_AVAILABLE;
  }

  int32_t ret;
  if (aIsOctet) {
    ret = ReadByte(mFdMap[aDeviceNo], aCommand);
    if (ret < 0) {
      LOG("%s: ReadByte(%d,%d) failed: %s(%d)\n", __func__,
          aDeviceNo, aCommand, strerror(errno), errno);
      return NS_ERROR_FAILURE;
    }
  } else {
    ret = ReadWord(mFdMap[aDeviceNo], aCommand);
    if (ret < 0) {
      LOG("%s: ReadWord(%d,%d) failed: %s(%d)\n", __func__,
          aDeviceNo, aCommand, strerror(errno), errno);
      return NS_ERROR_FAILURE;
    }
  }
  *aValue = ret;

  return NS_OK;
}

} // namespace i2c
} // namespace dom
} // namespace mozilla
