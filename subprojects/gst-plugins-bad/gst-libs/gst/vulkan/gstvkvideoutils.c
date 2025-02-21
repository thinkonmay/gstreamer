/*
 * GStreamer
 * Copyright (C) 2023 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvkvideoutils.h"

#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
/* *INDENT-OFF* */
static const struct {
  VkVideoCodecOperationFlagBitsKHR codec;
  const char *mime;
  VkStructureType stype;
} video_codecs_map[] = {
  { VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR, "video/x-h264",
      VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR },
  { VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR, "video/x-h265",
      VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR },
};

static const struct {
  VkVideoChromaSubsamplingFlagBitsKHR chroma;
  const char *chroma_str;
} video_chroma_map[] = {
  { VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR, "4:2:0" },
  { VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR, "4:2:2" },
  { VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR, "4:4:4" },
};

static const struct {
  VkVideoComponentBitDepthFlagBitsKHR bitdepth;
  int bit_depth;
} bit_depth_map[] = {
  {VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, 8},
  {VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR, 10},
  {VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR, 12},
};

static const struct {
  StdVideoH264ProfileIdc vk_profile;
  const char *profile_str;
} h264_profile_map[] = {
  { STD_VIDEO_H264_PROFILE_IDC_BASELINE, "baseline" },
  { STD_VIDEO_H264_PROFILE_IDC_MAIN, "main" },
  { STD_VIDEO_H264_PROFILE_IDC_HIGH, "high" },
  { STD_VIDEO_H264_PROFILE_IDC_HIGH_444_PREDICTIVE, "high-4:4:4" },
};

static const struct {
  VkVideoDecodeH264PictureLayoutFlagBitsKHR layout;
  const char *layout_str;
} h264_layout_map[] = {
  { VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR, "progressive" },
  { VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR,
      "interleaved" },
  { VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_SEPARATE_PLANES_BIT_KHR,
      "fields" },
};

static const struct {
  StdVideoH265ProfileIdc vk_profile;
  const char *profile_str;
} h265_profile_map[] = {
  { STD_VIDEO_H265_PROFILE_IDC_MAIN, "main" },
  { STD_VIDEO_H265_PROFILE_IDC_MAIN_10, "main-10" },
  { STD_VIDEO_H265_PROFILE_IDC_MAIN_STILL_PICTURE, "main-still-picture" },
  { STD_VIDEO_H265_PROFILE_IDC_FORMAT_RANGE_EXTENSIONS,
      "format-range-extensions" },
  { STD_VIDEO_H265_PROFILE_IDC_SCC_EXTENSIONS, "scc-extensions" },
};

/* *INDENT-ON* */
#endif

/**
 * gst_vulkan_video_profile_to_caps: (skip)
 * @profile: #GstVulkanVideoProfile to convert into a #GstCaps
 *
 * Returns: (transfer full): a #GstCaps from @profile
 *
 * Since: 1.24
 */
GstCaps *
gst_vulkan_video_profile_to_caps (const GstVulkanVideoProfile * profile)
{
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
  const char *mime, *chroma_sub, *profile_str = NULL, *layout = NULL;
  int i, luma, chroma;
  GstCaps *caps;

  g_return_val_if_fail (profile
      && profile->profile.sType == VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,
      NULL);

  for (i = 0; i < G_N_ELEMENTS (video_codecs_map); i++) {
    if (profile->profile.videoCodecOperation == video_codecs_map[i].codec) {
      mime = video_codecs_map[i].mime;

      switch (profile->profile.videoCodecOperation) {
        case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
          if (profile->codec.h264.sType == video_codecs_map[i].stype) {
            int j;
            for (j = 0; j < G_N_ELEMENTS (h264_profile_map); j++) {
              if (profile->codec.h264.stdProfileIdc
                  == h264_profile_map[j].vk_profile) {
                profile_str = h264_profile_map[j].profile_str;
                break;
              }
            }
            for (j = 0; j < G_N_ELEMENTS (h264_layout_map); j++) {
              if (profile->codec.h264.pictureLayout
                  == h264_layout_map[j].layout) {
                layout = h264_layout_map[j].layout_str;
                break;
              }
            }
          }
          break;
        case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
          if (profile->codec.h265.sType == video_codecs_map[i].stype) {
            int j;
            for (j = 0; j < G_N_ELEMENTS (h265_profile_map); j++) {
              if (profile->codec.h265.stdProfileIdc
                  == h265_profile_map[j].vk_profile)
                profile_str = h265_profile_map[j].profile_str;
            }
          }
          break;
        default:
          break;
      }

      break;
    }
  }
  if (i == G_N_ELEMENTS (video_codecs_map))
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (video_chroma_map); i++) {
    if (profile->profile.chromaSubsampling == video_chroma_map[i].chroma) {
      chroma_sub = video_chroma_map[i].chroma_str;
      break;
    }
  }
  if (i == G_N_ELEMENTS (video_chroma_map))
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (bit_depth_map); i++) {
    if (profile->profile.chromaBitDepth == bit_depth_map[i].bitdepth) {
      chroma = bit_depth_map[i].bit_depth;
      break;
    }
  }
  if (i == G_N_ELEMENTS (bit_depth_map))
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (bit_depth_map); i++) {
    if (profile->profile.lumaBitDepth == bit_depth_map[i].bitdepth) {
      luma = bit_depth_map[i].bit_depth;
      break;
    }
  }
  if (i == G_N_ELEMENTS (bit_depth_map))
    return NULL;

  caps = gst_caps_new_simple (mime, "chroma-format", G_TYPE_STRING, chroma_sub,
      "bit-depth-luma", G_TYPE_UINT, luma, "bit-depth-chroma", G_TYPE_UINT,
      chroma, NULL);

  if (profile_str)
    gst_caps_set_simple (caps, "profile", G_TYPE_STRING, profile_str, NULL);
  if (layout)
    gst_caps_set_simple (caps, "interlace-mode", G_TYPE_STRING, layout, NULL);

  return caps;

