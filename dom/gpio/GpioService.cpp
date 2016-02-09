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
  RefPtr<GpioService> service = sSingleton.get();
  return service.forget();
}

#define GPIO_PATH "/sys/class/gpio/"

NS_IMETHODIMP
GpioService::Export(uint32_t aPinNo) {
  if (mMapDirection.find(aPinNo) != mMapDirection.end()) {
    LOG("already exported\n");
    return NS_OK;
  }

  FILE* fp_export = ::fopen(GPIO_PATH "export", "w");
  if (fp_export == NULL) {
    LOG("%s: fopen(%s) failed: %s\n", __func__, GPIO_PATH "export",
        strerror(errno));
    return NS_ERROR_FAILURE;
  }
  ::fprintf(fp_export, "%d", aPinNo);
  ::fflush(fp_export);
  ::fclose(fp_export);

  mMapDirection[aPinNo] = true;

  // XXX: delay to avoid Permission Denied of trailing fopen
  ::usleep(10000);

  nsCString path;
  path.Assign(GPIO_PATH);
  path.Append("gpio");
  path.AppendInt(aPinNo);
  path.Append("/direction");
  FILE* fp_direction = ::fopen(path.get(), "w");
  if (fp_direction == NULL) {
    LOG("%s: fopen(%s) failed: %s\n", __func__, path.get(), strerror(errno));
    Unexport(aPinNo);
    return NS_ERROR_FAILURE;
  }
  mMapDirectionFp[aPinNo] = fp_direction;

  return SetDirection(aPinNo, true);
}

NS_IMETHODIMP
GpioService::Unexport(uint32_t aPinNo) {
  if (mMapDirection.find(aPinNo) == mMapDirection.end()) {
    LOG("already unexported\n");
    return NS_OK;
  }

  FILE* fp = ::fopen(GPIO_PATH "unexport", "w");
  if (fp == NULL) {
    LOG("%s: fopen(%s) failed: %s\n", __func__, GPIO_PATH "unexport",
        strerror(errno));
    return NS_ERROR_FAILURE;
  }
  ::fprintf(fp, "%d", aPinNo);
  ::fflush(fp);
  ::fclose(fp);
  mMapDirection.erase(aPinNo);

  if (mMapDirectionFp.find(aPinNo) != mMapDirectionFp.end()) {
    ::fclose(mMapDirectionFp[aPinNo]);
    mMapDirectionFp.erase(aPinNo);
  }
  if (mMapValueFp.find(aPinNo) != mMapValueFp.end()) {
    ::fclose(mMapValueFp[aPinNo]);
    mMapValueFp.erase(aPinNo);
  }
  return NS_OK;
}

NS_IMETHODIMP
GpioService::SetDirection(uint32_t aPinNo, bool aOut) {
  if (mMapDirection.find(aPinNo) == mMapDirection.end()) {
    LOG("not exported\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  FILE* fp_direction = mMapDirectionFp[aPinNo];
  if (::fprintf(fp_direction, "%s", aOut ? "out" : "in") < 0) {
    LOG("fprintf failed: %s\n", strerror(errno));
    return NS_ERROR_FAILURE;
  }
  ::fflush(fp_direction);
  mMapDirection[aPinNo] = aOut;

  if (mMapValueFp.find(aPinNo) != mMapValueFp.end())
    ::fclose(mMapValueFp[aPinNo]);

  // XXX: delay to avoid Permission Denied of trailing fopen
  ::usleep(10000);

  nsCString path;
  path.Assign(GPIO_PATH);
  path.Append("gpio");
  path.AppendInt(aPinNo);
  path.Append("/value");

  FILE* fp_value = ::fopen(path.get(), aOut ? "w" : "r");
  if (fp_value == NULL) {
    LOG("%s: fopen(%s) failed: %s\n", __func__, path.get(), strerror(errno));
    return NS_ERROR_FAILURE;
  }
  mMapValueFp[aPinNo] = fp_value;

  return NS_OK;
}

NS_IMETHODIMP
GpioService::GetDirection(uint32_t aPinNo, bool *aOut) {
  if (mMapDirection.find(aPinNo) == mMapDirection.end()) {
    LOG("not exported\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aOut = mMapDirection[aPinNo];
  return NS_OK;
}

NS_IMETHODIMP
GpioService::SetValue(uint32_t aPinNo, uint32_t aValue) {
  if (mMapDirection.find(aPinNo) == mMapDirection.end()) {
    LOG("not exported\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mMapDirection[aPinNo]) {
    LOG("This port is input mode\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  FILE* fp = mMapValueFp[aPinNo];
  if (::fprintf(fp, "%d", aValue) < 0) {
    LOG("fprintf failed: %s\n", strerror(errno));
    return NS_ERROR_FAILURE;
  }
  ::fflush(fp);

  return NS_OK;
}

NS_IMETHODIMP
GpioService::GetValue(uint32_t aPinNo, uint32_t *aValue) {
  if (mMapDirection.find(aPinNo) == mMapDirection.end()) {
    LOG("not exported\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mMapDirection[aPinNo]) {
    LOG("This port is output mode\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  int fd = fileno(mMapValueFp[aPinNo]);
  if (fd < 0) {
    LOG("fileno failed: %s\n", strerror(errno));
    return NS_ERROR_FAILURE;
  }
  if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
    LOG("lseek failed %s(%d)\n", strerror(errno), errno);
    return NS_ERROR_FAILURE;
  }

  char buf[16];
  ssize_t ret = read(fd, buf, sizeof(buf));
  if (ret < 0) {
    fprintf(stderr, "read failed %s(%d)\n", strerror(errno), errno);
    return NS_ERROR_FAILURE;
  }
  buf[ret] = '\0';

  char* endptr;
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
