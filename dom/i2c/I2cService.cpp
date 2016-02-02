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

namespace mozilla {
namespace dom {
namespace i2c {

NS_IMPL_ISUPPORTS(I2cService, nsII2cService)

/* static */ StaticRefPtr<I2cService> I2cService::sSingleton;

/* static */ already_AddRefed<I2cService>
I2cService::GetInstance()
{
  if (!sSingleton) {
    sSingleton = new I2cService();
    ClearOnShutdown(&sSingleton);
  }
  nsRefPtr<I2cService> service = sSingleton.get();
  return service.forget();
}

int32_t
I2cService::i2c_smbus_access(int file, char read_write, uint8_t command, 
                             int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;
	return ioctl(file,I2C_SMBUS,&args);
}

int32_t
I2cService::i2c_smbus_read_byte_data(int file, uint8_t command)
{
	union i2c_smbus_data data;
	if (i2c_smbus_access(file,I2C_SMBUS_READ,command,
	                     I2C_SMBUS_BYTE_DATA,&data))
		return -1;
	else
		return 0x0FF & data.byte;
}

int32_t
I2cService::i2c_smbus_write_byte_data(int file, uint8_t command, uint8_t value)
{
	union i2c_smbus_data data;
	data.byte = value;
	return i2c_smbus_access(file,I2C_SMBUS_WRITE,command,
	                        I2C_SMBUS_BYTE_DATA, &data);
}

int32_t
I2cService::i2c_smbus_read_word_data(int file, uint8_t command)
{
	union i2c_smbus_data data;
	if (i2c_smbus_access(file,I2C_SMBUS_READ,command,
	                     I2C_SMBUS_WORD_DATA,&data))
		return -1;
	else
		return 0x0FFFF & data.word;
}

int32_t
I2cService::i2c_smbus_write_word_data(int file, uint8_t command, uint16_t value)
{
	union i2c_smbus_data data;
	data.word = value;
	return i2c_smbus_access(file,I2C_SMBUS_WRITE,command,
	                        I2C_SMBUS_WORD_DATA, &data);
}

NS_IMETHODIMP
I2cService::Open(uint8_t aDeviceNo) {
  int fd;
  char deviceName[16];

  LOG("i2c Open(%d) called\n", aDeviceNo);

  if (map_fd.find(aDeviceNo) != map_fd.end()) {
    LOG("I2C device%d already opened\n", aDeviceNo);
    return NS_OK;
  }

  snprintf(deviceName, sizeof(deviceName), "/dev/i2c-%d", aDeviceNo);
  fd = open(deviceName, O_RDWR);
  if (fd < 0) {
    LOG("%s: open(%s) failed: %s(%d)\n", __func__, deviceName, strerror(errno), errno);
    return NS_ERROR_FAILURE;
  }

  map_fd[aDeviceNo] = fd;

  return NS_OK;
}

NS_IMETHODIMP
I2cService::SetDeviceAddress(uint8_t aDeviceNo, uint8_t aDeviceAddress) {

  LOG("i2c SetDeviceAddress(%d, %d) called\n", aDeviceNo, aDeviceAddress);

  if (map_fd.find(aDeviceNo) == map_fd.end()) {
    LOG("I2C device%d is not opened yet\n", aDeviceNo);
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (ioctl(map_fd[aDeviceNo], I2C_SLAVE, aDeviceAddress) < 0) {
    LOG("%s: ioctl(I2C_SLAVE) for %02x failed: %s(%d)\n", __func__,
        aDeviceAddress, strerror(errno), errno);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
I2cService::Write(uint8_t aDeviceNo, uint8_t aCommand, uint16_t aValue, bool aIsOctet) {

  LOG("i2c Write(%d,%d,%d) called\n", aDeviceNo, aCommand, aValue);

  if (map_fd.find(aDeviceNo) == map_fd.end()) {
    LOG("I2C device%d is not opened yet\n", aDeviceNo);
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (aIsOctet) {
    if (i2c_smbus_write_byte_data(map_fd[aDeviceNo], aCommand, (uint8_t)aValue) < 0) {
      LOG("%s: i2c_smbus_write_byte_data(%d,%d,%d) failed: %s(%d)\n", __func__,
          aDeviceNo, aCommand, aValue, strerror(errno), errno);
      return NS_ERROR_FAILURE;
    }
  } else {
    if (i2c_smbus_write_word_data(map_fd[aDeviceNo], aCommand, aValue) < 0) {
      LOG("%s: i2c_smbus_write_word_data(%d,%d,%d) failed: %s(%d)\n", __func__,
          aDeviceNo, aCommand, aValue, strerror(errno), errno);
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
I2cService::Read(uint8_t aDeviceNo, uint8_t aCommand, bool aIsOctet, uint16_t *aValue) {
  int32_t ret;

  LOG("i2c Read(%d,%d,%d) called\n", aDeviceNo, aCommand, aIsOctet);

  if (map_fd.find(aDeviceNo) == map_fd.end()) {
    LOG("I2C device%d is not opened yet\n", aDeviceNo);
    return NS_ERROR_NOT_AVAILABLE;
  }


  if (aIsOctet) {
    ret = i2c_smbus_read_byte_data(map_fd[aDeviceNo], aCommand);
    if (ret < 0) {
      LOG("%s: i2c_smbus_read_byte_data(%d,%d) failed: %s(%d)\n", __func__,
          aDeviceNo, aCommand, strerror(errno), errno);
      return NS_ERROR_FAILURE;
    }
  } else {
    ret = i2c_smbus_read_word_data(map_fd[aDeviceNo], aCommand);
    if (ret < 0) {
      LOG("%s: i2c_smbus_read_word_data(%d,%d) failed: %s(%d)\n", __func__,
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