#endif
  return NULL;
}

/**
 * gst_vulkan_video_profile_from_caps: (skip)
 * @profile: (out): the output profile
 * @caps: a #GstCaps to parse
 *
 * Returns: %TRUE if @caps was parsed correctly, otherwise %FALSE
 *
 * Since: 1.24
 */
gboolean
gst_vulkan_video_profile_from_caps (GstVulkanVideoProfile * profile,
    GstCaps * caps)
{
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
  const GstStructure *structure;
  const char *mime, *chroma_sub, *profile_str = NULL, *layout = NULL;
  int i, luma, chroma;

  g_return_val_if_fail (GST_IS_CAPS (caps), FALSE);
  g_return_val_if_fail (profile, FALSE);

  structure = gst_caps_get_structure (caps, 0);

  profile->profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;

  mime = gst_structure_get_name (structure);
  for (i = 0; i < G_N_ELEMENTS (video_codecs_map); i++) {
    if (g_strcmp0 (video_codecs_map[i].mime, mime) == 0) {
      profile->profile.videoCodecOperation = video_codecs_map[i].codec;

      switch (profile->profile.videoCodecOperation) {
        case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:{
          int j;

          profile->codec.h264.sType = video_codecs_map[i].stype;
          profile->codec.h264.stdProfileIdc =
              STD_VIDEO_H264_PROFILE_IDC_INVALID;
          profile->codec.h264.pictureLayout =
              VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_FLAG_BITS_MAX_ENUM_KHR;
          profile->profile.pNext = &profile->codec;

          profile_str = gst_structure_get_string (structure, "profile");
          for (j = 0; profile_str && j < G_N_ELEMENTS (h264_profile_map); j++) {
            if (g_strcmp0 (profile_str, h264_profile_map[j].profile_str) == 0) {
              profile->codec.h264.stdProfileIdc =
                  h264_profile_map[j].vk_profile;
              break;
            }
          }
          layout = gst_structure_get_string (structure, "interlace-mode");
          for (j = 0; layout && j < G_N_ELEMENTS (h264_layout_map); j++) {
            if (g_strcmp0 (layout, h264_layout_map[j].layout_str) == 0) {
              profile->codec.h264.pictureLayout = h264_layout_map[j].layout;
              break;
            }
          }
          break;
        }
        case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:{
          int j;

          profile->codec.h265.sType = video_codecs_map[i].stype;
          profile->codec.h265.stdProfileIdc =
              STD_VIDEO_H265_PROFILE_IDC_INVALID;
          profile->profile.pNext = &profile->codec;

          profile_str = gst_structure_get_string (structure, "profile");
          for (j = 0; profile_str && j < G_N_ELEMENTS (h265_profile_map); j++) {
            if (g_strcmp0 (profile_str, h265_profile_map[j].profile_str) == 0) {
              profile->codec.h265.stdProfileIdc =
                  h265_profile_map[j].vk_profile;
              break;
            }
          }
          break;
        }
        default:
          break;
      }

      break;
    }
  }
  if (i == G_N_ELEMENTS (video_codecs_map))
    return FALSE;

  if (!gst_structure_get (structure, "chroma-format", G_TYPE_STRING,
          &chroma_sub, "bit-depth-luma", G_TYPE_UINT, &luma, "bit-depth-chroma",
          G_TYPE_UINT, &chroma, NULL))
    return FALSE;

  for (i = 0; i < G_N_ELEMENTS (video_chroma_map); i++) {
    if (g_strcmp0 (chroma_sub, video_chroma_map[i].chroma_str) == 0) {
      profile->profile.chromaSubsampling = video_chroma_map[i].chroma;
      break;
    }
  }
  if (i == G_N_ELEMENTS (video_chroma_map))
    return FALSE;

  for (i = 0; i < G_N_ELEMENTS (bit_depth_map); i++) {
    if (luma == bit_depth_map[i].bit_depth) {
      profile->profile.lumaBitDepth = bit_depth_map[i].bitdepth;
      break;
    }
  }
  if (i == G_N_ELEMENTS (bit_depth_map))
    return FALSE;

  for (i = 0; i < G_N_ELEMENTS (bit_depth_map); i++) {
    if (chroma == bit_depth_map[i].bit_depth) {
      profile->profile.chromaBitDepth = bit_depth_map[i].bitdepth;
      break;
    }
  }
  if (i == G_N_ELEMENTS (bit_depth_map))
    return FALSE;
#endif
  return TRUE;
}
