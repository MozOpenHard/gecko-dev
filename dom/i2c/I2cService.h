/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_i2c_I2cService_h
#define mozilla_dom_i2c_I2cService_h

#include "mozilla/StaticPtr.h"
#include "nsII2cService.h"

#include <linux/i2c.h>
//#include <linux/i2c-dev.h>

namespace mozilla {
namespace dom {
namespace i2c {

// XXX: define here instead of including i2c-dev.h
struct i2c_smbus_ioctl_data {
        uint8_t read_write;
        uint8_t command;
        uint32_t size;
        union i2c_smbus_data *data;
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

  std::map<uint8_t, int> map_fd;

  int32_t i2c_smbus_access(int file, char read_write, uint8_t command,
                           int size, union i2c_smbus_data *data);
  int32_t i2c_smbus_read_byte_data(int file, uint8_t command);
  int32_t i2c_smbus_write_byte_data(int file, uint8_t command, uint8_t value);
  int32_t i2c_smbus_read_word_data(int file, uint8_t command);
  int32_t i2c_smbus_write_word_data(int file, uint8_t command, uint16_t value);
};

} // namespace i2c
} // namespace dom
} // namespace mozilla

#endif //mozilla_dom_i2c_I2cService_h
