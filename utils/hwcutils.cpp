/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwc-drm-utils"

#include <log/log.h>
#include <ui/Gralloc.h>
#include <ui/GraphicBufferMapper.h>

#include "bufferinfo/BufferInfoGetter.h"
#include "drm/DrmGenericImporter.h"
#include "drmhwcomposer.h"

#define UNUSED(x) (void)(x)

namespace android {

const hwc_drm_bo *DrmHwcBuffer::operator->() const {
  if (importer_ == nullptr) {
    ALOGE("Access of non-existent BO");
    exit(1);
    return nullptr;
  }
  return &bo_;
}

void DrmHwcBuffer::Clear() {
  if (importer_ != nullptr) {
    importer_->ReleaseBuffer(&bo_);
    importer_ = nullptr;
  }
}

int DrmHwcBuffer::ImportBuffer(buffer_handle_t handle, Importer *importer) {
  hwc_drm_bo tmp_bo{};

  int ret = BufferInfoGetter::GetInstance()->ConvertBoInfo(handle, &tmp_bo);
  if (ret) {
    ALOGE("Failed to convert buffer info %d", ret);
    return ret;
  }

  ret = importer->ImportBuffer(&tmp_bo);
  if (ret) {
    ALOGE("Failed to import buffer %d", ret);
    return ret;
  }

  if (importer_ != nullptr) {
    importer_->ReleaseBuffer(&bo_);
  }

  importer_ = importer;

  bo_ = tmp_bo;

  return 0;
}

int DrmHwcNativeHandle::CopyBufferHandle(buffer_handle_t handle) {
  native_handle_t *handle_copy = nullptr;
  GraphicBufferMapper &gm(GraphicBufferMapper::get());
  int ret = 0;

  ret = gm.getGrallocMapper().importBuffer(handle,
                                           (buffer_handle_t *)&handle_copy);

  if (ret) {
    ALOGE("Failed to import buffer handle %d", ret);
    return ret;
  }

  Clear();

  handle_ = handle_copy;

  return 0;
}

DrmHwcNativeHandle::~DrmHwcNativeHandle() {
  Clear();
}

void DrmHwcNativeHandle::Clear() {
  if (handle_ != nullptr) {
    GraphicBufferMapper &gm(GraphicBufferMapper::get());
    int ret = gm.freeBuffer(handle_);
    if (ret) {
      ALOGE("Failed to free buffer handle %d", ret);
    }
    handle_ = nullptr;
  }
}

int DrmHwcLayer::ImportBuffer(Importer *importer) {
  int ret = buffer.ImportBuffer(sf_handle, importer);
  if (ret)
    return ret;

  const hwc_drm_bo *bo = buffer.operator->();

  ret = handle.CopyBufferHandle(sf_handle);
  if (ret)
    return ret;

  gralloc_buffer_usage = bo->usage;

  return 0;
}

int DrmHwcLayer::InitFromDrmHwcLayer(DrmHwcLayer *src_layer,
                                     Importer *importer) {
  blending = src_layer->blending;
  sf_handle = src_layer->sf_handle;
  acquire_fence = -1;
  display_frame = src_layer->display_frame;
  alpha = src_layer->alpha;
  source_crop = src_layer->source_crop;
  transform = src_layer->transform;
  return ImportBuffer(importer);
}

void DrmHwcLayer::SetTransform(int32_t sf_transform) {
  transform = 0;
  // 270* and 180* cannot be combined with flips. More specifically, they
  // already contain both horizontal and vertical flips, so those fields are
  // redundant in this case. 90* rotation can be combined with either horizontal
  // flip or vertical flip, so treat it differently
  if (sf_transform == HWC_TRANSFORM_ROT_270) {
    transform = DrmHwcTransform::kRotate270;
  } else if (sf_transform == HWC_TRANSFORM_ROT_180) {
    transform = DrmHwcTransform::kRotate180;
  } else {
    if (sf_transform & HWC_TRANSFORM_FLIP_H)
      transform |= DrmHwcTransform::kFlipH;
    if (sf_transform & HWC_TRANSFORM_FLIP_V)
      transform |= DrmHwcTransform::kFlipV;
    if (sf_transform & HWC_TRANSFORM_ROT_90)
      transform |= DrmHwcTransform::kRotate90;
  }
}
}  // namespace android
