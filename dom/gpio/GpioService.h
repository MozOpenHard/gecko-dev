/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_gpio_GpioService_h
#define mozilla_dom_gpio_GpioService_h

#include "mozilla/StaticPtr.h"
#include "nsIGpioService.h"

namespace mozilla {
namespace dom {
namespace gpio {

/**
 * This class implements a service which lets us use the GPIO ports.
 */
class GpioService : public nsIGpioService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGPIOSERVICE

  static already_AddRefed<GpioService> GetInstance();

private:
  virtual ~GpioService() {};

  static StaticRefPtr<GpioService> sSingleton;

  std::map<uint32_t, bool> map_direction;
  std::map<uint32_t, FILE *> map_direction_fp;
  std::map<uint32_t, FILE *> map_value_fp;
};

} // namespace gpio
} // namespace dom
} // namespace mozilla

#endif //mozilla_dom_gpio_GpioService_h
