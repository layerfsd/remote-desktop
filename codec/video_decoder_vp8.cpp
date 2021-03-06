//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_decoder_vp8.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_decoder_vp8.h"

#include <thread>

#include "libyuv/convert_from.h"
#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

static const int kBytesPerPixel = 4;

VideoDecoderVP8::VideoDecoderVP8() :
    bytes_per_row_(0)
{
    // Nothing
}

VideoDecoderVP8::~VideoDecoderVP8()
{
    // Nothing
}

void VideoDecoderVP8::ConvertImageToARGB(const proto::VideoPacket *packet,
                                         vpx_image_t *image,
                                         DesktopRegion &dirty_region)
{
    switch (image->fmt)
    {
        case VPX_IMG_FMT_I420:
        {
            uint8_t *y_data = image->planes[0];
            uint8_t *u_data = image->planes[1];
            uint8_t *v_data = image->planes[2];
            int y_stride = image->stride[0];
            int uv_stride = image->stride[1];

            for (int i = 0; i < packet->dirty_rect_size(); ++i)
            {
                const proto::VideoRect &rect = packet->dirty_rect(i);

                if (rect.x() + rect.width() > screen_size_.Width() ||
                    rect.y() + rect.height() > screen_size_.Height())
                {
                    break;
                }

                int rgb_offset = bytes_per_row_ * rect.y() + rect.x() * sizeof(uint32_t);
                int y_offset = y_stride * rect.y() + rect.x();
                int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;

                libyuv::I420ToARGB(y_data + y_offset, y_stride,
                                   u_data + uv_offset, uv_stride,
                                   v_data + uv_offset, uv_stride,
                                   buffer_.get() + rgb_offset,
                                   bytes_per_row_,
                                   rect.width(),
                                   rect.height());

                dirty_region.AddRect(DesktopRect(rect));
            }
        }
        break;

        default:
        {
            LOG(ERROR) << "Unsupported image format: " << image->fmt;
            throw Exception("Unsupported image format recieved.");
        }
    }
}

void VideoDecoderVP8::Resize()
{
    codec_.reset(new vpx_codec_ctx_t());

    bytes_per_row_ = kBytesPerPixel * screen_size_.Width();

    vpx_codec_dec_cfg_t config;

    config.w = 0;
    config.h = 0;
    config.threads = (std::thread::hardware_concurrency() > 2) ? 2 : 1;

    vpx_codec_err_t ret = vpx_codec_dec_init(codec_.get(), vpx_codec_vp8_dx(), &config, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_dec_init() failed: " <<
            vpx_codec_err_to_string(ret) << " (" << ret << ")";
        throw Exception("Unable to resize video decoder.");
    }

    buffer_.resize(bytes_per_row_ * screen_size_.Height());
}

int32_t VideoDecoderVP8::Decode(const proto::VideoPacket *packet,
                                uint8_t **buffer,
                                DesktopRegion &changed_region,
                                DesktopSize &size,
                                PixelFormat &pixel_format)
{
    if (packet->flags() & proto::VideoPacket::FIRST_PACKET)
    {
        const proto::VideoPacketFormat &format = packet->format();

        if (format.has_screen_size())
        {
            screen_size_.FromVideoSize(format.screen_size());
            Resize();
        }

        size = screen_size_;
        pixel_format = PixelFormat::ARGB();
    }

    // Do the actual decoding.
    vpx_codec_err_t ret =
        vpx_codec_decode(codec_.get(),
                         reinterpret_cast<const uint8_t*>(packet->data().data()),
                         packet->data().size(),
                         nullptr,
                         0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_decode() failed: "
            << vpx_codec_error(codec_.get())
            << " ("<< vpx_codec_error_detail(codec_.get()) << ")";
        throw Exception("Unable to decode video packet.");
    }

    vpx_codec_iter_t iter = nullptr;

    // Gets the decoded data.
    vpx_image_t *image = vpx_codec_get_frame(codec_.get(), &iter);
    if (!image)
    {
        LOG(ERROR) << "No video frame decoded";
        throw Exception("No video frame decoded.");
    }

    ConvertImageToARGB(packet, image, changed_region);

    *buffer = buffer_.get();

    return packet->flags();
}

} // namespace aspia
