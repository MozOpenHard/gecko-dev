/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "jsapi.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Hal.h"
#include "GpioService.h"

#include <sys/stat.h>

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GpioService" , ## args)
#else
#define LOG(args...)
#endif

namespace mozilla {
namespace dom {
namespace gpio {

NS_IMPL_ISUPPORTS(GpioService, nsIGpioService)

/* static */ StaticRefPtr<GpioService> GpioService::sSingleton;

/* static */ already_AddRefed<GpioService>
GpioService::GetInstance()
{
  if (!sSingleton) {
    sSingleton = new GpioService();
    ClearOnShutdown(&sSingleton);
  }
  nsRefPtr<GpioService> service = sSingleton.get();
  return service.forget();
}

#define GPIO_PATH "/sys/class/gpio/"

NS_IMETHODIMP
GpioService::Export(uint32_t aPinNo) {
  FILE *fp_export, *fp_direction;
  char path[] = GPIO_PATH "gpioxxxx/direction";

  if (map_direction.find(aPinNo) != map_direction.end()) {
    LOG("already exported\n");
    return NS_OK;
  }

  fp_export = ::fopen(GPIO_PATH "export", "w");
  if (fp_export == NULL) {
    LOG("%s: fopen(%s) failed: %s\n", __func__, GPIO_PATH "export",
        strerror(errno));
    return NS_ERROR_FAILURE;
  }
  ::fprintf(fp_export, "%d", aPinNo);
  ::fflush(fp_export);
  ::fclose(fp_export);

  map_direction[aPinNo] = true;

  // XXX: delay to avoid Permission Denied of trailing fopen
  ::usleep(10000);

  ::snprintf(path, sizeof(path), GPIO_PATH "gpio%d/direction", aPinNo);
  fp_direction = ::fopen(path, "w");
  if (fp_direction == NULL) {
    LOG("%s: fopen(%s) failed: %s\n", __func__, path, strerror(errno));
    Unexport(aPinNo);
    return NS_ERROR_FAILURE;
  }
  map_direction_fp[aPinNo] = fp_direction;

  return SetDirection(aPinNo, true);
}

NS_IMETHODIMP
GpioService::Unexport(uint32_t aPinNo) {
  FILE *fp;

  if (map_direction.find(aPinNo) == map_direction.end()) {
    LOG("already unexported\n");
    return NS_OK;
  }

  fp = ::fopen(GPIO_PATH "unexport", "w");
  if (fp == NULL) {
    LOG("%s: fopen(%s) failed: %s\n", __func__, GPIO_PATH "unexport",
        strerror(errno));
    return NS_ERROR_FAILURE;
  }
  ::fprintf(fp, "%d", aPinNo);
  ::fflush(fp);
  ::fclose(fp);
  map_direction.erase(aPinNo);

  if (map_direction_fp.find(aPinNo) != map_direction_fp.end()) {
    ::fclose(map_direction_fp[aPinNo]);
    map_direction_fp.erase(aPinNo);
  }
  if (map_value_fp.find(aPinNo) != map_value_fp.end()) {
    ::fclose(map_value_fp[aPinNo]);
    map_value_fp.erase(aPinNo);
  }
  return NS_OK;
}

NS_IMETHODIMP
GpioService::SetDirection(uint32_t aPinNo, bool aOut) {
  FILE *fp_direction, *fp_value;
  char path[] = GPIO_PATH "gpioxxxx/value";

  if (map_direction.find(aPinNo) == map_direction.end()) {
    LOG("not exported\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  fp_direction = map_direction_fp[aPinNo];
  if (::fprintf(fp_direction, "%s", aOut ? "out" : "in") < 0) {
    LOG("fprintf failed: %s\n", strerror(errno));
    return NS_ERROR_FAILURE;
  }
  ::fflush(fp_direction);
  map_direction[aPinNo] = aOut;

  if (map_value_fp.find(aPinNo) != map_value_fp.end())
    ::fclose(map_value_fp[aPinNo]);

  // XXX: delay to avoid Permission Denied of trailing fopen
  ::usleep(10000);

  ::snprintf(path, sizeof(path), GPIO_PATH "gpio%d/value", aPinNo);
  fp_value = ::fopen(path, aOut ? "w" : "r");
  if (fp_value == NULL) {
    LOG("%s: fopen(%s) failed: %s\n", __func__, path, strerror(errno));
    return NS_ERROR_FAILURE;
  }
  map_value_fp[aPinNo] = fp_value;

  return NS_OK;
}

NS_IMETHODIMP
GpioService::GetDirection(uint32_t aPinNo, bool *aOut) {
  if (map_direction.find(aPinNo) == map_direction.end()) {
    LOG("not exported\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aOut = map_direction[aPinNo];
  return NS_OK;
}

NS_IMETHODIMP
GpioService::SetValue(uint32_t aPinNo, uint32_t aValue) {
  FILE *fp;

  if (map_direction.find(aPinNo) == map_direction.end()) {
    LOG("not exported\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!map_direction[aPinNo]) {
    LOG("This port is input mode\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  fp = map_value_fp[aPinNo];
  if (::fprintf(fp, "%d", aValue) < 0) {
    LOG("fprintf failed: %s\n", strerror(errno));
    return NS_ERROR_FAILURE;
  }
  ::fflush(fp);

  return NS_OK;
}

NS_IMETHODIMP
GpioService::GetValue(uint32_t aPinNo, uint32_t *aValue) {
  FILE *fp;
  int fd;
  char buf[16];
  ssize_t ret;
  char *endptr;

  if (map_direction.find(aPinNo) == map_direction.end()) {
    LOG("not exported\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (map_direction[aPinNo]) {
    LOG("This port is output mode\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  /* Use fd instead of fp. Because fgets returns previous value even thought
     after fseek(). */
  fd = fileno(map_value_fp[aPinNo]);
  if (fd < 0) {
    LOG("fileno failed: %s\n", strerror(errno));
    return NS_ERROR_FAILURE;
  }
  if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
    LOG("lseek failed %s(%d)\n", strerror(errno), errno);
    return NS_ERROR_FAILURE;
  }
  ret = read(fd, buf, sizeof(buf));
  if (ret < 0) {
    fprintf(stderr, "read failed %s(%d)\n", strerror(errno), errno);
    return NS_ERROR_FAILURE;
  }
  buf[ret] = '\0';

  ret = strtoul(buf, &endptr, 10);
  if (*endptr != '\0' && *endptr != '\n') {
    LOG("non numeric value [%s] found\n", endptr);
    return NS_ERROR_FAILURE;
  }
  *aValue = ret;

  return NS_OK;
}

} // namespace gpio
} // namespace dom
} // namespace mozilla
